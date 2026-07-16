param(
    [string]$Root = (Split-Path -Parent $PSScriptRoot)
)

$ErrorActionPreference = "Stop"

$cgenDir = Join-Path $Root "src\cgen"
$tccPath = Join-Path $cgenDir "xl6_cgen_tcc.c"

if (!(Test-Path -LiteralPath $tccPath)) {
    throw "missing TCC cgen source: $tccPath"
}

$declarations = @()

# 只接受从字符串首字符开始的 C 函数原型。先用字符串切片取出内容，
# 再解析函数名，避免脚本编码差异和超长字符串造成正则行为不一致。
$outputPrefix = 'xl6OutputPuts("'
$outputSuffix = '\n"'
$prototypePattern = '^(?<return>[^;{}=]+?[ \t\*]+)(?<name>xrt[A-Za-z0-9_]+)\s*\((?<params>[^;{}=]*)\);$'

foreach ($file in (Get-ChildItem -LiteralPath $cgenDir -File -Filter '*.c')) {
    foreach ($line in (Get-Content -LiteralPath $file.FullName -Encoding UTF8)) {
        $trimmed = $line.TrimStart()
        if (!$trimmed.StartsWith($outputPrefix, [System.StringComparison]::Ordinal)) {
            continue
        }

        $payloadStart = $outputPrefix.Length
        $payloadEnd = $trimmed.IndexOf($outputSuffix, $payloadStart, [System.StringComparison]::Ordinal)
        if ($payloadEnd -lt 0) {
            continue
        }

        $payload = $trimmed.Substring($payloadStart, $payloadEnd - $payloadStart)
        if ($payload.Length -eq 0 -or [char]::IsWhiteSpace($payload[0])) {
            continue
        }

        if ($payload -notmatch $prototypePattern) {
            continue
        }
        $declarations += [pscustomobject]@{
            Name = $Matches['name']
            ReturnType = (($Matches['return'] -replace '\s+', ' ').Trim())
            Signature = (($payload -replace '\\"', '"') -replace '\s+', ' ').Trim()
            Path = $file.FullName
        }
    }
}

$tccText = [System.IO.File]::ReadAllText($tccPath, [System.Text.Encoding]::UTF8)
$registered = @(
    [regex]::Matches($tccText, 'XL6_TCC_ADD_SYMBOL\((?<name>xrt[A-Za-z0-9_]+)\)') |
        ForEach-Object { $_.Groups['name'].Value } |
        Sort-Object -Unique
)
$tccDeclaredNames = @(
    $declarations |
        Where-Object { $_.Path -eq $tccPath } |
        ForEach-Object { $_.Name } |
        Sort-Object -Unique
)

if ($env:XL6_CONTRACT_DEBUG) {
    Write-Host "root=$Root declarations=$($declarations.Count) tccDeclarations=$($tccDeclaredNames.Count)"
}

$failed = $false

foreach ($name in $registered) {
    if ($tccDeclaredNames -contains $name) {
        continue
    }
    Write-Host "registered XRT symbol has no emitted declaration: $name" -ForegroundColor Red
    $failed = $true
}

# 参数名不属于 ABI，不能用原型全文比较。这里检查最容易造成未定义行为的
# 返回类型冲突；完整参数契约由生成模块的 TCC 编译门禁负责。
$groups = @($declarations | Group-Object Name)
foreach ($item in $groups) {
    if ($tccDeclaredNames -notcontains $item.Name) {
        continue
    }

    $returnTypes = @($item.Group | Select-Object -ExpandProperty ReturnType -Unique)
    if ($returnTypes.Count -le 1) {
        continue
    }

    Write-Host "conflicting emitted return types for $($item.Name)" -ForegroundColor Red
    $item.Group |
        Sort-Object Signature, Path |
        ForEach-Object { Write-Host "  $($_.Signature) [$($_.Path)]" -ForegroundColor Red }
    $failed = $true
}

if ($failed) {
    exit 1
}

Write-Host "TCC/XRT contract ok: $($registered.Count) registered symbols"
