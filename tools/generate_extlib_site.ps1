param(
    [string]$Root = "",
    [string]$WebsiteRoot = "D:\GIT\home\host\xlang\wwwroot",
    [string[]]$Package = @()
)

$ErrorActionPreference = "Stop"

if ($Root -eq "") {
    $Root = Split-Path -Parent $PSScriptRoot
}

$auditPath = Join-Path $Root "test\extlib_ecosystem.json"
if (!(Test-Path -LiteralPath $auditPath -PathType Leaf)) {
    & (Join-Path $PSScriptRoot "audit_extlib_ecosystem.ps1") -Root $Root
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
if (!(Test-Path -LiteralPath $WebsiteRoot -PathType Container)) {
    throw "Website root is missing: $WebsiteRoot"
}

function Html([string]$Text) {
    return [System.Net.WebUtility]::HtmlEncode($Text)
}

$audit = [System.IO.File]::ReadAllText($auditPath, [System.Text.UTF8Encoding]::new($false)) | ConvertFrom-Json
foreach ($name in $Package) {
    if ($name -notin @($audit.packages.name)) { throw "Unknown package: $name" }
}
$outputDir = Join-Path $WebsiteRoot "extlibs\generated"
[System.IO.Directory]::CreateDirectory($outputDir) | Out-Null

$indexRows = New-Object System.Collections.Generic.List[string]
foreach ($pkg in @($audit.packages | Sort-Object name)) {
    if ($pkg.visibility -eq "internal" -or $pkg.status -eq "internal") { continue }
    if ($Package.Count -ne 0 -and $pkg.name -notin $Package) { continue }
    $fileName = $pkg.name.Replace('.', '_') + ".html"
    # 包内详情页保存当前合同与带日期的证据；旧清单标签不能覆盖它或扩大验收范围。
    $detailSource = Join-Path $Root "release/extlibs/$($pkg.name)/docs/site.html"
    if (Test-Path -LiteralPath $detailSource -PathType Leaf) {
        $detailTarget = Join-Path $WebsiteRoot "extlibs/$fileName"
        Copy-Item -LiteralPath $detailSource -Destination $detailTarget -Force
        $entry = @"
<!doctype html>
<html lang="zh-CN"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>$(Html $pkg.name) - xlang</title><link rel="stylesheet" href="../../style.css"></head>
<body><main class="extlib-layout"><h1>$(Html $pkg.name)</h1>
<p><a href="../$fileName">API, examples, supported scope and dated verification</a></p>
<p>This inventory entry does not imply complete protocol or production readiness.</p></main></body></html>
"@
        [IO.File]::WriteAllText((Join-Path $outputDir $fileName), $entry, [Text.UTF8Encoding]::new($false))
        $indexRows.Add(('<tr><td><a href="../{0}"><code>{1}</code></a></td><td>documented</td><td>See the detailed contract and verification scope.</td></tr>' -f
            $fileName, (Html $pkg.name))) | Out-Null
        continue
    }
    $apiItems = @($pkg.api | ForEach-Object { "<li><code>$(Html $_)</code></li>" })
    if ($apiItems.Count -eq 0) { $apiItems = @("<li>API inventory has not been extracted yet.</li>") }
    $page = @"
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>$(Html $pkg.name) - xlang extension library</title>
  <link rel="stylesheet" href="../../style.css">
  <link rel="stylesheet" href="../../extlibs.css">
</head>
<body>
<main class="extlib-layout">
  <p><a href="index.html">Extension library catalog</a></p>
  <h1>$(Html $pkg.name)</h1>
  <p><strong>Status:</strong> <code>$(Html $pkg.status)</code></p>
  <p>$(Html $pkg.reason)</p>
  <h2>Package facts</h2>
  <table class="ext-kv">
    <tr><th>Version</th><td>$(Html $pkg.version)</td></tr>
    <tr><th>xlang sources</th><td>$($pkg.xlangSources)</td></tr>
    <tr><th>C sources</th><td>$($pkg.cSources)</td></tr>
    <tr><th>Implementation scope</th><td><code>$(Html $pkg.implementationScope)</code></td></tr>
    <tr><th>Budget scope</th><td><code>$(Html $pkg.budgetScope)</code></td></tr>
    <tr><th>Focused test</th><td>$($pkg.focusedCase)</td></tr>
    <tr><th>Runnable example</th><td>$($pkg.example)</td></tr>
  </table>
  <h2>Export inventory</h2>
  <ul>
    $([string]::Join("`n    ", $apiItems))
  </ul>
  <h2>Verification policy</h2>
  <p>A package becomes a first-phase published library only after it has a real implementation, boundary tests, runnable examples, and a successful package gate. This page is generated from the repository audit and does not imply production readiness.</p>
</main>
</body>
</html>
"@
    [System.IO.File]::WriteAllText((Join-Path $outputDir $fileName), $page, [System.Text.UTF8Encoding]::new($false))
    $indexRows.Add(('<tr><td><a href="{0}"><code>{1}</code></a></td><td><code>{2}</code></td><td>{3}</td></tr>' -f
        $fileName, (Html $pkg.name), (Html $pkg.status), (Html $pkg.reason))) | Out-Null
}

$index = @"
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>xlang extension library ecosystem</title>
  <link rel="stylesheet" href="../../style.css">
  <link rel="stylesheet" href="../../extlibs.css">
</head>
<body>
<main class="extlib-layout">
  <h1>xlang Extension Library Ecosystem</h1>
  <p>This catalog is generated from the release package audit. The 10000-line budget applies only to new placeholder implementations; existing implementations are tracked for migration and hardening separately.</p>
  <table class="ext-kv"><tr><th>Package</th><th>Status</th><th>Assessment</th></tr>
  $([string]::Join("`n  ", $indexRows))
  </table>
</main>
</body>
</html>
"@
if ($Package.Count -eq 0) {
    [System.IO.File]::WriteAllText((Join-Path $outputDir "index.html"), $index, [System.Text.UTF8Encoding]::new($false))
}

Write-Host "Generated extension library pages: $outputDir"
