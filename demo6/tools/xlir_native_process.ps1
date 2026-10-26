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

function Invoke-XlirGccToLog([string[]]$Arguments, [string]$Log, [switch]$PassThru,
    [ValidateRange(1,3600)][int]$TimeoutSeconds = 900) {
    # Arguments are logical argv, not PowerShell-escaped command fragments.
    # In particular New-XlirCStringDefine callers must use -RawArgument.
    $compiler = (Get-Command gcc -CommandType Application | Select-Object -First 1).Source
    $logPath = [IO.Path]::GetFullPath($Log)
    $responsePath = $logPath + '.rsp'
    $receiptPath = $logPath + '.command.json'
    foreach ($path in @($logPath, $responsePath, $receiptPath)) {
        if (Test-Path -LiteralPath $path) { throw "Preserve prior compiler evidence: $path" }
    }
    foreach ($argument in $Arguments) {
        if ($null -eq $argument -or $argument.IndexOfAny([char[]]@([char]0,[char]10,[char]13)) -ge 0) {
            throw 'Invalid GCC response argument'
        }
        # Nested response expansion would make the recorded argv incomplete.
        if ($argument.StartsWith('@')) { throw 'Nested GCC response files are not admitted' }
    }
    $helperHash = (Get-FileHash -LiteralPath $script:XlirNativeProcessImplementation).Hash
    $compilerHash = (Get-FileHash -LiteralPath $compiler).Hash
    $utf8 = [Text.UTF8Encoding]::new($false)
    # GCC's response reader consumes backslash escapes inside quoted words.
    # Preserve every logical byte, including C-string backslashes and quotes;
    # do not normalize all arguments as paths or apply Windows argv escaping.
    $lines = @($Arguments | ForEach-Object { '"' + $_.Replace('\','\\').Replace('"','\"') + '"' })
    [IO.File]::WriteAllText($responsePath, ($lines -join "`n"), $utf8)
    $receipt = [ordered]@{Complete=$false;Compiler=$compiler;CompilerSha256=$compilerHash;
        Helper=$script:XlirNativeProcessImplementation;HelperSha256=$helperHash;
        WorkingDirectory=(Get-Location).Path;Arguments=@($Arguments);ResponseFile=$responsePath;
        ResponseSha256=(Get-FileHash -LiteralPath $responsePath).Hash;TimeoutSeconds=$TimeoutSeconds;Exit=$null;Error=$null}
    $process = [Diagnostics.Process]::new()
    $process.StartInfo.FileName = $compiler
    $process.StartInfo.Arguments = Join-XlirWindowsCommandLine @('@' + $responsePath)
    $process.StartInfo.WorkingDirectory = (Get-Location).Path
    $process.StartInfo.UseShellExecute = $false
    $process.StartInfo.CreateNoWindow = $true
    $process.StartInfo.RedirectStandardOutput = $true
    $process.StartInfo.RedirectStandardError = $true
    $stdout = $null; $stderr = $null
    try {
        [void]$process.Start()
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        if (!$process.WaitForExit($TimeoutSeconds * 1000)) {
            Stop-XlirProcessTree $process
            throw "GCC compilation exceeded $TimeoutSeconds seconds"
        }
        $receipt.Exit = $process.ExitCode
        if ((Get-FileHash -LiteralPath $responsePath).Hash -cne $receipt.ResponseSha256 -or
            (Get-FileHash -LiteralPath $compiler).Hash -cne $compilerHash -or
            (Get-FileHash -LiteralPath $script:XlirNativeProcessImplementation).Hash -cne $helperHash) {
            throw 'GCC command inputs changed during compilation'
        }
        $receipt.Complete = $true
        if ($PassThru) {
            return [pscustomobject]@{status=$(if ($receipt.Exit -eq 0) {'pass'} else {'fail'});
                exitCode=$receipt.Exit;stdout=$stdout.Result;stderr=$stderr.Result;commandReceipt=$receiptPath}
        }
        return $receipt.Exit
    } catch { $receipt.Error = $_.Exception.Message; throw }
    finally {
        if ($null -ne $stdout -and $null -ne $stderr) {
            [IO.File]::WriteAllText($logPath, ($stdout.Result + $stderr.Result), $utf8)
        }
        $process.Dispose()
        [IO.File]::WriteAllText($receiptPath, ($receipt | ConvertTo-Json -Depth 5), $utf8)
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
