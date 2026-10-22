param([Parameter(Mandatory=$true)][string]$PublicDirectory,
    [Parameter(Mandatory=$true)][string]$ContractDriver,
    [Parameter(Mandatory=$true)][string]$Directory)
$ErrorActionPreference='Stop'
$rootPath=Split-Path -Parent $PSScriptRoot
Push-Location $rootPath
try {
    $outputPath=[IO.Path]::GetFullPath($Directory)
    if (Test-Path -LiteralPath $outputPath) { throw 'Keep prior publication evidence; choose a fresh directory' }
    $build=Get-Content (Join-Path $PublicDirectory 'build.json') -Raw | ConvertFrom-Json
    $contracts=Get-Content (Join-Path (Split-Path -Parent $ContractDriver) 'build.json') -Raw | ConvertFrom-Json
    $runtime=@($build.Objects | Where-Object {[IO.Path]::GetFileName($_.Path) -eq 'xrt_single_header_impl.o'})
    if (!$build.SourceXrt -or $runtime.Count -ne 1) { throw 'A current source runtime is required' }
    $files=@(Get-Item $PSCommandPath,src/runtime/xl6_code_lease.c,src/runtime/xl6_code_lease.h,
        src/runtime/xl6_slot.c,src/runtime/xl6_frame_storage.c,src/runtime/xl6_continuation_frame.c,src/runtime/xl6_future_wait.c,src/runtime/xl6_task.c,src/runtime/xl6_task_frame.c,src/runtime/xl6_task_pool.c,src/runtime/xl6_capture.c,src/runtime/xl6_value_iterator.c,src/runtime/xl6_future_callback.c,src/runtime/xl6_generator.c,src/runtime/xl6_slot.h,
        test/cases/xlir_code_scope_contract.c,test/cases/xlir_code_scope_drop.c,test/cases/xlir_code_scope_drop.h,
        test/cases/xlir_class_publication_outcome_contract.c,test/cases/xlir_class_publication_outcome.xl,
        test/cases/xlir_class_publication_outcome_native.c,test/cases/xlir_class_publication_outcome_host.c)
    $inputs=@($build.Inputs)+@($build.Headers)+@($runtime)+@($contracts.Inputs)+@($contracts.Drivers)+
        @($files | ForEach-Object {[pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}})
    if (@($inputs | Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256}).Count) { throw 'Stale publication inputs' }
    New-Item -ItemType Directory -Path $outputPath | Out-Null
    $prefix=Join-Path $outputPath 'publication'
    & $ContractDriver "$prefix.c" "$prefix.h" *> "$outputPath/ir.log"
    $irExit=$LASTEXITCODE
    $irComplete=[bool](Select-String -LiteralPath "$outputPath/ir.log" -SimpleMatch 'Class publication: 5 checked consuming bindings, 30 rejected C/header-gated mutations' -Quiet)
    $includePath=[IO.Path]::GetFullPath((Join-Path $PublicDirectory 'include'))
    $strict=@('-std=c11','-Wall','-Wextra','-Werror','-I.',"-I$includePath",'-Wno-unused-function',
        '-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label','-Wno-unused-but-set-variable',
        '-Wno-unused-const-variable','-Wno-missing-field-initializers','-Wno-pointer-sign','-Wno-format-truncation')
    $libs=@('-lWs2_32','-lIPHLPAPI','-lShell32','-lOle32','-lAdvapi32')
    $lanes=@()
    foreach ($level in @(0,2)) {
        $dll="$outputPath/scope-O$level.dll"
        & gcc @strict "-O$level" -shared test/cases/xlir_code_scope_drop.c -o $dll *> "$outputPath/dll-O$level.log"
        if ($LASTEXITCODE -ne 0) { throw 'Code scope DLL build failed' }
        foreach ($name in @('scope','publication')) {
            $lane="$outputPath/$name-O$level"
            $sources=if ($name -eq 'scope') { @('test/cases/xlir_code_scope_contract.c','src/runtime/xl6_code_lease.c') }
                else { @("$prefix.c",'test/cases/xlir_class_publication_outcome_native.c','test/cases/xlir_class_publication_outcome_host.c',
                    'src/runtime/xl6_code_lease.c','src/runtime/xl6_code_finalizer.c','src/runtime/xl6_slot.c','src/runtime/xl6_frame_storage.c','src/runtime/xl6_continuation_frame.c','src/runtime/xl6_future_wait.c','src/runtime/xl6_task.c','src/runtime/xl6_task_frame.c','src/runtime/xl6_task_pool.c','src/runtime/xl6_capture.c','src/runtime/xl6_value_iterator.c','src/runtime/xl6_future_callback.c','src/runtime/xl6_generator.c',
                    'src/runtime/xl6_future_payload.c','src/runtime/xl6_closure.c','src/runtime/xl6_cell.c','src/runtime/xl6_continuation_queue.c') }
            & gcc @strict "-O$level" @sources $runtime[0].Path @libs -o "$lane.exe" *> "$lane.build.log"
            $buildExit=$LASTEXITCODE; $runExit=$null; $complete=$false
            if ($buildExit -eq 0) {
                $arguments=if ($name -eq 'scope') {@('"'+$dll+'"')} else {@()}
                $options=@{FilePath="$lane.exe";WindowStyle='Hidden';PassThru=$true;RedirectStandardOutput="$lane.out.log";RedirectStandardError="$lane.err.log"}
                if ($arguments.Count) { $options.ArgumentList=$arguments }
$process=Start-Process @options
                $null = $process.Handle
                if (!$process.WaitForExit(60000)) { $process.Kill(); $process.WaitForExit(); $runExit=-999 }
                else { $process.Refresh(); $runExit=$process.ExitCode }
                $process.Dispose()
                $marker=if ($name -eq 'scope') {'Code scopes: 50 real DLL return-before-unmap lifetimes, 25 cyclic graph root transitions, 100000 allocation-free native-thread inheritances, 100 cross-thread and 100 identity refusals'}
                    else {'Generated class publication: 100 ordinary successes, 100 native-cursor commits and 100 constructor failures after self publication, catch/local/child/finalizer and allocator balance'}
                $complete=[bool](Select-String -LiteralPath "$lane.out.log" -SimpleMatch $marker -Quiet)
            }
            $lanes+=[pscustomobject]@{Name="$name-O$level";Build=$buildExit;Run=$runExit;Complete=$complete}
            Write-Output "$name-O$level build=$buildExit run=$runExit complete=$complete"
        }
    }
    $language=@()
    foreach ($phase in @('emit','repeat','run','module')) {
        $mode=if ($phase -eq 'repeat') {'emit'} else {$phase}
        $target="$outputPath/language-$phase"
        & $build.Driver test/cases/xlir_class_publication_outcome.xl $mode $target test/cases/xlir_class_publication_outcome_native.c *> "$target.log"
        $code=$LASTEXITCODE
        $complete=[bool](Select-String -LiteralPath "$target.log" -SimpleMatch "public source $mode complete" -Quiet)
        $language+=[pscustomobject]@{Name=$phase;Exit=$code;Complete=$complete}
        Write-Output "publication $phase exit=$code complete=$complete"
    }
    $deterministic=$true
    foreach ($extension in @('c','h','json')) {
        $a="$outputPath/language-emit.$extension"; $b="$outputPath/language-repeat.$extension"
        if (!(Test-Path -LiteralPath $a) -or !(Test-Path -LiteralPath $b) -or (Get-FileHash $a).Hash -ne (Get-FileHash $b).Hash) { $deterministic=$false }
    }
    $unchanged=@($inputs | Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256}).Count -eq 0
    $report=[pscustomobject]@{RecordedAt=(Get-Date).ToString('o');Inputs=$inputs;InputsUnchanged=$unchanged;
        IrExit=$irExit;IrComplete=$irComplete;Lanes=$lanes;Language=$language;Deterministic=$deterministic;
        Scope='Prepared class construction commit including native-cursor early self publication, actual constructor failure and native callback residency scopes. Original automatic object/code-cycle gate remains independent; this gate does not establish module cycle collection, arbitrary generated method scopes or class deep-clone ownership.'}
    [IO.File]::WriteAllText("$outputPath/results.json",($report | ConvertTo-Json -Depth 7),[Text.UTF8Encoding]::new($false))
    if (!$unchanged -or !$deterministic -or $irExit -ne 0 -or !$irComplete -or
        @($lanes | Where-Object {$_.Build -ne 0 -or $_.Run -ne 0 -or !$_.Complete}).Count -or
        @($language | Where-Object {$_.Exit -ne 0 -or !$_.Complete}).Count) { throw 'Publication batch collected failures; inspect all lanes' }
} finally { Pop-Location }
