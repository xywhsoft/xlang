param([string]$Root = '', [string]$Directory = '')
$ErrorActionPreference = 'Stop'
if (!$Root) { $Root = Split-Path -Parent $PSScriptRoot }
$Root = (Resolve-Path -LiteralPath $Root).Path
. (Join-Path $Root 'tools/xlir_native_process.ps1')
if (!$Directory) { $Directory = Join-Path $Root ('build/test/gcc-response-' + [Guid]::NewGuid().ToString('N')) }
$out = [IO.Path]::GetFullPath($Directory)
if (Test-Path -LiteralPath $out) { throw 'Preserve prior response-file evidence' }
[IO.Directory]::CreateDirectory($out) | Out-Null
$fixture = Join-Path $Root 'test/cases/xlir_native_response_contract.c'
$inputs = @($PSCommandPath,$script:XlirNativeProcessImplementation,$fixture,(Get-Command gcc -CommandType Application | Select-Object -First 1).Source) |
    ForEach-Object { [ordered]@{Path=$_;Sha256=(Get-FileHash -LiteralPath $_).Hash} }
$unicode = -join ([char]0x6D4B,[char]0x8BD5)
$values = @('', 'path with spaces', 'D:\backslash path\', 'trailing space ', 'embedded "quote" and backslash \', $unicode)
$rows = @(); $rejected = 0; $previous = Get-Location
try {
    Set-Location $Root
    for ($index=0; $index -lt 10; ++$index) {
        $name = "case-$index"; $binary = Join-Path $out "$name.exe"; $log = Join-Path $out "$name.build.log"
        $value = if ($index -lt 6) { $values[$index] } else { 'long arguments' }
        $literal = '"' + $value.Replace('\','\\').Replace('"','\"') + '"'
        $expected = [Text.Encoding]::UTF8.GetBytes($value)
        if ($index -eq 6) { $literal='"a\0b"'; $expected=[byte[]]@(97,0,98) }
        $flags = @('-std=c11','-Wall','-Wextra')
        if ($index -ne 8) { $flags += '-Werror' }
        $define = '-DXL6_NATIVE_DEFINE=' + $literal
        if ($index -lt 4 -or $index -eq 5) {
            $fromHelper = New-XlirCStringDefine 'XL6_NATIVE_DEFINE' $value -RawArgument
            if ($fromHelper -cne $define) { throw 'Raw C-string helper changed its logical define' }
            $define = $fromHelper
        }
        $arguments = $flags + @($define)
        if ($index -eq 7) {
            $arguments += @(0..3999 | ForEach-Object { "-DXL6_RESPONSE_PADDING_$_=$_" })
            $arguments += '-DXL6_EXPECTED_PADDING=4000'
        }
        if ($index -eq 8) { $arguments += '-DXL6_RESPONSE_WARNING=1' }
        if ($index -eq 9) { $arguments += '-DXL6_RESPONSE_INTENTIONAL_ERROR=1' }
        $arguments += @($fixture,'-o',$binary)
        $logicalLength = (Join-XlirWindowsCommandLine $arguments).Length
        if ($index -eq 7 -and $logicalLength -le 32767) { throw 'The actual long command did not exceed the Windows limit' }
        if ($index -ge 8) {
            $native = Invoke-XlirGccToLog $arguments $log -PassThru
            $code = $native.exitCode
            $expectedStatus = if ($index -eq 8) {'pass'} else {'fail'}
            if ($native.status -cne $expectedStatus -or !$native.stderr -or $native.stdout) { throw 'Structured compiler result changed native streams or status' }
        } else { $code = Invoke-XlirGccToLog $arguments $log }
        $receipt = [IO.File]::ReadAllText($log+'.command.json', [Text.Encoding]::UTF8) | ConvertFrom-Json
        if (!$receipt.Complete -or $receipt.Exit -ne $code -or @($receipt.Arguments).Count -ne $arguments.Count) { throw 'Incomplete compiler command receipt' }
        for ($argumentIndex=0; $argumentIndex -lt $arguments.Count; ++$argumentIndex) {
            if ($receipt.Arguments[$argumentIndex] -cne $arguments[$argumentIndex]) { throw "Logical compiler argument changed at $argumentIndex" }
        }
        $run = $null
        if ($index -eq 9) {
            if (!$code -or ![IO.File]::ReadAllText($log).Contains('intentional-response-compile-failure')) { throw 'Compiler failure was lost' }
        } else {
            if ($code) { throw "Response case $index did not compile: $code" }
            if ($index -eq 8 -and ![IO.File]::ReadAllText($log).Contains('response-stderr-marker')) { throw 'Successful native stderr was lost' }
            $run = Invoke-XlirNativeToLog $binary @() (Join-Path $out "$name.run.log")
            $actual = [IO.File]::ReadAllText((Join-Path $out "$name.run.log")).Trim()
            $hex = -join @($expected | ForEach-Object { $_.ToString('x2') })
            if ($run -ne 0 -or $actual -cne ($expected.Length.ToString()+':'+$hex)) { throw "Response case $index changed C literal bytes" }
        }
        $rows += [ordered]@{Name=$name;Build=$code;Run=$run;LogicalArguments=$arguments.Count;LogicalCommandLength=$logicalLength;CommandReceipt=$log+'.command.json';Passed=$true}
    }
    foreach ($bad in @("bad$([char]0)argument","bad`nargument","bad`rargument",'@nested.rsp')) {
        $refused=$false
        try { [void](Invoke-XlirGccToLog @($bad) (Join-Path $out "rejected-$rejected.log")) } catch { $refused=$true }
        if (!$refused) { throw 'Invalid response argument was admitted' }
        if (Test-Path -LiteralPath (Join-Path $out "rejected-$rejected.log.rsp")) { throw 'Rejected arguments wrote a response file' }
        ++$rejected
    }
    $refused=$false
    try { [void](Invoke-XlirGccToLog @('--version') (Join-Path $out 'case-0.build.log')) } catch { $refused=$true }
    if (!$refused) { throw 'Existing compiler evidence was overwritten' }
    ++$rejected
    foreach ($record in $inputs) {
        if ((Get-FileHash -LiteralPath $record.Path).Hash -cne $record.Sha256) { throw "Response input changed: $($record.Path)" }
    }
    $result=[ordered]@{Complete=$true;Edition=$PSVersionTable.PSEdition;Inputs=@($inputs);Cases=$rows;Rejected=$rejected;
        LongCommandLength=$rows[7].LogicalCommandLength;ActualGccExecutions=10;ActualProgramsRun=9;
        FullParameterizedAccepted=$false;FullGoalComplete=$false}
    [IO.File]::WriteAllText((Join-Path $out 'results.json'),($result | ConvertTo-Json -Depth 7),[Text.UTF8Encoding]::new($false))
    Write-Output "GCC response transport: ten actual builds, nine exact-byte executions, five refusals; $($PSVersionTable.PSEdition)"
} finally { Set-Location $previous }
