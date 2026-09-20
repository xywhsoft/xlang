param([string]$Root = '', [string]$SourceRuntimeBuildManifest = '')
$ErrorActionPreference = 'Stop'
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
Push-Location $Root
try {
    $flags = @('-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable',
        '-Wno-unused-parameter','-Wno-pointer-sign','-Wno-format-truncation','-I.','-Irelease','-Ilib/tcc')
    & gcc -std=c11 -Wall -Wextra -Werror -O2 -shared test/cases/xlir_module_retirement_dependency.c -o build/test/xlir_module_retirement_dependency.dll
    if ($LASTEXITCODE -ne 0) { throw 'Retirement dependency fixture build failed' }
    # Compile both public API configurations, even though the executable only
    # retains tested host-boundary paths and injects its TCC symbol lookup.
    foreach ($compiler in @(0,1)) {
        & gcc @flags -DXL_STATIC_EMBED "-DXL_ENABLE_EXE_COMPILER=$compiler" -fsyntax-only src/api/xl_api.c
        if ($LASTEXITCODE -ne 0) { throw "Source API configuration $compiler failed strict compilation" }
    }
    & gcc @flags -O2 -fwhole-program -c test/cases/xlir_module_deactivation_contract.c -o build/test/xlir_module_deactivation.o
    if ($LASTEXITCODE -ne 0) { throw 'Source API deactivation host compilation failed' }
    $runtimeObject = 'build/test/xlir_module_deactivation_xrt.o'
    $runtimeInputs = @()
    if ($SourceRuntimeBuildManifest) {
        $runtimeBuild = Get-Content -LiteralPath $SourceRuntimeBuildManifest -Raw | ConvertFrom-Json
        if (!$runtimeBuild.SourceXrt) { throw 'Module deactivation requires current source XRT' }
        $runtimeObjects = @($runtimeBuild.Objects | Where-Object { [IO.Path]::GetFileName($_.Path) -eq 'xrt_single_header_impl.o' })
        if ($runtimeObjects.Count -ne 1) { throw 'Missing source runtime object' }
        $runtimeObject = $runtimeObjects[0].Path; $runtimeInputs = @($runtimeBuild.Inputs) + $runtimeObjects
        if (@($runtimeInputs | Where-Object { (Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256 }).Count) { throw 'Stale module runtime inputs' }
    } else {
        & gcc @flags -O0 -c tools/xrt_single_header_impl.c -o $runtimeObject
        if ($LASTEXITCODE -ne 0) { throw 'Module deactivation source XRT build failed' }
    }
    & gcc @flags -O2 build/test/xlir_module_deactivation.o src/runtime/xl6_code_lease.c src/runtime/xl6_continuation_queue.c $runtimeObject release/xl.dll -lWs2_32 -lIPHLPAPI -lShell32 -lOle32 -lAdvapi32 -o build/test/xlir_module_deactivation.exe
    if ($LASTEXITCODE -ne 0) { throw 'Source API deactivation contract build failed' }
    & build/test/xlir_module_deactivation.exe
    if ($LASTEXITCODE -ne 0) { throw 'Source API deactivation transaction failed' }
    if (@($runtimeInputs | Where-Object { (Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256 }).Count) { throw 'Runtime changed during module deactivation gate' }
} finally { Pop-Location }
Write-Output 'Module deactivation source API passed; TCC lookup is injected. Retirement has a separate gate; production-generated code-owner binding/dispatch is not yet admitted.'
