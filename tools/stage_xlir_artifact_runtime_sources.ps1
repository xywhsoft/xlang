param([Parameter(Mandatory=$true)][string]$Directory)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'xlir_native_process.ps1')
$rootPath = (Get-Location).Path
$destination = (Resolve-Path $Directory).Path
$manifestPath = Join-Path $destination 'build.json'
$build = Get-Content $manifestPath -Raw | ConvertFrom-Json
if ((Get-FileHash $build.Driver).Hash -ne $build.DriverSha256) { throw 'Compiler binary changed' }
function Get-StagedFileHash([string]$Path) {
    # File.Move publishes complete bytes, but a concurrent reader can still
    # encounter the mover's transient Windows sharing handle. Permit rename
    # sharing and retry only native sharing/lock violations, never a hash
    # mismatch, missing file, permission error or other publication failure.
    $deadline = [DateTime]::UtcNow.AddSeconds(5)
    while ($true) {
        $stream = $null
        $algorithm = $null
        try {
            $stream = [IO.File]::Open($Path, [IO.FileMode]::Open,
                [IO.FileAccess]::Read, ([IO.FileShare]::Read -bor [IO.FileShare]::Delete))
            $algorithm = [Security.Cryptography.SHA256]::Create()
            return [BitConverter]::ToString($algorithm.ComputeHash($stream)).Replace('-', '')
        } catch [IO.IOException] {
            $errorCode = $_.Exception.HResult -band 0xffff
            if ($errorCode -notin @(32,33) -or [DateTime]::UtcNow -ge $deadline) { throw }
            Start-Sleep -Milliseconds 10
        } finally {
            if ($null -ne $algorithm) { $algorithm.Dispose() }
            if ($null -ne $stream) { $stream.Dispose() }
        }
    }
}
$stageManifest=Join-Path $destination 'artifact_sources.json'
$mappingHash=(Get-FileHash tools/tcc_vfs_manifest_win64_xlc.txt).Hash
$stageHash=(Get-FileHash $PSCommandPath).Hash
$priorStage=if (Test-Path -LiteralPath $stageManifest) { Get-Content -LiteralPath $stageManifest -Raw | ConvertFrom-Json } else { $null }
if ($priorStage -and ($priorStage.DriverSha256 -ne $build.DriverSha256 -or
    $priorStage.MappingSha256 -ne $mappingHash -or $priorStage.StageScriptSha256 -ne $stageHash)) {
    throw 'Staging contract changed; use a fresh source build directory rather than replacing a published package'
}
$records = @()
function Stage-File([string]$source, [string]$relative) {
    $target = [IO.Path]::GetFullPath((Join-Path $destination $relative))
    if (!$target.StartsWith($destination + [IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) { throw 'Staged source escaped private build directory' }
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $target) | Out-Null
    $hash = (Get-FileHash -LiteralPath $source).Hash
    if (Test-Path -LiteralPath $target) {
        if ((Get-StagedFileHash $target) -ne $hash) {
            throw "Published staged file changed; use a fresh source directory: $target"
        }
    } else {
        if ($priorStage) { throw "Published staging inventory changed: $target" }
        # Publish once. Concurrent initial staging may race to publish the same
        # bytes, but readers never see a target being truncated or rewritten.
        $temporary=$target+'.stage-'+[Guid]::NewGuid().ToString('N')
        try {
            Copy-Item -LiteralPath $source -Destination $temporary
            if ((Get-FileHash -LiteralPath $temporary).Hash -ne $hash) { throw "Source changed during staging: $source" }
            try { [IO.File]::Move($temporary,$target) }
            catch { if (!(Test-Path -LiteralPath $target) -or (Get-StagedFileHash $target) -ne $hash) { throw } }
        } finally { if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary } }
    }
    if ((Get-StagedFileHash $target) -ne $hash) { throw "Staging mismatch: $source" }
    [pscustomobject]@{Source=$source;Path=$target;Sha256=$hash}
}
# Materialize the compiler-relative development layout already understood by
# xlGetProjectRoot, using the source/header mappings of the shipped package.
# This is not a generated VFS and does not build the import-xlang/TCC payload.
foreach ($entry in Get-Content -LiteralPath tools/tcc_vfs_manifest_win64_xlc.txt -Encoding UTF8) {
    if ($entry.StartsWith('dir|__xl_compiler/src|')) {
        $parts = $entry.Split('|'); $sourceRoot = $parts[2].Replace('%XL6_ROOT%',$rootPath)
        $patterns = $parts[3].Split(';')
        foreach ($file in Get-ChildItem -LiteralPath $sourceRoot -Recurse -File) {
            if (!@($patterns | Where-Object { $file.Name -like $_ }).Count) { continue }
            $records += Stage-File $file.FullName (Join-Path 'src' (Get-XlirContainedRelativePath $sourceRoot $file.FullName))
        }
    } elseif ($entry.StartsWith('file|__xl_compiler/include/') -or
              $entry.StartsWith('file|lib/xrt/xrt.h|') -or $entry.StartsWith('file|lib/xruntime/xruntime.h|')) {
        $parts = $entry.Split('|'); $relative = $parts[1].Replace('__xl_compiler/','')
        $records += Stage-File ($parts[2].Replace('%XL6_ROOT%',$rootPath)) $relative
    }
}
if ($records.Count -lt 20) { throw 'Runtime source package unexpectedly incomplete' }
if ($priorStage) {
    $expected=@($priorStage.Files | ForEach-Object { $_.Source+'|'+$_.Path+'|'+$_.Sha256 })
    $actual=@($records | ForEach-Object { $_.Source+'|'+$_.Path+'|'+$_.Sha256 })
    if ($expected.Count -ne $actual.Count -or @(Compare-Object $expected $actual).Count) { throw 'Published staging inventory changed; use a fresh source directory' }
    Write-Output "Reused immutable staged package: $($records.Count) source/header files"
    return
}
$report = [pscustomobject]@{RecordedAt=(Get-Date).ToString('o');Files=$records;Compiler=$build.Driver;DriverSha256=$build.DriverSha256;
    Mapping='tools/tcc_vfs_manifest_win64_xlc.txt';MappingSha256=$mappingHash;
    StageScriptSha256=$stageHash;
    Scope='Disk deployment of existing compiler-relative language runtime sources and declarations; not rebuilt release/VFS or recursive import-xlang/TCC payload.'}
$temporaryManifest=$stageManifest+'.stage-'+[Guid]::NewGuid().ToString('N')
try {
    [IO.File]::WriteAllText($temporaryManifest, ($report | ConvertTo-Json -Depth 6), [Text.UTF8Encoding]::new($false))
    try { [IO.File]::Move($temporaryManifest,$stageManifest) }
    catch {
        if (!(Test-Path -LiteralPath $stageManifest)) { throw }
        $published=Get-Content -LiteralPath $stageManifest -Raw | ConvertFrom-Json
        $publishedRows=@($published.Files | ForEach-Object { $_.Source+'|'+$_.Path+'|'+$_.Sha256 })
        $recordRows=@($records | ForEach-Object { $_.Source+'|'+$_.Path+'|'+$_.Sha256 })
        if ($published.DriverSha256 -ne $report.DriverSha256 -or $published.MappingSha256 -ne $mappingHash -or
            $published.StageScriptSha256 -ne $stageHash -or $published.Files.Count -ne $records.Count -or
            @(Compare-Object -ReferenceObject $publishedRows -DifferenceObject $recordRows).Count) { throw }
    }
} finally { if (Test-Path -LiteralPath $temporaryManifest) { Remove-Item -LiteralPath $temporaryManifest } }
Write-Output "Staged $($records.Count) source/header files without rebuilding compiler or release"
