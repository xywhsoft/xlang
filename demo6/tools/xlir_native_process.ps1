$script:XlirNativeProcessImplementation = $PSCommandPath
if ($script:XlirNativeProcessHelpersLoaded) { return }
$script:XlirNativeProcessHelpersLoaded = $true

function New-XlirCStringDefine([string]$Name, [string]$Value, [switch]$RawArgument) {
    if ($Name -notmatch '^[A-Za-z_][A-Za-z0-9_]*$' -or
        $Value.IndexOfAny([char[]]@([char]0, [char]10, [char]13, [char]34)) -ge 0) {
        throw 'Invalid C string preprocessor definition'
    }
    $escaped = $Value.Replace('\', '\\')
    # Windows PowerShell 5 removes unescaped quotes while serializing native
    # arguments. PowerShell 7 uses the standard argument-passing contract.
    # ProcessStartInfo/ArgumentList callers require an unencoded raw argument;
    # their own Windows command-line serializer performs transport escaping.
    $quote = if (!$RawArgument -and $PSVersionTable.PSEdition -eq 'Desktop') { '\"' } else { '"' }
    return "-D$Name=$quote$escaped$quote"
}

function Get-XlirContainedRelativePath([string]$Base, [string]$Path) {
    $basePath = [IO.Path]::GetFullPath($Base).TrimEnd('\', '/')
    $fullPath = [IO.Path]::GetFullPath($Path)
    if ($fullPath.Equals($basePath, [StringComparison]::OrdinalIgnoreCase)) { return '.' }
    $prefix = $basePath + [IO.Path]::DirectorySeparatorChar
    if (!$fullPath.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Path escaped its reviewed root: $fullPath"
    }
    return $fullPath.Substring($prefix.Length)
}

function Invoke-XlirNativeToLog([string]$File, [string[]]$Arguments, [string]$Log) {
    # Native stderr is diagnostic data. Windows PowerShell may promote it to a
    # terminating error before the caller observes the process exit code.
    $previousPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        & $File @Arguments *> $Log
        return $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousPreference
    }
}

function ConvertTo-XlirWindowsArgument([AllowEmptyString()][string]$Value) {
    if ($Value.Length -ne 0 -and $Value -notmatch '[\s"]') { return $Value }
    $builder = [Text.StringBuilder]::new()
    [void]$builder.Append('"')
    $backslashes = 0
    foreach ($character in $Value.ToCharArray()) {
        if ($character -eq '\') { $backslashes++; continue }
        if ($character -eq '"') {
            [void]$builder.Append((('\' * (2 * $backslashes + 1)) -join ''))
            [void]$builder.Append('"')
        } else {
            if ($backslashes) { [void]$builder.Append((('\' * $backslashes) -join '')) }
            [void]$builder.Append($character)
        }
        $backslashes = 0
    }
    if ($backslashes) { [void]$builder.Append((('\' * (2 * $backslashes)) -join '')) }
    [void]$builder.Append('"')
    return $builder.ToString()
}

function Join-XlirWindowsCommandLine([string[]]$Arguments) {
    return (@($Arguments | ForEach-Object { ConvertTo-XlirWindowsArgument $_ }) -join ' ')
}

function Stop-XlirProcessTree([Diagnostics.Process]$Process) {
    if ($null -eq $Process -or $Process.HasExited) { return }
    $taskkill = Join-Path $env:SystemRoot 'System32\taskkill.exe'
    $previousPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        & $taskkill /PID ([string]$Process.Id) /T /F *> $null
    } finally {
        $ErrorActionPreference = $previousPreference
    }
    if (!$Process.WaitForExit(5000)) { $Process.Kill(); $Process.WaitForExit() }
}
