$script:XlirPublicCgenImplementation = $PSCommandPath
if ($script:XlirPublicCgenHelpersLoaded) { return }
$script:XlirPublicCgenHelpersLoaded = $true

. (Join-Path $PSScriptRoot 'xlir_native_process.ps1')

function Get-XlirPublicCgenHash([string]$Path) {
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash
}

function New-XlirPublicCgenSnapshot {
    param([Parameter(Mandatory=$true)][AllowEmptyCollection()][string[]]$Paths)

    $records = [System.Collections.Generic.List[object]]::new()
    $known = [System.Collections.Generic.HashSet[string]]::new(
        [StringComparer]::OrdinalIgnoreCase)
    foreach ($candidate in $Paths) {
        $path = [IO.Path]::GetFullPath($candidate)
        if (!$known.Add($path)) { continue }
        if (!(Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "XLIR validation input is missing: $path"
        }
        $records.Add([pscustomobject]@{
            Path = $path
            Sha256 = Get-XlirPublicCgenHash $path
        })
    }
    return $records.ToArray()
}

function Assert-XlirPublicCgenSnapshotUnchanged {
    param([Parameter(Mandatory=$true)][AllowEmptyCollection()][object[]]$Snapshot)

    foreach ($record in $Snapshot) {
        if (!(Test-Path -LiteralPath $record.Path -PathType Leaf) -or
            (Get-XlirPublicCgenHash $record.Path) -ne $record.Sha256) {
            throw "XLIR validation input changed during validation: $($record.Path)"
        }
    }
}

function Assert-XlirPublicCgenDirectoryInventoryUnchanged {
    param(
        [Parameter(Mandatory=$true)][string]$Directory,
        [Parameter(Mandatory=$true)][string]$Filter,
        [Parameter(Mandatory=$true)][AllowEmptyCollection()][object[]]$Snapshot
    )

    $expected = @($Snapshot | ForEach-Object { $_.Path } | Sort-Object)
    $current = @(Get-ChildItem -LiteralPath $Directory -File -Filter $Filter |
        ForEach-Object { $_.FullName } | Sort-Object)
    if ($expected.Count -ne $current.Count -or
        ($expected -join "`n") -cne ($current -join "`n")) {
        throw "XLIR validation input inventory changed during validation: $Directory ($Filter)"
    }
    Assert-XlirPublicCgenSnapshotUnchanged $Snapshot
}

function Open-XlirPublicCgen {
    param(
        [Parameter(Mandatory=$true)][string]$Root,
        [Parameter(Mandatory=$true)][string]$PublicDirectory
    )

    $rootPath = (Resolve-Path -LiteralPath $Root).Path
    $publicPath = if ([IO.Path]::IsPathRooted($PublicDirectory)) {
        [IO.Path]::GetFullPath($PublicDirectory)
    } else {
        [IO.Path]::GetFullPath((Join-Path $rootPath $PublicDirectory))
    }
    $directoryPath = (Resolve-Path -LiteralPath $publicPath).Path
    $manifestPath = Join-Path $directoryPath 'build.json'
    if (!(Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
        throw "Current-source public compiler manifest is missing: $manifestPath"
    }
    $manifestHashBefore = Get-XlirPublicCgenHash $manifestPath
    try {
        $manifestText = [IO.File]::ReadAllText($manifestPath, [Text.Encoding]::UTF8)
        $manifest = $manifestText | ConvertFrom-Json
    } catch {
        throw "Current-source public compiler manifest is invalid: $manifestPath ($($_.Exception.Message))"
    }
    $manifestHashAfter = Get-XlirPublicCgenHash $manifestPath
    if ($manifestHashBefore -ne $manifestHashAfter) {
        throw 'Current-source public compiler manifest changed while it was opened'
    }
    if (!$manifest.SourceXrt -or $manifest.RuntimeOnly -or $manifest.EnableExeCompiler -ne 1) {
        throw 'XLIR Cgen admission requires a complete current-source public compiler build'
    }
    if (@($manifest.Sources | Where-Object { $_ -eq 'test/cases/xlir_public_source_driver.c' }).Count -ne 1) {
        throw 'Current-source public compiler manifest does not identify the reviewed source driver'
    }

    if (![IO.Path]::IsPathRooted([string]$manifest.Driver)) {
        throw 'Current-source public compiler manifest contains a relative driver path'
    }
    $driver = [IO.Path]::GetFullPath([string]$manifest.Driver)
    $expectedPrefix = $directoryPath.TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
    if (!$driver.StartsWith($expectedPrefix, [StringComparison]::OrdinalIgnoreCase) -or
        [IO.Path]::GetFileName($driver) -ne 'public_source.exe') {
        throw "Public compiler driver escaped its reviewed build directory: $driver"
    }
    if (!(Test-Path -LiteralPath $driver -PathType Leaf) -or
        (Get-XlirPublicCgenHash $driver) -ne [string]$manifest.DriverSha256) {
        throw 'Current-source public compiler driver is missing or stale'
    }
    foreach ($record in @($manifest.Headers) + @($manifest.Objects)) {
        try { $null = Get-XlirContainedRelativePath -Base $directoryPath -Path ([string]$record.Path) }
        catch { throw "Generated public compiler input escaped its build directory: $($record.Path)" }
    }

    $records = [System.Collections.Generic.List[object]]::new()
    $known = [System.Collections.Generic.Dictionary[string,string]]::new(
        [System.StringComparer]::OrdinalIgnoreCase)
    foreach ($record in @($manifest.Inputs) + @($manifest.Headers) + @($manifest.Objects)) {
        if (![IO.Path]::IsPathRooted([string]$record.Path)) {
            throw 'Current-source public compiler manifest contains a relative input path'
        }
        $path = [IO.Path]::GetFullPath([string]$record.Path)
        $hash = [string]$record.Sha256
        if (!$path -or $hash -notmatch '^[0-9A-Fa-f]{64}$') {
            throw 'Current-source public compiler manifest contains an invalid input record'
        }
        if ($known.ContainsKey($path)) {
            if ($known[$path] -ne $hash) {
                throw "Current-source public compiler manifest has conflicting hashes: $path"
            }
            continue
        }
        if (!(Test-Path -LiteralPath $path -PathType Leaf) -or
            (Get-XlirPublicCgenHash $path) -ne $hash) {
            throw "Current-source public compiler input is missing or stale: $path"
        }
        $known.Add($path, $hash)
        $records.Add([pscustomobject]@{ Path = $path; Sha256 = $hash })
    }
    if ($records.Count -lt 1) {
        throw 'Current-source public compiler manifest contains no frozen inputs'
    }
    foreach ($relativeSource in @($manifest.Sources)) {
        $sourcePath = [IO.Path]::GetFullPath((Join-Path $rootPath ([string]$relativeSource)))
        $null = Get-XlirContainedRelativePath -Base $rootPath -Path $sourcePath
        if (!$known.ContainsKey($sourcePath)) {
            throw "Current-source translation unit is not frozen by the manifest: $relativeSource"
        }
    }

    return [pscustomobject]@{
        Root = $rootPath
        Directory = $directoryPath
        ManifestPath = $manifestPath
        ManifestSha256 = $manifestHashAfter
        Manifest = $manifest
        Driver = $driver
        DriverSha256 = [string]$manifest.DriverSha256
        Inputs = $records.ToArray()
    }
}

function Assert-XlirPublicCgenUnchanged {
    param([Parameter(Mandatory=$true)]$Session)

    if (!(Test-Path -LiteralPath $Session.ManifestPath -PathType Leaf) -or
        (Get-XlirPublicCgenHash $Session.ManifestPath) -ne $Session.ManifestSha256) {
        throw 'Current-source public compiler manifest changed during validation'
    }
    if (!(Test-Path -LiteralPath $Session.Driver -PathType Leaf) -or
        (Get-XlirPublicCgenHash $Session.Driver) -ne $Session.DriverSha256) {
        throw 'Current-source public compiler driver changed during validation'
    }
    foreach ($record in @($Session.Inputs)) {
        if (!(Test-Path -LiteralPath $record.Path -PathType Leaf) -or
            (Get-XlirPublicCgenHash $record.Path) -ne $record.Sha256) {
            throw "Current-source public compiler input changed during validation: $($record.Path)"
        }
    }
}

function Start-XlirPublicCgen {
    param(
        [Parameter(Mandatory=$true)]$Session,
        [Parameter(Mandatory=$true)][string]$Source,
        [Parameter(Mandatory=$true)][ValidateSet('default','xlir')][string]$Backend,
        [Parameter(Mandatory=$true)][string]$Output,
        [ValidateRange(1, 600)][int]$TimeoutSeconds = 60
    )

    $sourcePath = if ([IO.Path]::IsPathRooted($Source)) {
        [IO.Path]::GetFullPath($Source)
    } else {
        [IO.Path]::GetFullPath((Join-Path $Session.Root $Source))
    }
    if (!(Test-Path -LiteralPath $sourcePath -PathType Leaf)) {
        throw "XLIR Cgen source is missing: $sourcePath"
    }
    $sourceSha256 = Get-XlirPublicCgenHash $sourcePath
    $outputPath = if ([IO.Path]::IsPathRooted($Output)) {
        [IO.Path]::GetFullPath($Output)
    } else {
        [IO.Path]::GetFullPath((Join-Path $Session.Root $Output))
    }
    if ($outputPath.Equals($sourcePath, [StringComparison]::OrdinalIgnoreCase)) {
        throw "XLIR Cgen output aliases its source input: $outputPath"
    }
    if ($outputPath.Equals($Session.Driver, [StringComparison]::OrdinalIgnoreCase) -or
        $outputPath.Equals($Session.ManifestPath, [StringComparison]::OrdinalIgnoreCase) -or
        @($Session.Inputs | Where-Object {
            $outputPath.Equals($_.Path, [StringComparison]::OrdinalIgnoreCase)
        }).Count) {
        throw "XLIR Cgen output aliases a frozen compiler input: $outputPath"
    }
    $outputParent = Split-Path -Parent $outputPath
    if (!(Test-Path -LiteralPath $outputParent -PathType Container)) {
        [IO.Directory]::CreateDirectory($outputParent) | Out-Null
    }
    if (Test-Path -LiteralPath $outputPath) {
        Remove-Item -LiteralPath $outputPath
    }

    $process = [Diagnostics.Process]::new()
    $process.StartInfo = [Diagnostics.ProcessStartInfo]::new()
    $process.StartInfo.FileName = $Session.Driver
    $process.StartInfo.WorkingDirectory = $Session.Root
    $process.StartInfo.Arguments = Join-XlirWindowsCommandLine @(
        $sourcePath, "cgen-$Backend", $outputPath)
    $process.StartInfo.UseShellExecute = $false
    $process.StartInfo.CreateNoWindow = $true
    $process.StartInfo.RedirectStandardOutput = $true
    $process.StartInfo.RedirectStandardError = $true
    $process.StartInfo.EnvironmentVariables['XLANG_PACKAGE_PATH'] =
        (Join-Path $Session.Root 'release\extlibs') + ';' +
        (Join-Path $Session.Root 'test\fixtures')
    $timer = [Diagnostics.Stopwatch]::StartNew()
    try {
        if (!$process.Start()) { throw 'Current-source public compiler process did not start' }
        $stdoutTask = $process.StandardOutput.ReadToEndAsync()
        $stderrTask = $process.StandardError.ReadToEndAsync()
    } catch {
        $timer.Stop()
        $process.Dispose()
        throw
    }

    return [pscustomobject]@{
        Process = $process
        StdoutTask = $stdoutTask
        StderrTask = $stderrTask
        Timer = $timer
        TimeoutSeconds = $TimeoutSeconds
        Source = $sourcePath
        SourceSha256 = $sourceSha256
        Backend = $Backend
        Artifact = $outputPath
        Completed = $false
    }
}

function Complete-XlirPublicCgen {
    param([Parameter(Mandatory=$true)]$Running)

    if ($Running.Completed) { throw 'Current-source public compiler invocation was already completed' }
    $process = $Running.Process
    $remaining = [Math]::Max(0,
        ($Running.TimeoutSeconds * 1000) - [int]$Running.Timer.ElapsedMilliseconds)
    $timedOut = !$process.HasExited -and !$process.WaitForExit($remaining)
    if ($timedOut) { Stop-XlirProcessTree $process }
    else { $process.WaitForExit() }
    try {
        $stdout = $Running.StdoutTask.Result
        $stderr = $Running.StderrTask.Result
        $exitCode = if ($timedOut) { $null } else { $process.ExitCode }
    } finally {
        $Running.Timer.Stop()
        $process.Dispose()
        $Running.Completed = $true
    }

    $diagnostic = (@($stdout.TrimEnd(), $stderr.TrimEnd()) |
        Where-Object { $_ }) -join [Environment]::NewLine
    $sourceUnchanged = (Test-Path -LiteralPath $Running.Source -PathType Leaf) -and
        (Get-XlirPublicCgenHash $Running.Source) -eq $Running.SourceSha256
    $artifactExists = Test-Path -LiteralPath $Running.Artifact -PathType Leaf
    $artifactBytes = if ($artifactExists) { (Get-Item -LiteralPath $Running.Artifact).Length } else { 0 }
    $artifactComplete = $artifactExists -and $artifactBytes -gt 0
    $complete = !$timedOut -and $stdout.Contains("public source cgen-$($Running.Backend) complete")
    $executionFailure = ''
    if ($timedOut) {
        $executionFailure = "compiler timed out after $($Running.TimeoutSeconds) seconds"
    } elseif (!$sourceUnchanged) {
        $executionFailure = 'compiler source changed during validation'
    } elseif ($exitCode -notin @(0, 1)) {
        $executionFailure = "unexpected compiler exit code: $exitCode"
    } elseif ($exitCode -eq 1 -and !$diagnostic) {
        $executionFailure = 'compiler rejected input without a diagnostic'
    } elseif ($exitCode -eq 1 -and $artifactExists) {
        $executionFailure = 'compiler rejected input but left a generated C artifact'
    } elseif ($exitCode -eq 0 -and !$artifactComplete) {
        $executionFailure = 'compiler succeeded without a generated C artifact'
    } elseif ($exitCode -eq 0 -and !$complete) {
        $executionFailure = 'compiler succeeded without its completion marker'
    }
    return [pscustomobject]@{
        Source = $Running.Source
        SourceSha256 = $Running.SourceSha256
        SourceUnchanged = $sourceUnchanged
        Backend = $Running.Backend
        ExitCode = $exitCode
        Output = $diagnostic
        Stdout = $stdout.Trim()
        Stderr = $stderr.Trim()
        Artifact = $Running.Artifact
        ArtifactExists = $artifactExists
        ArtifactBytes = $artifactBytes
        ArtifactSha256 = if ($artifactExists) { Get-XlirPublicCgenHash $Running.Artifact } else { $null }
        Complete = $complete
        TimedOut = $timedOut
        DurationMilliseconds = [Math]::Round($Running.Timer.Elapsed.TotalMilliseconds, 3)
        ExecutionFailure = $executionFailure
    }
}

function Invoke-XlirPublicCgenBatch {
    param(
        [Parameter(Mandatory=$true)]$Session,
        [Parameter(Mandatory=$true)][ValidateCount(1, 16)][object[]]$Requests,
        [ValidateRange(1, 600)][int]$TimeoutSeconds = 60
    )

    $outputs = [System.Collections.Generic.HashSet[string]]::new(
        [StringComparer]::OrdinalIgnoreCase)
    foreach ($request in $Requests) {
        if (!$request.Source -or !$request.Backend -or !$request.Output) {
            throw 'Every XLIR Cgen batch request requires Source, Backend and Output'
        }
        if ([string]$request.Backend -notin @('default', 'xlir')) {
            throw "XLIR Cgen batch backend is invalid: $($request.Backend)"
        }
        $fullOutput = if ([IO.Path]::IsPathRooted([string]$request.Output)) {
            [IO.Path]::GetFullPath([string]$request.Output)
        } else {
            [IO.Path]::GetFullPath((Join-Path $Session.Root ([string]$request.Output)))
        }
        if (!$outputs.Add($fullOutput)) {
            throw "XLIR Cgen batch outputs must be unique: $fullOutput"
        }
    }

    $running = [System.Collections.Generic.List[object]]::new()
    $results = [System.Collections.Generic.List[object]]::new()
    try {
        foreach ($request in $Requests) {
            $running.Add((Start-XlirPublicCgen -Session $Session -Source $request.Source `
                -Backend $request.Backend -Output $request.Output -TimeoutSeconds $TimeoutSeconds))
        }
        foreach ($invocation in $running) {
            $results.Add((Complete-XlirPublicCgen -Running $invocation))
        }
    } finally {
        foreach ($invocation in $running) {
            if (!$invocation.Completed) {
                Stop-XlirProcessTree $invocation.Process
                $invocation.Timer.Stop()
                $invocation.Process.Dispose()
                $invocation.Completed = $true
            }
        }
    }
    return $results.ToArray()
}

function Invoke-XlirPublicCgen {
    param(
        [Parameter(Mandatory=$true)]$Session,
        [Parameter(Mandatory=$true)][string]$Source,
        [Parameter(Mandatory=$true)][ValidateSet('default','xlir')][string]$Backend,
        [Parameter(Mandatory=$true)][string]$Output,
        [ValidateRange(1, 600)][int]$TimeoutSeconds = 60
    )

    $results = @(Invoke-XlirPublicCgenBatch -Session $Session -Requests @(
        [pscustomobject]@{ Source = $Source; Backend = $Backend; Output = $Output }
    ) -TimeoutSeconds $TimeoutSeconds)
    return $results[0]
}
