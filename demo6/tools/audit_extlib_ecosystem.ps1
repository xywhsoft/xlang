param(
    [string]$Root = "",
    [switch]$CheckOnly
)

$ErrorActionPreference = "Stop"

# 并行开发时只校验包清单，不覆盖已有的生成报告。

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

function Test-PackageEntry([string]$Directory, [string]$Relative, [string]$PathType) {
    if ([IO.Path]::IsPathRooted($Relative) -or
        $Relative -match '^[A-Za-z]:' -or
        $Relative -match '(^|[\\/])\.\.([\\/]|$)') { return $false }
    return Test-Path -LiteralPath (Join-Path $Directory $Relative) -PathType $PathType
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
    $issues = [Collections.Generic.List[string]]::new()
    if (Test-Path -LiteralPath $manifestPath -PathType Leaf) {
        try {
            $manifest = Read-Utf8Text $manifestPath | ConvertFrom-Json
            if ($manifest -isnot [pscustomobject] -or $manifest.format -cne 'xlang-package-1') {
                $issues.Add('invalid manifest format')
                $manifest = $null
            }
        } catch { $issues.Add('manifest JSON cannot be parsed') }
    } else { $issues.Add('manifest missing') }
    if ($null -ne $manifest -and $manifest.name -cne $name) { $issues.Add('manifest name differs from directory') }

    # 清点实际参与构建的源文件，不把 test、废弃副本或未接入的源码当作实现证据。
    $declared = @{ cSources = [Collections.Generic.List[object]]::new(); xlSources = [Collections.Generic.List[object]]::new() }
    if ($null -ne $manifest -and $manifest.artifact.kind -eq 'source') {
        foreach ($kind in @('cSources', 'xlSources')) {
            foreach ($relative in @($manifest.artifact.$kind)) {
                if ($null -eq $relative) { continue }
                if ($relative -isnot [string] -or [string]::IsNullOrWhiteSpace($relative)) {
                    $issues.Add("invalid artifact.$kind entry")
                    continue
                }
                $path = Join-Path $directory.FullName $relative
                if (!(Test-PackageEntry $directory.FullName $relative Leaf)) {
                    $issues.Add("missing artifact.${kind}: $relative")
                    continue
                }
                $declared[$kind].Add((Get-Item -LiteralPath $path))
            }
        }
        foreach ($relative in @($manifest.artifact.includeDirs)) {
            if ($null -eq $relative) { continue }
            if ($relative -isnot [string] -or [string]::IsNullOrWhiteSpace($relative) -or
                !(Test-PackageEntry $directory.FullName $relative Container)) {
                $issues.Add("missing artifact.includeDirs: $relative")
            }
        }
        if ($declared.cSources.Count + $declared.xlSources.Count -eq 0) { $issues.Add('source package has no available declared sources') }
    } elseif ($null -ne $manifest) {
        $issues.Add('artifact kind requires an explicit audit implementation')
    }
    if ($null -ne $manifest) {
        foreach ($relative in @($manifest.coverage.tests)) {
            if ($null -eq $relative) { continue }
            if ($relative -isnot [string] -or [string]::IsNullOrWhiteSpace($relative) -or
                !(Test-PackageEntry $directory.FullName $relative Leaf)) {
                $issues.Add("missing coverage.tests: $relative")
            }
        }
    }
    $xlSources = @($declared.xlSources.ToArray())
    $cSources = @($declared.cSources.ToArray())
    $tests = @(Get-ChildItem -LiteralPath $directory.FullName -Recurse -File -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match '\\tests?\\' })
    $casePath = Join-Path $caseRoot ("extlib_{0}.xl" -f $name.Replace('.', '_'))
    $examplePath = Join-Path $exampleRoot ("{0}.xl" -f $name.Replace('.', '_'))
    $usesLegacyErrorAbi = $false
    $usesLegacyValueAbi = $false
    $usesRedeclaredXrtAbi = $false
    foreach ($source in $cSources) {
        $sourceText = Read-Utf8Text $source.FullName
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

    $gates = @()
    if ($featuredNames.Contains($name)) { $gates += 'featured' }
    if ($phase1Names.Contains($name)) { $gates += 'phase1' }
    if ($platformGateNames.Contains($name)) { $gates += 'platform' }
    $implementationScope = "requires-source-review"
    $budgetScope = "requires-new-code-estimate"
    if ($issues.Count -ne 0) {
        $status = 'invalid-package'
        $reason = [string]::Join('; ', $issues)
    } elseif ($usesLegacyErrorAbi -or $usesLegacyValueAbi -or $usesRedeclaredXrtAbi) {
        $status = "needs-abi-review"
        $reason = if ($usesLegacyValueAbi) {
            "text scan found retired xvo ABI markers; confirm active use and compile against current XRT"
        } elseif ($usesRedeclaredXrtAbi) {
            "text scan found local XRT API declarations; check their necessity and physical ABI against public headers"
        } else {
            "text scan found retired error ABI markers; confirm the call site before declaring a migration blocker"
        }
    } elseif ($internalNames -contains $name) {
        $status = "internal"
        $implementationScope = "internal-dependency"
        $budgetScope = "not-applicable-internal"
        $reason = "private dependency package"
    } elseif ($gates.Count -ne 0) {
        $status = "registered"
        $reason = "registered in $($gates -join ', ') suites; registration is not a test result or release admission"
    } elseif ($platformNames -contains $name) {
        $status = "environment-review"
        $reason = "historical environment-sensitive candidate; inspect current backend and controlled test requirements before deciding deferral"
    } elseif ($existingHardeningNames -contains $name) {
        $status = "existing-hardening"
        $implementationScope = "existing-implementation"
        $budgetScope = "not-applicable-existing-implementation"
        $reason = "has an existing implementation footprint; harden its real backend, tests, examples, and platform boundary before first-phase admission"
    } elseif ($largeNames -contains $name) {
        $status = "budget-review"
        $reason = "historical large-backend candidate; only a reviewed placeholder's NEW implementation estimate can establish the 10000-line deferral"
    } else {
        $status = "source-review"
        $reason = "declared sources exist; completeness cannot be inferred from file count, maintenance comments or defensive failure returns"
    }
    # 已有真实后端的预算豁免不取决于套件登记，不能把旧功能重新计入占位实现预算。
    if ($existingHardeningNames -contains $name) { $budgetScope = 'not-applicable-existing-implementation' }

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
        visibility = if ($internalNames -contains $name) { 'internal' } else { 'public' }
        gates = $gates
        verification = 'not-evaluated-by-inventory'
        implementationScope = $implementationScope
        budgetScope = $budgetScope
        reason = $reason
        manifest = Test-Path -LiteralPath $manifestPath -PathType Leaf
        manifestValid = $issues.Count -eq 0
        issues = @($issues.ToArray())
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
        if ($pkg.status -in @('invalid-package', 'blocked-dependency')) { continue }
        foreach ($dependency in @($pkg.dependencies)) {
            $missing = !$packageByName.ContainsKey($dependency)
            $dependencyStatus = if ($missing) { 'missing' } else { [string]$packageByName[$dependency].status }
            if ($dependencyStatus -in @('missing', 'invalid-package', 'blocked-dependency')) {
                if ($pkg.status -ne "blocked-dependency") { $changed = $true }
                $pkg.status = "blocked-dependency"
                $pkg.reason = "blocked by dependency '${dependency}': $dependencyStatus"
                break
            }
        }
    }
    if (!$changed) { break }
}

$report = [pscustomobject]@{
    format = "xlang-extlib-ecosystem-2"
    generatedAt = (Get-Date).ToUniversalTime().ToString("o")
    root = "release/extlibs"
    packages = $packages.ToArray()
}
if (!$CheckOnly) {
    [System.IO.File]::WriteAllText($outputJson, ($report | ConvertTo-Json -Depth 6), [System.Text.UTF8Encoding]::new($false))
}

$counts = @($packages | Group-Object status | Sort-Object Name)
$lines = New-Object System.Collections.Generic.List[string]
$lines.Add("# Extension Library Ecosystem Audit") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("Generated by tools/audit_extlib_ecosystem.ps1. No code is executed. Registration, source inventory and historical review candidates are not verification or release admission.") | Out-Null
$lines.Add("Only declared manifest sources are counted. ABI text matches are review hints, not proof of a compiler failure. Placeholder/new-code budget decisions require manual scope and cost evidence.") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("## Summary") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("| Status | Count |") | Out-Null
$lines.Add("| --- | ---: |") | Out-Null
foreach ($count in $counts) { $lines.Add(("| `{0}` | {1} |" -f $count.Name, $count.Count)) | Out-Null }
$lines.Add("") | Out-Null
$lines.Add("## Package Inventory") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("| Package | Status | Gates | XL | C | Focused file | Example file | Reason |") | Out-Null
$lines.Add("| --- | --- | --- | ---: | ---: | --- | --- | --- |") | Out-Null
foreach ($pkg in $packages) {
    $lines.Add(("| `{0}` | `{1}` | {2} | {3} | {4} | {5} | {6} | {7} |" -f $pkg.name, $pkg.status, ($pkg.gates -join ', '), $pkg.xlangSources, $pkg.cSources, $pkg.focusedCase, $pkg.example, $pkg.reason.Replace('|', '\|'))) | Out-Null
}
if (!$CheckOnly) {
    [System.IO.File]::WriteAllText($outputMarkdown, [string]::Join("`n", $lines) + "`n", [System.Text.UTF8Encoding]::new($false))
}

if (!$CheckOnly) {
    Write-Host "Extension ecosystem audit written: $outputJson"
    Write-Host "Extension ecosystem report written: $outputMarkdown"
}
foreach ($count in $counts) { Write-Host ("{0}: {1}" -f $count.Name, $count.Count) }
if ($CheckOnly -and @($packages | Where-Object { $_.status -eq 'invalid-package' }).Count -ne 0) {
    throw 'Extension ecosystem contains invalid package manifests or declared paths'
}
