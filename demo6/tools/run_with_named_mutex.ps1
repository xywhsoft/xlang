param(
    [Parameter(Mandatory = $true)]
    [string]$Name,

    [Parameter(Mandatory = $true)]
    [string]$FilePath,

    [Parameter(Mandatory = $true)]
    [string]$WorkingDirectory,

    [string]$Argument0 = "",
    [string]$Argument1 = "",
    [int]$TimeoutMs = 300000
)

$ErrorActionPreference = "Stop"
$mutex = [System.Threading.Mutex]::new($false, $Name)
$locked = $false
$exitCode = 1

try {
    try {
        $locked = $mutex.WaitOne($TimeoutMs)
    } catch [System.Threading.AbandonedMutexException] {
        $locked = $true
    }
    if ( ! $locked ) {
        Write-Error "timed out waiting for named mutex: $Name"
    } else {
        $start = @{
            FilePath = $FilePath
            WorkingDirectory = $WorkingDirectory
            NoNewWindow = $true
            PassThru = $true
            Wait = $true
        }
        $arguments = @($Argument0, $Argument1) | Where-Object { $_ -ne "" }
        if ( $arguments.Count -gt 0 ) {
            $start.ArgumentList = $arguments
        }
        $process = Start-Process @start
        $exitCode = $process.ExitCode
    }
} finally {
    if ( $locked ) {
        $mutex.ReleaseMutex()
    }
    $mutex.Dispose()
}

exit $exitCode
