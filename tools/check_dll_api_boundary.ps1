param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$headerPath = Join-Path $Root "release\include\xl.h"
$xlcHeaderPath = Join-Path $Root "release\include\xlc.h"
$runtimeHeaderPath = Join-Path $Root "release\include\xl_runtime.h"
$extlibAbiHeaderPath = Join-Path $Root "release\include\xl_extlib_abi.h"
$apiPath = Join-Path $Root "src\api\xl_api.c"
$apiSymbolsPath = Join-Path $Root "src\api\xl_api_symbols.c"
$futureBridgePath = Join-Path $Root "src\api\xl_future_bridge.c"
$xrtSymbolsPath = Join-Path $Root "src\api\xl_xrt_symbols.inc"
$tccEnvironmentPath = Join-Path $Root "src\api\xl_tcc_standalone_env.c"
$irCgenRootPath = Join-Path $Root "src\cgen\xl6_ir_cgen.c"
$irCgenHeaderPath = Join-Path $Root "src\cgen\xl6_ir_cgen.h"
$irCgenVerifyPath = Join-Path $Root "src\cgen\xl6_ir_cgen_verify.c"
$irCgenModuleEmitPath = Join-Path $Root "src\cgen\xl6_ir_cgen_module_emit.c"
$irCgenNativeDependenciesPath = Join-Path $Root "src\cgen\xl6_ir_cgen_native_dependencies.c"
$docName = "DLL" + ([char]0x5D4C) + ([char]0x5165) + ([char]0x5F0F) + ([char]0x8FD0) + ([char]0x884C) + ([char]0x5E93) + ([char]0x8BBE) + ([char]0x8BA1) + ".md"
$docPath = Join-Path (Join-Path $Root "docs") $docName
$buildDllPath = Join-Path $Root "build_dll.bat"
$buildXrtPath = Join-Path $Root "tools\build_xrt_obj.bat"
$xrtImplPath = Join-Path $Root "tools\xrt_single_header_impl.c"
$vendoredXrtPath = Join-Path $Root "lib\xrt\xrt.h"
$runtimeManifestPath = Join-Path $Root "tools\tcc_vfs_manifest_win64_runtime.txt"
$compilerManifestPath = Join-Path $Root "tools\tcc_vfs_manifest_win64_xlc.txt"
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

function Normalize-VfsName($text)
{
    return $text.Replace("\", "/").Trim("/").ToLowerInvariant()
}

function Get-ManifestEntries($text, $name)
{
    $entries = New-Object System.Collections.Generic.List[object]
    $seen = New-Object System.Collections.Generic.HashSet[string]
    $lineNo = 0

    foreach ( $rawLine in ($text -split "`r?`n") ) {
        $lineNo += 1
        $line = $rawLine.Trim()
        if ( $line.Length -eq 0 -or $line.StartsWith("#") ) {
            continue
        }

        $parts = $line.Split("|")
        if ( $parts.Count -lt 3 ) {
            Add-Failure "$name manifest has bad line ${lineNo}: $rawLine"
            continue
        }

        $kind = $parts[0].Trim().ToLowerInvariant()
        if ( $kind -ne "dir" -and $kind -ne "file" ) {
            Add-Failure "$name manifest has unknown entry kind at line ${lineNo}: $kind"
            continue
        }

        $virtual = Normalize-VfsName $parts[1].Trim()
        if ( $virtual.Length -eq 0 ) {
            Add-Failure "$name manifest has empty virtual path at line ${lineNo}"
            continue
        }

        $key = "$kind|$virtual"
        if ( ! $seen.Add($key) ) {
            Add-Failure "$name manifest has duplicate virtual entry: $key"
        }

        $entries.Add([pscustomobject]@{
            Kind = $kind
            Virtual = $virtual
            Source = $parts[2].Trim()
            Line = $lineNo
        }) | Out-Null
    }

    return $entries
}

function Test-ManifestEntry($entries, $kind, $virtual)
{
    $normalized = Normalize-VfsName $virtual
    foreach ( $entry in $entries ) {
        if ( $entry.Kind -eq $kind -and $entry.Virtual -eq $normalized ) {
            return $true
        }
    }
    return $false
}

function Test-ManifestVirtual($entries, $virtual)
{
    $normalized = Normalize-VfsName $virtual
    foreach ( $entry in $entries ) {
        if ( $entry.Virtual -eq $normalized ) {
            return $true
        }
    }
    return $false
}

function Test-ManifestFileSourceSuffix($entries, $virtual, $suffix)
{
    $normalized = Normalize-VfsName $virtual
    $normalizedSuffix = $suffix.Replace("\", "/").ToLowerInvariant()
    foreach ( $entry in $entries ) {
        $source = $entry.Source.Replace("\", "/").ToLowerInvariant()
        if ( $entry.Kind -eq "file" -and
             $entry.Virtual -eq $normalized -and
             $source.EndsWith($normalizedSuffix) ) {
            return $true
        }
    }
    return $false
}

function Get-BuildBlock($text, $label)
{
    $pattern = '(?s)echo ' + [regex]::Escape($label) + '\.\.\.(.*?)(?:\r?\nif errorlevel 1 \()'
    $match = [regex]::Match($text, $pattern)
    if ( ! $match.Success ) {
        Add-Failure "build_dll.bat missing build block: $label"
        return ""
    }
    return $match.Groups[1].Value
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

foreach ( $path in @(
    $headerPath,
    $xlcHeaderPath,
    $runtimeHeaderPath,
    $extlibAbiHeaderPath,
    $apiPath,
    $apiSymbolsPath,
    $futureBridgePath,
    $xrtSymbolsPath,
    $tccEnvironmentPath,
    $irCgenRootPath,
    $irCgenHeaderPath,
    $irCgenVerifyPath,
    $irCgenModuleEmitPath,
    $irCgenNativeDependenciesPath,
    $docPath,
    $buildDllPath,
    $buildXrtPath,
    $xrtImplPath,
    $vendoredXrtPath,
    $runtimeManifestPath,
    $compilerManifestPath
) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        Add-Failure "required DLL API file missing: $path"
    }
}

if ( $failures.Count -eq 0 ) {
    $header = Read-Utf8Text $headerPath
    $xlcHeader = Read-Utf8Text $xlcHeaderPath
    $runtimeHeader = Read-Utf8Text $runtimeHeaderPath
    $extlibAbiHeader = Read-Utf8Text $extlibAbiHeaderPath
    $publicHeaders = $header + "`n" + $xlcHeader
    $api = Read-Utf8Text $apiPath
    $apiSymbols = Read-Utf8Text $apiSymbolsPath
    $futureBridge = Read-Utf8Text $futureBridgePath
    $xrtSymbols = Read-Utf8Text $xrtSymbolsPath
    $tccEnvironment = Read-Utf8Text $tccEnvironmentPath
    $irCgenRoot = Read-Utf8Text $irCgenRootPath
    $irCgenHeader = Read-Utf8Text $irCgenHeaderPath
    $irCgenVerify = Read-Utf8Text $irCgenVerifyPath
    $irCgenModuleEmit = Read-Utf8Text $irCgenModuleEmitPath
    $irCgenNativeDependencies = Read-Utf8Text $irCgenNativeDependenciesPath
    $doc = Read-Utf8Text $docPath
    $buildDll = Read-Utf8Text $buildDllPath
    $buildXrt = Read-Utf8Text $buildXrtPath
    $xrtImpl = Read-Utf8Text $xrtImplPath
    $runtimeManifest = Read-Utf8Text $runtimeManifestPath
    $compilerManifest = Read-Utf8Text $compilerManifestPath
    $runtimeManifestEntries = Get-ManifestEntries $runtimeManifest "runtime"
    $compilerManifestEntries = Get-ManifestEntries $compilerManifest "compiler"
    $xlRuntimeBuildBlock = Get-BuildBlock $buildDll "Building DEMO6 xl.dll runtime"
    $xlcBuildBlock = Get-BuildBlock $buildDll "Building DEMO6 xlc.dll compiler runtime"

    # XRT/xruntime declarations have one generated product source. The runtime
    # header may only include that SDK header and must not mirror prototypes.
    if ( $runtimeHeader -notmatch '(?m)^\s*#include\s+"xruntime_decl[.]h"\s*$' ) {
        Add-Failure "release/include/xl_runtime.h must include the generated xruntime declaration header"
    }
    if ( $runtimeHeader -notmatch '(?m)^\s*#include\s+"xl_extlib_abi[.]h"\s*$' -or
         $runtimeHeader -match '(?m)^\s*struct\s+xl6_typed_container\s*\{' ) {
        Add-Failure "xl_runtime.h must reuse the single native extension typed-container ABI"
    }
    if ( $extlibAbiHeader -notmatch '(?m)^\s*struct\s+xl6_typed_container\s*\{' -or
         $extlibAbiHeader -notmatch '(?m)^\s*typedef\s+struct\s+xl6_typed_container\s*\*\s*xtarray\s*;' ) {
        Add-Failure "xl_extlib_abi.h must define the shared typed-container handle layout"
    }
    if ( $runtimeHeader -match '(?m)^\s*(?!return\b)(?!if\b)(?!while\b)(?!for\b)(?!switch\b)(?:[A-Za-z_][A-Za-z0-9_]*[\s*]+)+xrt[A-Z][A-Za-z0-9_]*\s*\(' ) {
        Add-Failure "release/include/xl_runtime.h must not mirror XRT function declarations"
    }
    if ( $tccEnvironment -notmatch '#include\s+"src/api/xl_xrt_symbols[.]inc"' -or
         $tccEnvironment -notmatch 'int\s+xlTccRegisterXrtEnvironment\s*\(' -or
         $tccEnvironment -notmatch 'tcc_add_symbol\s*\(\s*state\s*,\s*"xlTccRegisterXrtEnvironment"' -or
         $apiSymbols -notmatch '#include\s+"xl_xrt_symbols[.]inc"' -or
         $apiSymbols -notmatch 'xlApiAddDefaultRuntimeSymbols\s*\(' -or
         $xrtSymbols -notmatch 'XL_XRT_SYMBOL\s*\(\s*xrtTypeValue\s*\)' ) {
        Add-Failure "runtime and standalone TCC environments must share the centralized XRT symbol table"
    }
    if ( $runtimeHeader -notmatch '(?m)^\s*static\s+inline\s+xvalue\s*\*\s*xl6ValueHandleOwner\s*\(\s*const\s+ptr\s*\*\s*\w+\s*\)' ) {
        Add-Failure "release/include/xl_runtime.h must provide inline xl6ValueHandleOwner"
    }
    foreach ( $compilerOwnedHelper in @(
        "XL6_XRT_VALUE_STRING_HELPERS",
        "XL6_XRT_VALUE_RELEASE_CONVERT_HELPERS"
    ) ) {
        if ( $runtimeHeader -match ('(?m)^\s*#define\s+' + [regex]::Escape($compilerOwnedHelper) + '\b') ) {
            Add-Failure "release/include/xl_runtime.h must not define compiler-owned helper block $compilerOwnedHelper"
        }
    }
    $backendMembers = @([regex]::Matches(
        $xlcHeader, '(?m)^\s*XLC_BACKEND_[A-Z0-9_]+\s*='))
    if ( $backendMembers.Count -ne 1 -or
         $xlcHeader -notmatch '(?m)^\s*XLC_BACKEND_XLIR\s*=\s*0\s*$' -or
         $api -notmatch 'backend\s*!=\s*XLC_BACKEND_XLIR' ) {
        Add-Failure "xlc.dll must expose and accept only the verified XLIR backend"
    }
    foreach ( $requiredShard in @(
        'xl6_ir_cgen_verify.c',
        'xl6_ir_cgen_needs_analysis.c',
        'xl6_ir_cgen_needs_runtime_analysis.c',
        'xl6_ir_cgen_native_dependencies.c',
        'xl6_ir_cgen_module_emit.c',
        'xl6_ir_cgen_header_emit.c'
    ) ) {
        if ( !$irCgenRoot.Contains($requiredShard) ) {
            Add-Failure "unified XLIR C emitter root is missing required shard: $requiredShard"
        }
    }
    if ( $irCgenHeader -notmatch 'xl6IrCgenEmitCEx\s*\(' -or
         $irCgenHeader -notmatch 'xl6IrCgenEmitHeader\s*\(' -or
         $irCgenVerify -notmatch 'module->stage\s*!=\s*XL6_IR_STAGE_LOWERED' -or
         $irCgenVerify -notmatch 'xl6IrVerifyModule\s*\(' -or
         $irCgenModuleEmit -notmatch 'xl6IrCgenBuildNeeds\s*\(' -or
         $irCgenNativeDependencies -notmatch 'xl6IrCgenVerifyEmittable\s*\(' -or
         $irCgenNativeDependencies -notmatch 'xl6IrCgenVisitNativeModules\s*\(' ) {
        Add-Failure "xlc.dll C/header emission and native dependencies must share verified lowered XLIR"
    }
    if ( ($irCgenRoot + $irCgenHeader + $irCgenVerify +
          $irCgenModuleEmit + $irCgenNativeDependencies) -match
         'XL6_Ast|XL6_Hir|XLC_BACKEND_LEGACY|xl6CgenEmitCEx' ) {
        Add-Failure "xlc.dll unified emitter boundary contains a retired frontend/backend dependency"
    }
    foreach ( $enumApi in @(
        "xl6EnumInfo",
        "xl6EnumVariantAt",
        "xl6EnumFindVariant",
        "xl6EnumFindValue",
        "xl6EnumFieldAt",
        "xl6EnumFindField"
    ) ) {
        if ( $runtimeHeader -notmatch ('(?m)^\s*static\s+inline\s+[^;]*\b' + $enumApi + '\s*\(') ) {
            Add-Failure "release/include/xl_runtime.h must provide inline enum reflection helper $enumApi"
        }
    }
    $headerMatches = New-Object System.Collections.Generic.List[object]
    $headerApis = New-Object System.Collections.Generic.HashSet[string]
    $apiDefinitions = New-Object System.Collections.Generic.HashSet[string]
    $exampleDir = Join-Path $Root "release\examples"
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
        $trimmedLine = $line.Trim()
        if ( $trimmedLine.EndsWith(";") ) {
            continue
        }
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
        Add-Failure "release/include/xl.h must keep XL_API_VERSION at 1u for current RC boundary"
    }
    if ( $header -notmatch '(?s)defined\(XL_STATIC_EMBED\).*?#\s*define\s+XL_API\s*(?:\r?\n)' ) {
        Add-Failure "release/include/xl.h must keep XL_API undecorated for statically embedded EXE runtimes"
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

    if ( $api -notmatch 'XL_TCC_VFS_XRT_C' -or
         $api -notmatch 'visit\(user, "define", "XRT_MODULE_ALL", "1", 0\)' -or
         $api -notmatch 'visit\(user, "define", "XRUNTIME_MODULE_ALL", "1", 0\)' -or
         $api -notmatch '__xl_objects/static-native/xrt[.]o' -or
         $api -notmatch '__xl_objects/shared-native/xrt[.]o' ) {
        Add-Failure "xlc.dll external C output must link its complete relocatable XRT product"
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
    if ( $buildDll -notmatch 'call\s+tools\\build_xrt_obj[.]bat' ) {
        Add-Failure "build_dll.bat must build XRT through tools\\build_xrt_obj.bat"
    }
    if ( $buildXrt -notmatch 'tools\\xrt_single_header_impl[.]c' ) {
        Add-Failure "tools\\build_xrt_obj.bat must compile tools\\xrt_single_header_impl.c"
    }
    if ( $xrtImpl -notmatch '#include\s+"lib/xruntime/xruntime[.]h"' ) {
        Add-Failure "tools\\xrt_single_header_impl.c must include the vendored xruntime product snapshot"
    }

    # Release builds must not follow an external, potentially unstable XRT tree.
    $externalXrtPatterns = @(
        '(?i)D:\\GIT\\xrt',
        '(?i)%XRT_(ROOT|PATH|DIR)%',
        '(?i)\.\.\\xrt\\'
    )
    foreach ( $pattern in $externalXrtPatterns ) {
        if ( $buildDll -match $pattern -or $buildXrt -match $pattern -or $xrtImpl -match $pattern ) {
            Add-Failure "DEMO6 build scripts must not read an external XRT source tree: $pattern"
        }
    }
    if ( $buildDll -notmatch 'tcc_vfs_manifest_win64_runtime[.]txt' -or
         $buildDll -notmatch 'tcc_vfs_manifest_win64_xlc[.]txt' ) {
        Add-Failure "build_dll.bat must generate separate runtime and compiler VFS manifests for xl.dll/xlc.dll"
    }
    $runtimeResourcePattern = '(build\\tmp|%XL6_BUILD_TMP%)\\tcc_builtin_resources_runtime[.]c'
    $compilerResourcePattern = '(build\\tmp|%XL6_BUILD_TMP%)\\tcc_builtin_resources[.]c'
    if ( $xlRuntimeBuildBlock -notmatch '-DXL_RUNTIME_ONLY_DLL' -or
         $xlRuntimeBuildBlock -notmatch $runtimeResourcePattern ) {
        Add-Failure "xl.dll runtime build must use the runtime-only embedded TCC VFS resource set"
    }
    if ( $xlcBuildBlock -notmatch $compilerResourcePattern ) {
        Add-Failure "xlc.dll compiler build must use the compiler-capable embedded TCC VFS resource set"
    }
    if ( $xlRuntimeBuildBlock -match $compilerResourcePattern ) {
        Add-Failure "xl.dll runtime build must not use the compiler-capable VFS resource set"
    }
    if ( $xlcBuildBlock -match $runtimeResourcePattern ) {
        Add-Failure "xlc.dll compiler build must not use the runtime-only VFS resource set"
    }
    if ( $buildDll -match 'copy\s+/Y\s+"%XL6_TCC_DLL%"\s+release\\libtcc[.]dll' ) {
        Add-Failure "build_dll.bat must not copy external libtcc.dll into release"
    }

    if ( ! (Test-ManifestEntry $runtimeManifestEntries "dir" "include") ) {
        Add-Failure "runtime VFS manifest must include the TCC include directory"
    }
    if ( ! (Test-ManifestEntry $runtimeManifestEntries "file" "lib/libtcc1.a") ) {
        Add-Failure "runtime VFS manifest must include lib/libtcc1.a"
    }
    if ( ! (Test-ManifestEntry $runtimeManifestEntries "file" "include/xl.h") ) {
        Add-Failure "runtime VFS manifest must expose the public xl.h header"
    }
    if ( ! (Test-ManifestFileSourceSuffix $runtimeManifestEntries "lib/xrt/xrt.h" "release/include/xrt_decl.h") ) {
        Add-Failure "runtime VFS xrt.h compatibility path must use the XRT declaration SDK"
    }
    if ( ! (Test-ManifestFileSourceSuffix $runtimeManifestEntries "lib/xruntime/xruntime.h" "release/include/xruntime_decl.h") ) {
        Add-Failure "runtime VFS xruntime.h compatibility path must use the xruntime declaration SDK"
    }
    if ( ! (Test-ManifestEntry $runtimeManifestEntries "file" "include/__xl_runtime/xruntime_decl.h") ) {
        Add-Failure "runtime VFS manifest must expose the private xruntime declaration SDK"
    }
    if ( ! (Test-ManifestEntry $runtimeManifestEntries "file" "include/__xl_runtime/xl_runtime.h") ) {
        Add-Failure "runtime VFS manifest must include xl_runtime.h under the private __xl_runtime path"
    }
    if ( ! (Test-ManifestEntry $runtimeManifestEntries "file" "include/__xl_runtime/xl_extlib_abi.h") ) {
        Add-Failure "runtime VFS manifest must include the shared native extension ABI header"
    }
    if ( Test-ManifestVirtual $runtimeManifestEntries "include/xl_runtime.h" ) {
        Add-Failure "runtime VFS manifest must not expose xl_runtime.h under the public include path"
    }
    foreach ( $forbidden in @("include/xlc.h", "__xl_compiler/src", "__xl_compiler/lib/tcc", "__xl_compiler/lib/lzma", "lib/xrt/xrt.o", "lib/kernel32.def", "lib/msvcrt.def", "lib/user32.def", "lib/gdi32.def", "lib/ws2_32.def", "lib/shell32.def") ) {
        if ( Test-ManifestVirtual $runtimeManifestEntries $forbidden ) {
            Add-Failure "runtime VFS manifest must not carry compiler/exe resource: $forbidden"
        }
    }

foreach ( $required in @("include", "include/xl.h", "include/xlc.h", "include/__xl_runtime/xl_runtime.h", "include/__xl_runtime/xl_extlib_abi.h", "include/__xl_runtime/xruntime_decl.h", "lib/libtcc1.a", "lib/kernel32.def", "lib/msvcrt.def", "lib/user32.def", "lib/gdi32.def", "lib/ws2_32.def", "lib/shell32.def", "lib/xrt/xrt.h", "lib/xruntime/xruntime.h", "__xl_compiler/include/xl.h", "__xl_compiler/include/xl_runtime.h", "__xl_compiler/include/xl_extlib_abi.h", "__xl_compiler/include/xruntime_decl.h", "__xl_compiler/src", "__xl_compiler/lib/tcc", "__xl_compiler/lib/tcc/libtcc.c", "__xl_compiler/lib/tcc/stab.def", "__xl_compiler/lib/lzma", "__xl_compiler/lib/lzma/lzmadec.c", "__xl_compiler/build/tmp/tcc_builtin_resources_runtime.c") ) {
    if ( ! (Test-ManifestVirtual $compilerManifestEntries $required) ) {
        Add-Failure "compiler VFS manifest missing required resource: $required"
    }
}
    if ( ! (Test-ManifestFileSourceSuffix $compilerManifestEntries "lib/xrt/xrt.h" "release/include/xrt_decl.h") ) {
        Add-Failure "compiler VFS xrt.h compatibility path must use the XRT declaration SDK"
    }
    if ( ! (Test-ManifestFileSourceSuffix $compilerManifestEntries "lib/xruntime/xruntime.h" "lib/xruntime/xruntime.h") ) {
        Add-Failure "compiler VFS xruntime.h path must retain the complete xruntime implementation product"
    }
    if ( Test-ManifestVirtual $compilerManifestEntries "include/xl_runtime.h" ) {
        Add-Failure "compiler VFS manifest must not expose xl_runtime.h under the public include path"
    }
    if ( Test-ManifestVirtual $compilerManifestEntries "lib/xrt/xrt.o" ) {
        Add-Failure "compiler VFS manifest must use xrt.h and must not embed xrt.o"
    }
    if ( $futureBridge -notmatch '#include\s+"include/xruntime_decl[.]h"' -or
         $futureBridge -match '#include\s+"release/include/xruntime_decl[.]h"' ) {
        Add-Failure "embedded compiler sources must use the private compiler include root for xruntime declarations"
    }
    $embeddedRuntimeSources = [regex]::Match(
        $api, '(?s)static const char\* runtimeSources\[\]\s*=\s*\{(?<body>.*?)\};\s*static const char\* compilerSources')
    $embeddedCompilerSources = [regex]::Match(
        $api, '(?s)static const char\* compilerSources\[\]\s*=\s*\{(?<body>.*?)\};')
    if ( !$embeddedRuntimeSources.Success -or
         !$embeddedRuntimeSources.Groups['body'].Value.Contains('"src/cgen/xl6_cgen_shared_runtime.c"') -or
         !$embeddedRuntimeSources.Groups['body'].Value.Contains('"src/api/xl_foreign_error.c"') ) {
        Add-Failure "embedded xlang runtime must carry shared XLIR Cgen and foreign-error implementations"
    }
    if ( !$embeddedCompilerSources.Success -or
         !$embeddedCompilerSources.Groups['body'].Value.Contains('"src/resource/xl6_win_resource.c"') ) {
        Add-Failure "embedded xlang compiler must carry the platform resource implementation"
    }
    $compilerVfsDefine = '-DXL6_COMPILER_SOURCE_VFS'
    $xlangRuntimeStart = $api.IndexOf('static int xlAddEmbeddedXlangRuntimeSources')
    $xlangRuntimeEnd = if ( $xlangRuntimeStart -ge 0 ) {
        $api.IndexOf("`nstatic ", $xlangRuntimeStart + 1)
    } else { -1 }
    $compilerVfsDefineIndex = $api.IndexOf($compilerVfsDefine)
    if ( ([regex]::Matches($api, [regex]::Escape($compilerVfsDefine))).Count -ne 1 -or
         $xlangRuntimeStart -lt 0 -or $xlangRuntimeEnd -le $xlangRuntimeStart -or
         $compilerVfsDefineIndex -le $xlangRuntimeStart -or
         $compilerVfsDefineIndex -ge $xlangRuntimeEnd ) {
        Add-Failure "compiler-source VFS mode must be scoped exactly to embedded xlang runtime compilation"
    }
    if ( $runtimeManifest -notmatch 'XRT/xruntime functions are provided by tcc_add_symbol' ) {
        Add-Failure "runtime VFS manifest must document that XRT/xruntime is provided by symbols"
    }
    if ( $compilerManifest -notmatch 'complete xruntime single-header' ) {
        Add-Failure "compiler VFS manifest must document the XRT/xruntime product model"
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
