param([string]$Root = "")
$ErrorActionPreference = "Stop"
if ($Root -eq "") { $Root = Split-Path -Parent $PSScriptRoot }
$xl = Join-Path $Root "release\xl.exe"
$cases = @("cgen_auto_inference_matrix", "cgen_call_var_arg_exprs",
    "perf_var_recursive_alloc", "cgen_call_scalar_arg_exprs", "cgen_var_return_exprs",
    "cgen_expr_statement_discard", "cgen_array_literal_expr_elements", "cgen_var_values")
function Require-Operations([hashtable]$Expected) {
    foreach ($entry in $Expected.GetEnumerator()) {
        $actual = @($module.instructions | Where-Object { $_.runtimeOpName -eq $entry.Key })
        if ($actual.Count -ne $entry.Value) {
            throw "$name lost $($entry.Key): $($actual.Count) != $($entry.Value)"
        }
    }
}
foreach ($name in $cases) {
    $source = Join-Path $Root "test\cases\$name.xl"
    $prefix = Join-Path $Root "build\test\${name}_core_value"
    & $xl ir $source --lowered --format json --emit "$prefix.json"
    if ($LASTEXITCODE -ne 0) { throw "Core call/value IR failed: $name" }
    & $xl cgen $source --emit "$prefix.c"
    if ($LASTEXITCODE -ne 0) { throw "Core call/value Cgen failed: $name" }
    & $xl cgen $source --backend xlir --emit "$prefix.repeat.c"
    if ($LASTEXITCODE -ne 0) { throw "Core call/value repeated Cgen failed: $name" }
    $c = [IO.File]::ReadAllText("$prefix.c")
    if ($c -cne [IO.File]::ReadAllText("$prefix.repeat.c") -or
        $c -match '__xl6_user_global_|xl6_temp_managed_|<__xl_runtime/xl_runtime\.h>|\(\{') {
        throw "Core call/value depends on nondeterminism or legacy/private representation: $name"
    }
    $module = [IO.File]::ReadAllText("$prefix.json") | ConvertFrom-Json
    $types = @{}; $definitions = @{}
    foreach ($type in $module.types) { $types[$type.id] = $type }
    foreach ($instruction in $module.instructions) {
        if ($instruction.result -ne 0) { $definitions[$instruction.result] = $instruction }
    }
    switch ($name) {
        "cgen_auto_inference_matrix" {
            $expected = @{
                34=@("int",3,1);35=@("float",4,1);36=@("bool",2,1);37=@("string",25,2)
                39=@("array<int>",26,2);40=@("list<string>",26,2);41=@("dict<int>",26,2)
                43=@("array<int>",26,2);44=@("list<string>",26,2);45=@("dict<int>",26,2)
                47=@("int",3,1);48=@("string",25,2);49=@("int",3,1);50=@("nullable<int>",36,1)
                52=@("int",3,1);53=@("string",25,2);54=@("int",3,1)
                56=@("MatrixBox",26,2);57=@("int",3,1);58=@("string",25,2);59=@("int",3,1)
                61=@("function",31,2);62=@("int",3,1);63=@("int",3,1)
                65=@("future<int>",30,2);66=@("int",3,1)
                68=@("channel<int>",26,2);69=@("bool",2,1)
            }
            # Multi-name declarations are verified product fields, not two
            # unrelated source value-declaration nodes at magic line numbers.
            $receive = @($module.instructions | Where-Object { $_.runtimeOpName -eq 'channel-owned-recv' })
            if ($receive.Count -ne 1 -or $receive[0].operandCount -ne 1) {
                throw 'Auto Channel receive lost its canonical owning product'
            }
            $productType = $types[$receive[0].type]
            if ($productType.params.Count -ne 2 -or
                $types[$productType.params[0]].displayName -ne 'int' -or
                $types[$productType.params[1]].displayName -ne 'nullable<int>') {
                throw 'Auto Channel receive must infer (int, nullable<int>)'
            }
            foreach ($entry in $expected.GetEnumerator()) {
                $decl = @($module.nodes | Where-Object {
                    $_.kindName -eq "value-declaration" -and $_.line -eq $entry.Key
                })
                $slot = @($module.storages | Where-Object {
                    $_.kindName -eq "global" -and $_.declaration -in $decl.id
                })
                if ($decl.Count -ne 1 -or $slot.Count -ne 1 -or
                    $slot[0].type -ne $decl[0].storageType -or
                    $types[$slot[0].type].displayName -ne $entry.Value[0] -or
                    $types[$slot[0].type].cAbi -ne $entry.Value[1] -or
                    $types[$slot[0].type].ownershipAbi -ne $entry.Value[2]) {
                    throw "Auto inference lost canonical physical type at source line $($entry.Key)"
                }
            }
        }
        "cgen_call_var_arg_exprs" {
            $sites = @($module.dynamicCallSites)
            $keyword = @($module.dynamicCallArguments | Where-Object { $_.name -ne "" -and $null -ne $_.name })
            $calls = @($module.instructions | Where-Object { $_.runtimeOpName -eq "xvalue-dynamic-call" })
            if ($sites.Count -ne 2 -or $calls.Count -ne 2 -or
                @($sites | Where-Object { $_.positionalCount -eq 2 -and $_.keywordCount -eq 0 }).Count -ne 1 -or
                @($sites | Where-Object { $_.positionalCount -eq 0 -and $_.keywordCount -eq 2 }).Count -ne 1 -or
                ($keyword.name -join ",") -ne "right,left") {
                throw "Dynamic call lost source-order positional / keyword frame metadata"
            }
            foreach ($call in $calls) {
                $test = $module.instructions[$call.index + 1]
                if ($call.operandCount -ne 3 -or $types[$call.type].name -ne "var" -or
                    $test.kindName -ne "error-test" -or $test.operands[0] -ne $call.result) {
                    throw "Dynamic invocation lost owned result / explicit error split"
                }
            }
        }
        "perf_var_recursive_alloc" {
            $parameters = @($module.storages | Where-Object {
                $_.kindName -eq "parameter" -and $types[$_.type].name -eq "var"
            })
            if ($parameters.Count -ne 2) { throw "Recursive var parameter coverage changed" }
            foreach ($parameter in $parameters) {
                if (@($module.instructions | Where-Object {
                    $_.kindName -eq "release" -and $_.targetSymbol -eq $parameter.symbol
                }).Count -lt 2) { throw "Recursive managed parameter lost normal/exception cleanup" }
                $function = @($module.functions | Where-Object { $_.declaration -eq $parameter.owner })[0]
                foreach ($block in @($module.blocks | Select-Object -Skip $function.firstBlock -First $function.blockCount)) {
                    $body = @($module.instructions | Select-Object -Skip $block.firstInstruction -First $block.instructionCount)
                    if ($body[-1].kindName -in @("return", "throw") -and
                        @($body | Where-Object { $_.kindName -eq "release" -and $_.targetSymbol -eq $parameter.symbol }).Count -ne 1) {
                        throw "Recursive parameter must be released exactly once on every terminal exit"
                    }
                }
            }
        }
        "cgen_call_scalar_arg_exprs" {
            Require-Operations @{"integer-convert-checked"=3;"xvalue-binary-add"=1;"xvalue-i64"=2}
            foreach ($convert in @($module.instructions | Where-Object { $_.runtimeOpName -eq "integer-convert-checked" })) {
                $test = $module.instructions[$convert.index + 1]
                if ($test.kindName -ne "error-test" -or $test.operands[0] -ne $convert.result) {
                    throw "Scalar argument conversion lost its checked failure edge"
                }
            }
        }
        "cgen_var_return_exprs" {
            $products = @($module.types | Where-Object { $_.cAbi -eq 32 })
            if ($products.Count -ne 5 -or @($products | Where-Object { !$_.errorResultCName }).Count -ne 0) {
                throw "Return signatures lost exact structural products / error carriers"
            }
            $projection = @($module.nodes | Where-Object { $_.kindName -eq "index" -and $_.line -eq 88 })
            $sequence = @($module.instructions | Where-Object { $_.origin -in $projection.id })
            $retain = @($sequence | Where-Object { $_.runtimeOpName -eq "xvalue-retain" })
            $release = @($sequence | Where-Object { $_.kindName -eq "release" })
            if ($projection.Count -ne 1 -or $retain.Count -ne 1 -or $release.Count -ne 2 -or
                $retain[0].index -ge $release[0].index -or
                @($sequence | Where-Object { $_.runtimeOpName -eq "xvalue-index-get-xvalue-key" }).Count -ne 1) {
                throw "Managed return projection does not retain before releasing receiver/key anchors"
            }
        }
        "cgen_expr_statement_discard" {
            Require-Operations @{"xvalue-approx"=1;"xvalue-array-append-new"=3;"xvalue-array-new"=1;
                "xvalue-binary-add"=1;"xvalue-contains"=1;"xvalue-equal"=1;"xvalue-truthy"=4}
            if (@($module.instructions | Where-Object { $_.kindName -eq "release" }).Count -eq 0) {
                throw "Owned expression discard has no release"
            }
        }
        "cgen_array_literal_expr_elements" {
            Require-Operations @{"xvalue-array-new"=6;"xvalue-array-append-new"=11;
                "xvalue-binary-add"=6;"xvalue-retain"=9;"integer-convert-checked"=9}
        }
        "cgen_var_values" {
            Require-Operations @{"xvalue-binary-add"=7;"xvalue-equal"=10;"xvalue-approx"=10;
                "xvalue-order-ge"=2;"xvalue-order-gt"=2;"xvalue-order-le"=1;"xvalue-order-lt"=6;
                "xvalue-truthy"=32;"xvalue-time"=8;"xvalue-type-id"=13;"xvalue-to-string"=4}
            foreach ($operation in @("sub","mul","div","mod","bit-and","bit-or","bit-xor","shl","shr")) {
                Require-Operations @{("xvalue-binary-" + $operation)=1}
            }
            if (@($module.instructions | Where-Object { $_.kindName -eq "branch" }).Count -lt 20) {
                throw "Dynamic short-circuit conditions lost explicit CFG"
            }
        }
    }
}
Write-Output "XLIR core call/value contracts passed: all eight original fixtures, canonical inference, calls, products, ownership, checked conversion and dynamic CFG."
