param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$compiler = Join-Path $Root "release\xl.exe"
$opsSource = Join-Path $Root "test\cases\cgen_protocol_ops.xl"
$complexSource = Join-Path $Root "test\cases\cgen_protocol_complex_receiver.xl"
$genericSource = Join-Path $Root "test\cases\xlir_class_protocol_syntax_generic.xl"
$overloadIndexSource = Join-Path $Root "test\cases\cgen_protocol_overload_index.xl"
$fieldSetterSource = Join-Path $Root "test\cases\cgen_protocol_field_neg_compare.xl"
$virtualPropertySource = Join-Path $Root "release\examples\oop_virtual_dispatch.xl"
$outputDir = Join-Path $Root "build\tmp"
$opsTypedPath = Join-Path $outputDir "xlir_class_protocol_ops.typed.ir"
$opsLoweredPath = Join-Path $outputDir "xlir_class_protocol_ops.lowered.ir"
$opsCPath = Join-Path $outputDir "xlir_class_protocol_ops.c"
$complexTypedPath = Join-Path $outputDir "xlir_class_protocol_complex.typed.ir"
$complexLoweredPath = Join-Path $outputDir "xlir_class_protocol_complex.lowered.ir"
$complexCPath = Join-Path $outputDir "xlir_class_protocol_complex.c"
$genericTypedPath = Join-Path $outputDir "xlir_class_protocol_generic.typed.ir"
$genericLoweredPath = Join-Path $outputDir "xlir_class_protocol_generic.lowered.ir"
$genericCPath = Join-Path $outputDir "xlir_class_protocol_generic.c"
$overloadIndexTypedPath = Join-Path $outputDir "xlir_class_protocol_overload_index.typed.ir"
$overloadIndexLoweredPath = Join-Path $outputDir "xlir_class_protocol_overload_index.lowered.ir"
$overloadIndexCPath = Join-Path $outputDir "xlir_class_protocol_overload_index.c"
$fieldSetterTypedPath = Join-Path $outputDir "xlir_class_protocol_field_setter.typed.ir"
$fieldSetterLoweredPath = Join-Path $outputDir "xlir_class_protocol_field_setter.lowered.ir"
$fieldSetterCPath = Join-Path $outputDir "xlir_class_protocol_field_setter.c"
$fieldSetterCRepeatPath = Join-Path $outputDir "xlir_class_protocol_field_setter.repeat.c"
$virtualPropertyTypedPath = Join-Path $outputDir "xlir_class_virtual_property.typed.ir"
$virtualPropertyLoweredPath = Join-Path $outputDir "xlir_class_virtual_property.lowered.ir"
$virtualPropertyCPath = Join-Path $outputDir "xlir_class_virtual_property.c"

foreach ( $path in @($compiler, $opsSource, $complexSource, $genericSource, $overloadIndexSource, $fieldSetterSource, $virtualPropertySource) ) {
    if ( !(Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR class protocol syntax gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

function Invoke-Xlir([string]$source, [string]$typed, [string]$lowered, [string]$generated) {
    & $compiler ir $source --format text --emit $typed
    if ( $LASTEXITCODE -ne 0 ) { throw "typed XLIR failed for $source" }
    & $compiler ir $source --lowered --format text --emit $lowered
    if ( $LASTEXITCODE -ne 0 ) { throw "lowered XLIR failed for $source" }
    & $compiler cgen $source --backend xlir --emit $generated
    if ( $LASTEXITCODE -ne 0 ) { throw "XLIR Cgen failed for $source" }
}

Push-Location $Root
try {
    Invoke-Xlir $opsSource $opsTypedPath $opsLoweredPath $opsCPath
    Invoke-Xlir $complexSource $complexTypedPath $complexLoweredPath $complexCPath
    Invoke-Xlir $genericSource $genericTypedPath $genericLoweredPath $genericCPath
    Invoke-Xlir $overloadIndexSource $overloadIndexTypedPath $overloadIndexLoweredPath $overloadIndexCPath
    Invoke-Xlir $fieldSetterSource $fieldSetterTypedPath $fieldSetterLoweredPath $fieldSetterCPath
    & $compiler cgen $fieldSetterSource --backend xlir --emit $fieldSetterCRepeatPath
    if ( $LASTEXITCODE -ne 0 ) { throw "repeated XLIR Cgen failed for $fieldSetterSource" }
    Invoke-Xlir $virtualPropertySource $virtualPropertyTypedPath $virtualPropertyLoweredPath $virtualPropertyCPath

    $opsTyped = [System.IO.File]::ReadAllText($opsTypedPath)
    $opsLowered = [System.IO.File]::ReadAllText($opsLoweredPath)
    $complexTyped = [System.IO.File]::ReadAllText($complexTypedPath)
    $complexLowered = [System.IO.File]::ReadAllText($complexLoweredPath)
    $complexC = [System.IO.File]::ReadAllText($complexCPath)
    $genericTyped = [System.IO.File]::ReadAllText($genericTypedPath)
    $genericLowered = [System.IO.File]::ReadAllText($genericLoweredPath)
    $overloadIndexTyped = [System.IO.File]::ReadAllText($overloadIndexTypedPath)
    $overloadIndexLowered = [System.IO.File]::ReadAllText($overloadIndexLoweredPath)
    $fieldSetterTyped = [System.IO.File]::ReadAllText($fieldSetterTypedPath)
    $fieldSetterLowered = [System.IO.File]::ReadAllText($fieldSetterLoweredPath)
    $fieldSetterC = [System.IO.File]::ReadAllText($fieldSetterCPath)
    $fieldSetterCRepeat = [System.IO.File]::ReadAllText($fieldSetterCRepeatPath)
    $virtualPropertyTyped = [System.IO.File]::ReadAllText($virtualPropertyTypedPath)
    $virtualPropertyLowered = [System.IO.File]::ReadAllText($virtualPropertyLoweredPath)

    $opsSites = [regex]::Matches(
        $opsTyped, '(?m)^  call-site .* kind=9 target=[1-9][0-9]* .* receiver=n[1-9][0-9]* ')
    if ( $opsSites.Count -lt 9 ) {
        throw "operator/contains/call syntax did not become resolved protocol CallSites"
    }
    if ( $opsLowered.Contains('runtime=xvalue-contains') ) {
        throw "class membership fell back to dynamic xvalue membership"
    }

    $memberNode = [regex]::Match(
        $complexTyped,
        '(?m)^  n(?<origin>[1-9][0-9]*) kind=member/27 .* target=(?<target>[1-9][0-9]*) .* text=missing @63:19')
    if ( !$memberNode.Success ) {
        throw "__field__ member syntax has no exact semantic target"
    }
    $memberOrigin = $memberNode.Groups['origin'].Value
    $memberTarget = $memberNode.Groups['target'].Value
    $sitePattern = '(?m)^  call-site .* origin=n' + $memberOrigin +
        ' kind=9 target=' + $memberTarget +
        ' .* receiver=n[1-9][0-9]* .* fixed=1 variadic=none'
    if ( ![regex]::IsMatch($complexTyped, $sitePattern) ) {
        throw "__field__ member syntax has no canonical one-argument CallSite"
    }
    $implicitPattern = '(?m)^  call-argument .* source=implicit-string expression=n0 parameter=0 evaluation=0 name=missing$'
    if ( ([regex]::Matches($complexTyped, $implicitPattern)).Count -ne 1 ) {
        throw "__field__ member name is not represented by one implicit-string IR argument"
    }

    $materializePattern = '(?s)runtime-call .* runtime=string-dup .* origin=n' +
        $memberOrigin + ' .* immediate="missing".*?call .* target=' + $memberTarget +
        ' .* origin=n' + $memberOrigin + ' .*?release .* origin=n' + $memberOrigin
    if ( ![regex]::IsMatch($complexLowered, $materializePattern) ) {
        throw "__field__ implicit string lacks materialize/call/release ownership lowering"
    }
    if ( !$complexC.Contains('xrtStrDup("missing")') -or
         !$complexC.Contains('xrtFree(') ) {
        throw "generated C lacks the IR-directed implicit string lifetime"
    }

    $genericMember = [regex]::Match(
        $genericTyped,
        '(?m)^  n(?<origin>[1-9][0-9]*) kind=member/27 .* type=(?<type>[1-9][0-9]*) storage=\k<type> .* target=[1-9][0-9]* .* text=missing @16:12')
    if ( !$genericMember.Success -or
         ![regex]::IsMatch(
             $genericTyped,
             '(?m)^  type t' + $genericMember.Groups['type'].Value +
             ' .* cabi=3 .* name=int ') -or
         !$genericTyped.Contains('source=implicit-string expression=n0 parameter=0 evaluation=0 name=missing') -or
         !$genericLowered.Contains('generic-instance source=') -or
         ![regex]::IsMatch(
             $genericLowered,
             '(?s)source=implicit-string expression=n0 parameter=0 evaluation=0 name=missing.*?runtime-call .* runtime=string-dup .* immediate="missing"') ) {
        throw "generic __field__ CallSite did not preserve concrete result and implicit payload through specialization"
    }

    $oneDimensionalSetter = [regex]::Match(
        $overloadIndexTyped,
        '(?m)^  n(?<origin>[1-9][0-9]*) kind=assign/29 .* target=(?<target>[1-9][0-9]*) .* text== @62:9$')
    $multiDimensionalSetter = [regex]::Match(
        $overloadIndexTyped,
        '(?m)^  n(?<origin>[1-9][0-9]*) kind=assign/29 .* target=(?<target>[1-9][0-9]*) .* text== @65:12$')
    if ( !$oneDimensionalSetter.Success -or !$multiDimensionalSetter.Success ) {
        throw "__setItem__ assignments have no exact semantic targets"
    }
    foreach ( $setter in @(
        @{ Match = $oneDimensionalSetter; Fixed = 2; OperandCount = 3 },
        @{ Match = $multiDimensionalSetter; Fixed = 3; OperandCount = 4 }
    ) ) {
        $origin = $setter.Match.Groups['origin'].Value
        $target = $setter.Match.Groups['target'].Value
        $operands = '\[' + ((1..$setter.OperandCount | ForEach-Object { 'v[0-9]+' }) -join ',') + '\]'
        if ( ![regex]::IsMatch(
                 $overloadIndexTyped,
                 '(?m)^  call-site .* origin=n' + $origin +
                 ' kind=9 target=' + $target +
                 ' result=t0 receiver=n[1-9][0-9]* .* fixed=' +
                 $setter.Fixed + ' variadic=none dispatchKind=0 receiverAt=1$') -or
             ![regex]::IsMatch(
                 $overloadIndexLowered,
                 '(?m)^      i[0-9]+ call .* type=0 target=' + $target +
                 ' .* origin=n' + $origin + ' .* operands=' + $operands) ) {
            throw "__setItem__ assignment did not preserve receiver/index/value CallSite lowering"
        }
    }
    if ( [regex]::IsMatch(
             $overloadIndexTyped,
             '(?m)^  n[1-9][0-9]* kind=index/28 .* target=[1-9][0-9]* .* @(62:1|65:1)$') -or
         $overloadIndexLowered.Contains('runtime=xvalue-array-set-i64-new') ) {
        throw "plain protocol index assignment still executes __item__ or falls back to dynamic array write"
    }

    $fieldSetter = [regex]::Match(
        $fieldSetterTyped,
        '(?m)^  n(?<origin>[1-9][0-9]*) kind=assign/29 .* call=protocol/9 .* target=(?<target>[1-9][0-9]*) .* text== @89:13$')
    if ( !$fieldSetter.Success ) {
        throw "__setField__ assignment has no exact semantic target"
    }
    $fieldOrigin = $fieldSetter.Groups['origin'].Value
    $fieldTarget = $fieldSetter.Groups['target'].Value
    $fieldSite = [regex]::Match(
        $fieldSetterTyped,
        '(?m)^  call-site .* origin=n' + $fieldOrigin +
        ' kind=9 target=' + $fieldTarget +
        ' result=t0 receiver=n[1-9][0-9]* .* arguments=(?<first>[0-9]+)\+2 fixed=2 variadic=none dispatchKind=0 receiverAt=1$')
    if ( !$fieldSite.Success ) {
        throw "__setField__ assignment has no canonical receiver/name/value CallSite"
    }
    $firstArgument = [int]$fieldSite.Groups['first'].Value
    if ( ![regex]::IsMatch(
             $fieldSetterTyped,
             '(?m)^  call-argument a' + $firstArgument +
             ' source=implicit-string expression=n0 parameter=0 evaluation=1 name=extra$') -or
         ![regex]::IsMatch(
             $fieldSetterTyped,
             '(?m)^  call-argument a' + ($firstArgument + 1) +
             ' source=explicit expression=n[1-9][0-9]* parameter=1 evaluation=0 name=$') ) {
        throw "__setField__ CallSite did not preserve implicit member name followed by explicit value"
    }
    if ( [regex]::IsMatch(
             $fieldSetterTyped,
             '(?m)^  n[1-9][0-9]* kind=member/27 .* call=protocol/9 .* text=extra @89:7$') ) {
        throw "plain __setField__ assignment also acquired an __field__ getter plan"
    }
    # Inspect only this operation's instruction lines. Whole-module dot-star
    # patterns both obscure evaluation order and backtrack excessively when a
    # changed renderer no longer has an old textual shape.
    $fieldInstructions = @($fieldSetterLowered -split '\r?\n' | Where-Object {
        $_ -match ('^      i[0-9]+ .* origin=n' + $fieldOrigin + ' ')
    })
    $fieldBox = @($fieldInstructions | Where-Object { $_ -match ' runtime=xvalue-i64 ' })
    $fieldName = @($fieldInstructions | Where-Object { $_ -match ' runtime=string-dup ' })
    $fieldCall = @($fieldInstructions | Where-Object { $_ -match ('^      i[0-9]+ call .* target=' + $fieldTarget + ' ') })
    if ($fieldBox.Count -ne 1 -or $fieldName.Count -ne 1 -or $fieldCall.Count -ne 1) {
        throw '__setField__ requires exactly one RHS box, implicit name and selected call'
    }
    $boxIndex = [Array]::IndexOf($fieldInstructions, $fieldBox[0])
    $nameIndex = [Array]::IndexOf($fieldInstructions, $fieldName[0])
    $callIndex = [Array]::IndexOf($fieldInstructions, $fieldCall[0])
    if ($boxIndex -ge $nameIndex -or $nameIndex -ge $callIndex -or
        $fieldName[0] -notmatch ' immediate="extra"' -or
        $fieldInstructions[$callIndex + 1] -notmatch '^      i[0-9]+ error-test ' -or
        @($fieldInstructions | Where-Object { $_ -match '^      i[0-9]+ release ' }).Count -lt 3 -or
        @($fieldInstructions | Where-Object { $_ -match 'runtime=xvalue-dynamic-field-set-' }).Count -ne 0) {
        throw '__setField__ lost RHS-first evaluation, immediate outcome or explicit owner cleanup'
    }
    $fieldSetterCContract = [regex]::Match(
        $fieldSetterC,
        '(?m)^[ \t]*xlir_result_void (?<carrier>r[0-9]+) = xlir_fn_' + $fieldTarget +
        '\(v[0-9]+, v[0-9]+, v[0-9]+\);\r?\n' +
        '[ \t]*if \(\k<carrier>\.ok\) \{ goto b[0-9]+; \} else goto b[0-9]+;$')
    if ( $fieldSetterC -cne $fieldSetterCRepeat -or !$fieldSetterCContract.Success ) {
        throw "__setField__ generated C is not deterministic mechanical result-carrier CallSite emission"
    }

    $virtualSlots = [regex]::Matches(
        $virtualPropertyTyped, '(?m)^  class-dispatch-slot d[0-9]+ .* name=(score|tag)$')
    $virtualImpls = [regex]::Matches(
        $virtualPropertyTyped, '(?m)^  class-dispatch-impl d[0-9]+ ')
    if ( $virtualSlots.Count -ne 3 -or $virtualImpls.Count -ne 6 -or
         ![regex]::IsMatch(
             $virtualPropertyLowered,
             '(?m)^  class-dispatch-slot d[0-9]+ .* return=t0 error=no-raise physical=none params=\[t[1-9][0-9]*/value\] name=tag$') -or
         ([regex]::Matches($virtualPropertyLowered, '(?m)^      i[0-9]+ virtual-call ')).Count -lt 3 ) {
        throw "virtual func/get/let declarations did not form complete base/override dispatch metadata"
    }

    $command = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include build\tmp\xlir_class_protocol_ops.c test\cases\xlir_default_class_layout_host.c !XL6_CGEN_LINKS! -o build\tmp\xlir_class_protocol_ops.exe && build\tmp\xlir_class_protocol_ops.exe && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include build\tmp\xlir_class_protocol_complex.c test\cases\xlir_default_class_layout_host.c !XL6_CGEN_LINKS! -o build\tmp\xlir_class_protocol_complex.exe && build\tmp\xlir_class_protocol_complex.exe && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include build\tmp\xlir_class_protocol_generic.c test\cases\xlir_default_class_layout_host.c !XL6_CGEN_LINKS! -o build\tmp\xlir_class_protocol_generic.exe && build\tmp\xlir_class_protocol_generic.exe && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include build\tmp\xlir_class_protocol_overload_index.c test\cases\xlir_default_class_layout_host.c !XL6_CGEN_LINKS! -o build\tmp\xlir_class_protocol_overload_index.exe && build\tmp\xlir_class_protocol_overload_index.exe && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include build\tmp\xlir_class_protocol_field_setter.c test\cases\xlir_default_class_layout_host.c !XL6_CGEN_LINKS! -o build\tmp\xlir_class_protocol_field_setter.exe && build\tmp\xlir_class_protocol_field_setter.exe && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include build\tmp\xlir_class_virtual_property.c test\cases\xlir_default_class_layout_host.c !XL6_CGEN_LINKS! -o build\tmp\xlir_class_virtual_property.exe && build\tmp\xlir_class_virtual_property.exe'
    & cmd.exe /d /v:on /c $command
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR class protocol syntax generated-C runtime failed"
    }
} finally {
    Pop-Location
}

Write-Output "XLIR class protocol getter/setter syntax and virtual property dispatch CallSite/runtime gate ok."
