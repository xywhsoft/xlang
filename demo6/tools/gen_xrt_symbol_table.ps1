param(
    [string]$Root = "",
    [switch]$Check
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$headerPath = Join-Path $Root "lib\xruntime\xruntime.h"
$outputPath = Join-Path $Root "src\api\xl_xrt_symbols.inc"
$implementationMarker = "#if defined(XRUNTIME_IMPLEMENTATION) && !defined(XRUNTIME_IMPLEMENTATION_ONCE)"
$functionPattern = '\bXRT_API\s+[^;{}]*?\b(xrt[A-Za-z0-9_]*)\s*\([^;{}]*\)\s*;'



# 只分析单头文件的公共声明区，避免把实现定义误判为 API。
$text = [System.IO.File]::ReadAllText($headerPath, [System.Text.Encoding]::UTF8)
$marker = $text.IndexOf($implementationMarker, [System.StringComparison]::Ordinal)
if ( $marker -lt 0 ) {
    throw "xruntime implementation marker is missing: $headerPath"
}
$text = $text.Substring(0, $marker)
$text = [regex]::Replace($text, '/\*.*?\*/', ' ', [System.Text.RegularExpressions.RegexOptions]::Singleline)
$text = [regex]::Replace($text, '//[^\r\n]*', ' ')
$text = [regex]::Replace($text, '(?m)^[ \t]*#[^\r\n]*(?:\\\r?\n[^\r\n]*)*', ' ')



# 按稳定的序数顺序输出，重复声明和异常少量结果都视为构建错误。
$matches = [regex]::Matches(
    $text,
    $functionPattern,
    [System.Text.RegularExpressions.RegexOptions]::Singleline
)
$names = New-Object System.Collections.Generic.List[string]
foreach ( $match in $matches ) {
    $names.Add($match.Groups[1].Value)
}
$unique = $names.ToArray()
[Array]::Sort($unique, [System.StringComparer]::Ordinal)
for ( $i = 1; $i -lt $unique.Count; $i += 1 ) {
    if ( $unique[$i - 1] -ceq $unique[$i] ) {
        throw "duplicate XRT/xruntime API declarations were found"
    }
}
if ( $unique.Count -lt 1000 ) {
    throw "too few XRT/xruntime API functions were found: $($unique.Count)"
}

$lines = New-Object System.Collections.Generic.List[string]
$lines.Add("/* 由 tools/gen_xrt_symbol_table.py 或 .ps1 生成；不要手工修改。 */")
$lines.Add("/* 当前登记 $($unique.Count) 个 XRT/xruntime 公共函数。 */")
foreach ( $name in $unique ) {
    $lines.Add("XL_XRT_SYMBOL($name)")
}
$expected = [string]::Join("`n", $lines) + "`n"

if ( $Check ) {
    $actual = if ( Test-Path -LiteralPath $outputPath -PathType Leaf ) {
        [System.IO.File]::ReadAllText($outputPath, [System.Text.Encoding]::UTF8)
    } else {
        ""
    }
    if ( $actual -cne $expected ) {
        Write-Error "XRT/xruntime symbol table is stale: $outputPath"
        exit 1
    }
    Write-Host "XRT/xruntime symbol table is current: $($unique.Count) functions"
    exit 0
}

[System.IO.File]::WriteAllText(
    $outputPath,
    $expected,
    [System.Text.UTF8Encoding]::new($false)
)
Write-Host "generated ${outputPath}: $($unique.Count) functions"
