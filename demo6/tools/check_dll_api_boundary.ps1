param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$headerPath = Join-Path $Root "include\xl.h"
$xlcHeaderPath = Join-Path $Root "include\xlc.h"
$apiPath = Join-Path $Root "src\api\xl_api.c"
$docName = "DLL" + ([char]0x5D4C) + ([char]0x5165) + ([char]0x5F0F) + ([char]0x8FD0) + ([char]0x884C) + ([char]0x5E93) + ([char]0x8BBE) + ([char]0x8BA1) + ".md"
$docPath = Join-Path (Join-Path $Root "docs") $docName
$buildDllPath = Join-Path $Root "build_dll.bat"
$minPublicApis = 61
$minDllHostExamples = 6
$failures = New-Object System.Collections.Generic.List[string]

function Add-Failure($message)
{
    $script:failures.Add($message) | Out-Null
}

function Read-Utf8Text($path)
{
    return [System.IO.File]::ReadAllText($path, [System.Text.Encoding]::UTF8)
}

function Remove-CCommentsAndStrings($text)
{
    $builder = New-Object System.Text.StringBuilder
    $state = "normal"
    $escape = $false
    $i = 0

    while ( $i -lt $text.Length ) {
        $ch = $text[$i]
        $next = [char]0
        if ( $i + 1 -lt $text.Length ) {
            $next = $text[$i + 1]
        }

        if ( $state -eq "normal" ) {
            if ( $ch -eq [char]47 -and $next -eq [char]47 ) {
                [void]$builder.Append("  ")
                $state = "line-comment"
                $i += 2
                continue
            }
            if ( $ch -eq [char]47 -and $next -eq [char]42 ) {
                [void]$builder.Append("  ")
                $state = "block-comment"
                $i += 2
                continue
            }
            if ( $ch -eq [char]34 ) {
                [void]$builder.Append(" ")
                $state = "string"
                $escape = $false
                $i++
                continue
            }
            if ( $ch -eq [char]39 ) {
                [void]$builder.Append(" ")
                $state = "char"
                $escape = $false
                $i++
                continue
            }
            [void]$builder.Append($ch)
            $i++
            continue
        }

        if ( $state -eq "line-comment" ) {
            if ( $ch -eq [char]10 -or $ch -eq [char]13 ) {
                [void]$builder.Append($ch)
                $state = "normal"
            } else {
                [void]$builder.Append(" ")
            }
            $i++
            continue
        }

        if ( $state -eq "block-comment" ) {
            if ( $ch -eq [char]42 -and $next -eq [char]47 ) {
                [void]$builder.Append("  ")
                $state = "normal"
                $i += 2
                continue
            }
            if ( $ch -eq [char]10 -or $ch -eq [char]13 ) {
                [void]$builder.Append($ch)
            } else {
                [void]$builder.Append(" ")
            }
            $i++
            continue
        }

        if ( $state -eq "string" -or $state -eq "char" ) {
            if ( $ch -eq [char]10 -or $ch -eq [char]13 ) {
                [void]$builder.Append($ch)
                $state = "normal"
                $escape = $false
                $i++
                continue
            }
            [void]$builder.Append(" ")
            if ( $escape ) {
                $escape = $false
            } elseif ( $ch -eq [char]92 ) {
                $escape = $true
            } elseif ( ($state -eq "string" -and $ch -eq [char]34) -or ($state -eq "char" -and $ch -eq [char]39) ) {
                $state = "normal"
            }
            $i++
            continue
        }
    }

    return $builder.ToString()
}

foreach ( $path in @($headerPath, $xlcHeaderPath, $apiPath, $docPath, $buildDllPath) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        Add-Failure "required DLL API file missing: $path"
    }
}

if ( $failures.Count -eq 0 ) {
    $header = Read-Utf8Text $headerPath
    $xlcHeader = Read-Utf8Text $xlcHeaderPath
    $publicHeaders = $header + "`n" + $xlcHeader
    $api = Read-Utf8Text $apiPath
    $doc = Read-Utf8Text $docPath
    $buildDll = Read-Utf8Text $buildDllPath
    $headerMatches = New-Object System.Collections.Generic.List[object]
    $headerApis = New-Object System.Collections.Generic.HashSet[string]
    $apiDefinitions = New-Object System.Collections.Generic.HashSet[string]
    $exampleDir = Join-Path $Root "examples"
    $exampleSources = @(Get-ChildItem -LiteralPath $exampleDir -Filter "xl_dll_*.c" -File)
    $exampleTextBuilder = New-Object System.Text.StringBuilder
    $exampleCode = ""

    if ( $exampleSources.Count -eq 0 ) {
        Add-Failure "no xl.dll C host examples found under examples"
    } else {
        foreach ( $source in $exampleSources ) {
            $sourceText = Read-Utf8Text $source.FullName
            $cleanSourceText = Remove-CCommentsAndStrings $sourceText
            [void]$exampleTextBuilder.AppendLine($cleanSourceText)
        }
        $exampleCode = $exampleTextBuilder.ToString()
    }

    if ( $exampleSources.Count -lt $minDllHostExamples ) {
        Add-Failure "xl.dll C host example baseline regressed: $($exampleSources.Count) < $minDllHostExamples"
    }

    foreach ( $line in ($publicHeaders -split "`r?`n") ) {
        $match = [regex]::Match($line, '^\s*(?:XL_API|XLC_API)\s+(.+?)\s+((?:xl|xlc)[A-Za-z_][A-Za-z0-9_]*)\s*\(([^;]*)\)\s*;')
        if ( $match.Success ) {
            $headerMatches.Add($match) | Out-Null
        }
    }

    if ( $headerMatches.Count -eq 0 ) {
        Add-Failure "public DLL headers have no API declarations"
    }

    if ( $headerMatches.Count -lt $minPublicApis ) {
        Add-Failure "public DLL API baseline regressed: $($headerMatches.Count) < $minPublicApis"
    }

    foreach ( $match in $headerMatches ) {
        $name = $match.Groups[2].Value
        if ( ! $headerApis.Add($name) ) {
            Add-Failure "duplicate public API declaration in DLL headers: $name"
        }
    }

    foreach ( $line in ($api -split "`r?`n") ) {
        if ( $line -match '^\s' ) {
            continue
        }
        if ( $line -match '^\s*(static|typedef|#)' ) {
            continue
        }
        $match = [regex]::Match($line, '^\s*[A-Za-z_][A-Za-z0-9_\s\*]*\b(xl[A-Za-z_][A-Za-z0-9_]*)\s*\(')
        if ( $match.Success ) {
            $name = $match.Groups[1].Value
            if ( ! $apiDefinitions.Add($name) ) {
                Add-Failure "duplicate public xl* definition in src/api/xl_api.c: $name"
            }
        }
    }

    foreach ( $name in ($headerApis | Sort-Object) ) {
        if ( ! $apiDefinitions.Contains($name) ) {
            Add-Failure "public API declaration has no implementation: $name"
        }

        if ( $doc -notmatch ('(?<![A-Za-z0-9_])' + [regex]::Escape($name) + '(?![A-Za-z0-9_])') ) {
            Add-Failure "DLL API design document does not mention public API: $name"
        }

        if ( $name.StartsWith("xl") -and ! $name.StartsWith("xlc") -and
             $exampleCode -notmatch ('(?<![A-Za-z0-9_])' + [regex]::Escape($name) + '\s*\(') ) {
            Add-Failure "xl.dll C host examples do not call public API: $name"
        }
    }

    foreach ( $name in ($apiDefinitions | Sort-Object) ) {
        if ( ! $headerApis.Contains($name) ) {
            Add-Failure "public xl*/xlc* implementation is not declared in DLL public headers: $name"
        }
    }

    if ( $header -match 'xlUninit|xlUnInit' -or $api -match 'xlUninit|xlUnInit' -or $doc -match 'xlUninit|xlUnInit' ) {
        Add-Failure "DLL API must not use Uninit/UnInit lifecycle naming"
    }

    if ( $header -notmatch '#define\s+XL_API_VERSION\s+1u' ) {
        Add-Failure "include/xl.h must keep XL_API_VERSION at 1u for current RC boundary"
    }

    if ( $api -match 'xl__runtimeCount' ) {
        Add-Failure "xl.dll runtime lifecycle must keep manual/context reference counts separated"
    }

    foreach ( $name in @(
        "xl__runtimeManualCount",
        "xl__runtimeContextCount",
        "xlRuntimeAcquireManualUnlocked",
        "xlRuntimeReleaseManualUnlocked",
        "xlRuntimeAcquireContextUnlocked",
        "xlRuntimeReleaseContextUnlocked"
    ) ) {
        if ( $api -notmatch ('(?<![A-Za-z0-9_])' + [regex]::Escape($name) + '(?![A-Za-z0-9_])') ) {
            Add-Failure "xl.dll runtime lifecycle missing separated reference helper: $name"
        }
    }

    foreach ( $name in @(
        "xlUnloadTccApi",
        "XL_TccApi loaded",
        "*api = loaded",
        "loaded.tcc_set_options == NULL",
        "xlLoadTccFromBasePath"
    ) ) {
        if ( $api -notmatch [regex]::Escape($name) ) {
            Add-Failure "xl.dll libtcc loader must keep local-load-then-commit rollback pattern: $name"
        }
    }

    if ( $api -match 'api->module\s*=\s*(LoadLibraryW|dlopen)' ) {
        Add-Failure "xlLoadTccFromPath must not write module handle into global xl__tcc before symbol validation"
    }

    if ( $api -notmatch 'getenv\s*\(\s*name\s*\)' ) {
        Add-Failure "xl.dll TCC defaults must use a shared getenv helper for centralized toolchain variables"
    }

    foreach ( $name in @(
        "XL6_TCC_DLL",
        "XL6_TCC_ROOT",
        "XL6_TCC_INCLUDE",
        "XL6_TCC_WIN_INCLUDE",
        "XL6_TCC_WINAPI_INCLUDE",
        "XL6_TCC_LIB",
        "XL6_TCC_LIBTCC1"
    ) ) {
        if ( $api -notmatch [regex]::Escape($name) ) {
            Add-Failure "xl.dll TCC defaults must honor centralized toolchain environment variable: $name"
        }
    }

    if ( $api -notmatch 'XRT_IMPLEMENTATION' -or $api -notmatch 'lib/xrt/xrt[.]h' ) {
        Add-Failure "xlc.dll exe compiler path must provide XRT through the single-header implementation"
    }

    foreach ( $name in @(
        "XL_LoadedLibrary",
        "loadedLibraries",
        "loadedLibraryCount",
        "baseDir",
        "xlGetCurrentDirText",
        "xlPathSnapshot",
        "xlPathSnapshotFromCurrentDir",
        "xlTextLooksLikePath",
        "xlLinkValueSnapshotFromCurrentDir",
        "xlCloseLoadedLibrary",
        "xlLoadedLibrariesFree",
        "xlContextAddLoadedLibrary"
    ) ) {
        if ( $api -notmatch ('(?<![A-Za-z0-9_])' + [regex]::Escape($name) + '(?![A-Za-z0-9_])') ) {
            Add-Failure "xl.dll dependency library handles must be owned by context: $name"
        }
    }

    if ( $exampleCode -notmatch 'run_default_toolchain_snapshot_case\s*\(' ) {
        Add-Failure "xl.dll C host examples must cover relative workDir with default toolchain path snapshots"
    }

    if ( $exampleCode -notmatch 'run_dependency_path_snapshot_case\s*\(' ) {
        Add-Failure "xl.dll C host examples must cover relative dependency path snapshots across workDir changes"
    }

    if ( $api -match 'XL_StringList\s+loadLibraries' -or $api -match 'xlStringListAppend\s*\(\s*&ctx->loadLibraries' ) {
        Add-Failure "xlLoadDependencyLibrary must not store dependency libraries as path-only string entries"
    }

    if ( $api -notmatch '(?s)int\s+xlAddIncludePath\s*\(.*?xlPathSnapshotFromCurrentDir\(ctx,\s*path' ) {
        Add-Failure "xlAddIncludePath must snapshot relative include paths at API call time"
    }

    if ( $api -notmatch '(?s)int\s+xlAddLibraryPath\s*\(.*?xlPathSnapshotFromCurrentDir\(ctx,\s*path' ) {
        Add-Failure "xlAddLibraryPath must snapshot relative library paths at API call time"
    }

    if ( $api -notmatch '(?s)int\s+xlAddLinkLibrary\s*\(.*?xlLinkValueSnapshotFromCurrentDir\(ctx,\s*nameOrPath' ) {
        Add-Failure "xlAddLinkLibrary must snapshot path-like link values while preserving bare library names"
    }

    if ( $api -notmatch '(?s)int\s+xlLoadDependencyLibrary\s*\(.*?snapshot\s*=\s*xlPathSnapshotFromCurrentDir\(ctx,\s*path.*?LoadLibraryW\(\(LPCWSTR\)pathW\).*?xlContextAddLoadedLibrary\(ctx,\s*snapshot' ) {
        Add-Failure "xlLoadDependencyLibrary must load and remember the dependency library using the same path snapshot"
    }

    if ( $api -notmatch '(?s)static\s+int\s+xlContextAddSymbol.*?nameCopy\s*=\s*xlDupText\(name\).*?xrtRealloc\(ctx->symbols' ) {
        Add-Failure "xlContextAddSymbol must duplicate symbol name before growing the symbol table"
    }

    if ( $api -notmatch '(?s)static\s+int\s+xlContextAddLoadedLibrary.*?pathCopy\s*=\s*xlDupText\(path\).*?xrtRealloc\(ctx->loadedLibraries' ) {
        Add-Failure "xlContextAddLoadedLibrary must duplicate path before growing the loaded library table"
    }

    if ( $buildDll -notmatch 'src\\api\\xl_api[.]c' ) {
        Add-Failure "build_dll.bat does not compile src\\api\\xl_api.c"
    }
    if ( $buildDll -notmatch 'tcc_vfs_manifest_win64_runtime[.]txt' -or
         $buildDll -notmatch 'tcc_vfs_manifest_win64_xlc[.]txt' ) {
        Add-Failure "build_dll.bat must generate separate runtime and compiler VFS manifests for xl.dll/xlc.dll"
    }
    if ( $buildDll -match 'copy\s+/Y\s+"%XL6_TCC_DLL%"\s+release\\libtcc[.]dll' ) {
        Add-Failure "build_dll.bat must not copy external libtcc.dll into release"
    }
}

if ( $failures.Count -gt 0 ) {
    foreach ( $failure in $failures ) {
        Write-Host $failure
    }
    exit 1
}

Write-Host "DLL API boundary ok: $($headerApis.Count) public APIs."
exit 0
