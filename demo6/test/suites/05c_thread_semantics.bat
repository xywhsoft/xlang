@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: thread semantics...

release\xl.exe check test\cases\sem_sync_ctor_args.xl >nul 2> build\test\sem_sync_ctor_args.err.txt
if not errorlevel 1 (
    echo sem_sync_ctor_args.xl should fail.
    exit /b 1
)
findstr /C:"sync constructor does not accept arguments" build\test\sem_sync_ctor_args.err.txt >nul
if errorlevel 1 (
    echo sem_sync_ctor_args.xl missing sync constructor diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_sync_ctor_named_argument.xl >nul 2> build\test\sem_sync_ctor_named_argument.err.txt
if not errorlevel 1 (
    echo sem_sync_ctor_named_argument.xl should fail.
    exit /b 1
)
findstr /C:"sync constructor accepts positional arguments only" build\test\sem_sync_ctor_named_argument.err.txt >nul
if errorlevel 1 (
    echo sem_sync_ctor_named_argument.xl missing sync constructor named argument diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_sync_method_args.xl >nul 2> build\test\sem_sync_method_args.err.txt
if not errorlevel 1 (
    echo sem_sync_method_args.xl should fail.
    exit /b 1
)
findstr /C:"sync method does not accept arguments" build\test\sem_sync_method_args.err.txt >nul
if errorlevel 1 (
    echo sem_sync_method_args.xl missing sync method argument diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_sync_wait_timeout_type.xl >nul 2> build\test\sem_sync_wait_timeout_type.err.txt
if not errorlevel 1 (
    echo sem_sync_wait_timeout_type.xl should fail.
    exit /b 1
)
findstr /C:"sync method argument must be int" build\test\sem_sync_wait_timeout_type.err.txt >nul
if errorlevel 1 (
    echo sem_sync_wait_timeout_type.xl missing sync timeout type diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_sync_post_multiple_type.xl >nul 2> build\test\sem_sync_post_multiple_type.err.txt
if not errorlevel 1 (
    echo sem_sync_post_multiple_type.xl should fail.
    exit /b 1
)
findstr /C:"sync method argument must be int" build\test\sem_sync_post_multiple_type.err.txt >nul
if errorlevel 1 (
    echo sem_sync_post_multiple_type.xl missing sync postMultiple type diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_sync_unknown_method.xl >nul 2> build\test\sem_sync_unknown_method.err.txt
if not errorlevel 1 (
    echo sem_sync_unknown_method.xl should fail.
    exit /b 1
)
findstr /C:"unknown sync method" build\test\sem_sync_unknown_method.err.txt >nul
if errorlevel 1 (
    echo sem_sync_unknown_method.xl missing unknown sync method diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_sync_receiver_expr.xl >nul 2> build\test\sem_sync_receiver_expr.err.txt
if not errorlevel 1 (
    echo sem_sync_receiver_expr.xl should fail.
    exit /b 1
)
findstr /C:"sync method receiver" build\test\sem_sync_receiver_expr.err.txt >nul
if errorlevel 1 (
    echo sem_sync_receiver_expr.xl missing named receiver diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_sync_cond_mutex_arg.xl >nul 2> build\test\sem_sync_cond_mutex_arg.err.txt
if not errorlevel 1 (
    echo sem_sync_cond_mutex_arg.xl should fail.
    exit /b 1
)
findstr /C:"cond wait first argument must be mutex" build\test\sem_sync_cond_mutex_arg.err.txt >nul
if errorlevel 1 (
    echo sem_sync_cond_mutex_arg.xl missing cond mutex diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_sync_cond_wait_timeout_type.xl >nul 2> build\test\sem_sync_cond_wait_timeout_type.err.txt
if not errorlevel 1 (
    echo sem_sync_cond_wait_timeout_type.xl should fail.
    exit /b 1
)
findstr /C:"cond wait timeout must be int" build\test\sem_sync_cond_wait_timeout_type.err.txt >nul
if errorlevel 1 (
    echo sem_sync_cond_wait_timeout_type.xl missing cond timeout type diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_sync_copy.xl >nul 2> build\test\sem_sync_copy.err.txt
if not errorlevel 1 (
    echo sem_sync_copy.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\cgen_thread.xl
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\cgen_thread.xl
if errorlevel 1 exit /b 1

release\xl.exe build test\cases\cgen_thread.xl -o build\test\cgen_thread.exe
if errorlevel 1 exit /b 1

build\test\cgen_thread.exe
if errorlevel 1 exit /b 1
for /L %%I in (1,1,3) do (
    build\test\cgen_thread.exe
    if errorlevel 1 (
        echo cgen_thread.exe repeat run %%I failed.
        exit /b 1
    )
)

release\xl.exe cgen test\cases\cgen_thread.xl --emit build\test\cgen_thread.c
if errorlevel 1 exit /b 1

findstr /C:"xrtThreadCreate" build\test\cgen_thread.c >nul
if errorlevel 1 (
    echo cgen_thread.c missing xrtThreadCreate.
    exit /b 1
)
findstr /C:"xl6_thread_start" build\test\cgen_thread.c >nul
if errorlevel 1 (
    echo cgen_thread.c missing thread start helper.
    exit /b 1
)
findstr /C:"xl6_thread_require_handle(xl6_thread_start" build\test\cgen_thread.c >nul
if errorlevel 1 (
    echo cgen_thread.c missing thread constructor error guard.
    exit /b 1
)
findstr /C:"if ( xl6_error_has() ) { return 0; }" build\test\cgen_thread.c >nul
if errorlevel 1 (
    echo cgen_thread.c missing thread active error preservation guard.
    exit /b 1
)
findstr /C:"thread requires a callable func value" build\test\cgen_thread.c >nul
if errorlevel 1 (
    echo cgen_thread.c missing empty function value thread diagnostic.
    exit /b 1
)
findstr /C:"XL6_Function missingWorker;" build\test\cgen_thread.c >nul
if errorlevel 1 (
    echo cgen_thread.c missing empty function value thread fixture.
    exit /b 1
)
findstr /C:"xl6_thread_start(missingWorker, 1" build\test\cgen_thread.c >nul
if errorlevel 1 (
    echo cgen_thread.c missing empty function value thread start call.
    exit /b 1
)
findstr /C:"thread start failed" build\test\cgen_thread.c >nul
if errorlevel 1 (
    echo cgen_thread.c missing thread start failure diagnostic.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_thread.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'static void\* xl6_thread_start\([\s\S]*?if \( fn\.invoke == 0 \) \{[\s\S]*?if \( !retainBorrow && fn\.release != 0 && fn\.env != 0 \) \{ fn\.release\(fn\.env\); \}[\s\S]*?thread requires a callable func value' -or $text -notmatch 'static void\* xl6_thread_start\([\s\S]*?if \( arg == 0 \) \{[\s\S]*?if \( fn\.release != 0 && fn\.env != 0 \) \{ fn\.release\(fn\.env\); \}[\s\S]*?out of memory while starting thread' -or $text -notmatch 'static void\* xl6_thread_start\([\s\S]*?if \( pThread == 0 \) \{[\s\S]*?if \( fn\.release != 0 && fn\.env != 0 \) \{ fn\.release\(fn\.env\); \}[\s\S]*?xrtFree\(arg\);[\s\S]*?thread start failed'){ exit 1 }"
if errorlevel 1 (
    echo cgen_thread.c missing thread start failure cleanup.
    exit /b 1
)
findstr /C:"xl6_thread_release" build\test\cgen_thread.c >nul
if errorlevel 1 (
    echo cgen_thread.c missing thread release helper.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_thread.c',[System.Text.Encoding]::UTF8); $checks=@('slow = xl6_thread_require_handle(xl6_thread_start(xl6_fnvalue_xl_fn_slowWorker_make(), 0','slowImmediate = (xrtThreadWaitTimeout(slow, 0LL) == 0)','!slowImmediate','slowWaited = (xrtThreadWaitTimeout(slow, 2000LL) == 0)','slowCode = (int)xrtThreadGetExitCode(slow)','slowRepeatedWait = (xrtThreadWait(slow), true)','slowCodeAgain = (int)xrtThreadGetExitCode(slow)'); foreach($p in $checks){ if($text.IndexOf($p) -lt 0){ Write-Host ('cgen_thread.c missing '+$p); exit 1 } }"
if errorlevel 1 (
    echo cgen_thread.c missing waitTimeout timeout/completed-state guard lowering.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_thread.c',[System.Text.Encoding]::UTF8); $checks=@('conditionalBorrowedThread = xl6_thread_require_handle(xl6_thread_start((chooseBorrowedThread ? borrowedThreadLeft : borrowedThreadRight), 1','conditionalBorrowedThreadAgain = xl6_thread_require_handle(xl6_thread_start((chooseBorrowedThread ? borrowedThreadLeft : borrowedThreadRight), 1','mixedThreadOwned = xl6_thread_require_handle(xl6_thread_start((chooseMixedThread ? xl6_function_copy_value(mixedThreadSource) : xl_fn_makeThreadWorker(16LL)), 0','mixedThreadBorrowed = xl6_thread_require_handle(xl6_thread_start((chooseMixedThread ? xl6_function_copy_value(mixedThreadSource) : xl_fn_makeThreadWorker(17LL)), 0','conditionalMemberThread = xl6_thread_require_handle(xl6_thread_start((chooseThreadMember ? threadBox.op : threadBoxAlt.op), 1','borrowedThreadRight.signature ==','borrowedThreadLeft.signature ==','mixedThreadSource.signature ==','xl6_function_call_var(threadBoxAlt.op, 0)'); foreach($p in $checks){ if($text.IndexOf($p) -lt 0){ Write-Host ('cgen_thread.c missing '+$p); exit 1 } }"
if errorlevel 1 (
    echo cgen_thread.c missing borrowed conditional thread callable ownership guard.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_thread.c',[System.Text.Encoding]::UTF8); $checks=@('capturedReceiverThread = xl6_thread_require_handle(xl6_thread_start(xl6_fnvalue_xl_fn_worker_make(), 0','void* capturedReceiverThread;','xrtThreadWait(capturedReceiverThread), true','(int)xrtThreadGetExitCode(capturedReceiverThread)','threadJoiner = xl6_anon_','_make()'); foreach($p in $checks){ if($text.IndexOf($p) -lt 0){ Write-Host ('cgen_thread.c missing '+$p); exit 1 } }"
if errorlevel 1 (
    echo cgen_thread.c missing captured thread receiver guard lowering.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_thread.c',[System.Text.Encoding]::UTF8); $checks=@('capturedTimedThread = xl6_thread_require_handle(xl6_thread_start(xl6_fnvalue_xl_fn_monitorSlowWorker_make(), 0','capturedStopThread = xl6_thread_require_handle(xl6_thread_start(xl6_fnvalue_xl_fn_slowWorker_make(), 0','_make()','xrtThreadWaitTimeout(capturedTimedThread, 0LL) == 0','xrtThreadGetState(capturedTimedThread)','xrtThreadWaitTimeout(capturedTimedThread, 2000LL) == 0','(int)xrtThreadGetExitCode(capturedTimedThread)','xrtThreadStop(capturedStopThread), true','xrtThreadWait(capturedStopThread), true'); foreach($p in $checks){ if($text.IndexOf($p) -lt 0){ Write-Host ('cgen_thread.c missing '+$p); exit 1 } }"
if errorlevel 1 (
    echo cgen_thread.c missing captured thread waitTimeout/state/stop guard lowering.
    exit /b 1
)

release\xl.exe check test\cases\cgen_thread_default.xl
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\cgen_thread_default.xl
if errorlevel 1 exit /b 1

release\xl.exe build test\cases\cgen_thread_default.xl -o build\test\cgen_thread_default.exe
if errorlevel 1 exit /b 1

build\test\cgen_thread_default.exe
if errorlevel 1 exit /b 1
for /L %%I in (1,1,5) do (
    build\test\cgen_thread_default.exe
    if errorlevel 1 (
        echo cgen_thread_default.exe repeat run %%I failed.
        exit /b 1
    )
)

release\xl.exe cgen test\cases\cgen_thread_default.xl --emit build\test\cgen_thread_default.c
if errorlevel 1 exit /b 1

findstr /C:"xl6_thread_release" build\test\cgen_thread_default.c >nul
if errorlevel 1 (
    echo cgen_thread_default.c missing thread release helper.
    exit /b 1
)

release\xl.exe check test\cases\sem_thread_ctor_args.xl >nul 2> build\test\sem_thread_ctor_args.err.txt
if not errorlevel 1 (
    echo sem_thread_ctor_args.xl should fail.
    exit /b 1
)
findstr /C:"thread constructor requires one func() as int entry argument" build\test\sem_thread_ctor_args.err.txt >nul
if errorlevel 1 (
    echo sem_thread_ctor_args.xl missing thread constructor diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_thread_ctor_named_argument.xl >nul 2> build\test\sem_thread_ctor_named_argument.err.txt
if not errorlevel 1 (
    echo sem_thread_ctor_named_argument.xl should fail.
    exit /b 1
)
findstr /C:"thread constructor accepts positional arguments only" build\test\sem_thread_ctor_named_argument.err.txt >nul
if errorlevel 1 (
    echo sem_thread_ctor_named_argument.xl missing thread constructor named argument diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_thread_entry_signature.xl >nul 2> build\test\sem_thread_entry_signature.err.txt
if not errorlevel 1 (
    echo sem_thread_entry_signature.xl should fail.
    exit /b 1
)
findstr /C:"thread entry must match func() as int" build\test\sem_thread_entry_signature.err.txt >nul
if errorlevel 1 (
    echo sem_thread_entry_signature.xl missing thread entry signature diagnostic.
    exit /b 1
)
findstr /C:"function " build\test\sem_thread_entry_signature.err.txt >nul
if not errorlevel 1 (
    echo sem_thread_entry_signature.xl should not emit generic function value diagnostics.
    exit /b 1
)

release\xl.exe check test\cases\sem_thread_method_args.xl >nul 2> build\test\sem_thread_method_args.err.txt
if not errorlevel 1 (
    echo sem_thread_method_args.xl should fail.
    exit /b 1
)
findstr /C:"thread method does not accept arguments" build\test\sem_thread_method_args.err.txt >nul
if errorlevel 1 (
    echo sem_thread_method_args.xl missing thread method argument diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_thread_wait_timeout_type.xl >nul 2> build\test\sem_thread_wait_timeout_type.err.txt
if not errorlevel 1 (
    echo sem_thread_wait_timeout_type.xl should fail.
    exit /b 1
)
findstr /C:"thread.waitTimeout argument must be int" build\test\sem_thread_wait_timeout_type.err.txt >nul
if errorlevel 1 (
    echo sem_thread_wait_timeout_type.xl missing thread waitTimeout type diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_thread_unknown_method.xl >nul 2> build\test\sem_thread_unknown_method.err.txt
if not errorlevel 1 (
    echo sem_thread_unknown_method.xl should fail.
    exit /b 1
)
findstr /C:"unknown thread method" build\test\sem_thread_unknown_method.err.txt >nul
if errorlevel 1 (
    echo sem_thread_unknown_method.xl missing unknown thread method diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_thread_receiver_expr.xl >nul 2> build\test\sem_thread_receiver_expr.err.txt
if not errorlevel 1 (
    echo sem_thread_receiver_expr.xl should fail.
    exit /b 1
)
findstr /C:"thread method receiver" build\test\sem_thread_receiver_expr.err.txt >nul
if errorlevel 1 (
    echo sem_thread_receiver_expr.xl missing named receiver diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_thread_copy.xl >nul 2> build\test\sem_thread_copy.err.txt
if not errorlevel 1 (
    echo sem_thread_copy.xl should fail.
    exit /b 1
)


echo DEMO6 suite ok: thread semantics
exit /b 0

