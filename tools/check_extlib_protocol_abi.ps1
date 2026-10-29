param(
    [string]$PublicSourceDirectory='',
    [string]$WorkDirectory='build/test/extlib_protocol_abi',
    [string[]]$Package=@('xssh','xhttp.protocol','xhttp.body')
)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'extlib_smoke_process.ps1')
$work=[IO.Path]::GetFullPath((Join-Path $root $WorkDirectory))
if(Test-Path -LiteralPath $work){throw 'Choose a fresh protocol ABI evidence directory'}
[void][IO.Directory]::CreateDirectory($work)
$utf8=[Text.UTF8Encoding]::new($false)
$gcc=(Get-Command gcc -CommandType Application).Source
$xl=Join-Path $root 'release/xl.exe'
$driver=$null; $build=$null; $artifact=$null
if($PublicSourceDirectory){
    $directory=[IO.Path]::GetFullPath((Join-Path $root $PublicSourceDirectory))
    $build=Get-Content (Join-Path $directory 'build.json') -Raw|ConvertFrom-Json
    $artifact=Get-Content (Join-Path $directory 'artifact_sources.json') -Raw|ConvertFrom-Json
    $driver=$build.Driver
    if($build.RuntimeOnly -or (Get-FileHash $driver).Hash -ne $build.DriverSha256 -or $artifact.DriverSha256 -ne $build.DriverSha256){throw 'Invalid source driver'}
    foreach($file in @($build.Inputs)+@($build.Headers)+@($build.Objects)){
        if((Get-FileHash $file.Path).Hash -ne $file.Sha256){throw "Stale source input: $($file.Path)"}
    }
    foreach($file in $artifact.Files){
        if((Get-FileHash $file.Source).Hash -ne $file.Sha256 -or (Get-FileHash $file.Path).Hash -ne $file.Sha256){throw "Stale artifact input: $($file.Path)"}
    }
}
$available=@(
    @{name='xssh'; destructor='xssh_writer_destroy_native'; native='xssh_native.c'},
    @{name='xhttp.protocol'; destructor='xhttp_protocol_headers_destroy'; native='src/xlang/xhttp_protocol_native_headers.c'},
    @{name='xhttp.body'; destructor='xhttp_body_destroy'; native='src/xlang/xhttp_body_native.c'},
    @{name='xhttp'; destructor='xhttp_url_destroy'; native='src/xlang/xhttp_native_url.c'},
    @{name='xhttp.multipart'; destructor='xhttp_multipart_boundary_destroy'; native='src/xlang/xhttp_multipart_native_common.c'},
    @{name='xhttp.form_data'; destructor='xhttp_form_data_destroy'; native='src/xlang/xhttp_form_data_native.c'},
    @{name='xhttp.body_decode'; destructor='xhttp_body_decode_result_destroy'; native='src/xlang/xhttp_body_decode_native.c'},
    @{name='xhttp.body_stream'; destructor='xhttp_body_stream_destroy'; native='src/xlang/xhttp_body_stream_native.c'},
    @{name='xhttp.client'; destructor='xhttp_client_request_destroy'; native='src/xlang/xhttp_client_native_request.c'},
    @{name='xws'; destructor='xws_config_destroy'; native='src/xlang/xws_native_connection.c'}
)
foreach($name in $Package){if($name -notin $available.name){throw "Unknown ABI package: $name"}}
$plans=@($available|Where-Object {$_.name -in $Package})
if(!$plans.Count){throw 'Select at least one ABI package'}
# 包的依赖也参与本次生成，输入快照必须覆盖完整依赖闭包。
$packageInputs=@{}
function IncludePackage([string]$name){
    if($packageInputs.ContainsKey($name)){return}
    $directory=Join-Path $root "release/extlibs/$name"
    $manifest=Get-Content (Join-Path $directory 'xlang.package.json') -Raw|ConvertFrom-Json
    $packageInputs[$name]=$directory
    foreach($dependency in $manifest.dependencies){IncludePackage $dependency.name}
}
foreach($plan in $plans){IncludePackage $plan.name}
$inputs=[ordered]@{}; $checks=[Collections.Generic.List[object]]::new(); $coverage=[Collections.Generic.List[object]]::new()
$environment=@{XLANG_PACKAGE_PATH=(Join-Path $root 'release/extlibs')}
function InputPaths {
    $PSCommandPath; $gcc; $xl
    foreach($directory in $packageInputs.Values){Get-ChildItem $directory -Recurse -File|ForEach-Object FullName}
    foreach($file in @('lib/xrt/xrt.h','lib/xruntime/xruntime.h','release/xl.dll','release/xlc.dll',
        'tools/extlib_smoke_process.ps1','tools/xlir_native_process.ps1','tools/check_extlib_protocol_abi.sh',
        'test/support/extlib_http_object_runtime.c','test/cases/extlib_protocol_native_contract.c')){Join-Path $root $file}
    Get-ChildItem (Join-Path $root 'release/include') -Recurse -File|ForEach-Object FullName
    if($build){$driver; $build.Inputs.Path; $build.Headers.Path; $build.Objects.Path; $artifact.Files.Source; $artifact.Files.Path}
}
foreach($path in @(InputPaths|Sort-Object -Unique)){$inputs[$path]=(Get-FileHash $path).Hash}
function Check([string]$name,[string]$program,[string[]]$arguments){
    $r=Invoke-ExtlibSmokeProcess $program $arguments $root 120 $environment
    $checks.Add([ordered]@{name=$name;program=$program;arguments=$arguments;result=$r})
    Write-Host "[protocol-abi] ${name}: $($r.status)"
    if($r.status -ne 'pass'){throw "$name failed: $($r.stderr)`n$($r.stdout)"}
    return $r.stdout.Replace("`r`n","`n").TrimEnd("`n")
}
function Language([string]$name,[string]$case,[string]$action,[string]$prefix){
    if($driver){
        $mode=@{run='run';build='exe';cgen='emit'}[$action]
        return (Check $name $driver @($case,$mode,$prefix)) -replace '(?:\n)?public source (?:run|exe|emit) complete$',''
    }
    $arguments=@($action,$case)
    if($action -eq 'build'){$arguments+=@('-o',"$prefix.exe")}
    if($action -eq 'cgen'){$arguments+=@('--emit',"$prefix.c")}
    return Check $name $xl $arguments
}
$outcome='failed'; $failure=''; $changed=@(); $httpObjects=@()
try {
    $null=Check 'gcc-version' $gcc @('--version')
    foreach($plan in $plans){
        $name=$plan.name; $pkg="release/extlibs/$name"
        $manifest=Get-Content (Join-Path $root "$pkg/xlang.package.json") -Raw|ConvertFrom-Json
        $symbols=@(foreach($file in $manifest.artifact.xlSources){
            # 只清点外部符号名，类型必须从真正生成的 C 中取得，不另写 ABI 推导器。
            [regex]::Matches((Get-Content (Join-Path $root "$pkg/$file") -Raw),'extern "(\w+)"')|ForEach-Object{$_.Groups[1].Value}
        })|Sort-Object -Unique
        $declarations=@()
        foreach($case in $manifest.coverage.tests){
            $path="$pkg/$case"; $id="$name-$([IO.Path]::GetFileNameWithoutExtension($case))"
            $prefix=Join-Path $work $id
            $jit=Language "$id-jit" $path 'run' $prefix
            $null=Language "$id-build" $path 'build' $prefix
            $aot=Check "$id-aot" "$prefix.exe" @()
            if($jit -cne $aot){throw "$id JIT/AOT output mismatch"}
            $null=Language "$id-cgen" $path 'cgen' $prefix
            $text=Get-Content "$prefix.c" -Raw
            foreach($symbol in $symbols){
                $pattern='(?m)^extern [^\r\n]*\b'+[regex]::Escape($symbol)+'\([^\r\n]*\);\r?$'
                $declarations+=@([regex]::Matches($text,$pattern)|ForEach-Object{$_.Value.Trim()})
            }
            if($driver){
                $formal=Check "$id-formal" $xl @('run',$path)
                if($formal -cne $jit){throw "$id formal/source output mismatch"}
            }
        }
        $declarations=@($declarations|Sort-Object -Unique)
        foreach($symbol in $symbols){
            if(@($declarations|Where-Object{$_ -match ('\b'+[regex]::Escape($symbol)+'\(')}).Count -ne 1){throw "Missing or conflicting generated ABI: $symbol"}
        }
        $header=Join-Path $work "$name-abi.h"
        $preamble="#include <stdbool.h>`n#include <stdint.h>`n#include <stddef.h>`n"
        [IO.File]::WriteAllText($header,$preamble+($declarations -join "`n")+"`n",$utf8)
        $flags=@('-std=c11','-Wall','-Wextra','-Werror','-Ilib/xrt','-Ilib/xruntime','-Irelease/include')
        $flags+=@($manifest.artifact.includeDirs|ForEach-Object{"-I$pkg/$_"})
        $flags+=@($manifest.artifact.xrtModules|ForEach-Object{"-D$_"})
        foreach($source in $manifest.artifact.cSources){
            $null=Check "$name-abi-$source" $gcc ($flags+@('-include',$header,'-fsyntax-only',"$pkg/$source"))
            if($name -in @('xhttp.protocol','xhttp.body')){
                $object=Join-Path $work "$name-$([IO.Path]::GetFileNameWithoutExtension($source)).o"
                $null=Check "$name-object-$source" $gcc ($flags+@('-O2','-include',$header,'-c',"$pkg/$source",'-o',$object))
                $httpObjects+=$object
            }
        }
        # 负控制证明检查器确实能拦住析构返回类型回退，而非仅验证语法可读。
        $mutant=Join-Path $work "$name-negative.h"
        $original=[IO.File]::ReadAllText($header)
        $needle="extern void $($plan.destructor)("
        if(!$original.Contains($needle)){throw "Missing destructor control: $name"}
        [IO.File]::WriteAllText($mutant,$original.Replace($needle,"extern int64_t $($plan.destructor)("),$utf8)
        $arguments=$flags+@('-include',$mutant,'-fsyntax-only',"$pkg/$($plan.native)")
        $r=Invoke-ExtlibSmokeProcess $gcc $arguments $root 120 $environment
        $checks.Add([ordered]@{name="$name-negative-abi";program=$gcc;arguments=$arguments;expected='rejected';result=$r})
        if($r.status -ne 'fail' -or $r.stderr -notmatch 'conflicting types' -or $r.stderr -notmatch $plan.destructor){throw "$name negative ABI control did not reject correctly"}
        $coverage.Add([ordered]@{package=$name;symbols=$symbols;generatedDeclarations=$declarations.Count;
            destructor=$plan.destructor;native=$plan.native;
            header=$header;headerSha256=(Get-FileHash $header).Hash;negativeHeader=$mutant;negativeSha256=(Get-FileHash $mutant).Hash})
    }
    if('xhttp.protocol' -in $plans.name -and 'xhttp.body' -in $plans.name){
    $base=@('-std=c11','-O2','-Wall','-Wextra','-Werror','-Ilib/xrt')
    $runtime=Join-Path $work 'http-runtime.o'
    $null=Check 'http-runtime' $gcc ($base+@('-c','test/support/extlib_http_object_runtime.c','-o',$runtime))
    $test=Join-Path $work 'http-native.exe'
    $null=Check 'http-native-build' $gcc ($base+@('-DXRT_MODULE_IO_BUFFER','-DXRT_MODULE_MEMORY_DEBUG',
        '-include',(Join-Path $work 'xhttp.protocol-abi.h'),'-include',(Join-Path $work 'xhttp.body-abi.h'),
        'test/cases/extlib_protocol_native_contract.c',$runtime)+$httpObjects+@('-lws2_32','-ladvapi32','-lm','-o',$test))
    $null=Check 'http-native-run' $test @()
    }
    $outcome='passed-scoped'
} catch {
    $failure=$_.Exception.Message
} finally {
    $changed=@($inputs.Keys|Where-Object{!(Test-Path -LiteralPath $_) -or (Get-FileHash $_).Hash -ne $inputs[$_]})
    if($changed.Count){$outcome='failed';$failure+='; inputs changed during gate'}
    $record=[ordered]@{date=(Get-Date).ToString('o');outcome=$outcome;failure=$failure;
        scope='All declared native signatures of the selected packages against C implementations; selected JIT/AOT and negative ABI controls. Not full network protocol certification.';
        packages=$plans.name;dependencyPackages=@($packageInputs.Keys|Sort-Object);
        sourceDriver=$driver;inputs=$inputs;changedInputs=$changed;coverage=@($coverage.ToArray());checks=@($checks.ToArray())}
    [IO.File]::WriteAllText((Join-Path $work 'results.json'),($record|ConvertTo-Json -Depth 9),$utf8)
}
if($outcome -ne 'passed-scoped'){throw $failure}
