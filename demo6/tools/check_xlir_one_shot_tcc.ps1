param([string]$Root = '', [Parameter(Mandatory=$true)][string]$SourceDriver, [string]$Directory='')
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'xlir_native_process.ps1')
. (Join-Path $PSScriptRoot 'xlir_source_host_cohort.ps1')
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
Push-Location $Root
try {
    if(!$Directory){$Directory='build/test/xlir_one_shot_tcc_gate'}
    $work=[IO.Path]::GetFullPath($Directory)
    if(Test-Path -LiteralPath $work){throw 'Preserve previous one-shot TCC evidence'}
    New-Item -ItemType Directory -Path $work|Out-Null
    $cohort=Get-XlirSourceHostCohort $Root $SourceDriver $work
    $inputs=@($cohort.Inputs)+@($PSCommandPath,$script:XlirNativeProcessImplementation,
        'test/cases/xlir_one_shot_lifetime.xl','test/cases/xlir_one_shot_lifetime_host.c',
        'test/cases/xlir_one_shot_lifetime_artifact.c','test/cases/xlir_async_module_lifetime_host.c',
        'test/cases/xlir_async_module_lifetime_artifact.c','test/cases/xlir_one_shot_tcc_host.c'|ForEach-Object {
            [pscustomobject]@{Path=[IO.Path]::GetFullPath($_);Sha256=(Get-FileHash -LiteralPath $_).Hash}})
    $prefix = Join-Path $work 'xlir_one_shot_lifetime'
    foreach ($pass in @('generated','repeat')) {
        & $SourceDriver test/cases/xlir_one_shot_lifetime.xl "$prefix.$pass.c" "$prefix.$pass.h" "$prefix.$pass.json" --script
        if ($LASTEXITCODE -ne 0) { throw 'TCC joint source generation failed' }
    }
    foreach ($ext in @('c','h','json')) {
        if ((Get-FileHash "$prefix.generated.$ext").Hash -ne (Get-FileHash "$prefix.repeat.$ext").Hash) { throw "TCC joint nondeterministic $ext" }
    }
    $ir = Get-Content "$prefix.generated.json" -Raw | ConvertFrom-Json
    if ($ir.asyncMachines.Count -ne 1) { throw 'TCC joint canonical machine missing' }
    [IO.File]::WriteAllText("$prefix.binding.h", "#define JOINT_RUN xlir_async_start_$($ir.asyncMachines[0].targetSymbol)`n", [Text.UTF8Encoding]::new($false))
    $flags = @('-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter',
        '-Wno-unused-label','-Wno-pointer-sign','-Wno-format-truncation','-Wno-missing-field-initializers',
        '-I.','-Irelease','-Irelease/include','-Ilib/tcc','-Itest/cases',"-I$work","-I$($cohort.IncludeDirectory)",'-DXL_ENABLE_EXE_COMPILER=1')
    $libs=@('-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32')
    $defines=@('-DJOINT_SOURCE_XRT',
        (New-XlirCStringDefine 'JOINT_TCC_ARTIFACT_DIRECTORY' $work.Replace('\','/') -RawArgument),
        (New-XlirCStringDefine 'JOINT_TCC_INCLUDE_DIRECTORY' $cohort.IncludeDirectory.Replace('\','/') -RawArgument))
    $code=Invoke-XlirGccToLog ($flags+@('-O2')+$defines+@('-c','test/cases/xlir_one_shot_tcc_host.c','-o',"$prefix.host.o")) "$work/host.build.log"
    if($code){throw 'TCC joint source host build failed'}
    $code=Invoke-XlirGccToLog ($flags+@('-O2',"$prefix.host.o")+@($cohort.Objects)+@('release/xl.dll')+$libs+@('-o',"$prefix.exe")) "$work/host.link.log"
    if($code){throw 'TCC joint complete source resident link failed'}
    Copy-Item -LiteralPath release/xl.dll -Destination "$work/xl.dll"
    $failed = @()
    $rows=@()
    foreach ($mode in 0..6) {
        $log="$work/mode$mode.log";$code=Invoke-XlirNativeToLog "$prefix.exe" @("$mode") $log
        $complete=[bool](Select-String -LiteralPath $log -Pattern "^one-shot mode=${mode}: 25 .*MEM_FREE.*checked status, bound entry, globals after async, balanced memory$" -Quiet)
        $rows+=[pscustomobject]@{Mode=$mode;Exit=$code;Complete=$complete;Log=$log;Sha256=(Get-FileHash -LiteralPath $log).Hash}
        if($code -or !$complete){$failed+=$mode}
    }
    $unchanged=@($inputs|Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256}).Count -eq 0
    [IO.File]::WriteAllText("$work/results.json",([pscustomobject]@{Complete=(!$failed.Count-and$unchanged);InputsUnchanged=$unchanged;
        Inputs=$inputs;Rows=$rows;RealLifetimes=175;Deterministic=$true;Scope='All seven original one-shot modes, 25 actual TCC compile/relocate/MEM_FREE cycles each, complete verified source cohort and explicit private artifact directory.'}|ConvertTo-Json -Depth 7),[Text.UTF8Encoding]::new($false))
    if(!$unchanged){throw 'One-shot TCC inputs changed'}
    if ($failed.Count) { throw "All actual TCC modes collected; failed: $($failed -join ', ')" }
} finally { Pop-Location }
Write-Output 'Actual TCC compilation/relocation/page deletion joined with source module retirement; compiler driver/current runtime source plus released TCC/environment, not same-source release admission.'
