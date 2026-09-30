param([string]$Root = "", [string]$RuntimeDirectory = "", [string]$WorkDirectory = "")

$ErrorActionPreference = "Stop"
if ($Root -eq "") { $Root = Split-Path -Parent $PSScriptRoot }
if (!$RuntimeDirectory) { $RuntimeDirectory = Join-Path $Root 'release' }
$xl = Join-Path $RuntimeDirectory "xl.exe"
$source = "test\cases\xlir_string_bridge_abi.xl"
$work = if ($WorkDirectory) { $WorkDirectory } else { "build\test\xlir_string_bridges" }
$jsonPath = "$work\module.json"
$cPath = "$work\module.c"
$runtime = "$work\runtime.o"
$runtimeHost = "test\cases\xlir_string_bridge_abi_host.c"

Push-Location $Root
try {
    [IO.Directory]::CreateDirectory($work) | Out-Null
    [IO.Directory]::CreateDirectory("$work\__xl_runtime") | Out-Null
    Copy-Item release\include\xruntime_decl.h "$work\__xl_runtime\xruntime_decl.h" -Force
    foreach ($entry in Get-Content tools/tcc_vfs_manifest_win64_xlc.txt) {
        if (!$entry.StartsWith('file|include/__xl_runtime/')) { continue }
        $parts = $entry.Split('|')
        Copy-Item -LiteralPath ($parts[2].Replace('%XL6_ROOT%', $Root)) -Destination (Join-Path "$work\__xl_runtime" ([IO.Path]::GetFileName($parts[1]))) -Force
    }
    & $xl ir $source --lowered --format json --emit $jsonPath
    if ($LASTEXITCODE -ne 0) { throw "String bridge lowering failed" }
    $module = Get-Content $jsonPath -Raw | ConvertFrom-Json
    $names = @("byte_len", "byte_at", "slice", "replace", "starts_with", "ends_with", "split", "split_lines", "join", "bytes", "chars")
    foreach ($name in $names) {
        $targets = @($module.callTargets | Where-Object { $_.cName -eq "xl6_xrt_string_$name" })
        if ($targets.Count -eq 0) { throw "Missing string target: $name" }
        foreach ($target in $targets) {
            if ($target.runtimeBridgeName -ne "string-$($name.Replace('_', '-'))" -or
                $target.resultConversionAbiName -ne "direct") {
                throw "String bridge lost its explicit ABI: $name"
            }
        }
    }
    foreach ($negative in @("xlir_string_bridge_bad_result", "xlir_string_bridge_bad_parameter",
            "xlir_string_join_bad_result", "xlir_string_join_bad_parameter",
            "xlir_string_bytes_bad_result", "xlir_string_bytes_bad_parameter",
            "xlir_string_chars_bad_result", "xlir_string_chars_bad_parameter")) {
        # Windows PowerShell 将重定向的 stderr 包装为 ErrorRecord；负例必须收集而非提前终止。
        try {
            $ErrorActionPreference = "Continue"
            $output = @(& $xl ir "test\cases\$negative.xl" --lowered 2>&1)
            $exitCode = $LASTEXITCODE
        } finally { $ErrorActionPreference = "Stop" }
        if ($exitCode -eq 0 -or ($output -join "`n") -notmatch "string runtime bridge has no exact parameter/result ABI") {
            throw "Malformed string bridge was not rejected at its boundary: $negative"
        }
    }

    & $xl cgen $source --backend xlir --emit $cPath
    if ($LASTEXITCODE -ne 0) { throw "String bridge Cgen failed" }
    $generated = Get-Content $cPath -Raw
    if ($generated.Contains("typedef struct xlir_string_list")) { throw "String list ABI is redeclared" }
    & $xl cgen test/cases/xlir_string_bridge_minimal.xl --backend xlir --emit "$work\minimal.c"
    if ($LASTEXITCODE -ne 0) { throw "Minimal string bridge Cgen failed" }
    $minimal = Get-Content "$work\minimal.c" -Raw
    if (!$minimal.Contains("size_t xl6_xrt_string_byte_len(") -or
        $minimal.Contains("xl6_xrt_string_split(") -or
        $minimal.Contains("xl6_xrt_string_replace(") -or
        $minimal.Contains("xl6_xrt_string_join(") -or
        $minimal.Contains("xl6_xrt_string_bytes(") -or
        $minimal.Contains("xl6_xrt_string_chars(") -or
        $minimal.Contains("xl6_xrt_string_slice(")) {
        throw "String bridge demand calculation emitted unrelated adapters"
    }

    # 独立运行库对象，避免其他测试重建共享 xrt_cgen.o 时读到半成品。
    & gcc -std=c11 -I. -c tools/xrt_single_header_impl.c -o $runtime
    if ($LASTEXITCODE -ne 0) { throw "String bridge private runtime build failed" }
    # 闭包和所有权操作来自语言运行库，不能只链接 XRT 后依赖偶然的旧生成代码。
    $languageSources = @(Get-ChildItem src/runtime -File -Filter '*.c' | Sort-Object Name | ForEach-Object FullName)
    & gcc -std=c11 -Wall -Wextra -Werror -Wno-unused-function -Wno-unused-label -Wno-unused-variable -Wno-unused-parameter -Wno-unused-const-variable -Wno-missing-field-initializers "-I$work" -Irelease/include -Ilib/xrt -Ilib/xruntime -I. -include $cPath -c $runtimeHost -o "$work\host.o"
    if ($LASTEXITCODE -ne 0) { throw "String bridge host compile failed" }
    & gcc -std=c11 -I. @languageSources "$work\host.o" $runtime -lWs2_32 -lIPHLPAPI -lShell32 -o "$work\strict.exe"
    if ($LASTEXITCODE -ne 0) { throw "String bridge strict C build failed" }
    $output = @(& (Join-Path $Root "$work\strict.exe"))
    Write-Output $output
    if ($LASTEXITCODE -ne 0 -or ($output -join "`n") -notmatch "string bridge allocation failures ok" -or
        ($output -join "`n") -notmatch "string join allocation failures ok" -or
        ($output -join "`n") -notmatch "string bridges ok") { throw "String bridge C ownership check failed" }

    $output = @(& $xl run $source)
    if ($LASTEXITCODE -ne 0 -or ($output -join "`n") -notmatch "string bridges ok") { throw "String bridge JIT failed" }
    & $xl build $source -o "$work\aot.exe"
    if ($LASTEXITCODE -ne 0) { throw "String bridge AOT build failed" }
    $output = @(& (Join-Path $Root "$work\aot.exe"))
    if ($LASTEXITCODE -ne 0 -or ($output -join "`n") -notmatch "string bridges ok") { throw "String bridge AOT run failed" }

    & $xl run test/cases/xlir_string_join_failure.xl
    if ($LASTEXITCODE -ne 0) { throw "String allocation failures escaped the language error contract" }

    $xlOutput = @(& $xl run release/examples/stdlib_string_boundaries.xl)
    if ($LASTEXITCODE -ne 0) { throw "String boundary xlang example failed" }
    $cOutput = @(& $xl run release/examples/stdlib_string_boundaries_c.c)
    if ($LASTEXITCODE -ne 0) { throw "String boundary C example failed" }
    if (($xlOutput -join "`n") -cne ($cOutput -join "`n") -or $xlOutput.Count -ne 21) {
        throw "String boundary C/xlang examples disagree"
    }
} finally {
    Pop-Location
}
Write-Output "XLIR string bridge ABI, allocation failure, JIT/AOT and bilingual examples ok."
