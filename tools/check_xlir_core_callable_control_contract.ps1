param([string]$Root = "")
$ErrorActionPreference = "Stop"
if ($Root -eq "") { $Root = Split-Path -Parent $PSScriptRoot }
$xl = Join-Path $Root "release\xl.exe"
$cases = @("cgen_function_default", "cgen_function_null_compare",
    "cgen_optional_nullable_function_default", "cgen_assert_dynamic_condition",
    "cgen_var_comparison_expressions", "cgen_range_step_runtime_zero",
    "xlir_named_callable_owner_lifetime")
foreach ($name in $cases) {
    $source = Join-Path $Root "test\cases\$name.xl"
    $prefix = Join-Path $Root "build\test\${name}_core8"
    & $xl ir $source --lowered --format json --emit "$prefix.json"
    if ($LASTEXITCODE -ne 0) { throw "Callable/control lowering failed: $name" }
    & $xl cgen $source --emit "$prefix.c"
    if ($LASTEXITCODE -ne 0) { throw "Callable/control Cgen failed: $name" }
    & $xl cgen $source --backend xlir --emit "$prefix.repeat.c"
    if ($LASTEXITCODE -ne 0) { throw "Callable/control repeated Cgen failed: $name" }
    $c = [IO.File]::ReadAllText("$prefix.c")
    if ($c -cne [IO.File]::ReadAllText("$prefix.repeat.c") -or
        $c -match '__xl6_user_global_|<__xl_runtime/xl_runtime\.h>|\(\{') {
        throw "Callable/control is not deterministic mechanical XLIR C: $name"
    }
    $module = [IO.File]::ReadAllText("$prefix.json") | ConvertFrom-Json
    $types = @{}
    foreach ($type in $module.types) { $types[$type.id] = $type }
    foreach ($node in @($module.nodes | Where-Object { $_.namedCallableAbi -ne 0 })) {
        if ($node.storageType -eq 0 -or $node.type -eq 0 -or
            $node.valueCategoryName -ne "rvalue" -or $node.ownershipName -ne "move" -or
            $node.ownershipPlanned -ne 1) { throw "Named callable is not a typed owning value" }
    }
    switch ($name) {
        "cgen_function_default" {
            if (@($module.storages).Count -ne 1 -or $module.storages[0].kindName -ne "global" -or
                $types[$module.storages[0].type].cAbi -ne 31 -or
                $types[$module.storages[0].type].nullAbi -ne 1) {
                throw "Default function storage lost its empty pointer sentinel ABI"
            }
        }
        "cgen_function_null_compare" {
            if (@($module.instructions | Where-Object { $_.kindName -eq "null-test" }).Count -ne 9 -or
                @($module.instructions | Where-Object { $_.runtimeOpName -eq "xvalue-object-get-literal-closure" }).Count -ne 6) {
                throw "Borrowed/owned/conditional function null comparison coverage changed"
            }
            foreach ($node in @($module.nodes | Where-Object {
                $_.memberAbiName -eq "class-field-closure" -and
                $module.nodes[$_.parent - 1].kindName -ne "assign"
            })) {
                if ($node.valueCategoryName -ne "rvalue" -or $node.ownershipName -ne "move") {
                    throw "Closure field read hides its materialized owner"
                }
            }
            if (!$c.Contains('if (xrtValueType(borrowed) == 0) return NULL;')) {
                throw "Empty function field read lost its physical sentinel"
            }
        }
        "cgen_assert_dynamic_condition" {
            foreach ($op in @("xvalue-equal", "xvalue-approx", "xvalue-truthy")) {
                if (@($module.instructions | Where-Object { $_.runtimeOpName -eq $op }).Count -eq 0) {
                    throw "Dynamic assertion lacks explicit boolean conversion: $op"
                }
            }
        }
        "cgen_var_comparison_expressions" {
            foreach ($op in @("xvalue-equal", "xvalue-order-lt", "xvalue-approx", "xvalue-truthy")) {
                if (@($module.instructions | Where-Object { $_.runtimeOpName -eq $op }).Count -eq 0) {
                    throw "Dynamic expression lost its explicit runtime operation: $op"
                }
            }
        }
        "cgen_range_step_runtime_zero" {
            if (@($module.nodes | Where-Object {
                $_.loopAbiName -eq "range-i64-value" -and $_.loopErrorAbiName -eq "may-raise"
            }).Count -ne 1 -or @($module.instructions | Where-Object {
                $_.kindName -eq "error-raise"
            }).Count -eq 0 -or @($module.instructions | Where-Object {
                $_.kindName -eq "throw"
            }).Count -eq 0) { throw "Zero-step range lost its explicit failure CFG" }
        }
        "xlir_named_callable_owner_lifetime" {
            if (@($module.nodes | Where-Object { $_.namedCallableAbi -ne 0 }).Count -lt 9 -or
                @($module.instructions | Where-Object { $_.kindName -eq "closure-make" }).Count -lt 8) {
                throw "Named callable lifetime acquisition coverage was removed"
            }
            $boxes = @($module.instructions | Where-Object { $_.runtimeOpName -eq "xvalue-callable-from-closure" })
            if ($boxes.Count -ne 1) { throw "Named callable erasure ownership coverage changed" }
            $box = $boxes[0]
            $anchor = @($module.instructions | Where-Object { $_.result -eq $box.operands[0] })
            if ($anchor.Count -ne 1 -or $anchor[0].kindName -ne "load" -or
                $module.instructions[$box.index + 1].kindName -ne "error-test" -or
                @($module.instructions | Where-Object {
                    $_.kindName -eq "release" -and $_.targetSymbol -eq $anchor[0].targetSymbol -and
                    $_.index -gt $box.index
                }).Count -lt 2) {
                throw "Erasure borrow lacks both success and failure owner cleanup"
            }
        }
    }
}
Write-Output "XLIR callable/control contract passed: named owners, empty closure storage, owned field reads, nullable calls, dynamic predicates and zero-step failure CFG."
