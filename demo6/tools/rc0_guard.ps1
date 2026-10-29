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
    $failureCount = $script:Failures.Count
    try {
        & $Body
        if ( $script:Failures.Count -eq $failureCount ) {
            Write-Host "OK: $Name"
        } else {
            Write-Host "FAIL: $Name ($($script:Failures.Count - $failureCount) child failures)"
        }
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

function Resolve-GitCommand {
    $command = Get-Command "git" -ErrorAction SilentlyContinue
    if ( $null -ne $command ) {
        return $command.Source
    }

    $candidates = @()
    if ( $env:ProgramFiles ) {
        $candidates += (Join-Path $env:ProgramFiles "Git\cmd\git.exe")
        $candidates += (Join-Path $env:ProgramFiles "Git\bin\git.exe")
    }
    if ( ${env:ProgramFiles(x86)} ) {
        $candidates += (Join-Path ${env:ProgramFiles(x86)} "Git\cmd\git.exe")
        $candidates += (Join-Path ${env:ProgramFiles(x86)} "Git\bin\git.exe")
    }
    foreach ( $candidate in $candidates ) {
        if ( Test-Path -LiteralPath $candidate -PathType Leaf ) {
            return $candidate
        }
    }
    throw "Git is required for diff whitespace validation but git.exe was not found"
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
    # 只拦截旧 custom 语言模型相关符号；XRT 自身允许存在 Custom 命名的普通 C API。
    Assert-NoPattern "removed custom source model" @("src") "\b(KW_CUSTOM|XL6_AST_[A-Z0-9_]*CUSTOM|XL6_[A-Za-z0-9_]*CUSTOM|custom\s+[A-Za-z_])\b" -CaseSensitive
    Assert-NoPattern "removed output compat header" @("src") "xl6_output_compat"
    Assert-NoPattern "codegen and dump CRT text output" @("src\cgen", "src\common", "src\abi", "src\ast", "src\ir") "\b(fputc|fputs|fprintf)\s*\("
    # stdout/stderr 作为保留标识符文本是合法的，只禁止真实的 C 标识符引用。
    Assert-NoPattern "codegen direct stdio output" @("src\cgen") "(FILE\s*\*)|(\b(stdout|stderr)\b(?!`"))" -CaseSensitive
    Assert-NoPattern "stale xvalue scalar conversion fallback" @("src\cgen") "__xl6_conv_" -CaseSensitive
}

Invoke-GuardStep "static XRT boundary" {
    Assert-NoPattern "removed common file wrappers" @("src") "xl6(?!Std)(?!OutputOpenFile\b)[A-Za-z0-9_]*(ReadFileText|OpenFile|FileExists|CreateDirAll|StrDupRange)|xl6_file\.c"
    Assert-NoPattern "removed local time literal wrapper" @("src") "xl6_time_literal"
    Assert-NoPattern "codegen CRT heap allocation" @("src\cgen") "\b(malloc|calloc|realloc|free)\s*\(" -CaseSensitive
    Assert-NoPattern "CRT file path I/O" @("src\driver", "src\package", "src\compile", "src\api", "src\backend", "src\sem", "src\ast", "src\ir", "src\abi", "src\cgen", "src\common") "\b(fopen|_wfopen|fread|fclose|fseek|ftell|rename|remove|_unlink|unlink|_rmdir|rmdir)\s*\(" -CaseSensitive
    Assert-NoPattern "CRT buffered file output outside common" @("src\driver", "src\package", "src\compile", "src\api", "src\backend", "src\sem", "src\ast", "src\ir", "src\abi", "src\cgen") "\b(fwrite|fflush)\s*\(" -CaseSensitive
    $outputFilePattern = "\\src\\common\\xl6_output" + "\.c$"
    Assert-NoPatternExcept "CRT buffered file output in common" @("src\common") "\b(fwrite|fflush)\s*\(" $outputFilePattern -CaseSensitive
    Assert-NoPattern "direct CRT/Win32 command execution" @("src") "\b(system|popen|_popen|WinExec|ShellExecute[A-Za-z]*|CreateProcess[A-Za-z]*)\s*\(" -CaseSensitive
    Assert-NoPattern "CRT time in CLI" @("src\driver") "\btime\s*\(" -CaseSensitive
    Assert-NoPattern "CRT file deletion" @("src\driver", "src\package", "src\common") "\b(remove|_rmdir|rmdir)\s*\("
    Assert-NoPattern "shell directory creation" @("src\driver", "src\package", "src\common") "\bsystem\s*\(\s*`"[^`"]*\bmkdir\b"
}

Invoke-GuardStep "static wiring guards" {
    $scripts = @(
        "check_async_task_soak.ps1",
        "check_docs_index.ps1",
        "check_cgenrule_docs.ps1",
        "check_debugger_not_released.ps1",
        "check_risk_ledger.ps1",
        "check_cgen_statement_expr_allowlist.ps1",
        "check_cgen_module_size.ps1",
        "check_markdown_links.ps1",
        "check_tool_wiring.ps1",
        "check_xrt_snapshot.ps1",
        "check_xrt_header_abi.ps1",
        "check_xrt_ownership_contract.ps1",
        "check_xruntime_language_bridge.ps1",
        "check_batch_wiring.ps1",
        "check_cli_temp_cleanup.ps1",
        "check_source_wiring.ps1",
        "check_xlir_contract.ps1",
        "check_xlir_cgen_determinism.ps1",
        "check_xlir_cgen_runtime_matrix.ps1",
        "check_xlir_export_boundary.ps1",
        "check_xlir_global_export_boundary.ps1",
        "check_xlir_extern_global_boundary.ps1",
        "check_xlir_null_abi_boundary.ps1",
        "check_xlir_callsite_binding.ps1",
        "check_xlir_callsite_variadic.ps1",
        "check_xlir_callsite_c_vararg.ps1",
        "check_xlir_callsite_generic_specialization.ps1",
        "check_xlir_native_nominal_receiver.ps1",
        "check_xlir_native_handle_bind.ps1",
        "check_xlir_value_initializer.ps1",
        "check_xlir_reachability.ps1",
        "check_xlir_product_result.ps1",
        "check_xlir_named_function_adapter.ps1",
        "check_xlir_collection_member_intrinsics.ps1",
        "check_xlir_collection_algebra.ps1",
        "check_xlir_xvalue_array_iterator_projection.ps1",
        "check_xlir_dynamic_collection_methods.ps1",
        "check_xlir_migration_coverage.ps1",
        "check_xlir_default_backend_boundary.ps1",
        "check_source_encoding.ps1",
        "check_frontend_fuzz.ps1",
        "check_toolchain_paths.ps1",
        "check_xrt_product_trim.ps1",
        "check_dll_api_boundary.ps1",
        "check_test_case_wiring.ps1",
        "check_test_data_wiring.ps1",
        "check_example_wiring.ps1",
        "check_xlang_output_markers.ps1",
        "check_c_fixture_wiring.ps1",
        "check_package_fixtures.ps1",
        "check_release_extlibs.ps1",
        "check_extlib_audit.ps1",
        "check_extlib_aho.ps1",
        "check_extlib_argon2_bloom.ps1",
        "check_extlib_blake2.ps1",
        "check_extlib_blake3.ps1",
        "check_extlib_xxhash.ps1",
        "check_extlib_crc32.ps1",
        "check_extlib_md5.ps1",
        "check_extlib_murmur3.ps1",
        "check_extlib_sha3.ps1",
        "check_extlib_siphash128.ps1",
        "check_extlib_helper_boundaries.ps1",
        "check_extlib_sdk_features.ps1",
        "check_extlib_brotli.ps1",
        "check_extlib_smoke_process.ps1",
        "check_extlib_xregex.ps1",
        "check_extlib_mqtt.ps1",
        "check_extlib_serial.ps1",
        "check_extlib_sqlite.ps1",
        "check_format_extlibs.ps1",
        "check_generated_c_warnings.ps1",
        "check_package_xrt_symbols.ps1",
        "check_stdlib_coverage.ps1",
        "check_stdlib_crt_math_direct.ps1",
        "check_stdlib_test_wiring.ps1",
        "check_stdlib_dll_symbols.ps1",
        "check_stdlib_vfs.ps1",
        "check_stdlib_io_console.ps1",
        "check_tcc_export_environment.ps1",
        "check_tcc_vfs_resources.ps1",
        "check_tcc_xrt_contract.ps1",
        "check_release_tmp_artifacts.ps1",
        "check_negative_diagnostics.ps1"
    )

    foreach ( $script in $scripts ) {
        # Collect every independent gate before repairing. A failed child must
        # not hide later ABI/ownership failures or turn the enclosing step green.
        $checkName = $script
        Invoke-GuardStep $checkName { Invoke-CheckScript $checkName }
    }

    # Discover every specialized XLIR gate not already named above.  The
    # repository inventory is authoritative, so adding a new XLIR ABI gate
    # automatically extends the RC0 static/semantic boundary.
    Invoke-GuardStep "XLIR supplemental guards" {
        Invoke-Cmd "powershell.exe" @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "tools\run_xlir_guards.ps1", "-Mode", "Rc0Supplement")
    }
}

Invoke-GuardStep "handwritten C safety" {
    Invoke-CheckScript "check_c_string_safety.ps1"
}

Invoke-GuardStep "build" {
    Invoke-Cmd "cmd.exe" @("/c", "build.bat")
}

Invoke-GuardStep "standalone compiler VFS" {
    Invoke-CheckScript "check_standalone_compiler_vfs.ps1"
}

Invoke-GuardStep "dev project syntax boundary" {
    Invoke-Cmd "powershell.exe" @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "tools\check_dev_projects.ps1", "-Mode", "check", "-KeepGoing")
}

Invoke-GuardStep "command surface" {
    $tmp = Join-Path $Root "build\test\rc0_guard"
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
    Invoke-XlToFile "cgen stdout" @("cgen", $basic) (Join-Path $tmp "stdout.c")
    Invoke-XlToFile "run" @("run", $basic) (Join-Path $tmp "run.txt")
    Invoke-XlToFile "build" @("build", $basic, "-o", (Join-Path $tmp "app.exe")) (Join-Path $tmp "build.txt")

    foreach ( $name in @("lex.json", "check.json", "parse.json", "ir.json", "out.c", "out.h", "out.abi.json", "stdout.c", "app.exe") ) {
        $file = Join-Path $tmp $name
        if ( ! (Test-Path -LiteralPath $file -PathType Leaf) ) {
            throw "missing command output: $name"
        }
    }
}

Invoke-GuardStep "diagnostics" {
    $tmp = Join-Path $Root "build\test\rc0_guard_diag"
    if ( Test-Path -LiteralPath $tmp ) {
        Remove-Item -LiteralPath $tmp -Recurse -Force
    }
    New-Item -ItemType Directory -Path $tmp | Out-Null

    Invoke-XlExpectJsonFailure "semantic json diagnostic" @("check", "test\cases\sem_unknown_type.xl", "--json") (Join-Path $tmp "sem.json") "sem"
    Invoke-XlExpectJsonFailure "import json diagnostic" @("check", "test\cases\import_missing_target.xl", "--json") (Join-Path $tmp "import.json") "import"
    Invoke-XlExpectTextFailure "semantic text diagnostic" @("check", "test\cases\sem_unknown_type.xl") (Join-Path $tmp "sem.txt") "sem"
    Invoke-XlExpectTextFailure "import text diagnostic" @("check", "test\cases\import_missing_target.xl") (Join-Path $tmp "import.txt") "import"
}

if ( ! $SkipFullTests ) {
    Invoke-GuardStep "full tests" {
        Invoke-Cmd "cmd.exe" @("/c", "test_all.bat")
    }
}

Invoke-GuardStep "diff whitespace" {
    $git = Resolve-GitCommand
    Set-Location (Split-Path -Parent $Root)
    & $git diff --check -- .gitignore demo6
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
