param([Parameter(Mandatory=$true)][string]$PublicDirectory,
    [Parameter(Mandatory=$true)][string]$Directory)
$ErrorActionPreference='Stop'
$rootPath=Split-Path -Parent $PSScriptRoot
Push-Location $rootPath
try {
    $outputPath=[IO.Path]::GetFullPath((Join-Path $rootPath $Directory))
    if (Test-Path -LiteralPath $outputPath) { throw 'Keep prior evidence; choose a fresh directory' }
    New-Item -ItemType Directory -Path $outputPath | Out-Null
    $build=Get-Content (Join-Path $PublicDirectory 'build.json') -Raw | ConvertFrom-Json
    if (!$build.SourceXrt) { throw 'A current source runtime is required' }
    foreach ($input in $build.Inputs) { if ((Get-FileHash -LiteralPath $input.Path).Hash -ne $input.Sha256) { throw "Source build drift: $($input.Path)" } }
    $runtime=@($build.Objects | Where-Object { [IO.Path]::GetFileName($_.Path) -eq 'xrt_single_header_impl.o' })
    if ($runtime.Count -ne 1 -or (Get-FileHash -LiteralPath $runtime[0].Path).Hash -ne $runtime[0].Sha256) { throw 'Source runtime object mismatch' }
    $files=@(Get-Item src/runtime/xl6_code_owner.h,src/runtime/xl6_code_lease.h,src/runtime/xl6_code_lease.c,src/runtime/xl6_code_finalizer.c,
        test/cases/xlir_code_finalizer_drop.h,test/cases/xlir_code_finalizer_drop.c,test/cases/xlir_code_finalizer_contract.c,
        test/cases/xlir_class_finalizer_cursor.xl,test/cases/xlir_class_finalizer_cursor_native.c,
        lib/xrt/xrt.h,lib/xruntime/xruntime.h,$PSCommandPath)
    $inputs=@($files | ForEach-Object { [pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash} })
    $flags=@('-std=c11','-Wall','-Wextra','-Werror','-I.')
    $dll=Join-Path $outputPath 'xlir_code_finalizer_drop.dll'
    & gcc @flags -O0 -shared test/cases/xlir_code_finalizer_drop.c -o $dll *> "$outputPath/dll.log"
    if ($LASTEXITCODE -ne 0) { throw 'Unloadable callback DLL build failed' }
    $lanes=@()
    foreach ($level in @(0,2)) {
        $prefix=Join-Path $outputPath "O$level"
        & gcc @flags "-O$level" test/cases/xlir_code_finalizer_contract.c src/runtime/xl6_code_finalizer.c src/runtime/xl6_code_lease.c `
            $runtime[0].Path -lWs2_32 -lIPHLPAPI -lShell32 -lOle32 -lAdvapi32 -o "$prefix.exe" *> "$prefix.build.log"
        if ($LASTEXITCODE -ne 0) { Get-Content "$prefix.build.log" -Tail 20; throw 'Resident finalizer host build failed' }
$process=Start-Process -FilePath "$prefix.exe" -ArgumentList @('"'+$dll+'"') -WindowStyle Hidden -PassThru -RedirectStandardOutput "$prefix.out.log" -RedirectStandardError "$prefix.err.log"
        $null = $process.Handle
        if (!$process.WaitForExit(60000)) { $process.Kill(); throw 'Finalizer host timeout' }
        $process.Refresh(); $code=$process.ExitCode
        $complete=[bool](Select-String -LiteralPath "$prefix.out.log" -SimpleMatch 'Resident object finalizer: 250 real DLL lifetimes, 500 finalizers/contexts, 250 field drops, 25 allocation failures, 25 publication refusals, 4900 graph probes,' -Quiet)
        $lanes+=[pscustomobject]@{Name="O$level";Exit=$code;Complete=$complete}
        Get-Content "$prefix.out.log"; if ($code -ne 0) { Get-Content "$prefix.err.log" }
    }
    # Same-source public default backend. The module entry remains active;
    # these are language finalizer/cursor semantics, NOT automatic module pins.
    $language=@()
    foreach ($phase in @('emit','repeat','run','module')) {
        $mode=if ($phase -eq 'repeat') {'emit'} else {$phase}
        $prefix=Join-Path $outputPath "language-$phase"
        $arguments=@('test/cases/xlir_class_finalizer_cursor.xl',$mode,$prefix,'test/cases/xlir_class_finalizer_cursor_native.c')
$process=Start-Process -FilePath $build.Driver -ArgumentList $arguments -WindowStyle Hidden -PassThru -RedirectStandardOutput "$prefix.log" -RedirectStandardError "$prefix.err.log"
        $null = $process.Handle
        if (!$process.WaitForExit(60000)) { $process.Kill(); throw 'Language cursor finalizer timeout' }
        $process.Refresh(); $code=$process.ExitCode
        $complete=[bool](Select-String -LiteralPath "$prefix.log" -SimpleMatch "public source $mode complete" -Quiet)
        $language+=[pscustomobject]@{Phase=$phase;Exit=$code;Complete=$complete}
        Write-Output "Class/native cursor $phase exit=$code"
        if ($code -ne 0) { Get-Content "$prefix.err.log" }
    }
    $deterministic=$true
    foreach ($extension in @('c','h','json')) {
        $a=Join-Path $outputPath "language-emit.$extension"; $b=Join-Path $outputPath "language-repeat.$extension"
        if (!(Test-Path -LiteralPath $a) -or !(Test-Path -LiteralPath $b) -or (Get-FileHash -LiteralPath $a).Hash -ne (Get-FileHash -LiteralPath $b).Hash) { $deterministic=$false }
    }
    $unchanged=@($inputs | Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256}).Count -eq 0
    $artifacts=@(Get-ChildItem -LiteralPath $outputPath -File | Where-Object {$_.Extension -in @('.exe','.dll')} | ForEach-Object {
        [pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}
    })
    $report=[pscustomobject]@{RecordedAt=(Get-Date).ToString('o');Inputs=$inputs;InputsUnchanged=$unchanged;SourceBuild=(Join-Path $PublicDirectory 'build.json');Runtime=$runtime[0];Lanes=$lanes;Artifacts=$artifacts;Language=$language;Deterministic=$deterministic;
        Scope='Resident finalizer/context release with explicit inherited CodeLease, actual unloadable callbacks and fields, native Retain/COW/cursor aliases, child extraction, callback-created cursor, exact graph and failure/diagnostic balance. No automatic language binding, module-cycle collection or production safepoint admission.'}
    [IO.File]::WriteAllText("$outputPath/results.json",($report | ConvertTo-Json -Depth 6),[Text.UTF8Encoding]::new($false))
    if (!$unchanged -or !$deterministic -or @($language | Where-Object {$_.Exit -ne 0 -or !$_.Complete}).Count -or
        @($lanes | Where-Object {$_.Exit -ne 0 -or !$_.Complete}).Count) { throw 'Resident finalizer batch failed' }
} finally { Pop-Location }
