# 复用仓库的 Windows 参数编码与进程树终止，不让 stderr 丢失真实退出码。
. (Join-Path $PSScriptRoot 'xlir_native_process.ps1')

function Invoke-ExtlibSmokeProcess([string]$File, [string[]]$Arguments, [string]$WorkingDirectory, [ValidateRange(1,3600)][int]$TimeoutSeconds, [hashtable]$Environment = @{}) {
    $process = [Diagnostics.Process]::new()
    $process.StartInfo.FileName = $File
    $process.StartInfo.Arguments = Join-XlirWindowsCommandLine $Arguments
    $process.StartInfo.WorkingDirectory = $WorkingDirectory
    $process.StartInfo.UseShellExecute = $false
    $process.StartInfo.CreateNoWindow = $true
    $process.StartInfo.RedirectStandardOutput = $true
    $process.StartInfo.RedirectStandardError = $true
    $process.StartInfo.StandardOutputEncoding = [Text.UTF8Encoding]::new($false)
    $process.StartInfo.StandardErrorEncoding = [Text.UTF8Encoding]::new($false)
    foreach ($name in $Environment.Keys) { $process.StartInfo.EnvironmentVariables[$name] = $Environment[$name] }
    $watch = [Diagnostics.Stopwatch]::StartNew()
    $started = $false
    $result = [ordered]@{ status='launch-error'; exitCode=$null; stdout=''; stderr=''; elapsedMs=0 }
    try {
        if (!$process.Start()) { throw "Unable to start $File" }
        $started = $true
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        $timedOut = !$process.WaitForExit($TimeoutSeconds * 1000)
        if ($timedOut) { Stop-XlirProcessTree $process }
        $result.exitCode = $process.ExitCode
        $result.status = if ($timedOut) { 'timeout' } elseif ($process.ExitCode -eq 0) { 'pass' } else { 'fail' }
        # 后代进程若错误保留输出管道，也不能让报告收集阶段无限等待。
        if (!$stdout.Wait(5000) -or !$stderr.Wait(5000)) {
            $result.status = 'capture-timeout'
            $result.stderr = 'Child process retained redirected output after the tested process exited'
        } else {
            $result.stdout = $stdout.GetAwaiter().GetResult()
            $result.stderr = $stderr.GetAwaiter().GetResult()
        }
    } catch {
        $result.status = 'runner-error'
        $result.stderr = $_.Exception.Message
    } finally {
        if ($started) {
            try { Stop-XlirProcessTree $process } catch {
                $result.status = 'cleanup-error'
                $result.stderr += "`n" + $_.Exception.Message
            }
        }
        $watch.Stop()
        $result.elapsedMs = $watch.ElapsedMilliseconds
        $process.Dispose()
    }
    return [pscustomobject]$result
}
