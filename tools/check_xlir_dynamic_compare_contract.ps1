param([string]$Root = "")

$ErrorActionPreference = "Stop"
if ($Root -eq "") { $Root = Split-Path -Parent $PSScriptRoot }
$xl = Join-Path $Root "release\xl.exe"
$output = Join-Path $Root "build\test"
[System.IO.Directory]::CreateDirectory($output) | Out-Null

function Invoke-Xl([string[]]$Arguments) {
    & $xl @Arguments
    if ($LASTEXITCODE -ne 0) { throw "Dynamic comparison command failed: $Arguments" }
}

Push-Location $Root
try {
    foreach ($source in @("test\cases\xlir_dynamic_compare_contract.xl", "test\cases\cgen_var_values.xl")) {
        $case = [System.IO.Path]::GetFileNameWithoutExtension($source)
        $generated = "build\test\${case}_generated.c"
        $again = "build\test\${case}_compare_again.c"
        $json = "build\test\${case}_compare.lowered.json"
        Invoke-Xl @("ir", $source, "--lowered", "--format", "json", "--emit", $json)
        $ir = Get-Content -Raw $json | ConvertFrom-Json
        $operations = @($ir.instructions | Where-Object {
            $_.runtimeOpName -match '^xvalue-(equal|order-(lt|le|gt|ge)|approx)$'
        })
        if ($operations.Count -lt 6) { throw "Missing physical dynamic comparisons: $case" }
        $types = @{}; $definitions = @{}
        foreach ($type in $ir.types) { $types[[string]$type.id] = $type }
        foreach ($instruction in $ir.instructions) {
            if ($instruction.result -ne 0) { $definitions[[string]$instruction.result] = $instruction }
        }
        foreach ($operation in $operations) {
            $result = $types[[string]$operation.type]
            if ($operation.kindName -ne "runtime-call" -or $operation.operandCount -ne 2 -or
                $result.cAbi -ne 2 -or $result.ownershipAbi -ne 1) {
                throw "Dynamic comparison lost its exact borrowed operands / boolean result ABI"
            }
            foreach ($value in $operation.operands) {
                $type = $types[[string]$definitions[[string]$value].type]
                if ($type.cAbi -ne 26 -or $type.ownershipAbi -ne 2 -or $type.nullAbi -ne 2) {
                    throw "Dynamic comparison operand is not a canonical managed xvalue: $($type | ConvertTo-Json -Compress)"
                }
            }
        }
        if ($case -eq "xlir_dynamic_compare_contract") {
            foreach ($name in @("xvalue-equal", "xvalue-order-lt", "xvalue-order-le",
                "xvalue-order-gt", "xvalue-order-ge", "xvalue-approx")) {
                if (@($operations | Where-Object { $_.runtimeOpName -eq $name }).Count -ne 3) {
                    throw "Sync, async and generator must each own their physical comparison: $name"
                }
            }
        }
        Invoke-Xl @("cgen", $source, "--emit", $generated)
        Invoke-Xl @("cgen", $source, "--backend", "xlir", "--emit", $again)
        if (![System.Linq.Enumerable]::SequenceEqual(
            [byte[]][System.IO.File]::ReadAllBytes((Join-Path $Root $generated)),
            [byte[]][System.IO.File]::ReadAllBytes((Join-Path $Root $again)))) {
            throw "Dynamic comparison default / explicit XLIR C is not deterministic"
        }
        $c = Get-Content -Raw $generated
        foreach ($required in @("#include <__xl_runtime/xruntime_decl.h>", "XVALUE_TIME",
            "XVALUE_UINT", "a != a || b != b", "uint64_t a = xlir_xvalue_u64(left)")) {
            if (!$c.Contains($required)) { throw "Dynamic comparison lost public ABI / exact numeric dispatch: $required" }
        }
        if ($c -match '#include[^\r\n]*[/\\]xl_runtime\.h|\b(?:leftType|rightType) == 5\b') {
            throw "Private runtime or obsolete numeric type tags returned to dynamic comparison"
        }
        $run = @("run", $source)
        $build = @("build", $source)
        if ($case -eq "cgen_var_values") {
            $run += "test\cases\cgen_class_host.c"
            $build += "test\cases\cgen_class_host.c"
        }
        Invoke-Xl $run
        $exe = "build\test\${case}_compare_cli.exe"
        Invoke-Xl ($build + @("-o", $exe))
        & ".\$exe"
        if ($LASTEXITCODE -ne 0) { throw "Dynamic comparison built executable failed: $case" }
    }

    # Unit tests exercise malformed physical IR, not merely textual snapshots.
    $unit = '!XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -o build\test\xlir_cgen.exe test\cases\xlir_cgen.c src\ast\xl6_ast.c src\type\xl6_type_model.c src\ir\xl6_ir.c src\ir\xl6_ir_lower.c src\cgen\xl6_ir_cgen.c src\cgen\xl6_cgen_shared_runtime.c src\diag\xl6_diag.c src\common\xl6_output.c src\common\xl6_console.c !XL6_CGEN_LINKS! && build\test\xlir_cgen.exe'
    $compareHostCommand = '!XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror test\cases\xlir_dynamic_compare_contract_host.c !XL6_CGEN_LINKS! -o build\test\xlir_dynamic_compare_contract_host.exe && build\test\xlir_dynamic_compare_contract_host.exe'
    $original = '!XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include build\test\cgen_var_values_generated.c -DXLIR_GENERATED_MODULE_PREINCLUDED test\cases\xlir_generated_module_host.c -x none !XL6_CGEN_LINKS! -o build\test\cgen_var_values_compare_host.exe'
    # Compile the C import host separately; it must not include the XLIR module.
    $original = '!XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -c test\cases\cgen_class_host.c -o build\test\dynamic_compare_class_host.o && ' + $original.Replace('-x none', '-x none build\test\dynamic_compare_class_host.o') + ' && build\test\cgen_var_values_compare_host.exe'
    & $env:ComSpec /d /v:on /c "call test\suites\common.bat && $unit && $compareHostCommand && $original"
    if ($LASTEXITCODE -ne 0) { throw "Dynamic comparison malformed-IR / strict-C runtime gate failed" }
} finally {
    Pop-Location
}
Write-Output "XLIR dynamic comparison contract passed: one verified ABI, three execution modes, exact integer ordering, time/bytes tags, NaN/null semantics, strict C, CLI run/build and original cgen_var_values assertions."
