param([Parameter(Mandatory=$true)][string]$Directory)
$ErrorActionPreference = 'Stop'
$manifestPath = Join-Path $Directory 'build.json'
$manifest = Get-Content $manifestPath -Raw | ConvertFrom-Json
if (!$manifest.RuntimeOnly -or $manifest.EnableExeCompiler -ne 0) { throw 'A real runtime-only build is required' }
if (@(@($manifest.Inputs) + @($manifest.Headers) + @($manifest.Objects) | Where-Object { (Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256 }).Count -or
    (Get-FileHash $manifest.Driver).Hash -ne $manifest.DriverSha256) { throw 'Stale runtime-only compiler inputs' }
$symbols = @(& nm -g $manifest.Driver | Select-String ' (T|I) (__imp_)?(xlRunScriptFile|xlCompileScriptFileToModule|xlRunModuleMain|xlCompileScriptStringToModule|xlCreateContext|xl6IrLowerModule|xl6IrCgenEmitCEx)$' | ForEach-Object Line)
$forbidden = @(& nm -g $manifest.Driver | Select-String ' (T|I) (__imp_)?xlc(CreateContext|Compile|Run|RelocateModule)$' | ForEach-Object Line)
$imports = @(& objdump -p $manifest.Driver | Select-String 'DLL Name:' | ForEach-Object Line)
if ($symbols.Count -ne 7 -or @($symbols | Where-Object { $_ -notmatch ' T ' }).Count -or $forbidden.Count -or $imports -match 'xlc.dll') { throw 'Runtime-only entrypoint origin mismatch' }
$files = @(@(Get-ChildItem src,lib,test/cases,test/fixtures,tools,res/tcc -Recurse -File) +
    @(Get-Item release/xl.dll,$manifest.Driver) + @($manifest.Headers | ForEach-Object { Get-Item -LiteralPath $_.Path })) | Sort-Object FullName -Unique
$inputs = @($files | ForEach-Object { [pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash} })
$outDir = 'build/test/xlir_public_runtime_cases'
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
function Invoke-Phase([string]$source, [string]$mode, [string]$symbol = '') {
    $prefix = "$outDir/$source.$mode"
    $process = Start-Process -FilePath $manifest.Driver -ArgumentList @("test/cases/$source.xl",$mode) -WindowStyle Hidden -PassThru `
        -RedirectStandardOutput "$prefix.log" -RedirectStandardError "$prefix.err.log"
    if ($process.WaitForExit(45000)) { $code = $process.ExitCode }
    else { $process.Kill(); $process.WaitForExit(); $code = 'timeout-45s' }
    $process.Dispose()
    $complete = [bool](Select-String -LiteralPath "$prefix.log" -SimpleMatch "runtime source $mode complete" -Quiet)
    $message = [string](Get-Content "$prefix.err.log" -Raw)
    $diagnostic = $symbol -ne '' -and $message -match 'incompatible|conflicting' -and $message -match [regex]::Escape($symbol)
    $passed = if ($symbol) { $code -eq 1 -and !$complete -and $diagnostic } else { $code -eq 0 -and $complete }
    [pscustomobject]@{Exit=$code;Complete=$complete;ExpectedDiagnostic=$diagnostic;Passed=$passed;Log="$prefix.log";ErrorLog="$prefix.err.log"}
}
$cases = @('cgen_basic','import_source_use','import_source_public_types','cgen_optional_nullable_function_default',
    'cgen_try_finally_pending_async','stdlib_net_fast_path','stdlib_net_send_file','stdlib_tls_server_loopback',
    'xlir_net_resolver_standalone','xlir_net_datagram_boundaries','import_package_use','import_package_include_dir','xlir_pkg_valid') |
    ForEach-Object { [pscustomobject]@{Source=$_;RejectSymbol=''} }
$cases += @([pscustomobject]@{Source='xlir_pkg_bad_width';RejectSymbol='xlir_pkg_bad_add'},
    [pscustomobject]@{Source='xlir_pkg_bad_global';RejectSymbol='xlir_pkg_bad_count'},
    [pscustomobject]@{Source='xlir_pkg_bad_destroy';RejectSymbol='xlir_pkg_box_close'})
$oldPackagePath = [Environment]::GetEnvironmentVariable('XLANG_PACKAGE_PATH','Process')
$results = @()
try {
    $env:XLANG_PACKAGE_PATH = (Resolve-Path test/fixtures).Path
    foreach ($case in $cases) {
        $run = Invoke-Phase $case.Source 'run' $case.RejectSymbol
        $module = Invoke-Phase $case.Source 'module' $case.RejectSymbol
        $results += [pscustomobject]@{Fixture=$case.Source;RejectSymbol=$case.RejectSymbol;Run=$run;Module=$module;Passed=($run.Passed -and $module.Passed)}
        Write-Output "$($case.Source) run=$($run.Exit) module=$($module.Exit) passed=$($run.Passed -and $module.Passed)"
    }
    $recovery = Invoke-Phase 'unused' 'recovery'
} finally {
    if ($null -eq $oldPackagePath) { Remove-Item Env:XLANG_PACKAGE_PATH -ErrorAction SilentlyContinue }
    else { [Environment]::SetEnvironmentVariable('XLANG_PACKAGE_PATH',$oldPackagePath,'Process') }
}
$unchanged = @($inputs | Where-Object { (Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256 }).Count -eq 0
$report = [pscustomobject]@{RecordedAt=(Get-Date).ToString('o');Results=$results;Recovery=$recovery;Inputs=$inputs;InputsUnchanged=$unchanged;
    CompilerManifest=$manifestPath;DriverSha256=$manifest.DriverSha256;Scope=$manifest.Scope;Symbols=$symbols;Imports=$imports;
    PackagePathRestored=([Environment]::GetEnvironmentVariable('XLANG_PACKAGE_PATH','Process') -eq $oldPackagePath)}
[IO.File]::WriteAllText((Join-Path (Get-Location) 'build/test/xlir_public_runtime_batch.results.json'), ($report | ConvertTo-Json -Depth 7), [Text.UTF8Encoding]::new($false))
if (!$unchanged -or !$report.PackagePathRestored -or !$recovery.Passed -or @($results | Where-Object { !$_.Passed }).Count) { throw 'All runtime-only phases collected; failures remain' }
