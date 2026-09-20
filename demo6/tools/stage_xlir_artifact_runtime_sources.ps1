param([Parameter(Mandatory=$true)][string]$Directory)
$ErrorActionPreference = 'Stop'
$rootPath = (Get-Location).Path
$destination = (Resolve-Path $Directory).Path
$manifestPath = Join-Path $destination 'build.json'
$build = Get-Content $manifestPath -Raw | ConvertFrom-Json
if ((Get-FileHash $build.Driver).Hash -ne $build.DriverSha256) { throw 'Compiler binary changed' }
$records = @()
function Stage-File([string]$source, [string]$relative) {
    $target = [IO.Path]::GetFullPath((Join-Path $destination $relative))
    if (!$target.StartsWith($destination + [IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) { throw 'Staged source escaped private build directory' }
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $target) | Out-Null
    Copy-Item -LiteralPath $source -Destination $target -Force
    $hash = (Get-FileHash -LiteralPath $source).Hash
    if ((Get-FileHash -LiteralPath $target).Hash -ne $hash) { throw "Staging mismatch: $source" }
    [pscustomobject]@{Source=$source;Path=$target;Sha256=$hash}
}
# Materialize the compiler-relative development layout already understood by
# xlGetProjectRoot, using the source/header mappings of the shipped package.
# This is not a generated VFS and does not build the import-xlang/TCC payload.
foreach ($entry in Get-Content tools/tcc_vfs_manifest_win64_xlc.txt) {
    if ($entry.StartsWith('dir|__xl_compiler/src|')) {
        $parts = $entry.Split('|'); $sourceRoot = $parts[2].Replace('%XL6_ROOT%',$rootPath)
        $patterns = $parts[3].Split(';')
        foreach ($file in Get-ChildItem -LiteralPath $sourceRoot -Recurse -File) {
            if (!@($patterns | Where-Object { $file.Name -like $_ }).Count) { continue }
            $records += Stage-File $file.FullName (Join-Path 'src' ([IO.Path]::GetRelativePath($sourceRoot,$file.FullName)))
        }
    } elseif ($entry.StartsWith('file|__xl_compiler/include/') -or
              $entry.StartsWith('file|lib/xrt/xrt.h|') -or $entry.StartsWith('file|lib/xruntime/xruntime.h|')) {
        $parts = $entry.Split('|'); $relative = $parts[1].Replace('__xl_compiler/','')
        $records += Stage-File ($parts[2].Replace('%XL6_ROOT%',$rootPath)) $relative
    }
}
if ($records.Count -lt 20) { throw 'Runtime source package unexpectedly incomplete' }
$report = [pscustomobject]@{RecordedAt=(Get-Date).ToString('o');Files=$records;Compiler=$build.Driver;DriverSha256=$build.DriverSha256;
    Mapping='tools/tcc_vfs_manifest_win64_xlc.txt';MappingSha256=(Get-FileHash tools/tcc_vfs_manifest_win64_xlc.txt).Hash;
    StageScriptSha256=(Get-FileHash $PSCommandPath).Hash;
    Scope='Disk deployment of existing compiler-relative language runtime sources and declarations; not rebuilt release/VFS or recursive import-xlang/TCC payload.'}
[IO.File]::WriteAllText((Join-Path $destination 'artifact_sources.json'), ($report | ConvertTo-Json -Depth 6), [Text.UTF8Encoding]::new($false))
Write-Output "Staged $($records.Count) source/header files without rebuilding compiler or release"
