param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$docsDir = Join-Path $Root "docs"
$minRootBatchFiles = 10
$minToolBatchFiles = 1
$failures = New-Object System.Collections.Generic.List[string]

function Add-Failure($message)
{
    $script:failures.Add($message) | Out-Null
}

function Read-Utf8Text($path)
{
    return [System.IO.File]::ReadAllText($path, [System.Text.Encoding]::UTF8)
}

function Check-BatchAsciiBytes($path, $relative)
{
    $bytes = [System.IO.File]::ReadAllBytes($path)
    $start = 0

    if ( $bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF ) {
        $start = 3
    }

    for ( $i = $start; $i -lt $bytes.Length; $i++ ) {
        $b = $bytes[$i]
        if ( $b -gt 127 -or ($b -lt 32 -and $b -ne 9 -and $b -ne 10 -and $b -ne 13) ) {
            Add-Failure "batch file must stay ASCII after optional UTF-8 BOM: ${relative} byte $i"
            return
        }
    }
}

function Normalize-RelativePath($path)
{
    return $path.Replace("/", "\")
}

function Check-SmokeBatch($text)
{
    $forbiddenCalls = @(
        "test.bat",
        "test_all.bat",
        "test_tutorial.bat",
        "test_feature_suite.bat",
        "test_dll.bat",
        "examples\feature_suite\run_feature_suite.bat"
    )
    $requiredPatterns = @(
        '(?im)^\s*call\s+build\.bat\s*$',
        'tools\\check_docs_index\.ps1',
        'tools\\check_source_wiring\.ps1',
        'tools\\check_tool_wiring\.ps1',
        'release\\xl\.exe\s+lex\s+',
        'release\\xl\.exe\s+parse\s+',
        'release\\xl\.exe\s+check\s+',
        'release\\xl\.exe\s+cgen\s+',
        'release\\xl\.exe\s+abi\s+',
        'release\\xl\.exe\s+xapi\s+',
        'release\\xl\.exe\s+run\s+examples\\containers\.xl',
        'release\\xl\.exe\s+run\s+examples\\stdlib_string\.xl'
    )

    foreach ( $call in $forbiddenCalls ) {
        $pattern = '(?im)^\s*(call\s+)?' + [regex]::Escape($call) + '(\s|$)'
        if ( $text -match $pattern ) {
            Add-Failure "test_smoke.bat must stay a fast smoke entry and must not call ${call}"
        }
    }

    foreach ( $pattern in $requiredPatterns ) {
        if ( $text -notmatch $pattern ) {
            Add-Failure "test_smoke.bat missing smoke coverage pattern: $pattern"
        }
    }
}

function Check-AllBatch($text)
{
    $requiredPatterns = @(
        '(?im)^\s*call\s+test\.bat\s*$',
        '(?im)^\s*call\s+test_tutorial\.bat\s*$',
        '(?im)^\s*call\s+test_dll\.bat\s*$'
    )
    $previousIndex = -1

    foreach ( $pattern in $requiredPatterns ) {
        $match = [regex]::Match($text, $pattern)
        if ( ! $match.Success ) {
            Add-Failure "test_all.bat missing full regression entry pattern: $pattern"
            continue
        }

        if ( $match.Index -lt $previousIndex ) {
            Add-Failure "test_all.bat must run test.bat, test_tutorial.bat, test_dll.bat in that order"
        }
        $previousIndex = $match.Index
    }
}

function Check-TutorialBatch($text)
{
    $requiredPatterns = @(
        'for\s+%%f\s+in\s+\(examples\\tutorial\\\*\.xl\)',
        'release\\xl\.exe\s+check\s+"%%f"',
        'release\\xl\.exe\s+run\s+"examples\\tutorial\\%%f"',
        'release\\xl\.exe\s+run\s+"examples\\tutorial\\%%f"\s+--link\s+examples\\tutorial\\host\.c',
        'release\\xl\.exe\s+cgen\s+examples\\tutorial\\11_export_to_c\.xl\s+--emit\s+release\\tmp\\tutorial\\11_export_to_c\.c\s+--emit-header\s+release\\tmp\\tutorial\\11_export_to_c\.h',
        '"%XL6_TCC%"\s+-Irelease\\tmp\\tutorial\s+%XL6_TCC_INCLUDES%\s+%XL6_TCC_LIB_FLAGS%\s+release\\tmp\\tutorial\\11_export_to_c\.c\s+examples\\tutorial\\host_export\.c\s+%XL6_DEFAULT_LINKS%\s+-o\s+release\\tmp\\tutorial\\host_export\.exe',
        'release\\tmp\\tutorial\\host_export\.exe'
    )
    $requiredTutorialFiles = @(
        "01_entry_and_layout.xl",
        "02_variables_and_literals.xl",
        "03_operators.xl",
        "04_control_flow.xl",
        "05_functions.xl",
        "06_function_values_and_closures.xl",
        "07_containers.xl",
        "08_generics.xl",
        "09_class_lifecycle.xl",
        "10_c_interop_and_custom.xl",
        "11_export_to_c.xl",
        "12_errors_and_docs.xl"
    )

    foreach ( $pattern in $requiredPatterns ) {
        if ( $text -notmatch $pattern ) {
            Add-Failure "test_tutorial.bat missing tutorial coverage pattern: $pattern"
        }
    }

    foreach ( $file in $requiredTutorialFiles ) {
        if ( ! $text.Contains($file) ) {
            Add-Failure "test_tutorial.bat does not mention tutorial file: $file"
        }
    }
}

function Check-DllBatch($text)
{
    $requiredPatterns = @(
        '(?im)^\s*call\s+tools\\xl6_toolchain_env\.bat\s*$',
        '(?im)^\s*call\s+build_dll\.bat\s*$',
        'release\\xl_dll_null_context\.exe',
        'release\\xl_dll_threaded_host\.exe',
        'release\\xlc_dll_embed_string\.exe',
        'release\\xlc\.dll',
        'release\\xlc\.def',
        'release\\tmp\\xl_dll_embed_string_script\.exe',
        'release\\tmp\\xl_dll_embed_file_script\.exe',
        'release\\tmp\\xl_dll_link_host\.o',
        'release\\tmp\\libxl6dlllink\.a',
        'release\\tmp\\xl_dll_loadlib_host\.dll',
        'release\\tmp\\xl_dll_loadlib_host\.def'
    )

    foreach ( $pattern in $requiredPatterns ) {
        if ( $text -notmatch $pattern ) {
            Add-Failure "test_dll.bat missing DLL coverage pattern: $pattern"
        }
    }
}

function Check-MainRegressionBatch($text)
{
    $requiredSnippets = @(
        "build_mixed_save_temps.log 2>&1",
        "temporary files kept:\s*(.+)",
        "Remove-Item -LiteralPath `$dir -Recurse -Force"
    )

    foreach ( $snippet in $requiredSnippets ) {
        if ( ! $text.Contains($snippet) ) {
            Add-Failure "test.bat must keep save-temps wrapper verification deterministic and cleaned: $snippet"
        }
    }
}

function Check-TccCommandLine($text, $relative)
{
    $lines = $text -split '\r?\n'
    $includeVars = @("%XL6_TCC_INCLUDES%", "%XL6_TCC_INCLUDES_WINAPI%", "%XL6_TCC_SOURCE_INCLUDE%")
    $linkVars = @("%XL6_TCC_LIB_FLAGS%", "%XL6_DEFAULT_LINKS%")

    for ( $i = 0; $i -lt $lines.Count; $i++ ) {
        $line = $lines[$i]

        if ( $line -notmatch '%XL6_TCC%' -or $line -notmatch '\.c(\s|$)' ) {
            continue
        }

        if ( $line -match '(^|\s)-ar(\s|$)' -or $line -match '(^|\s)-impdef(\s|$)' ) {
            continue
        }

        $hasInclude = $false
        foreach ( $includeVar in $includeVars ) {
            if ( $line.Contains($includeVar) ) {
                $hasInclude = $true
                break
            }
        }

        if ( ! $hasInclude ) {
            Add-Failure "TCC C command missing centralized include flags: ${relative}:$($i + 1)"
        }

        $looksLikeLink = $line -match '(^|\s)-shared(\s|$)' -or $line -match '(^|\s)-o\s+.*\.(exe|dll)(\s|$)'
        if ( ! $looksLikeLink ) {
            continue
        }

        $hasLinkFlags = $false
        foreach ( $linkVar in $linkVars ) {
            if ( $line.Contains($linkVar) ) {
                $hasLinkFlags = $true
                break
            }
        }

        if ( ! $hasLinkFlags ) {
            Add-Failure "TCC link command missing centralized library flags: ${relative}:$($i + 1)"
        }
    }
}

function Check-Rc0Batch($text)
{
    if ( $text -notmatch '(?im)^\s*powershell\s+-NoProfile\s+-ExecutionPolicy\s+Bypass\s+-File\s+tools\\rc0_guard\.ps1\s+%\*\s*$' ) {
        Add-Failure "rc0_guard.bat must delegate to tools\rc0_guard.ps1 with argument forwarding"
    }

    if ( $text -notmatch '(?im)^\s*exit\s+/b\s+%errorlevel%\s*$' ) {
        Add-Failure "rc0_guard.bat must return the PowerShell guard exit code"
    }
}

if ( ! (Test-Path -LiteralPath $docsDir -PathType Container) ) {
    Write-Host "docs directory missing: $docsDir"
    exit 1
}

$baselineDocs = @(Get-ChildItem -LiteralPath $docsDir -Filter "*.md" -File | Where-Object {
    (Read-Utf8Text $_.FullName).Contains("rc0_guard.bat -SkipFullTests")
})

if ( $baselineDocs.Count -ne 1 ) {
    Write-Host "RC0 baseline doc lookup failed: expected 1 match, actual $($baselineDocs.Count)"
    exit 1
}

$baselineText = Read-Utf8Text $baselineDocs[0].FullName
$batchFiles = @(Get-ChildItem -LiteralPath $Root -File -Filter "*.bat" | Sort-Object Name)
$toolBatchFiles = @(Get-ChildItem -LiteralPath (Join-Path $Root "tools") -File -Filter "*.bat" | Sort-Object Name)
$extraBatchFiles = @(
    (Join-Path $Root "examples\feature_suite\run_feature_suite.bat")
) | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf }
$toolchainEnv = "tools\xl6_toolchain_env.bat"
$toolchainEnvPath = Join-Path $Root $toolchainEnv

if ( $batchFiles.Count -lt $minRootBatchFiles ) {
    Add-Failure "root batch file baseline regressed: $($batchFiles.Count) < $minRootBatchFiles"
}

if ( $toolBatchFiles.Count -lt $minToolBatchFiles ) {
    Add-Failure "tool batch file baseline regressed: $($toolBatchFiles.Count) < $minToolBatchFiles"
}

if ( ! (Test-Path -LiteralPath $toolchainEnvPath -PathType Leaf) ) {
    Add-Failure "toolchain environment batch is missing: $toolchainEnv"
}

foreach ( $bat in @($batchFiles + $toolBatchFiles) ) {
    $relative = $bat.FullName.Substring((Resolve-Path -LiteralPath $Root).Path.Length + 1)
    Check-BatchAsciiBytes $bat.FullName $relative
}

foreach ( $bat in $batchFiles ) {
    $text = Read-Utf8Text $bat.FullName
    $relative = $bat.FullName.Substring((Resolve-Path -LiteralPath $Root).Path.Length + 1)
    Check-TccCommandLine $text $relative

    if ( $text -notmatch '(?im)^@echo\s+off\s*$' ) {
        Add-Failure "batch file missing '@echo off': $($bat.Name)"
    }
    if ( $text -notmatch '(?im)^setlocal\b' ) {
        Add-Failure "batch file missing setlocal: $($bat.Name)"
    }
    if ( $text -notmatch '(?im)^cd\s+/d\s+%~dp0\s*$' ) {
        Add-Failure "batch file does not switch to script directory: $($bat.Name)"
    }
    if ( ! $baselineText.Contains($bat.Name) ) {
        Add-Failure "batch file is not listed in RC0 baseline doc: $($bat.Name)"
    }

    $toolchainUse = [regex]::Match($text, '(?i)%XL6_(TCC|FLEX|LEMON|XRT|DEFAULT)[A-Z0-9_]*%')
    if ( $toolchainUse.Success ) {
        $envCall = [regex]::Match($text, '(?im)^\s*call\s+tools[\\/]xl6_toolchain_env\.bat\s*$')
        if ( ! $envCall.Success ) {
            Add-Failure "batch file uses toolchain variables without calling ${toolchainEnv}: $($bat.Name)"
        } elseif ( $envCall.Index -gt $toolchainUse.Index ) {
            Add-Failure "batch file calls ${toolchainEnv} after first toolchain variable use: $($bat.Name)"
        }
    }

    foreach ( $match in [regex]::Matches($text, '(?im)^\s*call\s+([^"&|<>`r`n]+?\.bat)(?:\s|$)') ) {
        $targetText = Normalize-RelativePath ($match.Groups[1].Value.Trim())
        if ( $targetText.StartsWith("%") ) {
            continue
        }

        $targetPath = Join-Path $Root $targetText
        if ( ! (Test-Path -LiteralPath $targetPath -PathType Leaf) ) {
            Add-Failure "batch call target is missing: $($bat.Name) -> $targetText"
        }
    }

    if ( $bat.Name -eq "test_smoke.bat" ) {
        Check-SmokeBatch $text
    }
    if ( $bat.Name -eq "test_all.bat" ) {
        Check-AllBatch $text
    }
    if ( $bat.Name -eq "test_tutorial.bat" ) {
        Check-TutorialBatch $text
    }
    if ( $bat.Name -eq "test_dll.bat" ) {
        Check-DllBatch $text
    }
    if ( $bat.Name -eq "test.bat" ) {
        Check-MainRegressionBatch $text
    }
    if ( $bat.Name -eq "rc0_guard.bat" ) {
        Check-Rc0Batch $text
    }
}

foreach ( $bat in $extraBatchFiles ) {
    $text = Read-Utf8Text $bat
    $relative = $bat.Substring((Resolve-Path -LiteralPath $Root).Path.Length + 1)
    Check-TccCommandLine $text $relative
}

if ( $failures.Count -gt 0 ) {
    foreach ( $failure in $failures ) {
        Write-Host $failure
    }
    exit 1
}

Write-Host "Batch wiring ok: $($batchFiles.Count) root batch files, toolchain env and regression entries guarded."
exit 0
