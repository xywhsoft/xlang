param([switch]$SkipAot)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../../../..')).Path
. (Join-Path $root 'tools/extlib_smoke_process.ps1')
$work = Join-Path $root ('build/test/tar_text_guard_' + [guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($work)
$calls = [Collections.Generic.List[object]]::new()
$inputs = @('release/xl.exe', 'release/xl.dll', 'release/xlc.dll', 'lib/xrt/xrt.h',
    'lib/xruntime/xruntime.h', 'tools/extlib_smoke_process.ps1', 'tools/xlir_native_process.ps1')
foreach ($name in @('tar', 'gzip', '_zlib', 'bz2', '_bzip2', 'lzma')) {
    $inputs += (Get-ChildItem (Join-Path $root "release/extlibs/$name") -Recurse -File |
        Where-Object { $_.Extension -in @('.c', '.h', '.xl', '.py', '.ps1', '.sh') -or
            $_.Name -in @('README.md', 'xlang.package.json') }).FullName
}
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$python = (Get-Command python -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$inputs += @($gcc, $python, (& $gcc -print-prog-name=cc1))
$hashes = @{}
foreach ($p in $inputs) {
    $full = if ([IO.Path]::IsPathRooted($p)) { $p } else { Join-Path $root $p }
    $hashes[$full] = (Get-FileHash -LiteralPath $full).Hash
}
$passed = $false
function Run([string]$label, [string]$program, [string[]]$arguments, [string]$directory=$root) {
    $result = Invoke-ExtlibSmokeProcess $program $arguments $directory 180
    $calls.Add([pscustomobject]@{label=$label;program=$program;arguments=$arguments;result=$result})
    if ($result.status -ne 'pass') { throw "$label failed: $($result.stderr)" }
    Write-Host "[tar text] ${label}: PASS"
}
Push-Location $root
try {
    $flags = @('-std=c11','-O2','-Wall','-Wextra','-Werror','-DXRT_MODULE_MEMORY_DEBUG',
        '-Ilib/xrt','-Irelease/extlibs/tar')
    $runtime = 'release/extlibs/tar/test/runtime.c'
    $libs = @('-lws2_32','-lbcrypt','-lshlwapi')
    $probe = Join-Path $work 'matrix.exe'
    Run 'C matrix compile' $gcc ($flags + @('release/extlibs/tar/test/test_text_metadata.c',
        $runtime,'-o',$probe) + $libs)
    Run 'C matrix' $probe @()
    $core = Join-Path $work 'core.exe'
    Run 'C core compile' $gcc ($flags + @('release/extlibs/tar/test/test_tar_c.c',
        'release/extlibs/tar/xrt_tar_common.c','release/extlibs/tar/xrt_tar_reader.c',
        'release/extlibs/tar/xrt_tar_writer.c','release/extlibs/tar/xrt_tar_extract.c',
        $runtime,'-o',$core) + $libs)
    [void][IO.Directory]::CreateDirectory((Join-Path $work 'run'))
    Run 'C core and extraction guard' $core @() (Join-Path $work 'run')
    Run 'C matrix JIT' "$root/release/xl.exe" @('run','release/extlibs/tar/test/test_text_metadata.c')
    Run 'XL text JIT' "$root/release/xl.exe" @('run','release/extlibs/tar/test/text_metadata.xl')
    $example = Join-Path $work 'metadata.exe'
    Run 'C example compile' $gcc ($flags + @('release/extlibs/tar/examples/metadata_hex_c.c',
        $runtime,'-o',$example) + $libs)
    $oracle = @('-B','-X','utf8','release/extlibs/tar/test/text_metadata.py',
        "$root/release",$example,(Join-Path $work 'oracle'))
    if (-not $SkipAot) {
        foreach ($source in @('test/text_metadata.xl','test/test_text_metadata.c',
                'examples/metadata_hex.xl','examples/metadata_hex_c.c')) {
            $output = Join-Path $work ([IO.Path]::GetFileName($source) + '.exe')
            Run "$source AOT build" "$root/release/xl.exe" @('build',"release/extlibs/tar/$source",'-o',$output)
            if ($source.StartsWith('test/')) { Run "$source AOT run" $output @() }
        }
        $oracle += @('--xl-aot',(Join-Path $work 'metadata_hex.xl.exe'),
            '--c-aot',(Join-Path $work 'metadata_hex_c.c.exe'))
    }
    Run 'independent Python metadata' $python $oracle
    $passed = $true
} finally {
    Pop-Location
    $changed = @($hashes.Keys | Where-Object { (Get-FileHash -LiteralPath $_).Hash -ne $hashes[$_] })
    $report = @{passed=($passed -and $changed.Count -eq 0);inputs=$hashes;changed=$changed;calls=@($calls.ToArray());work=$work;skipAot=[bool]$SkipAot}
    [IO.File]::WriteAllText(($work + '.report.json'),($report | ConvertTo-Json -Depth 10),[Text.UTF8Encoding]::new($false))
    Write-Host "[tar text] artifacts: $work"
    if ($changed.Count -ne 0) { throw "TAR text inputs changed: $changed" }
}
