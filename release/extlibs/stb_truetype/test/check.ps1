param([switch]$SkipAot)
$ErrorActionPreference='Stop'
[Console]::OutputEncoding=[Text.UTF8Encoding]::new($false)
$root=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
. (Join-Path $root 'tools/extlib_smoke_process.ps1')
$gcc=(Get-Command gcc -CommandType Application -ErrorAction Stop|Select-Object -First 1).Source
$pkg='release/extlibs/stb_truetype';$xl=Join-Path $root 'release/xl.exe';$cc1=''
$parent=Join-Path $root 'build/test';$work=Join-Path $parent ('stbtt_guard_'+[guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($work)|Out-Null
$temporary=Join-Path $work 'temp';[IO.Directory]::CreateDirectory($temporary)|Out-Null
$environment=@{TEMP=$temporary;TMP=$temporary;XLANG_PACKAGE_PATH=(Join-Path $root 'release/extlibs')}
$utf8=[Text.UTF8Encoding]::new($false);$checks=[Collections.Generic.List[object]]::new();$issues=[Collections.Generic.List[string]]::new()
$hashes=[ordered]@{};$passed=$false;$failure=''
$flags=@('-std=c11','-O2','-Wall','-Wextra','-Werror','-Ilib/xrt',"-I$pkg/include")
function Inputs {
    Get-ChildItem (Join-Path $root $pkg),(Join-Path $root 'release/include') -Recurse -File|ForEach-Object FullName
    @('lib/xrt/xrt.h','lib/xruntime/xruntime.h','release/xl.exe','release/xl.dll','release/xlc.dll','tools/extlib_smoke_process.ps1','tools/xlir_native_process.ps1',
        'test/cases/extlib_stb_truetype.xl','release/examples/extlibs/stb_truetype.xl','release/examples/extlibs/stb_truetype_c.c')|ForEach-Object {Join-Path $root $_}
    $gcc;(Get-Process -Id $PID).Path
    if($cc1){$cc1}
}
foreach($path in @(Inputs|Sort-Object -Unique)){$hashes[$path]=(Get-FileHash -LiteralPath $path).Hash}
function Run([string]$Name,[string]$Program,[string[]]$Arguments,[int]$Expected=0,[bool]$Collect=$false){
    if($null -eq $Arguments){$Arguments=@()}
    $r=Invoke-ExtlibSmokeProcess $Program $Arguments $root 120 $environment
    $checks.Add([ordered]@{name=$Name;program=$Program;arguments=$Arguments;expected=$Expected;result=$r})
    if($r.exitCode -ne $Expected){
        if($Collect){$issues.Add("$Name expected $Expected actual=$($r.exitCode)")}
        else{throw "$Name failed: $($r.status) exit=$($r.exitCode)`n$($r.stderr)`n$($r.stdout)"}
    }
    Write-Host "[stbtt] $Name exit=$($r.exitCode) expected=$Expected"
    return $r.stdout.Replace("`r`n","`n").TrimEnd("`n")
}
function Golden([string]$Text,[string]$Path){
    if($Text -cne [IO.File]::ReadAllText((Join-Path $root $Path)).Replace("`r`n","`n").TrimEnd("`n")){throw "Golden mismatch: $Path"}
}
Push-Location $root
try{
    $null=Run compiler $gcc @('--version')
    $cc1=Run cc1 $gcc @('-print-prog-name=cc1');$hashes[$cc1]=(Get-FileHash -LiteralPath $cc1).Hash
    $runtime=Join-Path $work 'runtime.o';$native=Join-Path $work 'native.o'
    $null=Run runtime $gcc ($flags+@('-c',"$pkg/test/xrt_runtime.c",'-o',$runtime))
    $null=Run native $gcc ($flags+@('-Wmissing-prototypes','-c',"$pkg/stb_truetype_native.c",'-o',$native))
    foreach($name in @('test_stb_truetype','discovery_contract_c')){
        $binary=Join-Path $work "$name.exe";$objects=if($name -eq 'test_stb_truetype'){@($native,$runtime)}else{@($runtime)}
        $null=Run "$name-compile" $gcc ($flags+@("$pkg/test/$name.c")+$objects+@('-lm','-o',$binary))
        $normal=Run "$name-GCC" $binary @()
        if($name -eq 'discovery_contract_c'){
            if((Run "$name-TCC" $xl @('run',"$pkg/test/$name.c")) -cne $normal){throw 'Discovery GCC/TCC mismatch'}
            if(!$SkipAot){
                $binary=Join-Path $work "$name-aot.exe";$null=Run "$name-build" $xl @('build',"$pkg/test/$name.c",'-o',$binary)
                if((Run "$name-AOT" $binary @()) -cne $normal){throw 'Discovery GCC/AOT mismatch'}
            }
        }
    }
    $font=Join-Path $root "$pkg/test/assets/DejaVuSans-test.ttf"
    $example=Join-Path $work 'example.exe'
    $null=Run 'old C example compile' $gcc ($flags+@('release/examples/extlibs/stb_truetype_c.c',$runtime,'-lm','-o',$example))
    $normal=Run 'old C example' $example @($font)
    if((Run 'old C direct' $xl @('run','release/examples/extlibs/stb_truetype_c.c','--',$font)) -cne $normal){throw 'Old C direct mismatch'}
    if((Run 'old XL example' $xl @('run','release/examples/extlibs/stb_truetype.xl','--',$font)) -cne $normal){throw 'Old XL example mismatch'}
    $mask=Join-Path $work 'mask.exe'
    $null=Run 'mask compile' $gcc ($flags+@("$pkg/examples/mask_c.c",$native,$runtime,'-lm','-o',$mask))
    Golden (Run 'mask GCC' $mask @($font)) "$pkg/examples/mask.expected.txt"
    Golden (Run 'mask TCC' $xl @('run',"$pkg/examples/mask_c.c",'--',$font)) "$pkg/examples/mask.expected.txt"
    foreach($source in @('test/cases/extlib_stb_truetype.xl',"$pkg/test/boundaries.xl","$pkg/test/allocations.xl","$pkg/test/api_surface.xl","$pkg/examples/mask.xl")){
        $name=[IO.Path]::GetFileNameWithoutExtension($source);$text=Run "$name-JIT" $xl @('run',$source)
        if($name -eq 'mask'){Golden $text "$pkg/examples/mask.expected.txt"}
        if(!$SkipAot){
            $binary=Join-Path $work "$name.exe";$null=Run "$name-build" $xl @('build',$source,'-o',$binary)
            if((Run "$name-AOT" $binary @()) -cne $text){throw "$name JIT/AOT mismatch"}
        }
    }
    if(!$SkipAot){
        foreach($source in @('release/examples/extlibs/stb_truetype_c.c','release/examples/extlibs/stb_truetype.xl',"$pkg/examples/mask_c.c")){
            $name=[IO.Path]::GetFileNameWithoutExtension($source);$binary=Join-Path $work "$name.exe"
            $null=Run "$name-build" $xl @('build',$source,'-o',$binary)
            $text=Run "$name-AOT" $binary @($font)
            if($name -eq 'mask_c'){Golden $text "$pkg/examples/mask.expected.txt"}elseif($text -cne $normal){throw 'Old AOT example mismatch'}
        }
    }
    $module=Join-Path $work 'module.c';$null=Run cgen $xl @('cgen',"$pkg/test/api_surface.xl",'--emit',$module)
    $decls=@(Select-String -LiteralPath $module -Pattern '^extern .*xstbtt_[a-z_]+\('|ForEach-Object Line|Sort-Object -Unique)
    $expected=@(Select-String -LiteralPath "$pkg/src/stb_truetype.xl" -Pattern 'extern "(xstbtt_[a-z_]+)"'|ForEach-Object {$_.Matches.Groups[1].Value}|Sort-Object -Unique)
    $actual=@($decls|ForEach-Object {[regex]::Match($_,'xstbtt_[a-z_]+').Value}|Sort-Object -Unique)
    if($decls.Count -ne 25 -or @(Compare-Object $expected $actual).Count){throw 'Incomplete generated ABI'}
    $abi=Join-Path $work 'abi.c';[IO.File]::WriteAllText($abi,"#include `"stb_truetype_native.h`"`n"+($decls -join "`n"),$utf8)
    $null=Run 'generated ABI' $gcc ($flags+@('-fsyntax-only',$abi))
    [IO.File]::WriteAllText($abi,([IO.File]::ReadAllText($abi).Replace('double xstbtt_font_scale','int xstbtt_font_scale')),$utf8)
    $null=Run 'wrong-return control' $gcc ($flags+@('-fsyntax-only',$abi)) 1
    $text=Run 'private bitmap method' $xl @('check',"$pkg/test/private_bitmap_negative.xl") 1
    $last=$checks[$checks.Count-1].result
    if(($last.stdout+$last.stderr) -notmatch 'bitmapNative' -or ($last.stdout+$last.stderr) -notmatch 'access'){throw 'Wrong private method diagnostic'}
    foreach($type in @('font','bitmap')){
        $null=Run "private $type constructor" $xl @('check',"$pkg/test/private_$($type)_constructor_negative.xl") 1 $true
        $last=$checks[$checks.Count-1].result
        if($last.exitCode -ne 0 -and (($last.stdout+$last.stderr) -notmatch $type -or ($last.stdout+$last.stderr) -notmatch 'access')){throw 'Wrong private constructor diagnostic'}
    }
    if($issues.Count){throw ('stbtt release blockers: '+($issues -join '; '))}
    $passed=$true
}catch{$failure=$_.Exception.Message;throw}finally{
    $changed=@($hashes.Keys|Where-Object {!(Test-Path -LiteralPath $_) -or (Get-FileHash -LiteralPath $_).Hash -ne $hashes[$_]})
    $changed+=@(Inputs|Where-Object {!$hashes.Contains($_)})
    if($changed.Count){$passed=$false}
    [IO.File]::WriteAllText(($work+'.json'),([ordered]@{passed=$passed;failure=$failure;issues=$issues.ToArray();skipAot=[bool]$SkipAot;checks=$checks.ToArray();inputs=$hashes;changedInputs=$changed}|ConvertTo-Json -Depth 12),$utf8)
    Pop-Location
    if(!$work.StartsWith([IO.Path]::GetFullPath($parent)+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Unsafe test cleanup'}
    if($passed){Remove-Item -LiteralPath $work -Recurse -Force}else{Write-Warning "stbtt failure artifacts: $work"}
    Write-Host "[stbtt] report: $work.json"
}
if(!$passed){throw 'stbtt gate failed'}
