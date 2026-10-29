param(
    [string]$Root = "",
    [string]$SourceDriver = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

# Always generate from current compiler sources, never the release compiler.
if ($SourceDriver -eq "") {
    Push-Location $Root
    try {
        New-Item -ItemType Directory -Force -Path build/test | Out-Null
        if (!(Test-Path -LiteralPath build/test/xl.dll) -or
            (Get-FileHash release/xl.dll).Hash -ne (Get-FileHash build/test/xl.dll).Hash) {
            Copy-Item -LiteralPath release/xl.dll -Destination build/test/xl.dll -Force
        }
        $prefix = 'build/test/xlir_product_result_source'
        $compilerSources = @('src/ast/xl6_ast.c','src/type/xl6_type_model.c',
            'src/ir/xl6_ir.c','src/ir/xl6_ir_lower.c',
            'src/cgen/xl6_ir_cgen.c','src/cgen/xl6_cgen_shared_runtime.c',
            'src/diag/xl6_diag.c','src/common/xl6_output.c','src/common/xl6_console.c',
            'src/lexer/xl6_token.c','src/lexer/xl6_lexer_api.c','src/lexer/xl6_lexer_gen.c',
            'src/parser/xl6_parser.c','src/parser/xl6_parser_api.c',
            'src/sem/xl6_sem.c','src/sem/xl6_protocol.c','src/sem/xl6_type_registry.c',
            'src/sem/xl6_sem_docs.c','src/sem/xl6_sem_field_attrs.c',
            'src/stdlib/xl6_stdlib.c','src/stdlib/xl6_stdlib_data.c')
        $strict = @('-std=c11','-Wall','-Wextra','-Werror','-O0','-Wno-unused-function','-Wno-unused-label',
            '-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-const-variable',
            '-Wno-unused-but-set-variable','-Wno-missing-field-initializers',
            '-Wno-pointer-sign','-Wno-format-truncation','-I.','-Ibuild/test')
        & gcc @strict test/cases/xlir_owner_consumption_contract.c @compilerSources release/xl.dll -o "$prefix.contract.exe"
        if ($LASTEXITCODE -ne 0) { throw 'Product owner source contract build failed' }
        $SourceDriver = Join-Path $Root "$prefix.contract.exe"
    } finally { Pop-Location }
}
$SourceDriver = (Resolve-Path -LiteralPath $SourceDriver).Path
$source = Join-Path $Root "test\cases\cgen_multi_return.xl"
$classSource = Join-Path $Root "test\cases\cgen_class_method_multi_return.xl"
$closureSource = Join-Path $Root "test\cases\xlir_product_closure_result.xl"
$genericSource = Join-Path $Root "test\cases\xlir_product_generic_result.xl"
$declarationOnlySource = Join-Path $Root "test\cases\xlir_product_declaration_only.xl"
$managedSource = Join-Path $Root "test\cases\xlir_managed_product_result.xl"
$hostSource = Join-Path $Root "test\cases\xlir_product_result_host.c"
$managedHostSource = Join-Path $Root "test\cases\xlir_managed_product_result_host.c"
$outputDir = Join-Path $Root "build\test"
$loweredPath = Join-Path $outputDir "xlir_product_result.lowered.json"
$classLoweredPath = Join-Path $outputDir "xlir_product_result_class.lowered.json"
$closureLoweredPath = Join-Path $outputDir "xlir_product_result_closure.lowered.json"
$genericLoweredPath = Join-Path $outputDir "xlir_product_result_generic.lowered.json"
$declarationOnlyLoweredPath = Join-Path $outputDir "xlir_product_declaration_only.lowered.json"
$managedLoweredPath = Join-Path $outputDir "xlir_managed_product_result.lowered.json"
$cPath = Join-Path $outputDir "xlir_product_result_generated.c"
$classCPath = Join-Path $outputDir "xlir_product_result_class_generated.c"
$closureCPath = Join-Path $outputDir "xlir_product_result_closure_generated.c"
$genericCPath = Join-Path $outputDir "xlir_product_result_generic_generated.c"
$managedCPath = Join-Path $outputDir "xlir_managed_product_result_generated.c"
$exePath = Join-Path $outputDir "xlir_product_result_host.exe"
$classExePath = Join-Path $outputDir "xlir_product_result_class_host.exe"
$closureExePath = Join-Path $outputDir "xlir_product_result_closure_host.exe"
$genericExePath = Join-Path $outputDir "xlir_product_result_generic_host.exe"
$managedExePath = Join-Path $outputDir "xlir_managed_product_result_host.exe"

foreach ( $path in @($SourceDriver, $source, $classSource, $closureSource, $genericSource, $declarationOnlySource, $managedSource, $hostSource, $managedHostSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR product-result gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

function Read-LoweredProduct([string]$inputPath, [string]$outputPath) {
    & $SourceDriver $inputPath "$outputPath.c" "$outputPath.h" $outputPath
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR product-result lowering failed: $inputPath"
    }
    return [System.IO.File]::ReadAllText(
        $outputPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
}

function Assert-ProductContract($module, [string]$label) {
    $products = @($module.types | Where-Object {
        $_.kind -eq 5 -and $_.cAbi -eq 32 -and $_.ownershipAbi -eq 1 -and
        $_.nullAbi -eq 0 -and $_.cName -match '^xlir_product_t[0-9]+$' -and
        @($_.params).Count -ge 2
    })
    if ( $products.Count -ne 1 ) {
        throw "$label has no single canonical trivial product type"
    }
    $product = $products[0]
    $productId = [int]$product.id
    $makes = @($module.instructions | Where-Object {
        $_.kindName -eq 'product-make' -and $_.type -eq $productId -and
        $_.result -ne 0 -and $_.operandCount -eq @($product.params).Count -and
        $_.elementIndex -eq -1
    })
    $gets = @($module.instructions | Where-Object {
        $_.kindName -eq 'product-get' -and $_.operandCount -eq 1 -and
        $_.result -ne 0 -and $_.elementIndex -ge 0 -and
        $_.elementIndex -lt @($product.params).Count
    })
    if ( $makes.Count -lt 1 -or $gets.Count -lt 2 ) {
        throw "$label does not materialize product construction and projection in XLIR"
    }
    foreach ( $get in $gets ) {
        $sourceId = [int]@($get.operands)[0]
        $source = @($module.instructions | Where-Object { $_.result -eq $sourceId })
        if ( $source.Count -ne 1 -or $source[0].type -ne $productId -or
             $get.type -ne @($product.params)[[int]$get.elementIndex] ) {
            throw "$label product projection disagrees with its source product type"
        }
    }
    $functions = @($module.functions | Where-Object { $_.returnType -eq $productId })
    $targets = @($module.callTargets | Where-Object { $_.returnType -eq $productId })
    $calls = @($module.instructions | Where-Object {
        $_.kindName -in @('call', 'virtual-call', 'closure-call') -and
        $_.type -eq $productId -and $_.result -ne 0
    })
    if ( $functions.Count -lt 1 -or $targets.Count -lt 1 -or $calls.Count -lt 1 ) {
        throw "$label callable, function, and call instructions do not share the product TypeId"
    }
    return $product
}

$lowered = Read-LoweredProduct $source $loweredPath
$product = Assert-ProductContract $lowered "free-function fixture"
$classLowered = Read-LoweredProduct $classSource $classLoweredPath
$classProduct = Assert-ProductContract $classLowered "class-method fixture"
$closureLowered = Read-LoweredProduct $closureSource $closureLoweredPath
$closureProduct = Assert-ProductContract $closureLowered "closure fixture"
$closureSignatures = @($closureLowered.types | Where-Object {
    $_.kind -eq 4 -and $_.callableResultType -eq $closureProduct.id -and
    @($_.returns).Count -eq 2
})
$closureCalls = @($closureLowered.instructions | Where-Object {
    $_.kindName -eq 'closure-call' -and $_.type -eq $closureProduct.id -and
    $_.result -ne 0 -and $_.calleeType -in @($closureSignatures.id)
})
if ( $closureSignatures.Count -lt 1 -or $closureCalls.Count -ne 2 ) {
    throw "closure fixture does not carry its product through callableResultType and closure-call"
}
$genericLowered = Read-LoweredProduct $genericSource $genericLoweredPath
$genericProduct = Assert-ProductContract $genericLowered "generic fixture"
$genericParameters = @($genericLowered.types | Where-Object { $_.kind -eq 7 })
$genericParameterIds = @($genericParameters.id)
if ( $genericParameters.Count -lt 1 -or @($genericProduct.params).Count -ne 2 -or
     @($genericProduct.params | Where-Object { $_ -in $genericParameterIds }).Count -ne 0 ) {
    throw "generic fixture does not isolate its template tuple from the concrete product ABI"
}
$declarationOnlyLowered = Read-LoweredProduct $declarationOnlySource $declarationOnlyLoweredPath
$declarationOnlyProducts = @($declarationOnlyLowered.types | Where-Object {
    $_.kind -eq 5 -and $_.cAbi -eq 32 -and $_.ownershipAbi -eq 1 -and
    @($_.params).Count -eq 2
})
if ( $declarationOnlyProducts.Count -ne 1 ) {
    throw "declaration-only fixture has no canonical trivial product type"
}
$declarationOnlyProductId = [int]$declarationOnlyProducts[0].id
$declarationOnlyTargets = @($declarationOnlyLowered.callTargets | Where-Object {
    $_.returnType -eq $declarationOnlyProductId -and $_.reachability -eq 'unreachable'
})
$declarationOnlyFunctions = @($declarationOnlyLowered.functions | Where-Object {
    $_.returnType -eq $declarationOnlyProductId -and $_.reachability -eq 'unreachable'
})
$declarationOnlyCalls = @($declarationOnlyLowered.instructions | Where-Object {
    $_.kindName -in @('call', 'virtual-call', 'closure-call')
})
if ( $declarationOnlyTargets.Count -ne 1 -or $declarationOnlyFunctions.Count -ne 1 -or
     $declarationOnlyCalls.Count -ne 0 ) {
    throw "declaration-only fixture relies on a call expression instead of declaration-driven tuple interning"
}
$managedLowered = Read-LoweredProduct $managedSource $managedLoweredPath
$managedProducts = @($managedLowered.types | Where-Object {
    $_.kind -eq 5 -and $_.cAbi -eq 32 -and $_.ownershipAbi -eq 2 -and
    @($_.params).Count -eq 2
})
if ( $managedProducts.Count -ne 1 ) {
    throw "managed fixture has no canonical owned product type"
}
$managedProductId = [int]$managedProducts[0].id
$managedTakes = @($managedLowered.instructions | Where-Object {
    $_.kindName -eq 'product-take' -and $_.result -ne 0 -and
    $_.elementIndex -in @(0, 1)
})
$managedDrops = @($managedLowered.instructions | Where-Object {
    $_.kindName -eq 'product-drop' -and $_.result -eq 0 -and $_.elementIndex -eq 1
})
$managedMakes = @($managedLowered.instructions | Where-Object {
    $_.kindName -eq 'product-make' -and $_.type -eq $managedProductId -and
    $_.operandCount -eq 2
})
$ambiguousManagedGets = @($managedLowered.instructions | Where-Object {
    $_.kindName -eq 'product-get' -and $_.type -in @($managedProducts[0].params)
})
if ( $managedTakes.Count -ne 4 -or $managedDrops.Count -ne 0 -or
     $managedMakes.Count -ne 1 -or $ambiguousManagedGets.Count -ne 0 ) {
    throw "managed fixture does not encode exact field takes into protected owners"
}

& $SourceDriver $source $cPath "$cPath.h" "$cPath.json"
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR product-result Cgen failed" }
& $SourceDriver $classSource $classCPath "$classCPath.h" "$classCPath.json"
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR class product-result Cgen failed" }
& $SourceDriver $closureSource $closureCPath "$closureCPath.h" "$closureCPath.json"
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR closure product-result Cgen failed" }
& $SourceDriver $genericSource $genericCPath "$genericCPath.h" "$genericCPath.json"
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR generic product-result Cgen failed" }
& $SourceDriver $managedSource $managedCPath "$managedCPath.h" "$managedCPath.json"
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR managed product-result Cgen failed" }

foreach ( $entry in @(
    @{ Path = $cPath; Product = $product },
    @{ Path = $classCPath; Product = $classProduct },
    @{ Path = $closureCPath; Product = $closureProduct },
    @{ Path = $genericCPath; Product = $genericProduct }
) ) {
    $generated = [System.IO.File]::ReadAllText($entry.Path, [System.Text.Encoding]::UTF8)
    $typeName = [string]$entry.Product.cName
    if ( ([regex]::Matches($generated, "typedef struct $typeName")).Count -ne 1 -or
         ! $generated.Contains("($typeName){") -or
         ! $generated.Contains('.e0') -or ! $generated.Contains('.e1') ) {
        throw "XLIR generated C has no deduplicated named product construction/projection ABI"
    }
}
$managedGenerated = [System.IO.File]::ReadAllText($managedCPath, [System.Text.Encoding]::UTF8)
if ( ([regex]::Matches($managedGenerated, 'typedef struct xlir_product_t[0-9]+')).Count -ne 1 -or
     ([regex]::Matches($managedGenerated, '\.e[01] = \(xlir_string\)\{0\};')).Count -lt 3 -or
     ! $managedGenerated.Contains('xlir_string_drop(xlir_released);') -or
     ! $managedGenerated.Contains('xl6SlotRuntimeV1Get()->Apply') ) {
    throw "XLIR generated C does not mechanically move and drop managed product fields"
}

Push-Location $Root
try {
    $strict = @('-std=c11','-Wall','-Wextra','-Werror','-O0','-Wno-unused-function',
        '-Wno-unused-label','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-const-variable',
        '-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign',
        '-Wno-format-truncation','-I.','-Ibuild/test')
    foreach ($entry in @(
        @{Host=$hostSource;Exe=$exePath;Flags=@()},
        @{Host=$hostSource;Exe=$classExePath;Flags=@('-DXLIR_PRODUCT_CLASS_METHOD')},
        @{Host=$hostSource;Exe=$closureExePath;Flags=@('-DXLIR_PRODUCT_CLOSURE')},
        @{Host=$hostSource;Exe=$genericExePath;Flags=@('-DXLIR_PRODUCT_GENERIC')},
        @{Host=$managedHostSource;Exe=$managedExePath;Flags=@()}
    )) {
        & gcc @strict @($entry.Flags) $entry.Host release/xl.dll -o $entry.Exe
        if ($LASTEXITCODE -ne 0) { throw "Product-result strict C build failed: $($entry.Exe)" }
        & $entry.Exe
        if ($LASTEXITCODE -ne 0) { throw "Product-result strict C runtime failed: $($entry.Exe)" }
    }
} finally { Pop-Location }

Write-Output "XLIR product-result gate ok: declaration-driven canonical product TypeId, trivial get and protected managed field ownership, direct/class/closure/generic callable ABI, strict C, and runtimes."
exit 0
