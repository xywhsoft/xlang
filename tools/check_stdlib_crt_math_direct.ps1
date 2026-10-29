param([string]$Root = "")

$ErrorActionPreference = "Stop"
if ( $Root -eq "" ) { $Root = Split-Path -Parent $PSScriptRoot }

$xl = Join-Path $Root "release\xl.exe"
$fixture = Join-Path $Root "test\cases\stdlib_crt_math.xl"
$outDir = Join-Path $Root "build\test\stdlib_crt_math_direct"
$irPath = Join-Path $outDir "math.lowered.xlir"
$cPath = Join-Path $outDir "math.c"
if ( ! (Test-Path -LiteralPath $xl -PathType Leaf) ) {
    throw "CRT math direct-call check requires release/xl.exe"
}
[System.IO.Directory]::CreateDirectory($outDir) | Out-Null

& $xl ir $fixture --lowered --emit $irPath
if ( $LASTEXITCODE -ne 0 ) { throw "CRT math lowered XLIR generation failed" }
& $xl cgen $fixture --backend xlir --emit $cPath
if ( $LASTEXITCODE -ne 0 ) { throw "CRT math C generation failed" }

$ir = [System.IO.File]::ReadAllText($irPath, [System.Text.Encoding]::UTF8)
$c = [System.IO.File]::ReadAllText($cPath, [System.Text.Encoding]::UTF8)
$symbols = @(
    "sin", "cos", "tan", "asin", "acos", "atan", "atan2", "sqrt", "pow",
    "exp", "log", "log10", "floor", "ceil", "round", "nearbyint", "fabs",
    "sinh", "cosh", "tanh", "asinh", "acosh", "atanh", "erf", "erfc",
    "tgamma", "lgamma", "fma", "copysign", "nextafter", "remainder",
    "fdim", "fmin", "fmax", "ldexp", "scalbn", "logb", "ilogb",
    "frexp", "modf", "remquo"
)
foreach ( $symbol in $symbols ) {
    $escaped = [regex]::Escape($symbol)
    $target = [regex]::Match($ir, "(?m)^\s*call-target .*\bcname=$escaped\b.*$").Value
    if ( $target -eq "" -or
         $target -notmatch '\bexecution=foreign-nonblocking\b' -or
         $target -notmatch '\berrors=no-raise/none\b' ) {
        throw "CRT math target lacks its reviewed effects: $symbol : $target"
    }
    if ( $c -notmatch "(?m)^extern (?:double|int) $escaped\(" -or
         $c -notmatch "=\s*$escaped\(" ) {
        throw "CRT math C is not a direct scalar call: $symbol"
    }
}
if ( $c -match '(?m)^\s*xl6ForeignCallBegin\s*\(' ) {
    throw "reviewed CRT math calls still create a foreign error scope"
}
if ( $c -match '\bllabs\s*\(' ) {
    throw "integer math.abs still uses undefined llabs(INT64_MIN)"
}
foreach ( $declaration in @(
    'extern double frexp\(double, int\*\);',
    'extern double modf\(double, double\*\);',
    'extern double remquo\(double, double, int\*\);'
) ) {
    if ( $c -notmatch $declaration ) {
        throw "CRT out-parameter declaration does not match the C ABI: $declaration"
    }
}

# Length-bearing strings must not regress to strlen-based hash inputs.
$hashFixture = Join-Path $Root "test\cases\stdlib_hash.xl"
$hashIrPath = Join-Path $outDir "hash.lowered.xlir"
$hashCPath = Join-Path $outDir "hash.c"
& $xl ir $hashFixture --lowered --emit $hashIrPath
if ( $LASTEXITCODE -ne 0 ) { throw "Hash lowered XLIR generation failed" }
& $xl cgen $hashFixture --backend xlir --emit $hashCPath
if ( $LASTEXITCODE -ne 0 ) { throw "Hash C generation failed" }
$hashIr = [System.IO.File]::ReadAllText($hashIrPath, [System.Text.Encoding]::UTF8)
$hashC = [System.IO.File]::ReadAllText($hashCPath, [System.Text.Encoding]::UTF8)
if ( $hashIr -match '(?m)^\s*call-target .*\bcname=strlen\b' -or
     $hashC -notmatch 'xrtHash32\(data\.Data, xlir_hash_limit\(data\.Size, limit\)\)' -or
     $hashC -notmatch 'xrtHash64\(data\.Data, xlir_hash_limit\(data\.Size, limit\)\)' ) {
    throw "Hash inputs lost their precise pointer/length ABI"
}
Write-Host "CRT direct-call contract passed: $($symbols.Count) math symbols; hash uses precise string lengths."
