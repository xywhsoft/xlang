param([string]$Root = '', [Parameter(Mandatory=$true)][string]$SourceDriver, [string]$Directory='')
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'xlir_native_process.ps1')
. (Join-Path $PSScriptRoot 'xlir_source_host_cohort.ps1')
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
Push-Location $Root
try {
    if(!$Directory){$Directory='build/test/xlir_async_module_lifetime_gate'}
    $work=[IO.Path]::GetFullPath($Directory)
    if(Test-Path -LiteralPath $work){throw 'Preserve prior DLL lifetime evidence'}
    New-Item -ItemType Directory -Path $work|Out-Null
    $prefix = Join-Path $work 'xlir_async_module_lifetime'
    $cohort=Get-XlirSourceHostCohort $Root $SourceDriver $work
    $inputs=@($cohort.Inputs)+@($PSCommandPath,$script:XlirNativeProcessImplementation,'test/cases/xlir_async_module_lifetime.xl','test/cases/xlir_async_module_lifetime_host.c','test/cases/xlir_async_module_lifetime_artifact.c'|ForEach-Object {[pscustomobject]@{Path=[IO.Path]::GetFullPath($_);Sha256=(Get-FileHash -LiteralPath $_).Hash}})
    & $SourceDriver test/cases/xlir_async_module_lifetime.xl "$prefix.generated.c" "$prefix.generated.h" "$prefix.generated.json" --script
    if ($LASTEXITCODE -ne 0) { throw 'Joint lifecycle generation failed' }
    & $SourceDriver test/cases/xlir_async_module_lifetime.xl "$prefix.repeat.c" "$prefix.repeat.h" "$prefix.repeat.json" --script
    if ($LASTEXITCODE -ne 0) { throw 'Joint lifecycle repeat generation failed' }
    foreach ($ext in @('c','h','json')) {
        if ((Get-FileHash "$prefix.generated.$ext").Hash -ne (Get-FileHash "$prefix.repeat.$ext").Hash) { throw "Joint nondeterministic $ext" }
    }
    $ir = Get-Content "$prefix.generated.json" -Raw | ConvertFrom-Json
    # Fail safely with an older driver, before the host deliberately unmaps
    # the producer while keeping its payload alive.
    $generated = [IO.File]::ReadAllText("$prefix.generated.c")
    if ($generated.Contains('xlir_async_value_free') -or
        !$generated.Contains('xl6PayloadResolveOwned(destination, value, XL6_FUTURE_PAYLOAD_VALUE)')) {
        throw 'Future payload destructor is not resident; unsafe unload test not executed'
    }
    if ($ir.asyncMachines.Count -ne 1) { throw 'Joint fixture must have one verified machine' }
    [IO.File]::WriteAllText("$prefix.binding.h", "#define JOINT_RUN xlir_async_start_$($ir.asyncMachines[0].targetSymbol)`n", [Text.UTF8Encoding]::new($false))
    $flags = @('-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-Wno-unused-label',
        '-Wno-unused-parameter','-Wno-pointer-sign','-Wno-format-truncation','-Wno-missing-field-initializers','-I.','-Irelease','-Irelease/include','-Ilib/tcc','-Itest/cases',"-I$work","-I$($cohort.IncludeDirectory)",'-DXL_ENABLE_EXE_COMPILER=1')
    $libs=@('-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32')
    $hostFlags=@((New-XlirCStringDefine 'JOINT_DLL_PATH' "$prefix.generated.dll" -RawArgument),(New-XlirCStringDefine 'JOINT_DLL_NAME' 'xlir_async_module_lifetime.generated.dll' -RawArgument))
    $build=Invoke-XlirGccToLog ($flags+@('-O2')+$hostFlags+@('-c','test/cases/xlir_async_module_lifetime_host.c','-o',"$prefix.host.o")) "$work/host.build.log"
    if ($build -ne 0) { throw 'Joint source API host compilation failed' }
    $build=Invoke-XlirGccToLog ($flags+@('-O2',"$prefix.host.o")+@($cohort.Objects)+@('release/xl.dll')+$libs+@("-Wl,--out-implib,$prefix.host.a",'-Wl,--export-all-symbols','-o',"$prefix.exe")) "$work/host.link.log"
    if ($build -ne 0) { throw 'Joint resident runtime host link failed' }
    $build=Invoke-XlirGccToLog ($flags+@('-O2','-shared','test/cases/xlir_async_module_lifetime_artifact.c',"$prefix.host.a",'release/xl.dll','-Wl,--export-all-symbols','-o',"$prefix.generated.dll")) "$work/artifact.build.log"
    if ($build -ne 0) { throw 'Joint generated unloadable DLL link failed' }
    Copy-Item -LiteralPath release/xl.dll -Destination "$work/xl.dll"
    $failed = @()
    $rows=@()
    foreach ($mode in 0..5) {
        $log="$work/mode$mode.log";$code=Invoke-XlirNativeToLog "$prefix.exe" @("$mode") $log
        $complete=[bool](Select-String -LiteralPath $log -SimpleMatch "generated async/source API mode=${mode}: 25 real DLL unloads, pending entry/retirement/cleanup and balanced memory" -Quiet)
        $rows+=[pscustomobject]@{Mode=$mode;Exit=$code;Complete=$complete;Log=$log;Sha256=(Get-FileHash -LiteralPath $log).Hash}
        if ($code -ne 0-or!$complete) { $failed += $mode }
    }
    $unchanged=@($inputs|Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash-ne$_.Sha256}).Count-eq0
    [IO.File]::WriteAllText("$work/results.json",([pscustomobject]@{Complete=(!$failed.Count-and$unchanged);InputsUnchanged=$unchanged;Inputs=$inputs;Rows=$rows;RealLifetimes=150;Deterministic=$true;Scope='Six original source API/DLL lifetime modes, 25 real unloads each, complete verified source dependencies and actual resident payload.'}|ConvertTo-Json -Depth 7),[Text.UTF8Encoding]::new($false))
    if(!$unchanged){throw 'Joint lifecycle inputs changed'}
    if ($failed.Count) { throw "All joint lifecycle modes collected; failed: $($failed -join ', ')" }
} finally { Pop-Location }
Write-Output 'Generated DLL/source API/resident dispatcher lifecycle passed; DLL unmapping is real, TCC relocation and escaped closure/native payload coverage remain separate.'
