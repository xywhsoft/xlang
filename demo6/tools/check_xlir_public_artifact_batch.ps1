param([Parameter(Mandatory=$true)][string]$Directory)
$ErrorActionPreference = 'Stop'
$manifestPath = Join-Path $Directory 'build.json'
$manifest = Get-Content $manifestPath -Raw | ConvertFrom-Json
$packagePath = Join-Path $Directory 'artifact_sources.json'
$packageFiles = @()
if (Test-Path -LiteralPath $packagePath) {
    $package = Get-Content $packagePath -Raw | ConvertFrom-Json
    if (@($package.Files | Where-Object { (Get-FileHash -LiteralPath $_.Source).Hash -ne $_.Sha256 -or (Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256 }).Count) { throw 'Stale artifact runtime source package' }
    $packageFiles = @($package.Files | ForEach-Object { Get-Item -LiteralPath $_.Path })
}
if ($manifest.RuntimeOnly -or $manifest.EnableExeCompiler -ne 1) { throw 'Compiler-enabled source build required' }
if (@(@($manifest.Inputs) + @($manifest.Headers) + @($manifest.Objects) | Where-Object { (Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256 }).Count -or
    (Get-FileHash $manifest.Driver).Hash -ne $manifest.DriverSha256) { throw 'Stale artifact compiler inputs' }
$outDir = 'build/test/xlir_public_artifact_cases'
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$hostPath = Join-Path (Resolve-Path $outDir) 'dll_host.exe'
& gcc -std=c11 -Wall -Wextra -Werror test/cases/xlir_artifact_dll_host.c -o $hostPath *> "$outDir/host_build.log"
if ($LASTEXITCODE -ne 0) { throw 'DLL probe host failed to build' }
$files = @(@(Get-ChildItem src,lib,test/cases,test/fixtures,tools,res/tcc -Recurse -File) +
    @(Get-Item release/xl.dll,$manifest.Driver,$hostPath) + $packageFiles + @($manifest.Headers | ForEach-Object { Get-Item -LiteralPath $_.Path })) | Sort-Object FullName -Unique
$inputs = @($files | ForEach-Object { [pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash} })
function Invoke-Process([string]$file, [string[]]$arguments, [string]$log) {
    $options = @{FilePath=$file;WindowStyle='Hidden';PassThru=$true;RedirectStandardOutput="$log.log";RedirectStandardError="$log.err.log"}
    if ($arguments.Count) { $options.ArgumentList = $arguments }
$process = Start-Process @options
    $null = $process.Handle
    if ($process.WaitForExit(45000)) { $code = $process.ExitCode }
    else { $process.Kill(); $process.WaitForExit(); $code = 'timeout-45s' }
    $process.Dispose()
    [pscustomobject]@{Exit=$code;Log="$log.log";ErrorLog="$log.err.log"}
}
$cases = @(
    @{Source='cgen_optional_nullable_function_default';Mode='exe';Hosts=@()},
    @{Source='cgen_try_finally_pending_async';Mode='exe';Hosts=@()},
    @{Source='xlir_pkg_valid';Mode='exe';Hosts=@()},
    @{Source='xlir_pkg_mixed';Mode='exe';Hosts=@('cgen_extern_function_value_host')},
    @{Source='xlir_net_resolver_standalone';Mode='exe';Hosts=@()},
    @{Source='xlir_pkg_bad_width';Mode='exe';Hosts=@();Symbol='xlir_pkg_bad_add'},
    @{Source='xlir_pkg_bad_global';Mode='exe';Hosts=@();Symbol='xlir_pkg_bad_count'},
    @{Source='xlir_pkg_bad_destroy';Mode='exe';Hosts=@();Symbol='xlir_pkg_box_close'},
    @{Source='xlir_pkg_mixed';Mode='exe';Hosts=@('xlir_pkg_mixed_bad_host');Symbol='native_add_i64'},
    @{Source='xlir_artifact_scalar';Mode='dll';Hosts=@()},
    @{Source='xlir_artifact_package';Mode='dll';Hosts=@()},
    @{Source='xlir_artifact_string';Mode='dll';Hosts=@()},
    @{Source='xlir_artifact_bad_width';Mode='dll';Hosts=@();Symbol='xlir_pkg_bad_add'},
    @{Source='xlir_artifact_bad_global';Mode='dll';Hosts=@();Symbol='xlir_pkg_bad_count'},
    @{Source='xlir_artifact_bad_destroy';Mode='dll';Hosts=@();Symbol='xlir_pkg_box_close'},
    @{Source='xlir_artifact_global';Mode='dll';Hosts=@();Global=$true;Added=$true}
)
$oldPackagePath = [Environment]::GetEnvironmentVariable('XLANG_PACKAGE_PATH','Process')
$results = @()
try {
    $env:XLANG_PACKAGE_PATH = (Resolve-Path test/fixtures).Path
    foreach ($case in $cases) {
        $name = "$($case.Source)_$($case.Mode)"; if ($case.Symbol) { $name += '_reject' }
        $prefix = Join-Path (Resolve-Path $outDir) $name
        $artifact = "$prefix.$($case.Mode)"
        # This gate owns this output. Seed both success and rejection probes so
        # an older successful artifact cannot make a broken emission look green.
        [IO.File]::WriteAllText($artifact, 'KEEP', [Text.UTF8Encoding]::new($false))
        $arguments = @("test/cases/$($case.Source).xl",$case.Mode,$prefix) + @($case.Hosts | ForEach-Object { "test/cases/$_.c" })
        $build = Invoke-Process $manifest.Driver $arguments "$prefix.build"
        $complete = [bool](Select-String -LiteralPath $build.Log -SimpleMatch "public source $($case.Mode) complete" -Quiet)
        $message = [string](Get-Content $build.ErrorLog -Raw)
        $bytes = [IO.File]::ReadAllBytes($artifact)
        $kept = $bytes.Length -eq 4 -and [Text.Encoding]::UTF8.GetString($bytes) -eq 'KEEP'
        $pe = $bytes.Length -gt 64 -and $bytes[0] -eq 77 -and $bytes[1] -eq 90
        $run = $null; $imports = @(); $passed = $false; $importCheck = $null
        if ($case.Symbol) {
            $passed = $build.Exit -eq 1 -and !$complete -and $kept -and
                $message -match 'incompatible|conflicting' -and $message -match [regex]::Escape($case.Symbol)
        } elseif ($build.Exit -eq 0 -and $complete -and $pe) {
            $imports = @(& objdump -p $artifact | Select-String 'DLL Name:' | ForEach-Object Line)
            if ($case.Mode -eq 'exe') { $run = Invoke-Process $artifact @() "$prefix.execute" }
            else {
                $hostArguments = @($artifact); if ($case.Global) { $hostArguments += 'global' }
                $run = Invoke-Process $hostPath $hostArguments "$prefix.execute"
            }
            $passed = $run.Exit -eq 0 -and !($imports -match 'DLL Name:\s+xlc?\.dll')
            if ($case.Mode -eq 'dll') { $passed = $passed -and [bool](Select-String -LiteralPath $run.Log -SimpleMatch 'artifact DLL check and cleanup complete' -Quiet) }
            if ($case.Global) {
                $header = Invoke-Process $manifest.Driver @("test/cases/$($case.Source).xl",'emit',"$prefix.import") "$prefix.header"
                $linked = $null; $importRun = $null
                if ($header.Exit -eq 0) {
                    & gcc -std=c11 -Wall -Wextra -Werror -DXL6_USE_SHARED=1 -include "$prefix.import.h" test/cases/xlir_artifact_global_import_host.c $artifact -o "$prefix.import.exe" *> "$prefix.import.build.log"
                    $linked = $LASTEXITCODE
                    if ($linked -eq 0) { $importRun = Invoke-Process "$prefix.import.exe" @() "$prefix.import.execute" }
                }
                $importPassed = $header.Exit -eq 0 -and $linked -eq 0 -and $importRun -and $importRun.Exit -eq 0
                if ($importPassed) { $importPassed = [bool](Select-String -LiteralPath $importRun.Log -SimpleMatch 'artifact imported globals and function complete' -Quiet) }
                $importCheck = [pscustomobject]@{Header=$header;LinkExit=$linked;Run=$importRun;Passed=[bool]$importPassed}
                $passed = $passed -and $importPassed
            }
        }
        $results += [pscustomobject]@{Fixture=$name;Source=$case.Source;Mode=$case.Mode;Hosts=$case.Hosts;Added=[bool]$case.Added;RejectSymbol=$case.Symbol;
            Build=$build;BuildComplete=$complete;OutputPreserved=$kept;Pe=$pe;Run=$run;Imports=$imports;ImportCheck=$importCheck;Passed=$passed;
            Artifact=$artifact;ArtifactSha256=(Get-FileHash -LiteralPath $artifact).Hash}
        Write-Output "$name build=$($build.Exit) run=$($run.Exit) preserved=$kept passed=$passed"
    }
} finally {
    if ($null -eq $oldPackagePath) { Remove-Item Env:XLANG_PACKAGE_PATH -ErrorAction SilentlyContinue }
    else { [Environment]::SetEnvironmentVariable('XLANG_PACKAGE_PATH',$oldPackagePath,'Process') }
}
$unchanged = @($inputs | Where-Object { (Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256 }).Count -eq 0
$report = [pscustomobject]@{RecordedAt=(Get-Date).ToString('o');Results=$results;Inputs=$inputs;InputsUnchanged=$unchanged;
    CompilerManifest=$manifestPath;DriverSha256=$manifest.DriverSha256;Scope=$manifest.Scope;SourcePackage=$packagePath;
    PackagePathRestored=([Environment]::GetEnvironmentVariable('XLANG_PACKAGE_PATH','Process') -eq $oldPackagePath)}
[IO.File]::WriteAllText((Join-Path (Get-Location) 'build/test/xlir_public_artifact_batch.results.json'), ($report | ConvertTo-Json -Depth 7), [Text.UTF8Encoding]::new($false))
if (!$unchanged -or !$report.PackagePathRestored -or @($results | Where-Object { !$_.Passed }).Count) { throw 'All external artifact phases collected; failures remain' }
