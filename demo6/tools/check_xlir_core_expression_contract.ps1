param([string]$Root = "")
$ErrorActionPreference = "Stop"
if ($Root -eq "") { $Root = Split-Path -Parent $PSScriptRoot }
$Root = (Resolve-Path -LiteralPath $Root).Path
$xl = Join-Path $Root "release\xl.exe"
$output = Join-Path $Root "build\tmp"
[System.IO.Directory]::CreateDirectory($output) | Out-Null

# These are the original core-suite programs. Validate semantic IR facts rather
# than requiring local names or typed-container helpers from the retired emitter.
# The suite retains their run/build assertions and strict standalone C execution.
$cases = @("cgen_typed_container_index_assign", "cgen_minimal_runtime",
    "cgen_top_level_before_main", "cgen_string_concat", "cgen_string_compare",
    "cgen_string_literals", "cgen_auto_expressions")

function Require-Operations([hashtable]$Expected) {
    foreach ($entry in $Expected.GetEnumerator()) {
        $actual = @($module.instructions | Where-Object { $_.runtimeOpName -eq $entry.Key })
        if ($actual.Count -ne $entry.Value) {
            throw "$name lost $($entry.Key): $($actual.Count) != $($entry.Value)"
        }
    }
}
function Require-Global([int]$Line, [string]$TypeName, [int]$CAbi) {
    $declaration = @($module.nodes | Where-Object {
        $_.kindName -eq "value-declaration" -and $_.line -eq $Line
    })
    $storage = @($module.storages | Where-Object {
        $_.kindName -eq "global" -and $_.declaration -in $declaration.id
    })
    if ($declaration.Count -ne 1 -or $storage.Count -ne 1 -or
        $storage[0].type -ne $declaration[0].storageType -or
        $types[$storage[0].type].name -ne $TypeName -or
        $types[$storage[0].type].cAbi -ne $CAbi) {
        throw "$name lost its canonical $TypeName storage at line $Line"
    }
}

foreach ($name in $cases) {
    $source = Join-Path $Root "test\cases\$name.xl"
    $prefix = Join-Path $output "xlir_core_expression_$name"
    & $xl ir $source --lowered --format json --emit "$prefix.json"
    if ($LASTEXITCODE -ne 0) { throw "Core expression IR failed: $name" }
    & $xl cgen $source --emit "$prefix.c"
    if ($LASTEXITCODE -ne 0) { throw "Core expression Cgen failed: $name" }
    & $xl cgen $source --backend xlir --emit "$prefix.repeat.c"
    if ($LASTEXITCODE -ne 0) { throw "Core expression repeated Cgen failed: $name" }
    $module = [IO.File]::ReadAllText("$prefix.json") | ConvertFrom-Json
    $c = [IO.File]::ReadAllText("$prefix.c")
    if ($c -cne [IO.File]::ReadAllText("$prefix.repeat.c")) {
        throw "Core expression C is not deterministic: $name"
    }
    if ($c -match '__xl6_user_global_|xl6_temp_managed_|<__xl_runtime/xl_runtime\.h>|\(\{') {
        throw "Core expression depends on legacy storage, private runtime, or statement expressions: $name"
    }
    $types = @{}
    $definitions = @{}
    foreach ($type in $module.types) { $types[$type.id] = $type }
    foreach ($instruction in $module.instructions) {
        if ($instruction.result -ne 0) { $definitions[$instruction.result] = $instruction }
    }
    switch ($name) {
        "cgen_typed_container_index_assign" {
            $expected = @{
                10 = @("xvalue-array-i64-set-new", "xvalue-array-set-i64-new")
                16 = @("xvalue-list-i64-set-new", "xvalue-int-map-set-i64-new")
                23 = @("xvalue-key-dispatch-set-new", "xvalue-index-set-xvalue-key-new")
                25 = @("xvalue-object-literal-set-new", "xvalue-object-set-literal-new")
                38 = @("xvalue-array-i64-set-new", "xvalue-array-set-i64-new")
            }
            $writes = @($module.nodes | Where-Object { $_.indexWriteAbiName -ne "none" })
            if ($writes.Count -ne $expected.Count) { throw "Core index write coverage changed" }
            foreach ($entry in $expected.GetEnumerator()) {
                $lhs = @($writes | Where-Object { $_.line -eq $entry.Key })
                $assign = @($module.nodes | Where-Object {
                    $_.kindName -eq "assign" -and $_.line -eq $entry.Key
                })
                $write = @($module.instructions | Where-Object {
                    $_.origin -in $assign.id -and $_.runtimeOpName -eq $entry.Value[1]
                })
                if ($lhs.Count -ne 1 -or $lhs[0].indexWriteAbiName -ne $entry.Value[0] -or
                    $assign.Count -ne 1 -or $assign[0].indexWriteErrorAbiName -ne "may-raise" -or
                    $write.Count -ne 1 -or $types[$write[0].type].name -ne "bool") {
                    throw "Core indexed mutation lost its exact write/failure ABI at line $($entry.Key)"
                }
                $commit = $write[0]
                $branches = @($module.instructions | Where-Object {
                    $_.kindName -eq "branch" -and $_.operands.Count -eq 1 -and
                    $_.operands[0] -eq $commit.result -and $_.origin -eq $commit.origin -and
                    $_.index -eq ($commit.index + 1) -and $_.trueTarget -ne 0 -and
                    $_.falseTarget -ne 0 -and $_.trueTarget -ne $_.falseTarget
                })
                if ($branches.Count -ne 1 -or @($module.instructions | Where-Object {
                    $_.origin -eq $commit.origin -and $_.kindName -eq "error-raise"
                }).Count -ne 1) { throw "Core indexed mutation has no explicit checked commit edge" }
                $boxed = $definitions[$commit.operands[-1]]
                if ($types[$boxed.type].cAbi -ne 26 -or $types[$boxed.type].ownershipAbi -ne 2) {
                    throw "Core indexed mutation lost its managed boxed item"
                }
                if ($entry.Key -eq 38) {
                    $retain = $definitions[$boxed.operands[0]]
                    if ($boxed.kindName -ne "value" -or $retain.runtimeOpName -ne "xvalue-retain") {
                        throw "Core class array write lost its explicit retain then static erasure"
                    }
                }
            }
            if ($c -match 'exit\([23]\);|xl6_runtime_fatal\(') {
                throw "Core checked writes must raise language errors, not terminate the host"
            }
        }
        "cgen_minimal_runtime" {
            Require-Global 3 "int" 3
            if (@($module.instructions | Where-Object { $_.kindName -eq "runtime-call" }).Count -ne 0 -or
                $c -match 'XL6_Function|__xl_std_|xl6_xrt_array_varargs|xrtValueRelease\(|xrtValueArray\(|xruntime_decl\.h') {
                throw "Minimal scalar module unnecessarily depends on managed runtime components"
            }
        }
        "cgen_top_level_before_main" {
            $init = @($module.functions | Where-Object { $_.kindName -eq "module-init" })
            $entry = @($module.functions | Where-Object { $_.kindName -eq "user" -and $_.reachability -eq "root" })
            # Physical entry calls follow the verified effect. An ordinary
            # integer exit code is not interchangeable with a MAY_RAISE carrier.
            $entryCall = if ($entry[0].errorAbi -eq 'may-raise') {
                'xlir_result_i64 result = xlir_fn_' + $entry[0].symbol +
                    '\(arg\);\s*if \(!result.ok\) return 0;\s*xrtClearError\(\);\s*\*exit_code = \(int\)result.value;'
            } else {
                'int code = \(int\)xlir_fn_' + $entry[0].symbol +
                    '\(arg\);\s*xrtClearError\(\);\s*\*exit_code = code;'
            }
            if ($init.Count -ne 1 -or $entry.Count -ne 1 -or $entry[0].parameterCount -ne 1 -or
                $types[$entry[0].returnType].name -ne "int" -or
                $c -notmatch ('int xl_script_main_checked\(xvalue\* arg, int\* exit_code\)\s*\{\s*\*exit_code = 1;\s*if \(!xlir_module_init\(\)\) return 0;\s*' + $entryCall + '\s*return 1;') -or
                $c -notmatch 'return xl_script_main_checked\(arg, &code\) \? code : 1;') {
                throw "Module entry no longer initializes top-level code before the verified user main"
            }
        }
        "cgen_string_concat" {
            Require-Operations @{ "string-concat" = 4 }
        }
        "cgen_string_compare" {
            Require-Operations @{ "string-equal" = 2; "string-order-lt" = 1; "string-order-gt" = 1; "string-approx" = 2 }
        }
        "cgen_string_literals" {
            Require-Operations @{ "string-dup" = 8 }
        }
        "cgen_auto_expressions" {
            Require-Global 6 "int" 3
            Require-Global 16 "float" 4
            Require-Global 26 "var" 26
            Require-Operations @{ "xvalue-binary-add" = 1; "xvalue-truthy" = 1; "string-approx" = 1 }
        }
    }
    foreach ($operation in @($module.instructions | Where-Object {
        $_.runtimeOpName -in @("string-concat", "string-equal", "string-order-lt", "string-order-gt", "string-approx")
    })) {
        if ($operation.operands.Count -ne 2 -or @($operation.operands | Where-Object {
            $types[$definitions[$_].type].cAbi -ne 25
        }).Count -ne 0) { throw "String operator lost its exact managed-string operands: $name" }
        $resultAbi = if ($operation.runtimeOpName -eq "string-concat") { 25 } else { 2 }
        if ($types[$operation.type].cAbi -ne $resultAbi) { throw "String operator result ABI changed: $name" }
    }
    if ($name -in @("cgen_string_concat", "cgen_string_literals")) {
        $globals = @($module.storages | Where-Object { $_.kindName -eq "global" })
        if ($globals.Count -ne 4 -or @($globals | Where-Object {
            $types[$_.type].cAbi -ne 25 -or $types[$_.type].ownershipAbi -ne 2
        }).Count -ne 0) { throw "String source forms lost their managed storage ABI: $name" }
    }
    Write-Output "XLIR core expression contract ok: $name"
}
