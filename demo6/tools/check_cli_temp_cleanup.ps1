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

function Count-TempDirs($pattern)
{
    if ( ! (Test-Path -LiteralPath $script:tmpDir -PathType Container) ) {
        return 0
    }
    return @(Get-ChildItem -LiteralPath $script:tmpDir -Directory -Filter $pattern -ErrorAction SilentlyContinue).Count
}

function Invoke-XlCapture
{
    param(
        [string]$Name,
        [string[]]$CommandArgs,
        [string]$Stdout,
        [string]$Stderr
    )

    foreach ( $path in @($Stdout, $Stderr) ) {
        if ( Test-Path -LiteralPath $path -PathType Leaf ) {
            Remove-Item -LiteralPath $path -Force
        }
    }

    $process = Start-Process `
        -FilePath $script:xl `
        -ArgumentList $CommandArgs `
        -NoNewWindow `
        -PassThru `
        -Wait `
        -RedirectStandardOutput $Stdout `
        -RedirectStandardError $Stderr

    return [pscustomobject]@{
        Name = $Name
        Code = $process.ExitCode
        Stdout = $Stdout
        Stderr = $Stderr
    }
}

function Assert-RawNativeOutput($name, $path)
{
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        return
    }
    $text = Get-Content -LiteralPath $path -Raw
    if ( $text -match "CategoryInfo|FullyQualifiedErrorId|NativeCommandError" ) {
        Add-Failure "$name stderr/stdout was wrapped by PowerShell native error formatting"
    }
}

function Assert-NoTempDirDelta($name, $pattern, $before)
{
    $after = Count-TempDirs $pattern
    if ( $after -ne $before ) {
        Add-Failure "$name leaked $pattern directories: before=$before after=$after"
    }
}

function Assert-WithinTmpDir($path)
{
    $resolvedTmp = (Resolve-Path -LiteralPath $script:tmpDir).Path
    $resolvedPath = (Resolve-Path -LiteralPath $path).Path
    if ( ! $resolvedTmp.EndsWith("\") ) {
        $resolvedTmp = $resolvedTmp + "\"
    }
    if ( ! $resolvedPath.StartsWith($resolvedTmp, [System.StringComparison]::OrdinalIgnoreCase) ) {
        throw "save-temps directory is outside release\tmp: $resolvedPath"
    }
    return $resolvedPath
}

if ( ! (Test-Path -LiteralPath $xl -PathType Leaf) ) {
    Write-Host "release xl.exe missing: $xl"
    exit 1
}

if ( ! (Test-Path -LiteralPath $tmpDir -PathType Container) ) {
    New-Item -ItemType Directory -Path $tmpDir | Out-Null
}

$buildBefore = Count-TempDirs "xl6_build_*"
$runBefore = Count-TempDirs "xl6_run_*"

$caseBasic = "test\cases\cgen_basic.xl"
$caseMain = "test\cases\cgen_main_entry.xl"
$caseBadRun = "test\cases\sem_unknown_type.xl"

$buildOut = Join-Path $tmpDir "cli_temp_cleanup_build.out.txt"
$buildErr = Join-Path $tmpDir "cli_temp_cleanup_build.err.txt"
$buildExe = Join-Path $tmpDir "cli_temp_cleanup_build.exe"
$result = Invoke-XlCapture -Name "build success" -CommandArgs @("build", $caseBasic, "-o", $buildExe) -Stdout $buildOut -Stderr $buildErr
Assert-RawNativeOutput "build success stdout" $buildOut
Assert-RawNativeOutput "build success stderr" $buildErr
if ( $result.Code -ne 0 ) {
    Add-Failure "build success case failed with exit code $($result.Code)"
}
Assert-NoTempDirDelta "successful build" "xl6_build_*" $buildBefore

$runOut = Join-Path $tmpDir "cli_temp_cleanup_run.out.txt"
$runErr = Join-Path $tmpDir "cli_temp_cleanup_run.err.txt"
$result = Invoke-XlCapture -Name "run success" -CommandArgs @("run", $caseBasic) -Stdout $runOut -Stderr $runErr
Assert-RawNativeOutput "run success stdout" $runOut
Assert-RawNativeOutput "run success stderr" $runErr
if ( $result.Code -ne 0 ) {
    Add-Failure "run success case failed with exit code $($result.Code)"
}
Assert-NoTempDirDelta "successful run" "xl6_run_*" $runBefore

$buildFailOut = Join-Path $tmpDir "cli_temp_cleanup_build_fail.out.txt"
$buildFailErr = Join-Path $tmpDir "cli_temp_cleanup_build_fail.err.txt"
$result = Invoke-XlCapture -Name "build failure" -CommandArgs @("build", $caseMain, "-shared", "-o", (Join-Path $tmpDir "cli_temp_cleanup_bad.dll")) -Stdout $buildFailOut -Stderr $buildFailErr
Assert-RawNativeOutput "build failure stdout" $buildFailOut
Assert-RawNativeOutput "build failure stderr" $buildFailErr
if ( $result.Code -eq 0 ) {
    Add-Failure "build failure case unexpectedly succeeded"
} elseif ( (Get-Content -LiteralPath $buildFailErr -Raw) -notmatch "shared build cannot contain xlang main" ) {
    Add-Failure "build failure case did not emit expected diagnostic"
}
Assert-NoTempDirDelta "failed build" "xl6_build_*" $buildBefore

$runFailOut = Join-Path $tmpDir "cli_temp_cleanup_run_fail.out.txt"
$runFailErr = Join-Path $tmpDir "cli_temp_cleanup_run_fail.err.txt"
$result = Invoke-XlCapture -Name "run failure" -CommandArgs @("run", $caseBadRun) -Stdout $runFailOut -Stderr $runFailErr
Assert-RawNativeOutput "run failure stdout" $runFailOut
Assert-RawNativeOutput "run failure stderr" $runFailErr
if ( $result.Code -eq 0 ) {
    Add-Failure "run failure case unexpectedly succeeded"
}
Assert-NoTempDirDelta "failed run" "xl6_run_*" $runBefore

$saveOut = Join-Path $tmpDir "cli_temp_cleanup_save_temps.out.txt"
$saveErr = Join-Path $tmpDir "cli_temp_cleanup_save_temps.err.txt"
$saveExe = Join-Path $tmpDir "cli_temp_cleanup_save_temps.exe"
$result = Invoke-XlCapture -Name "save temps build" -CommandArgs @("build", $caseBasic, "--save-temps", "-o", $saveExe) -Stdout $saveOut -Stderr $saveErr
Assert-RawNativeOutput "save temps stdout" $saveOut
Assert-RawNativeOutput "save temps stderr" $saveErr
if ( $result.Code -ne 0 ) {
    Add-Failure "save-temps build failed with exit code $($result.Code)"
} else {
    $log = (Get-Content -LiteralPath $saveOut -Raw) + "`n" + (Get-Content -LiteralPath $saveErr -Raw)
    $match = [regex]::Match($log, "temporary files kept:\s*(.+)")
    if ( ! $match.Success ) {
        Add-Failure "save-temps build did not report kept directory"
    } else {
        $keptDir = $match.Groups[1].Value.Trim()
        if ( ! (Test-Path -LiteralPath $keptDir -PathType Container) ) {
            Add-Failure "save-temps kept directory is missing: $keptDir"
        } else {
            try {
                $safeDir = Assert-WithinTmpDir $keptDir
                Remove-Item -LiteralPath $safeDir -Recurse -Force
            } catch {
                Add-Failure $_.Exception.Message
            }
        }
    }
}
Assert-NoTempDirDelta "save-temps cleanup" "xl6_build_*" $buildBefore

if ( $failures.Count -gt 0 ) {
    foreach ( $failure in $failures ) {
        Write-Host $failure
    }
    exit 1
}

Write-Host "CLI temp cleanup ok: build dirs=$buildBefore, run dirs=$runBefore."
exit 0
