$ErrorActionPreference='Stop'
$target=Join-Path $PSScriptRoot 'check_trees.ps1'
function Verify-Gate([string]$path) {
    $bytes=[IO.File]::ReadAllBytes($path)
    if($bytes.Length -lt 3 -or $bytes[0] -ne 239 -or $bytes[1] -ne 187 -or $bytes[2] -ne 191){throw 'XML gate requires UTF8 BOM for PowerShell 5.1'}
    $tokens=$null;$errors=$null
    $ast=[Management.Automation.Language.Parser]::ParseFile($path,[ref]$tokens,[ref]$errors)
    if($errors.Count){throw 'XML gate parse errors'}
    $calls=@($ast.FindAll({param($n) $n -is [Management.Automation.Language.CommandAst] -and $n.GetCommandName() -eq 'Run' -and $n.CommandElements.Count -gt 1 -and $n.CommandElements[1].Value -eq 'SAX public Reader type'},$true))
    if($calls.Count -ne 1){throw 'XML mandatory Reader call missing'}
    if(![IO.File]::ReadAllText($path).Contains("throw 'mandatory public type check was not executed'")){throw 'XML terminal check guard missing'}
}
Verify-Gate $target
$work=Join-Path ([IO.Path]::GetTempPath()) ('xml-gate-encoding-'+[guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($work)|Out-Null
try {
    $text=[IO.File]::ReadAllText($target)
    $cases=@(@('no-bom',$text,[Text.UTF8Encoding]::new($false),'requires UTF8 BOM'),
        @('missing-call',$text.Replace("Run 'SAX public Reader type'","Run 'skipped Reader test'"),[Text.UTF8Encoding]::new($true),'Reader call missing'))
    foreach($case in $cases){
        $path=Join-Path $work ($case[0]+'.ps1')
        [IO.File]::WriteAllText($path,$case[1],$case[2])
        $rejected=$false
        try { Verify-Gate $path } catch { $rejected=$_.Exception.Message.Contains($case[3]) }
        if(!$rejected){throw 'XML gate encoding control was not rejected'}
    }
} finally {
    foreach($name in @('no-bom.ps1','missing-call.ps1')){[IO.File]::Delete((Join-Path $work $name))}
    [IO.Directory]::Delete($work)
}
Write-Output 'XML gate encoding: UTF8 BOM, mandatory Reader call, 2 controls, PASS'
