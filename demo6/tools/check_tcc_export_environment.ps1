param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$Root = (Resolve-Path -LiteralPath $Root).Path
$releaseDir = Join-Path $Root "release"
$tmpDir = Join-Path $Root "build\test\tcc_export_environment"
$includeDir = Join-Path $releaseDir "include"
$xlExe = Join-Path $releaseDir "xl.exe"
$xlDef = Join-Path $releaseDir "xl.def"
$xlcDef = Join-Path $releaseDir "xlc.def"
$xlDll = Join-Path $releaseDir "xl.dll"
$xlcDll = Join-Path $releaseDir "xlc.dll"
$libtccHeader = Join-Path $includeDir "libtcc.h"
$vfsHeader = Join-Path $includeDir "tcc_builtin_vfs.h"
$cExample = Join-Path $releaseDir "examples\stdlib_math_c.c"
$failures = New-Object System.Collections.Generic.List[string]
$utf8 = New-Object System.Text.UTF8Encoding($false)

function Add-Failure($message)
{
    $script:failures.Add($message) | Out-Null
}

function Resolve-Under-Root($path, $fallback)
{
    if ( $path -eq $null -or $path -eq "" ) {
        $path = $fallback
    }
    if ( [System.IO.Path]::IsPathRooted($path) ) {
        return $path
    }
    return Join-Path $script:Root $path
}

function Read-ExportSet($path)
{
    $set = @{}
    foreach ( $line in [System.IO.File]::ReadAllLines($path) ) {
        $name = $line.Trim()
        if ( $name -eq "" -or $name -eq "EXPORTS" -or $name.StartsWith(";") ) {
            continue
        }
        $set[$name] = $true
    }
    return $set
}

function Read-PublicTccApiNames($paths)
{
    $names = @{}
    foreach ( $path in $paths ) {
        $text = [System.IO.File]::ReadAllText($path)
        $matches = [System.Text.RegularExpressions.Regex]::Matches(
            $text,
            "(?m)^\s*(?:LIBTCCAPI|TCC_VFS_API)\s+[^;\r\n]*?\b(_?tcc_[A-Za-z0-9_]+)\s*\(")
        foreach ( $match in $matches ) {
            $names[$match.Groups[1].Value] = $true
        }
    }
    return @($names.Keys | Sort-Object)
}

function Invoke-Checked([string]$File, [string[]]$Arguments, [string]$Name)
{
    $output = & $File @Arguments 2>&1
    $code = $LASTEXITCODE
    $text = [string]::Join([Environment]::NewLine, @($output))
    if ( $code -ne 0 ) {
        Add-Failure "$Name failed with exit code $code. $text"
    }
    return $text
}

foreach ( $path in @($xlExe, $xlDef, $xlcDef, $xlDll, $xlcDll, $libtccHeader, $vfsHeader, $cExample) ) {
    if ( !(Test-Path -LiteralPath $path -PathType Leaf) ) {
        Add-Failure "required file missing: $path"
    }
}

if ( $failures.Count -eq 0 ) {
    $publicTccApis = Read-PublicTccApiNames -paths @($libtccHeader, $vfsHeader)
    foreach ( $defPath in @($xlDef, $xlcDef) ) {
        $exports = Read-ExportSet $defPath
        $tccExportCount = @($exports.Keys | Where-Object { $_ -match "^_?tcc_" }).Count
        if ( $tccExportCount -lt 40 ) {
            Add-Failure "$defPath exports too few TCC symbols: $tccExportCount"
        }
        foreach ( $name in $publicTccApis ) {
            if ( !$exports.ContainsKey($name) ) {
                Add-Failure "$defPath missing public TCC API export: $name"
            }
        }
        foreach ( $name in @("xlTccRegisterXrtEnvironment", "xlTccRegisterContextEnvironment", "xlTccImpdef") ) {
            if ( !$exports.ContainsKey($name) ) {
                Add-Failure "$defPath missing xlang TCC helper export: $name"
            }
        }
    }
}

if ( $failures.Count -eq 0 ) {
    if ( !(Test-Path -LiteralPath $tmpDir -PathType Container) ) {
        New-Item -ItemType Directory -Path $tmpDir | Out-Null
    }

    $runText = Invoke-Checked -File $xlExe -Arguments @("run", $cExample) -Name "xl run C example"
    if ( $runText.IndexOf("advanced=ok") -lt 0 ) {
        Add-Failure "xl run C example output missing advanced=ok"
    }

    $builtExe = Join-Path $tmpDir "stdlib_math_c_from_xl.exe"
    Remove-Item -LiteralPath $builtExe -Force -ErrorAction SilentlyContinue
    Invoke-Checked -File $xlExe -Arguments @("build", $cExample, "-o", $builtExe) -Name "xl build C example" | Out-Null
    if ( Test-Path -LiteralPath $builtExe -PathType Leaf ) {
        $builtText = Invoke-Checked -File $builtExe -Arguments @() -Name "built C example exe"
        if ( $builtText.IndexOf("advanced=ok") -lt 0 ) {
            Add-Failure "built C example output missing advanced=ok"
        }
    } else {
        Add-Failure "xl build C example did not create exe: $builtExe"
    }

    $impdefPath = Join-Path $tmpDir "xl_from_xl_impdef.def"
    Remove-Item -LiteralPath $impdefPath -Force -ErrorAction SilentlyContinue
    Invoke-Checked -File $xlExe -Arguments @("impdef", $xlDll, "-o", $impdefPath) -Name "xl impdef xl.dll" | Out-Null
    if ( Test-Path -LiteralPath $impdefPath -PathType Leaf ) {
        $impdefExports = Read-ExportSet $impdefPath
        foreach ( $name in @("tcc_new", "tcc_compile_string", "tcc_vfs_mount_memory", "xlTccRegisterXrtEnvironment") ) {
            if ( !$impdefExports.ContainsKey($name) ) {
                Add-Failure "impdef output missing export: $name"
            }
        }
    } else {
        Add-Failure "xl impdef did not create def: $impdefPath"
    }
}

if ( $failures.Count -eq 0 ) {
    $tcc = Resolve-Under-Root $env:XL6_TCC "build\tmp\tcc_bootstrap.exe"
    if ( !(Test-Path -LiteralPath $tcc -PathType Leaf) ) {
        Add-Failure "TCC bootstrap compiler missing: $tcc"
    } else {
        $hostSource = Join-Path $tmpDir "host_tcc_xrt_env.c"
        $hostCode = @'
#include "xl.h"
#include "libtcc.h"
#include <stdio.h>

int main(void)
{
    TCCState* s = tcc_new();
    int (*fn)(void);
    int rc;
    const char* code = "double xrtMathHypot(double,double); int host_check(void){ double v = xrtMathHypot(3.0,4.0); return (v > 4.999 && v < 5.001) ? 0 : 7; }";
    if ( s == NULL ) return 1;
    if ( tcc_set_output_type(s, TCC_OUTPUT_MEMORY) != 0 ) return 2;
    if ( !xlTccRegisterXrtEnvironment(s) ) return 3;
    if ( tcc_compile_string(s, code) != 0 ) return 4;
    if ( tcc_relocate(s) != 0 ) return 5;
    fn = (int (*)(void))tcc_get_symbol(s, "host_check");
    if ( fn == NULL ) return 6;
    rc = fn();
    tcc_delete(s);
    if ( rc != 0 ) return rc;
    puts("tcc_xrt_env=ok");
    return 0;
}
'@
        [System.IO.File]::WriteAllText($hostSource, $hostCode, [System.Text.Encoding]::ASCII)

        $includeArgs = @(
            "-I$includeDir",
            "-I$(Resolve-Under-Root $env:XL6_TCC_INCLUDE 'lib\tcc\include')",
            "-I$(Resolve-Under-Root $env:XL6_TCC_WIN_INCLUDE 'res\tcc\win64\x86_64\include')",
            "-I$(Resolve-Under-Root $env:XL6_TCC_WINAPI_INCLUDE 'res\tcc\win64\x86_64\include\winapi')"
        )
        $libArgs = @("-L$(Resolve-Under-Root $env:XL6_TCC_LIB 'res\tcc\win64\x86_64\lib')")
        $oldPath = $env:PATH
        $env:PATH = "$releaseDir;$oldPath"
        try {
            foreach ( $pair in @(@("xl", $xlDef), @("xlc", $xlcDef)) ) {
                $name = $pair[0]
                $defPath = $pair[1]
                $hostExe = Join-Path $tmpDir "host_tcc_xrt_env_$name.exe"
                Remove-Item -LiteralPath $hostExe -Force -ErrorAction SilentlyContinue
                $compileArgs = @()
                $compileArgs += $includeArgs
                $compileArgs += $libArgs
                $compileArgs += @($hostSource, $defPath, "-o", $hostExe)
                Invoke-Checked -File $tcc -Arguments $compileArgs -Name "host TCC XRT env compile via $name.def" | Out-Null
                if ( Test-Path -LiteralPath $hostExe -PathType Leaf ) {
                    $hostText = Invoke-Checked -File $hostExe -Arguments @() -Name "host TCC XRT env run via $name.dll"
                    if ( $hostText.IndexOf("tcc_xrt_env=ok") -lt 0 ) {
                        Add-Failure "host TCC XRT env output missing ok via $name.dll"
                    }
                } else {
                    Add-Failure "host TCC XRT env compile did not create exe via $name.def"
                }
            }
        } finally {
            $env:PATH = $oldPath
        }
    }
}

if ( $failures.Count -gt 0 ) {
    foreach ( $failure in $failures ) {
        Write-Host $failure
    }
    exit 1
}

Write-Host "TCC export and XRT environment checks ok."
exit 0
