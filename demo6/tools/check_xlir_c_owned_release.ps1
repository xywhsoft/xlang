param([string]$ReleaseDirectory='release',
    [string]$Directory=('build/c-owned/release-'+(Get-Date -Format 'yyyyMMdd-HHmmss')))
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$work=if([IO.Path]::IsPathRooted($Directory)){[IO.Path]::GetFullPath($Directory)}else{Join-Path $root $Directory}
if(Test-Path -LiteralPath $work){throw 'Use a fresh release acceptance directory'}
[IO.Directory]::CreateDirectory($work)|Out-Null
$release=if([IO.Path]::IsPathRooted($ReleaseDirectory)){$ReleaseDirectory}else{Join-Path $root $ReleaseDirectory}
$compiler=(Resolve-Path -LiteralPath (Join-Path $release 'xl.exe')).Path
$checks=[Collections.Generic.List[object]]::new()
function Invoke-ReleaseCheck([string]$Name,[string]$Program,[string[]]$StepArguments) {
    $output=@(& $Program @StepArguments 2>&1)
    $code=$LASTEXITCODE
    $output|Set-Content -LiteralPath "$work/$Name.log" -Encoding utf8
    $checks.Add([ordered]@{name=$Name;exitCode=$code;log="$Name.log"})
    if($code){throw "Release check failed: $Name ($code): $output"}
    Write-Output "Release C owner $Name passed"
}
Push-Location $root
try {
    foreach($name in @('variadic_results','variadic_inputs','nested_results','text_length','spawn_keywords','nullable_callables','partial_results')) {
        $source="test/cases/xlir_c_owned_$name.xl"
        Invoke-ReleaseCheck "$name-check" $compiler @('check',$source)
        Invoke-ReleaseCheck "$name-jit" $compiler @('run',$source,'test/cases/xlir_c_owned_native.c','--backend','xlir')
        Invoke-ReleaseCheck "$name-build" $compiler @('build',$source,'test/cases/xlir_c_owned_native.c','-o',"$work/$name.exe",'--backend','xlir')
        Invoke-ReleaseCheck "$name-exe" "$work/$name.exe" @()
    }
    foreach($name in @('stdlib_task_spawn_keywords','sem_task_spawn_dict_vararg')) {
        $source="test/cases/$name.xl"
        Invoke-ReleaseCheck "$name-jit" $compiler @('run',$source,'--backend','xlir')
        Invoke-ReleaseCheck "$name-build" $compiler @('build',$source,'-o',"$work/$name.exe",'--backend','xlir')
        Invoke-ReleaseCheck "$name-exe" "$work/$name.exe" @()
        Invoke-ReleaseCheck "$name-cgen" $compiler @('cgen',$source,'--emit',"$work/$name.c",'--backend','xlir')
        if([IO.File]::ReadAllText("$work/$name.c").Contains('__xl_runtime/xl6_c_owned.h')) {
            throw "Ordinary keyword task acquired C owner machinery: $name"
        }
    }
} finally {
    [ordered]@{compiler=$compiler;checks=@($checks.ToArray());
        binaries=@('xl.exe','xlc.dll','xl.dll'|ForEach-Object {
            $path=Join-Path $release $_
            [ordered]@{path=$path;sha256=(Get-FileHash -LiteralPath $path).Hash}
        })}|ConvertTo-Json -Depth 6|Set-Content -LiteralPath "$work/results.json" -Encoding utf8
    Pop-Location
}
