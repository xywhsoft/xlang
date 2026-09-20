param([string]$Root = "")
$ErrorActionPreference = "Stop"
if ($Root -eq "") { $Root = Split-Path -Parent $PSScriptRoot }
$xl = Join-Path $Root "release\xl.exe"
$cases = @("cgen_runtime_type_baseline", "cgen_type_conversions", "cgen_implicit_methods",
    "cgen_closure", "cgen_closure_resource_capture", "xlir_open_object_field_lifetime",
    "xlir_refcounted_storage_assignment_lifetime")
function Require-Ops([hashtable]$Expected) {
    foreach ($pair in $Expected.GetEnumerator()) {
        if (@($module.instructions | Where-Object { $_.runtimeOpName -eq $pair.Key }).Count -ne $pair.Value) {
            throw "$name lost exact operation count: $($pair.Key) = $($pair.Value)"
        }
    }
}
foreach ($name in $cases) {
    $source = Join-Path $Root "test\cases\$name.xl"
    $prefix = Join-Path $Root "build\test\${name}_core_type"
    & $xl ir $source --lowered --format json --emit "$prefix.json"
    if ($LASTEXITCODE -ne 0) { throw "Core type/closure lowering failed: $name" }
    & $xl cgen $source --emit "$prefix.c"
    if ($LASTEXITCODE -ne 0) { throw "Core type/closure Cgen failed: $name" }
    & $xl cgen $source --backend xlir --emit "$prefix.repeat.c"
    if ($LASTEXITCODE -ne 0) { throw "Core type/closure repeated Cgen failed: $name" }
    $c = [IO.File]::ReadAllText("$prefix.c")
    if ($c -cne [IO.File]::ReadAllText("$prefix.repeat.c") -or
        $c -match '__xl6_user_global_|<__xl_runtime/xl_runtime\.h>|\(\{') {
        throw "Core type/closure regressed to legacy or nondeterministic emission: $name"
    }
    $module = [IO.File]::ReadAllText("$prefix.json") | ConvertFrom-Json
    $types = @{}; $definitions = @{}
    foreach ($type in $module.types) { $types[$type.id] = $type }
    foreach ($instruction in $module.instructions) {
        if ($instruction.result -ne 0) { $definitions[$instruction.result] = $instruction }
    }
    if ($name -in @("cgen_runtime_type_baseline", "xlir_open_object_field_lifetime")) {
        $factory = @($module.callTargets | Where-Object { $_.cName -eq "xl6_xrt_dynamic_fields_create" })
        if ($factory.Count -ne 1 -or $factory[0].runtimeBridgeName -ne "dynamic-fields" -or
            $factory[0].resultPhysicalAbiName -ne "native-class-field-take" -or
            $factory[0].parameterCount -ne 0 -or $factory[0].resultPhysicalDestroySymbol -eq 0 -or
            $factory[0].resultPhysicalFieldDeclaration -eq 0 -or
            $types[$factory[0].returnType].ownershipAbi -ne 2 -or
            $factory[0].errorAbiName -ne "no-raise") {
            throw "Dynamic fields factory lacks verified owned native-result adapter"
        }
        if (!$c.Contains('#include <__xl_runtime/xruntime_decl.h>') -or
            !$c.Contains('static void* xl6_xrt_dynamic_fields_create(void)') -or
            $c -match 'extern\s+void\*\s+xrtDynamicFieldsCreate' -or
            @($module.callTargets | Where-Object { $_.cName -eq "xrtDynamicFieldsCreate" }).Count -ne 0) {
            throw "Dynamic fields bypasses the exact public opaque-pointer ABI"
        }
    }
    switch ($name) {
        "xlir_refcounted_storage_assignment_lifetime" {
            $writes = @($module.instructions | Where-Object {
                $_.kindName -eq "store" -and $types[$_.type].cAbi -eq 31 -and
                $module.nodes[$_.origin - 1].kindName -eq "assign"
            })
            if ($writes.Count -ne 12) { throw "Closure local/global/static replacement coverage changed" }
            foreach ($write in $writes) {
                $drop = $module.instructions[$write.index - 1]
                if ($drop.kindName -ne "release" -or $drop.targetSymbol -ne $write.targetSymbol -or
                    $drop.type -ne $write.type -or $definitions[$write.operands[0]].index -ge $drop.index) {
                    throw "Storage replacement did not acquire input before dropping its old owner"
                }
            }
            if (@($module.instructions | Where-Object {
                $_.kindName -eq "retain" -and $types[$_.type].cAbi -eq 31
            }).Count -lt 6) { throw "Alias/self assignment lost explicit closure acquisition" }
        }
        "xlir_open_object_field_lifetime" {
            $reads = @($module.nodes | Where-Object {
                $_.kindName -in @("member", "index") -and
                $_.callKindName -in @("property-get", "protocol") -and
                $types[$_.storageType].ownershipAbi -eq 2 -and
                $module.nodes[$_.parent - 1].kindName -ne "assign"
            })
            if ($reads.Count -lt 8) { throw "Managed getter projection regression coverage was removed" }
            foreach ($read in $reads) {
                if ($read.valueCategoryName -ne "rvalue" -or $read.ownershipName -ne "move" -or
                    $read.ownershipPlanned -ne 1) {
                    throw "Managed getter retained ambiguous source-lvalue ownership"
                }
            }
        }
        "cgen_runtime_type_baseline" {
            Require-Ops @{"xvalue-type-id"=11;"xvalue-union-box"=1;
                "xvalue-callable-from-closure"=1;"xvalue-dynamic-call"=1}
            foreach ($nominal in @("Base", "Child", "Actor", "Native")) {
                $type = @($module.types | Where-Object { $_.name -eq $nominal })
                if ($type.Count -ne 1 -or $type[0].cAbi -ne 26 -or $type[0].ownershipAbi -ne 2 -or
                    $type[0].typeValueId -eq "0x0000000000000000" -or
                    @($module.instructions | Where-Object {
                        $_.runtimeOpName -eq "xvalue-class-identity-bind" -and $_.payloadType -eq $type[0].id
                    }).Count -eq 0) { throw "Nominal object lost its physical managed identity: $nominal" }
            }
            $array = @($module.types | Where-Object { $_.displayName -eq "array<Child>" })
            if ($array.Count -ne 1 -or $array[0].ownershipAbi -ne 2 -or
                @($module.instructions | Where-Object {
                    $_.runtimeOpName -eq "xvalue-type-bind" -and $_.runtimeIdentityType -eq $array[0].id
                }).Count -eq 0) { throw "Typed class array lost its exact runtime identity binding" }
        }
        "cgen_type_conversions" {
            Require-Ops @{"integer-convert-checked"=5;"string-to-f64-checked"=1;
                "string-to-time-checked"=1;"xvalue-to-time-checked"=1;
                "i64-to-string"=1;"bool-to-string"=1;"time-to-string"=1;"xvalue-to-string"=1}
            foreach ($op in @($module.instructions | Where-Object { $_.runtimeOpName -match '-checked$' })) {
                $test = $module.instructions[$op.index + 1]
                if ($test.kindName -ne "error-test" -or $test.operands[0] -ne $op.result) {
                    throw "Checked conversion lost immediate explicit error routing"
                }
            }
        }
        "cgen_implicit_methods" {
            Require-Ops @{"i64-to-string"=2;"bool-to-string"=1;"string-copy"=1;"xvalue-to-string"=1}
        }
        "cgen_closure" {
            if (@($module.closureCaptures).Count -ne 1 -or $module.closureCaptures[0].abiName -ne "copy-value" -or
                @($module.instructions | Where-Object { $_.kindName -eq "retain" }).Count -eq 0 -or
                @($module.instructions | Where-Object { $_.kindName -eq "closure-call" }).Count -eq 0) {
                throw "Closure environment, copy ownership or typed invocation contract was removed"
            }
        }
        "cgen_closure_resource_capture" {
            if ((@($module.closureCaptures.abiName | Sort-Object) -join ",") -ne "copy-value,retain-closure,retain-xvalue") {
                throw "Resource closure captures lost exact acquisition/drop ABI"
            }
            if (@($module.instructions | Where-Object { $_.kindName -eq "release" -and $types[$_.type].cAbi -eq 31 }).Count -lt 2 -or
                @($module.instructions | Where-Object { $_.kindName -eq "retain" -and $types[$_.type].cAbi -eq 31 }).Count -eq 0) {
                throw "Closure reassignment lost release-before-overwrite / borrowed-source retain"
            }
        }
    }
    if ($name -in @("cgen_closure", "cgen_closure_resource_capture")) {
        if ($c -match '(?<![A-Za-z0-9_])(malloc|calloc|realloc|free)\s*\(' -or
            $c -notmatch 'xrt(Malloc|Calloc)\(' -or !$c.Contains('xrtFree(')) {
            throw "Closure storage escaped the XRT allocator boundary"
        }
    }
}
Write-Output "XLIR core type/closure contract passed: nominal and composite identity, public dynamic-field ABI, checked conversions, closure captures and allocator ownership."
