param([Parameter(Mandatory=$true)][string]$PublicDirectory,
    [Parameter(Mandatory=$true)][string]$Directory)
$ErrorActionPreference='Stop'
$manifest=Get-Content -LiteralPath (Join-Path $PublicDirectory 'build.json') -Raw | ConvertFrom-Json
$outputPath=if ([IO.Path]::IsPathRooted($Directory)) { [IO.Path]::GetFullPath($Directory) }
    else { [IO.Path]::GetFullPath((Join-Path (Get-Location) $Directory)) }
if (Test-Path -LiteralPath $outputPath) { throw 'Preserve prior public generator evidence' }
$cases=@('cgen_generator','xlir_generator_ownership_public','sem_task_spawn_return_generator')
$filePaths=@($PSCommandPath)+@($cases | ForEach-Object {"test/cases/$_.xl"})
$inputs=@($manifest.Inputs)+@($manifest.Headers)+@([pscustomobject]@{Path=$manifest.Driver;Sha256=$manifest.DriverSha256})+
    @(Get-Item -LiteralPath $filePaths | ForEach-Object {
        [pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}
    })
if (@($inputs | Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256}).Count) { throw 'Stale public generator inputs' }
New-Item -ItemType Directory -Path $outputPath | Out-Null
$results=@()
foreach ($case in $cases) {
    $prefix=Join-Path $outputPath $case; $phases=@()
    foreach ($phase in @('emit','repeat','run','module')) {
        $mode=if ($phase -eq 'repeat') {'emit'} else {$phase}
        $target=if ($phase -eq 'repeat') {"$prefix.repeat"} else {$prefix}
$process=Start-Process -FilePath $manifest.Driver -ArgumentList @("test/cases/$case.xl",$mode,$target) -WindowStyle Hidden -PassThru `
            -RedirectStandardOutput "$prefix.$phase.log" -RedirectStandardError "$prefix.$phase.err.log"
        $null = $process.Handle
        if ($process.WaitForExit(45000)) { $code=$process.ExitCode } else { $process.Kill(); $process.WaitForExit(); $code=-999 }
        $complete=[bool](Select-String -LiteralPath "$prefix.$phase.log" -SimpleMatch "public source $mode complete" -Quiet)
        $phases+=[pscustomobject]@{Name=$phase;Exit=$code;Complete=$complete}
        $process.Dispose()
    }
    $deterministic=@($phases | Where-Object {$_.Name -in @('emit','repeat') -and ($_.Exit -ne 0 -or !$_.Complete)}).Count -eq 0
    if ($deterministic) { foreach ($suffix in @('c','h','json')) {
        if ((Get-FileHash "$prefix.$suffix").Hash -ne (Get-FileHash "$prefix.repeat.$suffix").Hash) { $deterministic=$false }
    } }
    $results+=[pscustomobject]@{Fixture=$case;Phases=$phases;Deterministic=$deterministic}
    Write-Output "$case deterministic=$deterministic phases=$($phases.Exit -join ',')"
}
$unchanged=@($inputs | Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256}).Count -eq 0
$report=[pscustomobject]@{RecordedAt=(Get-Date).ToString('o');Inputs=$inputs;InputsUnchanged=$unchanged;Results=$results;
    Scope='Current-source public default Cgen, deterministic C/header/IR, xlcRun and relocated-module execution of original generator assertions, nested/yielded-error/async consumer paths, and retained generator payload transfer through task.spawn/Future/AWAIT. Does not replace packaged CLI/DLL/AOT or automatic module-lifetime gates.'}
[IO.File]::WriteAllText("$outputPath/results.json",($report | ConvertTo-Json -Depth 7),[Text.UTF8Encoding]::new($false))
if (!$unchanged -or @($results | Where-Object {!$_.Deterministic}).Count -or
    @($results.Phases | Where-Object {$_.Exit -ne 0 -or !$_.Complete}).Count) { throw 'Public generator source batch failed; inspect all phases' }
