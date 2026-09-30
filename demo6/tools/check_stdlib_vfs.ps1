param([string]$Root = "")
$ErrorActionPreference = "Stop"
if ($Root -eq "") { $Root = Split-Path -Parent $PSScriptRoot }
$Root = (Resolve-Path -LiteralPath $Root).Path
$xl = Join-Path $Root "release\xl.exe"
$outputDir = Join-Path $Root "build\test"
[System.IO.Directory]::CreateDirectory($outputDir) | Out-Null

function Invoke-Test([string]$File, [string]$Arguments, [int]$Expected = 0, [string]$Marker = "") {
    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = New-Object System.Diagnostics.ProcessStartInfo
    $process.StartInfo.FileName = $File
    $process.StartInfo.Arguments = $Arguments
    $process.StartInfo.WorkingDirectory = $Root
    $process.StartInfo.UseShellExecute = $false
    $process.StartInfo.CreateNoWindow = $true
    $process.StartInfo.RedirectStandardInput = $true
    $process.StartInfo.RedirectStandardOutput = $true
    $process.StartInfo.RedirectStandardError = $true
    $process.StartInfo.StandardOutputEncoding = [System.Text.Encoding]::UTF8
    $process.StartInfo.StandardErrorEncoding = [System.Text.Encoding]::UTF8
    try {
        [void]$process.Start()
        $process.StandardInput.Close()
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        if (!$process.WaitForExit(60000)) { $process.Kill(); $process.WaitForExit(); throw "Timed out: $File $Arguments" }
        $diagnostic = $stdout.Result + $stderr.Result
        if ($process.ExitCode -ne $Expected -or ($Marker -ne "" -and !$diagnostic.Contains($Marker))) {
            throw "VFS gate failed: $File $Arguments exit=$($process.ExitCode) $diagnostic"
        }
    } finally { $process.Dispose() }
}
function Compile-Strict([string]$Source, [string]$Executable) {
    $command = '!XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE=\"{0}\" test\cases\xlir_generated_module_host.c !XL6_CGEN_LINKS! -o "{1}"' -f $Source.Replace('\','/'),$Executable
    & cmd.exe /d /v:on /c $command
    if ($LASTEXITCODE -ne 0) { throw "Strict generated C failed: $Source" }
}

$compilerEnvironmentBefore = @{}
Push-Location $Root
try {
    # Binary fixtures are generated independently of the language's IO library.
    [System.IO.File]::WriteAllBytes((Join-Path $outputDir 'vfs-disk.bin'), [byte[]]@(97,0,98))
    $fixture = @'
import pathlib, struct, zlib
data = b'a\0b'
name = b'entry.bin'
index = struct.pack('<HBBIQQQII', len(name), 0, 0, 0, 80, len(data), len(data), zlib.crc32(data), 0) + name
size = 80 + len(data) + len(index) + 32
header = struct.pack('<8sHHIII6QII', b'XRTPACK\0', 1, 0, 80, 0, 1, 80, len(data), 80 + len(data), len(index), size, len(data), zlib.crc32(index), 0)
trailer = struct.pack('<8sQQ', b'XRTPEND\0', size, 0)
archive = header + data + index + trailer + struct.pack('<II', zlib.crc32(trailer + header), 0)
pathlib.Path('build/test/vfs-pack.bin').write_bytes(archive)
damaged = bytearray(archive)
damaged[80] ^= 1
pathlib.Path('build/test/vfs-pack-corrupt.bin').write_bytes(damaged)
pathlib.Path('build/test/vfs-pack-slice.bin').write_bytes(b'prefix!' + archive + b'tail')
'@
    $fixture | & python -
    if ($LASTEXITCODE -ne 0) { throw "VFS pack fixture generation failed" }
    # One shared compiler preparation for the whole batch.
    $toolchainOutput = & cmd.exe /d /v:on /c 'call test\suites\common.bat && set XL6_CGEN'
    if ($LASTEXITCODE -ne 0) { throw "VFS toolchain preparation failed" }
    foreach ($line in $toolchainOutput) {
        if ($line -match '^(XL6_CGEN_[A-Z_]+)=(.*)$') {
            $key = $Matches[1]
            $compilerEnvironmentBefore[$key] = [Environment]::GetEnvironmentVariable($key, 'Process')
            [Environment]::SetEnvironmentVariable($key, $Matches[2], 'Process')
        } else { Write-Output $line }
    }
    foreach ($key in @('XL6_CGEN_CC', 'XL6_CGEN_FLAGS', 'XL6_CGEN_INCLUDES', 'XL6_CGEN_LINKS')) {
        if (![Environment]::GetEnvironmentVariable($key, 'Process')) { throw "Missing compiler variable: $key" }
    }
    $fixtures = @('test/cases/stdlib_vfs_memory.xl', 'test/cases/stdlib_vfs_overlay.xl',
        'test/cases/stdlib_vfs_disk.xl', 'test/cases/stdlib_vfs_limits.xl',
        'test/cases/stdlib_vfs_minimal.xl', 'test/cases/stdlib_vfs_pack.xl',
        'test/cases/stdlib_vfs_pack_limits.xl', 'test/cases/stdlib_vfs_lifetime.xl')
    foreach ($relative in $fixtures) {
        $name = [System.IO.Path]::GetFileNameWithoutExtension($relative)
        $source = Join-Path $Root $relative
        $cPath = Join-Path $outputDir "$name.c"
        $repeat = Join-Path $outputDir "$name.repeat.c"
        Invoke-Test $xl ('check "{0}"' -f $source)
        Invoke-Test $xl ('cgen "{0}" --emit "{1}"' -f $source,$cPath)
        Invoke-Test $xl ('cgen "{0}" --emit "{1}"' -f $source,$repeat)
        $c = [System.IO.File]::ReadAllText($cPath)
        if ($c -cne [System.IO.File]::ReadAllText($repeat)) { throw "$name is not deterministic" }
        if ($c -match '#include\s*[<\"][^>\"]*(?<!_decl)\b(?:xrt|xruntime)\.h[>\"]') { throw "$name imports an implementation header" }
        if ($name -eq 'stdlib_vfs_minimal' -and $c -match 'xl6_xrt_vfs_(?:pack|disk)_|XRT_MODULE_VFS_(?:PACK|DISK)') { throw "Memory-only program retains disk/pack adapters" }
        $strict = Join-Path $outputDir "$name.strict.exe"
        Compile-Strict $cPath $strict
        Invoke-Test $strict ""
        Invoke-Test $xl ('run "{0}"' -f $source)
        $exe = Join-Path $outputDir "$name.aot.exe"
        Invoke-Test $xl ('build "{0}" -o "{1}"' -f $source,$exe)
        Invoke-Test $exe ""
        Write-Output "${name}: strict C, JIT/AOT, deterministic output and dependency isolation passed"
    }
    foreach ($relative in @('test/cases/bad_xlir_vfs_wrong_input.xl',
        'test/cases/bad_xlir_vfs_wrong_result.xl', 'test/cases/bad_xlir_vfs_wrong_owner.xl')) {
        $name = [System.IO.Path]::GetFileNameWithoutExtension($relative)
        $cPath = Join-Path $outputDir "$name.c"
        if (Test-Path -LiteralPath $cPath) { Remove-Item -LiteralPath $cPath }
        $marker = if ($name -eq 'bad_xlir_vfs_wrong_owner') { 'exact compiler-runtime bridge ABI' } else { 'XLIR VFS' }
        Invoke-Test $xl ('cgen "{0}" --emit "{1}"' -f (Join-Path $Root $relative),$cPath) 1 $marker
        if (Test-Path -LiteralPath $cPath) { throw "Invalid VFS resource ABI emitted C: $name" }
    }
    $example = Join-Path $Root 'release/examples/stdlib_vfs.xl'
    $exampleExe = Join-Path $outputDir 'stdlib_vfs.example.exe'
    Invoke-Test $xl ('run "{0}"' -f $example)
    Invoke-Test $xl ('build "{0}" -o "{1}"' -f $example,$exampleExe)
    Invoke-Test $exampleExe ""
} finally {
    foreach ($key in $compilerEnvironmentBefore.Keys) {
        [Environment]::SetEnvironmentVariable($key, $compilerEnvironmentBefore[$key], 'Process')
    }
    Pop-Location
}
Write-Output 'VFS foundation gate passed: binary/NUL/UTF-8, clamping, overlay/MISS/ERROR, pack CRC/slices/limits, lifetime, strict C, JIT/AOT and three ABI negatives.'
