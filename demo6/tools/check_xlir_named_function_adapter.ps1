param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$runtimeHost = Join-Path $Root "test\cases\xlir_default_class_layout_host.c"
$externRuntimeHost = Join-Path $Root "test\cases\xlir_named_function_extern_adapter_host.c"
$xcallRuntimeHost = Join-Path $Root "test\cases\xlir_xcall_i64_host.c"
$outputDir = Join-Path $Root "build\test"
$fixtures = @(
    "cgen_function_adapter_dedupe",
    "cgen_function_named_value",
    "cgen_toplevel_typed_func_value",
    "cgen_named_function_argument",
    "cgen_function_multi_return",
    "cgen_c_callback_export",
    "cgen_function_value_error_propagation",
    "cgen_function_param_modes",
    "cgen_extern_function_value",
    "xlir_named_function_variadic_adapter",
    "xlir_named_function_extern_adapter"
)

foreach ( $path in @($xl, $runtimeHost, $externRuntimeHost, $xcallRuntimeHost) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR named-function adapter gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

$dedupeSource = Join-Path $Root "test\cases\cgen_function_adapter_dedupe.xl"
$dedupeJson = Join-Path $outputDir "xlir_named_function_adapter.lowered.json"
& $xl ir $dedupeSource --lowered --format json --emit $dedupeJson
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR named-function adapter lowering failed"
}
$module = [System.IO.File]::ReadAllText(
    $dedupeJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$adapters = @($module.closureTargets | Where-Object {
    $_.kind -eq 2 -and $_.adaptedTargetSymbol -ne 0 -and
    $_.symbol -ne 0 -and $_.captureCount -eq 0 -and
    $_.reachability -eq "transitive"
})
if ( $adapters.Count -ne 1 ) {
    throw "XLIR did not deduplicate the named target/signature pair to one adapter"
}
$adapter = $adapters[0]
$functions = @($module.functions | Where-Object {
    $_.kind -eq 4 -and $_.symbol -eq $adapter.symbol -and
    $_.declaration -eq $adapter.declaration -and
    $_.parameterCount -eq 1 -and $_.reachability -eq "transitive"
})
if ( $functions.Count -ne 1 ) {
    throw "XLIR named adapter has no independent verified function CFG role"
}
$function = $functions[0]
$firstInstructionId = [int]$function.firstInstruction + 1
$lastInstructionId = [int]$function.firstInstruction + [int]$function.instructionCount
$body = @($module.instructions | Where-Object {
    $_.id -ge $firstInstructionId -and $_.id -le $lastInstructionId
})
$directCalls = @($body | Where-Object {
    $_.kindName -eq "call" -and $_.targetSymbol -eq $adapter.adaptedTargetSymbol -and
    $_.operandCount -eq 2
})
$makes = @($module.instructions | Where-Object {
    $_.kindName -eq "closure-make" -and $_.targetSymbol -eq $adapter.symbol -and
    $_.operandCount -eq 0
})
if ( $directCalls.Count -ne 1 -or $makes.Count -ne 2 -or
     @($body | Where-Object { $_.kindName -eq "const" }).Count -lt 1 -or
     @($body | Where-Object { $_.kindName -eq "store" }).Count -lt 1 -or
     @($body | Where-Object { $_.kindName -eq "return" }).Count -ne 1 ) {
    throw "XLIR named adapter CFG does not own default binding, direct call and return"
}

$runtimeBindingSource = Join-Path $Root "test\cases\cgen_function_named_value.xl"
$runtimeBindingJson = Join-Path $outputDir "xlir_named_function_runtime_binding.lowered.json"
& $xl ir $runtimeBindingSource --lowered --format json --emit $runtimeBindingJson
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR named-function runtime binding lowering failed"
}
$runtimeBindingModule = [System.IO.File]::ReadAllText(
    $runtimeBindingJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$defaultBindings = @($runtimeBindingModule.closureTargets | Where-Object {
    $_.kind -eq 2 -and @($_.callParameterNames).Count -eq 2 -and
    $_.callParameterNames[0] -eq "left" -and
    $_.callParameterNames[1] -eq "right" -and
    @($_.callParameterDefaultAbis).Count -eq 2 -and
    $_.callParameterDefaultAbis[0] -eq "none" -and
    $_.callParameterDefaultAbis[1] -eq "i64"
})
if ( $defaultBindings.Count -ne 1 ) {
    throw "XLIR closure target lost its value-specific required/default call-frame partition"
}

$effectSource = Join-Path $Root "test\cases\cgen_function_value_error_propagation.xl"
$effectJson = Join-Path $outputDir "xlir_callable_effect.lowered.json"
& $xl ir $effectSource --lowered --format json --emit $effectJson
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR callable-effect lowering failed"
}
$effectModule = [System.IO.File]::ReadAllText(
    $effectJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$effectAdapters = @($effectModule.closureTargets | Where-Object {
    $_.kind -eq 2 -and $_.adaptedTargetSymbol -ne 0 -and
    $_.symbol -ne 0 -and $_.captureCount -eq 0 -and
    $_.reachability -eq "transitive"
})
if ( $effectAdapters.Count -ne 1 ) {
    throw "XLIR may-raise named target has no unique explicit adapter"
}
$effectAdapter = $effectAdapters[0]
$effectFunctions = @($effectModule.functions | Where-Object {
    $_.kind -eq 4 -and $_.symbol -eq $effectAdapter.symbol -and
    $_.declaration -eq $effectAdapter.declaration -and
    $_.parameterCount -eq 1 -and $_.errorAbi -eq "may-raise" -and
    $_.reachability -eq "transitive"
})
if ( $effectFunctions.Count -ne 1 ) {
    throw "XLIR may-raise named adapter has no verified effect-bearing CFG"
}
$effectFunction = $effectFunctions[0]
$effectFirstInstructionId = [int]$effectFunction.firstInstruction + 1
$effectLastInstructionId = [int]$effectFunction.firstInstruction + [int]$effectFunction.instructionCount
$effectBody = @($effectModule.instructions | Where-Object {
    $_.id -ge $effectFirstInstructionId -and $_.id -le $effectLastInstructionId
})
if ( @($effectBody | Where-Object {
         $_.kindName -eq "call" -and $_.targetSymbol -eq $effectAdapter.adaptedTargetSymbol
     }).Count -ne 1 -or
     @($effectBody | Where-Object {
         $_.kindName -eq "error-test" -and $_.targetSymbol -eq $effectAdapter.adaptedTargetSymbol
     }).Count -ne 1 -or
     @($effectBody | Where-Object { $_.kindName -eq "throw" }).Count -ne 1 -or
     @($effectBody | Where-Object { $_.kindName -eq "return" }).Count -ne 1 ) {
    throw "XLIR may-raise named adapter does not own explicit call/error-test/throw/return CFG"
}
$effectClosureCalls = @($effectModule.instructions | Where-Object { $_.kindName -eq "closure-call" })
if ($effectClosureCalls.Count -ne 1) {
    throw "XLIR first-class callable effect has no universal success carrier and error-test edge"
}
$effectClosureOutcome = $effectModule.instructions[[int]$effectClosureCalls[0].index + 1]
$effectClosureTests = @($effectModule.instructions | Where-Object {
    $_.kindName -eq 'error-test' -and $_.targetSymbol -eq 0 -and
    $_.operandCount -eq 1 -and $_.operands[0] -eq $effectClosureCalls[0].result
})
if ($effectClosureTests.Count -ne 1 -or $effectClosureOutcome.id -ne $effectClosureTests[0].id) {
    throw "XLIR first-class callable outcome must be tested exactly once before its distinct cleanup outcome"
}
$bridges = @($effectModule.callTargets | Where-Object {
    $_.runtimeBridgeName -in @("print", "string-contains")
})
if ( $bridges.Count -ne 2 -or
     @($bridges | Where-Object {
         $_.runtimeBridgeName -eq "string-contains" -and
         $_.cName -eq "xl6_xrt_string_contains" -and $_.parameterCount -eq 3
     }).Count -ne 1 ) {
    throw "XLIR standard wrapper dependencies have no explicit verified runtime-bridge ABI"
}

$modeSource = Join-Path $Root "test\cases\cgen_function_param_modes.xl"
$modeJson = Join-Path $outputDir "xlir_named_function_parameter_modes.lowered.json"
& $xl ir $modeSource --lowered --format json --emit $modeJson
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR named-function parameter-mode lowering failed"
}
$modeModule = [System.IO.File]::ReadAllText(
    $modeJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$byrefTypes = @($modeModule.types | Where-Object {
    @($_.parameterModes).Count -eq 1 -and $_.parameterModes[0] -eq "byref" -and
    @($_.parameterAbis).Count -eq 1 -and $_.parameterAbis[0] -eq "inout"
})
$outTypes = @($modeModule.types | Where-Object {
    @($_.parameterModes).Count -eq 1 -and $_.parameterModes[0] -eq "out" -and
    @($_.parameterAbis).Count -eq 1 -and $_.parameterAbis[0] -eq "out"
})
if ( $byrefTypes.Count -ne 1 -or $outTypes.Count -ne 1 ) {
    throw "XLIR function parameter modes are not canonical type identities with exact physical ABIs"
}
if ( @($modeModule.closureTargets | Where-Object { $_.kind -eq 2 }).Count -ne 2 -or
     @($modeModule.functions | Where-Object { $_.kind -eq 4 }).Count -ne 2 -or
     @($modeModule.instructions | Where-Object { $_.kindName -eq "closure-call" }).Count -ne 2 -or
     @($modeModule.instructions | Where-Object { $_.kindName -eq "storage-address" }).Count -lt 4 ) {
    throw "XLIR function byref/out adapters do not carry explicit storage-reference CFG"
}

Push-Location $Root
try {
    foreach ( $fixture in $fixtures ) {
        $source = Join-Path $Root "test\cases\$fixture.xl"
        $generated = Join-Path $outputDir "$fixture.named-adapter.c"
        $exe = Join-Path $outputDir "$fixture.named-adapter.exe"
        if ( ! (Test-Path -LiteralPath $source -PathType Leaf) ) {
            throw "XLIR named-function adapter fixture is missing: $source"
        }
        & $xl cgen $source --backend xlir --emit $generated
        if ( $LASTEXITCODE -ne 0 ) {
            throw "XLIR named-function adapter Cgen failed: $fixture"
        }
        $generatedText = [System.IO.File]::ReadAllText(
            $generated, [System.Text.Encoding]::UTF8)
        if ( ! $generatedText.Contains("xlir_closure_new(") -or
             ! $generatedText.Contains("xlir_closure_invoke_") ) {
            throw "XLIR generated C lost the mechanical named-adapter closure ABI: $fixture"
        }
        if ( $fixture -eq "cgen_function_value_error_propagation" -and
             (! [regex]::IsMatch($generatedText,
                 'static bool xlir_closure_invoke_[^(]+\(void\* environment, int64_t\* result, int64_t a0\)') -or
              ! $generatedText.Contains("((bool (*)(void*, int64_t*, int64_t))") -or
              ! $generatedText.Contains("bool xl6_xrt_string_contains(")) ) {
            throw "XLIR generated C lost the uniform callable-effect or explicit runtime-bridge ABI"
        }
        if ( $fixture -eq "cgen_function_param_modes" -and
             (! [regex]::IsMatch($generatedText,
                 'static bool xlir_closure_invoke_[^(]+\(void\* environment, int64_t\* a0\)') -or
              @([regex]::Matches($generatedText,
                 'static void xlir_fn_[^(]+\(void\* environment, int64_t\* s')).Count -lt 2 -or
              @([regex]::Matches($generatedText,
                 '\(int64_t\*\)v\d+')).Count -lt 4) ) {
            throw "XLIR generated C lost the typed byref/out closure boundary"
        }
        if ( $fixture -eq "cgen_function_named_value" -and
             (! $generatedText.Contains("xlir_closure_call_defaults_") -or
              ! $generatedText.Contains("call_parameter_defaults") -or
              ! $generatedText.Contains("runtime callable argument binding is incomplete")) ) {
            throw "XLIR generated C lost the mechanical closure call-frame default descriptor"
        }
        $selectedRuntimeHost = if ( $fixture -eq "xlir_named_function_extern_adapter" ) {
            $externRuntimeHost
        } elseif ( $fixture -eq "cgen_extern_function_value" ) {
            $xcallRuntimeHost
        } else {
            $runtimeHost
        }
        $command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include $generated $selectedRuntimeHost !XL6_CGEN_LINKS! -o $exe && $exe"
        & $env:ComSpec /d /v:on /c $command
        if ( $LASTEXITCODE -ne 0 ) {
            throw "XLIR named-function adapter strict-C runtime failed: $fixture"
        }
    }
} finally {
    Pop-Location
}

Write-Output "XLIR named-function adapter gate ok: deduplicated no-raise/may-raise adapter CFGs, value-specific runtime default binding, universal callable effect carrier, product result, C callback/export and extern targets, explicit runtime bridges, strict C, and runtimes."
exit 0
