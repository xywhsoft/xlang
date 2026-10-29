$ErrorActionPreference = "Stop"

$root = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
Set-Location $root

function Invoke-XlExpectExit {
    param(
        [string]$Label,
        [string[]]$ArgsList,
        [int]$Expected
    )

    & ".\release\xl.exe" @ArgsList
    $code = $LASTEXITCODE
    if ($code -ne $Expected) {
        Write-Host "$Label exit mismatch: $code, expected $Expected"
        exit 1
    }
}

function Invoke-XlExpectFailure {
    param(
        [string]$Label,
        [string[]]$ArgsList,
        [string]$ExpectedText = ""
    )

    $safeLabel = $Label -replace "[^A-Za-z0-9_.-]", "_"
    $errPath = Join-Path $root "build\test\$safeLabel.err.txt"
    $outPath = Join-Path $root "build\test\$safeLabel.out.txt"
    $process = Start-Process `
        -FilePath (Join-Path $root "release\xl.exe") `
        -ArgumentList $ArgsList `
        -NoNewWindow `
        -Wait `
        -PassThru `
        -RedirectStandardOutput $outPath `
        -RedirectStandardError $errPath
    $code = $process.ExitCode
    if ($code -eq 0) {
        Write-Host "$Label should fail but exited 0"
        exit 1
    }
    if ($ExpectedText.Length -gt 0) {
        $text = [System.IO.File]::ReadAllText($errPath, [System.Text.Encoding]::UTF8)
        if ($text.IndexOf($ExpectedText, [System.StringComparison]::Ordinal) -lt 0) {
            Write-Host "$Label missing expected diagnostic: $ExpectedText"
            exit 1
        }
    }
}

Invoke-XlExpectFailure `
    -Label "dynamic array out-of-range set" `
    -ArgsList @("run", "test\cases\cgen_issue_dynamic_array_oob_set.xl") `
    -ExpectedText "RangeError"

Invoke-XlExpectExit `
    -Label "try return" `
    -ArgsList @("run", "test\cases\cgen_issue_try_return.xl") `
    -Expected 1

Invoke-XlExpectExit `
    -Label "main return" `
    -ArgsList @("run", "test\cases\cgen_issue_exit_code.xl") `
    -Expected 7

Invoke-XlExpectExit `
    -Label "dev feedback priority run" `
    -ArgsList @("run", "test\cases\cgen_dev_feedback_priority.xl") `
    -Expected 0

Invoke-XlExpectExit `
    -Label "stdlib codec crypto buffer run" `
    -ArgsList @("run", "test\cases\stdlib_dev_codec_crypto_buffer_build.xl") `
    -Expected 0

Invoke-XlExpectExit `
    -Label "utf8 argv run" `
    -ArgsList @("run", "test\cases\cgen_issue_utf8_argv.xl", "--", "milk") `
    -Expected 0

& ".\release\xl.exe" build "test\cases\cgen_issue_utf8_argv.xl" -o "build\test\cgen_issue_utf8_argv.exe"
if ($LASTEXITCODE -ne 0) {
    Write-Host "utf8 argv build failed: $LASTEXITCODE"
    exit 1
}

& ".\build\test\cgen_issue_utf8_argv.exe" "milk"
if ($LASTEXITCODE -ne 0) {
    Write-Host "utf8 argv exe failed: $LASTEXITCODE"
    exit 1
}

& ".\release\xl.exe" build "test\cases\cgen_dev_feedback_priority.xl" -o "build\test\cgen_dev_feedback_priority.exe"
if ($LASTEXITCODE -ne 0) {
    Write-Host "dev feedback priority build failed: $LASTEXITCODE"
    exit 1
}

& ".\build\test\cgen_dev_feedback_priority.exe"
if ($LASTEXITCODE -ne 0) {
    Write-Host "dev feedback priority exe failed: $LASTEXITCODE"
    exit 1
}

& ".\release\xl.exe" build "test\cases\stdlib_dev_codec_crypto_buffer_build.xl" -o "build\test\stdlib_dev_codec_crypto_buffer_build.exe"
if ($LASTEXITCODE -ne 0) {
    Write-Host "stdlib codec crypto buffer build failed: $LASTEXITCODE"
    exit 1
}

& ".\build\test\stdlib_dev_codec_crypto_buffer_build.exe"
if ($LASTEXITCODE -ne 0) {
    Write-Host "stdlib codec crypto buffer exe failed: $LASTEXITCODE"
    exit 1
}

$runtimeHeader = Join-Path $root "release\include\xl_runtime.h"
$runtimeHeaderBackup = Join-Path $root "build\test\xl_runtime.h.backup"
$pollutionSource = Join-Path $root "build\test\xl_runtime_pollution_probe.xl"
$pollutionExe = Join-Path $root "build\test\xl_runtime_pollution_probe.exe"
$pollutionOk = $false
[System.IO.File]::Copy($runtimeHeader, $runtimeHeaderBackup, $true)
try {
    [System.IO.File]::WriteAllText(
        $runtimeHeader,
        "#error external xl_runtime.h pollution should not be used`r`n",
        [System.Text.Encoding]::ASCII)
    [System.IO.File]::WriteAllText(
        $pollutionSource,
        "func main(...) as int`r`n    println(`"runtime-private-ok`")`r`n    return 0`r`nend`r`n",
        [System.Text.UTF8Encoding]::new($false))

    & ".\release\xl.exe" run $pollutionSource
    if ($LASTEXITCODE -eq 0) {
        & ".\release\xl.exe" build $pollutionSource -o $pollutionExe
        if ($LASTEXITCODE -eq 0) {
            & $pollutionExe
            if ($LASTEXITCODE -eq 0) {
                $pollutionOk = $true
            }
        }
    }
}
finally {
    # 子进程退出后的短暂文件映射由有限重试吸收。
    $restored = $false
    $restoreError = $null
    for ($attempt = 0; ($attempt -lt 30) -and (-not $restored); $attempt += 1) {
        try {
            [System.IO.File]::Copy($runtimeHeaderBackup, $runtimeHeader, $true)
            $restored = $true
        }
        catch {
            $restoreError = $_
            Start-Sleep -Milliseconds 100
        }
    }
    if (-not $restored) {
        throw $restoreError
    }
    if ((Get-Item -LiteralPath $runtimeHeader).Length -ne (Get-Item -LiteralPath $runtimeHeaderBackup).Length) {
        throw "xl_runtime.h restore verification failed"
    }
}
if (-not $pollutionOk) {
    Write-Host "xl_runtime private VFS include pollution guard failed"
    exit 1
}

exit 0
