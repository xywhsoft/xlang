param([string]$PublicDirectory = '', [string]$Directory = '')
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'xlir_native_process.ps1')
$rootPath = Split-Path -Parent $PSScriptRoot
Push-Location $rootPath
try {
    if (!$Directory) { $Directory = 'build/test/buffer-values-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') }
    $outPath = [IO.Path]::GetFullPath($Directory)
    if (Test-Path -LiteralPath $outPath) { throw 'Preserve prior Buffer evidence; choose a fresh directory' }
    New-Item -ItemType Directory -Path $outPath | Out-Null
    if (!$PublicDirectory) {
        $PublicDirectory = "$outPath/source"
        & "$PSScriptRoot/build_xlir_public_source_driver.ps1" -Directory $PublicDirectory
        if (!$?) { throw 'Current-source Buffer compiler build failed' }
    }
    $public = Get-Content -LiteralPath (Join-Path $PublicDirectory 'build.json') -Raw | ConvertFrom-Json
    if ($public.RuntimeOnly -or !$public.SourceXrt) { throw 'Buffer requires the full current-source compiler/runtime' }
    $gcc = (Get-Command gcc -CommandType Application).Source
    # These exact fixture names are canonical inventory references, not a
    # manual-only population or a successful subset selected after failures.
    $cases = @('stdlib_buffer_value','stdlib_buffer_snapshot','stdlib_buffer','stdlib_buffer_file_runtime',
        'stdlib_number_format','stdlib_number_values')
    $negative = @('bad_xlir_buffer_iterator_handle_read','bad_xlir_buffer_iterator_handle_write',
        'bad_xlir_buffer_text_result','bad_xlir_buffer_bytes_input','bad_xlir_buffer_iterator_owner')
    $files = @($cases + $negative | ForEach-Object { Get-Item "test/cases/$_.xl" }) +
        @(Get-Item $PSCommandPath,$gcc,test/cases/stdlib_buffer_value_host.c,test/support/buffer_value_native.c,
            test/cases/xlir_text_value_contract.c,test/cases/stdlib_string_support.c,
            tools/stage_xlir_artifact_runtime_sources.ps1,tools/xlir_native_process.ps1)
    $inputs = @($public.Inputs) + @($public.Headers) + @($public.Objects) +
        @($files | ForEach-Object { [pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash} })
    foreach ($record in $inputs) {
        if ((Get-FileHash -LiteralPath $record.Path).Hash -ne $record.Sha256) { throw "Stale Buffer input $($record.Path)" }
    }
    $rows = [Collections.Generic.List[object]]::new()
    function Run([string]$Name, [string]$File, [string[]]$Arguments, [bool]$Reject = $false) {
        $process = [Diagnostics.Process]::new()
        $process.StartInfo.FileName = $File
        $process.StartInfo.Arguments = Join-XlirWindowsCommandLine $Arguments
        $process.StartInfo.WorkingDirectory = $rootPath
        $process.StartInfo.UseShellExecute = $false
        $process.StartInfo.CreateNoWindow = $true
        $process.StartInfo.RedirectStandardOutput = $true
        $process.StartInfo.RedirectStandardError = $true
        $process.StartInfo.StandardOutputEncoding = [Text.Encoding]::UTF8
        $process.StartInfo.StandardErrorEncoding = [Text.Encoding]::UTF8
        try {
            [void]$process.Start()
            $stdout = $process.StandardOutput.ReadToEndAsync()
            $stderr = $process.StandardError.ReadToEndAsync()
            if (!$process.WaitForExit(45000)) { Stop-XlirProcessTree $process; throw "Timeout: $Name" }
            $code = $process.ExitCode
            [IO.File]::WriteAllText("$outPath/$Name.log", $stdout.Result + $stderr.Result, [Text.UTF8Encoding]::new($false))
        } finally { $process.Dispose() }
        $ok = if ($Reject) { $code -ne 0 } else { $code -eq 0 }
        $rows.Add([pscustomobject]@{Name=$Name;Exit=$code;Reject=$Reject;Complete=$ok})
        if (!$ok) { Get-Content "$outPath/$Name.log" -Tail 30; throw "$Name failed: $code" }
    }
    & "$PSScriptRoot/stage_xlir_artifact_runtime_sources.ps1" -Directory $PublicDirectory *> "$outPath/stage.log"
    if (!$?) { throw 'Buffer private source package staging failed' }
    Copy-Item -LiteralPath release/xl.dll -Destination "$outPath/xl.dll"
    $strict = @('-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-label',
        '-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-const-variable','-Wno-unused-but-set-variable',
        '-Wno-missing-field-initializers','-Wno-pointer-sign','-Wno-format-truncation',
        '-I.','-Irelease','-Irelease/include',"-I$PublicDirectory/include")
    $objects = @($public.Objects | Where-Object { [IO.Path]::GetFileName($_.Path) -ne 'xlir_public_source_driver.o' } | ForEach-Object Path)
    Run 'contract-build' $gcc ($strict + @('-DXL_STATIC_EMBED','-Ilib/tcc','test/cases/xlir_text_value_contract.c') + $objects +
        @('release/xl.dll','-lWs2_32','-lIPHLPAPI','-lShell32','-lOle32','-lAdvapi32','-o',"$outPath/contract.exe"))
    Run 'native-build' $gcc ($strict + @('test/cases/stdlib_buffer_value_host.c','test/support/buffer_value_native.c',
        'src/runtime/xl6_buffer_value.c','src/runtime/xl6_buffer_iterator.c','src/runtime/xl6_native_handle.c',
        'src/api/xl_foreign_error.c','-o',"$outPath/native.exe"))
    Run 'native-run' "$outPath/native.exe" @()
    Run 'native-reentry' "$outPath/native.exe" @('reentry')
    foreach ($operation in @('from-text','from-bytes','copy','assign','append','insert','self-assign',
        'self-append','self-insert','resize','trim','bytes','text','iterator','take')) {
        for ($point=1; $point -le 64; ++$point) {
            $name = "native.$operation.oom-$point"
            Run $name "$outPath/native.exe" @($operation,"$point")
            if ((Get-Content "$outPath/$name.log" -Raw) -match 'hit=0 balanced') { break }
        }
        if ($point -gt 64) { throw "Incomplete native allocation sweep: $operation" }
    }
    foreach ($case in $cases) {
        $source = "test/cases/$case.xl"; $prefix = "$outPath/$case"
        Run "$case.contract" "$outPath/contract.exe" @($source,"$prefix.c")
        Run "$case.strict-c" $gcc ($strict + @('-c',"$prefix.c",'-o',"$prefix.o"))
        if ((Get-Content -LiteralPath "$prefix.c" -Raw) -match 'xl6_xrt_buffer_(text|iter_[a-z_]+)\(|XL6_BufferIter|xl6_buffer_has_nul') {
            throw "Retired Buffer Cgen implementation emitted: $case"
        }
        $sidecars = if ($case -eq 'stdlib_buffer') { @('test/cases/stdlib_string_support.c') } else { @() }
        Run "$case.jit" $public.Driver (@($source,'run',$prefix) + $sidecars)
        Run "$case.aot-build" $public.Driver (@($source,'exe',$prefix) + $sidecars)
        Run "$case.aot-run" "$prefix.exe" @()
    }
    foreach ($case in $negative) {
        $path = "$outPath/$case.c"
        Run "$case.reject" $public.Driver @("test/cases/$case.xl",'cgen-default',$path) $true
        if (Test-Path -LiteralPath $path) { throw "Invalid Buffer declaration emitted C: $case" }
    }
    $unchanged = !@($inputs | Where-Object { (Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256 }).Count
    $report = [pscustomobject]@{Complete=$unchanged;InputsUnchanged=$unchanged;Inputs=$inputs;Checks=$rows.Count;
        PositiveFixtures=$cases;NegativeFixtures=$negative;Results=$rows;
        Scope='Windows current-source Buffer exact value/range/snapshot adapters and five ownership/ABI refusals; strict C/JIT/AOT plus actual selected native OOM, thread and reentry. Does not certify all legacy Buffer core/codecs, all containers, Linux, or final 53-module acceptance.'}
    [IO.File]::WriteAllText("$outPath/results.json", ($report | ConvertTo-Json -Depth 8), [Text.UTF8Encoding]::new($false))
    if (!$unchanged) { throw 'Buffer inputs changed during acceptance' }
    Write-Output "Buffer value acceptance: $($rows.Count) checks, unchanged inputs"
} finally { Pop-Location }
