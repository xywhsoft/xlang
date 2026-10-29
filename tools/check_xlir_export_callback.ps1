param([string]$Root = "")
$ErrorActionPreference = "Stop"
if ($Root -eq "") { $Root = Split-Path -Parent $PSScriptRoot }
$xl = Join-Path $Root 'release\xl.exe'
$fixtures = @(
    [pscustomobject]@{
        Name = 'cgen_c_callback_export'
        File = 'cgen_c_callback_export.xl'
        Prefix = 'xlir_export_callback_public'
    },
    [pscustomobject]@{
        Name = 'xlir_export_callback_lifetime'
        File = 'xlir_export_callback_lifetime.xl'
        Prefix = 'xlir_export_callback_lifetime'
    }
)
Push-Location $Root
try {
    $negativeCommand = '"' + $xl + '" check test\cases\sem_c_callback_nullable_string_return_invalid.xl > build\tmp\xlir_export_callback_nullable_negative.log 2>&1'
    & $env:ComSpec /d /c $negativeCommand
    if ($LASTEXITCODE -ne 1 -or ![IO.File]::ReadAllText('build\tmp\xlir_export_callback_nullable_negative.log').Contains('ownership protocol')) {
        throw 'Nullable callbacks must still reject managed returns without an ownership protocol'
    }
    foreach ($fixture in $fixtures) {
        $name = $fixture.Name
        $prefix = 'build\test\' + $fixture.Prefix
        & $xl ir ("test\cases\" + $fixture.File) --lowered --format json --emit "$prefix.json"
        if ($LASTEXITCODE -ne 0) { throw "Callback export lowering failed: $name" }
        $ir = [IO.File]::ReadAllText("$prefix.json") | ConvertFrom-Json
        $count = 0
        foreach ($export in $ir.exports) {
            for ($p = 0; $p -lt @($export.parameterTypes).Count; ++$p) {
                $type = @($ir.types | Where-Object { $_.id -eq $export.parameterTypes[$p] })[0]
                if ($type.cAbi -eq 31) {
                    if ($export.parameterValueAbis[$p] -notin @('raw-c-callback-borrow', 'raw-c-callback-owned-result-borrow') -or
                        $export.parameterAbis[$p] -ne 'borrow') {
                        throw 'Public callback must have an exact borrowed raw C bridge ABI'
                    }
                    ++$count
                }
            }
        }
        if ($count -lt 4) { throw "Callback fixture lost export bridge coverage: $name" }
        if ($name -eq 'xlir_export_callback_lifetime' -and @($ir.exports | Where-Object {
            @($_.parameterValueAbis) -contains 'raw-c-callback-owned-result-borrow'
        }).Count -ne 2) { throw 'Managed callback results must carry their owned-result boundary ABI' }
        & $xl cgen ("test\cases\" + $fixture.File) --emit "$prefix.c" --emit-header "$prefix.h"
        if ($LASTEXITCODE -ne 0) { throw "Callback export C/header emission failed: $name" }
        & $xl cgen ("test\cases\" + $fixture.File) --backend xlir --emit "$prefix.repeat.c"
        if ($LASTEXITCODE -ne 0) { throw "Callback export repeat emission failed: $name" }
        $c = [IO.File]::ReadAllText("$prefix.c")
        if ($c -cne [IO.File]::ReadAllText("$prefix.repeat.c")) { throw 'Callback C is not deterministic' }
        $header = [IO.File]::ReadAllText("$prefix.h")
        if ($header -match 'xlir_(?:closure|generator|cell|xvalue_iterator)|XL6_Function' -or
            $header -match 'typedef union xlir_union_' -or
            $header -notmatch 'xlir_raw_callback_t') {
            throw 'Public callback header leaked an internal closure carrier'
        }
        $publicProductCount = ([regex]::Matches(
            $header, 'typedef struct xlir_product_t[0-9]+')).Count
        $expectedProductCount = if ($name -eq 'xlir_export_callback_lifetime') { 1 } else { 0 }
        if ($publicProductCount -ne $expectedProductCount) {
            throw "Public callback header structural reachability mismatch: expected $expectedProductCount product(s), found $publicProductCount"
        }
        foreach ($required in @('xlir_export_callback_wrap_t', 'xlir_export_bridge_failed:',
            'C callback export bridge allocation failed', 'xlir_export_arg_0 = NULL;', 'xlir_closure_release(owned)')) {
            if (!$c.Contains($required)) { throw "Callback boundary lost its mechanical bridge: $required" }
        }
    }
    $command = 'call test\suites\common.bat' +
        ' && "!XL6_CGEN_CC!" !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -x c -fsyntax-only build\test\xlir_export_callback_public.h' +
        ' && "!XL6_CGEN_CC!" !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror test\cases\xlir_export_callback_public_module.c test\cases\xlir_export_callback_public_host.c !XL6_CGEN_LINKS! -o build\test\xlir_export_callback_public.exe' +
        ' && build\test\xlir_export_callback_public.exe' +
        ' && "!XL6_CGEN_CC!" !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror test\cases\xlir_export_callback_lifetime_host.c !XL6_CGEN_LINKS! -o build\test\xlir_export_callback_lifetime.exe' +
        ' && build\test\xlir_export_callback_lifetime.exe' +
        ' && "!XL6_CGEN_CC!" !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -o build\test\xlir_export_callback_contract.exe test\cases\xlir_export_callback_contract.c src\ast\xl6_ast.c src\type\xl6_type_model.c src\ir\xl6_ir.c src\ir\xl6_ir_lower.c src\cgen\xl6_ir_cgen.c src\cgen\xl6_cgen_shared_runtime.c src\diag\xl6_diag.c src\common\xl6_output.c src\common\xl6_console.c !XL6_CGEN_LINKS!' +
        ' && build\test\xlir_export_callback_contract.exe' +
        ' && "!XL6_CGEN_CC!" !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -fsyntax-only build\test\xlir_export_callback_contract_generated.c'
    & $env:ComSpec /d /v:on /c $command
    if ($LASTEXITCODE -ne 0) { throw 'Callback export strict C/public linkage/lifetime gate failed' }
    # These language exports expose borrowed var / owned nominal callback
    # values. Their public headers must remain strict C independently of
    # implementation/runtime coverage for the imported standard library.
    foreach ($name in @('extlib_xml_sax_min', 'extlib_xbot_agent')) {
        $generated = "build\test\$name.callback-export.c"
        $publicHeader = "build\test\$name.callback-export.h"
        & $xl cgen "test\cases\$name.xl" --backend xlir --emit $generated --emit-header $publicHeader
        if ($LASTEXITCODE -ne 0) { throw "Managed callback library export regressed: $name" }
        & gcc -std=c11 -Wall -Wextra -Werror -x c -Ibuild/test -fsyntax-only $publicHeader
        if ($LASTEXITCODE -ne 0) { throw "Managed callback library public header is not strict C: $name" }
    }
} finally { Pop-Location }
Write-Output 'XLIR exported callback ABI passed: canonical nullable signatures, standalone public header, separate C linkage, borrowed/owned managed values, persistent/null callbacks, original errors, partial acquisition balance and seven verifier/Cgen mutations.'
exit 0
