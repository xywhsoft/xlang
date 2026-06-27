param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$cgenDir = Join-Path $Root "src\cgen"
$failures = New-Object System.Collections.Generic.List[string]

function Add-Failure($message)
{
    $script:failures.Add($message) | Out-Null
}

function Normalize-PathText($path)
{
    return $path.Replace("/", "\")
}

if ( ! (Test-Path -LiteralPath $cgenDir -PathType Container) ) {
    Write-Host "cgen directory missing: $cgenDir"
    exit 1
}

$allowed = @(
    [pscustomobject]@{ Path = "src\cgen\xl6_cgen_args.c"; Text = 'xl6OutputPrintf(ctx->out, "({ xvalue __xl6_cond_result_'; Count = 1 },
    [pscustomobject]@{ Path = "src\cgen\xl6_cgen_args.c"; Text = 'xl6OutputPrintf(ctx->out, "({ xvalue __xl6_cmp_left_'; Count = 1 },
    [pscustomobject]@{ Path = "src\cgen\xl6_cgen_args.c"; Text = 'xl6OutputPrintf(ctx->out, "({ xvalue __xl6_approx_left_'; Count = 1 },
    [pscustomobject]@{ Path = "src\cgen\xl6_cgen_args.c"; Text = 'xl6OutputPrintf(ctx->out, "({ xvalue __xl6_bin_left_'; Count = 1 },
    [pscustomobject]@{ Path = "src\cgen\xl6_cgen_args.c"; Text = 'xl6OutputPrintf(ctx->out, "({ xvalue __xl6_logic_left_'; Count = 1 },
    [pscustomobject]@{ Path = "src\cgen\xl6_cgen_args.c"; Text = 'xl6OutputPrintf(ctx->out, "({ xvalue __xl6_nc_'; Count = 1 },
    [pscustomobject]@{ Path = "src\cgen\xl6_cgen_args.c"; Text = 'xl6OutputPuts("({ ", ctx->out);'; Count = 1 },
    [pscustomobject]@{ Path = "src\cgen\xl6_cgen_async_func.c"; Text = 'xl6OutputPuts("({ xfuture* ", ctx->out);'; Count = 1 }
)

$files = @(Get-ChildItem -LiteralPath $cgenDir -Filter "*.c" -File | Sort-Object FullName)
$matches = @(Select-String -Path ($files | ForEach-Object { $_.FullName }) -SimpleMatch "({")

foreach ( $match in $matches ) {
    $relative = Normalize-PathText ($match.Path.Substring($Root.Length).TrimStart("\", "/"))
    $ok = $false
    foreach ( $rule in $allowed ) {
        if ( $relative.Equals($rule.Path, [System.StringComparison]::OrdinalIgnoreCase) -and
             $match.Line.Contains($rule.Text) ) {
            $ok = $true
            break
        }
    }

    if ( ! $ok ) {
        Add-Failure ("unexpected cgen statement expression fallback: {0}:{1}: {2}" -f $relative, $match.LineNumber, $match.Line.Trim())
    }
}

foreach ( $rule in $allowed ) {
    $count = @(
        $matches | Where-Object {
            $relative = Normalize-PathText ($_.Path.Substring($Root.Length).TrimStart("\", "/"))
            $relative.Equals($rule.Path, [System.StringComparison]::OrdinalIgnoreCase) -and $_.Line.Contains($rule.Text)
        }
    ).Count

    if ( $count -ne $rule.Count ) {
        Add-Failure ("cgen statement expression allowlist count changed for {0}: expected {1}, actual {2}" -f $rule.Text, $rule.Count, $count)
    }
}

if ( $failures.Count -gt 0 ) {
    foreach ( $failure in $failures ) {
        Write-Host $failure
    }
    exit 1
}

Write-Host "Cgen statement expression allowlist ok: $($matches.Count) fallback templates."
exit 0
