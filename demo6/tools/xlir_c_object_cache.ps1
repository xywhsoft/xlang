# Shared by the two source validation builders. This caches compilation, never
# validation results or links. Every invocation preprocesses the actual C input:
# included .c files, headers, include shadowing and predefined macros therefore
# participate in the key without an incomplete hand-maintained dependency list.
$script:XlirCObjectCacheImplementation = $PSCommandPath
. (Join-Path $PSScriptRoot 'xlir_native_process.ps1')

function Get-XlirTextHash([string]$Text) {
    $algorithm = [Security.Cryptography.SHA256]::Create()
    try { [BitConverter]::ToString($algorithm.ComputeHash([Text.Encoding]::UTF8.GetBytes($Text))).Replace('-', '') }
    finally { $algorithm.Dispose() }
}

function New-XlirCBuildContext {
    param([string]$Root, [string]$Directory, [string]$CacheDirectory = '', [switch]$NoCache)
    $compiler = (Get-Command gcc -CommandType Application -ErrorAction Stop).Source
    $rootPath = (Resolve-Path -LiteralPath $Root).Path
    $directoryPath = [IO.Path]::GetFullPath($Directory)
    if (!$CacheDirectory) { $CacheDirectory = Join-Path $rootPath 'build/cache/xlir-c-objects-v1' }
    $cachePath = [IO.Path]::GetFullPath($CacheDirectory)
    $programs = @($compiler)
    foreach ($program in @('cc1', 'as')) {
        $programPath = (& $compiler "-print-prog-name=$program" | Out-String).Trim()
        if ($LASTEXITCODE -ne 0 -or !$programPath) { throw "Cannot identify GCC $program" }
        if (![IO.Path]::IsPathRooted($programPath)) {
            $adjacent = Join-Path (Split-Path -Parent $compiler) ($programPath + '.exe')
            $programPath = if (Test-Path -LiteralPath $adjacent -PathType Leaf) { $adjacent } else {
                (Get-Command $programPath -CommandType Application -ErrorAction Stop).Source
            }
        }
        $programs += (Resolve-Path -LiteralPath $programPath).Path
    }
    # Compiler companion DLLs are executable inputs too, not C dependencies.
    foreach ($parent in @($programs | ForEach-Object { Split-Path -Parent $_ } | Select-Object -Unique)) {
        $programs += @(Get-ChildItem -LiteralPath $parent -File -Filter '*.dll' | Select-Object -ExpandProperty FullName)
    }
    $tools = @($programs | Sort-Object -Unique | ForEach-Object {
        [pscustomobject]@{ Path = $_; Sha256 = (Get-FileHash -LiteralPath $_).Hash }
    })
    $specs = (& $compiler -dumpspecs | Out-String)
    if ($LASTEXITCODE -ne 0) { throw 'Cannot identify GCC specs' }
    $search = (& $compiler -print-search-dirs | Out-String)
    if ($LASTEXITCODE -ne 0) { throw 'Cannot identify GCC search paths' }
    # These overrides may select compiler/assembler binaries other than the
    # ones above. Preserve the environment but conservatively bypass reuse.
    $overrides = @('GCC_EXEC_PREFIX', 'COMPILER_PATH' | Where-Object { [Environment]::GetEnvironmentVariable($_) })
    $implementation = Get-XlirTextHash ((Get-FileHash -LiteralPath $script:XlirCObjectCacheImplementation).Hash +
        (Get-FileHash -LiteralPath $script:XlirNativeProcessImplementation).Hash)
    $toolchainKey = Get-XlirTextHash (($tools | ConvertTo-Json -Compress) + $specs + $search + $implementation)
    [IO.Directory]::CreateDirectory($directoryPath) | Out-Null
    if (!$NoCache -and !$overrides.Count) { [IO.Directory]::CreateDirectory($cachePath) | Out-Null }
    [pscustomobject]@{
        Compiler = $compiler; Root = $rootPath; Directory = $directoryPath; CacheDirectory = $cachePath
        Toolchain = $tools; ToolchainSha256 = $toolchainKey
        CacheEnabled = !$NoCache -and !$overrides.Count
        DisabledReason = if ($NoCache) { 'requested clean compilation' } elseif ($overrides.Count) { 'compiler path override' } else { '' }
    }
}

function Invoke-XlirCObject {
    param([Parameter(Mandatory = $true)]$Context,
        [Parameter(Mandatory = $true)][string]$Source,
        [Parameter(Mandatory = $true)][string]$Object,
        [Parameter(Mandatory = $true)][string[]]$Flags)
    # This helper accepts one ordinary C compilation, not response files,
    # plugins, alternate frontends, assembler input or alternate tool searches.
    foreach ($flag in $Flags) {
        if ($flag -notmatch '^-(std=c11|W[A-Za-z0-9_=-]+|O[0123sg]|I.+|D.+|ffunction-sections|fdata-sections|fwhole-program)$') {
            throw "Unsupported cached C build flag: $flag"
        }
    }
    $sourcePath = (Resolve-Path -LiteralPath $Source).Path
    $objectPath = [IO.Path]::GetFullPath($Object)
    if (Test-Path -LiteralPath $objectPath) { throw "Refusing to overwrite a batch object: $objectPath" }
    $prefix = Join-Path $Context.Directory ([IO.Path]::GetFileNameWithoutExtension($objectPath))
    $preprocessed = "$prefix.i"
    $preprocessLog = "$prefix.preprocess.log"
    $compileLog = "$prefix.compile.log"
    foreach ($path in @($preprocessed, $preprocessLog, $compileLog)) {
        if (Test-Path -LiteralPath $path) { throw "Choose fresh build outputs: $path" }
    }
    $timer = [Diagnostics.Stopwatch]::StartNew()
    $hit = $false; $rejected = $false; $compileSeconds = 0.0
    Push-Location $Context.Root
    try {
        # Comments carry GCC fallthrough annotations. Preserve macro comments
        # as well so splitting preprocessing from compilation cannot silently
        # change the strict warning contract.
        $preprocessExit = Invoke-XlirNativeToLog $Context.Compiler `
            (@($Flags) + @('-E', '-CC', $sourcePath, '-o', $preprocessed)) $preprocessLog
        if ($preprocessExit -ne 0) {
            Get-Content -LiteralPath $preprocessLog -Tail 30 | Write-Host
            throw "C preprocessing failed: $sourcePath"
        }
        $preprocessSeconds = $timer.Elapsed.TotalSeconds
        $preprocessedHash = (Get-FileHash -LiteralPath $preprocessed).Hash
        $key = Get-XlirTextHash (([ordered]@{
            Version = 1; Toolchain = $Context.ToolchainSha256; Root = $Context.Root
            Source = $sourcePath; Flags = $Flags; Preprocessed = $preprocessedHash
        } | ConvertTo-Json -Depth 5 -Compress))
        # Assembly file directives and PCH pragmas have additional inputs not
        # represented by the preprocessed bytes. Never cache those units.
        $extraInput = Select-String -LiteralPath $preprocessed -Pattern '\.(incbin|include)([ \t]|\\[tnr])+\\?"|#pragma GCC pch_preprocess' -Quiet
        $cacheable = $Context.CacheEnabled -and !$extraInput
        $entry = Join-Path $Context.CacheDirectory $key
        if ($cacheable -and (Test-Path -LiteralPath $entry -PathType Container)) {
            try {
                $receipt = Get-Content -LiteralPath (Join-Path $entry 'receipt.json') -Raw | ConvertFrom-Json
                $cachedObject = Join-Path $entry 'object.o'
                $cachedLog = Join-Path $entry 'compile.log'
                if ($receipt.Key -ne $key -or $receipt.PreprocessedSha256 -ne $preprocessedHash -or
                    (Get-FileHash -LiteralPath $cachedObject).Hash -ne $receipt.ObjectSha256 -or
                    (Get-FileHash -LiteralPath $cachedLog).Hash -ne $receipt.LogSha256) { throw 'Cache content mismatch' }
                Copy-Item -LiteralPath $cachedObject -Destination $objectPath
                Copy-Item -LiteralPath $cachedLog -Destination $compileLog
                if ((Get-FileHash -LiteralPath $objectPath).Hash -ne $receipt.ObjectSha256) { throw 'Cache copy mismatch' }
                $hit = $true
            } catch {
                # A corrupt/incomplete cache is not evidence. Preserve it and
                # rebuild privately; do not replace or repair another entry.
                $rejected = $true
                if (Test-Path -LiteralPath $objectPath) { Remove-Item -LiteralPath $objectPath }
                if (Test-Path -LiteralPath $compileLog) { Remove-Item -LiteralPath $compileLog }
            }
        }
        if (!$hit) {
            $compileTimer = [Diagnostics.Stopwatch]::StartNew()
            $compileExit = Invoke-XlirNativeToLog $Context.Compiler `
                (@($Flags) + @('-x', 'cpp-output', '-c', $preprocessed, '-o', $objectPath)) $compileLog
            $compileSeconds = $compileTimer.Elapsed.TotalSeconds
            if ($compileExit -ne 0) {
                Get-Content -LiteralPath $compileLog -Tail 30 | Write-Host
                throw "C compilation failed: $sourcePath"
            }
            if ($cacheable -and !$rejected) {
                # Publish a complete immutable directory in one rename. A
                # competing builder may win; its result is never overwritten.
                $pending = Join-Path $Context.CacheDirectory ($key + '.pending-' + [Guid]::NewGuid().ToString('N'))
                [IO.Directory]::CreateDirectory($pending) | Out-Null
                Copy-Item -LiteralPath $objectPath -Destination (Join-Path $pending 'object.o')
                Copy-Item -LiteralPath $compileLog -Destination (Join-Path $pending 'compile.log')
                $receipt = [ordered]@{
                    Key = $key; Source = $sourcePath; PreprocessedSha256 = $preprocessedHash
                    ToolchainSha256 = $Context.ToolchainSha256; Flags = $Flags
                    ObjectSha256 = (Get-FileHash -LiteralPath $objectPath).Hash
                    LogSha256 = (Get-FileHash -LiteralPath $compileLog).Hash
                }
                [IO.File]::WriteAllText((Join-Path $pending 'receipt.json'), ($receipt | ConvertTo-Json -Depth 5), [Text.UTF8Encoding]::new($false))
                try { [IO.Directory]::Move($pending, $entry) }
                catch { if (!(Test-Path -LiteralPath $entry -PathType Container)) { throw } }
            }
        }
        $timer.Stop()
        [pscustomobject]@{
            Source = $sourcePath; Path = $objectPath; Sha256 = (Get-FileHash -LiteralPath $objectPath).Hash
            PreprocessedSha256 = $preprocessedHash; CacheKey = $key; CacheHit = $hit; CacheRejected = $rejected
            CacheEligible = $cacheable; PreprocessSeconds = [Math]::Round($preprocessSeconds, 3)
            CompileSeconds = [Math]::Round($compileSeconds, 3); TotalSeconds = [Math]::Round($timer.Elapsed.TotalSeconds, 3)
            PreprocessLog = $preprocessLog; CompileLog = $compileLog
        }
    } finally {
        Pop-Location
        # This private intermediate was created above. Keep its hash and both
        # logs, but not tens of duplicate megabytes per translation unit.
        if (Test-Path -LiteralPath $preprocessed) { Remove-Item -LiteralPath $preprocessed }
    }
}

function Assert-XlirCToolchainUnchanged($Context) {
    foreach ($input in $Context.Toolchain) {
        if ((Get-FileHash -LiteralPath $input.Path).Hash -ne $input.Sha256) { throw "Compiler tool changed: $($input.Path)" }
    }
}
