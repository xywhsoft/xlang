param([string]$Root = "")
$ErrorActionPreference = "Stop"
if ($Root -eq "") { $Root = Split-Path -Parent $PSScriptRoot }
$xl = Join-Path $Root "release\xl.exe"
$fixtures = @("xlir_module_finalization", "xlir_module_native_finalization", "xlir_module_scalar_finalization")
$hosts = @{
    xlir_module_finalization = "test\cases\xlir_module_finalization_host.c"
    xlir_module_native_finalization = "test\cases\xlir_module_native_finalization_host.c"
    xlir_module_scalar_finalization = "test\cases\xlir_module_scalar_finalization_host.c"
}
foreach ($name in $fixtures) {
    $source = Join-Path $Root "test\cases\$name.xl"
    $prefix = Join-Path $Root "build\test\$name"
    foreach ($repeat in @("", ".repeat")) {
        & $xl ir $source --format json --emit "$prefix$repeat.typed.json"
        if ($LASTEXITCODE -ne 0) { throw "$name typed IR failed" }
        & $xl ir $source --lowered --format json --emit "$prefix$repeat.json"
        if ($LASTEXITCODE -ne 0) { throw "$name lowered IR failed" }
        & $xl cgen $source --backend xlir --emit "$prefix$repeat.c"
        if ($LASTEXITCODE -ne 0) { throw "$name Cgen failed" }
    }
    foreach ($extension in @("typed.json", "json", "c")) {
        if ([IO.File]::ReadAllText("$prefix.$extension") -cne
            [IO.File]::ReadAllText("$prefix.repeat.$extension")) {
            throw "$name non-deterministic $extension"
        }
    }
    $typed = [IO.File]::ReadAllText("$prefix.typed.json") | ConvertFrom-Json
    $ir = [IO.File]::ReadAllText("$prefix.json") | ConvertFrom-Json
    if ($typed.moduleFinalizerFunction -ne 0 -or $typed.moduleFinalizerCleanupCount -ne 0) {
        throw "$name typed module contains executable finalization"
    }
    $functions = @($ir.functions | Where-Object { $_.kindName -eq "module-fini" })
    if ($functions.Count -ne 1 -or $ir.moduleFinalizerFunction -lt 1 -or
        $ir.functions[$ir.moduleFinalizerFunction - 1].kindName -ne "module-fini" -or
        $functions[0].errorAbi -ne $(if ($ir.moduleFinalizerCleanupCount -ne 0) {'may-raise'} else {'no-raise'})) {
        throw "$name has no unique effect-bearing finalizer ABI"
    }
    $fini = $functions[0]
    $body = @($ir.instructions | Where-Object {
        $_.index -ge $fini.firstInstruction -and
        $_.index -lt $fini.firstInstruction + $fini.instructionCount
    })
    if ($ir.moduleFinalizerCleanupCount -eq 0) {
        if ($body.Count -ne 1 -or $body[0].kindName -ne "return") {
            throw "$name owner-free boundary must preserve error state without runtime calls"
        }
    } elseif ($body[0].kindName -ne "cleanup-inherit-begin" -or $body[-4].kindName -ne "cleanup-end" -or
        $body[-3].kindName -ne 'error-test' -or $body[-3].operands[0] -ne $body[-4].result -or
        $body[-2].kindName -ne 'throw' -or $body[-1].kindName -ne "return") {
        throw "$name has no inherited first-failure transaction and explicit outcome CFG"
    }
    $owners = @($body | Where-Object { $_.kindName -in @("release", "cell-release", "load") })
    if ($owners.Count -ne $ir.moduleFinalizerCleanupCount -or
        @($owners.targetSymbol | Select-Object -Unique).Count -ne $owners.Count) {
        throw "$name finalizer lost or duplicated a global owner"
    }
    $symbols = @($owners.targetSymbol)
    $expected = @($ir.storages | Where-Object { $_.kindName -eq "global" -and $_.symbol -in $symbols })
    [array]::Reverse($expected)
    if ($expected.Count -ne $owners.Count) { throw "$name finalizer cleans non-global storage" }
    for ($i = 0; $i -lt $owners.Count; ++$i) {
        if ($owners[$i].targetSymbol -ne $expected[$i].symbol) {
            throw "$name global cleanup is not in reverse initialization order"
        }
    }
    $generated = [IO.File]::ReadAllText("$prefix.c")
    if ($generated -match '__xl6_user_global_|<__xl_runtime/xl_runtime\.h>|\(\{') {
        throw "$name finalization must not use legacy tracing or statement expressions"
    }
    Write-Output "$name verified finalizer: $($owners.Count) exact global owners, reverse order and error preservation"
}
Push-Location $Root
try {
    # A default-null function global still has a RELEASE ABI even when no
    # closure was constructed. Dependency discovery must follow that opcode.
    & $xl cgen test\cases\cgen_function_default.xl --backend xlir --emit build\test\xlir_module_empty_callable.c
    if ($LASTEXITCODE -ne 0) { throw "Default callable module emission failed" }
    & gcc -std=c11 -Wall -Wextra -Werror -Wno-unused-function -Wno-unused-label -Wno-unused-variable -Wno-unused-parameter -Wno-unused-const-variable -Wno-missing-field-initializers -fsyntax-only build\test\xlir_module_empty_callable.c
    if ($LASTEXITCODE -ne 0) { throw "Default callable RELEASE lost its runtime helper dependency" }
    $command = 'call test\suites\common.bat'
    foreach ($name in $fixtures) {
        $command += ' && "!XL6_CGEN_CC!" !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror ' +
            $hosts[$name] + ' !XL6_CGEN_LINKS! -o build\test\' + $name + '.strict.exe && build\test\' + $name + '.strict.exe'
    }
    $command += ' && "!XL6_CGEN_CC!" !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -o build\test\xlir_module_init_cgen.exe test\cases\xlir_module_init_cgen.c src\ast\xl6_ast.c src\type\xl6_type_model.c src\ir\xl6_hir_type.c src\ir\xl6_hir.c src\ir\xl6_ir.c src\ir\xl6_ir_lower.c src\cgen\xl6_ir_cgen.c src\cgen\xl6_cgen_shared_runtime.c src\diag\xl6_diag.c src\common\xl6_output.c src\common\xl6_console.c !XL6_CGEN_LINKS! && build\test\xlir_module_init_cgen.exe'
    & $env:ComSpec /d /v:on /c $command
    if ($LASTEXITCODE -ne 0) { throw "Module finalization strict-C lifecycle/allocation/verifier gate failed" }
    $cliSource = Join-Path $Root "test\cases\xlir_module_finalization_cli.xl"
    $expectedOutput = "module-ready`n2`n1"
    $jitOutput = @(& $xl run $cliSource)
    if ($LASTEXITCODE -ne 0 -or ($jitOutput -join "`n").Trim() -cne $expectedOutput) {
        throw "Module finalization JIT host did not release globals after main: $jitOutput"
    }
    & $xl build $cliSource -o build\test\xlir_module_finalization_cli.exe
    if ($LASTEXITCODE -ne 0) { throw "Module finalization AOT build failed" }
    $aotOutput = @(& .\build\test\xlir_module_finalization_cli.exe)
    if ($LASTEXITCODE -ne 0 -or ($aotOutput -join "`n").Trim() -cne $expectedOutput) {
        throw "Module finalization AOT host did not release globals after main: $aotOutput"
    }
    & $env:ComSpec /d /c 'release\xl.exe run test\cases\xlir_module_finalization_cli_error.xl > build\test\xlir_module_finalization_cli_error.jit.log 2>&1'
    if ($LASTEXITCODE -ne 1) { throw "Module finalization JIT lost the language-error exit status: $LASTEXITCODE" }
    $failure = [IO.File]::ReadAllText((Join-Path $Root 'build\test\xlir_module_finalization_cli_error.jit.log'))
    if (!$failure.Contains('module-main-failure-sentinel') -or $failure -notmatch '(?m)^2\r?\n1\r?$') {
        throw "Module finalization JIT lost reverse destruction or original error: $failure"
    }
    & $xl build test\cases\xlir_module_finalization_cli_error.xl -o build\test\xlir_module_finalization_cli_error.exe
    if ($LASTEXITCODE -ne 0) { throw "Throwing module AOT build failed" }
    & $env:ComSpec /d /c 'build\test\xlir_module_finalization_cli_error.exe > build\test\xlir_module_finalization_cli_error.aot.log 2>&1'
    if ($LASTEXITCODE -ne 1) { throw "Module finalization AOT lost the language-error exit status: $LASTEXITCODE" }
    $failure = [IO.File]::ReadAllText((Join-Path $Root 'build\test\xlir_module_finalization_cli_error.aot.log'))
    if (!$failure.Contains('module-main-failure-sentinel') -or $failure -notmatch '(?m)^2\r?\n1\r?$') {
        throw "Module finalization AOT lost reverse destruction or original error: $failure"
    }
} finally { Pop-Location }
Write-Output "XLIR module finalization contract passed: deterministic typed/lowered IR/C, strict C, global/static/collection/closure/native owners, partial init, reentry, retry, allocation balance, nine verifier/Cgen mutations and JIT/AOT host unload."
exit 0
