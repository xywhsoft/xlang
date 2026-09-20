param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\cgen_container_ops.xl"
$hostSource = Join-Path $Root "test\cases\xlir_default_class_layout_host.c"
$outputDir = Join-Path $Root "build\test"
$typedPath = Join-Path $outputDir "xlir_collection_algebra.typed.json"
$loweredPath = Join-Path $outputDir "xlir_collection_algebra.lowered.json"
$generated = Join-Path $outputDir "xlir_collection_algebra.c"
$generatedRepeat = Join-Path $outputDir "xlir_collection_algebra.repeat.c"
$executable = Join-Path $outputDir "xlir_collection_algebra.exe"

foreach ( $path in @($xl, $source, $hostSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR collection-algebra gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

& $xl ir $source --format json --emit $typedPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR collection-algebra Typed dump failed" }
& $xl ir $source --lowered --format json --emit $loweredPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR collection-algebra Lowered dump failed" }
$typed = [System.IO.File]::ReadAllText(
    $typedPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$lowered = [System.IO.File]::ReadAllText(
    $loweredPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json

function Get-Type($module, [int]$id) {
    return @($module.types | Where-Object { [int]$_.id -eq $id })[0]
}

$valueDefinitions = @{}
foreach ( $instruction in @($lowered.instructions) ) {
    if ( [int]$instruction.result -ne 0 ) {
        $valueDefinitions[[string][int]$instruction.result] = $instruction
    }
}

$expectedOperations = [ordered]@{
    "xvalue-array-concat" = 2
    "xvalue-list-merge-right" = 2
    "xvalue-list-without-i64-key" = 2
    "xvalue-dict-merge-right" = 2
    "xvalue-dict-without-string-key" = 2
    "xvalue-set-union" = 2
    "xvalue-set-intersection" = 2
    "xvalue-set-difference" = 2
    "xvalue-set-symmetric-difference" = 2
    "xvalue-set-subset" = 1
    "xvalue-set-proper-subset" = 1
    "xvalue-set-superset" = 1
    "xvalue-set-proper-superset" = 1
}
$collectionOperations = @($lowered.instructions | Where-Object {
    $expectedOperations.Contains($_.runtimeOpName)
})
foreach ( $expected in $expectedOperations.GetEnumerator() ) {
    $matches = @($collectionOperations | Where-Object {
        $_.runtimeOpName -eq $expected.Key
    })
    if ( $matches.Count -ne [int]$expected.Value ) {
        throw "XLIR collection-algebra operation count changed: $($expected.Key)"
    }
}
if ( $collectionOperations.Count -ne
     ($expectedOperations.Values | Measure-Object -Sum).Sum ) {
    throw "XLIR collection-algebra gate found an unexpected operation"
}

$relationOperations = @(
    "xvalue-set-subset", "xvalue-set-proper-subset",
    "xvalue-set-superset", "xvalue-set-proper-superset"
)
foreach ( $instruction in $collectionOperations ) {
    if ( $instruction.kindName -ne "runtime-call" -or
         $instruction.opcodeName -ne "none" -or
         [int]$instruction.hasImmediate -ne 0 -or
         [int]$instruction.operandCount -ne 2 -or
         $instruction.collectionAbiName -ne "none" -or
         $instruction.typedCollectionOpName -ne "invalid" -or
         $instruction.dynamicCollectionOpName -ne "invalid" ) {
        throw "XLIR collection algebra is not a closed two-operand runtime operation: $($instruction.runtimeOpName)"
    }
    $left = $valueDefinitions[[string][int]$instruction.operands[0]]
    $right = $valueDefinitions[[string][int]$instruction.operands[1]]
    if ( $null -eq $left -or $null -eq $right ) {
        throw "XLIR collection algebra operand has no unique SSA definition"
    }
    $leftType = Get-Type $lowered ([int]$left.type)
    $rightType = Get-Type $lowered ([int]$right.type)
    $resultType = Get-Type $lowered ([int]$instruction.type)
    $expectedCollection = if ( $instruction.runtimeOpName.StartsWith("xvalue-array-") ) {
        "array"
    } elseif ( $instruction.runtimeOpName.StartsWith("xvalue-list-") ) {
        "list"
    } elseif ( $instruction.runtimeOpName.StartsWith("xvalue-dict-") ) {
        "dict"
    } else {
        "set"
    }
    if ( $null -eq $leftType -or $null -eq $rightType -or
         $null -eq $resultType -or
         $leftType.collectionAbiName -ne $expectedCollection -or
         [int]$leftType.cAbi -ne 26 -or [int]$leftType.ownershipAbi -ne 2 ) {
        throw "XLIR collection algebra lost its exact managed collection TypeId: $($instruction.runtimeOpName)"
    }
    if ( $instruction.runtimeOpName -eq "xvalue-list-without-i64-key" ) {
        if ( [int]$instruction.type -ne [int]$left.type -or
             [int]$rightType.cAbi -ne 3 ) {
            throw "XLIR list subtraction lost its exact list/i64/list signature"
        }
    } elseif ( $instruction.runtimeOpName -eq
                   "xvalue-dict-without-string-key" ) {
        if ( [int]$instruction.type -ne [int]$left.type -or
             [int]$rightType.cAbi -ne 25 -or
             [int]$rightType.ownershipAbi -ne 2 ) {
            throw "XLIR dict subtraction lost its exact dict/string/dict signature"
        }
    } elseif ( $instruction.runtimeOpName -in $relationOperations ) {
        if ( [int]$right.type -ne [int]$left.type -or
             [int]$resultType.cAbi -ne 2 ) {
            throw "XLIR set relation lost its exact set/set/bool signature"
        }
    } elseif ( [int]$right.type -ne [int]$left.type -or
               [int]$instruction.type -ne [int]$left.type ) {
        throw "XLIR collection algebra lost its exact same-TypeId result signature: $($instruction.runtimeOpName)"
    }
}

$typedInsert = @($typed.intrinsicCalls | Where-Object {
    $_.kindName -eq "typed-collection-member" -and
    $_.typedCollectionOpName -eq "insert" -and
    $_.collectionAbiName -eq "array"
})
$loweredInsert = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -eq "xvalue-typed-collection-method" -and
    $_.typedCollectionOpName -eq "insert" -and
    $_.collectionAbiName -eq "array"
})
if ( $typedInsert.Count -ne 1 -or $loweredInsert.Count -ne 1 -or
     [int]$typedInsert[0].operandCount -ne 2 -or
     [int]$loweredInsert[0].operandCount -ne 3 ) {
    throw "XLIR typed array insert has no unique index/item operation ABI"
}

& $xl cgen $source --backend xlir --emit $generated
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR collection-algebra Cgen failed" }
& $xl cgen $source --backend xlir --emit $generatedRepeat
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR collection-algebra repeat Cgen failed" }
$firstBytes = [System.IO.File]::ReadAllBytes($generated)
$secondBytes = [System.IO.File]::ReadAllBytes($generatedRepeat)
if ( -not [System.Linq.Enumerable]::SequenceEqual(
         [byte[]]$firstBytes, [byte[]]$secondBytes) ) {
    throw "XLIR collection-algebra generated C is not byte-for-byte deterministic"
}
$generatedText = [System.IO.File]::ReadAllText(
    $generated, [System.Text.Encoding]::UTF8)
$requiredTokens = @(
    "xrtValueArrayConcat(", "xlir_xvalue_list_merge_right(",
    "xlir_xvalue_list_without_i64_key(", "xlir_xvalue_dict_merge_right(",
    "xlir_xvalue_dict_without_string_key(", "xrtValueSetUnion(",
    "xrtValueSetIntersection(", "xrtValueSetDifference(",
    "xrtValueSetSymmetricDifference(", "xrtValueSetIsSubset(",
    "xrtValueSetIsSuperset(", "xrtValueArrayInsert("
)
foreach ( $token in $requiredTokens ) {
    if ( ! $generatedText.Contains($token) ) {
        throw "XLIR collection-algebra Cgen lost mechanical runtime emission: $token"
    }
}
foreach ( $forbidden in @(
    "xlir_xvalue_binary(", "xl6TypedArrayConcat(",
    "xl6_xrt_typed_list_concat(", "xl6_xrt_typed_dict_concat(",
    "xl6TypedSetUnion(", "xl6_xrt_typed_set_relation("
) ) {
    if ( $generatedText.Contains($forbidden) ) {
        throw "XLIR collection algebra leaked into a generic or legacy Cgen helper: $forbidden"
    }
}

Push-Location $Root
try {
    $command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include $generated $hostSource !XL6_CGEN_LINKS! -o $executable && $executable"
    $runtimeOutput = @(& $env:ComSpec /d /v:on /c $command 2>&1)
    if ( $LASTEXITCODE -ne 0 ) {
        $runtimeOutput | ForEach-Object { Write-Output $_ }
        throw "XLIR collection-algebra strict-C runtime failed"
    }
    $runtimeText = ($runtimeOutput | ForEach-Object { $_.ToString() }) -join "`n"
    if ( ! $runtimeText.Contains("container ops ok") ) {
        throw "XLIR collection-algebra runtime lost its observable completion contract"
    }
} finally {
    Pop-Location
}

Write-Output "XLIR collection-algebra gate ok: 22 explicit array/list/dict/set operations, exact TypeId signatures, typed array insert, byte-for-byte deterministic mechanical C, no generic/legacy fallback, strict C, and runtime semantics."
exit 0
