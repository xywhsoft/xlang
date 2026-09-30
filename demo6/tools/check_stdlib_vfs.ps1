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
import pathlib, struct, zlib, lzma
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

# Independent stored/LZMA1 vectors for both public archive formats.
decoded = b'compressed hello'
encoded = b'\x5d\x00\x00\x01\x00' + lzma.compress(decoded, format=lzma.FORMAT_RAW,
    filters=[{'id': lzma.FILTER_LZMA1, 'dict_size': 65536, 'lc': 3, 'lp': 0, 'pb': 2}])
items = sorted([(b'entry.bin', data, data, 0), (b'dir/text.txt', encoded, decoded, 1)])
def pack_vector(legacy):
    header_size = 64 if legacy else 80
    payload = b''
    index = b''
    for name, stored, plain, codec in items:
        offset = header_size + len(payload)
        if legacy:
            index += struct.pack('<H',len(name)) + name + struct.pack('<BBQIQI',
                codec,0,offset,len(stored),len(plain),zlib.crc32(plain))
        else:
            index += struct.pack('<HBBIQQQII',len(name),codec,0,0,offset,
                len(stored),len(plain),zlib.crc32(plain),0) + name
        payload += stored
    total = header_size + len(payload) + len(index) + (24 if legacy else 32)
    if legacy:
        header = struct.pack('<8sHHIQQQI20s',b'XSVFHDR\0',1,64,len(items),64,
            64+len(payload),total,zlib.crc32(index),b'\0'*20)
        trailer = struct.pack('<8sQ',b'XSVPACK\0',0)
    else:
        header = struct.pack('<8sHHIII6QII',b'XRTPACK\0',1,0,80,0,len(items),80,
            len(payload),80+len(payload),len(index),total,19,zlib.crc32(index),0)
        trailer = struct.pack('<8sQQ',b'XRTPEND\0',total,0)
    return header+payload+index+trailer+struct.pack('<II',zlib.crc32(trailer+header),0)
for legacy in (False,True):
    pathlib.Path('build/test/vfs-pack-full-%d.bin' % (2 if legacy else 1)).write_bytes(pack_vector(legacy))
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
        'test/cases/stdlib_vfs_pack_limits.xl', 'test/cases/stdlib_vfs_lifetime.xl',
        'test/cases/stdlib_fs_handles.xl', 'test/cases/stdlib_fs_native.xl',
        'test/cases/stdlib_fs_resources.xl', 'test/cases/stdlib_fs_root.xl',
        'test/cases/stdlib_fs_allocation.xl', 'test/cases/stdlib_fs_path.xl',
        'test/cases/stdlib_fs_whole.xl', 'test/cases/stdlib_file_bytes_only.xl',
        'test/cases/stdlib_file_read_text_binary_reject.xl',
        'test/cases/stdlib_fs_path_whole_allocation.xl', 'test/cases/stdlib_fs_platform.xl',
        'test/cases/stdlib_fs_tree.xl', 'test/cases/stdlib_fs_temp.xl', 'test/cases/stdlib_fs_tree_temp_allocation.xl',
        'test/cases/stdlib_vfs_pack_full.xl', 'test/cases/stdlib_vfs_pack_allocation.xl', 'test/cases/stdlib_fs_names.xl',
        'test/cases/stdlib_fs_walk.xl', 'test/cases/stdlib_fs_glob_only.xl', 'test/cases/stdlib_fs_walk_allocation.xl',
        'test/cases/xlir_bytes_dynamic_cast.xl','test/cases/xlir_directory_bridge.xl')
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
        if ($name -eq 'stdlib_vfs_minimal' -and $c -match 'XRT_MODULE_FILE_(?:ROOT|MAP|LOCK)|xl6_xrt_fs_') { throw 'Memory-only program retains unused File resources' }
        if ($name -in @('stdlib_fs_whole','stdlib_fs_path','stdlib_file_bytes_only') -and $c -match 'XL6_FILE_RUNTIME') { throw 'New filesystem public API retains the legacy monolithic file emitter' }
        if ($name -eq 'stdlib_file_bytes_only' -and $c -match 'XL6_BUFFER_RUNTIME') { throw 'Bytes-only program retains Buffer adapters' }
        if ($name -in @('stdlib_fs_walk','stdlib_fs_glob_only') -and $c -match 'XL6_DIR_RUNTIME') { throw 'New directory API retains the legacy directory emitter' }
        if ($name -eq 'stdlib_fs_glob_only' -and $c -match 'xlir_fs_walk_entry|xlir_fs_walk_callbacks') { throw 'Glob-only program retains unused event/callback adapters' }
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
        'test/cases/bad_xlir_vfs_wrong_result.xl', 'test/cases/bad_xlir_vfs_wrong_owner.xl',
        'test/cases/bad_xlir_fs_wrong_owner.xl', 'test/cases/bad_xlir_fs_wrong_result.xl',
        'test/cases/bad_xlir_fs_pack_wrong_options.xl', 'test/cases/bad_xlir_fs_wrong_array_element.xl',
        'test/cases/bad_xlir_fs_wrong_visitor.xl')) {
        $name = [System.IO.Path]::GetFileNameWithoutExtension($relative)
        $cPath = Join-Path $outputDir "$name.c"
        if (Test-Path -LiteralPath $cPath) { Remove-Item -LiteralPath $cPath }
        $marker = if ($name -in @('bad_xlir_fs_wrong_array_element','bad_xlir_fs_wrong_visitor')) { 'XLIR VFS runtime bridge' } elseif ($name -eq 'bad_xlir_vfs_wrong_owner' -or $name -like 'bad_xlir_fs_*') { 'exact compiler-runtime bridge ABI' } else { 'XLIR VFS' }
        Invoke-Test $xl ('cgen "{0}" --emit "{1}"' -f (Join-Path $Root $relative),$cPath) 1 $marker
        if (Test-Path -LiteralPath $cPath) { throw "Invalid VFS resource ABI emitted C: $name" }
    }
    foreach ($relative in @('release/examples/stdlib_vfs.xl', 'release/examples/stdlib_fs_file.xl',
        'release/examples/stdlib_fs_paths_text.xl','release/examples/stdlib_fs_tree_temp.xl',
        'release/examples/stdlib_fs_walk.xl')) {
        $example = Join-Path $Root $relative
        $exampleExe = Join-Path $outputDir ([IO.Path]::GetFileNameWithoutExtension($relative) + '.example.exe')
        Invoke-Test $xl ('run "{0}"' -f $example)
        Invoke-Test $xl ('build "{0}" -o "{1}"' -f $example,$exampleExe)
        Invoke-Test $exampleExe ""
    }
} finally {
    foreach ($key in $compilerEnvironmentBefore.Keys) {
        [Environment]::SetEnvironmentVariable($key, $compilerEnvironmentBefore[$key], 'Process')
    }
    Pop-Location
}
Write-Output 'VFS/File gate passed: thirty positive fixtures, strict C, JIT/AOT, deterministic output, resource/length/close/OOM boundaries and eight ABI negatives.'
