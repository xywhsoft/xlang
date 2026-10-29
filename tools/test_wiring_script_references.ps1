$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'wiring_script_references.ps1')
$temporary=Join-Path ([IO.Path]::GetTempPath()) ('xlang_wiring_'+[guid]::NewGuid().ToString('N'))
$root=Join-Path $temporary 'fixture'
[void][IO.Directory]::CreateDirectory((Join-Path $root 'tools'))
$utf8=[Text.UTF8Encoding]::new($false)
function Write-Fixture($name,$text) { [IO.File]::WriteAllText((Join-Path $root $name),$text,$utf8) }
try {
    Write-Fixture 'tools/a.ps1' '& (Join-Path $PSScriptRoot ''b.py'') # "unreferenced.ps1"'
    Write-Fixture 'tools/b.py' "from child import run`n# import orphan`n"
    Write-Fixture 'tools/child.py' "run=1`nresource='payload.json'`nescape='../../outside.ps1'`n"
    Write-Fixture 'tools/orphan.py' "run=2`n"
    Write-Fixture 'tools/payload.json' '{"source":"real_fixture.c"}'
    Write-Fixture 'tools/unreferenced.ps1' 'throw "must never be executed or included"'
    [void][IO.Directory]::CreateDirectory((Join-Path $root 'release/extlibs/package/test'))
    [void][IO.Directory]::CreateDirectory((Join-Path $root 'build'))
    Write-Fixture 'entry.bat' "@echo off`npowershell -File tools/a.ps1`npowershell -File release/extlibs/package/test/check.ps1`npython build/report.json`n"
    Write-Fixture 'release/extlibs/package/test/check.ps1' '$data=''package.json'''
    Write-Fixture 'release/extlibs/package/test/package.json' '{"fixture":"native_probe.c"}'
    Write-Fixture 'build/report.json' '{"inputs":["tools/unreferenced.ps1"]}'
    [IO.File]::WriteAllText((Join-Path $temporary 'outside.ps1'),'throw "outside root"',$utf8)
    $paths=@(Get-WiringScriptClosure $root @('tools/a.ps1') | ForEach-Object {
        $_.Path.Substring($root.Length+1).Replace('\','/')
    } | Sort-Object)
    $expected=@('tools/a.ps1','tools/b.py','tools/child.py','tools/payload.json')
    if (@(Compare-Object $expected $paths).Count) { throw ('Wrong runner closure: '+($paths -join ',')) }
    $paths=@(Get-WiringScriptClosure $root @('entry.bat') | ForEach-Object {
        $_.Path.Substring($root.Length+1).Replace('\','/')
    } | Sort-Object)
    $expected+=@('entry.bat','release/extlibs/package/test/check.ps1','release/extlibs/package/test/package.json')
    if (@(Compare-Object $expected $paths).Count) { throw ('Wrong batch/package closure: '+($paths -join ',')) }
    Write-Output 'Runner reference graph: ASTs, unquoted batch/package references, JSON, comment, generated-report and root-escape controls passed.'
} finally {
    $resolved=[IO.Path]::GetFullPath($temporary)
    $parent=[IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\')+'\'
    if (!$resolved.StartsWith($parent,[StringComparison]::OrdinalIgnoreCase) -or
        [IO.Path]::GetFileName($resolved) -notlike 'xlang_wiring_*') { throw 'Unsafe temporary cleanup path' }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
