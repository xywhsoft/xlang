param([string]$Root = "")

$ErrorActionPreference = "Stop"
if ($Root -eq "") { $Root = Split-Path -Parent $PSScriptRoot }
$xl = Join-Path $Root "release\xl.exe"
$source = "test\cases\xlir_foreign_string_abi.xl"
$hostSource = "test\cases\xlir_foreign_string_abi_host.c"
$jsonPath = "build\test\xlir_foreign_string_abi.json"
$cPath = "build\test\xlir_foreign_string_abi.c"
$exePath = "build\test\xlir_foreign_string_abi.exe"

Push-Location $Root
try {
    [System.IO.Directory]::CreateDirectory("build\test") | Out-Null
    & $xl ir $source --lowered --format json --emit $jsonPath
    if ($LASTEXITCODE -ne 0) { throw "Foreign string lowering failed" }
    $module = Get-Content -LiteralPath $jsonPath -Raw | ConvertFrom-Json
    $names = @("xext_text_static", "xext_text_null",
        "xext_text_alias", "xext_text_join", "xext_text_get", "xext_text_read")
    foreach ($name in $names) {
        $targets = @($module.callTargets | Where-Object { $_.cName -eq $name })
        if ($targets.Count -eq 0) { throw "Missing foreign string target: $name" }
        foreach ($target in $targets) {
            if ($target.resultConversionAbiName -ne "borrowed-string-copy") {
                throw "Foreign string target lacks its owned-copy contract: $name"
            }
        }
    }
    foreach ($target in @($module.callTargets | Where-Object {
        $_.runtimeBridgeName -ne "none"
    })) {
        if ($target.resultConversionAbiName -ne "direct") {
            throw "Runtime bridge acquired a redundant foreign copy: $($target.cName)"
        }
    }

    & $xl cgen $source --backend xlir --emit $cPath
    if ($LASTEXITCODE -ne 0) { throw "Foreign string C generation failed" }
    $generated = Get-Content -LiteralPath $cPath -Raw
    if ($generated -notmatch 'extern const char\* xext_text_static\(') {
        throw "Foreign string prototype lost its const C return type"
    }
    # 单一翻译单元检查真实 C 声明是否一致，并以严格警告执行拥有权测试。
    $command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED -include $cPath $hostSource !XL6_CGEN_LINKS! -o $exePath && $exePath"
    # 检查实际运行输出，避免仅凭启动器退出码误报通过。
    $output = @(& $env:ComSpec /d /v:on /c $command)
    Write-Output $output
    if ($LASTEXITCODE -ne 0 -or
        ($output -join "`n") -notmatch "foreign string ownership ok" -or
        !(Test-Path -LiteralPath $exePath -PathType Leaf)) {
        throw "Foreign string strict-C/runtime check failed"
    }

    $output = @(& $xl run $source $hostSource)
    if ($LASTEXITCODE -ne 0 -or ($output -join "`n") -notmatch "foreign string ownership ok") {
        throw "Foreign string in-memory run failed"
    }
    $aotPath = "build\test\xlir_foreign_string_abi_aot.exe"
    & $xl build $source $hostSource -o $aotPath
    if ($LASTEXITCODE -ne 0) { throw "Foreign string AOT build failed" }
    $output = @(& (Join-Path $Root $aotPath))
    if ($LASTEXITCODE -ne 0 -or ($output -join "`n") -notmatch "foreign string ownership ok") {
        throw "Foreign string AOT runtime check failed"
    }
} finally {
    Pop-Location
}
Write-Output "XLIR foreign string copy, nullable, alias, callback and native receiver lifetime ok."
