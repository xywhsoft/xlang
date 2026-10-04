param([string]$PublicDirectory = '', [string]$Directory = '')
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'xlir_native_process.ps1')
$rootPath = Split-Path -Parent $PSScriptRoot
Push-Location $rootPath
try {
    if (!$Directory) { $Directory = 'build/test/stack-native-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') }
    $outPath = [IO.Path]::GetFullPath($Directory)
    if (Test-Path -LiteralPath $outPath) { throw 'Preserve prior Stack native evidence' }
    New-Item -ItemType Directory -Path $outPath | Out-Null
    if (!$PublicDirectory) {
        $PublicDirectory = "$outPath/source"
        & "$PSScriptRoot/build_xlir_public_source_driver.ps1" -Directory $PublicDirectory
        if (!$?) { throw 'Current-source Stack compiler build failed' }
    }
    $public = Get-Content -LiteralPath (Join-Path $PublicDirectory 'build.json') -Raw | ConvertFrom-Json
    if ($public.RuntimeOnly -or !$public.SourceXrt) { throw 'Full current-source compiler/runtime required' }
    $cases = @('stdlib_stack_native','stdlib_stack_scalar','xlir_stack_resource_abi','xlir_stack_class_default_abi','stdlib_stack_values','stdlib_stack_owned','stdlib_stack_owned_continuation','stdlib_stack_try_owned','stdlib_stack_try_continuation',
        'stdlib_stack_nullable_scalar','stdlib_stack_nullable_continuation','stdlib_stack_nullable_i64','stdlib_stack_nullable_bool','stdlib_stack_nullable_u64','stdlib_stack_nullable_f32','stdlib_stack_nullable_f64','stdlib_stack_nullable_time','stdlib_stack_nullable_char')
    $extraCases = @('xlir_nullable_scalar_no_resource','stdlib_nullable_resource_transport','stdlib_nullable_queue_transport')
    $negative = @('char_output_type','bytes_output_type','bytes_input_type','owned_argument','owned_depth','owned_result','try_argument','try_depth','try_result','try_nullable_result',
        'nullable_literal_range','nullable_variable_narrow','nullable_identity')
    $gcc = (Get-Command gcc -CommandType Application | Select-Object -First 1).Source
    $python = (Get-Command python -CommandType Application | Select-Object -First 1).Source
    $files = @(($cases + $extraCases) | ForEach-Object { Get-Item "test/cases/$_.xl" }) +
        @($negative | ForEach-Object { Get-Item "test/fixtures/stack_value_contract/$_.xl" }) +
        @(Get-Item -LiteralPath @($PSCommandPath,$gcc,$python,'release/xl.dll','test/cases/stdlib_stack_native_host.c',
            'test/support/stack_native.c','test/cases/xlir_module_product_oom_host.c',
            'test/support/module_product_memory.c','test/support/module_product_memory.h',
            'test/cases/xlir_stack_native_contract.c','test/cases/xlir_nullable_scalar_native.c','tools/check_xlir_artifact_runtime.ps1'))
    $inputs = @($public.Inputs) + @($public.Headers) + @($public.Objects) +
        @($files | ForEach-Object { [pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash} })
    foreach ($record in $inputs) {
        if ((Get-FileHash -LiteralPath $record.Path).Hash -ne $record.Sha256) { throw "Stale core input $($record.Path)" }
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
            $stdout = $process.StandardOutput.ReadToEndAsync(); $stderr = $process.StandardError.ReadToEndAsync()
            if (!$process.WaitForExit(45000)) { Stop-XlirProcessTree $process; throw "Timeout $Name" }
            $code = $process.ExitCode
            [IO.File]::WriteAllText("$outPath/$Name.log", $stdout.Result + $stderr.Result, [Text.UTF8Encoding]::new($false))
        } finally { $process.Dispose() }
        $ok = if ($Reject) { $code -ne 0 } else { $code -eq 0 }
        $rows.Add([pscustomobject]@{Name=$Name;Exit=$code;Complete=$ok})
        if (!$ok) { Get-Content "$outPath/$Name.log" -Tail 25; throw "$Name failed: $code" }
    }
    & "$PSScriptRoot/check_xlir_artifact_runtime.ps1" -PublicDirectory $PublicDirectory -Directory "$outPath/artifact" -Cases ($cases + $extraCases) *> "$outPath/artifact.log"
    if (!$?) { throw 'Stack actual artifact acceptance failed' }
    $artifact = Get-Content "$outPath/artifact/results.json" -Raw | ConvertFrom-Json
    if (!$artifact.Complete -or !$artifact.InputsUnchanged) { throw 'Incomplete Stack artifact evidence' }
    foreach ($case in $cases) {
        $plan = $artifact.NativeLinkage | Where-Object Fixture -eq $case
        if ('src/runtime/xl6_stack.c' -notin $plan.Sources -or 'xrtTypedStackCreate' -notin $plan.Defined -or
            'xrtTypedStackCreate' -notin $plan.Required) { throw 'Missing actual native typed Stack closure' }
        if (@($plan.Sources | Where-Object { $_ -match 'xl6_buffer_|xl6_regex_|xl6_log_|xl6_process_' }).Count) { throw 'Unrelated resident component in Stack' }
    }
    foreach ($case in ($cases + $extraCases)) {
        $plan = $artifact.NativeLinkage | Where-Object Fixture -eq $case
        $expected = if ($case -eq 'stdlib_stack_nullable_scalar') { @('bool','i64','u64','f32','f64','time','char') }
            elseif ($case -eq 'stdlib_stack_nullable_continuation' -or $case -eq 'stdlib_nullable_resource_transport') { @('i64') }
            elseif ($case -match '^stdlib_stack_nullable_(bool|i64|u64|f32|f64|time|char)$') { @($Matches[1]) }
            else { @() }
        $selected = @($plan.Sources | Where-Object { $_ -match '^src/runtime/xl6_nullable_' } | ForEach-Object {
            $_ -replace '^src/runtime/xl6_nullable_|\.c$', '' })
        if ((($expected | Sort-Object) -join ',') -cne (($selected | Sort-Object) -join ',')) { throw "Wrong nullable native profile closure: $case" }
    }
    Copy-Item -LiteralPath release/xl.dll -Destination "$outPath/xl.dll"
    $objects = @($public.Objects | Where-Object { [IO.Path]::GetFileName($_.Path) -ne 'xlir_public_source_driver.o' } | ForEach-Object Path)
    $strict = @('-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-label',
        '-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-const-variable','-Wno-unused-but-set-variable',
        '-Wno-missing-field-initializers','-Wno-pointer-sign','-Wno-format-truncation',
        '-I.','-Irelease','-Irelease/include',"-I$PublicDirectory/include")
    Run 'native-build' $gcc ($strict + @('test/cases/stdlib_stack_native_host.c','test/support/stack_native.c','src/runtime/xl6_stack.c','-o',"$outPath/native.exe"))
    Run 'native-vectors' "$outPath/native.exe" @()
    Run 'nullable-native-build' $gcc ($strict + @('-DXL_STATIC_EMBED','-Ilib/tcc','test/cases/xlir_nullable_scalar_native.c') +
        $objects + @('release/xl.dll','-lWs2_32','-lIPHLPAPI','-lShell32','-lOle32','-lAdvapi32','-o',"$outPath/nullable-native.exe"))
    Run 'nullable-native-vectors' "$outPath/nullable-native.exe" @()
    $sweeps = @()
    foreach ($operation in @('create','reserve','push','peek','clone','trim','owned-peek','try-peek')) {
        for ($point=0; $point -le 64; ++$point) {
            $name = "native.$operation.oom-$point"
            Run $name "$outPath/native.exe" @($operation,"$point")
            if ((Get-Content "$outPath/$name.log" -Raw) -match 'hit=0 balanced') { break }
        }
        if ($point -gt 64) { throw "Incomplete Stack native prefix: $operation" }
        $sweeps += [pscustomobject]@{Fixture=$operation;FirstNoHit=$point}
    }
    Run 'contract-build' $gcc ($strict + @('-DXL_STATIC_EMBED','-Ilib/tcc','test/cases/xlir_stack_native_contract.c') +
        $objects + @('release/xl.dll','-lWs2_32','-lIPHLPAPI','-lShell32','-lOle32','-lAdvapi32','-o',"$outPath/contract.exe"))
    foreach ($case in $cases) {
        $prefix = "$outPath/$case"
        Run "$case.emit" $public.Driver @("test/cases/$case.xl",'emit',$prefix)
        Run "$case.contract" "$outPath/contract.exe" @("test/cases/$case.xl","$prefix.contract.c")
        if ((Get-Content "$prefix.c" -Raw) -cne (Get-Content "$prefix.contract.c" -Raw)) { throw 'Noncanonical Stack C output' }
        if ((Get-Content "$prefix.c" -Raw) -match 'xlir_stack_(handle|payload|create|push|clone|raise|reserve|pop|peek|equals|trim|clear|size)') { throw 'Retired generated Stack implementation emitted' }
    }
    foreach($case in $negative){
        $prefix="$outPath/negative.$case"
        Run "negative.$case" $public.Driver @("test/fixtures/stack_value_contract/$case.xl",'emit',$prefix) $true
        $diagnostic = switch ($case) {
            'owned_argument' { '\[sem\].*\[SC-ARG-COUNT\]' }
            'owned_depth' { '\[sem\].*Stack\.peekAt.*: target int, value string' }
            'owned_result' { '\[sem\].*: target string, value bytes' }
            'try_argument' { '\[sem\].*\[SC-ARG-COUNT\]' }
            'try_depth' { '\[sem\].*Stack\.peekAt.*: target int, value string' }
            'try_result' { '\[sem\].*: target nullable<bytes>, value nullable' }
            'try_nullable_result' { '\[sem\].*: target string, value nullable' }
            'nullable_literal_range' { '\[sem\].*Stack.push.*target nullable<int32>, value int' }
            'nullable_variable_narrow' { '\[sem\].*Stack.push.*target nullable<int32>, value int' }
            'nullable_identity' { '\[sem\].*Stack.push.*target nullable<char>' }
            default { '\[sem\].*Stack.*: target (char|bytes), value (uint32|var|string)' }
        }
        if((Get-Content "$prefix.log" -Raw) -notmatch $diagnostic -or
            (Test-Path "$prefix.c") -or (Test-Path "$prefix.h")){throw 'Wrong Stack type rejection or partial output'}
    }
    Run 'debug-host-build' $gcc ($strict + @('-c','test/support/module_product_memory.c','-o',"$outPath/debug-host.o"))
    foreach($case in @('stdlib_stack_native','stdlib_stack_values','stdlib_stack_owned','stdlib_stack_try_owned','stdlib_stack_nullable_scalar')){
    $prefix = "$outPath/$case"
    Run "$case.language-oom-build" $gcc ($strict + @('-DXL6_PRODUCT_USER_ENTRY','-include',"$prefix.c",
        'test/cases/xlir_module_product_oom_host.c',"$outPath/debug-host.o") +
        $objects + @('release/xl.dll','-lWs2_32','-lIPHLPAPI','-lShell32','-lOle32','-lAdvapi32','-o',"$prefix.host.exe"))
    Run "$case.language-oom-normal" "$prefix.host.exe" @()
    for ($point=0; $point -le 4096; ++$point) {
        $name = "$case.language.oom-$point"
        Run $name "$prefix.host.exe" @("$point")
        if ((Get-Content "$outPath/$name.log" -Raw) -match 'hit=0 balanced') { break }
    }
    if ($point -gt 4096) { throw 'Incomplete Stack language allocation prefix' }
    $sweeps += [pscustomobject]@{Fixture=$case;FirstNoHit=$point}
    }
    $unchanged = !@($inputs | Where-Object { (Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256 }).Count
    $report = [pscustomobject]@{Complete=$unchanged;InputsUnchanged=$unchanged;Checks=$rows.Count;Inputs=$inputs;
        PositiveFixtures=$cases;ExtraFixtures=$extraCases;NegativeFixtures=$negative;Sweeps=$sweeps;Results=$rows;
        Scope='Windows actual strict C/JIT/AOT/pruned Stack artifacts: owned/try and seven tagged nullable stored profiles, exact carrier identity, padding-independent compare/hash, literal fit/widening, parameter isolation, continuation/finally lifetime and pre-output mutation refusal. Nullable values/raw queue exclude descriptors; AVL selects only its used descriptor. Native and calling-thread language OOM prefixes. Legacy Into retirement, batch and other 53-module acceptance remain pending.'}
    [IO.File]::WriteAllText("$outPath/results.json", ($report | ConvertTo-Json -Depth 8), [Text.UTF8Encoding]::new($false))
    if (!$unchanged) { throw 'Stack inputs changed during acceptance' }
    Write-Host "Stack native Windows: $($rows.Count) checks"
} finally { Pop-Location }
