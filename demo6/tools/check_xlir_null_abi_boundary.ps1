param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_null_abi_boundary.xl"
$hostSource = Join-Path $Root "test\cases\xlir_null_abi_boundary_host.c"
$scalarSource = Join-Path $Root "test\cases\xlir_nullable_scalar.xl"
$scalarHostSource = Join-Path $Root "test\cases\xlir_nullable_scalar_host.c"
$cellSource = Join-Path $Root "test\cases\xlir_nullable_cell_capture.xl"
$cellHostSource = Join-Path $Root "test\cases\xlir_nullable_cell_capture_host.c"
$closureSource = Join-Path $Root "test\cases\xlir_nullable_closure_capture.xl"
$closureHostSource = Join-Path $Root "test\cases\xlir_nullable_closure_capture_host.c"
$asyncSource = Join-Path $Root "test\cases\xlir_nullable_async.xl"
$asyncHostSource = Join-Path $Root "test\cases\xlir_nullable_async_host.c"
$continuationSource = Join-Path $Root "test\cases\xlir_nullable_async_continuation.xl"
$continuationHostSource = Join-Path $Root "test\cases\xlir_nullable_async_continuation_host.c"
$classFieldSource = Join-Path $Root "test\cases\xlir_nullable_class_field.xl"
$classFieldHostSource = Join-Path $Root "test\cases\xlir_nullable_class_field_host.c"
$sentinelCoalesceSource = Join-Path $Root "test\cases\xlir_nullable_sentinel_coalesce.xl"
$sentinelCoalesceHostSource = Join-Path $Root "test\cases\xlir_nullable_sentinel_coalesce_host.c"
$sentinelAssignSource = Join-Path $Root "test\cases\xlir_nullable_sentinel_assign_if_null.xl"
$sentinelAssignHostSource = Join-Path $Root "test\cases\xlir_nullable_sentinel_assign_if_null_host.c"
$indexAssignSource = Join-Path $Root "test\cases\xlir_nullable_index_assign_if_null.xl"
$indexAssignHostSource = Join-Path $Root "test\cases\xlir_nullable_index_assign_if_null_host.c"
$indexAssignMayRaiseSource = Join-Path $Root "test\cases\xlir_nullable_index_assign_if_null_may_raise.xl"
$outputDir = Join-Path $Root "build\test"
$irPath = Join-Path $outputDir "xlir_null_abi_boundary.lowered.xlir"
$jsonPath = Join-Path $outputDir "xlir_null_abi_boundary.lowered.json"
$cPath = Join-Path $outputDir "xlir_null_abi_boundary_generated.c"
$headerPath = Join-Path $outputDir "xlir_null_abi_boundary_generated.h"
$exePath = Join-Path $outputDir "xlir_null_abi_boundary_host.exe"
$scalarIrPath = Join-Path $outputDir "xlir_nullable_scalar.lowered.xlir"
$scalarJsonPath = Join-Path $outputDir "xlir_nullable_scalar.lowered.json"
$scalarCPath = Join-Path $outputDir "xlir_nullable_scalar_generated.c"
$scalarHeaderPath = Join-Path $outputDir "xlir_nullable_scalar_generated.h"
$scalarExePath = Join-Path $outputDir "xlir_nullable_scalar_host.exe"
$cellIrPath = Join-Path $outputDir "xlir_nullable_cell_capture.lowered.xlir"
$cellJsonPath = Join-Path $outputDir "xlir_nullable_cell_capture.lowered.json"
$cellCPath = Join-Path $outputDir "xlir_nullable_cell_capture_generated.c"
$cellExePath = Join-Path $outputDir "xlir_nullable_cell_capture_host.exe"
$closureIrPath = Join-Path $outputDir "xlir_nullable_closure_capture.lowered.xlir"
$closureJsonPath = Join-Path $outputDir "xlir_nullable_closure_capture.lowered.json"
$closureCPath = Join-Path $outputDir "xlir_nullable_closure_capture_generated.c"
$closureExePath = Join-Path $outputDir "xlir_nullable_closure_capture_host.exe"
$asyncIrPath = Join-Path $outputDir "xlir_nullable_async.lowered.xlir"
$asyncJsonPath = Join-Path $outputDir "xlir_nullable_async.lowered.json"
$asyncCPath = Join-Path $outputDir "xlir_nullable_async_generated.c"
$asyncExePath = Join-Path $outputDir "xlir_nullable_async_host.exe"
$continuationIrPath = Join-Path $outputDir "xlir_nullable_async_continuation.lowered.xlir"
$continuationJsonPath = Join-Path $outputDir "xlir_nullable_async_continuation.lowered.json"
$continuationCPath = Join-Path $outputDir "xlir_nullable_async_continuation_generated.c"
$continuationExePath = Join-Path $outputDir "xlir_nullable_async_continuation_host.exe"
$classFieldIrPath = Join-Path $outputDir "xlir_nullable_class_field.lowered.xlir"
$classFieldJsonPath = Join-Path $outputDir "xlir_nullable_class_field.lowered.json"
$classFieldCPath = Join-Path $outputDir "xlir_nullable_class_field_generated.c"
$classFieldExePath = Join-Path $outputDir "xlir_nullable_class_field_host.exe"
$sentinelCoalesceIrPath = Join-Path $outputDir "xlir_nullable_sentinel_coalesce.lowered.xlir"
$sentinelCoalesceJsonPath = Join-Path $outputDir "xlir_nullable_sentinel_coalesce.lowered.json"
$sentinelCoalesceCPath = Join-Path $outputDir "xlir_nullable_sentinel_coalesce_generated.c"
$sentinelCoalesceExePath = Join-Path $outputDir "xlir_nullable_sentinel_coalesce_host.exe"
$sentinelAssignIrPath = Join-Path $outputDir "xlir_nullable_sentinel_assign_if_null.lowered.xlir"
$sentinelAssignJsonPath = Join-Path $outputDir "xlir_nullable_sentinel_assign_if_null.lowered.json"
$sentinelAssignCPath = Join-Path $outputDir "xlir_nullable_sentinel_assign_if_null_generated.c"
$sentinelAssignExePath = Join-Path $outputDir "xlir_nullable_sentinel_assign_if_null_host.exe"

foreach ( $path in @($xl, $source, $hostSource, $scalarSource, $scalarHostSource,
                      $cellSource, $cellHostSource, $closureSource, $closureHostSource,
                      $asyncSource, $asyncHostSource,
                      $continuationSource, $continuationHostSource,
                      $classFieldSource, $classFieldHostSource,
                      $sentinelCoalesceSource, $sentinelCoalesceHostSource,
                      $sentinelAssignSource, $sentinelAssignHostSource,
                      $indexAssignSource, $indexAssignHostSource,
                      $indexAssignMayRaiseSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR contextual-null gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

& $xl ir $source --lowered --format text --emit $irPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR contextual-null text IR failed" }
$text = [System.IO.File]::ReadAllText($irPath, [System.Text.Encoding]::UTF8)
$classMatch = [regex]::Match(
        $text, 'type t(?<id>[0-9]+) kind=2 collection=none resource=none persistence=class-schema/t0 cabi=26 callable=invalid ownership=2 null=2 nominal=s[1-9][0-9]* typeValue=0x[0-9A-F]{16} name=NullToken display=NullToken integer=none params=\[\] error-result=xlir_result_xvalue')
if ( ! $classMatch.Success ) {
    throw "XLIR class type has no xvalue-sentinel null representation"
}
$classId = $classMatch.Groups['id'].Value
$nullableMatch = [regex]::Match(
            $text, "type t(?<id>[0-9]+) kind=8 collection=none resource=none persistence=nullable/t$classId cabi=26 callable=invalid ownership=2 null=2 nominal=s0 typeValue=0x[0-9A-F]{16} name=nullable display=nullable<NullToken> integer=none params=\[t$classId\] error-result=xlir_result_xvalue")
if ( ! $nullableMatch.Success ) {
    throw "XLIR nullable class does not preserve its element xvalue-sentinel ABI"
}
$nullableId = $nullableMatch.Groups['id'].Value
    if ( $text -notmatch 'type t[0-9]+ kind=1 collection=none resource=none persistence=none/t0 cabi=28 callable=invalid ownership=1 null=1 nominal=s0 typeValue=0x[0-9A-F]{16} name=point display=point integer=none params=\[\] error-result=xlir_result_pointer' ) {
    throw "XLIR raw pointer type has no pointer-sentinel null ABI"
}
$classWrap = [regex]::Match(
    $text,
    "\bnullable-wrap opcode=none runtime=none v(?<result>[0-9]+) type=$nullableId .*operands=\[v(?<source>[0-9]+)\]")
if ( ([regex]::Matches($text, '\bnull opcode=none runtime=none')).Count -lt 4 -or
     ([regex]::Matches($text, '\bnull-test opcode=none runtime=none')).Count -lt 5 -or
     ! $classWrap.Success ) {
    throw "XLIR contextual null construction/test or class-to-nullable wrap is missing"
}
if ( $text.Contains("immediate=NULL") ) {
    throw "XLIR null regressed to an untyped CONST payload"
}

& $xl ir $source --lowered --format json --emit $jsonPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR contextual-null JSON IR failed" }
$json = [System.IO.File]::ReadAllText($jsonPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$classType = $json.types | Where-Object { $_.id -eq [int]$classId }
$nullableType = $json.types | Where-Object { $_.id -eq [int]$nullableId }
$pointerType = $json.types | Where-Object { $_.cAbi -eq 28 -and $_.nullAbi -eq 1 }
if ( $classType.nullAbi -ne 2 -or $nullableType.kind -ne 8 -or
     $nullableType.cAbi -ne $classType.cAbi -or
     $nullableType.ownershipAbi -ne $classType.ownershipAbi -or
     $nullableType.nullAbi -ne $classType.nullAbi -or
     @($nullableType.params).Count -ne 1 -or @($nullableType.params)[0] -ne [int]$classId -or
     @($pointerType).Count -lt 1 ) {
    throw "XLIR JSON type table has no exact contextual null representation ABI"
}
if ( @($json.instructions | Where-Object { $_.kindName -eq 'null' }).Count -lt 4 -or
     @($json.instructions | Where-Object { $_.kindName -eq 'null-test' }).Count -lt 5 -or
     @($json.instructions | Where-Object {
         $_.kindName -eq 'nullable-wrap' -and $_.type -eq [int]$nullableId
     }).Count -lt 1 ) {
    throw "XLIR JSON instruction table omits contextual null facts"
}

& $xl cgen $source --backend xlir --emit $cPath --emit-header $headerPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR contextual-null Cgen failed" }
$generated = [System.IO.File]::ReadAllText($cPath, [System.Text.Encoding]::UTF8)
$header = [System.IO.File]::ReadAllText($headerPath, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "xrtValueRetain(xrtValueNull())",
    "== NULL || xrtValueType(",
    "int64_t xlir_null_abi_probe(void)"
) ) {
    if ( ! $generated.Contains($required) ) {
        throw "XLIR contextual-null C emitter ignored a verified representation fact: $required"
    }
}
$classWrapC = "v$($classWrap.Groups['result'].Value) = v$($classWrap.Groups['source'].Value);"
if ( ! $generated.Contains($classWrapC) ) {
    throw "XLIR contextual-null C emitter omitted the verified class-to-nullable wrap: $classWrapC"
}
if ( $generated -notmatch '= \(v[0-9]+ == NULL\);' ) {
    throw "XLIR contextual-null C emitter omitted a verified raw-pointer null test"
}
if ( ! $header.Contains("int64_t xlir_null_abi_probe(void);") ) {
    throw "XLIR contextual-null export header is missing"
}

$command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror $hostSource !XL6_CGEN_LINKS! -o $exePath && $exePath"
& $env:ComSpec /d /v:on /c $command
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR contextual-null generated-C runtime failed" }

& $xl ir $scalarSource --lowered --format text --emit $scalarIrPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR tagged nullable scalar text IR failed" }
$scalarText = [System.IO.File]::ReadAllText($scalarIrPath, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "cabi=36 callable=invalid ownership=1 null=3",
    "error-result=xlir_result_t",
    "nullable-wrap opcode=none runtime=none",
    "nullable-unwrap opcode=none runtime=none",
    "null-test opcode=none runtime=none"
) ) {
    if ( ! $scalarText.Contains($required) ) {
        throw "XLIR tagged nullable scalar ABI is incomplete: $required"
    }
}
if ( ([regex]::Matches($scalarText, 'kind=8 collection=none resource=none persistence=nullable/t[0-9]+ cabi=36 callable=invalid ownership=1 null=3')).Count -lt 4 -or
     ([regex]::Matches($scalarText, '\bnullable-wrap opcode=none')).Count -lt 4 -or
     ([regex]::Matches($scalarText, '\bnullable-unwrap opcode=none')).Count -lt 4 -or
     $scalarText.Contains("binary opcode=null-coalesce") ) {
    throw "XLIR tagged nullable scalar types or lazy coalesce CFG are incomplete"
}

& $xl ir $scalarSource --lowered --format json --emit $scalarJsonPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR tagged nullable scalar JSON IR failed" }
$scalarJson = [System.IO.File]::ReadAllText($scalarJsonPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$taggedTypes = @($scalarJson.types | Where-Object {
    $_.kind -eq 8 -and $_.cAbi -eq 36 -and $_.ownershipAbi -eq 1 -and $_.nullAbi -eq 3
})
if ( $taggedTypes.Count -lt 4 -or
     @($taggedTypes | Where-Object { $_.errorResultCName -match '^xlir_result_t[0-9]+$' }).Count -ne $taggedTypes.Count -or
     @($scalarJson.instructions | Where-Object { $_.kindName -eq 'nullable-wrap' }).Count -lt 4 -or
     @($scalarJson.instructions | Where-Object { $_.kindName -eq 'nullable-unwrap' }).Count -lt 4 ) {
    throw "XLIR tagged nullable scalar JSON omits type or instruction ABI facts"
}

& $xl cgen $scalarSource --backend xlir --emit $scalarCPath --emit-header $scalarHeaderPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR tagged nullable scalar Cgen failed" }
$scalarGenerated = [System.IO.File]::ReadAllText($scalarCPath, [System.Text.Encoding]::UTF8)
$scalarHeader = [System.IO.File]::ReadAllText($scalarHeaderPath, [System.Text.Encoding]::UTF8)
foreach ( $pattern in @(
    'typedef struct xlir_nullable_t[0-9]+ \{ bool hasValue; int64_t value; \} xlir_nullable_t[0-9]+;',
    'typedef struct xlir_nullable_t[0-9]+ \{ bool hasValue; uint64_t value; \} xlir_nullable_t[0-9]+;',
    'typedef struct xlir_nullable_t[0-9]+ \{ bool hasValue; double value; \} xlir_nullable_t[0-9]+;',
    'typedef struct xlir_nullable_t[0-9]+ \{ bool hasValue; bool value; \} xlir_nullable_t[0-9]+;',
    'typedef struct xlir_result_t[0-9]+ \{ bool ok; xlir_nullable_t[0-9]+ value; \} xlir_result_t[0-9]+;',
    '= \(xlir_nullable_t[0-9]+\)\{ false \};',
    '= \(xlir_nullable_t[0-9]+\)\{ true, v[0-9]+ \};',
    '= !v[0-9]+\.hasValue;',
    '= v[0-9]+\.value;'
) ) {
    if ( $scalarGenerated -notmatch $pattern ) {
        throw "XLIR tagged nullable scalar C ABI is incomplete: $pattern"
    }
}
foreach ( $required in @(
    "int64_t xlir_nullable_scalar_probe(bool present);",
    "uint64_t xlir_nullable_scalar_u64(bool present);",
    "double xlir_nullable_scalar_float(bool present);",
    "bool xlir_nullable_scalar_bool(bool present);",
    "xlir_nullable_t4 xlir_nullable_scalar_echo(xlir_nullable_t4 value);",
    "xlir_nullable_t4 xlir_nullable_scalar_checked_echo(xlir_nullable_t4 value, bool accept);"
) ) {
    if ( ! $scalarHeader.Contains($required) ) {
        throw "XLIR tagged nullable scalar public boundary is incomplete: $required"
    }
}
$scalarCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror $scalarHostSource !XL6_CGEN_LINKS! -o $scalarExePath && $scalarExePath"
& $env:ComSpec /d /v:on /c $scalarCommand
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR tagged nullable scalar generated-C runtime failed" }

& $xl ir $cellSource --lowered --format text --emit $cellIrPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR tagged nullable mutable-capture text IR failed" }
$cellText = [System.IO.File]::ReadAllText($cellIrPath, [System.Text.Encoding]::UTF8)
foreach ( $required in @("cabi=36 callable=invalid ownership=1 null=3", "abi=retain-cell", "cell-new",
                          "cell-load", "cell-store", "cell-retain", "cell-release") ) {
    if ( ! $cellText.Contains($required) ) {
        throw "XLIR tagged nullable mutable-capture ABI is incomplete: $required"
    }
}

& $xl ir $cellSource --lowered --format json --emit $cellJsonPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR tagged nullable mutable-capture JSON IR failed" }
$cellJson = [System.IO.File]::ReadAllText($cellJsonPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$cellCapture = @($cellJson.closureCaptures | Where-Object {
    $_.abiName -eq "retain-cell" -and $_.cellType -ne 0
})
if ( $cellCapture.Count -ne 1 -or
     @($cellJson.instructions | Where-Object { $_.kindName -eq "cell-new" }).Count -lt 1 -or
     @($cellJson.instructions | Where-Object { $_.kindName -eq "cell-store" }).Count -lt 2 ) {
    throw "XLIR tagged nullable mutable-capture JSON omits its exact cell ABI"
}

& $xl cgen $cellSource --backend xlir --emit $cellCPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR tagged nullable mutable-capture Cgen failed" }
$cellGenerated = [System.IO.File]::ReadAllText($cellCPath, [System.Text.Encoding]::UTF8)
foreach ( $pattern in @(
    'union \{[^}]*void\* tagged;',
    'static xlir_cell\* xlir_cell_new_xlir_nullable_t[0-9]+\(void\)',
    'static xlir_nullable_t[0-9]+ xlir_cell_load_xlir_nullable_t[0-9]+\(const xlir_cell\* cell\)',
    'static void xlir_cell_store_xlir_nullable_t[0-9]+\(xlir_cell\* cell, xlir_nullable_t[0-9]+ value\)',
    'cell->kind == 6\) xrtFree\(cell->value\.tagged\)'
) ) {
    if ( $cellGenerated -notmatch $pattern ) {
        throw "XLIR tagged nullable mutable-capture C ABI is incomplete: $pattern"
    }
}
$cellCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror $cellHostSource !XL6_CGEN_LINKS! -o $cellExePath && $cellExePath"
& $env:ComSpec /d /v:on /c $cellCommand
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR tagged nullable mutable-capture generated-C runtime failed" }

& $xl ir $closureSource --lowered --format text --emit $closureIrPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR tagged nullable closure text IR failed" }
$closureText = [System.IO.File]::ReadAllText($closureIrPath, [System.Text.Encoding]::UTF8)
foreach ( $required in @("closure-capture", "abi=copy-value", "closure-make", "closure-call") ) {
    if ( ! $closureText.Contains($required) ) {
        throw "XLIR tagged nullable closure ABI is incomplete: $required"
    }
}

& $xl ir $closureSource --lowered --format json --emit $closureJsonPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR tagged nullable closure JSON IR failed" }
$closureJson = [System.IO.File]::ReadAllText($closureJsonPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$closureCapture = @($closureJson.closureCaptures | Where-Object { $_.abiName -eq "copy-value" })
$closureCall = @($closureJson.instructions | Where-Object { $_.kindName -eq "closure-call" })
if ( $closureCapture.Count -ne 1 -or $closureCall.Count -ne 1 ) {
    throw "XLIR tagged nullable closure JSON omits its exact capture or result ABI"
}
$closureCaptureType = @($closureJson.types | Where-Object { $_.id -eq $closureCapture[0].type })
if ( $closureCaptureType.Count -ne 1 -or $closureCaptureType[0].cAbi -ne 36 -or
     $closureCall[0].type -ne $closureCapture[0].type ) {
    throw "XLIR tagged nullable closure capture/result TypeId is inconsistent"
}

& $xl cgen $closureSource --backend xlir --emit $closureCPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR tagged nullable closure Cgen failed" }
$closureGenerated = [System.IO.File]::ReadAllText($closureCPath, [System.Text.Encoding]::UTF8)
foreach ( $pattern in @(
    'typedef struct xlir_closure_env_[0-9]+ \{[^}]*xlir_nullable_t[0-9]+ c0;',
    'static bool xlir_closure_invoke_[0-9]+\(void\* environment, xlir_nullable_t[0-9]+\* result\)',
    'e[0-9]+->c0 = v[0-9]+;',
    '\(\(bool \(\*\)\(void\*, xlir_nullable_t[0-9]+\*\)\)v[0-9]+->invoke\)'
) ) {
    if ( $closureGenerated -notmatch $pattern ) {
        throw "XLIR tagged nullable closure C ABI is incomplete: $pattern"
    }
}
$closureCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror $closureHostSource !XL6_CGEN_LINKS! -o $closureExePath && $closureExePath"
& $env:ComSpec /d /v:on /c $closureCommand
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR tagged nullable closure generated-C runtime failed" }

& $xl ir $asyncSource --lowered --format text --emit $asyncIrPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR tagged nullable async text IR failed" }
$asyncText = [System.IO.File]::ReadAllText($asyncIrPath, [System.Text.Encoding]::UTF8)
foreach ( $required in @("async-result=copy-tagged", "async=copy-tagged", "captures=[copy-value,copy-value]") ) {
    if ( ! $asyncText.Contains($required) ) {
        throw "XLIR tagged nullable async ABI is incomplete: $required"
    }
}

& $xl ir $asyncSource --lowered --format json --emit $asyncJsonPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR tagged nullable async JSON IR failed" }
$asyncJson = [System.IO.File]::ReadAllText($asyncJsonPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$asyncTarget = @($asyncJson.callTargets | Where-Object {
    $_.kind -eq 4 -and $_.asyncResultAbiName -eq "copy-tagged" -and
    @($_.asyncCaptureAbis).Count -eq 2 -and
    @($_.asyncCaptureAbis | Where-Object { $_ -eq "copy-value" }).Count -eq 2
})
if ( $asyncTarget.Count -ne 1 -or
     @($asyncJson.instructions | Where-Object { $_.kindName -eq "await" -and $_.asyncResultAbiName -eq "copy-tagged" }).Count -ne 2 ) {
    throw "XLIR tagged nullable async JSON omits exact result or frame capture facts"
}

& $xl cgen $asyncSource --backend xlir --emit $asyncCPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR tagged nullable async Cgen failed" }
$asyncGenerated = [System.IO.File]::ReadAllText($asyncCPath, [System.Text.Encoding]::UTF8)
foreach ( $pattern in @(
    'typedef struct xlir_async_frame_[0-9]+ \{[^}]*xlir_nullable_t[0-9]+ p1;',
    'static xlir_nullable_t[0-9]+ xlir_future_await_xlir_nullable_t[0-9]+\(xfuture\* future\)',
    'xlir_nullable_t[0-9]+\* value;',
    'result->Value = value; result->Destroy = xlir_async_value_free; result->DestroyData = NULL;'
) ) {
    if ( $asyncGenerated -notmatch $pattern ) {
        throw "XLIR tagged nullable async C ABI is incomplete: $pattern"
    }
}
$asyncCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror $asyncHostSource !XL6_CGEN_LINKS! -o $asyncExePath && $asyncExePath"
& $env:ComSpec /d /v:on /c $asyncCommand
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR tagged nullable async generated-C runtime failed" }

& $xl ir $continuationSource --lowered --format text --emit $continuationIrPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR tagged nullable continuation text IR failed" }
$continuationText = [System.IO.File]::ReadAllText($continuationIrPath, [System.Text.Encoding]::UTF8)
foreach ( $required in @("async-machine m0 status=analyzed target=2", "machine-frame f0 source=storage residence=launch-parameter", "transfer=copy-value destroy=none", "suspension q0", "result=v22/t4/copy-tagged", "null-test", "nullable-unwrap") ) {
    if ( ! $continuationText.Contains($required) ) {
        throw "XLIR tagged nullable machine continuation ABI is incomplete: $required"
    }
}

& $xl ir $continuationSource --lowered --format json --emit $continuationJsonPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR tagged nullable continuation JSON IR failed" }
$continuationJson = [System.IO.File]::ReadAllText($continuationJsonPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$continuationTarget = @($continuationJson.callTargets | Where-Object {
    $_.kind -eq 4 -and $_.asyncDispatch -eq 1 -and $_.asyncMachine -ne 4294967295 -and
    $_.asyncResultAbiName -eq "copy-i64" -and
    @($_.asyncCaptureAbis).Count -eq 2 -and $_.asyncCaptureAbis[0] -eq "retain-future" -and
    $_.asyncCaptureAbis[1] -eq "copy-value"
})
$continuationMachine = @($continuationJson.asyncMachines | Where-Object {
    $_.statusName -eq "analyzed" -and $_.targetSymbol -eq $continuationTarget[0].symbol -and
    $_.frameSlotCount -eq 1 -and $_.suspensionCount -eq 1
})
$continuationCapture = @($continuationJson.asyncMachineFrameSlots | Where-Object {
    $_.sourceName -eq "storage" -and $_.residenceName -eq "launch-parameter" -and
    $_.transferAbiName -eq "copy-value" -and
    $_.destroyAbiName -eq "none"
})
$continuationSuspension = @($continuationJson.asyncSuspensions | Where-Object {
    $_.machine -eq $continuationMachine[0].id -and $_.resultAbiName -eq "copy-tagged"
})
if ( $continuationTarget.Count -ne 1 -or $continuationMachine.Count -ne 1 -or
     $continuationCapture.Count -ne 1 -or $continuationSuspension.Count -ne 1 -or
     $continuationSuspension[0].resultType -eq $continuationTarget[0].workerReturnType ) {
    throw "XLIR tagged nullable continuation JSON omits its typed input-to-result transition"
}

& $xl cgen $continuationSource --backend xlir --emit $continuationCPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR tagged nullable continuation Cgen failed" }
$continuationGenerated = [System.IO.File]::ReadAllText($continuationCPath, [System.Text.Encoding]::UTF8)
foreach ( $pattern in @(
    'typedef struct xlir_async_machine_frame_[0-9]+ \{[^}]*int64_t f0;[^}]*xpromise\* outer;',
    'v[0-9]+ = \*\(const xlir_nullable_t[0-9]+\*\)input->Value;',
    'v[0-9]+ = !v[0-9]+\.hasValue;',
    'v[0-9]+ = v[0-9]+\.value;',
    'v[0-9]+ = frame->f0;',
    'xrtPromiseResolveOwned\(destination, value, xlir_async_value_free, NULL\)',
    'xlir_async_machine_watch_[0-9]+_0\(frame, v[0-9]+\)'
) ) {
    if ( $continuationGenerated -notmatch $pattern ) {
        throw "XLIR tagged nullable continuation C ABI is incomplete: $pattern"
    }
}
if ( $continuationGenerated -match 'xlir_async_capture_chain_' ) {
    throw "XLIR tagged nullable continuation escaped the verified async machine Cgen path"
}
$continuationCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror $continuationHostSource !XL6_CGEN_LINKS! -o $continuationExePath && $continuationExePath"
& $env:ComSpec /d /v:on /c $continuationCommand
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR tagged nullable continuation generated-C runtime failed" }

& $xl ir $classFieldSource --lowered --format text --emit $classFieldIrPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR tagged nullable class-field text IR failed" }
$classFieldText = [System.IO.File]::ReadAllText($classFieldIrPath, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "init=tagged-null",
    "runtime=xvalue-tagged-nullable-box",
    "runtime=xvalue-object-get-literal-tagged-nullable"
) ) {
    if ( ! $classFieldText.Contains($required) ) {
        throw "XLIR tagged nullable class-field lifecycle is incomplete: $required"
    }
}

& $xl ir $classFieldSource --lowered --format json --emit $classFieldJsonPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR tagged nullable class-field JSON IR failed" }
$classFieldJson = [System.IO.File]::ReadAllText($classFieldJsonPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$taggedClassFields = @($classFieldJson.classFields | Where-Object {
    $_.initializerAbiName -eq "tagged-null"
})
$taggedBoxes = @($classFieldJson.instructions | Where-Object {
    $_.runtimeOpName -eq "xvalue-tagged-nullable-box" -and $_.operandCount -eq 1
})
$taggedGets = @($classFieldJson.instructions | Where-Object {
    $_.runtimeOpName -eq "xvalue-object-get-literal-tagged-nullable" -and
    $_.operandCount -eq 1 -and $_.hasImmediate -eq 1
})
if ( $taggedClassFields.Count -ne 1 -or $taggedBoxes.Count -lt 1 -or
     $taggedGets.Count -lt 2 ) {
    throw "XLIR tagged nullable class-field JSON omits its initializer, box or projection ABI"
}
$taggedFieldType = @($classFieldJson.types | Where-Object {
    $_.id -eq $taggedClassFields[0].type -and $_.cAbi -eq 36 -and $_.nullAbi -eq 3
})
if ( $taggedFieldType.Count -ne 1 -or
     @($taggedGets | Where-Object { $_.type -ne $taggedClassFields[0].type }).Count -ne 0 ) {
    throw "XLIR tagged nullable class-field projection does not preserve the declared TypeId"
}

& $xl cgen $classFieldSource --backend xlir --emit $classFieldCPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR tagged nullable class-field Cgen failed" }
$classFieldGenerated = [System.IO.File]::ReadAllText($classFieldCPath, [System.Text.Encoding]::UTF8)
foreach ( $pattern in @(
    'static xvalue\* xlir_xvalue_box_xlir_nullable_t[0-9]+\(xlir_nullable_t[0-9]+ value\)',
    'if \(!value\.hasValue\) return xrtValueRetain\(xrtValueNull\(\)\);',
    'static xlir_nullable_t[0-9]+ xlir_xvalue_object_get_literal_xlir_nullable_t[0-9]+\(',
    'if \(xrtValueType\(item\) == 0\) return \(xlir_nullable_t[0-9]+\)\{ false \};',
    'if \(!xrtValueGetInt\(item, &value\)\) abort\(\);'
) ) {
    if ( $classFieldGenerated -notmatch $pattern ) {
        throw "XLIR tagged nullable class-field C ABI is incomplete: $pattern"
    }
}
$classFieldCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror $classFieldHostSource !XL6_CGEN_LINKS! -o $classFieldExePath && $classFieldExePath"
& $env:ComSpec /d /v:on /c $classFieldCommand
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR tagged nullable class-field generated-C runtime failed" }

& $xl ir $sentinelCoalesceSource --lowered --format text --emit $sentinelCoalesceIrPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR xvalue-sentinel coalesce text IR failed" }
$sentinelCoalesceText = [System.IO.File]::ReadAllText($sentinelCoalesceIrPath, [System.Text.Encoding]::UTF8)
foreach ( $pattern in @(
        'type t[0-9]+ kind=1 collection=none resource=none persistence=xrt-clone/t0 cabi=26 callable=invalid ownership=2 null=2 nominal=s0 typeValue=0x[0-9A-F]{16} name=var display=var',
        'type t[0-9]+ kind=2 collection=none resource=none persistence=class-schema/t0 cabi=26 callable=invalid ownership=2 null=2 nominal=s[1-9][0-9]* typeValue=0x[0-9A-F]{16} name=CoalesceNode display=CoalesceNode',
        'type t[0-9]+ kind=8 collection=none resource=none persistence=nullable/t[0-9]+ cabi=26 callable=invalid ownership=2 null=2 nominal=s0 typeValue=0x[0-9A-F]{16} name=nullable display=nullable<CoalesceNode>',
    '\bnull-test opcode=none runtime=none',
    '\bnullable-unwrap opcode=none runtime=none',
    '\bretain opcode=none runtime=none',
    '\brelease opcode=none runtime=none'
) ) {
    if ( $sentinelCoalesceText -notmatch $pattern ) {
        throw "XLIR xvalue-sentinel coalesce has no exact typed ownership CFG: $pattern"
    }
}

& $xl ir $sentinelCoalesceSource --lowered --format json --emit $sentinelCoalesceJsonPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR xvalue-sentinel coalesce JSON IR failed" }
$sentinelCoalesceJson = [System.IO.File]::ReadAllText($sentinelCoalesceJsonPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$sentinelVarTypes = @($sentinelCoalesceJson.types | Where-Object {
    $_.kind -eq 1 -and $_.cAbi -eq 26 -and $_.ownershipAbi -eq 2 -and
    $_.nullAbi -eq 2 -and $_.nominalSymbol -eq 0
})
$sentinelClassTypes = @($sentinelCoalesceJson.types | Where-Object {
    $_.kind -eq 2 -and $_.cAbi -eq 26 -and $_.ownershipAbi -eq 2 -and
    $_.nullAbi -eq 2 -and $_.nominalSymbol -ne 0
})
if ( $sentinelVarTypes.Count -lt 1 -or $sentinelClassTypes.Count -lt 1 ) {
    throw "XLIR xvalue-sentinel coalesce JSON omits var or class representation ABI"
}
$sentinelClassType = $sentinelClassTypes[0]
$sentinelNullableTypes = @($sentinelCoalesceJson.types | Where-Object {
    $_.kind -eq 8 -and $_.cAbi -eq $sentinelClassType.cAbi -and
    $_.ownershipAbi -eq $sentinelClassType.ownershipAbi -and
    $_.nullAbi -eq $sentinelClassType.nullAbi -and
    @($_.params).Count -eq 1 -and @($_.params)[0] -eq $sentinelClassType.id
})
if ( $sentinelNullableTypes.Count -ne 1 ) {
    throw "XLIR nullable class does not preserve the exact xvalue-sentinel representation"
}
$sentinelNullableType = $sentinelNullableTypes[0]
$sentinelUnwraps = @($sentinelCoalesceJson.instructions | Where-Object {
    $_.kindName -eq 'nullable-unwrap' -and $_.type -eq $sentinelClassType.id -and
    $_.operandCount -eq 1
})
$sentinelNullTests = @($sentinelCoalesceJson.instructions | Where-Object {
    $_.kindName -eq 'null-test' -and $_.operandCount -eq 1
})
$sentinelRetains = @($sentinelCoalesceJson.instructions | Where-Object {
    $_.kindName -eq 'retain' -and $_.operandCount -eq 1 -and
    $_.type -in @($sentinelVarTypes[0].id, $sentinelClassType.id)
})
$sentinelReleases = @($sentinelCoalesceJson.instructions | Where-Object {
    $_.kindName -eq 'release' -and
    $_.type -in @($sentinelVarTypes[0].id, $sentinelNullableType.id)
})
if ( $sentinelUnwraps.Count -ne 1 -or $sentinelNullTests.Count -lt 2 -or
     $sentinelRetains.Count -lt 2 -or $sentinelReleases.Count -lt 4 ) {
    throw "XLIR xvalue-sentinel coalesce JSON omits its typed test/unwrap/ownership operations"
}

& $xl cgen $sentinelCoalesceSource --backend xlir --emit $sentinelCoalesceCPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR xvalue-sentinel coalesce Cgen failed" }
$sentinelCoalesceGenerated = [System.IO.File]::ReadAllText($sentinelCoalesceCPath, [System.Text.Encoding]::UTF8)
foreach ( $pattern in @(
    'v(?<unwrap>[0-9]+) = v[0-9]+;\r?\n\s+v[0-9]+ = xrtValueRetain\(v\k<unwrap>\);',
    '\{ xvalue\* xlir_released = (?<slot>s[0-9]+); \k<slot> = NULL; xrtValueRelease\(xlir_released\); \}',
    'int64_t xlir_nullable_sentinel_var\(bool xlir_input_0\)',
    'int64_t xlir_nullable_sentinel_class\(bool xlir_input_0\)',
    'int64_t xlir_nullable_literal_coalesce\(void\)'
) ) {
    if ( $sentinelCoalesceGenerated -notmatch $pattern ) {
        throw "XLIR xvalue-sentinel coalesce C ABI is incomplete: $pattern"
    }
}
$sentinelCoalesceCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror $sentinelCoalesceHostSource !XL6_CGEN_LINKS! -o $sentinelCoalesceExePath && $sentinelCoalesceExePath"
& $env:ComSpec /d /v:on /c $sentinelCoalesceCommand
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR xvalue-sentinel coalesce generated-C runtime failed" }

& $xl ir $sentinelAssignSource --lowered --format text --emit $sentinelAssignIrPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR null-aware assignment text IR failed" }
$sentinelAssignText = [System.IO.File]::ReadAllText($sentinelAssignIrPath, [System.Text.Encoding]::UTF8)
if ( ([regex]::Matches($sentinelAssignText,
          '\bload opcode=none runtime=none v[0-9]+ type=(?<type>[0-9]+) target=(?<storage>[0-9]+).* origin=n(?<origin>[0-9]+)[^\r\n]*\r?\n' +
          '\s+i[0-9]+ null-test [^\r\n]* origin=n\k<origin>[^\r\n]*\r?\n' +
          '\s+i[0-9]+ branch [^\r\n]* origin=n\k<origin>[^\r\n]*(?:\r?\n[\s\S]*?)?' +
          '\b(RUNTIME-CALL|call|value)\b[^\r\n]*\r?\n' +
          '\s+i[0-9]+ release [^\r\n]* type=\k<type> target=\k<storage>[^\r\n]* origin=n\k<origin>[^\r\n]*\r?\n' +
          '\s+i[0-9]+ store [^\r\n]* type=\k<type> target=\k<storage>[^\r\n]* origin=n\k<origin>',
          [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)).Count -lt 2 -or
     $sentinelAssignText -notmatch 'symbol=3 errors=no-raise .* reach=unreachable' ) {
    throw "XLIR null-aware assignment text omits lazy sentinel CFG or static non-null RHS elimination"
}

& $xl ir $sentinelAssignSource --lowered --format json --emit $sentinelAssignJsonPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR null-aware assignment JSON IR failed" }
$sentinelAssignJson = [System.IO.File]::ReadAllText($sentinelAssignJsonPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$varAssignExport = @($sentinelAssignJson.exports | Where-Object {
    $_.cName -eq 'xlir_nullable_sentinel_var_assign'
})
$classAssignExport = @($sentinelAssignJson.exports | Where-Object {
    $_.cName -eq 'xlir_nullable_sentinel_class_assign'
})
$nonnullAssignExport = @($sentinelAssignJson.exports | Where-Object {
    $_.cName -eq 'xlir_nonnull_assign_if_null'
})
if ( $varAssignExport.Count -ne 1 -or $classAssignExport.Count -ne 1 -or
     $nonnullAssignExport.Count -ne 1 ) {
    throw "XLIR null-aware assignment JSON omits its public probes"
}
$varAssignFunction = @($sentinelAssignJson.functions | Where-Object {
    $_.symbol -eq $varAssignExport[0].symbol
})
$classAssignFunction = @($sentinelAssignJson.functions | Where-Object {
    $_.symbol -eq $classAssignExport[0].symbol
})
$nonnullAssignFunction = @($sentinelAssignJson.functions | Where-Object {
    $_.symbol -eq $nonnullAssignExport[0].symbol
})
$deadFallback = @($sentinelAssignJson.functions | Where-Object {
    $_.symbol -eq 3 -and $_.reachability -eq 'unreachable'
})
if ( $varAssignFunction.Count -ne 1 -or $classAssignFunction.Count -ne 1 -or
     $nonnullAssignFunction.Count -ne 1 -or $deadFallback.Count -ne 1 ) {
    throw "XLIR null-aware assignment JSON has no exact reachability/function ABI"
}
function Get-FunctionInstructions($function) {
    return @($sentinelAssignJson.instructions |
        Select-Object -Skip $function.firstInstruction -First $function.instructionCount)
}
$varAssignInstructions = @(Get-FunctionInstructions $varAssignFunction[0])
$classAssignInstructions = @(Get-FunctionInstructions $classAssignFunction[0])
$nonnullAssignInstructions = @(Get-FunctionInstructions $nonnullAssignFunction[0])
foreach ( $instructions in @($varAssignInstructions, $classAssignInstructions) ) {
    $tests = @($instructions | Where-Object { $_.kindName -eq 'null-test' })
    $branches = @($instructions | Where-Object { $_.kindName -eq 'branch' })
    $ownedReplacements = @($instructions | Where-Object {
        $release = $_
        $release.kindName -eq 'release' -and $release.targetSymbol -ne 0 -and
        @($instructions | Where-Object {
            $_.kindName -eq 'store' -and $_.origin -eq $release.origin -and
            $_.type -eq $release.type -and $_.targetSymbol -eq $release.targetSymbol
        }).Count -ge 1
    })
    if ( $tests.Count -lt 1 -or $branches.Count -lt 1 -or $ownedReplacements.Count -lt 1 ) {
        throw "XLIR null-aware assignment JSON omits test/branch/release/store ownership CFG"
    }
}
if ( $nonnullAssignFunction[0].blockCount -ne 1 -or
     @($nonnullAssignInstructions | Where-Object {
         $_.kindName -in @('call', 'null-test', 'branch')
     }).Count -ne 0 ) {
    throw "XLIR statically non-null ?= did not eliminate its lazy RHS and branch"
}

& $xl cgen $sentinelAssignSource --backend xlir --emit $sentinelAssignCPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR null-aware assignment Cgen failed" }
$sentinelAssignGenerated = [System.IO.File]::ReadAllText($sentinelAssignCPath, [System.Text.Encoding]::UTF8)
foreach ( $pattern in @(
    'if \(v[0-9]+\) goto b[0-9]+; else goto b[0-9]+;',
    '\{ xvalue\* xlir_released = (?<slot>s[0-9]+); \k<slot> = NULL; xrtValueRelease\(xlir_released\); \}\r?\n\s+\k<slot> = v[0-9]+;',
    'int64_t xlir_nullable_sentinel_var_assign\(bool xlir_input_0\)',
    'int64_t xlir_nullable_sentinel_class_assign\(bool xlir_input_0\)',
    'int64_t xlir_nonnull_assign_if_null\(void\)'
) ) {
    if ( $sentinelAssignGenerated -notmatch $pattern ) {
        throw "XLIR null-aware assignment C ABI is incomplete: $pattern"
    }
}
$nonnullWrapper = [regex]::Match($sentinelAssignGenerated,
    'int64_t xlir_nonnull_assign_if_null\(void\)[\s\S]*?xlir_fn_(?<fn>[0-9]+)\(\)')
if ( ! $nonnullWrapper.Success ) { throw "XLIR non-null ?= wrapper has no resolved body" }
$nonnullBody = [regex]::Match($sentinelAssignGenerated,
    'static int64_t xlir_fn_' + $nonnullWrapper.Groups['fn'].Value +
    '\(void\)\s*\{(?<body>[\s\S]*?)\r?\n\}')
if ( ! $nonnullBody.Success -or $nonnullBody.Groups['body'].Value -match '\bxlir_fn_[0-9]+\(' ) {
    throw "XLIR statically non-null ?= generated C still evaluates its RHS"
}
$sentinelAssignCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror $sentinelAssignHostSource !XL6_CGEN_LINKS! -o $sentinelAssignExePath && $sentinelAssignExePath"
& $env:ComSpec /d /v:on /c $sentinelAssignCommand
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR null-aware assignment generated-C runtime failed" }

# Conditional index writes now share the verified address transaction. Their
# current-source gate checks lazy RHS, throwing operands, actual suspension and
# merge/failure cleanup; do not retain C-text layouts or a no-raise admission rule.
& (Get-Process -Id $PID).Path -NoProfile -File (Join-Path $Root "tools/check_xlir_index_conditional_assign.ps1") -Root $Root
if ($LASTEXITCODE -ne 0) { throw "XLIR conditional index generated-C runtime failed" }

& (Get-Process -Id $PID).Path -NoProfile -File (Join-Path $Root "tools/check_xlir_storage_conditional_assign.ps1") -Root $Root
if ($LASTEXITCODE -ne 0) { throw "XLIR conditional storage generated-C runtime failed" }

& (Get-Process -Id $PID).Path -NoProfile -File (Join-Path $Root "tools/check_xlir_member_assignment_transaction.ps1") -Root $Root
if ($LASTEXITCODE -ne 0) { throw "XLIR member assignment generated-C runtime failed" }

Write-Output "XLIR contextual null ABI gate ok: nullable xvalue/raw-pointer sentinels, explicit class wrap, tagged bool/int/u64/float payloads, canonical may-raise carriers, explicit wrap/unwrap and lazy coalesce CFG, xvalue-sentinel var/class lazy coalesce and null-aware assignment, static non-null ?= RHS elimination, borrowed index current with one-shot key, protected address, raising lazy RHS and consuming replacement, immutable closure capture/result, mutable capture cells, async result/frame copy, typed continuation coalesce, tagged class-field initialization/box/projection lifecycle, public ABI, JSON/text IR, strict C, and runtime."
exit 0
