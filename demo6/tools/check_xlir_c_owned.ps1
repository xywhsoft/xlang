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
# C-owning captures use current resident cleanup interfaces. Link generated
# programs to the same source runtime that is used by JIT/AOT acceptance.
if (!$PublicDriver) {
    $sourceDirectory=Join-Path $outputPath 'public-source'
    & "$PSScriptRoot/build_xlir_public_source_driver.ps1" -Directory $sourceDirectory
    if (!$?) { throw 'Current source runtime build failed' }
    $PublicDriver=Join-Path $sourceDirectory 'public_source.exe'
}
$sourceManifest=Get-Content -LiteralPath (Join-Path (Split-Path $PublicDriver) 'build.json') -Raw | ConvertFrom-Json
$sourceRuntimeObjects=@($sourceManifest.Objects | Where-Object { [IO.Path]::GetFileName($_.Path) -ne 'xlir_public_source_driver.o' } | ForEach-Object Path)
if (!$sourceManifest.SourceXrt -or !$sourceRuntimeObjects.Count) { throw 'Generated C requires a current source runtime manifest' }

& $context.Compiler @flags test/cases/xlir_c_owned_runtime.c release/xl.dll -o "$outputPath/runtime.exe"
if ($LASTEXITCODE) { throw 'C owner runtime test build failed' }
& "$outputPath/runtime.exe"
if ($LASTEXITCODE) { throw 'C owner runtime tests failed' }
$ownerO2Flags=@($flags | Where-Object { $_ -ne '-O0' }) + '-O2'
& $context.Compiler @ownerO2Flags test/cases/xlir_c_owned_runtime.c release/xl.dll -o "$outputPath/runtime-O2.exe"
if ($LASTEXITCODE) { throw 'Optimized C owner runtime test build failed' }
& "$outputPath/runtime-O2.exe"
if ($LASTEXITCODE) { throw 'Optimized C owner runtime tests failed' }
& $context.Compiler @flags test/cases/xlir_c_owned_print_runtime.c release/xl.dll -o "$outputPath/print-runtime.exe"
if ($LASTEXITCODE) { throw 'C print runtime test build failed' }
& "$outputPath/print-runtime.exe"
if ($LASTEXITCODE) { throw 'C print runtime tests failed' }
$generatedSources=@(
    'test/cases/xlir_c_owned_basic.xl','test/cases/xlir_c_owned_lifetime.xl',
    'test/cases/xlir_c_owned_reads.xl','test/cases/xlir_c_owned_capture.xl',
    'test/cases/xlir_c_owned_views.xl','test/cases/xlir_c_owned_flow.xl',
    'test/cases/xlir_c_owned_calls.xl','test/cases/xlir_c_owned_exports.xl','test/cases/xlir_c_owned_frames.xl',
    'test/cases/xlir_c_owned_stdlib.xl',
    'test/cases/xlir_c_owned_sizes.xl',
    'test/cases/xlir_c_owned_defaults.xl',
    'test/cases/xlir_c_owned_spawn.xl',
    'test/cases/xlir_c_owned_alias_reader.xl','test/cases/xlir_c_owned_tuple_forward.xl',
    'test/cases/xlir_c_owned_spawn_export.xl','test/cases/xlir_c_owned_methods.xl',
    'test/cases/xlir_c_owned_callable_results.xl','test/cases/xlir_c_owned_variadic_results.xl',
    'test/cases/xlir_c_owned_globals.xl',
    'test/cases/xlir_c_owned_nullable.xl','test/cases/xlir_c_owned_nullable_boundaries.xl',
    'test/cases/xlir_c_owned_writes.xl','test/cases/xlir_c_owned_print.xl',
    'test/cases/xlir_c_owned_traverse.xl','test/cases/xlir_c_owned_sequence.xl',
    'test/cases/xlir_c_owned_borrows.xl','test/cases/xlir_c_owned_failure.xl')
foreach ($source in $generatedSources) {
    $name=[IO.Path]::GetFileNameWithoutExtension($source).Substring('xlir_c_owned_'.Length)
    $generated = Join-Path $outputPath "$name.c"
    & $compiler $source $generated
    if ($LASTEXITCODE) { throw "C owner $name compilation failed" }
    if($name -eq 'stdlib' -and [IO.File]::ReadAllText($generated).Contains('xl6CBytesCopyV1(')) {
        throw 'Native read-only standard library calls materialized a C bytes input'
    }
    if($name -eq 'nullable' -and [IO.File]::ReadAllText($generated) -match 'xl6C(String|Bytes)CopyV1\(') {
        throw 'Typed nullable C reads materialized their input'
    }
    if ($LASTEXITCODE) { throw "C owner $name compilation failed" }
    $linkArgs=@($flags)+@("-I$outputPath/include",$generated,'test/cases/xlir_c_owned_native.c')+
        $sourceRuntimeObjects+@('release/xl.dll','-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32','-Wl,--gc-sections','-Wl,--no-keep-memory','-o',"$outputPath/$name.exe")
    $response=Join-Path $outputPath "$name.rsp"
    [IO.File]::WriteAllText($response,(@($linkArgs | ForEach-Object { '"'+$_.Replace('\','/').Replace('"','\"')+'"' }) -join "`n"),[Text.UTF8Encoding]::new($false))
    & $context.Compiler ('@'+$response)
    if ($LASTEXITCODE) { throw "C owner $name generated C build failed" }
    & "$outputPath/$name.exe"
    if ($LASTEXITCODE) { throw "C owner $name execution failed" }
    Write-Output "C owner $name passed"
}
# Ordinary ownership contracts protect the common call/cleanup paths touched by
# C loans. Reuse the exact compiler objects just built above.
foreach ($optimization in @('O0','O2')) {
    $contractFlags=@($flags | Where-Object { $_ -ne '-O0' }) + "-$optimization"
    foreach ($name in @('xlir_runtime_operand_catalog','xlir_capture_ownership_contract',
        'xlir_managed_storage_inputs_contract','xlir_conditional_nullable_join_contract')) {
        $executable=Join-Path $outputPath "$name-$optimization.exe"
        & $context.Compiler @contractFlags "test/cases/$name.c" @objects release/xl.dll '-Wl,--gc-sections' -o $executable
        if ($LASTEXITCODE) { throw "Ordinary contract build failed: $name $optimization" }
        & $executable
        if ($LASTEXITCODE) { throw "Ordinary contract failed: $name $optimization" }
    }
}
$prefix = "extern `"c_owned_text`" @no_raise func make() as point`nextern `"c_owned_release`" @no_raise func drop(point p)`n"
$negative = @(
    @{Name='parameter';Body="func probe(cstring a)`nend";Message='C markers are limited'},
    @{Name='export-parameter';Body="export func probe(cstring a)`nend";Message='C markers are limited'},
    @{Name='span-return';Body="extern `"wrong`" @no_raise func wrong() as cbytes";Message='C result markers require'},
    @{Name='optional-span-return';Body="extern `"wrong`" @no_raise func wrong() as cbytes?";Message='C result markers require'},
    @{Name='optional-async-return';Body="async func wrong() as cstring?`n return null`nend";Message='C result markers require'},
    @{Name='native-span-return';Body="class Box`n native point handle`n extern `"wrong`" func wrong() as cbytes`nend";Message='C result markers require'},
    @{Name='container';Body="func probe()`n array<cbytes> a`nend";Message='C markers are limited'},
    @{Name='global';Body="cstring a = cstring.take(make(), drop)";Message='explicit local initialization'},
    @{Name='releaser';Body="extern `"wrong`" func wrong(point p)`nfunc probe()`n cstring a = cstring.take(make(), wrong)`nend";Message='releaser must be'},
    @{Name='field';Body="class Box`n cstring value`nend";Message='C markers are limited'},
    @{Name='nullable';Body="func probe()`n cstring? value`nend";Message='C markers are limited'},
    @{Name='alias';Body="typedef Alias = cstring";Message='C markers are limited'},
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
& "$PSScriptRoot/check_xlir_c_owned_globals_fault.ps1" -Directory "$outputPath/globals-fault" -Compiler $compiler -PublicDriver $PublicDriver
if(!$?){throw 'C owner global initialization fault validation failed'}
& "$PSScriptRoot/check_xlir_c_owned_boundary_fault.ps1" -Directory "$outputPath/boundary-fault" -Compiler $compiler -PublicDriver $PublicDriver
if(!$?){throw 'C owner callable boundary fault validation failed'}
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
    $ordinarySources=@('char_bytes_literals','char_bytes_advanced','anonymous_functions',
        'cgen_implicit_string_methods','nullable_string_lifecycle','cgen_task_spawn_fstring',
        'stdlib_codec','stdlib_compress_values','stdlib_encoding','stdlib_unicode_validator',
        'xlir_managed_storage_inputs','cgen_base_method_static_dispatch','cgen_async_generator',
        'xlir_module_product_globals')
    foreach($name in $ordinarySources) {
        $source="test/cases/$name.xl"
        & $sourceDriver $source run "$outputPath/ordinary-jit-$name"
        if($LASTEXITCODE){throw "Ordinary source JIT failed: $name"}
        & $sourceDriver $source exe "$outputPath/ordinary-aot-$name"
        if($LASTEXITCODE){throw "Ordinary source EXE build failed: $name"}
        & "$outputPath/ordinary-aot-$name.exe"
        if($LASTEXITCODE){throw "Ordinary source EXE execution failed: $name"}
        & $sourceDriver $source cgen-default "$outputPath/ordinary-$name.c"
        if($LASTEXITCODE){throw "Ordinary C generation failed: $name"}
        if([IO.File]::ReadAllText("$outputPath/ordinary-$name.c").Contains('__xl_runtime/xl6_c_owned.h')) {
            throw "Ordinary program acquired the C owner runtime: $name"
        }
    }
    Write-Output "Ordinary source acceptance: $($ordinarySources.Count) JIT and standalone EXE programs; no C owner headers"
    & "$PSScriptRoot/check_xlir_c_owned_exports.ps1" -PublicDirectory (Split-Path $sourceDriver) -Directory "$outputPath/exports-boundary"
    if (!$?) { throw 'C owner public export boundary validation failed' }
}
