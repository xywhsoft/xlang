param(
    [string]$Root = "",
    [string]$Manifest = "",
    [string]$Output = "",
    [string]$TccDist = "",
    [string]$XrtObj = "",
    [string]$PackTool = "",
    [int]$PackLevel = 9,
    [switch]$NoCompress
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($Root)) {
    $Root = (Get-Location).Path
}
if ([string]::IsNullOrWhiteSpace($Manifest)) {
    $Manifest = Join-Path $Root "tools\tcc_vfs_manifest_win64.txt"
}
if ([string]::IsNullOrWhiteSpace($Output)) {
    $Output = Join-Path $Root "build\tmp\tcc_builtin_resources.c"
}
if ([string]::IsNullOrWhiteSpace($TccDist)) {
    $TccDist = Join-Path $Root "res\tcc\win64\x86_64"
}
if ([string]::IsNullOrWhiteSpace($XrtObj)) {
    $XrtObj = Join-Path $Root "build\tmp\xrt.o"
}
if ([string]::IsNullOrWhiteSpace($PackTool)) {
    $PackTool = Join-Path $Root "build\tmp\tcc_vfs_lzma_pack.exe"
}

function Resolve-RootPath([string]$Base, [string]$Path) {
    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }
    return [System.IO.Path]::GetFullPath((Join-Path $Base $Path))
}

$Root = [System.IO.Path]::GetFullPath($Root)
$Manifest = Resolve-RootPath $Root $Manifest
$Output = Resolve-RootPath $Root $Output
$TccDist = Resolve-RootPath $Root $TccDist
$XrtObj = Resolve-RootPath $Root $XrtObj
$PackTool = Resolve-RootPath $Root $PackTool

function Resolve-ManifestPath([string]$Text) {
    $value = $Text
    $value = $value.Replace("%XL6_ROOT%", $Root)
    $value = $value.Replace("%XL6_EMBED_TCC_DIST_X64%", $TccDist)
    $value = $value.Replace("%XL6_XRT_OBJ%", $XrtObj)
    $value = $value.Replace("%XL6_VFS_OUTPUT_DIR%", [IO.Path]::GetDirectoryName($Output))
    if ([System.IO.Path]::IsPathRooted($value)) {
        return [System.IO.Path]::GetFullPath($value)
    }
    return [System.IO.Path]::GetFullPath((Join-Path $Root $value))
}

function Normalize-VirtualName([string]$Text) {
    $name = $Text.Replace("\", "/").Trim("/")
    return $name.ToLowerInvariant()
}

function Escape-CString([string]$Text) {
    $builder = [System.Text.StringBuilder]::new()
    foreach ($ch in $Text.ToCharArray()) {
        $code = [int][char]$ch
        if ($ch -eq "\") {
            [void]$builder.Append("\\")
        } elseif ($ch -eq '"') {
            [void]$builder.Append("\""")
        } elseif ($code -ge 32 -and $code -lt 127) {
            [void]$builder.Append($ch)
        } else {
            [void]$builder.Append(("\x{0:x2}" -f $code))
        }
    }
    return $builder.ToString()
}

function Add-Resource($List, $Seen, [string]$VirtualName, [string]$SourcePath) {
    $virtual = Normalize-VirtualName $VirtualName
    if ($Seen.ContainsKey($virtual)) {
        return
    }
    if (-not (Test-Path -LiteralPath $SourcePath -PathType Leaf)) {
        throw "missing VFS resource source: $SourcePath"
    }
    $Seen[$virtual] = $true
    $List.Add([pscustomobject]@{
        Virtual = $virtual
        Source = [System.IO.Path]::GetFullPath($SourcePath)
    }) | Out-Null
}

$resources = [System.Collections.Generic.List[object]]::new()
$seen = @{}

foreach ($rawLine in [System.IO.File]::ReadAllLines($Manifest)) {
    $line = $rawLine.Trim()
    if ($line.Length -eq 0 -or $line.StartsWith("#")) {
        continue
    }

    $parts = $line.Split("|")
    if ($parts.Count -lt 3) {
        throw "bad manifest line: $rawLine"
    }

    $kind = $parts[0].Trim().ToLowerInvariant()
    if ($kind -eq "dir") {
        $virtualPrefix = Normalize-VirtualName $parts[1]
        $sourceDir = Resolve-ManifestPath $parts[2].Trim()
        $patterns = @()
        if ($parts.Count -ge 4 -and -not [string]::IsNullOrWhiteSpace($parts[3])) {
            $patterns = @($parts[3].Split(";", [System.StringSplitOptions]::RemoveEmptyEntries) | ForEach-Object { $_.Trim() })
        }
        if (-not (Test-Path -LiteralPath $sourceDir -PathType Container)) {
            throw "missing VFS resource directory: $sourceDir"
        }
        $baseLen = $sourceDir.TrimEnd("\", "/").Length + 1
        Get-ChildItem -LiteralPath $sourceDir -Recurse -File | Where-Object {
            if ($patterns.Count -eq 0) {
                return $true
            }
            foreach ($pattern in $patterns) {
                if ($_.Name -like $pattern) {
                    return $true
                }
            }
            return $false
        } | Sort-Object FullName | ForEach-Object {
            $relative = $_.FullName.Substring($baseLen).Replace("\", "/")
            Add-Resource $resources $seen ($virtualPrefix + "/" + $relative) $_.FullName
        }
    } elseif ($kind -eq "file") {
        Add-Resource $resources $seen $parts[1].Trim() (Resolve-ManifestPath $parts[2].Trim())
    } else {
        throw "unknown manifest kind: $kind"
    }
}

$outDir = Split-Path -Parent $Output
if (-not (Test-Path -LiteralPath $outDir)) {
    [System.IO.Directory]::CreateDirectory($outDir) | Out-Null
}
$stampPath = "$Output.stamp.json"

$sb = [System.Text.StringBuilder]::new()
[void]$sb.AppendLine("/* This file is generated by the DEMO6 TCC VFS resource generator. */")
[void]$sb.AppendLine("#include ""tcc_builtin_vfs.h""")
[void]$sb.AppendLine("")

$packedResources = [System.Collections.Generic.List[object]]::new()
$total = 0L
$packedTotal = 0L

if (-not $NoCompress -and -not (Test-Path -LiteralPath $PackTool -PathType Leaf)) {
    throw "missing LZMA pack tool: $PackTool"
}

for ($i = 0; $i -lt $resources.Count; $i++) {
    $bytes = [System.IO.File]::ReadAllBytes($resources[$i].Source)
    $packedBytes = $bytes
    $method = "TCC_BUILTIN_RESOURCE_STORE"
    if (-not $NoCompress -and $bytes.Length -gt 32) {
        $tmpPacked = Join-Path $outDir ("tcc_vfs_lzma_{0:0000}.bin" -f $i)
        & $PackTool $resources[$i].Source $tmpPacked $PackLevel
        if ($LASTEXITCODE -ne 0) {
            throw "LZMA pack failed: $($resources[$i].Source)"
        }
        $candidate = [System.IO.File]::ReadAllBytes($tmpPacked)
        Remove-Item -LiteralPath $tmpPacked -Force -ErrorAction SilentlyContinue
        if ($candidate.Length -lt $bytes.Length) {
            $packedBytes = $candidate
            $method = "TCC_BUILTIN_RESOURCE_LZMA"
        }
    }
    $packedResources.Add([pscustomobject]@{
        Virtual = $resources[$i].Virtual
        Source = $resources[$i].Source
        SourceHash = ([System.BitConverter]::ToString(
            [System.Security.Cryptography.SHA256]::Create().ComputeHash($bytes)
        ) -replace '-', '').ToLowerInvariant()
        Bytes = $bytes
        PackedBytes = $packedBytes
        Method = $method
    }) | Out-Null
    $total += $bytes.Length
    $packedTotal += $packedBytes.Length
}

for ($i = 0; $i -lt $packedResources.Count; $i++) {
    $bytes = $packedResources[$i].PackedBytes
    [void]$sb.AppendLine(("static const unsigned char tcc_builtin_res_{0:0000}[] = {{" -f $i))
    if ($bytes.Length -eq 0) {
        [void]$sb.AppendLine("    0x00")
    } else {
        for ($offset = 0; $offset -lt $bytes.Length; $offset += 16) {
            $end = [Math]::Min($offset + 16, $bytes.Length)
            $values = for ($j = $offset; $j -lt $end; $j++) {
                "0x{0:x2}" -f $bytes[$j]
            }
            [void]$sb.AppendLine(("    " + ($values -join ", ") + ","))
        }
    }
    [void]$sb.AppendLine("};")
    [void]$sb.AppendLine("")
}

[void]$sb.AppendLine("const TCCBuiltinResource tcc_builtin_resources[] = {")
for ($i = 0; $i -lt $packedResources.Count; $i++) {
    $item = $packedResources[$i]
    $name = Escape-CString $item.Virtual
    [void]$sb.AppendLine(("    {{ ""{0}"", tcc_builtin_res_{1:0000}, {2}u, {3}u, {4} }}," -f $name, $i, $item.PackedBytes.Length, $item.Bytes.Length, $item.Method))
}
[void]$sb.AppendLine("};")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("const unsigned int tcc_builtin_resources_count =")
[void]$sb.AppendLine("    (unsigned int)(sizeof(tcc_builtin_resources) / sizeof(tcc_builtin_resources[0]));")

$utf8NoBom = [System.Text.UTF8Encoding]::new($false)
$content = $sb.ToString().Replace("`r`n", "`n")
[System.IO.File]::WriteAllText($Output, $content, $utf8NoBom)

$stamp = [ordered]@{
    schema = 1
    resourceCount = $packedResources.Count
    resources = @($packedResources | ForEach-Object {
        [ordered]@{
            virtual = $_.Virtual
            source = $_.Source
            sha256 = $_.SourceHash
        }
    })
}
[System.IO.File]::WriteAllText(
    $stampPath,
    ($stamp | ConvertTo-Json -Depth 4),
    $utf8NoBom)

Write-Host ("generated {0} TCC VFS resources, {1} bytes packed to {2} bytes -> {3}" -f $resources.Count, $total, $packedTotal, $Output)
