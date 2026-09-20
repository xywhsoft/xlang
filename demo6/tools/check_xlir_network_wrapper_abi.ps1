param([string]$Root = '', [string]$SourceDriver = '', [string]$ReportPath = '')
$ErrorActionPreference = 'Stop'
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
Push-Location $Root
try {
    if ($SourceDriver -eq '') {
        $SourceDriver = 'build/test/xlir_network_wrapper_source_driver.exe'
        & tools/build_xlir_validation_driver.ps1 -Output $SourceDriver
        if ($LASTEXITCODE -ne 0) { throw 'Network wrapper current-source driver build failed' }
    }
    if ($ReportPath -eq '') { $ReportPath = 'build/test/xlir_network_wrapper_abi.results.json' }
    $strict = @('-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-label',
        '-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-const-variable',
        '-Wno-missing-field-initializers','-I.','-Ilib/xrt','-Ibuild/test')
    $results = @()
    foreach ($fixture in @('stdlib_net_fast_path','stdlib_net_send_file','stdlib_tls_server_loopback')) {
        $prefix = "build/test/xlir_network_wrapper_$fixture"
        $failure = $null
        try {
            foreach ($pass in @('', '.repeat')) {
                & $SourceDriver "test/cases/$fixture.xl" "$prefix$pass.c" "$prefix$pass.h" "$prefix$pass.json"
                if ($LASTEXITCODE -ne 0) { throw "Verified generation failed ($pass)" }
            }
            foreach ($extension in @('c','h','json')) {
                if ((Get-FileHash "$prefix.$extension").Hash -ne (Get-FileHash "$prefix.repeat.$extension").Hash) {
                    throw "Nondeterministic $extension"
                }
            }
            & gcc @strict -c "$prefix.c" -o "$prefix.o"
            if ($LASTEXITCODE -ne 0) { throw 'Strict C compilation failed' }
            $c = [IO.File]::ReadAllText("$prefix.c")
            $ir = Get-Content "$prefix.json" -Raw | ConvertFrom-Json
            $targets = @($ir.callTargets | Where-Object cName -eq 'xl6_xrt_net_bytes_destroy')
            if ($targets.Count -ne 1 -or $targets[0].errorAbiName -ne 'may-raise' -or
                $targets[0].errorPhysicalAbiName -ne 'foreign-scope') {
                throw 'NetBytes destructor lost its conservative external failure contract'
            }
            if (!$c.Contains('xlir_net_bytes_handle_drop') -or !$c.Contains('xl6_xrt_net_bytes_destroy(handle);') -or
                !$c.Contains('if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome);')) {
                throw 'Network native ownership callback lost its exact destructor or outcome boundary'
            }
            if ($fixture -eq 'stdlib_tls_server_loopback') {
                if (!$c.Contains('return xlir_net_tls_stream_borrow_handle(value);') -or
                    !$c.Contains('return xlir_net_bytes_borrow_handle(value);')) {
                    throw 'Shared TLS component lost its explicit class payload projections'
                }
                $byteFields = @($ir.classFields | Where-Object nativeDestroySymbol -eq $targets[0].symbol)
                if ($byteFields.Count -ne 1) { throw 'No unique verified NetBytes field binding' }
                # The JSON dump identifies the exact field but omits its name;
                # copy the emitted C literal for this test-only borrowed sentinel.
                $fieldLiteral = [regex]::Match($c, '(?s)static void\* xlir_net_bytes_borrow_handle\(.*?xrtStrView\(("(?:\\.|[^"\\])*")\)')
                if (!$fieldLiteral.Success) { throw 'Missing emitted NetBytes field projection' }
                $binding = '#define XLIR_TLS_BYTES_FIELD ' + $fieldLiteral.Groups[1].Value
                [IO.File]::WriteAllText((Join-Path (Get-Location) "$prefix.binding.h"), $binding, [Text.UTF8Encoding]::new($false))
                & gcc @strict -include "$prefix.c" -include "$prefix.binding.h" test/cases/xlir_tls_payload_projection_host.c release/xl.dll -lWs2_32 -lIPHLPAPI -lShell32 -o "$prefix.projection.exe"
                if ($LASTEXITCODE -ne 0) { throw 'TLS projection strict C link failed' }
                & "$prefix.projection.exe"
                if ($LASTEXITCODE -ne 0) { throw 'TLS payload projection/allocation outcome gate failed' }
            }
        } catch { $failure = $_.Exception.Message }
        $results += [pscustomobject]@{ Fixture=$fixture; Passed=($null -eq $failure); Failure=$failure }
        $report = [pscustomobject]@{ DriverSha256=(Get-FileHash $SourceDriver).Hash; Results=$results; Scope='verified C/H/IR generation, determinism and strict C; not network runtime admission' }
        [IO.File]::WriteAllText((Join-Path (Get-Location) $ReportPath), ($report|ConvertTo-Json -Depth 5), [Text.UTF8Encoding]::new($false))
        Write-Output "$fixture passed=$($null -eq $failure) $failure"
    }
    if (@($results | Where-Object { !$_.Passed }).Count) { throw 'Network wrapper batch contains failures; all fixtures were collected' }
} finally { Pop-Location }
Write-Output 'Network wrapper ABI passed: current-source C/header/IR, deterministic generation, exact native destructor effects and strict C. Runtime/link admission remains separate.'
