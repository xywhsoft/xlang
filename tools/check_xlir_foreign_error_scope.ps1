param([string]$Root = '')
$ErrorActionPreference = 'Stop'
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
Push-Location $Root
try {
    # Use one runtime DLL from two separately linked clients. A generated
    # module-local TLS implementation would fail this test.
    if (!(Test-Path -LiteralPath build/test/xl.dll) -or
        (Get-FileHash release/xl.dll).Hash -ne (Get-FileHash build/test/xl.dll).Hash) {
        Copy-Item -LiteralPath release/xl.dll -Destination build/test/xl.dll -Force
    }
    $strictFlags = @('-std=c11', '-Wall', '-Wextra', '-Werror',
        '-Wno-unused-function', '-Wno-unused-label', '-Wno-unused-variable',
        '-Wno-unused-parameter', '-Wno-unused-const-variable', '-Wno-missing-field-initializers',
        '-I.', '-Ibuild/test')
    & gcc -std=c11 -Wall -Wextra -Werror -I. -Irelease -shared test/cases/xlir_foreign_error_peer.c release/xl.dll -o build/test/xlir_foreign_error_peer.dll
    if ($LASTEXITCODE -ne 0) { throw 'Foreign error peer DLL compile failed' }
    & gcc -std=c11 -Wall -Wextra -Werror -I. -Irelease test/cases/xlir_foreign_error_runtime.c build/test/xlir_foreign_error_peer.dll release/xl.dll -o build/test/xlir_foreign_error_runtime.exe
    if ($LASTEXITCODE -ne 0) { throw 'Foreign error runtime unit compile failed' }
    & build/test/xlir_foreign_error_runtime.exe
    if ($LASTEXITCODE -ne 0) { throw 'Foreign error shared-runtime transaction contract failed' }
    & gcc -std=c11 -Wall -Wextra -Werror -I. -Irelease test/cases/xlir_cleanup_error_scope_runtime.c build/test/xlir_foreign_error_peer.dll release/xl.dll -o build/test/xlir_cleanup_error_scope_runtime.exe
    if ($LASTEXITCODE -ne 0) { throw 'Cleanup error boundary strict C compile failed' }
    & build/test/xlir_cleanup_error_scope_runtime.exe
    if ($LASTEXITCODE -ne 0) { throw 'Cleanup and foreign nested boundary runtime contract failed' }
    # Verify the same first-failure latch through runtime symbol registration
    # and embedded-source linkage, not only through the freshly rebuilt DLL.
    & release/xl.exe run test/cases/xlir_cleanup_error_scope_runtime.c test/cases/xlir_foreign_error_peer.c --inc-path . -- --smoke
    if ($LASTEXITCODE -ne 0) { throw 'Cleanup first-failure C-host JIT contract failed' }
    & release/xl.exe build test/cases/xlir_cleanup_error_scope_runtime.c test/cases/xlir_foreign_error_peer.c src/api/xl_foreign_error.c --inc-path . -o build/test/xlir_cleanup_error_scope_runtime.aot.exe
    if ($LASTEXITCODE -ne 0) { throw 'Cleanup first-failure standalone AOT build failed' }
    & build/test/xlir_cleanup_error_scope_runtime.aot.exe --smoke
    if ($LASTEXITCODE -ne 0) { throw 'Cleanup first-failure standalone AOT runtime failed' }
    $fixtures = [ordered]@{
        cgen_xcall_error_bridge = 'test/cases/cgen_xcall_error_bridge_host.c'
        xlir_foreign_scope_nested = 'test/cases/xlir_foreign_scope_nested_host.c'
        xlir_foreign_raw_callback_error = 'test/cases/xlir_foreign_raw_callback_error_host.c'
    }
    foreach ($name in $fixtures.Keys) {
        $source = "test/cases/$name.xl"
        $hostSource = $fixtures[$name]
        $prefix = "build/test/$name.foreign-scope"
        & release/xl.exe ir $source --lowered --format json --emit "$prefix.json"
        if ($LASTEXITCODE -ne 0) { throw "Foreign scope lowering failed: $name" }
        $ir = [IO.File]::ReadAllText("$prefix.json") | ConvertFrom-Json
        $targets = @($ir.callTargets | Where-Object { $_.cName -match '^(foreign_scope_|foreign_raw_|xcall_error_bridge_)' })
        if ($targets.Count -lt 2 -or @($targets | Where-Object {
            $_.errorAbiName -ne 'may-raise' -or $_.errorPhysicalAbiName -ne 'foreign-scope'
        }).Count -ne 0) { throw 'Unknown foreign targets must have explicit reentrant error effects' }
        if ($name -eq 'xlir_foreign_raw_callback_error' -and
            @($ir.closureTargets | Where-Object { $_.rawCallbackAbiName -eq 'foreign-scope-thunk' }).Count -lt 7) {
            throw 'May-raise raw C callbacks must have an explicit foreign-scope thunk ABI'
        }
        & release/xl.exe cgen $source --backend xlir --emit "$prefix.c"
        if ($LASTEXITCODE -ne 0) { throw "Foreign scope Cgen failed: $name" }
        & release/xl.exe run $source $hostSource
        if ($LASTEXITCODE -ne 0) { throw "Foreign scope JIT runtime failed: $name" }
        & gcc @strictFlags "$prefix.c" $hostSource test/cases/xlir_foreign_scope_entry.c release/xl.dll -o "$prefix.separate.exe"
        if ($LASTEXITCODE -ne 0) { throw "Foreign scope separate strict C compilation failed: $name" }
        & "$prefix.separate.exe"
        if ($LASTEXITCODE -ne 0) { throw "Foreign scope separate strict C runtime failed: $name" }
        & release/xl.exe build $source $hostSource --backend xlir -o "$prefix.aot.exe"
        if ($LASTEXITCODE -ne 0) { throw "Foreign scope standalone AOT build failed: $name" }
        & "$prefix.aot.exe"
        if ($LASTEXITCODE -ne 0) { throw "Foreign scope standalone AOT runtime failed: $name" }
    }
    & release/xl.exe cgen test/cases/cgen_c_callback_interop.xl --backend xlir --emit build/test/xlir_c_callback_interop.c
    if ($LASTEXITCODE -ne 0) { throw 'Existing raw callback interoperability lowering regressed' }
    & gcc @strictFlags test/cases/xlir_c_callback_interop_host.c release/xl.dll -o build/test/xlir_c_callback_interop.foreign-scope.exe
    if ($LASTEXITCODE -ne 0) { throw 'Existing raw callback interoperability strict C regressed' }
    & build/test/xlir_c_callback_interop.foreign-scope.exe
    if ($LASTEXITCODE -ne 0) { throw 'Existing raw callback interoperability runtime regressed' }
} finally { Pop-Location }
Write-Output 'XLIR foreign scope passed: shared runtime isolation, allocation-independent first failure, preserved original errors, nested handled callbacks, saved no-argument callbacks, five raw C result ABIs, strict separate C linkage, cleanup C-host JIT and standalone AOT.'
exit 0
