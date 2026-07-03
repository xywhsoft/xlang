param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
if ( ! (Test-Path -LiteralPath $xl -PathType Leaf) ) {
    Write-Host "xl.exe missing: $xl"
    exit 1
}

function New-DapPacket($obj)
{
    $json = $obj | ConvertTo-Json -Compress -Depth 16
    $len = [System.Text.Encoding]::UTF8.GetByteCount($json)
    return "Content-Length: $len`r`n`r`n$json"
}

function Assert-Contains($text, $needle)
{
    if ( ! $text.Contains($needle) ) {
        Write-Host "DAP smoke output missing: $needle"
        Write-Host $text
        exit 1
    }
}

$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.FileName = $xl
$psi.Arguments = "dap"
$psi.UseShellExecute = $false
$psi.RedirectStandardInput = $true
$psi.RedirectStandardOutput = $true
$psi.RedirectStandardError = $true
$psi.CreateNoWindow = $true

$proc = New-Object System.Diagnostics.Process
$proc.StartInfo = $psi
if ( ! $proc.Start() ) {
    Write-Host "failed to start xl dap"
    exit 1
}

$program = Join-Path $Root "release\examples\hello.xl"
$packets = @(
    @{ seq = 1; type = "request"; command = "initialize"; arguments = @{ adapterID = "xlang" } },
    @{ seq = 2; type = "request"; command = "setBreakpoints"; arguments = @{ source = @{ path = $program }; breakpoints = @( @{ line = 2 } ) } },
    @{ seq = 3; type = "request"; command = "setExceptionBreakpoints"; arguments = @{ filters = @() } },
    @{ seq = 4; type = "request"; command = "launch"; arguments = @{ program = $program; stopOnEntry = $false } },
    @{ seq = 5; type = "request"; command = "configurationDone"; arguments = @{} },
    @{ seq = 6; type = "request"; command = "threads"; arguments = @{} },
    @{ seq = 7; type = "request"; command = "stackTrace"; arguments = @{ threadId = 1 } },
    @{ seq = 8; type = "request"; command = "scopes"; arguments = @{ frameId = 1 } },
    @{ seq = 9; type = "request"; command = "variables"; arguments = @{ variablesReference = 1 } },
    @{ seq = 10; type = "request"; command = "pause"; arguments = @{ threadId = 1 } },
    @{ seq = 11; type = "request"; command = "evaluate"; arguments = @{ expression = "value" } },
    @{ seq = 12; type = "request"; command = "continue"; arguments = @{ threadId = 1 } },
    @{ seq = 13; type = "request"; command = "disconnect"; arguments = @{} }
)

foreach ( $packet in $packets ) {
    $proc.StandardInput.Write((New-DapPacket $packet))
    $proc.StandardInput.Flush()
}
$proc.StandardInput.Close()

if ( ! $proc.WaitForExit(10000) ) {
    try { $proc.Kill() } catch {}
    Write-Host "xl dap smoke timed out"
    exit 1
}

$stdout = $proc.StandardOutput.ReadToEnd()
$stderr = $proc.StandardError.ReadToEnd()

if ( $proc.ExitCode -ne 0 ) {
    Write-Host "xl dap exited with code $($proc.ExitCode)"
    Write-Host $stderr
    exit 1
}
if ( $stderr.Length -ne 0 ) {
    Write-Host "xl dap wrote to stderr:"
    Write-Host $stderr
    exit 1
}

Assert-Contains $stdout "Content-Length:"
Assert-Contains $stdout '"command":"initialize"'
Assert-Contains $stdout '"event":"initialized"'
Assert-Contains $stdout '"command":"setBreakpoints"'
Assert-Contains $stdout '"verified":true'
Assert-Contains $stdout '"command":"setExceptionBreakpoints"'
Assert-Contains $stdout '"command":"launch"'
Assert-Contains $stdout '"event":"output"'
Assert-Contains $stdout '"command":"configurationDone"'
Assert-Contains $stdout '"event":"stopped"'
Assert-Contains $stdout '"command":"threads"'
Assert-Contains $stdout '"command":"stackTrace"'
Assert-Contains $stdout '"command":"scopes"'
Assert-Contains $stdout '"variablesReference":1'
Assert-Contains $stdout '"command":"variables"'
Assert-Contains $stdout '"command":"pause"'
Assert-Contains $stdout '"reason":"pause"'
Assert-Contains $stdout '"success":false,"command":"evaluate"'
Assert-Contains $stdout '"event":"terminated"'
Assert-Contains $stdout '"command":"disconnect"'

Write-Host "DAP smoke ok."
