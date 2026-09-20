param([string]$Root = "")
$ErrorActionPreference = "Stop"
if ($Root -eq "") { $Root = Split-Path -Parent $PSScriptRoot }
$Root = (Resolve-Path -LiteralPath $Root).Path
$xl = Join-Path $Root "release\xl.exe"
$outputDir = Join-Path $Root "build\test"
[System.IO.Directory]::CreateDirectory($outputDir) | Out-Null

function Invoke-Captured([string]$File, [string]$Arguments, [int]$Expected, [string]$Marker = "") {
    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = New-Object System.Diagnostics.ProcessStartInfo
    $process.StartInfo.FileName = $File
    $process.StartInfo.Arguments = $Arguments
    $process.StartInfo.UseShellExecute = $false
    $process.StartInfo.CreateNoWindow = $true
    $process.StartInfo.RedirectStandardOutput = $true
    $process.StartInfo.RedirectStandardError = $true
    try {
        [void]$process.Start()
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        $process.WaitForExit()
        $diagnostic = $stdout.Result + $stderr.Result
        if ($process.ExitCode -ne $Expected -or
            ($Marker -ne "" -and !$diagnostic.Contains($Marker)) -or
            ($Marker -eq "" -and $stderr.Result -ne "")) {
            throw "Host error boundary failed: $File $Arguments exit=$($process.ExitCode), expected=$Expected $diagnostic"
        }
    } finally { $process.Dispose() }
}

$fixtures = @(
    @{ Name = "xlir_host_error_boundary_script"; Exit = 0; Marker = "" },
    @{ Name = "xlir_host_error_boundary_main_no_raise"; Exit = 0; Marker = "" },
    @{ Name = "xlir_host_error_boundary_main_may_raise"; Exit = 7; Marker = "" },
    @{ Name = "xlir_host_error_boundary_script_throw"; Exit = 1; Marker = "script-boundary-sentinel" },
    @{ Name = "xlir_host_error_boundary_main_throw"; Exit = 1; Marker = "main-boundary-sentinel" },
    @{ Name = "xlir_host_error_boundary_init_throw"; Exit = 1; Marker = "init-boundary-sentinel" }
)
Push-Location $Root
try {
    foreach ($fixture in $fixtures) {
        $source = Join-Path $Root ("test\cases\" + $fixture.Name + ".xl")
        $ir = Join-Path $outputDir ($fixture.Name + ".lowered.json")
        $cPath = Join-Path $outputDir ($fixture.Name + ".c")
        $repeat = Join-Path $outputDir ($fixture.Name + ".repeat.c")
        Invoke-Captured $xl ('ir "{0}" --lowered --format json --emit "{1}"' -f $source,$ir) 0
        $module = [System.IO.File]::ReadAllText($ir) | ConvertFrom-Json
        $raising = @($module.callTargets | Where-Object {
            $_.kind -eq 1 -and $_.reachability -ne "unreachable" -and $_.errorAbiName -eq "may-raise"
        })
        if (($fixture.Name -eq "xlir_host_error_boundary_main_no_raise" -and $raising.Count -ne 0) -or
            ($fixture.Name -eq "xlir_host_error_boundary_main_may_raise" -and $raising.Count -eq 0)) {
            throw "Host boundary fixture lost its distinct verified errorAbiName"
        }
        Invoke-Captured $xl ('cgen "{0}" --emit "{1}"' -f $source,$cPath) 0
        Invoke-Captured $xl ('cgen "{0}" --backend xlir --emit "{1}"' -f $source,$repeat) 0
        if ([System.IO.File]::ReadAllText($cPath) -cne [System.IO.File]::ReadAllText($repeat)) {
            throw "Host error boundary C emission is not deterministic"
        }
        & gcc -std=c11 -Wall -Wextra -Werror -Wno-unused-function -Wno-unused-label -Wno-unused-variable -Wno-unused-parameter -Wno-unused-const-variable -Wno-missing-field-initializers "-I$outputDir" -c $cPath -o (Join-Path $outputDir ($fixture.Name + ".o"))
        if ($LASTEXITCODE -ne 0) { throw "Host error boundary strict C compile failed" }
        foreach ($backend in @("xlir", "legacy")) {
            Invoke-Captured $xl ('run "{0}" --backend {1}' -f $source,$backend) $fixture.Exit $fixture.Marker
            $exe = Join-Path $outputDir ($fixture.Name + ".$backend.exe")
            Invoke-Captured $xl ('build "{0}" --backend {1} -o "{2}"' -f $source,$backend,$exe) 0
            Invoke-Captured $exe "" $fixture.Exit $fixture.Marker
        }
    }
    # These are verified artifact outcomes, not XSON- or symbol-name patches.
    $shell = [System.IO.File]::ReadAllText((Join-Path $Root "src\cgen\xl6_ir_cgen_function_shell.c"))
    foreach ($required in @("if (!xlir_module_init()) return 1;", "if (!result.ok) return 1;",
        "if (ok) xrtClearError();", "XL6_IR_ERROR_ABI_MAY_RAISE")) {
        if (!$shell.Contains($required)) { throw "Host boundary lost explicit outcome handling: $required" }
    }
} finally { Pop-Location }
Write-Output "XLIR host error boundary ok: native diagnostics survive inside the body, successful script/main/module-init boundaries do not fabricate exceptions, nonzero user exit is preserved, real script/main/initializer errors remain uncaught, deterministic strict C and six XLIR/legacy run/build contracts."
exit 0
