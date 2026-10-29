param([string]$Root = '', [Parameter(Mandatory=$true)][string]$SourceDriver,
    [string]$SourceRuntimeBuildManifest = '', [string]$Directory = '')
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'xlir_native_process.ps1')
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
Push-Location $Root
try {
    $prefix = 'build/test/xlir_object_escape'
    $outputPath = [IO.Path]::GetFullPath('build/test')
    if ($Directory) {
        $outputPath = [IO.Path]::GetFullPath($Directory)
        if (Test-Path -LiteralPath $outputPath) { throw 'Choose a fresh object escape directory' }
        New-Item -ItemType Directory -Path $outputPath | Out-Null
        Copy-Item -LiteralPath release/xl.dll -Destination (Join-Path $outputPath 'xl.dll')
        $prefix = Join-Path $outputPath 'xlir_object_escape'
    }
    if (!$SourceRuntimeBuildManifest) { throw 'A complete source cohort is required; no release-provider fallback for native-catalog retirement' }
    $SourceRuntimeObject = ''
    $runtimeInputs = @()
    if ($SourceRuntimeBuildManifest) {
        $runtimeBuild = Get-Content -LiteralPath $SourceRuntimeBuildManifest -Raw | ConvertFrom-Json
        if (!$runtimeBuild.SourceXrt) { throw 'Object gate requires a current source XRT build' }
        $runtimeInputs = @($runtimeBuild.Inputs)
        $runtimeObjects = @($runtimeBuild.Objects | Where-Object { [IO.Path]::GetFileName($_.Path) -eq 'xrt_single_header_impl.o' })
        if ($runtimeObjects.Count -ne 1) { throw 'Source XRT object is ambiguous or missing' }
        $runtimeInputs += @($runtimeBuild.Headers)+@($runtimeBuild.Objects)
        if (@($runtimeInputs | Where-Object { (Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256 }).Count) { throw 'Source XRT build is stale' }
        $SourceRuntimeObject = $runtimeObjects[0].Path
    }
    $runtimeInclude = Join-Path $outputPath '__xl_runtime'
    New-Item -ItemType Directory -Force -Path $runtimeInclude | Out-Null
    Copy-Item -LiteralPath release/include/xruntime_decl.h -Destination (Join-Path $runtimeInclude 'xruntime_decl.h') -Force
    if ($SourceRuntimeBuildManifest) {
        foreach ($header in $runtimeBuild.Headers) {
            Copy-Item -LiteralPath $header.Path -Destination (Join-Path $runtimeInclude ([IO.Path]::GetFileName($header.Path))) -Force
        }
    }
    foreach ($pass in @('generated','repeat')) {
        & $SourceDriver test/cases/xlir_object_escape.xl "$prefix.$pass.c" "$prefix.$pass.h" "$prefix.$pass.json" --script
        if ($LASTEXITCODE -ne 0) { throw 'Object escape source generation failed' }
    }
    foreach ($ext in @('c','h','json')) {
        if ((Get-FileHash "$prefix.generated.$ext").Hash -ne (Get-FileHash "$prefix.repeat.$ext").Hash) { throw "Object escape nondeterministic $ext" }
    }
    $files=@($PSCommandPath,$SourceDriver,$SourceRuntimeBuildManifest,'release/xl.dll',
        'test/cases/xlir_object_escape.xl','test/cases/xlir_object_escape_tcc_host.c',
        'test/cases/xlir_object_escape_artifact.c','test/cases/xlir_class_retirement_outcome_tcc_host.c',
        'test/cases/xlir_one_shot_tcc_host.c','test/cases/xlir_async_module_lifetime_host.c',
        'src/api/xl_api.c','src/api/xl_api_path.c','src/common/xl6_console.c','tools/xlir_native_process.ps1')
    $files+=@('src/runtime/xl6_array_erased.h','src/runtime/xl6_array_value_read.h')
    $runtimeInputs+=@($files|ForEach-Object { $p=(Resolve-Path -LiteralPath $_).Path; [pscustomobject]@{Path=$p;Sha256=(Get-FileHash -LiteralPath $p).Hash} })
    $flags = @('-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter',
        '-Wno-pointer-sign','-Wno-format-truncation','-Wno-missing-field-initializers',"-I$outputPath",'-I.','-Irelease','-Irelease/include','-Ilib/tcc',
        (New-XlirCStringDefine 'JOINT_TCC_INCLUDE_DIRECTORY' $outputPath.Replace('\','/') -RawArgument))
    # The included actual host API/registrar refers to the full resident
    # symbol table. Keep exactly the source cohort, including the compiler
    # bodies retained at O0; do not guess a changing runtime source subset.
    $excluded=@('xl_api.o','xl_api_path.o','xl6_console.o','xlir_public_source_driver.o')
    $dependencies=@($runtimeBuild.Objects | Where-Object {[IO.Path]::GetFileName($_.Path) -notin $excluded} | ForEach-Object Path)
    $names = @('closure','class','container','detached-child','internal-only','trivial-value',
        'shared-global-callable','shared-global-class','shared-global-cloned-container','shared-global-child')
    $results = @()
    foreach ($lane in @('guarded','automatic','outcome-O0','outcome-O2')) {
        $defines=@('-DJOINT_SOURCE_XRT','-DXL_ENABLE_EXE_COMPILER=1');$hostSource='test/cases/xlir_object_escape_tcc_host.c';$level='-O2'
        if($lane -eq 'automatic'){$defines+='-DJOINT_ESCAPE_AUTOMATIC'}
        if($lane.StartsWith('outcome')){$hostSource='test/cases/xlir_class_retirement_outcome_tcc_host.c';$level="-$($lane.Substring(8))"}
        $buildExit=Invoke-XlirGccToLog ($flags+$defines+@($level,'-c',$hostSource,'-o',"$prefix.$lane.host.o")) "$prefix.$lane.build.log"
        if(!$buildExit){
            $buildExit=Invoke-XlirGccToLog ($flags+@($level,"$prefix.$lane.host.o")+$dependencies+@('release/xl.dll','-lWs2_32','-lIPHLPAPI','-lShell32','-lOle32','-lAdvapi32','-o',"$prefix.$lane.exe")) "$prefix.$lane.link.log"
        }
        $modes=if($lane.StartsWith('outcome')){0..3}else{0..9}
        foreach ($mode in $modes) {
            $log = "$prefix.$lane.mode$mode.log";$code=$null;$complete=$false
            if(!$buildExit){
                & "$prefix.$lane.exe" $mode *> $log; $code = $LASTEXITCODE
                $marker=if($lane.StartsWith('outcome')){"class retirement outcome mode=${mode}: 25 real TCC lifetimes, 75 once-only finalizers"}
                    else{"object escape mode=${mode}: transactions=25 automatic_owner_missing=0;"}
                $complete=$code -eq 0 -and [bool](Select-String -LiteralPath $log -SimpleMatch $marker -Quiet)
            }
            $results += [pscustomobject]@{Lane=$lane;Mode=$mode;Name=$names[$mode];Build=$buildExit;Exit=$code;Complete=$complete;Log=$log}
            Write-Output "$lane mode=$mode build=$buildExit exit=$code complete=$complete"
        }
    }
    $report = [pscustomobject]@{RecordedAt=(Get-Date).ToString('o');Results=$results;Inputs=$runtimeInputs;
        DriverSha256=(Get-FileHash $SourceDriver).Hash;TccRuntimeSha256=(Get-FileHash release/xl.dll).Hash;
        SourceRuntimeSha256=(Get-FileHash $SourceRuntimeObject).Hash;
        SourceRuntimeBuildManifest=$SourceRuntimeBuildManifest;
        Scope='Current source XRT/compiler/resident host with real released TCC allocation. Original guarded 10x25 retained with exact external-root observation; additive unguarded 10x25 automatic lifetimes; four semantic retirement failure modes x25 on host O0/O2. No default backend switch, Linux, release/VFS rebuild or arbitrary transitive native admission.'}
    [IO.File]::WriteAllText([IO.Path]::GetFullPath("$prefix.results.json"), ($report | ConvertTo-Json -Depth 6), [Text.UTF8Encoding]::new($false))
    if (@($runtimeInputs | Where-Object { (Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256 }).Count) { throw 'Source XRT inputs changed during object gate' }
    if (@($results | Where-Object {!$_.Complete}).Count) { throw 'All original and additive modes collected; inspect build/runtime failures in results JSON.' }
} finally { Pop-Location }
