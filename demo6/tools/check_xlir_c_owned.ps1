param([string]$Directory = ('build/c-owned/check-' + (Get-Date -Format 'yyyyMMdd-HHmmss')),
    [switch]$BuildOnly, [string]$PublicDriver = '')
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'xlir_c_object_cache.ps1')
$rootPath = (Get-Location).Path
$outputPath = [IO.Path]::GetFullPath((Join-Path $rootPath $Directory))
if (Test-Path -LiteralPath $outputPath) { throw 'Use a fresh C owner validation directory' }
New-Item -ItemType Directory -Force -Path "$outputPath/include/__xl_runtime" | Out-Null
Copy-Item -Path src/runtime/*.h -Destination "$outputPath/include/__xl_runtime"
Copy-Item -LiteralPath release/include/xruntime_decl.h -Destination "$outputPath/include/__xl_runtime/xruntime_decl.h"
Copy-Item -LiteralPath release/xl.dll -Destination "$outputPath/xl.dll"
$sources = @('src/ast/xl6_ast.c','src/type/xl6_type_model.c',
    'src/ir/xl6_ir.c','src/ir/xl6_ir_class_identity.c','src/ir/xl6_ir_lower.c',
    'src/cgen/xl6_ir_cgen.c','src/cgen/xl6_cgen_shared_runtime.c',
    'src/diag/xl6_diag.c','src/common/xl6_output.c','src/common/xl6_console.c',
    'src/lexer/xl6_token.c','src/lexer/xl6_lexer_api.c','src/lexer/xl6_lexer_gen.c',
    'src/parser/xl6_parser.c','src/parser/xl6_parser_api.c',
    'src/sem/xl6_sem.c','src/sem/xl6_protocol.c','src/sem/xl6_type_registry.c',
    'src/sem/xl6_sem_docs.c','src/sem/xl6_sem_field_attrs.c','src/sem/xl6_completion.c',
    'src/stdlib/xl6_stdlib.c','src/stdlib/xl6_stdlib_data.c','src/compile/xl6_compile_unit.c',
    'src/compile/xl6_imports.c','src/compile/xl6_source_overlay.c','src/api/xl_api_path.c',
    'src/package/xl6_package.c','src/compile/xl6_build_inputs.c','src/compile/xl6_source_report.c')
$flags = @('-std=c11','-Wall','-Wextra','-Werror','-O0','-ffunction-sections','-fdata-sections',
    '-Wno-unused-function','-Wno-unused-label','-Wno-unused-variable','-Wno-unused-parameter',
    '-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers',
    '-Wno-pointer-sign','-Wno-format-truncation','-I.','-Irelease')
$context = New-XlirCBuildContext -Root $rootPath -Directory $outputPath
$objects = @()
foreach ($source in $sources) {
    $object = Join-Path $outputPath ([IO.Path]::GetFileNameWithoutExtension($source) + '.o')
    $record = Invoke-XlirCObject -Context $context -Source (Join-Path $rootPath $source) -Object $object -Flags $flags
    Write-Output "$(if ($record.CacheHit) {'reuse'} else {'compile'}) $source"
    $objects += $object
}
$compiler = Join-Path $outputPath 'contract.exe'
& $context.Compiler @flags test/cases/xlir_c_owned_contract.c @objects release/xl.dll '-Wl,--gc-sections' -o $compiler
if ($LASTEXITCODE) { throw 'C owner compiler driver link failed' }
Write-Output "C owner compiler: $compiler"
if ($BuildOnly) { exit 0 }
& $context.Compiler @flags test/cases/xlir_c_owned_runtime.c release/xl.dll -o "$outputPath/runtime.exe"
if ($LASTEXITCODE) { throw 'C owner runtime test build failed' }
& "$outputPath/runtime.exe"
if ($LASTEXITCODE) { throw 'C owner runtime tests failed' }
& $context.Compiler @flags test/cases/xlir_c_owned_print_runtime.c release/xl.dll -o "$outputPath/print-runtime.exe"
if ($LASTEXITCODE) { throw 'C print runtime test build failed' }
& "$outputPath/print-runtime.exe"
if ($LASTEXITCODE) { throw 'C print runtime tests failed' }
$generatedSources=@(
    'test/cases/xlir_c_owned_basic.xl','test/cases/xlir_c_owned_lifetime.xl',
    'test/cases/xlir_c_owned_reads.xl','test/cases/xlir_c_owned_capture.xl',
    'test/cases/xlir_c_owned_views.xl','test/cases/xlir_c_owned_flow.xl',
    'test/cases/xlir_c_owned_writes.xl','test/cases/xlir_c_owned_print.xl',
    'test/cases/xlir_c_owned_traverse.xl','test/cases/xlir_c_owned_sequence.xl',
    'test/cases/xlir_c_owned_borrows.xl','test/cases/xlir_c_owned_failure.xl')
foreach ($source in $generatedSources) {
    $name=[IO.Path]::GetFileNameWithoutExtension($source).Substring('xlir_c_owned_'.Length)
    $generated = Join-Path $outputPath "$name.c"
    & $compiler $source $generated
    if ($LASTEXITCODE) { throw "C owner $name compilation failed" }
    & $context.Compiler @flags "-I$outputPath/include" $generated test/cases/xlir_c_owned_native.c release/xl.dll -o "$outputPath/$name.exe"
    if ($LASTEXITCODE) { throw "C owner $name generated C build failed" }
    & "$outputPath/$name.exe"
    if ($LASTEXITCODE) { throw "C owner $name execution failed" }
    Write-Output "C owner $name passed"
}
$prefix = "extern `"c_owned_text`" @no_raise func make() as point`nextern `"c_owned_release`" @no_raise func drop(point p)`n"
$negative = @(
    @{Name='parameter';Body="func probe(cstring a)`nend";Message='C markers are limited'},
    @{Name='container';Body="func probe()`n array<cbytes> a`nend";Message='C markers are limited'},
    @{Name='global';Body="cstring a = cstring.take(make(), drop)";Message='explicit local initialization'},
    @{Name='releaser';Body="extern `"wrong`" func wrong(point p)`nfunc probe()`n cstring a = cstring.take(make(), wrong)`nend";Message='releaser must be'},
    @{Name='field';Body="class Box`n cstring value`nend";Message='C markers are limited'},
    @{Name='nullable';Body="func probe()`n cstring? value`nend";Message='C markers are limited'},
    @{Name='alias';Body="typedef Alias = cstring";Message='C markers are limited'},
    @{Name='coroutine';Body="async func probe()`n cstring value = cstring.take(make(), drop)`nend";Message='coroutine frames'},
    @{Name='binary-text';Body="func probe()`n cbytes value = cbytes.take(make(), 1, drop)`n string wrong = string(value)`nend";Message='matching base type'}
)
foreach ($case in $negative) {
    $source = Join-Path $outputPath ('reject-' + $case.Name + '.xl')
    [IO.File]::WriteAllText($source, $prefix + $case.Body, [Text.UTF8Encoding]::new($false))
    $priorErrorAction = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    $diagnostic = @(& $compiler $source check 2>&1)
    $rejected = $LASTEXITCODE
    $ErrorActionPreference = $priorErrorAction
    if (!$rejected -or ($diagnostic -join "`n") -notmatch [regex]::Escape($case.Message)) {
        throw "C owner negative case $($case.Name) was not rejected with its expected diagnostic: $diagnostic"
    }
}
Write-Output "C owner negative cases passed: $($negative.Count)"
if ($PublicDriver) {
    $sourceDriver=(Resolve-Path -LiteralPath $PublicDriver).Path
    $publicSources=@($generatedSources|Where-Object{$_ -notlike '*_failure.xl'})+
        @('test/cases/xlir_c_owned_storage.xl','test/cases/xlir_c_owned_escapes.xl')
    foreach($source in $publicSources) {
        $name=[IO.Path]::GetFileNameWithoutExtension($source).Substring('xlir_c_owned_'.Length)
        & $sourceDriver $source run "$outputPath/jit-$name" test/cases/xlir_c_owned_native.c
        if($LASTEXITCODE){throw "C owner source JIT failed: $name"}
        & $sourceDriver $source exe "$outputPath/aot-$name" test/cases/xlir_c_owned_native.c
        if($LASTEXITCODE){throw "C owner source EXE build failed: $name"}
        & "$outputPath/aot-$name.exe"
        if($LASTEXITCODE){throw "C owner source EXE execution failed: $name"}
    }
    Write-Output "C owner source acceptance: $($publicSources.Count) JIT and $($publicSources.Count) standalone EXE programs passed"
}
