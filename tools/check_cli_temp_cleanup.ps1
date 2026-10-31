param(
    [string]$Root = "",
    [string]$RuntimeDirectory = "",
    [string]$WorkDirectory = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

if (!$RuntimeDirectory) { $RuntimeDirectory = Join-Path $Root 'release' }
$RuntimeDirectory = (Resolve-Path -LiteralPath $RuntimeDirectory).Path
$xl = Join-Path $RuntimeDirectory 'xl.exe'
$tmpDir = Join-Path $RuntimeDirectory 'tmp'
$artifactDir = if ($WorkDirectory) { [IO.Path]::GetFullPath($WorkDirectory) } else { Join-Path $Root 'build\test\cli_temp_cleanup' }
$failures = New-Object System.Collections.Generic.List[string]
$strictUtf8 = New-Object System.Text.UTF8Encoding($false, $true)

Set-Location $Root

function Add-Failure($message)
{
    $script:failures.Add($message) | Out-Null
}

function Join-CodePoints($codes)
{
    $builder = New-Object System.Text.StringBuilder
    foreach ( $code in $codes ) {
        [void]$builder.Append([char]$code)
    }
    return $builder.ToString()
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

function Read-CapturedText($name, $path)
{
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        Add-Failure "$name output file is missing: $path"
        return ""
    }
    try {
        $bytes = [System.IO.File]::ReadAllBytes($path)
        $text = $script:strictUtf8.GetString($bytes)
    } catch {
        Add-Failure "$name output is not strict UTF-8: $path"
        return ""
    }
    if ( $text.IndexOf([char]0xFFFD) -ge 0 ) {
        Add-Failure "$name output contains UTF-8 replacement character: $path"
    }
    if ( $text -match "CategoryInfo|FullyQualifiedErrorId|NativeCommandError" ) {
        Add-Failure "$name stderr/stdout was wrapped by PowerShell native error formatting"
    }
    return $text
}

function Assert-RawNativeOutput($name, $path)
{
    [void](Read-CapturedText $name $path)
}

function Assert-EmptyCapturedText($name, $path)
{
    $text = Read-CapturedText $name $path
    if ( $text.Length -ne 0 ) {
        Add-Failure "$name output should be empty"
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

$tmpExistedBefore = Test-Path -LiteralPath $tmpDir -PathType Container
if ( Test-Path -LiteralPath $artifactDir -PathType Container ) {
    Remove-Item -LiteralPath $artifactDir -Recurse -Force
}
New-Item -ItemType Directory -Path $artifactDir | Out-Null

$buildBefore = Count-TempDirs "xl6_build_*"
$runBefore = Count-TempDirs "xl6_run_*"

$caseBasic = "test\cases\cgen_basic.xl"
$caseMain = "test\cases\cgen_main_entry.xl"
$caseBadRun = "test\cases\sem_unknown_type.xl"
$diagUnknownType = Join-CodePoints @(0x672A, 0x77E5, 0x7C7B, 0x578B)

$buildOut = Join-Path $artifactDir "build.out.txt"
$buildErr = Join-Path $artifactDir "build.err.txt"
$buildExe = Join-Path $artifactDir "build.exe"
$result = Invoke-XlCapture -Name "build success" -CommandArgs @("build", $caseBasic, "-o", $buildExe) -Stdout $buildOut -Stderr $buildErr
Assert-RawNativeOutput "build success stdout" $buildOut
Assert-RawNativeOutput "build success stderr" $buildErr
if ( $result.Code -ne 0 ) {
    Add-Failure "build success case failed with exit code $($result.Code)"
}
Assert-EmptyCapturedText "build success stderr" $buildErr
Assert-NoTempDirDelta "successful build" "xl6_build_*" $buildBefore

$runOut = Join-Path $artifactDir "run.out.txt"
$runErr = Join-Path $artifactDir "run.err.txt"
$result = Invoke-XlCapture -Name "run success" -CommandArgs @("run", $caseBasic) -Stdout $runOut -Stderr $runErr
Assert-RawNativeOutput "run success stdout" $runOut
Assert-RawNativeOutput "run success stderr" $runErr
if ( $result.Code -ne 0 ) {
    Add-Failure "run success case failed with exit code $($result.Code)"
}
Assert-EmptyCapturedText "run success stderr" $runErr
Assert-NoTempDirDelta "successful run" "xl6_run_*" $runBefore

$buildFailOut = Join-Path $artifactDir "build_fail.out.txt"
$buildFailErr = Join-Path $artifactDir "build_fail.err.txt"
$result = Invoke-XlCapture -Name "build failure" -CommandArgs @("build", $caseMain, "-shared", "-o", (Join-Path $artifactDir "bad.dll")) -Stdout $buildFailOut -Stderr $buildFailErr
Assert-RawNativeOutput "build failure stdout" $buildFailOut
Assert-RawNativeOutput "build failure stderr" $buildFailErr
$buildFailText = Read-CapturedText "build failure stderr" $buildFailErr
if ( $result.Code -eq 0 ) {
    Add-Failure "build failure case unexpectedly succeeded"
} elseif ( $buildFailText -notmatch "shared build cannot contain xlang main" ) {
    Add-Failure "build failure case did not emit expected diagnostic"
}
Assert-NoTempDirDelta "failed build" "xl6_build_*" $buildBefore

$runFailOut = Join-Path $artifactDir "run_fail.out.txt"
$runFailErr = Join-Path $artifactDir "run_fail.err.txt"
$result = Invoke-XlCapture -Name "run failure" -CommandArgs @("run", $caseBadRun) -Stdout $runFailOut -Stderr $runFailErr
Assert-RawNativeOutput "run failure stdout" $runFailOut
Assert-RawNativeOutput "run failure stderr" $runFailErr
$runFailText = Read-CapturedText "run failure stderr" $runFailErr
if ( $result.Code -eq 0 ) {
    Add-Failure "run failure case unexpectedly succeeded"
} elseif ( $runFailText.IndexOf($diagUnknownType) -lt 0 ) {
    Add-Failure "run failure case did not emit expected semantic diagnostic"
}
Assert-NoTempDirDelta "failed run" "xl6_run_*" $runBefore

$saveOut = Join-Path $artifactDir "save_temps.out.txt"
$saveErr = Join-Path $artifactDir "save_temps.err.txt"
$saveExe = Join-Path $artifactDir "save_temps.exe"
$result = Invoke-XlCapture -Name "save temps build" -CommandArgs @("build", $caseBasic, "--save-temps", "-o", $saveExe) -Stdout $saveOut -Stderr $saveErr
Assert-RawNativeOutput "save temps stdout" $saveOut
Assert-RawNativeOutput "save temps stderr" $saveErr
if ( $result.Code -ne 0 ) {
    Add-Failure "save-temps build failed with exit code $($result.Code)"
} else {
    $log = (Read-CapturedText "save temps stdout" $saveOut) + "`n" + (Read-CapturedText "save temps stderr" $saveErr)
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

$remainingTmpItems = if (Test-Path -LiteralPath $tmpDir -PathType Container) { @(Get-ChildItem -LiteralPath $tmpDir -Force) } else { @() }
if ( $remainingTmpItems.Count -ne 0 ) {
    Add-Failure "release tmp must be empty after CLI temp cleanup: $($remainingTmpItems.Count) item(s) remain"
}
if (!$tmpExistedBefore -and $remainingTmpItems.Count -eq 0 -and (Test-Path -LiteralPath $tmpDir -PathType Container)) {
    # Remove only the empty parent introduced by these checks. A racing writer
    # makes this non-recursive removal fail, preserving its contents.
    $runtimePath = (Resolve-Path -LiteralPath $RuntimeDirectory).Path
    $resolvedTmp = (Resolve-Path -LiteralPath $tmpDir).Path
    if ($resolvedTmp -ne [IO.Path]::GetFullPath((Join-Path $runtimePath 'tmp')) -or
        ((Get-Item -LiteralPath $runtimePath -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) -or
        ((Get-Item -LiteralPath $resolvedTmp -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        Add-Failure 'Refusing an unexpected or reparse temp parent'
    } else {
        try { Remove-Item -LiteralPath $resolvedTmp -ErrorAction Stop }
        catch { Add-Failure ("Cannot remove the owned empty temp parent: " + $_.Exception.Message) }
    }
}

if ( $failures.Count -gt 0 ) {
    foreach ( $failure in $failures ) {
        Write-Host $failure
    }
    exit 1
}

Write-Host "CLI temp cleanup ok: release tmp build dirs=$buildBefore, run dirs=$runBefore."
exit 0
