param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}
$Root = (Resolve-Path -LiteralPath $Root).Path

function Invoke-Xl([string[]]$Arguments, [string]$Failure)
{
    & $xl @Arguments
    if ( $LASTEXITCODE -ne 0 ) { throw $Failure }
}

function Assert-Deterministic([string]$First, [string]$Second, [string]$Label)
{
    if ( ![System.Linq.Enumerable]::SequenceEqual(
            [byte[]][IO.File]::ReadAllBytes($First),
            [byte[]][IO.File]::ReadAllBytes($Second)) ) {
        throw "XLIR char/bytes $Label is not deterministic"
    }
}

function Get-CanonicalType($Module, [string]$Name)
{
    $matches = @($Module.types | Where-Object {
        [int]$_.kind -eq 1 -and $_.name -eq $Name
    })
    if ( $matches.Count -ne 1 ) {
        throw "XLIR char/bytes has no unique canonical type: $Name"
    }
    return $matches[0]
}

function Assert-CheckedRuntimeOperations($Module, [string[]]$Names)
{
    $instructions = @($Module.instructions)
    foreach ( $name in $Names ) {
        $calls = @($instructions | Where-Object {
            $_.kindName -eq "runtime-call" -and $_.runtimeOpName -eq $name
        })
        if ( $calls.Count -eq 0 ) {
            throw "Lowered XLIR char/bytes fixture lost runtime operation: $name"
        }
        foreach ( $call in $calls ) {
            $tests = @($instructions | Where-Object {
                [int]$_.index -eq ([int]$call.index + 1) -and
                $_.kindName -eq "error-test" -and
                @($_.operands).Count -eq 1 -and
                [int]$_.operands[0] -eq [int]$call.result
            })
            if ( $tests.Count -ne 1 ) {
                throw "Runtime operation has no adjacent explicit failure edge: $name at i$($call.index)"
            }
        }
    }
}

function Assert-NegativeCase(
    [string]$Path,
    [string]$ExpectedText,
    [string]$Label)
{
    $savedPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        $output = (& $xl check $Path 2>&1 | Out-String)
        $exitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $savedPreference
    }
    if ( $exitCode -eq 0 ) {
        throw "Invalid char/bytes spelling was accepted: $Label"
    }
    if ( $ExpectedText -ne "" -and !$output.Contains($ExpectedText) ) {
        throw "Invalid char/bytes spelling lost its diagnostic contract: $Label`n$output"
    }
}

$xl = Join-Path $Root "release\xl.exe"
$outputDir = Join-Path $Root "build\test"
$hostSource = Join-Path $Root "test\cases\xlir_default_class_layout_host.c"
$sources = @(
    [pscustomobject]@{
        Name = "char_bytes_literals"
        Path = Join-Path $Root "test\cases\char_bytes_literals.xl"
    },
    [pscustomobject]@{
        Name = "char_bytes_advanced"
        Path = Join-Path $Root "test\cases\char_bytes_advanced.xl"
    }
)
$negativeCases = @(
    [pscustomobject]@{
        Path = Join-Path $Root "test\cases\char_bytes_invalid_char_width.xl"
        Text = "[parse]"
        Label = "multi-scalar char"
    },
    [pscustomobject]@{
        Path = Join-Path $Root "test\cases\char_bytes_invalid_char_surrogate.xl"
        Text = "[parse]"
        Label = "surrogate char"
    },
    [pscustomobject]@{
        Path = Join-Path $Root "test\cases\char_bytes_invalid_bf_prefix.xl"
        Text = "[parse]"
        Label = "formatted bytes prefix"
    },
    [pscustomobject]@{
        Path = Join-Path $Root "test\cases\char_bytes_invalid_brf_prefix.xl"
        Text = "[parse]"
        Label = "raw formatted bytes prefix"
    }
)

foreach ( $path in @($xl, $hostSource) + @($sources.Path) + @($negativeCases.Path) ) {
    if ( !(Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR char/bytes gate input is missing: $path"
    }
}
$literalFixtureText = [IO.File]::ReadAllText(
    $sources[0].Path, [Text.Encoding]::UTF8)
foreach ( $required in @(
    "formatted strings must decode ordinary escapes",
    "rf strings must preserve ordinary escapes",
    "formatted backticks must preserve ordinary escapes",
    "formatted triple strings must use common-indent framing"
) ) {
    if ( !$literalFixtureText.Contains($required) ) {
        throw "XLIR char/bytes fixture lost string-form semantics: $required"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

foreach ( $source in $sources ) {
    $source | Add-Member NoteProperty Typed (Join-Path $outputDir "$($source.Name).char_bytes.typed.json")
    $source | Add-Member NoteProperty TypedRepeat (Join-Path $outputDir "$($source.Name).char_bytes.typed.repeat.json")
    $source | Add-Member NoteProperty Lowered (Join-Path $outputDir "$($source.Name).char_bytes.lowered.json")
    $source | Add-Member NoteProperty LoweredRepeat (Join-Path $outputDir "$($source.Name).char_bytes.lowered.repeat.json")
    $source | Add-Member NoteProperty C (Join-Path $outputDir "$($source.Name).char_bytes.c")
    $source | Add-Member NoteProperty CRepeat (Join-Path $outputDir "$($source.Name).char_bytes.repeat.c")
    $source | Add-Member NoteProperty Exe (Join-Path $outputDir "$($source.Name).char_bytes.strict.exe")

    Invoke-Xl @("ir", $source.Path, "--typed", "--format", "json", "--emit", $source.Typed) `
        "Typed XLIR failed: $($source.Name)"
    Invoke-Xl @("ir", $source.Path, "--typed", "--format", "json", "--emit", $source.TypedRepeat) `
        "Repeated typed XLIR failed: $($source.Name)"
    Invoke-Xl @("ir", $source.Path, "--lowered", "--format", "json", "--emit", $source.Lowered) `
        "Lowered XLIR failed: $($source.Name)"
    Invoke-Xl @("ir", $source.Path, "--lowered", "--format", "json", "--emit", $source.LoweredRepeat) `
        "Repeated lowered XLIR failed: $($source.Name)"
    Invoke-Xl @("cgen", $source.Path, "--backend", "xlir", "--emit", $source.C) `
        "XLIR Cgen failed: $($source.Name)"
    Invoke-Xl @("cgen", $source.Path, "--backend", "xlir", "--emit", $source.CRepeat) `
        "Repeated XLIR Cgen failed: $($source.Name)"

    Assert-Deterministic $source.Typed $source.TypedRepeat "$($source.Name) typed graph"
    Assert-Deterministic $source.Lowered $source.LoweredRepeat "$($source.Name) lowered graph"
    Assert-Deterministic $source.C $source.CRepeat "$($source.Name) generated C"
}

$typed = [IO.File]::ReadAllText($sources[0].Typed, [Text.Encoding]::UTF8) | ConvertFrom-Json
$lowered = [IO.File]::ReadAllText($sources[0].Lowered, [Text.Encoding]::UTF8) | ConvertFrom-Json
$advancedTyped = [IO.File]::ReadAllText($sources[1].Typed, [Text.Encoding]::UTF8) | ConvertFrom-Json
$advancedLowered = [IO.File]::ReadAllText($sources[1].Lowered, [Text.Encoding]::UTF8) | ConvertFrom-Json

$char = Get-CanonicalType $typed "char"
$bytes = Get-CanonicalType $typed "bytes"
$string = Get-CanonicalType $typed "string"
$u8 = Get-CanonicalType $typed "uint8"
if ( [int]$char.cAbi -ne 43 -or [int]$char.ownershipAbi -ne 1 -or
     [int]$char.nullAbi -ne 0 -or $char.integerTargetName -ne "unicode-scalar" -or
     $char.errorResultCName -ne "xlir_result_char" ) {
    throw "Typed XLIR char is not a trivial uint32 Unicode-scalar ABI"
}
if ( [int]$bytes.cAbi -ne 44 -or [int]$bytes.ownershipAbi -ne 2 -or
     [int]$bytes.nullAbi -ne 2 -or $bytes.errorResultCName -ne "xlir_result_xvalue" ) {
    throw "Typed XLIR bytes is not the closed managed XValue-backed ABI"
}
if ( [int]$string.cAbi -ne 25 -or [int]$string.ownershipAbi -ne 2 -or
     $u8.integerTargetName -ne "u8" -or [int]$u8.ownershipAbi -ne 1 ) {
    throw "Typed XLIR string/u8 companion ABI is not canonical"
}

$charLiterals = @($typed.nodes | Where-Object {
    $_.kindName -eq "literal" -and $_.sourceKindName -eq "char"
})
$bytesLiterals = @($typed.nodes | Where-Object {
    $_.kindName -eq "literal" -and $_.sourceKindName -eq "bytes"
})
if ( $charLiterals.Count -lt 4 -or
     @($charLiterals | Where-Object { [int]$_.storageType -ne [int]$char.id }).Count -ne 0 -or
     $bytesLiterals.Count -lt 8 -or
     @($bytesLiterals | Where-Object { [int]$_.storageType -ne [int]$bytes.id }).Count -ne 0 ) {
    throw "Typed XLIR literals lost their exact char/bytes TypeId payloads"
}

$stringWrites = @($typed.nodes | Where-Object {
    $_.indexWriteAbiName -eq "string-scalar-set-copy"
})
$bytesWrites = @($typed.nodes | Where-Object {
    $_.indexWriteAbiName -eq "bytes-set-copy"
})
if ( $stringWrites.Count -lt 2 -or $bytesWrites.Count -lt 2 -or
     @($typed.nodes | Where-Object {
         $_.indexWriteAbiName -ne "none" -and
         $_.indexWriteAbiName -notin @("string-scalar-set-copy", "bytes-set-copy")
     }).Count -ne 0 ) {
    throw "Typed XLIR mutable string/bytes writes do not use copy-and-commit ABIs"
}

$stringLoops = @($typed.nodes | Where-Object {
    $_.loopAbiName -eq "string-utf8-value"
})
$bytesLoops = @($typed.nodes | Where-Object {
    $_.loopAbiName -eq "bytes-index-value"
})
if ( $stringLoops.Count -ne 1 -or $bytesLoops.Count -ne 1 -or
     $stringLoops[0].loopItemAbiName -ne "char" -or
     $stringLoops[0].loopItemLifetimeAbiName -ne "trivial-replace" -or
     $stringLoops[0].loopResourceAbiName -ne "string-snapshot-release" -or
     $bytesLoops[0].loopKeyAbiName -ne "ordinal-i64" -or
     $bytesLoops[0].loopItemAbiName -ne "u8" -or
     $bytesLoops[0].loopItemLifetimeAbiName -ne "trivial-replace" -or
     $bytesLoops[0].loopResourceAbiName -ne "bytes-snapshot-release" ) {
    throw "Typed XLIR string/bytes iteration ABI is incomplete or ambiguous"
}

foreach ( $operation in @(
    "bytes-at", "bytes-length", "bytes-retain", "bytes-from-data",
    "bytes-concat", "bytes-equal", "bytes-set-copy",
    "string-scalar-at", "string-scalar-set-copy",
    "string-concat-char-left", "string-concat-char-right",
    "string-from-utf8",
    "integer-convert-checked"
) ) {
    if ( @($lowered.instructions | Where-Object {
            $_.runtimeOpName -eq $operation
         }).Count -eq 0 ) {
        throw "Lowered XLIR char/bytes operation is missing: $operation"
    }
}
Assert-CheckedRuntimeOperations $lowered @(
    "bytes-from-data", "bytes-concat", "bytes-set-copy",
    "string-scalar-at", "string-scalar-set-copy",
    "string-concat-char-left", "string-concat-char-right",
    "string-from-utf8",
    "integer-convert-checked"
)
if ( @($lowered.instructions | Where-Object {
        $_.kindName -eq "release" -and [int]$_.type -eq [int]$bytes.id
     }).Count -lt 8 ) {
    throw "Lowered XLIR bytes values have no explicit managed release coverage"
}

$advancedChar = Get-CanonicalType $advancedTyped "char"
$advancedBytes = Get-CanonicalType $advancedTyped "bytes"
foreach ( $shape in @(
    [pscustomobject]@{ Name = "future<char>"; Kind = 6; Param = $advancedChar.id },
    [pscustomobject]@{ Name = "future<bytes>"; Kind = 6; Param = $advancedBytes.id },
    [pscustomobject]@{ Name = "generator<char>"; Kind = 6; Param = $advancedChar.id },
    [pscustomobject]@{ Name = "generator<bytes>"; Kind = 6; Param = $advancedBytes.id }
) ) {
    $matches = @($advancedTyped.types | Where-Object {
        [int]$_.kind -eq $shape.Kind -and $_.displayName -eq $shape.Name -and
        @($_.params).Count -eq 1 -and [int]$_.params[0] -eq [int]$shape.Param
    })
    if ( $matches.Count -lt 1 ) {
        throw "Typed XLIR lost callable continuation shape: $($shape.Name)"
    }
}
if ( @($advancedLowered.instructions | Where-Object {
        $_.runtimeOpName -eq "xvalue-char"
     }).Count -lt 2 -or
     @($advancedLowered.instructions | Where-Object {
        $_.runtimeOpName -eq "persistence-xlon-stringify"
     }).Count -lt 2 ) {
    throw "Lowered XLIR lost XValue char/bytes boxing or XLON persistence coverage"
}
Assert-CheckedRuntimeOperations $advancedLowered @(
    "xvalue-char", "bytes-from-data", "bytes-concat",
    "string-concat-char-right"
)

$generated = [IO.File]::ReadAllText($sources[0].C, [Text.Encoding]::UTF8)
$advancedGenerated = [IO.File]::ReadAllText($sources[1].C, [Text.Encoding]::UTF8)
foreach ( $required in @(
    "typedef struct xlir_result_char { bool ok; uint32_t value; } xlir_result_char;",
    "static uint32_t xlir_i64_to_char_checked(int64_t value)",
    "static bool xlir_string_scalar_at(xlir_string text, int64_t index, uint32_t* scalar)",
    "static xvalue* xlir_bytes_concat(const xvalue* left, const xvalue* right)",
    "static xvalue* xlir_bytes_set_copy(const xvalue* value, int64_t index, int64_t item)"
) ) {
    if ( !$generated.Contains($required) ) {
        throw "Generated C lost a mechanical char/bytes ABI renderer: $required"
    }
}
if ( !$advancedGenerated.Contains("xrtValueChar(") -or
     !$advancedGenerated.Contains("xl6PayloadResolveOwned") -or
     !$advancedGenerated.Contains("xlir_generator_item_get") ) {
    throw "Generated C lost char/bytes XValue, Future or generator transport"
}
foreach ( $forbidden in @("XL6_HIR", "xl6Hir", "sourceKind", "indexWriteAbi", "loopItemAbi") ) {
    if ( $generated.Contains($forbidden) -or $advancedGenerated.Contains($forbidden) ) {
        throw "Generated char/bytes C leaked semantic metadata or legacy IR: $forbidden"
    }
}

foreach ( $case in $negativeCases ) {
    Assert-NegativeCase $case.Path $case.Text $case.Label
}

Push-Location $Root
try {
    $baseC = $sources[0].C
    $baseExe = $sources[0].Exe
    $advancedC = $sources[1].C
    $advancedExe = $sources[1].Exe
    $command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include `"$baseC`" `"$hostSource`" !XL6_CGEN_LINKS! -o `"$baseExe`" && `"$baseExe`" && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include `"$advancedC`" `"$hostSource`" !XL6_CGEN_LINKS! -o `"$advancedExe`" && `"$advancedExe`""
    & $env:ComSpec /d /v:on /c $command
    if ( $LASTEXITCODE -ne 0 ) {
        throw "Char/bytes strict-C compile or runtime semantics failed"
    }
} finally {
    Pop-Location
}

Write-Output "XLIR char/bytes ABI gate passed: Unicode-scalar char, checked integer conversion, mutable copy-and-commit string/bytes indexing, COW aliases, raw/triple/NUL bytes, scalar/octet iteration, XValue/XLON, closure/Future/generator transport, rejected spellings, deterministic IR/C and strict runtime."
exit 0
