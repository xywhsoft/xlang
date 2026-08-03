$pkgDir = Split-Path -Parent $PSScriptRoot
$srcDir = Join-Path $pkgDir "src"
$outFile = Join-Path $pkgDir "xlang.package.json"
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)

# XGE 使用 unity build: xge_impl.c 包含所有 xge_*.c
# 因此 cSources 只列 xge_compile.c (包装器) + xui_*.c (独立编译) + xge_native.c (桥接)
$xuiFiles = Get-ChildItem -Path $srcDir -Filter "xui_*.c" | Sort-Object Name | ForEach-Object { "src/" + $_.Name }

$cFiles = @("xge_compile.c") + $xuiFiles + @("xge_native.c")

$cSourcesJson = ($cFiles | ForEach-Object { "            `"$_`"" }) -join ",`n"

$json = @"
{
    "format": "xlang-package-1",
    "name": "_xge_native",
    "version": "2.0.0",
    "abiVersion": "1",
    "kind": "demo6-private-extlib",
    "artifact": {
        "kind": "source",
        "cSources": [
$cSourcesJson
        ],
        "xlSources": []
    },
    "links": ["ws2_32", "iphlpapi", "gdi32", "user32", "shell32", "ole32", "winmm", "avrt", "opengl32"],
    "loadlibs": [],
    "dependencies": [],
    "native": {
        "singleton": false,
        "loadName": "",
        "version": ""
    }
}
"@

[System.IO.File]::WriteAllText($outFile, $json, $utf8NoBom)
Write-Host "Generated: $outFile"
Write-Host "cSources count: $($cFiles.Count)"
