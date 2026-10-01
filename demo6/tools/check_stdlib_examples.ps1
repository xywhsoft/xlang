param(
    [string]$Root = "",
    [switch]$SkipDeferred
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot 'extlib_smoke_process.ps1')

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$exampleDir = Join-Path $Root "release\examples"
$minStdlibExampleFiles = 17

if ( !(Test-Path $xl) ) {
    Write-Host "release\xl.exe not found. Run build.bat before checking examples."
    exit 1
}

$examples = @(Get-ChildItem -Path $exampleDir -Filter "stdlib_*.xl" -File | Sort-Object Name)

if ( $SkipDeferred ) {
    $deferred = @("stdlib_http*.xl", "stdlib_websocket*.xl")
    $skipped = @()
    foreach ( $pattern in $deferred ) {
        $skipped += @($examples | Where-Object { $_.Name -like $pattern })
    }
    $examples = @($examples | Where-Object {
        $name = $_.Name
        -not ($deferred | Where-Object { $name -like $_ })
    })
    if ( $skipped.Count -gt 0 ) {
        Write-Host "Skipping deferred examples: $((@($skipped | Sort-Object Name | ForEach-Object { $_.Name })) -join ', ')"
    }
}

if ( $examples.Count -eq 0 ) {
    Write-Host "No standard library examples found."
    exit 1
}

if ( $examples.Count -lt $minStdlibExampleFiles ) {
    Write-Host "Standard library example baseline regressed: $($examples.Count) < $minStdlibExampleFiles"
    exit 1
}

$failures = [Collections.Generic.List[string]]::new()
Push-Location $Root
try {
    foreach ( $example in $examples ) {
        Write-Host "CHECK example $($example.Name)"
        $result = Invoke-ExtlibSmokeProcess $xl @('check', $example.FullName) $Root 60
        Write-Output ($result.stdout + $result.stderr)
        if ($result.status -ne 'pass') {
            $failures.Add("$($example.Name): check $($result.status), exit=$($result.exitCode)")
            continue
        }

        Write-Host "RUN example $($example.Name)"
        $result = Invoke-ExtlibSmokeProcess $xl @('run', $example.FullName) $Root 60
        Write-Output ($result.stdout + $result.stderr)
        if ($result.status -ne 'pass') {
            $failures.Add("$($example.Name): run $($result.status), exit=$($result.exitCode)")
        }
    }
}
finally {
    Pop-Location
}

if ($failures.Count) {
    Write-Host "Standard library examples: $($failures.Count) failures after collecting all $($examples.Count) files."
    $failures | ForEach-Object { Write-Host "  $_" }
    exit 1
}
Write-Host "Standard library examples check/run ok: $($examples.Count) files."
exit 0
