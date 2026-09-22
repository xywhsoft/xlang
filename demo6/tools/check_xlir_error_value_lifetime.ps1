param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$Root = (Resolve-Path -LiteralPath $Root).Path
$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\cgen_error_value_lifetime.xl"
$hostSource = Join-Path $Root "test\cases\xlir_error_value_lifetime_host.c"
$outputDir = Join-Path $Root "build\test"
$typedPath = Join-Path $outputDir "cgen_error_value_lifetime_xlir.typed.json"
$typedAgainPath = Join-Path $outputDir "cgen_error_value_lifetime_xlir.typed.again.json"
$loweredPath = Join-Path $outputDir "cgen_error_value_lifetime_xlir.lowered.json"
$loweredAgainPath = Join-Path $outputDir "cgen_error_value_lifetime_xlir.lowered.again.json"
$generatedPath = Join-Path $outputDir "cgen_error_value_lifetime_xlir.c"
$generatedAgainPath = Join-Path $outputDir "cgen_error_value_lifetime_xlir.again.c"
$executable = Join-Path $outputDir "cgen_error_value_lifetime_xlir.exe"

foreach ( $path in @($xl, $source, $hostSource) ) {
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
if ( $errorTypes.Count -lt 2 -or
     @($errorTypes | Select-Object -ExpandProperty typeValueId -Unique).Count -ne 1 ) {
    throw "Typed XLIR error values lost their managed pointer-sentinel TypeValue identity"
}
$fieldNodes = @($typed.nodes | Where-Object {
    $_.sourceKindName -eq "member" -and
    $_.memberAbiName -eq "class-field-error-ref" -and
    $_.storageType -ne 0 -and $_.ownershipName -eq "borrow"
})
if ( $fieldNodes.Count -ne 5 ) {
    throw "Typed XLIR class error fields do not use the exact borrowed error projection ABI"
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
if ( $loweredErrorTypes.Count -lt 2 -or
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

# The null-aware assignment must acquire the replacement before releasing the
# old slot.  This is part of XLIR ownership semantics, not a Cgen convention.
$nullAwareKinds = @($instructions | Where-Object { $_.sourceLine -eq 56 } |
    Select-Object -ExpandProperty kindName)
$nullAwareExpected = @("load", "null-test", "branch", "load", "retain", "release", "store", "jump")
if ( ($nullAwareKinds -join ",") -ne ($nullAwareExpected -join ",") ) {
    throw "Lowered XLIR null-aware error assignment lost its test/acquire/release/store ordering"
}

# Every non-null direct error return must be rooted in RETAIN.  VALUE is the
# explicit rebind between physically equal frontend/canonical error TypeIds.
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
    if ( $definition.kindName -eq "value" -and @($definition.operands).Count -eq 1 ) {
        $sourceDefinition = Find-Definition $instructions ([int]$definition.operands[0])
        if ( $null -eq $sourceDefinition -or $sourceDefinition.kindName -ne "retain" ) {
            throw "Lowered XLIR error return TypeId rebind is not backed by RETAIN"
        }
        $sourceType = $lowered.types | Where-Object { $_.id -eq $sourceDefinition.type } |
            Select-Object -First 1
        $targetType = $lowered.types | Where-Object { $_.id -eq $definition.type } |
            Select-Object -First 1
        if ( $null -eq $sourceType -or $null -eq $targetType -or
             $sourceType.typeValueId -ne $targetType.typeValueId -or
             $sourceType.cAbi -ne $targetType.cAbi -or
             $sourceType.ownershipAbi -ne $targetType.ownershipAbi ) {
            throw "Lowered XLIR error return rebind crosses incompatible runtime identities"
        }
    }
    elseif ( $definition.kindName -ne "retain" ) {
        throw "Lowered XLIR returned a borrowed error without an explicit ownership operation"
    }
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
    $definition = Find-Definition $instructions ([int]$value)
    if ( $null -eq $definition -or $definition.kindName -ne "retain" -or
         !($definition.type -in $errorTypeIds) ) {
        throw "Lowered XLIR error tuple component is not independently owned"
    }
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
    "static xvalue* xlir_xvalue_error_ref(",
    "static xerror* xlir_xvalue_project_error_ref(",
    "static bool xlir_xcall_entry_t",
    "xlir_xvalue_cast_checked(f0, 0xEC7E8C31BA4646DBULL, 21, &a0)",
    "invoked = ((bool (*)(void*, xerror**, xerror*))closure->invoke)",
    "boxed = xlir_xvalue_error_ref(value, 0xEC7E8C31BA4646DBULL)",
    "xrtErrorFree(a0); a0 = NULL;",
    "xlir_xvalue_dynamic_call_owned(",
    "xlir_future_await_error("
) ) {
    if ( ! $generated.Contains($required) ) {
        throw "XLIR error-value generated C is missing verified mechanical emission: $required"
    }
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

Write-Output "XLIR error-value lifetime gate ok: managed pointer-sentinel identity, owned returns and tuples, acquire-before-release assignment, class projection, checked dynamic casts/callable adapters, retained-error awaits, deterministic IR/C, and strict C runtime."
exit 0
