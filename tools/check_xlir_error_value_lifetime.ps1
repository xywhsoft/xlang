param(
    [string]$Root = "",
    [string]$PublicDirectory = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$Root = (Resolve-Path -LiteralPath $Root).Path
$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\cgen_error_value_lifetime.xl"
$hostSource = Join-Path $Root "test\cases\xlir_error_value_lifetime_host.c"
$nativeTextHost = Join-Path $Root "test\cases\stdlib_error_text_native_host.c"
$outputDir = Join-Path $Root "build\test"
$typedPath = Join-Path $outputDir "cgen_error_value_lifetime_xlir.typed.json"
$typedAgainPath = Join-Path $outputDir "cgen_error_value_lifetime_xlir.typed.again.json"
$loweredPath = Join-Path $outputDir "cgen_error_value_lifetime_xlir.lowered.json"
$loweredAgainPath = Join-Path $outputDir "cgen_error_value_lifetime_xlir.lowered.again.json"
$generatedPath = Join-Path $outputDir "cgen_error_value_lifetime_xlir.c"
$generatedAgainPath = Join-Path $outputDir "cgen_error_value_lifetime_xlir.again.c"
$executable = Join-Path $outputDir "cgen_error_value_lifetime_xlir.exe"

foreach ( $path in @($xl, $source, $hostSource, $nativeTextHost) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR error-value lifetime gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

function Invoke-Xl([string[]]$Arguments, [string]$Failure) {
    & $xl @Arguments
    if ( $LASTEXITCODE -ne 0 ) { throw $Failure }
}

function Assert-ByteEqual([string]$Left, [string]$Right, [string]$Message) {
    if ( ! [System.Linq.Enumerable]::SequenceEqual(
            [byte[]][System.IO.File]::ReadAllBytes($Left),
            [byte[]][System.IO.File]::ReadAllBytes($Right)
        ) ) {
        throw $Message
    }
}

function Find-Definition([object[]]$Instructions, [int]$Value) {
    return $Instructions | Where-Object { $_.result -eq $Value } |
        Select-Object -First 1
}

Invoke-Xl @("ir", $source, "--typed", "--format", "json", "--emit", $typedPath) `
    "XLIR error-value Typed IR failed"
Invoke-Xl @("ir", $source, "--typed", "--format", "json", "--emit", $typedAgainPath) `
    "XLIR error-value repeated Typed IR failed"
Assert-ByteEqual $typedPath $typedAgainPath `
    "XLIR error-value Typed IR is not byte-for-byte deterministic"

$typed = [System.IO.File]::ReadAllText($typedPath, [System.Text.Encoding]::UTF8) |
    ConvertFrom-Json
$errorTypes = @($typed.types | Where-Object {
    $_.name -eq "error" -and $_.cAbi -eq 29 -and
    $_.ownershipAbi -eq 2 -and $_.nullAbi -eq 1 -and
    $_.typeValueId -ne "0x0000000000000000"
})
if ( $errorTypes.Count -ne 1 -or
     @($errorTypes | Select-Object -ExpandProperty typeValueId -Unique).Count -ne 1 ) {
    throw "Typed XLIR error values lost their managed pointer-sentinel TypeValue identity"
}
$fieldNodes = @($typed.nodes | Where-Object {
    $_.sourceKindName -eq "member" -and
    $_.memberAbiName -eq "class-field-error-ref" -and
    $_.storageType -eq $errorTypes[0].id
})
if ( $fieldNodes.Count -ne 5 -or
     @($fieldNodes | Where-Object {$_.ownershipName -eq 'borrow'}).Count -ne 2 -or
     @($fieldNodes | Where-Object {$_.ownershipName -eq 'move'}).Count -ne 3 ) {
    throw "Typed XLIR class error fields lost exact borrowed lvalues or independently owned read projections"
}
$errorCasts = @($typed.nodes | Where-Object {
    $_.sourceKindName -eq "type_as" -and
    $_.castAbiName -eq "xvalue-to-error-checked" -and
    $_.castErrorAbiName -eq "may-raise" -and
    $_.ownershipName -eq "move" -and $_.storageType -ne 0
})
if ( $errorCasts.Count -ne 2 ) {
    throw "Typed XLIR checked var-to-error casts lost target, ownership or failure ABI"
}
$xcall = @($typed.intrinsicCalls | Where-Object {
    $_.kindName -eq "xcall" -and $_.admissionName -eq "verified" -and
    $_.resultAbiName -eq "owned" -and $_.errorAbiName -eq "may-raise"
})
if ( $xcall.Count -ne 1 ) {
    throw "Typed XLIR error callable fixture has no unique verified xcall intrinsic"
}
$errorCallables = @($typed.types | Where-Object {
    $_.kind -eq 4 -and $_.callableErasureAbiName -eq "fixed" -and
    @($_.params).Count -eq 1 -and @($_.returns).Count -eq 1 -and
    @($_.parameterAbis).Count -eq 1 -and $_.parameterAbis[0] -eq "borrow"
})
if ( $errorCallables.Count -ne 1 ) {
    throw "Typed XLIR error callback lost its fixed borrowed-parameter callable ABI"
}
$errorCallable = $errorCallables[0]
$parameterType = $typed.types | Where-Object { $_.id -eq $errorCallable.params[0] } |
    Select-Object -First 1
$resultType = $typed.types | Where-Object { $_.id -eq $errorCallable.returns[0] } |
    Select-Object -First 1
if ( $null -eq $parameterType -or $null -eq $resultType -or
     $parameterType.typeValueId -ne $resultType.typeValueId -or
     $parameterType.typeValueId -ne $errorTypes[0].typeValueId ) {
    throw "Typed XLIR error callback parameter/result do not share the canonical error TypeValue identity"
}

Invoke-Xl @("ir", $source, "--lowered", "--format", "json", "--emit", $loweredPath) `
    "XLIR error-value lowering failed"
Invoke-Xl @("ir", $source, "--lowered", "--format", "json", "--emit", $loweredAgainPath) `
    "XLIR error-value repeated lowering failed"
Assert-ByteEqual $loweredPath $loweredAgainPath `
    "XLIR error-value Lowered IR is not byte-for-byte deterministic"

$lowered = [System.IO.File]::ReadAllText($loweredPath, [System.Text.Encoding]::UTF8) |
    ConvertFrom-Json
$instructions = @($lowered.instructions)
$loweredErrorTypes = @($lowered.types | Where-Object {
    $_.name -eq "error" -and $_.cAbi -eq 29 -and
    $_.ownershipAbi -eq 2 -and $_.nullAbi -eq 1
})
if ( $loweredErrorTypes.Count -ne 1 -or
     @($loweredErrorTypes | Select-Object -ExpandProperty typeValueId -Unique).Count -ne 1 ) {
    throw "Lowered XLIR error TypeIds lost their shared runtime identity"
}
$errorTypeIds = @($loweredErrorTypes | Select-Object -ExpandProperty id)

$expectedRuntimeOps = [ordered]@{
    "xvalue-project-error-ref" = 3
    "xvalue-error-ref" = 4
    "xvalue-callable-from-closure" = 1
    "xvalue-dynamic-call" = 1
}
foreach ( $entry in $expectedRuntimeOps.GetEnumerator() ) {
    $matches = @($instructions | Where-Object {
        $_.kindName -eq "runtime-call" -and $_.runtimeOpName -eq $entry.Key
    })
    if ( $matches.Count -ne $entry.Value ) {
        throw "Lowered XLIR error-value RuntimeOp partition is incomplete for $($entry.Key)"
    }
}
$loweredCasts = @($instructions | Where-Object {
    $_.kindName -eq "runtime-call" -and
    $_.runtimeOpName -eq "xvalue-cast-checked" -and
    $_.castAbiName -eq "xvalue-to-error-checked" -and
    $_.payloadType -eq $_.type -and $_.type -in $errorTypeIds
})
if ( $loweredCasts.Count -ne 2 ) {
    throw "Lowered XLIR lost checked xvalue-to-error RuntimeOps"
}
foreach ( $cast in $loweredCasts ) {
    $next = $instructions | Where-Object { $_.id -eq ([int]$cast.id + 1) } |
        Select-Object -First 1
    if ( $null -eq $next -or $next.kindName -ne "error-test" -or
         ! (@($next.operands) -contains [int]$cast.result) ) {
        throw "Lowered XLIR checked error cast is not immediately guarded by ERROR_TEST"
    }
}

# Acquire the replacement, detach the old owner, publish the new slot, then
# retire the detached temporary. Releasing the old live slot before publication
# would violate the current transactional ownership ABI.
$nullAware = @($instructions | Where-Object { $_.sourceLine -eq 56 })
$nullAwareKinds = @($nullAware | Select-Object -ExpandProperty kindName)
$nullAwareExpected = @('load','null-test','branch','load','retain','load','move','store','store','release','jump')
if ( ($nullAwareKinds -join ",") -ne ($nullAwareExpected -join ",") -or
     $nullAware[5].targetSymbol -ne $nullAware[0].targetSymbol -or
     $nullAware[6].targetSymbol -ne $nullAware[5].targetSymbol -or
     $nullAware[6].operands[0] -ne $nullAware[5].result -or
     $nullAware[7].targetSymbol -ne $nullAware[0].targetSymbol -or
     $nullAware[7].operands[0] -ne $nullAware[4].result -or
     $nullAware[8].targetSymbol -eq $nullAware[7].targetSymbol -or
     $nullAware[8].operands[0] -ne $nullAware[5].result -or
     $nullAware[9].targetSymbol -ne $nullAware[8].targetSymbol ) {
    throw "Lowered XLIR null-aware error assignment lost acquire/detach/publish/retire ownership ordering"
}

# A result is anchored across cleanup in an owning temporary, then explicitly
# detached for RETURN / PRODUCT_MAKE. Follow that owner to RETAIN or the caught
# ERROR_TAKE owner; a moved catch value does not need a redundant retain.
function Assert-ErrorOwnerTransfer([int]$Value, [int]$Consumer) {
    $definition = Find-Definition $instructions $Value
    if ($null -ne $definition -and $definition.kindName -in @('retain','error-take') -and
        $definition.type -in $errorTypeIds -and $definition.resultOwnershipName -eq 'move') { return }
    if ($null -eq $definition -or $definition.kindName -ne 'load' -or
        !($definition.type -in $errorTypeIds)) { throw 'Error result has no exact owning-slot projection' }
    $stores = @($instructions | Where-Object {
        $_.kindName -eq 'store' -and $_.targetSymbol -eq $definition.targetSymbol -and
        $_.id -lt $definition.id -and @($_.operands).Count -eq 1
    })
    $moves = @($instructions | Where-Object {
        $_.kindName -eq 'move' -and $_.targetSymbol -eq $definition.targetSymbol -and
        $_.id -gt $definition.id -and $_.id -lt $Consumer -and
        @($_.operands).Count -eq 1 -and $_.operands[0] -eq $Value
    })
    if ($stores.Count -ne 1 -or $moves.Count -ne 1) { throw 'Error result lost its unique anchored owner and consuming MOVE' }
    Assert-ErrorOwnerTransfer ([int]$stores[0].operands[0]) ([int]$stores[0].id)
}
$ownedErrorReturns = 0
foreach ( $return in @($instructions | Where-Object {
             $_.kindName -eq "return" -and @($_.operands).Count -eq 1 -and
             $_.sourceLine -in @(5, 15, 22, 32)
         }) ) {
    $definition = Find-Definition $instructions ([int]$return.operands[0])
    if ( $null -eq $definition -or !($definition.type -in $errorTypeIds) -or
         $definition.kindName -eq "null" ) {
        continue
    }
    Assert-ErrorOwnerTransfer ([int]$return.operands[0]) ([int]$return.id)
    ++$ownedErrorReturns
}
if ( $ownedErrorReturns -ne 4 ) {
    throw "Lowered XLIR did not prove ownership for every non-null direct error return"
}

$product = @($instructions | Where-Object {
    $_.kindName -eq "product-make" -and $_.sourceLine -eq 27 -and
    @($_.operands).Count -eq 2
})
if ( $product.Count -ne 1 ) {
    throw "Lowered XLIR error tuple return has no unique PRODUCT_MAKE"
}
foreach ( $value in @($product[0].operands) ) {
    Assert-ErrorOwnerTransfer ([int]$value) ([int]$product[0].id)
}
$awaits = @($instructions | Where-Object {
    $_.kindName -eq "await" -and $_.asyncResultAbiName -eq "retained-error" -and
    $_.type -in $errorTypeIds
})
if ( $awaits.Count -ne 3 ) {
    throw "Lowered XLIR repeated/temporary awaits lost retained-error result ABI"
}

Invoke-Xl @("cgen", $source, "--backend", "xlir", "--emit", $generatedPath) `
    "XLIR error-value C generation failed"
Invoke-Xl @("cgen", $source, "--backend", "xlir", "--emit", $generatedAgainPath) `
    "XLIR error-value repeated C generation failed"
Assert-ByteEqual $generatedPath $generatedAgainPath `
    "XLIR error-value C output is not byte-for-byte deterministic"

$generated = [System.IO.File]::ReadAllText($generatedPath, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "static bool xlir_xvalue_cast_checked(",
    "extern xvalue* xl6ErrorValueRefV1(",
    "static xerror* xlir_xvalue_project_error_ref(",
    "static bool xlir_xcall_entry_t",
    "xlir_xvalue_cast_checked(f0, 0xEC7E8C31BA4646DBULL, 21, &a0)",
    "invoked = ((bool (*)(void*, xerror**, xerror*))closure->invoke)",
    "boxed = xl6ErrorValueRefV1(value, 0xEC7E8C31BA4646DBULL)",
    "xerror* value = a0; a0 = (xerror*){0}; xrtErrorFree(value);",
    "xlir_xvalue_dynamic_call_owned(",
    "xlir_future_wait_checked(",
    "xerror* const* payload = (xerror* const*)(xrtFutureValue("
) ) {
    if ( ! $generated.Contains($required) ) {
        throw "XLIR error-value generated C is missing verified mechanical emission: $required"
    }
}
if ([regex]::Matches($generated,
        'v\d+ = \*payload != NULL \? xrtErrorRef\(\*payload\) : NULL;').Count -ne 3 -or
    [regex]::Matches($generated,
        'if \(\*payload != NULL && v\d+ == NULL\) await_ok_\d+ = false;').Count -ne 3) {
    throw 'Every error await must acquire an independent reference and preserve a failed acquisition'
}
foreach ( $forbidden in @(
    "XL6_Function", "xl6XrtCallableCreate", "xl6XrtValueInvoke", "xl6CallResult"
) ) {
    if ( $generated.Contains($forbidden) ) {
        throw "XLIR error-value Cgen regressed to a legacy callable compatibility ABI: $forbidden"
    }
}

Push-Location $Root
try {
    $command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror $hostSource !XL6_CGEN_LINKS! -o $executable && $executable"
    $xlirOutput = @(& $env:ComSpec /d /v:on /c $command 2>&1 |
        ForEach-Object { "$_" })
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR error-value strict-C runtime failed`n$($xlirOutput -join [Environment]::NewLine)"
    }
} finally {
    Pop-Location
}
$expectedOutput = "error value lifetime ok"
if ( !($xlirOutput -contains $expectedOutput) ) {
    throw "XLIR error-value runtime output diverged from the expected observable result"
}

# The same resident reader/text ABI is tested through the actual product, not
# a generated handle implementation. Close each logical fault prefix at no-hit.
Push-Location $Root
try {
    $nativeTextExe = Join-Path $outputDir 'stdlib_error_text_native.exe'
    $command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Irelease -Irelease/include -Werror $nativeTextHost test\support\module_product_memory.c !XL6_CGEN_LINKS! -o $nativeTextExe"
    & $env:ComSpec /d /v:on /c $command
    if ($LASTEXITCODE) { throw 'Resident error text host compilation failed' }
    & $nativeTextExe
    if ($LASTEXITCODE) { throw 'Resident error text vectors failed' }
    foreach ($action in @('box','clone','text','append')) {
        $closed = $false
        for ($point=0; $point -lt 128; ++$point) {
            $verdict = @(& $nativeTextExe $action $point 2>&1)
            if ($LASTEXITCODE) { throw "Resident error text $action fault $point failed: $verdict" }
            if (($verdict -join "`n") -match 'hit=0 balanced') { $closed=$true; break }
        }
        if (!$closed) { throw "Resident error text $action prefix did not close" }
    }
} finally { Pop-Location }

# Reject erased error boxes without the same checked allocation edge used by
# every other allocating box. Verify both local/caught values and callable /
# async error paths through source IR, C and header gates.
Push-Location $Root
try {
    $contractExe = Join-Path $outputDir 'xlir_error_boxing_outcome_contract.exe'
    $flags = @('-std=c11','-O0','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED',
        '-Wno-unused-function','-Wno-unused-label','-Wno-unused-variable',
        '-Wno-unused-parameter','-Wno-unused-const-variable','-Wno-unused-but-set-variable',
        '-Wno-missing-field-initializers','-Wno-pointer-sign','-Wno-format-truncation',
        '-I.','-Ilib/tcc')
    if ($PublicDirectory) {
        $public = Get-Content -LiteralPath (Join-Path $PublicDirectory 'build.json') -Raw | ConvertFrom-Json
        if ($public.RuntimeOnly -or !$public.SourceXrt) { throw 'Error contract requires complete compiler source' }
        foreach ($row in @($public.Inputs)+@($public.Headers)+@($public.Objects)) {
            if ((Get-FileHash -LiteralPath $row.Path).Hash -ne $row.Sha256) { throw "Stale error contract input: $($row.Path)" }
        }
        $compilerInputs = @($public.Objects | Where-Object {
            [IO.Path]::GetFileName($_.Path) -ne 'xlir_public_source_driver.o'
        } | ForEach-Object Path)
    } else {
        $compilerInputs = @('src/ast/xl6_ast.c','src/type/xl6_type_model.c',
            'src/ir/xl6_ir.c','src/ir/xl6_ir_lower.c','src/cgen/xl6_ir_cgen.c',
            'src/cgen/xl6_cgen_shared_runtime.c','src/diag/xl6_diag.c',
            'src/common/xl6_output.c','src/common/xl6_console.c','src/lexer/xl6_token.c',
            'src/lexer/xl6_lexer_api.c','src/lexer/xl6_lexer_gen.c',
            'src/parser/xl6_parser.c','src/parser/xl6_parser_api.c',
            'src/sem/xl6_sem.c','src/sem/xl6_protocol.c','src/sem/xl6_type_registry.c',
            'src/sem/xl6_sem_docs.c','src/sem/xl6_sem_field_attrs.c',
            'src/stdlib/xl6_stdlib.c','src/stdlib/xl6_stdlib_data.c')
    }
    & gcc @flags test/cases/xlir_error_boxing_outcome_contract.c @compilerInputs release/xl.dll `
        -lWs2_32 -lIPHLPAPI -lShell32 -lOle32 -lAdvapi32 -o $contractExe
    if ($LASTEXITCODE) { throw 'Error boxing outcome contract compilation failed' }
    foreach ($fixture in @('stdlib_error_text','cgen_error_value_lifetime')) {
        $prefix = Join-Path $outputDir "$fixture.error_outcome"
        & $contractExe "test/cases/$fixture.xl" "$prefix.c" "$prefix.h" "$prefix.json"
        if ($LASTEXITCODE) { throw "Error boxing outcome contract failed: $fixture" }
        & $contractExe "test/cases/$fixture.xl" "$prefix.repeat.c" "$prefix.repeat.h" "$prefix.repeat.json"
        if ($LASTEXITCODE) { throw "Repeated error boxing outcome contract failed: $fixture" }
        foreach ($suffix in @('c','h','json')) {
            Assert-ByteEqual "$prefix.$suffix" "$prefix.repeat.$suffix" "Error boxing nondeterministic $fixture $suffix"
        }
    }
    foreach ($level in @(0,2)) {
        $errorTextExe = Join-Path $outputDir "stdlib_error_text.oom.O$level.exe"
        $errorTextC = Join-Path $outputDir 'stdlib_error_text.error_outcome.c'
        $memoryObject = Join-Path $outputDir "stdlib_error_text.memory.O$level.o"
        & gcc @flags "-O$level" -Irelease -Irelease/include -c test/support/module_product_memory.c -o $memoryObject
        if ($LASTEXITCODE) { throw "Error text O$level memory support compilation failed" }
        & gcc @flags "-O$level" -Ibuild/test -Irelease -Irelease/include -include $errorTextC `
            test/cases/xlir_module_product_oom_host.c $memoryObject `
            release/xl.dll -o $errorTextExe
        if ($LASTEXITCODE) { throw "Error text strict O$level module host compilation failed" }
        & $errorTextExe
        if ($LASTEXITCODE) { throw "Error text O$level module baseline failed" }
        $closed = $false
        for ($point=0; $point -lt 4097; ++$point) {
            $verdict = @(& $errorTextExe $point 2>&1)
            if ($LASTEXITCODE) { throw "Error text O$level module OOM $point failed: $verdict" }
            if (($verdict -join "`n") -match 'hit=0 balanced') {
                Write-Output "Error text O$level complete OOM prefix: first no-hit=$point"
                $closed = $true
                break
            }
        }
        if (!$closed) { throw "Error text O$level module OOM prefix did not close" }
    }
} finally { Pop-Location }

Write-Output "XLIR error-value lifetime gate ok: managed pointer-sentinel identity, owned returns and tuples, acquire-before-release assignment, class projection, checked dynamic casts/callable adapters, retained-error awaits, deterministic IR/C, and strict C runtime."
exit 0
