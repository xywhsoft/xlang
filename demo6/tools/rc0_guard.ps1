param(
    [switch]$SkipFullTests
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$Root = Split-Path -Parent $ScriptDir
$Failures = New-Object System.Collections.Generic.List[string]

Set-Location $Root

function Add-Failure {
    param([string]$Message)
    $script:Failures.Add($Message) | Out-Null
}

function Invoke-GuardStep {
    param(
        [string]$Name,
        [scriptblock]$Body
    )

    Write-Host ""
    Write-Host "== $Name"
    try {
        & $Body
        Write-Host "OK: $Name"
    } catch {
        $message = "$Name failed: $($_.Exception.Message)"
        Add-Failure $message
        Write-Host "FAIL: $message"
    }
}

function Invoke-Cmd {
    param(
        [string]$Command,
        [string[]]$CommandArgs
    )

    & $Command @CommandArgs
    if ( $LASTEXITCODE -ne 0 ) {
        throw "$Command exited with code $LASTEXITCODE"
    }
}

function Invoke-CheckScript {
    param([string]$Name)

    Invoke-Cmd "powershell.exe" @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "tools\$Name")
}

function Get-CheckedSourceFiles {
    param([string[]]$Roots)

    $all = New-Object System.Collections.Generic.List[string]
    foreach ( $rootItem in $Roots ) {
        $full = Join-Path $Root $rootItem
        if ( Test-Path -LiteralPath $full -PathType Leaf ) {
            $all.Add($full) | Out-Null
        } elseif ( Test-Path -LiteralPath $full -PathType Container ) {
            Get-ChildItem -LiteralPath $full -Recurse -File |
                Where-Object { $_.Extension -in ".c", ".h", ".y", ".l", ".bat", ".ps1" } |
                ForEach-Object { $all.Add($_.FullName) | Out-Null }
        } else {
            throw "missing path: $rootItem"
        }
    }
    return $all.ToArray()
}

function Assert-NoPattern {
    param(
        [string]$Name,
        [string[]]$Roots,
        [string]$Pattern,
        [switch]$CaseSensitive
    )

    $files = Get-CheckedSourceFiles $Roots
    if ( $files.Count -eq 0 ) {
        throw "$Name has no files to scan"
    }

    if ( $CaseSensitive ) {
        $matches = @(Select-String -Path $files -Pattern $Pattern -AllMatches -CaseSensitive)
    } else {
        $matches = @(Select-String -Path $files -Pattern $Pattern -AllMatches)
    }
    if ( $matches.Count -gt 0 ) {
        $sample = $matches |
            Select-Object -First 20 |
            ForEach-Object { "{0}:{1}: {2}" -f $_.Path, $_.LineNumber, $_.Line.Trim() }
        throw "$Name found $($matches.Count) match(es):`n$($sample -join "`n")"
    }
}

function Assert-NoPatternExcept {
    param(
        [string]$Name,
        [string[]]$Roots,
        [string]$Pattern,
        [string]$AllowedPathPattern,
        [switch]$CaseSensitive
    )

    $files = Get-CheckedSourceFiles $Roots
    if ( $files.Count -eq 0 ) {
        throw "$Name has no files to scan"
    }

    if ( $CaseSensitive ) {
        $allMatches = @(Select-String -Path $files -Pattern $Pattern -AllMatches -CaseSensitive)
    } else {
        $allMatches = @(Select-String -Path $files -Pattern $Pattern -AllMatches)
    }

    $matches = @($allMatches | Where-Object { $_.Path -notmatch $AllowedPathPattern })
    if ( $matches.Count -gt 0 ) {
        $sample = $matches |
            Select-Object -First 20 |
            ForEach-Object { "{0}:{1}: {2}" -f $_.Path, $_.LineNumber, $_.Line.Trim() }
        throw "$Name found $($matches.Count) match(es):`n$($sample -join "`n")"
    }
}

function Assert-FileExists {
    param([string]$Path)
    $full = Join-Path $Root $Path
    if ( ! (Test-Path -LiteralPath $full -PathType Leaf) ) {
        throw "missing file: $Path"
    }
}

function Invoke-XlCaptureRaw {
    param(
        [string[]]$CommandArgs,
        [string]$StdoutPath,
        [string]$StderrPath
    )

    foreach ( $path in @($StdoutPath, $StderrPath) ) {
        if ( Test-Path -LiteralPath $path -PathType Leaf ) {
            Remove-Item -LiteralPath $path -Force
        }
    }

    $xl = Join-Path $Root "release\xl.exe"
    $process = Start-Process `
        -FilePath $xl `
        -ArgumentList $CommandArgs `
        -NoNewWindow `
        -PassThru `
        -Wait `
        -RedirectStandardOutput $StdoutPath `
        -RedirectStandardError $StderrPath

    return $process.ExitCode
}

function Assert-RawNativeOutput {
    param(
        [string]$Name,
        [string]$Path
    )

    if ( ! (Test-Path -LiteralPath $Path -PathType Leaf) ) {
        return
    }

    $text = Get-Content -LiteralPath $Path -Raw
    if ( $text -match "CategoryInfo|FullyQualifiedErrorId|NativeCommandError" ) {
        throw "$Name output was wrapped by PowerShell native error formatting"
    }
}

function Invoke-XlToFile {
    param(
        [string]$Name,
        [string[]]$CommandArgs,
        [string]$OutputPath
    )

    $stderrPath = "$OutputPath.stderr"
    $exitCode = Invoke-XlCaptureRaw $CommandArgs $OutputPath $stderrPath
    Assert-RawNativeOutput $Name $OutputPath
    Assert-RawNativeOutput $Name $stderrPath
    if ( $exitCode -ne 0 ) {
        throw "$Name failed with code $exitCode"
    }
}

function Invoke-Xl {
    param(
        [string]$Name,
        [string[]]$CommandArgs
    )

    $xl = Join-Path $Root "release\xl.exe"
    & $xl @CommandArgs
    if ( $LASTEXITCODE -ne 0 ) {
        throw "$Name failed with code $LASTEXITCODE"
    }
}

function Invoke-XlExpectJsonFailure {
    param(
        [string]$Name,
        [string[]]$CommandArgs,
        [string]$OutputPath,
        [string]$ExpectedPhase
    )

    $stderrPath = "$OutputPath.stderr"
    $exitCode = Invoke-XlCaptureRaw $CommandArgs $OutputPath $stderrPath
    Assert-RawNativeOutput $Name $OutputPath
    Assert-RawNativeOutput $Name $stderrPath
    if ( $exitCode -eq 0 ) {
        throw "$Name unexpectedly succeeded"
    }

    $raw = Get-Content -LiteralPath $OutputPath -Raw
    $json = $raw | ConvertFrom-Json
    if ( $json.ok -ne $false ) {
        throw "$Name did not report ok=false"
    }
    if ( $json.phase -ne $ExpectedPhase ) {
        throw "$Name phase mismatch: expected $ExpectedPhase, actual $($json.phase)"
    }
    $diagnostics = @($json.diagnostics)
    if ( $diagnostics.Count -lt 1 ) {
        throw "$Name emitted no diagnostics"
    }
    $first = $diagnostics[0]
    if ( [string]::IsNullOrEmpty($first.file) -or $first.line -lt 1 -or $first.column -lt 1 -or [string]::IsNullOrEmpty($first.message) ) {
        throw "$Name emitted an incomplete diagnostic"
    }
}

function Invoke-XlExpectTextFailure {
    param(
        [string]$Name,
        [string[]]$CommandArgs,
        [string]$OutputPath,
        [string]$ExpectedPhase
    )

    $stdoutPath = "$OutputPath.stdout"
    $exitCode = Invoke-XlCaptureRaw $CommandArgs $stdoutPath $OutputPath
    Assert-RawNativeOutput $Name $stdoutPath
    Assert-RawNativeOutput $Name $OutputPath
    if ( $exitCode -eq 0 ) {
        throw "$Name unexpectedly succeeded"
    }

    $text = Get-Content -LiteralPath $OutputPath -Raw
    if ( $text -notmatch "\[$ExpectedPhase\]" ) {
        throw "$Name text diagnostic does not contain phase [$ExpectedPhase]"
    }
    if ( $text -notmatch ":\d+:\d+:" ) {
        throw "$Name text diagnostic does not contain source location"
    }
}

Invoke-GuardStep "required files" {
    Assert-FileExists "build.bat"
    Assert-FileExists "test_all.bat"
    Assert-FileExists "src\common\xl6_output.c"
    Assert-FileExists "src\common\xl6_output.h"
    Assert-FileExists "lib\xrt\xrt.h"
}

Invoke-GuardStep "static output boundary" {
    Assert-NoPattern "removed output compat header" @("src") "xl6_output_compat"
    Assert-NoPattern "codegen and dump CRT text output" @("src\cgen", "src\common", "src\abi", "src\ast", "src\ir") "\b(fputc|fputs|fprintf)\s*\("
    Assert-NoPattern "codegen direct stdio output" @("src\cgen") "(FILE\s*\*)|(\bstdout\b)|(\bstderr\b)" -CaseSensitive
    Assert-NoPattern "stale xvalue scalar conversion fallback" @("src\cgen") "__xl6_conv_" -CaseSensitive
}

Invoke-GuardStep "static XRT boundary" {
    Assert-NoPattern "removed common file wrappers" @("src") "xl6(?!Std)(?!OutputOpenFile\b)[A-Za-z0-9_]*(ReadFileText|OpenFile|FileExists|CreateDirAll|StrDupRange)|xl6_file\.c"
    Assert-NoPattern "removed custom time literal wrapper" @("src") "xl6_time_literal"
    Assert-NoPattern "codegen CRT heap allocation" @("src\cgen") "\b(malloc|calloc|realloc|free)\s*\(" -CaseSensitive
    Assert-NoPattern "CRT file path I/O" @("src\driver", "src\package", "src\compile", "src\api", "src\backend", "src\sem", "src\ast", "src\ir", "src\abi", "src\cgen", "src\common") "\b(fopen|_wfopen|fread|fclose|fseek|ftell|rename|remove|_unlink|unlink|_rmdir|rmdir)\s*\(" -CaseSensitive
    Assert-NoPattern "CRT buffered file output outside common" @("src\driver", "src\package", "src\compile", "src\api", "src\backend", "src\sem", "src\ast", "src\ir", "src\abi", "src\cgen") "\b(fwrite|fflush)\s*\(" -CaseSensitive
    $outputFilePattern = "\\src\\common\\xl6_output" + "\.c$"
    Assert-NoPatternExcept "CRT buffered file output in common" @("src\common") "\b(fwrite|fflush)\s*\(" $outputFilePattern -CaseSensitive
    Assert-NoPattern "direct CRT/Win32 command execution" @("src") "\b(system|popen|_popen|WinExec|ShellExecute[A-Za-z]*|CreateProcess[A-Za-z]*)\s*\(" -CaseSensitive
    Assert-NoPattern "CRT time in CLI" @("src\driver") "\btime\s*\(" -CaseSensitive
    Assert-NoPattern "CRT file deletion" @("src\driver", "src\package", "src\common") "\b(remove|_rmdir|rmdir)\s*\("
    Assert-NoPattern "shell directory creation" @("src\driver", "src\package", "src\common") "\bsystem\s*\(\s*`"[^`"]*\bmkdir\b"
    Assert-NoPattern "unchecked CLI temp dir creation" @("src\driver") "(?m)^\s*xl6CliEnsureTmpDir\s*\(\s*\)\s*;"
}

Invoke-GuardStep "static wiring guards" {
    $scripts = @(
        "check_docs_index.ps1",
        "check_cgenrule_docs.ps1",
        "check_risk_ledger.ps1",
        "check_cgen_statement_expr_allowlist.ps1",
        "check_cgen_module_size.ps1",
        "check_markdown_links.ps1",
        "check_tool_wiring.ps1",
        "check_batch_wiring.ps1",
        "check_cli_temp_cleanup.ps1",
        "check_source_wiring.ps1",
        "check_source_encoding.ps1",
        "check_toolchain_paths.ps1",
        "check_dll_api_boundary.ps1",
        "check_test_case_wiring.ps1",
        "check_test_data_wiring.ps1",
        "check_example_wiring.ps1",
        "check_c_fixture_wiring.ps1",
        "check_package_fixtures.ps1",
        "check_stdlib_coverage.ps1",
        "check_stdlib_test_wiring.ps1",
        "check_stdlib_dll_symbols.ps1",
        "check_release_tmp_artifacts.ps1",
        "check_negative_diagnostics.ps1"
    )

    foreach ( $script in $scripts ) {
        Invoke-CheckScript $script
    }
}

Invoke-GuardStep "handwritten C safety" {
    Invoke-CheckScript "check_c_string_safety.ps1"
}

Invoke-GuardStep "build" {
    Invoke-Cmd "cmd.exe" @("/c", "build.bat")
}

Invoke-GuardStep "command surface" {
    $tmp = Join-Path $Root "release\tmp\rc0_guard"
    if ( Test-Path -LiteralPath $tmp ) {
        Remove-Item -LiteralPath $tmp -Recurse -Force
    }
    New-Item -ItemType Directory -Path $tmp | Out-Null

    $basic = "test\cases\cgen_basic.xl"
    Invoke-XlToFile "lex" @("lex", $basic, "--format", "json") (Join-Path $tmp "lex.json")
    Invoke-XlToFile "check" @("check", $basic, "--json") (Join-Path $tmp "check.json")
    Invoke-Xl "parse" @("parse", $basic, "--format", "json", "--emit", (Join-Path $tmp "parse.json"))
    Invoke-Xl "ir" @("ir", $basic, "--format", "json", "--emit", (Join-Path $tmp "ir.json"))
    Invoke-Xl "cgen" @("cgen", $basic, "--emit", (Join-Path $tmp "out.c"), "--emit-header", (Join-Path $tmp "out.h"))
    Invoke-Xl "abi" @("abi", $basic, "--json", "--emit", (Join-Path $tmp "out.abi.json"))
    Invoke-Xl "xapi" @("xapi", $basic, "--emit", (Join-Path $tmp "out.xapi.json"))
    Invoke-XlToFile "cgen stdout" @("cgen", $basic) (Join-Path $tmp "stdout.c")
    Invoke-XlToFile "run" @("run", $basic) (Join-Path $tmp "run.txt")
    Invoke-XlToFile "build" @("build", $basic, "-o", (Join-Path $tmp "app.exe")) (Join-Path $tmp "build.txt")

    foreach ( $name in @("lex.json", "check.json", "parse.json", "ir.json", "out.c", "out.h", "out.abi.json", "out.xapi.json", "stdout.c", "app.exe") ) {
        $file = Join-Path $tmp $name
        if ( ! (Test-Path -LiteralPath $file -PathType Leaf) ) {
            throw "missing command output: $name"
        }
    }
}

Invoke-GuardStep "diagnostics" {
    $tmp = Join-Path $Root "release\tmp\rc0_guard_diag"
    if ( Test-Path -LiteralPath $tmp ) {
        Remove-Item -LiteralPath $tmp -Recurse -Force
    }
    New-Item -ItemType Directory -Path $tmp | Out-Null

    Invoke-XlExpectJsonFailure "semantic json diagnostic" @("check", "test\cases\sem_unknown_type.xl", "--json") (Join-Path $tmp "sem.json") "sem"
    Invoke-XlExpectJsonFailure "import json diagnostic" @("check", "test\cases\import_missing_xapi.xl", "--json") (Join-Path $tmp "import.json") "import"
    Invoke-XlExpectTextFailure "semantic text diagnostic" @("check", "test\cases\sem_unknown_type.xl") (Join-Path $tmp "sem.txt") "sem"
    Invoke-XlExpectTextFailure "import text diagnostic" @("check", "test\cases\import_missing_xapi.xl") (Join-Path $tmp "import.txt") "import"
}

if ( ! $SkipFullTests ) {
    Invoke-GuardStep "full tests" {
        Invoke-Cmd "cmd.exe" @("/c", "test_all.bat")
    }
}

Invoke-GuardStep "diff whitespace" {
    Set-Location (Split-Path -Parent $Root)
    & git diff --check -- .gitignore demo6
    if ( $LASTEXITCODE -ne 0 ) {
        throw "git diff --check failed"
    }
    Set-Location $Root
}

Write-Host ""
if ( $Failures.Count -gt 0 ) {
    Write-Host "RC0 guard failed:"
    foreach ( $failure in $Failures ) {
        Write-Host " - $failure"
    }
    exit 1
}

Write-Host "RC0 guard ok."
exit 0
