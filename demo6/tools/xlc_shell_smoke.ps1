param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$tmpDir = Join-Path $Root "release\tmp"
$failures = New-Object System.Collections.Generic.List[string]

Set-Location $Root

function Add-Failure($message)
{
    $script:failures.Add($message) | Out-Null
}

function Read-Text($path)
{
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        Add-Failure "missing output file: $path"
        return ""
    }
    return [System.IO.File]::ReadAllText($path, [System.Text.Encoding]::UTF8)
}

function Invoke-NativeCapture
{
    param(
        [string]$Name,
        [string]$FilePath,
        [string[]]$CommandArgs,
        [string]$Stdout,
        [string]$Stderr,
        [int]$ExpectedCode = 0
    )

    foreach ( $path in @($Stdout, $Stderr) ) {
        if ( Test-Path -LiteralPath $path -PathType Leaf ) {
            Remove-Item -LiteralPath $path -Force
        }
    }

    $startArgs = @{
        FilePath = $FilePath
        NoNewWindow = $true
        PassThru = $true
        Wait = $true
        RedirectStandardOutput = $Stdout
        RedirectStandardError = $Stderr
    }
    if ( $CommandArgs -ne $null -and $CommandArgs.Count -gt 0 ) {
        $startArgs.ArgumentList = $CommandArgs
    }

    $process = Start-Process @startArgs

    if ( $process.ExitCode -ne $ExpectedCode ) {
        Add-Failure "$Name exit code $($process.ExitCode), expected $ExpectedCode"
    }
    return $process.ExitCode
}

function Invoke-XlCapture
{
    param(
        [string]$Name,
        [string[]]$CommandArgs,
        [string]$Stdout,
        [string]$Stderr,
        [int]$ExpectedCode = 0
    )

    return Invoke-NativeCapture -Name $Name -FilePath $script:xl -CommandArgs $CommandArgs -Stdout $Stdout -Stderr $Stderr -ExpectedCode $ExpectedCode
}

function Assert-Contains($name, $text, $needle)
{
    if ( $text.IndexOf($needle, [System.StringComparison]::Ordinal) -lt 0 ) {
        Add-Failure "$name missing expected text: $needle"
    }
}

if ( ! (Test-Path -LiteralPath $xl -PathType Leaf) ) {
    Write-Host "release xl.exe missing: $xl"
    exit 1
}

if ( ! (Test-Path -LiteralPath $tmpDir -PathType Container) ) {
    New-Item -ItemType Directory -Path $tmpDir | Out-Null
}

$helpOut = Join-Path $tmpDir "xlc_shell_help.out.txt"
$helpErr = Join-Path $tmpDir "xlc_shell_help.err.txt"
Invoke-XlCapture -Name "help" -CommandArgs @("--help") -Stdout $helpOut -Stderr $helpErr | Out-Null
$help = Read-Text $helpOut
foreach ( $cmd in @("xl lex", "xl parse", "xl check", "xl ir", "xl cgen", "xl abi", "xl xapi", "xl run", "xl dbg", "xl dap", "xl build") ) {
    Assert-Contains "help" $help $cmd
}

$case = "release\examples\stdlib_print.xl"

Invoke-XlCapture -Name "check" -CommandArgs @("check", $case) -Stdout (Join-Path $tmpDir "xlc_shell_check.out.txt") -Stderr (Join-Path $tmpDir "xlc_shell_check.err.txt") | Out-Null
Invoke-XlCapture -Name "lex json" -CommandArgs @("lex", $case, "--format", "json", "--emit", "release\tmp\xlc_shell.tokens.json") -Stdout (Join-Path $tmpDir "xlc_shell_lex.out.txt") -Stderr (Join-Path $tmpDir "xlc_shell_lex.err.txt") | Out-Null
Invoke-XlCapture -Name "parse json" -CommandArgs @("parse", $case, "--format", "json", "--emit", "release\tmp\xlc_shell.ast.json") -Stdout (Join-Path $tmpDir "xlc_shell_parse.out.txt") -Stderr (Join-Path $tmpDir "xlc_shell_parse.err.txt") | Out-Null
Invoke-XlCapture -Name "ir" -CommandArgs @("ir", $case, "--emit", "release\tmp\xlc_shell.xlir") -Stdout (Join-Path $tmpDir "xlc_shell_ir.out.txt") -Stderr (Join-Path $tmpDir "xlc_shell_ir.err.txt") | Out-Null
Invoke-XlCapture -Name "cgen" -CommandArgs @("cgen", $case, "--emit", "release\tmp\xlc_shell.c", "--emit-header", "release\tmp\xlc_shell.h") -Stdout (Join-Path $tmpDir "xlc_shell_cgen.out.txt") -Stderr (Join-Path $tmpDir "xlc_shell_cgen.err.txt") | Out-Null
Invoke-XlCapture -Name "abi" -CommandArgs @("abi", $case, "--json", "--emit", "release\tmp\xlc_shell.abi.json") -Stdout (Join-Path $tmpDir "xlc_shell_abi.out.txt") -Stderr (Join-Path $tmpDir "xlc_shell_abi.err.txt") | Out-Null
Invoke-XlCapture -Name "xapi" -CommandArgs @("xapi", $case, "--emit", "release\tmp\xlc_shell.xapi.json") -Stdout (Join-Path $tmpDir "xlc_shell_xapi.out.txt") -Stderr (Join-Path $tmpDir "xlc_shell_xapi.err.txt") | Out-Null

$runOut = Join-Path $tmpDir "xlc_shell_run.out.txt"
$runErr = Join-Path $tmpDir "xlc_shell_run.err.txt"
Invoke-XlCapture -Name "run" -CommandArgs @("run", $case) -Stdout $runOut -Stderr $runErr | Out-Null
Assert-Contains "run output" (Read-Text $runOut) "xlang print 123 done"

$buildExe = Join-Path $tmpDir "xlc_shell_build.exe"
Invoke-XlCapture -Name "build exe" -CommandArgs @("build", $case, "-o", $buildExe) -Stdout (Join-Path $tmpDir "xlc_shell_build.out.txt") -Stderr (Join-Path $tmpDir "xlc_shell_build.err.txt") | Out-Null
$exeOut = Join-Path $tmpDir "xlc_shell_build_run.out.txt"
$exeErr = Join-Path $tmpDir "xlc_shell_build_run.err.txt"
Invoke-NativeCapture -Name "built exe" -FilePath $buildExe -CommandArgs @() -Stdout $exeOut -Stderr $exeErr | Out-Null
Assert-Contains "built exe output" (Read-Text $exeOut) "xlang print 123 done"

$mixedRunOut = Join-Path $tmpDir "xlc_shell_mixed_c_direct_run.out.txt"
$mixedRunErr = Join-Path $tmpDir "xlc_shell_mixed_c_direct_run.err.txt"
Invoke-XlCapture `
    -Name "run xl plus c helper" `
    -CommandArgs @("run", "test\cases\cgen_task_api.xl", "test\cases\cgen_class_host.c") `
    -Stdout $mixedRunOut `
    -Stderr $mixedRunErr | Out-Null

$mixedExe = Join-Path $tmpDir "xlc_shell_mixed_c.exe"
Invoke-XlCapture `
    -Name "build xl plus c helper" `
    -CommandArgs @("build", "test\cases\cgen_task_api.xl", "test\cases\cgen_class_host.c", "-o", $mixedExe) `
    -Stdout (Join-Path $tmpDir "xlc_shell_mixed_c.out.txt") `
    -Stderr (Join-Path $tmpDir "xlc_shell_mixed_c.err.txt") | Out-Null
Invoke-NativeCapture `
    -Name "xl plus c helper exe" `
    -FilePath $mixedExe `
    -CommandArgs @() `
    -Stdout (Join-Path $tmpDir "xlc_shell_mixed_c_run.out.txt") `
    -Stderr (Join-Path $tmpDir "xlc_shell_mixed_c_run.err.txt") | Out-Null

$sharedSource = Join-Path $tmpDir "xlc_shell_shared.xl"
[System.IO.File]::WriteAllText(
    $sharedSource,
    "export `"xlc_shell_answer`" func answer() as int`n    return 42`nend`n",
    [System.Text.Encoding]::UTF8)

$sharedDll = Join-Path $tmpDir "xlc_shell_shared.dll"
$sharedHeader = Join-Path $tmpDir "xlc_shell_shared.h"
Invoke-XlCapture -Name "build shared" -CommandArgs @("build", $sharedSource, "-shared", "-o", $sharedDll, "--emit-header", $sharedHeader) -Stdout (Join-Path $tmpDir "xlc_shell_shared.out.txt") -Stderr (Join-Path $tmpDir "xlc_shell_shared.err.txt") | Out-Null
foreach ( $path in @($sharedDll, $sharedHeader) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        Add-Failure "shared build missing: $path"
    }
}

$badSharedErr = Join-Path $tmpDir "xlc_shell_bad_shared.err.txt"
Invoke-XlCapture -Name "reject shared main" -CommandArgs @("build", "test\cases\cgen_main_entry.xl", "-shared", "-o", "release\tmp\xlc_shell_bad_shared.dll") -Stdout (Join-Path $tmpDir "xlc_shell_bad_shared.out.txt") -Stderr $badSharedErr -ExpectedCode 1 | Out-Null
Assert-Contains "reject shared main" (Read-Text $badSharedErr) "shared build cannot contain xlang main"

$dbgOut = Join-Path $tmpDir "xlc_shell_dbg.out.txt"
$dbgErr = Join-Path $tmpDir "xlc_shell_dbg.err.txt"
Invoke-XlCapture -Name "debug" -CommandArgs @("dbg", "release\examples\hello.xl", "--break", "2", "--commands", "test\data\dbg_commands.txt", "--save-temps") -Stdout $dbgOut -Stderr $dbgErr | Out-Null
$dbgText = Read-Text $dbgOut
Assert-Contains "debug output" $dbgText "xl-debug stopped reason=breakpoint"
Assert-Contains "debug output" $dbgText "function=add"
Assert-Contains "debug output" $dbgText "xl-debug frame #0"
$dbgErrText = Read-Text $dbgErr
$dbgMatch = [regex]::Match($dbgErrText, "temporary files kept:\s*(.+)")
if ( ! $dbgMatch.Success ) {
    Add-Failure "debug did not report kept temporary directory"
} else {
    $dbgDir = $dbgMatch.Groups[1].Value.Trim()
    $dbgC = Join-Path $dbgDir "module.c"
    if ( ! (Test-Path -LiteralPath $dbgC -PathType Leaf) ) {
        Add-Failure "debug module.c is missing: $dbgC"
    } else {
        Assert-Contains "debug module.c" (Read-Text $dbgC) "xl6_debug_probe"
    }
    try {
        $resolvedTmp = (Resolve-Path -LiteralPath $tmpDir).Path
        $resolvedDbg = (Resolve-Path -LiteralPath $dbgDir).Path
        if ( ! $resolvedTmp.EndsWith("\") ) {
            $resolvedTmp += "\"
        }
        if ( $resolvedDbg.StartsWith($resolvedTmp, [System.StringComparison]::OrdinalIgnoreCase) ) {
            Remove-Item -LiteralPath $resolvedDbg -Recurse -Force
        }
    } catch {
        Add-Failure $_.Exception.Message
    }
}

$cmdOut = Join-Path $tmpDir "xlc_shell_default_links.out.txt"
$cmdErr = Join-Path $tmpDir "xlc_shell_default_links.err.txt"
Invoke-NativeCapture `
    -Name "default link args" `
    -FilePath "cmd.exe" `
    -CommandArgs @("/v:on", "/s", "/c", "call tools\xl6_toolchain_env.bat && release\xl.exe run release\examples\stdlib_print.xl !XL6_XL_DEFAULT_LINK_ARGS!") `
    -Stdout $cmdOut `
    -Stderr $cmdErr | Out-Null
Assert-Contains "default link args output" (Read-Text $cmdOut) "xlang print 123 done"

if ( $failures.Count -gt 0 ) {
    foreach ( $failure in $failures ) {
        Write-Host $failure
    }
    exit 1
}

Write-Host "xlc.dll shell smoke ok."
exit 0
