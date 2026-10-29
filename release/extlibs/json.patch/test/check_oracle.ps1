$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$parent = Join-Path $root 'build/test/json_patch_oracle'
$work = Join-Path $parent ([guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($work) | Out-Null

Push-Location $root
try {
    $gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop).Source
    $python = (Get-Command python -CommandType Application -ErrorAction Stop |
        Select-Object -First 1).Source
    $flags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
        '-Ilib/xrt', '-I', 'release/extlibs/json.patch/include')
    $runtime = Join-Path $work 'xrt_runtime.o'
    & $gcc @flags -c release/extlibs/json.patch/test/xrt_runtime.c -o $runtime
    if ($LASTEXITCODE -ne 0) { throw 'json.patch oracle runtime compile failed' }
    $program = Join-Path $work 'oracle_cases.exe'
    & $gcc @flags release/extlibs/json.patch/json_patch_native.c `
        release/extlibs/json.patch/test/oracle_cases.c $runtime `
        -lWs2_32 -lAdvapi32 -lIphlpapi -lm -o $program
    if ($LASTEXITCODE -ne 0) { throw 'json.patch oracle program compile failed' }
    & $python release/extlibs/json.patch/test/oracle.py $program
    if ($LASTEXITCODE -ne 0) { throw 'json.patch oracle differs' }
} finally {
    Pop-Location
    $parentPath = [IO.Path]::GetFullPath($parent).TrimEnd('\') + '\'
    $workPath = [IO.Path]::GetFullPath($work)
    if (-not $workPath.StartsWith($parentPath,
        [StringComparison]::OrdinalIgnoreCase)) {
        throw "json.patch oracle work path escaped build/test: $workPath"
    }
    Remove-Item -LiteralPath $workPath -Recurse -Force
}
