param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ($Root -eq "") {
    $Root = Split-Path -Parent $PSScriptRoot
}

$extlibRoot = Join-Path $Root "release\extlibs"
$caseRoot = Join-Path $Root "test\cases"
$exampleRoot = Join-Path $Root "release\examples\extlibs"
$featuredPath = Join-Path $Root "test\featured_extlibs.json"
$phase1Path = Join-Path $Root "test\phase1_extlibs.json"
$platformGatePath = Join-Path $Root "test\platform_extlibs.json"
$outputJson = Join-Path $Root "test\extlib_ecosystem.json"
$outputMarkdown = Join-Path $Root "docs\extlib_ecosystem_audit.generated.md"

if (!(Test-Path -LiteralPath $extlibRoot -PathType Container)) {
    throw "Extension library directory is missing: $extlibRoot"
}

function Read-Utf8Text([string]$Path) {
    return [System.IO.File]::ReadAllText($Path, [System.Text.UTF8Encoding]::new($false))
}

function Test-PlaceholderSource([string]$Path) {
    $text = Read-Utf8Text $Path
    return $text -match '(?im)\bTODO\b|\bnot implemented\b|\bplaceholder\b|return\s+(NULL|0|false)\s*;\s*(/\*.*\*/)?\s*$|\bunsupported\b'
}

$featuredNames = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
if (Test-Path -LiteralPath $featuredPath -PathType Leaf) {
    $featured = Read-Utf8Text $featuredPath | ConvertFrom-Json
    foreach ($item in @($featured.packages)) {
        [void]$featuredNames.Add([string]$item.name)
    }
}

$phase1Names = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
if (Test-Path -LiteralPath $phase1Path -PathType Leaf) {
    $phase1 = Read-Utf8Text $phase1Path | ConvertFrom-Json
    foreach ($item in @($phase1.packages)) {
        [void]$phase1Names.Add([string]$item.name)
    }
}

$platformGateNames = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
if (Test-Path -LiteralPath $platformGatePath -PathType Leaf) {
    $platformGate = Read-Utf8Text $platformGatePath | ConvertFrom-Json
    foreach ($item in @($platformGate.packages)) {
        [void]$platformGateNames.Add([string]$item.name)
    }
}

$internalNames = @("_bzip2", "_expat", "_libzip", "_tomlc17", "_xge_native", "_zlib", "pkg.math")
$largeNames = @(
    "dataframe", "docx", "duckdb", "embeddings", "epub", "faiss", "ffmpeg", "flatbuffers", "freetype", "ggml", "glad", "grpc",
    "harfbuzz", "hnswlib", "html5", "icu", "imgui", "influxdb", "jieba",
    "kubernetes", "leveldb", "libtorch",
    "libsndfile", "libsvm", "llama", "lmdb", "mnn", "mongodb", "mysql", "ncnn", "openvino", "opus",
    "pdf", "pdfium", "postgresql", "pptx", "protobuf", "quic", "rabbitmq", "ratatui", "rocksdb",
    "sentencepiece", "sox", "stablediffusion", "taglib", "tdengine", "tensorrt", "tokenizers", "treesitter", "vectordb", "webrtc",
    "whisper", "xlsx"
)
$existingHardeningNames = @(
    "clipboard", "dinov3", "imagehash", "md4c", "onnxruntime", "opencv", "rapidocr", "sqlite",
    "stb_image", "stb_image_resize2", "stb_image_write", "stb_truetype", "xfeat", "xbot", "xge", "xge_shape_ex"
)
$rebuildCandidateNames = @()
$platformNames = @(
    "acme", "canbus", "clickhouse", "clip", "coap", "com", "daemon", "docker", "enet",
    "etcd", "ftp", "git", "gpio", "ldap", "libusb",
    "mdns", "minio", "modbus", "mqtt", "nanomsg", "nats", "neo4j", "nsq", "ntp", "oauth2",
    "op", "opentelemetry", "passkey", "pcap", "portaudio", "procps", "prometheus", "redis", "rtmp",
    "sciter", "serial", "sharedmem", "singleinstance", "snmp", "socks", "sysinfo",
    "uia", "ui.auto", "upnp", "vault", "volc.core", "volc.phone", "volc.sts", "watch", "webview",
    "xui", "zeromq"
)

$packages = New-Object System.Collections.Generic.List[object]
foreach ($directory in Get-ChildItem -LiteralPath $extlibRoot -Directory | Sort-Object Name) {
    $name = $directory.Name
    $manifestPath = Join-Path $directory.FullName "xlang.package.json"
    $manifest = $null
    if (Test-Path -LiteralPath $manifestPath -PathType Leaf) {
        try { $manifest = Read-Utf8Text $manifestPath | ConvertFrom-Json } catch { }
    }

    $xlSources = @(Get-ChildItem -LiteralPath $directory.FullName -Recurse -File -Filter "*.xl" -ErrorAction SilentlyContinue)
    $cSources = @(Get-ChildItem -LiteralPath $directory.FullName -Recurse -File -Filter "*.c" -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -notmatch '\\tests?\\' })
    $tests = @(Get-ChildItem -LiteralPath $directory.FullName -Recurse -File -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match '\\tests?\\' })
    $casePath = Join-Path $caseRoot ("extlib_{0}.xl" -f $name.Replace('.', '_'))
    $examplePath = Join-Path $exampleRoot ("{0}.xl" -f $name.Replace('.', '_'))
    $hasPlaceholder = $false
    $usesLegacyErrorAbi = $false
    $usesLegacyValueAbi = $false
    $usesRedeclaredXrtAbi = $false
    foreach ($source in $cSources) {
        $sourceText = Read-Utf8Text $source.FullName
        if ($sourceText -match '(?im)\bTODO\b|\bnot implemented\b|\bplaceholder\b|\bunsupported\b|return\s+(NULL|0|false)\s*;\s*(/\*.*\*/)?\s*$') {
            $hasPlaceholder = $true
        }
        # Do not redeclare XRT errors in an extension library. Such declarations
        # drift whenever XRT refines public ABI typedefs.
        if ($sourceText -match '(?m)xrtSetError\s*\(\s*"|extern\s+void\s+xrtSetError\s*\([^\r\n]*bFree|^\s*extern\s+void\s+xrtSetErrorInfo\s*\(') {
            $usesLegacyErrorAbi = $true
        }
        if ($sourceText -match '(?m)^\s*extern\s+[^;\r\n]*\bxrt[A-Za-z0-9_]*\s*\(') {
            $usesRedeclaredXrtAbi = $true
        }
        # The retired ABI used xvo* symbols. Do not confuse an unrelated
        # package-local prefix such as xvolc_* with that old contract.
        if ($sourceText -match '\bXVO_DT_[A-Z_]+\b|\bextern\s+[^\r\n;]*\bxvo(?!lc)[A-Za-z0-9_]*\b|\btypedef\s+[^\r\n;]*\bxvo(?!lc)[A-Za-z0-9_]*\b') {
            $usesLegacyValueAbi = $true
        }
    }

    $implementationScope = "placeholder"
    $budgetScope = "first-phase-new-code"
    if ($usesLegacyErrorAbi -or $usesLegacyValueAbi -or $usesRedeclaredXrtAbi) {
        $status = "needs-xrt-migration"
        $implementationScope = "existing-implementation"
        $budgetScope = "not-applicable-existing-implementation"
        $reason = if ($usesLegacyValueAbi) {
            "uses the retired xvo dynamic-value ABI and must migrate to current XRT xvalue ownership before phase-one publication"
        } elseif ($usesRedeclaredXrtAbi) {
            "redeclares XRT APIs instead of using public XRT declarations and must migrate before phase-one publication"
        } else {
            "declares or uses a retired XRT error ABI instead of the public XRT declaration and must migrate before phase-one publication"
        }
    } elseif ($internalNames -contains $name) {
        $status = "internal"
        $implementationScope = "internal-dependency"
        $budgetScope = "not-applicable-internal"
        $reason = "private dependency package"
    } elseif ($featuredNames.Contains($name)) {
        $status = "featured-rc"
        $implementationScope = "verified-implementation"
        $budgetScope = "not-applicable-verified"
        $reason = "covered by the featured extension RC gate"
    } elseif ($phase1Names.Contains($name)) {
        $status = "phase1-verified"
        $implementationScope = "verified-implementation"
        $budgetScope = "not-applicable-verified"
        $reason = "covered by the phase-one xlang/example/C smoke suite"
    } elseif ($platformGateNames.Contains($name)) {
        $status = "platform-verified"
        $implementationScope = "verified-implementation"
        $budgetScope = "not-applicable-verified"
        $reason = "covered by an explicit platform integration gate; not included in the side-effect-free phase-one suite"
    } elseif ($platformNames -contains $name) {
        $status = "deferred-platform"
        $implementationScope = "environment-bound"
        $budgetScope = "not-applicable-environment-bound"
        $reason = "requires an external service, device, platform SDK, credentials, or a controlled integration environment"
    } elseif ($existingHardeningNames -contains $name) {
        $status = "existing-hardening"
        $implementationScope = "existing-implementation"
        $budgetScope = "not-applicable-existing-implementation"
        $reason = "has an existing implementation footprint; harden its real backend, tests, examples, and platform boundary before first-phase admission"
    } elseif ($rebuildCandidateNames -contains $name) {
        $status = "phase1-rebuild-candidate"
        $implementationScope = "placeholder"
        $budgetScope = "within-10000-lines-estimate"
        $reason = "current surface is simplified; a bounded real implementation is planned within the 10000-line first-phase placeholder budget"
    } elseif ($largeNames -contains $name) {
        $status = "deferred-large"
        $implementationScope = "placeholder"
        $budgetScope = "over-10000-lines-estimate"
        $reason = "placeholder completion requires more than the 10000-line first-phase implementation budget or a large native dependency"
    } elseif ($hasPlaceholder -or $cSources.Count -le 1) {
        $status = "placeholder"
        $implementationScope = "placeholder"
        $budgetScope = "requires-estimate"
        $reason = "current native surface is a skeleton or simplified implementation and must not be advertised as production ready"
    } else {
        $status = "phase1-candidate"
        $implementationScope = "existing-implementation"
        $budgetScope = "not-applicable-existing-implementation"
        $reason = "has a local implementation footprint and is suitable for focused hardening"
    }

    $api = New-Object System.Collections.Generic.List[string]
    foreach ($source in $xlSources) {
        foreach ($line in ((Read-Utf8Text $source.FullName) -split "`r?`n")) {
            # Function exports may carry overload parameter lists. Classes and
            # unions have no required separator, so accept both forms.
            if ($line -match '^\s*export\s+(func|class|union)\s+([^\s(]+)') {
                $api.Add(($matches[1] + " " + $matches[2])) | Out-Null
            }
        }
    }

    $packages.Add([pscustomobject]@{
        name = $name
        version = if ($null -ne $manifest) { [string]$manifest.version } else { "" }
        status = $status
        implementationScope = $implementationScope
        budgetScope = $budgetScope
        reason = $reason
        manifest = Test-Path -LiteralPath $manifestPath -PathType Leaf
        xlangSources = $xlSources.Count
        cSources = $cSources.Count
        packageTests = $tests.Count
        legacyErrorAbi = $usesLegacyErrorAbi
        legacyValueAbi = $usesLegacyValueAbi
        redeclaredXrtAbi = $usesRedeclaredXrtAbi
        focusedCase = Test-Path -LiteralPath $casePath -PathType Leaf
        example = Test-Path -LiteralPath $examplePath -PathType Leaf
        dependencies = if ($null -ne $manifest -and $null -ne $manifest.dependencies) {
            @($manifest.dependencies | ForEach-Object { [string]$_.name })
        } else {
            @()
        }
        api = @($api | Select-Object -Unique)
    }) | Out-Null
}

$packageByName = @{}
foreach ($pkg in $packages) {
    $packageByName[$pkg.name] = $pkg
}
for ($pass = 0; $pass -lt $packages.Count; ++$pass) {
    $changed = $false
    foreach ($pkg in $packages) {
        if ($pkg.status -eq "internal" -or $pkg.status -eq "needs-xrt-migration") { continue }
        foreach ($dependency in @($pkg.dependencies)) {
            if (!$packageByName.ContainsKey($dependency)) { continue }
            $dependencyStatus = [string]$packageByName[$dependency].status
            if ($dependencyStatus -eq "needs-xrt-migration" -or $dependencyStatus -eq "blocked-dependency") {
                if ($pkg.status -ne "blocked-dependency") { $changed = $true }
                $pkg.status = "blocked-dependency"
                $pkg.reason = "blocked by dependency '$dependency': its XRT ABI migration or transitive dependency blocker must be resolved first"
                break
            }
        }
    }
    if (!$changed) { break }
}

$report = [pscustomobject]@{
    format = "xlang-extlib-ecosystem-1"
    generatedAt = (Get-Date).ToUniversalTime().ToString("o")
    root = "release/extlibs"
    packages = $packages.ToArray()
}
[System.IO.File]::WriteAllText($outputJson, ($report | ConvertTo-Json -Depth 6), [System.Text.UTF8Encoding]::new($false))

$counts = @($packages | Group-Object status | Sort-Object Name)
$lines = New-Object System.Collections.Generic.List[string]
$lines.Add("# Extension Library Ecosystem Audit") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("Generated by tools/audit_extlib_ecosystem.ps1. This is an inventory, not a release claim.") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("## Summary") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("| Status | Count |") | Out-Null
$lines.Add("| --- | ---: |") | Out-Null
foreach ($count in $counts) { $lines.Add(("| `{0}` | {1} |" -f $count.Name, $count.Count)) | Out-Null }
$lines.Add("") | Out-Null
$lines.Add("## Package Inventory") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("| Package | Status | XL | C | Test | Example | Reason |") | Out-Null
$lines.Add("| --- | --- | ---: | ---: | --- | --- | --- |") | Out-Null
foreach ($pkg in $packages) {
    $lines.Add(("| `{0}` | `{1}` | {2} | {3} | {4} | {5} | {6} |" -f $pkg.name, $pkg.status, $pkg.xlangSources, $pkg.cSources, $pkg.focusedCase, $pkg.example, $pkg.reason)) | Out-Null
}
[System.IO.File]::WriteAllText($outputMarkdown, [string]::Join("`n", $lines) + "`n", [System.Text.UTF8Encoding]::new($false))

Write-Host "Extension ecosystem audit written: $outputJson"
Write-Host "Extension ecosystem report written: $outputMarkdown"
foreach ($count in $counts) { Write-Host ("{0}: {1}" -f $count.Name, $count.Count) }
