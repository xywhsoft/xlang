param([Parameter(Mandatory=$true)][string]$Directory,
    [string]$Implementation = '', [switch]$ExpectedFailure)
$ErrorActionPreference = 'Stop'
if (!$Implementation) { $Implementation = Join-Path $PSScriptRoot 'xlir_c_object_cache.ps1' }
$Implementation = (Resolve-Path -LiteralPath $Implementation).Path
$output = [IO.Path]::GetFullPath($Directory)
if (Test-Path -LiteralPath $output) { throw 'Preserve prior compiler-selection evidence' }
[void][IO.Directory]::CreateDirectory($output)
$originalPath = $env:Path
try {
    $gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
    $bin = Split-Path -Parent $gcc
    # Real duplicate PATH entries, including alternate slash spellings. No
    # mock command, synthetic compiler, or change to the user's saved PATH.
    $env:Path = $bin.Replace('\','/') + ';' + $bin.Replace('/','\') + ';' + $originalPath
    $compilerMatches = @(Get-Command gcc -CommandType Application -ErrorAction Stop)
    if ($compilerMatches.Count -lt 2) { throw 'Duplicate executable discovery was not exercised' }
    . $Implementation
    $fixture = Join-Path $output 'fixture'
    [void][IO.Directory]::CreateDirectory($fixture)
    $source = Join-Path $fixture 'source.c'
    $header = Join-Path $fixture 'answer.h'
    [IO.File]::WriteAllText($source,"#include `"answer.h`"`nint answer(void) { return ANSWER; }`n",[Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText($header,"#define ANSWER 40`n",[Text.UTF8Encoding]::new($false))
    $report = [ordered]@{Complete=$false; ExpectedFailure=[bool]$ExpectedFailure;
        PowerShell=$PSVersionTable.PSVersion.ToString(); ExecutableMatches=$compilerMatches.Count;
        Control=$PSCommandPath; ControlSha256=(Get-FileHash -LiteralPath $PSCommandPath).Hash;
        Implementation=$Implementation; ImplementationSha256=(Get-FileHash -LiteralPath $Implementation).Hash;
        NativeImplementation=$script:XlirNativeProcessImplementation;
        NativeImplementationSha256=(Get-FileHash -LiteralPath $script:XlirNativeProcessImplementation).Hash;
        Rows=@(); ShippingAccepted=$false}
    try {
        $context = New-XlirCBuildContext -Root $fixture -Directory (Join-Path $output 'objects') -CacheDirectory (Join-Path $output 'cache')
        if ($ExpectedFailure) { throw 'The old compiler-discovery failure was not reproduced' }
        if ($context.Compiler -isnot [string] -or !(Test-Path -LiteralPath $context.Compiler -PathType Leaf) -or
            ![IO.Path]::GetFullPath($context.Compiler).Equals([IO.Path]::GetFullPath($compilerMatches[0].Source),[StringComparison]::OrdinalIgnoreCase)) {
            throw 'Compiler selection did not preserve the first actual executable'
        }
        $flags = @('-std=c11','-O2','-Wall','-Wextra','-Werror')
        $cold = Invoke-XlirCObject -Context $context -Source $source -Object (Join-Path $context.Directory 'cold.o') -Flags $flags
        $warm = Invoke-XlirCObject -Context $context -Source $source -Object (Join-Path $context.Directory 'warm.o') -Flags $flags
        if ($cold.CacheHit -or !$warm.CacheHit -or $cold.Sha256 -ne $warm.Sha256) { throw 'Exact-input cache reuse failed' }
        [IO.File]::WriteAllText($header,"#define ANSWER 41`n",[Text.UTF8Encoding]::new($false))
        $changed = Invoke-XlirCObject -Context $context -Source $source -Object (Join-Path $context.Directory 'changed.o') -Flags $flags
        if ($changed.CacheHit -or $changed.PreprocessedSha256 -eq $warm.PreprocessedSha256 -or $changed.Sha256 -eq $warm.Sha256) {
            throw 'A changed included header did not force a real recompilation'
        }
        Assert-XlirCToolchainUnchanged $context
        $report.Compiler=$context.Compiler; $report.Toolchain=$context.Toolchain
        $report.Rows=@($cold,$warm,$changed); $report.Complete=$true
    } catch {
        if (!$ExpectedFailure -or $_.FullyQualifiedErrorId -notmatch 'CommandNotFoundException' -or
            $_.Exception.Message -notmatch 'gcc[.]exe.+gcc[.]exe') { throw }
        $report.ExpectedDiscoveryRejection=$true; $report.Diagnostic=$_.Exception.Message; $report.Complete=$true
    } finally {
        $report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $output 'results.json') -Encoding UTF8
    }
    if (!$report.Complete) { throw 'Compiler-selection checks incomplete' }
    Write-Output "Compiler selection: $($compilerMatches.Count) real PATH matches; PS $($report.PowerShell); expected old failure=$ExpectedFailure; complete"
} finally { $env:Path = $originalPath }
