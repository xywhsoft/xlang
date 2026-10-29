param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"
if ( $Root -eq "" ) { $Root = Split-Path -Parent $PSScriptRoot }

$xl = Join-Path $Root "release\xl.exe"
$moduleHost = Join-Path $Root "test\cases\xlir_generated_module_host.c"
$fixtures = @(
    "nullable_function_frames",
    "nullable_function_lifecycle"
)

foreach ( $path in @($xl, $moduleHost) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR nullable function-value ABI gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path `
    (Join-Path $Root "build\tmp"),(Join-Path $Root "build\test") | Out-Null

Push-Location $Root
try {
    foreach ( $name in $fixtures ) {
        $source = Join-Path $Root "test\cases\$name.xl"
        $typedPath = Join-Path $Root "build\tmp\$name.function-value.typed.json"
        $loweredPath = Join-Path $Root "build\tmp\$name.function-value.lowered.json"
        $generatedPath = Join-Path $Root "build\test\$name.function-value.c"
        $repeatPath = Join-Path $Root "build\tmp\$name.function-value.repeat.c"
        $exePath = Join-Path $Root "build\tmp\$name.function-value.exe"
        if ( ! (Test-Path -LiteralPath $source -PathType Leaf) ) {
            throw "XLIR nullable function-value fixture is missing: $source"
        }

        & $xl ir $source --typed --format json --emit $typedPath
        if ( $LASTEXITCODE -ne 0 ) {
            throw "XLIR nullable function-value Typed dump failed: $name"
        }
        $typed = [System.IO.File]::ReadAllText(
            $typedPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
        $nullableTypes = @($typed.types | Where-Object {
            [int]$_.kind -eq 8 -and [int]$_.cAbi -eq 31 -and
            [int]$_.nullAbi -eq 1 -and @($_.params).Count -eq 1
        })
        $nullable = @($nullableTypes | Where-Object {
            $elementId = [int]$_.params[0]
            @($typed.types | Where-Object {
                [int]$_.id -eq $elementId -and [int]$_.kind -eq 4 -and
                [int]$_.cAbi -eq 31 -and [int]$_.callableResultType -ne 0
            }).Count -eq 1
        })
        if ( $nullable.Count -ne 1 ) {
            throw "Typed XLIR lost the unique pointer-sentinel nullable closure ABI: $name"
        }
        $nullableType = [int]$nullable[0].id
        $signatureType = [int]$nullable[0].params[0]

        if ( $name -eq "nullable_function_lifecycle" ) {
            $field = @($typed.classFields | Where-Object {
                [int]$_.type -eq $nullableType -and
                $_.initializerAbiName -eq "null-sentinel" -and
                [int]$_.initializer -ne 0 -and
                $_.initializerErrorAbiName -eq "no-raise"
            })
            if ( $field.Count -ne 1 ) {
                throw "Typed XLIR class field did not classify explicit null as a no-raise null-sentinel ABI"
            }
            $fieldDeclaration = [int]$field[0].declaration
        }

        & $xl ir $source --lowered --format json --emit $loweredPath
        if ( $LASTEXITCODE -ne 0 ) {
            throw "XLIR nullable function-value lowering failed: $name"
        }
        $lowered = [System.IO.File]::ReadAllText(
            $loweredPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
        if ( @($lowered.instructions | Where-Object {
                $_.kindName -eq "nullable-wrap" -and
                [int]$_.type -eq $nullableType
            }).Count -lt 1 ) {
            throw "Lowered XLIR nullable function return has no explicit NULLABLE_WRAP boundary: $name"
        }

        if ( $name -eq "nullable_function_lifecycle" ) {
            $fieldInit = @($lowered.instructions | Where-Object {
                [int]$_.origin -eq $fieldDeclaration
            })
            if ( @($fieldInit | Where-Object {
                    $_.runtimeOpName -eq "xvalue-null"
                }).Count -ne 1 -or
                 @($fieldInit | Where-Object {
                    $_.kindName -eq "error-test" -or $_.kindName -eq "throw"
                }).Count -ne 0 ) {
                throw "Lowered XLIR explicit-null class field initializer acquired a spurious error edge"
            }
            $stores = @($lowered.instructions | Where-Object {
                $_.runtimeOpName -eq "xvalue-object-set-literal-closure"
            })
            $loads = @($lowered.instructions | Where-Object {
                $_.runtimeOpName -eq "xvalue-object-get-literal-closure"
            })
            if ( $stores.Count -lt 2 -or $loads.Count -ne 1 ) {
                throw "Lowered XLIR nullable class function field lost its typed store/load operations"
            }
            foreach ( $instruction in @($stores + $loads) ) {
                if ( [int]$instruction.calleeType -ne $signatureType ) {
                    throw "Lowered XLIR class function field conflated nullable storage with callable signature TypeId"
                }
            }
            foreach ( $instruction in $stores ) {
                $closureValue = [int]$instruction.operands[1]
                $definition = @($lowered.instructions | Where-Object {
                    [int]$_.result -eq $closureValue
                })
                if ( $definition.Count -ne 1 -or
                     [int]$definition[0].type -ne $nullableType ) {
                    throw "Lowered XLIR nullable class function field store lost its pointer-sentinel operand TypeId"
                }
            }
            if ( [int]$loads[0].type -ne $nullableType ) {
                throw "Lowered XLIR nullable class function field load lost its pointer-sentinel result TypeId"
            }
        }

        & $xl cgen $source --backend xlir --emit $generatedPath
        if ( $LASTEXITCODE -ne 0 ) {
            throw "XLIR nullable function-value Cgen failed: $name"
        }
        & $xl cgen $source --backend xlir --emit $repeatPath
        if ( $LASTEXITCODE -ne 0 ) {
            throw "XLIR nullable function-value repeat Cgen failed: $name"
        }
        if ( ! [System.Linq.Enumerable]::SequenceEqual(
                [byte[]][System.IO.File]::ReadAllBytes($generatedPath),
                [byte[]][System.IO.File]::ReadAllBytes($repeatPath)
            ) ) {
            throw "XLIR nullable function-value C output is not byte-for-byte deterministic: $name"
        }
        $generated = [System.IO.File]::ReadAllText(
            $generatedPath, [System.Text.Encoding]::UTF8)
        foreach ( $required in @("xlir_closure*", "xlir_closure_release(") ) {
            if ( ! $generated.Contains($required) ) {
                throw "XLIR nullable function-value mechanical C is incomplete: $name / $required"
            }
        }
        if ( $name -eq "nullable_function_lifecycle" ) {
            foreach ( $required in @(
                "xlir_xvalue_object_set_literal_closure(",
                "xlir_xvalue_object_get_literal_closure(",
                "xrtValueRetain(xrtValueNull())",
                "xlir_closure* closure; *output = NULL;",
                "if (source == NULL || entry == NULL) goto incompatible;",
                "if (nullable && xrtValueType(source) == 0) return true;",
                "if (xrtValueType(source) == 0 || !xrtValueIsCallable(source)) goto incompatible;"
            ) ) {
                if ( ! $generated.Contains($required) ) {
                    throw "XLIR nullable class function field C bridge is incomplete: $required"
                }
            }
        }

        & $xl run $source
        if ( $LASTEXITCODE -ne 0 ) {
            throw "XLIR nullable function-value CLI runtime failed: $name"
        }
        $relativeGenerated = "build\test\$name.function-value.c"
        $relativeExe = "build\tmp\$name.function-value.exe"
        $command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED -include $relativeGenerated test\cases\xlir_generated_module_host.c !XL6_CGEN_LINKS! -o $relativeExe && $relativeExe"
        & $env:ComSpec /d /v:on /c $command
        if ( $LASTEXITCODE -ne 0 ) {
            throw "XLIR nullable function-value strict-C/runtime gate failed: $name"
        }
    }
} finally {
    Pop-Location
}

Write-Output "XLIR nullable function-value ABI gate passed: pointer-sentinel closure returns use explicit NULLABLE_WRAP, explicit-null class fields remain no-raise, field storage TypeId stays distinct from callable signature TypeId, and deterministic strict-C and CLI runtimes agree."
