param([Parameter(Mandatory=$true)][string]$Directory)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'xlir_native_process.ps1')
$rootPath=Split-Path -Parent $PSScriptRoot
$outPath=[IO.Path]::GetFullPath($Directory)
if(Test-Path -LiteralPath $outPath){throw 'Preserve prior process argument evidence'}
New-Item -ItemType Directory -Path $outPath | Out-Null
Push-Location $rootPath
try{
    $cc=(Get-Command gcc -CommandType Application | Select-Object -First 1).Source
    $paths=@($PSCommandPath,$script:XlirNativeProcessImplementation,$cc,'test/cases/xlir_native_process_define.c')
    $inputs=@($paths | ForEach-Object {[pscustomobject]@{Path=[IO.Path]::GetFullPath($_);Sha256=(Get-FileHash -LiteralPath $_).Hash}})
    $rows=[Collections.Generic.List[object]]::new()
    function Run([string]$Name,[string]$File,[string[]]$Arguments){
        $process=[Diagnostics.Process]::new()
        $process.StartInfo.FileName=$File
        $process.StartInfo.Arguments=Join-XlirWindowsCommandLine $Arguments
        $process.StartInfo.WorkingDirectory=$rootPath
        $process.StartInfo.UseShellExecute=$false
        $process.StartInfo.CreateNoWindow=$true
        $process.StartInfo.RedirectStandardOutput=$true
        $process.StartInfo.RedirectStandardError=$true
        try{
            [void]$process.Start();$stdout=$process.StandardOutput.ReadToEndAsync();$stderr=$process.StandardError.ReadToEndAsync()
            if(!$process.WaitForExit(30000)){Stop-XlirProcessTree $process;throw "Timeout $Name"}
            $code=$process.ExitCode
            [IO.File]::WriteAllText("$outPath/$Name.log",$stdout.Result+$stderr.Result,[Text.UTF8Encoding]::new($false))
        }finally{$process.Dispose()}
        $rows.Add([pscustomobject]@{Name=$Name;Exit=$code;Complete=$code -eq 0})
        if($code){throw "$Name failed: $code"}
    }
    $values=@('','D:/native test/headers','D:\native test\headers\','trailing space ')
    for($index=0;$index -lt $values.Count;++$index){
        $define=New-XlirCStringDefine 'XL6_NATIVE_DEFINE' $values[$index] -RawArgument
        Run "build-$index" $cc @('-std=c11','-Wall','-Wextra','-Werror',$define,'test/cases/xlir_native_process_define.c','-o',"$outPath/value-$index.exe")
        Run "run-$index" "$outPath/value-$index.exe" @($values[$index])
    }
    foreach($invalid in @("contains`"quote","contains`nnewline","contains`rcarriage","contains$([char]0)null")){
        $rejected=$false
        try{[void](New-XlirCStringDefine 'XL6_NATIVE_DEFINE' $invalid -RawArgument)}catch{$rejected=$true}
        if(!$rejected){throw 'Invalid C string transport was admitted'}
    }
    $unchanged=!@($inputs|Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256}).Count
    $report=[pscustomobject]@{Complete=$unchanged;InputsUnchanged=$unchanged;Edition=$PSVersionTable.PSEdition;
        Checks=$rows.Count+4;Inputs=$inputs;Results=$rows;Scope='Real GCC preprocessor and ProcessStartInfo transport; empty, space, backslash and trailing separator values, quote/CR/LF/NUL refusal'}
    [IO.File]::WriteAllText("$outPath/results.json",($report|ConvertTo-Json -Depth 6),[Text.UTF8Encoding]::new($false))
    if(!$unchanged){throw 'Process argument inputs changed'}
    Write-Host "Native process argument contract: $($report.Checks) checks, $($report.Edition)"
}finally{Pop-Location}
