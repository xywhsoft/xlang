param([string]$Directory = '', [string]$PublicDriver = '', [string]$XrtObject = '', [string]$Compiler = '')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
if (!$Directory) { $Directory = Join-Path $root ('build/test/foreign_bridges_' + [guid]::NewGuid().ToString('N')) }
$work = [IO.Path]::GetFullPath($Directory)
if (Test-Path -LiteralPath $work) { throw 'Choose a fresh verification directory' }
[IO.Directory]::CreateDirectory($work) | Out-Null
$records = [Collections.Generic.List[object]]::new()
function Run([string]$name, [string]$program, [string[]]$arguments) {
    $previous = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
    try { $output = & $program @arguments 2>&1; $code = $LASTEXITCODE }
    finally { $ErrorActionPreference = $previous }
    [IO.File]::WriteAllText((Join-Path $work ($name + '.log')), ($output -join "`n"))
    $records.Add([pscustomobject]@{ Name=$name; ExitCode=$code })
    if ($code -ne 0) { throw "${name}: exit $code`n$($output -join "`n")" }
    Write-Host "[foreign bridges] ${name}: PASS"
}
$previousPackages = $env:XLANG_PACKAGE_PATH
$env:XLANG_PACKAGE_PATH = Join-Path $root 'release/extlibs'
Push-Location $root
try {
    $gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop).Source
    $hostExe = Join-Path $work 'resource_host.exe'
    $implementation = $XrtObject
    if (!$implementation) {
        $implementation = Join-Path $work 'xrt.o'
        Run 'xrt_compile' $gcc @('-std=c11','-O0','-I.','-Irelease','-c',
            'tools/xrt_single_header_impl.c','-o',$implementation)
    }
    Run 'host_compile' $gcc @('-std=c11','-O0','-Wall','-Wextra','-Werror',
        '-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-I.','-Irelease',
        'test/cases/xlir_foreign_resource_host.c','src/runtime/xl6_foreign_resource.c',
        'src/runtime/xl6_closure.c','src/runtime/xl6_cell.c','src/runtime/xl6_code_lease.c',
        'src/runtime/xl6_ownership_plan.c',
        $implementation,'-lWs2_32','-lAdvapi32','-lIphlpapi','-lm','-o',$hostExe)
    Run 'host_faults' $hostExe @()
    $producer = Join-Path $work 'producer.dll'
    $adapter = Join-Path $work 'adapter.dll'
    foreach ($library in @($producer,$adapter)) {
        Run ([IO.Path]::GetFileNameWithoutExtension($library) + '_build') $gcc @(
            '-std=c11','-O2','-Wall','-Wextra','-Werror','-I.','-Irelease','-shared',
            'test/cases/xlir_foreign_resource_module.c','-o',$library)
    }
    $moduleHost = Join-Path $work 'module_host.exe'
    Run 'module_host_compile' $gcc @('-std=c11','-O2','-Wall','-Wextra','-Werror',
        '-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-I.','-Irelease',
        'test/cases/xlir_foreign_resource_module_host.c','src/runtime/xl6_foreign_resource.c',
        'src/runtime/xl6_closure.c','src/runtime/xl6_cell.c','src/runtime/xl6_code_lease.c',
        $implementation,'-lWs2_32','-lAdvapi32','-lIphlpapi','-lm','-o',$moduleHost)
    Run 'module_lifetime' $moduleHost @($producer,$adapter)
    $source = 'test/cases/xlir_foreign_bridges.xl'
    $native = 'test/cases/xlir_foreign_bridges_native.c'
    $prefix = Join-Path $work 'bridges'
    if ($PublicDriver) {
        Run 'jit' $PublicDriver @($source,'run',($prefix + '_jit'),$native)
        Run 'aot_build' $PublicDriver @($source,'exe',$prefix,$native)
    } else {
        $xl = if ($Compiler) { $Compiler } else { Join-Path $root 'release/xl.exe' }
        Run 'jit' $xl @('run',$source,$native,'--backend','xlir')
        Run 'aot_build' $xl @('build',$source,$native,'-o',($prefix + '.exe'),'--backend','xlir')
    }
    Run 'aot_execute' ($prefix + '.exe') @()
} finally {
    [IO.File]::WriteAllText((Join-Path $work 'results.json'), ($records | ConvertTo-Json -Depth 4))
    $env:XLANG_PACKAGE_PATH = $previousPackages
    Pop-Location
}
