param([string]$Root = '')
$ErrorActionPreference = 'Stop'
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
Push-Location $Root
try {
    $source = 'test/cases/xlir_module_cleanup_outcome.xl'
    $prefix = 'build/test/xlir_module_cleanup_outcome'
    New-Item -ItemType Directory -Force -Path build/test | Out-Null
    Copy-Item -LiteralPath release/xl.dll -Destination build/test/xl.dll -Force
    & release/xl.exe ir $source --lowered --format json --emit "$prefix.json"
    if ($LASTEXITCODE -ne 0) { throw 'Module cleanup outcome lowering failed' }
    $ir = [IO.File]::ReadAllText("$prefix.json") | ConvertFrom-Json
    $fini = $ir.functions[$ir.moduleFinalizerFunction - 1]
    if ($fini.kindName -ne 'module-fini' -or $fini.errorAbi -ne 'may-raise') {
        throw 'Module finalizer must expose its explicit failure carrier'
    }
    $body = @($ir.instructions | Where-Object {
        $_.index -ge $fini.firstInstruction -and $_.index -lt $fini.firstInstruction + $fini.instructionCount
    })
    if ($ir.moduleFinalizerCleanupCount -ne 3 -or $body.Count -ne 11 -or
        $body[0].kindName -ne 'cleanup-inherit-begin' -or $body[-4].kindName -ne 'cleanup-end' -or
        $body[-3].kindName -ne 'error-test' -or $body[-3].operands[0] -ne $body[-4].result -or
        $body[-2].kindName -ne 'throw' -or $body[-1].kindName -ne 'return') {
        throw 'Module cleanup needs inherited first failure, complete cleanup and exact outcome successors'
    }
    for ($i = 1; $i -lt 7; $i += 2) {
        if ($body[$i].kindName -ne 'release' -or $body[$i+1].kindName -ne 'cleanup-capture' -or
            $body[$i].errorScope -ne $body[0].errorScope -or $body[$i+1].errorScope -ne $body[0].errorScope) {
            throw 'Module cleanup must accumulate diagnostics after every exact owner'
        }
    }
    & release/xl.exe cgen $source --backend xlir --emit "$prefix.c"
    if ($LASTEXITCODE -ne 0) { throw 'Module cleanup Cgen failed' }
    & release/xl.exe cgen $source --backend xlir --emit "$prefix.repeat.c"
    if ($LASTEXITCODE -ne 0 -or [IO.File]::ReadAllText("$prefix.c") -cne [IO.File]::ReadAllText("$prefix.repeat.c")) {
        throw 'Module cleanup Cgen is not deterministic'
    }
    $strict = @('-std=c11','-Wall','-Wextra','-Werror','-O0','-Wno-unused-function','-Wno-unused-label',
        '-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-const-variable',
        '-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-I.','-Ibuild/test')
    $runtime = 'test/cases/xlir_module_cleanup_outcome_runtime.c'
    $hostSource = 'test/cases/xlir_module_cleanup_outcome_host.c'
    & gcc @strict $runtime $hostSource release/xl.dll -o "$prefix.strict.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Module cleanup strict C build failed' }
    & "$prefix.strict.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Module cleanup lost an outcome or resource' }
    & release/xl.exe run $runtime $hostSource --inc-path . --inc-path build/test -- --smoke
    if ($LASTEXITCODE -ne 0) { throw 'Module cleanup C-host JIT failed' }
    & release/xl.exe build $runtime $hostSource src/api/xl_foreign_error.c --inc-path . --inc-path build/test -o "$prefix.aot.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Module cleanup AOT build failed' }
    & "$prefix.aot.exe" --smoke
    if ($LASTEXITCODE -ne 0) { throw 'Module cleanup AOT runtime failed' }
    $boundarySource = 'test/cases/xlir_module_cleanup_boundary.c'
    Copy-Item -LiteralPath release/xlc.dll -Destination build/test/xlc.dll -Force
    & gcc @strict '-DXLIR_BOUNDARY_FAULTS' test/cases/xlir_module_cleanup_api.c $boundarySource release/xlc.dll release/xl.dll -o "$prefix.api.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Module cleanup checked DLL API compile failed' }
    & "$prefix.api.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Module cleanup checked DLL API lost failure/exit-code separation' }
    & release/xl.exe build $source $boundarySource --backend xlir --inc-path . -o "$prefix.boundary.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Module cleanup language-entry AOT build failed' }
    $savedCase = $env:XLIR_MODULE_CLEANUP_CASE
    try {
        # Real allocation faults are exercised before runtime-pool startup
        # by the strict C-host and DLL API. CLI keeps the NULL-result lane.
        foreach ($mode in 0..7) {
            $env:XLIR_MODULE_CLEANUP_CASE = [string]$mode
            foreach ($engine in @('jit', 'aot')) {
                $log = "$prefix.boundary.$mode.$engine.log"
                if ($engine -eq 'jit') {
                    & release/xl.exe run $source $boundarySource --inc-path . *> $log
                } else { & "$prefix.boundary.exe" *> $log }
                $code = $LASTEXITCODE
                $output = [IO.File]::ReadAllText($log)
                if ($code -ne $(if ($mode -eq 0) {0} else {1}) -or
                    $output -notmatch 'module-boundary: all owners closed exactly once') {
                    throw "Module cleanup $engine boundary failed mode $mode, code ${code}: $output"
                }
                if ($mode -in @(4,6) -and $output.Contains('secondary-cleanup-sentinel')) {
                    throw "Module cleanup $engine replaced a missing primary diagnostic"
                }
                if ($mode -eq 5 -and (!$output.Contains('primary-entry-sentinel') -or $output.Contains('secondary-cleanup-sentinel'))) {
                    throw "Module cleanup $engine replaced the primary entry error"
                }
                if ($mode -eq 7 -and (!$output.Contains('primary initialization failure') -or $output.Contains('secondary-cleanup-sentinel'))) {
                    throw "Module cleanup $engine replaced the primary initialization error"
                }
                if ($mode -le 1 -and $output.Contains('uncaught')) {
                    throw "Module cleanup $engine confused an ordinary process exit code with language failure"
                }
            }
        }
    } finally { $env:XLIR_MODULE_CLEANUP_CASE = $savedCase }
    $compilerSources = @('src/ast/xl6_ast.c','src/type/xl6_type_model.c',
        'src/ir/xl6_hir_type.c','src/ir/xl6_hir.c','src/ir/xl6_ir.c','src/ir/xl6_ir_lower.c',
        'src/cgen/xl6_ir_cgen.c','src/cgen/xl6_cgen_shared_runtime.c','src/diag/xl6_diag.c',
        'src/common/xl6_output.c','src/common/xl6_console.c')
    & gcc @strict '-Wno-pointer-sign' '-Wno-format-truncation' test/cases/xlir_module_init_cgen.c @compilerSources release/xl.dll -o "$prefix.contract.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Module cleanup verifier unit build failed' }
    & "$prefix.contract.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Module cleanup verifier mutations failed' }
} finally { Pop-Location }
Write-Output 'XLIR module cleanup outcome passed: inherited/missing first errors, nested native owners, exact release and retry, initialization failure, reentry, deterministic C, verifier mutations, checked DLL API and language/C-host JIT/AOT.'
exit 0
