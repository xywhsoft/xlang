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
        [string[]]$ArgsList
    )

    & ".\release\xl.exe" @ArgsList
    $code = $LASTEXITCODE
    if ($code -eq 0) {
        Write-Host "$Label should fail but exited 0"
        exit 1
    }
}

Invoke-XlExpectFailure `
    -Label "dynamic array out-of-range set" `
    -ArgsList @("run", "test\cases\cgen_issue_dynamic_array_oob_set.xl")

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

$milk = -join ([char[]](0x4E70, 0x725B, 0x5976))
$utf8SourcePath = Join-Path $root "build\test\cgen_issue_utf8_argv.generated.xl"
$utf8Source = @(
    "func main(...) as int",
    "    if arg.length < 2 then",
    "        return 1",
    "    end",
    "    if string(arg[1]) != `"$milk`" then",
    "        return 2",
    "    end",
    "    return 0",
    "end"
) -join "`r`n"
[System.IO.File]::WriteAllText($utf8SourcePath, $utf8Source, [System.Text.UTF8Encoding]::new($false))

Invoke-XlExpectExit `
    -Label "utf8 argv run" `
    -ArgsList @("run", $utf8SourcePath, "--", $milk) `
    -Expected 0

& ".\release\xl.exe" build $utf8SourcePath -o "build\test\cgen_issue_utf8_argv.exe"
if ($LASTEXITCODE -ne 0) {
    Write-Host "utf8 argv build failed: $LASTEXITCODE"
    exit 1
}

& ".\build\test\cgen_issue_utf8_argv.exe" $milk
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

exit 0
