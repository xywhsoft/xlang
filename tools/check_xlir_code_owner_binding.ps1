param([string]$Root = '', [string]$SourceDriver = '', [string]$SourceRuntimeBuildManifest = '')
$ErrorActionPreference = 'Stop'
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
Push-Location $Root
try {
    if ($SourceDriver -eq '') {
        $SourceDriver = 'build/test/xlir_code_owner_binding_driver.exe'
        & tools/build_xlir_validation_driver.ps1 -Driver test/cases/xlir_code_owner_binding_contract.c -Output $SourceDriver
        if ($LASTEXITCODE -ne 0) { throw 'Code-owner source driver build failed' }
    }
    & $SourceDriver
    if ($LASTEXITCODE -ne 0) { throw 'Code-owner IR contract failed' }
    $prefix = 'build/test/xlir_code_owner_binding.generated'
    foreach ($pass in @('', '.repeat')) {
        & $SourceDriver test/cases/xlir_code_owner_binding.xl "$prefix$pass.c" "$prefix$pass.h" "$prefix$pass.json" --script
        if ($LASTEXITCODE -ne 0) { throw 'Code-owner verified generation failed' }
    }
    foreach ($ext in @('c','h','json')) {
        if ((Get-FileHash "$prefix.$ext").Hash -ne (Get-FileHash "$prefix.repeat.$ext").Hash) { throw "Nondeterministic $ext" }
    }
    $flags = @('-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable',
        '-Wno-unused-label','-Wno-unused-parameter','-Wno-pointer-sign','-Wno-format-truncation',
        '-Wno-missing-field-initializers','-I.','-Ibuild/test','-Irelease','-Ilib/tcc')
    & gcc @flags -x c -fsyntax-only "$prefix.h"
    if ($LASTEXITCODE -ne 0) { throw 'Generated binding header failed standalone C compilation' }
    & gcc @flags -O2 -fwhole-program -c test/cases/xlir_code_owner_binding_host.c -o build/test/xlir_code_owner_binding_host.o
    if ($LASTEXITCODE -ne 0) { throw 'Generated/source API binding host compilation failed' }
    $runtimeInputs = @()
    $runtimeObject = 'build/test/xlir_code_owner_binding_xrt.o'
    if ($SourceRuntimeBuildManifest) {
        $runtimeBuild = Get-Content -LiteralPath $SourceRuntimeBuildManifest -Raw | ConvertFrom-Json
        if (!$runtimeBuild.SourceXrt) { throw 'Code owner binding requires current source XRT' }
        $runtimeObjects = @($runtimeBuild.Objects | Where-Object { [IO.Path]::GetFileName($_.Path) -eq 'xrt_single_header_impl.o' })
        if ($runtimeObjects.Count -ne 1) { throw 'Missing source runtime object' }
        $runtimeObject = $runtimeObjects[0].Path
        $runtimeInputs = @($runtimeBuild.Inputs) + $runtimeObjects
        if (@($runtimeInputs | Where-Object { (Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256 }).Count) { throw 'Stale runtime build' }
    } else {
        & gcc @flags -O0 -c tools/xrt_single_header_impl.c -o $runtimeObject
        if ($LASTEXITCODE -ne 0) { throw 'Code owner source XRT build failed' }
    }
    & gcc @flags -O2 build/test/xlir_code_owner_binding_host.o test/cases/xlir_code_owner_binding_artifact.c src/runtime/xl6_code_lease.c src/runtime/xl6_continuation_queue.c $runtimeObject release/xl.dll -lWs2_32 -lIPHLPAPI -lShell32 -lOle32 -lAdvapi32 -o build/test/xlir_code_owner_binding_host.exe
    if ($LASTEXITCODE -ne 0) { throw 'Generated binding host link failed' }
    $failed = @()
    foreach ($mode in @('bound','unbound','fail','unbound-fail','extended','traced','legacy-prefix')) {
        & build/test/xlir_code_owner_binding_host.exe $mode
        if ($LASTEXITCODE -ne 0) { $failed += $mode }
    }
    if ($failed.Count) { throw "Code-owner binding modes failed: $($failed -join ', ')" }
    if (@($runtimeInputs | Where-Object { (Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256 }).Count) { throw 'Runtime inputs changed during binding gate' }
} finally { Pop-Location }
Write-Output 'Generated binding/source host/owned ticket contract passed; production async admission and actual JIT unmapping remain unverified.'
