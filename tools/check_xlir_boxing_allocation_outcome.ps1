param([string]$Root = '', [string]$PublicDirectory = '')
$ErrorActionPreference = 'Stop'
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
Push-Location $Root
try {
    $prefix = 'build/test/xlir_boxing_allocation_outcome'
    New-Item -ItemType Directory -Force -Path build/test | Out-Null
    if (!(Test-Path -LiteralPath build/test/xl.dll) -or
        (Get-FileHash release/xl.dll).Hash -ne (Get-FileHash build/test/xl.dll).Hash) {
        Copy-Item -LiteralPath release/xl.dll -Destination build/test/xl.dll -Force
    }
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
    & gcc @strict test/cases/xlir_boxing_allocation_outcome_contract.c @compilerSources release/xl.dll -o "$prefix.contract.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Boxing allocation outcome source contract build failed' }
    & "$prefix.contract.exe" "$prefix.c" "$prefix.h" "$prefix.json"
    if ($LASTEXITCODE -ne 0) { throw 'Boxing allocation outcome mutation/verification failed' }
    & "$prefix.contract.exe" "$prefix.repeat.c" "$prefix.repeat.h" "$prefix.repeat.json"
    if ($LASTEXITCODE -ne 0) { throw 'Boxing allocation outcome repeated verification failed' }
    foreach ($suffix in @('c','h','json')) {
        if ([IO.File]::ReadAllText("$prefix.$suffix") -cne [IO.File]::ReadAllText("$prefix.repeat.$suffix")) {
            throw "Boxing allocation outcome nondeterministic $suffix"
        }
    }
    $hostSource = 'test/cases/xlir_boxing_allocation_outcome_host.c'
    $runtimeSource = 'test/cases/xlir_boxing_allocation_outcome_runtime.c'
    & gcc @strict $runtimeSource $hostSource release/xl.dll -o "$prefix.strict.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Boxing allocation outcome strict C build failed' }
    & "$prefix.strict.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Boxing allocation outcome runtime/memory failed' }
    & release/xl.exe run $runtimeSource $hostSource --inc-path . --inc-path build/test -- --smoke
    if ($LASTEXITCODE -ne 0) { throw 'Boxing allocation outcome C-host JIT failed' }
    # A raw C host cannot supply language IR to the executable planner. Use
    # the verified language plan to select ALL resident providers, rather
    # than a hard-coded foreign-error-only link that hid missing components.
    if (!$PublicDirectory) {
        $PublicDirectory="$prefix.source-$([Guid]::NewGuid().ToString('N'))"
        & "$PSScriptRoot/build_xlir_public_source_driver.ps1" -Directory $PublicDirectory
    }
    $public=Get-Content -LiteralPath (Join-Path $PublicDirectory 'build.json') -Raw | ConvertFrom-Json
    if ($public.RuntimeOnly -or !$public.SourceXrt) { throw 'Plan requires current complete compiler source' }
    foreach($row in @($public.Inputs)+@($public.Headers)+@($public.Objects)) {
        if ((Get-FileHash -LiteralPath $row.Path).Hash-ne$row.Sha256) { throw "Stale plan input: $($row.Path)" }
    }
    $objects=@($public.Objects | Where-Object {[IO.Path]::GetFileName($_.Path)-ne'xlir_public_source_driver.o'} | ForEach-Object Path)
    & gcc @strict -DXL_STATIC_EMBED -Ilib/tcc test/cases/xlir_artifact_runtime_contract.c @objects release/xl.dll -lWs2_32 -lIPHLPAPI -lShell32 -lOle32 -lAdvapi32 -o "$prefix.plan.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Boxing allocation outcome artifact plan build failed' }
    & "$prefix.plan.exe" --emit-native test/cases/xlir_boxing_allocation_outcome.xl "$prefix.native.c" 1> "$prefix.plan.log"
    if ($LASTEXITCODE -ne 0) { throw 'Boxing allocation outcome verified artifact plan failed' }
    $plan=Get-Content -LiteralPath "$prefix.plan.log" | Where-Object {$_-match '^\{"Fixture"'} | ConvertFrom-Json
    if (!$plan.Sources.Count) { throw 'Boxing allocation outcome resident plan is empty' }
    & release/xl.exe build $runtimeSource $hostSource @($plan.Sources) --inc-path . --inc-path release --inc-path build/test -o "$prefix.aot.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Boxing allocation outcome C-host AOT build failed' }
    & "$prefix.aot.exe" --smoke
    if ($LASTEXITCODE -ne 0) { throw 'Boxing allocation outcome C-host AOT runtime failed' }
} finally { Pop-Location }
Write-Output 'XLIR boxing allocation outcome passed: nine boxing and eight container outcome families, native boolean plus independent callback failure, contextual/generic conversion effects, null success, numeric identity, product conversion, verifier rejection, strict C real faults, C-host JIT/AOT smoke and deterministic C/header/IR.'
exit 0
