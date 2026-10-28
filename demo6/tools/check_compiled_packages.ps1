param([string]$PublicDirectory='', [string]$RuntimeDirectory='',
      [Parameter(Mandatory=$true)][string]$Directory)
$ErrorActionPreference='Stop'
$root=(Get-Location).Path
if(!$PublicDirectory -and !$RuntimeDirectory){throw 'Select a verified source cohort or a fresh CLI/DLL candidate'}
$public=if($PublicDirectory){(Resolve-Path -LiteralPath $PublicDirectory).Path}else{''}
$runtime=if($RuntimeDirectory){(Resolve-Path -LiteralPath $RuntimeDirectory).Path}else{''}
if(Test-Path -LiteralPath $Directory){throw 'Choose a fresh compiled-package acceptance directory'}
$out=[IO.Path]::GetFullPath($Directory)
New-Item -ItemType Directory -Path $out | Out-Null
$driver=if($runtime){Join-Path $runtime 'xl.exe'}else{Join-Path $public 'public_source.exe'}
if(!(Test-Path -LiteralPath $driver)){throw 'Compiler is missing'}
$cc=(Get-Command gcc -ErrorAction Stop).Source
$rows=[Collections.Generic.List[object]]::new()
function Invoke-Step($name,$program,$arguments,$expected=0) {
    $log=Join-Path $out ($name+'.log')
    [IO.File]::WriteAllText($log,'',[Text.UTF8Encoding]::new($false))
    $savedPreference=$ErrorActionPreference
    try {
        $ErrorActionPreference='Continue'
        & $program @arguments 2>&1 | ForEach-Object {$_.ToString()} | Set-Content -LiteralPath $log -Encoding UTF8
        $code=$LASTEXITCODE
    }
    finally {$ErrorActionPreference=$savedPreference}
    $rows.Add([pscustomobject]@{Name=$name;Exit=$code;Expected=$expected;Log=$log})
    if($code -ne $expected){Get-Content -LiteralPath $log -Tail 35;throw "Failed: $name ($code)"}
}
function Build-Xlang($name,$source,$binary) {
    if($runtime){Invoke-Step $name $driver @('build',$source,'-o',$binary)}
    else {Invoke-Step $name $driver @($source,'exe',[IO.Path]::ChangeExtension($binary,$null))}
    if(!(Test-Path -LiteralPath $binary)){throw "Missing artifact: $binary"}
}
function Reject-Xlang($name,$source,$fragment,[switch]$Run) {
    $arguments=if($runtime) {
        if($Run){@('run',$source)}else{@('build',$source,'-o',(Join-Path $out ($name+'.exe')))}
    }else{@($source,$(if($Run){'run'}else{'exe'}),(Join-Path $out $name))}
    Invoke-Step $name $driver $arguments 1
    if(![IO.File]::ReadAllText((Join-Path $out ($name+'.log'))).Contains($fragment)) {
        throw "Missing required diagnostic: $name"
    }
}
function Write-Response($path,$arguments) {
    $text=@($arguments|ForEach-Object{'"'+([string]$_).Replace('\','/').Replace('"','\"')+'"'}) -join "`n"
    [IO.File]::WriteAllText($path,$text+"`n",[Text.UTF8Encoding]::new($false))
}
$report=[ordered]@{Complete=$false;InputsUnchanged=$false;ShippingAcceptance=$false;SourceAcceptance=$false;Rows=$rows;
    Scope='Compiled-package feature only. No complete stdlib, cross-platform or hostile-native-pointer sandbox claim.'}
$saved=$env:XLANG_PACKAGE_PATH
try {
    $inputs=@((Get-Item -LiteralPath $PSCommandPath),(Get-Item -LiteralPath $cc))+
        @(Get-ChildItem -LiteralPath 'test/cases' -Filter '*compiled_package*' -File)+
        @(Get-ChildItem -LiteralPath 'test/fixtures' -Directory | Where-Object Name -Like 'compiled_*' |
            Get-ChildItem -Recurse -File)+
        @(Get-ChildItem -LiteralPath 'release/examples/embedded_package','release/include' -Recurse -File)+
        @((Get-Item -LiteralPath 'src/ast/xl6_ast.h'))
    if($runtime){$inputs+=@('xl.exe','xl.dll','xlc.dll'|ForEach-Object {Get-Item -LiteralPath (Join-Path $runtime $_)})}
    if($public){$inputs+=@((Get-Item -LiteralPath (Join-Path $public 'build.json')))}
    $report.Inputs=@($inputs | Sort-Object FullName -Unique | ForEach-Object {
        [pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}
    })
    $env:XLANG_PACKAGE_PATH=Join-Path $root 'test/fixtures'
    if($public) {
        $python=(Get-Command python -ErrorAction Stop).Source
        Invoke-Step 'source-receipt' $python @('tools/xlir_public_build_inputs.py','--root',$root,
            '--public',$public,'--out',(Join-Path $out 'source-receipt.json'))
        $build=Get-Content -LiteralPath (Join-Path $public 'build.json') -Raw | ConvertFrom-Json
        $objects=@($build.Objects|Where-Object{[IO.Path]::GetFileNameWithoutExtension($_.Path) -notin
            @('xlir_public_source_driver','xlir_public_runtime_driver')}|ForEach-Object{$_.Path})
        $native=Join-Path $out 'source-native';New-Item -ItemType Directory -Path $native | Out-Null
        Copy-Item -LiteralPath (Join-Path $public 'xl.dll') -Destination $native
        $sourceBinary=Join-Path $native 'ir.exe'
        $arguments=@($build.CompileFlags)+@('test/cases/compiled_package_ir_contract.c')+$objects+
            @((Join-Path $public 'xl.dll'),'-lWs2_32','-lIPHLPAPI','-lShell32','-lOle32','-lAdvapi32','-o',$sourceBinary)
        $response=Join-Path $native 'ir.rsp';Write-Response $response $arguments
        Invoke-Step 'source-ir-build' $cc @('@'+$response)
        $generated=Join-Path $native 'consumer.c'
        Invoke-Step 'source-ir' $sourceBinary @($generated,(Join-Path $out 'metadata.json'))
        foreach($optimization in @('O0','O2')) {
            Invoke-Step ('consumer-strict-'+$optimization) $cc @('-std=c11','-Wall','-Wextra','-Werror',('-'+$optimization),
                '-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label',
                '-Wno-unused-but-set-variable','-Wno-unused-const-variable','-Wno-missing-field-initializers',
                ('-I'+(Join-Path $public 'include')),('-I'+(Join-Path $root 'release/include')),
                '-c',$generated,'-o',(Join-Path $native ($optimization+'.o')))
        }
        $report.SourceAcceptance=$true
    }
    if($runtime) {
        foreach($library in @('xl','xlc')) {
            foreach($optimization in @('O0','O2')) {
                $native=Join-Path $out ($library+'-'+$optimization)
                New-Item -ItemType Directory -Path $native | Out-Null
                $dll=Join-Path $native ($library+'.dll')
                Copy-Item -LiteralPath (Join-Path $runtime ($library+'.dll')) -Destination $dll
                $client=Join-Path $native 'registry.exe'
                Invoke-Step ($library+'-'+$optimization+'-build') $cc @('-std=c11','-Wall','-Wextra','-Werror',('-'+$optimization),
                    ('-I'+$root),('-I'+(Join-Path $root 'release')),
                    'test/cases/compiled_package_registry.c',$dll,'-o',$client)
                $poison=Join-Path $native '__xl_compiled__';New-Item -ItemType Directory -Path $poison | Out-Null
                Copy-Item -LiteralPath 'test/cases/compiled_package_poison.c' -Destination (Join-Path $poison 'leaf.c')
                Push-Location -LiteralPath $native
                try {Invoke-Step ($library+'-'+$optimization+'-registry') $client @($dll,(Join-Path $root 'res/tcc/win64/x86_64'))}
                finally {Pop-Location}
            }
        }
        $native=Join-Path $out 'xlc-O2';$dll=Join-Path $native 'xlc.dll';$client=Join-Path $native 'compiler.exe'
        Invoke-Step 'xlc-consumer-build' $cc @('-std=c11','-Wall','-Wextra','-Werror','-O2',
            ('-I'+(Join-Path $root 'release')),'test/cases/compiled_package_compiler.c',$dll,'-o',$client)
        Push-Location -LiteralPath $native
        try {Invoke-Step 'xlc-consumer' $client @($dll,(Join-Path $root 'res/tcc/win64/x86_64'),(Join-Path $native 'consumer.c'))}
        finally {Pop-Location}
    }
    $hostBinary=Join-Path $out 'host.exe'
    Build-Xlang 'host-build' 'test/cases/compiled_package_host.xl' $hostBinary
    Build-Xlang 'baseline-build' 'test/cases/compiled_package_baseline.xl' (Join-Path $out 'baseline.exe')
    Invoke-Step 'baseline' (Join-Path $out 'baseline.exe') @()
    $baselineSource=[IO.File]::ReadAllText((Join-Path $root 'test/cases/compiled_package_baseline.xl')).Replace("`r`n","`n").Trim()
    $exposureSource=[IO.File]::ReadAllText((Join-Path $root 'test/cases/compiled_package_exposure.xl')).Replace("`r`n","`n").Trim()
    $registration='assert engine.importPackage(xlang.compiledPackage("compiled_demo"))'+"`n"
    if($exposureSource.Replace($registration,'') -cne $baselineSource){throw 'Size controls differ beyond package exposure'}
    $exposureBinary=Join-Path $out 'exposure.exe'
    Build-Xlang 'exposure-build' 'test/cases/compiled_package_exposure.xl' $exposureBinary
    Invoke-Step 'exposure' $exposureBinary @()
    Reject-Xlang 'dynamic-name' 'test/cases/bad_compiled_package_dynamic_name.xl' 'constant package name'
    Reject-Xlang 'missing-import' 'test/cases/bad_compiled_package_missing_import.xl' 'imported by the host'
    Reject-Xlang 'static-state' 'test/cases/bad_compiled_package_static_state.xl' 'mutable static'
    Reject-Xlang 'provider-lifetime' 'test/cases/compiled_package_host.xl' 'process-resident' -Run
    $deploy=Join-Path $out 'deploy'
    New-Item -ItemType Directory -Path $deploy | Out-Null
    Copy-Item -LiteralPath $hostBinary -Destination (Join-Path $deploy 'host.exe')
    Copy-Item -LiteralPath 'test/cases/compiled_package_script.xl' -Destination $deploy
    $env:XLANG_PACKAGE_PATH=''
    Push-Location -LiteralPath $deploy
    try {Invoke-Step 'source-free-host' (Join-Path $deploy 'host.exe') @()}
    finally {Pop-Location}
    $text=[IO.File]::ReadAllText((Join-Path $out 'source-free-host.log'))
    if(!$text.Contains('compiled_package_host=ok') -or !$text.Contains('compiled_package_script=ok')){throw 'Missing execution markers'}
    $files=@(Get-ChildItem -LiteralPath $deploy -File)
    if($files.Count -ne 2){throw 'Source-free deployment contains unexpected dependencies'}
    $env:XLANG_PACKAGE_PATH=Join-Path $root 'test/fixtures/compiled_package_decoy'
    Push-Location -LiteralPath $deploy
    try {Invoke-Step 'registered-before-disk' (Join-Path $deploy 'host.exe') @()}
    finally {Pop-Location}
    $objdump=(Get-Command objdump -ErrorAction Stop).Source
    Invoke-Step 'host-pe' $objdump @('-p',$hostBinary)
    $imports=@([regex]::Matches([IO.File]::ReadAllText((Join-Path $out 'host-pe.log')),'DLL Name:\s*(\S+)')|
        ForEach-Object{$_.Groups[1].Value})
    foreach($name in $imports) {
        if($name -notmatch '^(ADVAPI32|bcrypt|crypt32|GDI32|IPHLPAPI|KERNEL32|msvcrt|ncrypt|ole32|oleaut32|rpcrt4|SHELL32|USER32|USERENV|ucrtbase|VERSION|winmm|WS2_32|WSOCK32|shlwapi)\.dll$') {
            throw "Non-system DLL dependency: $name"
        }
    }
    $env:XLANG_PACKAGE_PATH=Join-Path $root 'release/examples/embedded_package/packages'
    foreach($source in @('release/examples/embedded_package/packages/embedded_math/math.xl',
        'release/examples/embedded_package/packages/embedded_math/native.c')) {
        if(!(Test-Path -LiteralPath $source -PathType Leaf)){throw "Example package source missing: $source"}
    }
    $example=Join-Path $out 'example.exe'
    Build-Xlang 'example-build' 'release/examples/embedded_package/host.xl' $example
    $exampleDeploy=Join-Path $out 'example-deploy';New-Item -ItemType Directory -Path $exampleDeploy | Out-Null
    Copy-Item -LiteralPath $example -Destination (Join-Path $exampleDeploy 'host.exe')
    Copy-Item -LiteralPath 'release/examples/embedded_package/external.xl' -Destination $exampleDeploy
    $env:XLANG_PACKAGE_PATH='';Push-Location -LiteralPath $exampleDeploy
    try {Invoke-Step 'example-source-free' (Join-Path $exampleDeploy 'host.exe') @()}
    finally {Pop-Location}
    $hostBytes=(Get-Item -LiteralPath $exposureBinary).Length;$baselineBytes=(Get-Item -LiteralPath (Join-Path $out 'baseline.exe')).Length
    $report.InputsUnchanged=@($report.Inputs | Where-Object {
        (Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256
    }).Count -eq 0
    if(!$report.InputsUnchanged){throw 'Acceptance inputs changed; rerun against a frozen candidate'}
    $report.Complete=$true;$report.ShippingAcceptance=[bool]$runtime
    $report.HostBytes=$hostBytes;$report.BaselineBytes=$baselineBytes;$report.DeltaBytes=$hostBytes-$baselineBytes
    $report.SizeComparison='baseline/exposure sources differ only by one explicit importPackage(compiledPackage(...)) assertion; source filenames have equal length'
    $report.FunctionalHostBytes=(Get-Item -LiteralPath $hostBinary).Length
    $report.DeltaPercent=[math]::Round(100.0*($hostBytes-$baselineBytes)/$baselineBytes,4)
    $report.SystemDlls=$imports
    if($runtime){$report.Products=@('xl.exe','xl.dll','xlc.dll'|ForEach-Object {
        $file=Join-Path $runtime $_;[pscustomobject]@{Path=$file;Sha256=(Get-FileHash -LiteralPath $file).Hash}})}
    Write-Output 'Compiled package source-free deployment passed.'
} finally {
    $env:XLANG_PACKAGE_PATH=$saved
    $report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $out 'results.json') -Encoding UTF8
}
