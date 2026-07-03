param(
    [string]$Root = "",
    [int]$Repeat = 1,
    [int]$TimeoutSeconds = 180,
    [switch]$IncludeHeavy
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$tmpDir = Join-Path $Root "release\tmp"
$failures = New-Object System.Collections.Generic.List[string]
$strictUtf8 = New-Object System.Text.UTF8Encoding($false, $true)

Set-Location $Root

function Add-Failure($message)
{
    $script:failures.Add($message) | Out-Null
}

function Read-CapturedText($name, $path)
{
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        Add-Failure "$name output file is missing: $path"
        return ""
    }

    try {
        $bytes = [System.IO.File]::ReadAllBytes($path)
        $text = $script:strictUtf8.GetString($bytes)
    } catch {
        Add-Failure "$name output is not strict UTF-8: $path"
        return ""
    }

    if ( $text.IndexOf([char]0xFFFD) -ge 0 ) {
        Add-Failure "$name output contains UTF-8 replacement character: $path"
    }

    if ( $text -match "CategoryInfo|FullyQualifiedErrorId|NativeCommandError" ) {
        Add-Failure "$name output was wrapped by PowerShell native error formatting"
    }

    return $text
}

function Format-CommandArg($arg)
{
    if ( $null -eq $arg ) {
        return '""'
    }

    if ( $arg.Length -eq 0 ) {
        return '""'
    }

    if ( $arg -notmatch '[\s"]' ) {
        return $arg
    }

    $builder = New-Object System.Text.StringBuilder
    [void]$builder.Append('"')
    $slashes = 0
    foreach ( $ch in $arg.ToCharArray() ) {
        if ( $ch -eq '\' ) {
            $slashes += 1
            continue
        }

        if ( $ch -eq '"' ) {
            [void]$builder.Append(('\' * ($slashes * 2 + 1)))
            [void]$builder.Append('"')
            $slashes = 0
            continue
        }

        if ( $slashes -gt 0 ) {
            [void]$builder.Append(('\' * $slashes))
            $slashes = 0
        }
        [void]$builder.Append($ch)
    }

    if ( $slashes -gt 0 ) {
        [void]$builder.Append(('\' * ($slashes * 2)))
    }
    [void]$builder.Append('"')
    return $builder.ToString()
}

function Format-CommandArgs($commandArgs)
{
    if ( $null -eq $commandArgs ) {
        return ""
    }

    $parts = New-Object System.Collections.Generic.List[string]
    foreach ( $arg in $commandArgs ) {
        $parts.Add((Format-CommandArg $arg)) | Out-Null
    }
    return [string]::Join(" ", $parts.ToArray())
}

function Invoke-ProcessCommand
{
    param(
        [string]$Name,
        [string]$FilePath,
        [string[]]$CommandArgs,
        [int]$Round
    )

    $safeName = $Name -replace '[^A-Za-z0-9_]+', '_'
    $stdout = Join-Path $tmpDir ("async_task_soak_{0}_{1}.out.txt" -f $Round, $safeName)
    $stderr = Join-Path $tmpDir ("async_task_soak_{0}_{1}.err.txt" -f $Round, $safeName)

    foreach ( $path in @($stdout, $stderr) ) {
        if ( Test-Path -LiteralPath $path -PathType Leaf ) {
            Remove-Item -LiteralPath $path -Force
        }
    }

    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $FilePath
    $psi.Arguments = Format-CommandArgs $CommandArgs
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.CreateNoWindow = $true

    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $psi
    $stdoutStream = $null
    $stderrStream = $null
    $stdoutTask = $null
    $stderrTask = $null
    $exitCode = $null
    $timedOut = $false

    try {
        $stdoutStream = [System.IO.File]::Open($stdout, [System.IO.FileMode]::Create, [System.IO.FileAccess]::Write, [System.IO.FileShare]::Read)
        $stderrStream = [System.IO.File]::Open($stderr, [System.IO.FileMode]::Create, [System.IO.FileAccess]::Write, [System.IO.FileShare]::Read)

        if ( ! $process.Start() ) {
            Add-Failure "$Name failed to start: $(Format-CommandArgs $CommandArgs)"
            return
        }

        # 启动后立即抽干两个管道，避免输出较多时子进程阻塞在 stdout/stderr。
        $stdoutTask = $process.StandardOutput.BaseStream.CopyToAsync($stdoutStream)
        $stderrTask = $process.StandardError.BaseStream.CopyToAsync($stderrStream)

        if ( ! $process.WaitForExit($TimeoutSeconds * 1000) ) {
            $timedOut = $true
            try { Stop-Process -Id $process.Id -Force } catch {}
            try { $process.WaitForExit() } catch {}
        } else {
            $exitCode = $process.ExitCode
        }

        foreach ( $taskInfo in @(
            @{ Name = "$Name stdout"; Task = $stdoutTask },
            @{ Name = "$Name stderr"; Task = $stderrTask }
        ) ) {
            if ( $null -ne $taskInfo.Task ) {
                try {
                    if ( ! $taskInfo.Task.Wait(30000) ) {
                        Add-Failure "$($taskInfo.Name) pipe copy timed out"
                    }
                } catch {
                    Add-Failure "$($taskInfo.Name) pipe copy failed: $($_.Exception.Message)"
                }
            }
        }
    } finally {
        if ( $stdoutStream -ne $null ) { $stdoutStream.Dispose() }
        if ( $stderrStream -ne $null ) { $stderrStream.Dispose() }
        if ( $process -ne $null ) { $process.Dispose() }
    }

    if ( $timedOut ) {
        Add-Failure "$Name timed out after ${TimeoutSeconds}s: $(Format-CommandArgs $CommandArgs)"
        return
    }

    $outText = Read-CapturedText "$Name stdout" $stdout
    $errText = Read-CapturedText "$Name stderr" $stderr

    if ( $exitCode -ne 0 ) {
        Add-Failure "$Name failed with exit code ${exitCode}: $(Format-CommandArgs $CommandArgs)`nstdout:`n$outText`nstderr:`n$errText"
        return
    }

    if ( $errText.Length -ne 0 ) {
        Add-Failure "$Name wrote to stderr: $(Format-CommandArgs $CommandArgs)`n$errText"
    }
}

function Invoke-XlCommand
{
    param(
        [string]$Name,
        [string[]]$CommandArgs,
        [int]$Round
    )

    Invoke-ProcessCommand -Name $Name -FilePath $script:xl -CommandArgs $CommandArgs -Round $Round
}

function Invoke-BuildRunCase
{
    param(
        [string]$Name,
        [string[]]$Sources,
        [string]$ExeName,
        [int]$Round
    )

    $exePath = Join-Path $tmpDir $ExeName
    if ( Test-Path -LiteralPath $exePath -PathType Leaf ) {
        Remove-Item -LiteralPath $exePath -Force
    }

    $buildArgs = @("build") + $Sources + @("-o", $exePath)
    Invoke-XlCommand -Name "$Name build" -CommandArgs $buildArgs -Round $Round
    if ( $script:failures.Count -gt 0 ) {
        return
    }

    if ( ! (Test-Path -LiteralPath $exePath -PathType Leaf) ) {
        Add-Failure "$Name build did not produce executable: $exePath"
        return
    }

    Invoke-ProcessCommand -Name "$Name exe" -FilePath $exePath -CommandArgs @() -Round $Round
}

if ( $Repeat -lt 1 ) {
    Write-Host "Repeat must be >= 1"
    exit 1
}

if ( $TimeoutSeconds -lt 1 ) {
    Write-Host "TimeoutSeconds must be >= 1"
    exit 1
}

if ( ! (Test-Path -LiteralPath $xl -PathType Leaf) ) {
    Write-Host "release xl.exe missing: $xl"
    exit 1
}

if ( ! (Test-Path -LiteralPath $tmpDir -PathType Container) ) {
    New-Item -ItemType Directory -Path $tmpDir | Out-Null
}

$commands = @(
    @{ Name = "concurrency example"; Args = @("run", "release\examples\concurrency.xl") },
    @{ Name = "async generator example"; Args = @("run", "release\examples\async_generator.xl") },
    @{ Name = "async case"; Args = @("run", "test\cases\cgen_async.xl") },
    @{ Name = "thread case"; Args = @("run", "test\cases\cgen_thread.xl") },
    @{ Name = "sync case"; Args = @("run", "test\cases\cgen_sync.xl") },
    @{ Name = "channel case"; Args = @("run", "test\cases\cgen_channel.xl") }
)

if ( $IncludeHeavy ) {
    $commands += @{ Name = "task api example"; Args = @("run", "release\examples\task_api.xl") }
}

$buildCases = @(
    @{
        Name = "task spawn build case"
        Sources = @("test\cases\cgen_task_spawn.xl")
        ExeName = "async_task_soak_cgen_task_spawn.exe"
    }
)

if ( $IncludeHeavy ) {
    $buildCases += @{
        Name = "task api build case"
        Sources = @("test\cases\cgen_task_api.xl", "test\cases\cgen_class_host.c")
        ExeName = "async_task_soak_cgen_task_api.exe"
    }
}

for ( $round = 1; $round -le $Repeat; $round++ ) {
    foreach ( $command in $commands ) {
        Invoke-XlCommand -Name $command.Name -CommandArgs $command.Args -Round $round
    }
    foreach ( $buildCase in $buildCases ) {
        Invoke-BuildRunCase -Name $buildCase.Name -Sources $buildCase.Sources -ExeName $buildCase.ExeName -Round $round
    }
}

if ( $failures.Count -gt 0 ) {
    foreach ( $failure in $failures ) {
        Write-Host $failure
    }
    exit 1
}

$heavyText = "off"
if ( $IncludeHeavy ) {
    $heavyText = "on"
}
Write-Host "Async/task soak ok: $Repeat round(s), $($commands.Count) run command(s), $($buildCases.Count) build/run case(s), heavy=$heavyText."
exit 0
