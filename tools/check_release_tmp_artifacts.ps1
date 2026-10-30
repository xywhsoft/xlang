param(
    [string]$Root = "",
    [int]$MinTextArtifacts = 0
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$artifactDir = Join-Path $Root "build\test"
$failures = New-Object System.Collections.Generic.List[string]
$strictUtf8 = New-Object System.Text.UTF8Encoding($false, $true)
$textExtensions = New-Object System.Collections.Generic.HashSet[string] ([System.StringComparer]::OrdinalIgnoreCase)
$files = @()
foreach ( $ext in @(".txt", ".json", ".xlir", ".c", ".h", ".def", ".log", ".out", ".ast") ) {
    $textExtensions.Add($ext) | Out-Null
}

function Add-Failure($message)
{
    $script:failures.Add($message) | Out-Null
}

function Test-WorkspaceBoundary
{
    $gitCommand = Get-Command "git" -ErrorAction SilentlyContinue
    if ( $null -eq $gitCommand ) {
        Add-Failure "git is required for the release workspace boundary check"
        return
    }

    $repoRootOutput = @(& $gitCommand.Source -C $Root rev-parse --show-toplevel 2>$null)
    $repoRootOk = $?
    $repoRoot = if ( $repoRootOutput.Count -gt 0 ) { $repoRootOutput[0] } else { "" }
    if ( ! $repoRootOk -or [string]::IsNullOrWhiteSpace($repoRoot) ) {
        Add-Failure "cannot resolve the git repository containing $Root"
        return
    }
    $repoRoot = $repoRoot.Trim()

    $rootPrefixOutput = @(& $gitCommand.Source -C $Root rev-parse --show-prefix 2>$null)
    $rootPrefixOk = $?
    $rootPrefix = if ( $rootPrefixOutput.Count -gt 0 ) { $rootPrefixOutput[0] } else { "" }
    if ( ! $rootPrefixOk ) {
        Add-Failure "cannot resolve the demo6 repository prefix"
        return
    }
    $rootPrefix = $rootPrefix.Trim().TrimEnd('/')
    if ($rootPrefix.Length -gt 0) { $rootPrefix += "/" }
    $rootPathspec = if ($rootPrefix.Length -gt 0) { $rootPrefix } else { "." }

    $forbiddenTrackedPatterns = @(
        '^(build|examples_out|logs|out|tmp|blog|test_min_out|test_progress_cancel|test_progress_out)/',
        '^tools/(?:.*/)?__pycache__/',
        '^release/projects/function_interop/build/',
        '^release/projects/xs/projects/',
        '^[^/]+\.(?:exe|dll|obj|o|lib|exp|pdb|ilk|def|bmp|db|db-shm|db-wal)$',
        '^(?:build_log|docs_list|err\d*|xrt_mem_report_auto)\.(?:txt|json)$',
        '^tmp_.*'
    )

    $trackedPaths = @(& $gitCommand.Source -C $repoRoot -c core.quotepath=false ls-files -- $rootPathspec)
    $trackedPathsOk = $?
    if ( ! $trackedPathsOk ) {
        Add-Failure "cannot enumerate tracked demo6 files"
        return
    }

    foreach ( $repoPath in $trackedPaths ) {
        if ( ! $repoPath.StartsWith($rootPrefix, [System.StringComparison]::OrdinalIgnoreCase) ) {
            continue
        }
        $relative = $repoPath.Substring($rootPrefix.Length)
        $fullPath = Join-Path $Root ($relative.Replace('/', '\'))
        if ( ! (Test-Path -LiteralPath $fullPath -PathType Leaf) ) {
            continue
        }

        $forbidden = $false
        foreach ( $pattern in $forbiddenTrackedPatterns ) {
            if ( $relative -match $pattern ) {
                $forbidden = $true
                break
            }
        }
        if ( $forbidden ) {
            Add-Failure "temporary artifact is tracked: $repoPath"
        }

        $leaf = [System.IO.Path]::GetFileName($relative)
        if ( @($leaf.ToCharArray() | Where-Object { [char]::IsControl($_) }).Count -gt 0 ) {
            Add-Failure "tracked filename contains a control character: $repoPath"
        }
    }

    $allowedRootFiles = New-Object System.Collections.Generic.HashSet[string] ([System.StringComparer]::OrdinalIgnoreCase)
    foreach ( $name in @(
        ".gitattributes",
        ".gitignore",
        "README_zh.md",
        "DEVELOPMENT.md",
        "DEMO6说明.md",
        "build.bat",
        "build.sh",
        "build_dll.bat",
        "build_linux.sh",
        "EXTLIB_NATIVE_TODO.md",
        "rc0_guard.bat",
        "README.md",
        "test.bat",
        "test_all.bat",
        "test_apps.bat",
        "test_dll.bat",
        "test_feature_suite.bat",
        "test_linux.sh",
        "test_smoke.bat",
        "test_tutorial.bat",
        "test_xlc_shell.bat"
    )) {
        $allowedRootFiles.Add($name) | Out-Null
    }

    foreach ( $rootFile in @(Get-ChildItem -LiteralPath $Root -Force -File) ) {
        if ( $allowedRootFiles.Contains($rootFile.Name) ) {
            continue
        }
        $repoPath = $rootPrefix + $rootFile.Name
        & $gitCommand.Source -C $repoRoot check-ignore --quiet --no-index -- $repoPath
        if ( $LASTEXITCODE -ne 0 ) {
            Add-Failure "unclassified file in demo6 root: $repoPath"
        }
    }
}

Test-WorkspaceBoundary

if ( Test-Path -LiteralPath $artifactDir -PathType Container ) {
    $files = @(
        Get-ChildItem -LiteralPath $artifactDir -Recurse -File |
            Where-Object { $textExtensions.Contains($_.Extension) } |
            Sort-Object FullName
    )
} elseif ( $MinTextArtifacts -gt 0 ) {
    Add-Failure "test artifact directory missing: $artifactDir"
}

if ( $files.Count -lt $MinTextArtifacts ) {
    Add-Failure "test text artifact baseline regressed: $($files.Count) < $MinTextArtifacts"
}

foreach ( $file in $files ) {
    try {
        $bytes = [System.IO.File]::ReadAllBytes($file.FullName)
        $text = $strictUtf8.GetString($bytes)
    } catch {
        Add-Failure "test text artifact is not strict UTF-8: $($file.FullName)"
        continue
    }

    if ( $text.IndexOf([char]0xFFFD) -ge 0 ) {
        Add-Failure "test text artifact contains UTF-8 replacement character: $($file.FullName)"
    }

    if ( $text -match "CategoryInfo|FullyQualifiedErrorId|NativeCommandError" ) {
        Add-Failure "test text artifact contains PowerShell native error wrapper: $($file.FullName)"
    }

    if ( $text.Contains("[JsonErr]") ) {
        Add-Failure "test text artifact contains XRT JSON parser debug output: $($file.FullName)"
    }

    if ( $text.Contains("code generation error") ) {
        Add-Failure "test text artifact contains generic cgen diagnostic: $($file.FullName)"
    }

    if ( $file.Extension.Equals(".c", [System.StringComparison]::OrdinalIgnoreCase) -and
         $text.Contains("({") ) {
        Add-Failure "test generated C contains GNU statement expression: $($file.FullName)"
    }
}

if ( $failures.Count -gt 0 ) {
    foreach ( $failure in $failures ) {
        Write-Host $failure
    }
    exit 1
}

Write-Host "Release workspace and test artifacts ok: $($files.Count) text files."
exit 0
