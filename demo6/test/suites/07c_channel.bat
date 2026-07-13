@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: channel...

release\xl.exe run test\cases\cgen_channel.xl
if errorlevel 1 exit /b 1
if not errorlevel 0 exit /b 1

release\xl.exe run test\cases\cgen_module_global_closure.xl
if errorlevel 1 exit /b 1
if not errorlevel 0 exit /b 1

release\xl.exe build test\cases\cgen_module_global_closure.xl -o build\test\cgen_module_global_closure.exe
if errorlevel 1 exit /b 1

build\test\cgen_module_global_closure.exe
if errorlevel 1 exit /b 1
if not errorlevel 0 exit /b 1

release\xl.exe build test\cases\cgen_channel.xl -o build\test\cgen_channel.exe
if errorlevel 1 exit /b 1

build\test\cgen_channel.exe
if errorlevel 1 exit /b 1
if not errorlevel 0 exit /b 1
for /L %%I in (1,1,5) do (
    build\test\cgen_channel.exe
    if errorlevel 1 (
        echo cgen_channel.exe repeat run %%I failed.
        exit /b 1
    )
    if not errorlevel 0 (
        echo cgen_channel.exe repeat run %%I crashed.
        exit /b 1
    )
)

release\xl.exe cgen test\cases\cgen_channel.xl --emit build\test\cgen_channel.c
if errorlevel 1 exit /b 1

findstr /C:"xrtChannelCreate" build\test\cgen_channel.c >nul
if errorlevel 1 (
    echo cgen_channel.c missing xrtChannelCreate.
    exit /b 1
)
findstr /C:"xl6_channel_require_handle(xrtChannelCreate" build\test\cgen_channel.c >nul
if errorlevel 1 (
    echo cgen_channel.c missing channel constructor error guard.
    exit /b 1
)
findstr /C:"xl6_channel_send_value" build\test\cgen_channel.c >nul
if errorlevel 1 (
    echo cgen_channel.c missing send helper.
    exit /b 1
)
findstr /C:"xl6_channel_recv_into" build\test\cgen_channel.c >nul
if errorlevel 1 (
    echo cgen_channel.c missing recv helper.
    exit /b 1
)
findstr /C:"xrtChannelTryRecv" build\test\cgen_channel.c >nul
if errorlevel 1 (
    echo cgen_channel.c missing try recv path.
    exit /b 1
)
findstr /C:"12:34:56" build\test\cgen_channel.c >nul
if errorlevel 1 (
    echo cgen_channel.c missing time channel literal lowering.
    exit /b 1
)
findstr /C:"xl6_channel_send_value(closedCh" build\test\cgen_channel.c >nul
if errorlevel 1 (
    echo cgen_channel.c missing closed channel send assertion.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_channel.c',[System.Text.Encoding]::UTF8); $checks=@('defaultCh = xl6_channel_require_handle(xrtChannelCreate(0)','xrtChannelCount(defaultCh) == 2LL','xl6_channel_recv_into(defaultCh, &defaultFirst, sizeof(long long), 0','xrtChannelClose(defaultCh)','xrtChannelIsClosed(defaultCh)','xl6_channel_recv_into(defaultCh, &defaultSecond, sizeof(long long), 1','!xl6_channel_recv_into(defaultCh, &defaultSecond, sizeof(long long), 0','!xl6_channel_send_value(defaultCh, &(long long){3LL}, sizeof(long long)','fullCh = xl6_channel_require_handle(xrtChannelCreate(1LL)','xl6_channel_send_value(fullCh, &(long long){10LL}, sizeof(long long)','xl6_channel_send_value(fullCh, &(long long){11LL}, sizeof(long long)','!xl6_channel_send_value(fullCh, &(long long){12LL}, sizeof(long long)'); foreach($p in $checks){ if($text.IndexOf($p) -lt 0){ Write-Host ('cgen_channel.c missing '+$p); exit 1 } }"
if errorlevel 1 (
    echo cgen_channel.c missing channel close-drain or capacity guard lowering.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_channel.c',[System.Text.Encoding]::UTF8); $checks=@('crossThreadCh = xl6_channel_require_handle(xrtChannelCreate(4LL)','xchannel crossThreadCh;','xl6_channel_send_value(crossThreadCh, &(long long){314LL}','xl6_channel_send_value(crossThreadCh, &(long long){159LL}','xrtChannelClose(crossThreadCh)','channelProducer = xl6_anon_','_make()','producerThread = xl6_thread_require_handle(xl6_thread_start(channelProducer, 1','xrtThreadWaitTimeout(producerThread, 2000LL) == 0','(int)xrtThreadGetExitCode(producerThread) == 17LL','xrtChannelIsClosed(crossThreadCh)','xl6_channel_recv_into(crossThreadCh, &crossFirst, sizeof(long long), 1','xl6_channel_recv_into(crossThreadCh, &crossSecond, sizeof(long long), 1','!xl6_channel_recv_into(crossThreadCh, &crossSecond, sizeof(long long), 0','!xl6_channel_send_value(crossThreadCh, &(long long){265LL}, sizeof(long long)'); foreach($p in $checks){ if($text.IndexOf($p) -lt 0){ Write-Host ('cgen_channel.c missing '+$p); exit 1 } }"
if errorlevel 1 (
    echo cgen_channel.c missing cross-thread captured channel guard lowering.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_channel.c',[System.Text.Encoding]::UTF8); $checks=@('delayedRecvCh = xl6_channel_require_handle(xrtChannelCreate(1LL)','xl6_channel_send_value(delayedRecvCh, &(long long){271LL}','xl6_channel_recv_into(delayedRecvCh, &delayedRecvValue, sizeof(long long), 1','xrtThreadWaitTimeout(delayedRecvThread, 2000LL) == 0','(int)xrtThreadGetExitCode(delayedRecvThread) == 19LL','delayedConsumerCh = xl6_channel_require_handle(xrtChannelCreate(1LL)','xl6_channel_recv_into(delayedConsumerCh, &got, sizeof(long long), 1','xl6_channel_send_value(delayedConsumerCh, &(long long){2718LL}','xrtThreadWaitTimeout(delayedConsumerThread, 2000LL) == 0','(int)xrtThreadGetExitCode(delayedConsumerThread) == 2718LL'); foreach($p in $checks){ if($text.IndexOf($p) -lt 0){ Write-Host ('cgen_channel.c missing '+$p); exit 1 } }"
if errorlevel 1 (
    echo cgen_channel.c missing blocking channel wakeup guard lowering.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_channel.c',[System.Text.Encoding]::UTF8); $checks=@('nonblockingTryThreadCh = xl6_channel_require_handle(xrtChannelCreate(1LL)','xl6_channel_recv_into(nonblockingTryThreadCh, &got, sizeof(long long), 0','(int)xrtThreadGetExitCode(nonblockingTryThread) == 31LL','xl6_channel_send_value(nonblockingTryThreadCh, &(long long){42LL}, sizeof(long long)','xl6_channel_recv_into(nonblockingTryThreadCh, &nonblockingTryLater, sizeof(long long), 1','xrtChannelClose(nonblockingTryThreadCh)'); foreach($p in $checks){ if($text.IndexOf($p) -lt 0){ Write-Host ('cgen_channel.c missing '+$p); exit 1 } }"
if errorlevel 1 (
    echo cgen_channel.c missing captured tryRecvInto nonblocking guard lowering.
    exit /b 1
)

release\xl.exe run test\cases\cgen_channel_string.xl
if errorlevel 1 exit /b 1
if not errorlevel 0 exit /b 1
release\xl.exe build test\cases\cgen_channel_string.xl -o build\test\cgen_channel_string.exe
if errorlevel 1 exit /b 1
for /L %%I in (1,1,5) do (
    build\test\cgen_channel_string.exe >nul
    if errorlevel 1 (
        echo cgen_channel_string.exe repeat run %%I failed.
        exit /b 1
    )
    if not errorlevel 0 (
        echo cgen_channel_string.exe repeat run %%I crashed.
        exit /b 1
    )
)
release\xl.exe cgen test\cases\cgen_channel_string.xl --emit build\test\cgen_channel_string.c
if errorlevel 1 exit /b 1
findstr /C:"xl6_channel_send_string" build\test\cgen_channel_string.c >nul
if errorlevel 1 (
    echo cgen_channel_string.c missing string send copy helper.
    exit /b 1
)
findstr /C:"xl6_channel_recv_string" build\test\cgen_channel_string.c >nul
if errorlevel 1 (
    echo cgen_channel_string.c missing string receive copy helper.
    exit /b 1
)
findstr /C:"xl6_channel_destroy(pendingCh)" build\test\cgen_channel_string.c >nul
if errorlevel 1 (
    echo cgen_channel_string.c missing buffered message cleanup.
    exit /b 1
)

release\xl.exe check test\cases\sem_channel_var_element.xl >nul 2> build\test\sem_channel_var_element.err.txt
if not errorlevel 1 (
    echo sem_channel_var_element.xl should fail.
    exit /b 1
)
findstr /C:"channel<T>" build\test\sem_channel_var_element.err.txt >nul
if errorlevel 1 (
    echo sem_channel_var_element.xl missing channel element diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_channel_generator_element.xl >nul 2> build\test\sem_channel_generator_element.err.txt
if not errorlevel 1 (
    echo sem_channel_generator_element.xl should fail.
    exit /b 1
)
findstr /C:"channel<T>" build\test\sem_channel_generator_element.err.txt >nul
if errorlevel 1 (
    echo sem_channel_generator_element.xl missing channel element diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_channel_typed_array_element.xl >nul 2> build\test\sem_channel_typed_array_element.err.txt
if not errorlevel 1 (
    echo sem_channel_typed_array_element.xl should fail.
    exit /b 1
)
findstr /C:"channel<T>" build\test\sem_channel_typed_array_element.err.txt >nul
if errorlevel 1 (
    echo sem_channel_typed_array_element.xl missing channel typed array element diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_channel_generic_arity.xl >nul 2> build\test\sem_channel_generic_arity.err.txt
if not errorlevel 1 (
    echo sem_channel_generic_arity.xl should fail.
    exit /b 1
)
findstr /C:"expected 1, actual 2" build\test\sem_channel_generic_arity.err.txt >nul
if errorlevel 1 (
    echo sem_channel_generic_arity.xl missing channel generic argument count context.
    exit /b 1
)

release\xl.exe check test\cases\sem_channel_ctor_arg_count_many.xl >nul 2> build\test\sem_channel_ctor_arg_count_many.err.txt
if not errorlevel 1 (
    echo sem_channel_ctor_arg_count_many.xl should fail.
    exit /b 1
)
findstr /L /C:"[sem] channel" build\test\sem_channel_ctor_arg_count_many.err.txt >nul
if errorlevel 1 (
    echo sem_channel_ctor_arg_count_many.xl missing channel constructor diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_channel_ctor_named_argument.xl >nul 2> build\test\sem_channel_ctor_named_argument.err.txt
if not errorlevel 1 (
    echo sem_channel_ctor_named_argument.xl should fail.
    exit /b 1
)
findstr /L /C:"[sem] channel" build\test\sem_channel_ctor_named_argument.err.txt >nul
if errorlevel 1 (
    echo sem_channel_ctor_named_argument.xl missing channel constructor named argument diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_channel_capacity_type.xl >nul 2> build\test\sem_channel_capacity_type.err.txt
if not errorlevel 1 (
    echo sem_channel_capacity_type.xl should fail.
    exit /b 1
)
findstr /L /C:"[sem] channel" build\test\sem_channel_capacity_type.err.txt >nul
if errorlevel 1 (
    echo sem_channel_capacity_type.xl missing channel capacity diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_channel_receiver_expr.xl >nul 2> build\test\sem_channel_receiver_expr.err.txt
if not errorlevel 1 (
    echo sem_channel_receiver_expr.xl should fail.
    exit /b 1
)
findstr /C:"channel receiver" build\test\sem_channel_receiver_expr.err.txt >nul
if errorlevel 1 (
    echo sem_channel_receiver_expr.xl missing named receiver diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_channel_send_named_argument.xl >nul 2> build\test\sem_channel_send_named_argument.err.txt
if not errorlevel 1 (
    echo sem_channel_send_named_argument.xl should fail.
    exit /b 1
)
findstr /C:"channel.send" build\test\sem_channel_send_named_argument.err.txt >nul
if errorlevel 1 (
    echo sem_channel_send_named_argument.xl missing channel send named argument diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_channel_recv_named_argument.xl >nul 2> build\test\sem_channel_recv_named_argument.err.txt
if not errorlevel 1 (
    echo sem_channel_recv_named_argument.xl should fail.
    exit /b 1
)
findstr /C:"channel.recvInto" build\test\sem_channel_recv_named_argument.err.txt >nul
if errorlevel 1 (
    echo sem_channel_recv_named_argument.xl missing channel recvInto named argument diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_channel_try_recv_named_argument.xl >nul 2> build\test\sem_channel_try_recv_named_argument.err.txt
if not errorlevel 1 (
    echo sem_channel_try_recv_named_argument.xl should fail.
    exit /b 1
)
findstr /C:"channel.tryRecvInto" build\test\sem_channel_try_recv_named_argument.err.txt >nul
if errorlevel 1 (
    echo sem_channel_try_recv_named_argument.xl missing channel tryRecvInto named argument diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_channel_unknown_method.xl >nul 2> build\test\sem_channel_unknown_method.err.txt
if not errorlevel 1 (
    echo sem_channel_unknown_method.xl should fail.
    exit /b 1
)
findstr /C:"unknown channel method" build\test\sem_channel_unknown_method.err.txt >nul
if errorlevel 1 (
    echo sem_channel_unknown_method.xl missing unknown channel method diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_channel_send_type.xl >nul 2> build\test\sem_channel_send_type.err.txt
if not errorlevel 1 (
    echo sem_channel_send_type.xl should fail.
    exit /b 1
)
findstr /C:"target int, value string" build\test\sem_channel_send_type.err.txt >nul
if errorlevel 1 (
    echo sem_channel_send_type.xl missing channel send type context.
    exit /b 1
)

release\xl.exe check test\cases\sem_channel_recv_target_type.xl >nul 2> build\test\sem_channel_recv_target_type.err.txt
if not errorlevel 1 (
    echo sem_channel_recv_target_type.xl should fail.
    exit /b 1
)
findstr /C:"target int, value float" build\test\sem_channel_recv_target_type.err.txt >nul
if errorlevel 1 (
    echo sem_channel_recv_target_type.xl missing channel recv target type context.
    exit /b 1
)

release\xl.exe check test\cases\sem_channel_try_recv_target_type.xl >nul 2> build\test\sem_channel_try_recv_target_type.err.txt
if not errorlevel 1 (
    echo sem_channel_try_recv_target_type.xl should fail.
    exit /b 1
)
findstr /C:"channel.tryRecvInto" build\test\sem_channel_try_recv_target_type.err.txt >nul
if errorlevel 1 (
    echo sem_channel_try_recv_target_type.xl missing channel tryRecvInto target type method.
    exit /b 1
)
findstr /C:"target int, value float" build\test\sem_channel_try_recv_target_type.err.txt >nul
if errorlevel 1 (
    echo sem_channel_try_recv_target_type.xl missing channel tryRecvInto target type context.
    exit /b 1
)

release\xl.exe check test\cases\sem_channel_recv_captured_target.xl >nul 2> build\test\sem_channel_recv_captured_target.err.txt
if not errorlevel 1 (
    echo sem_channel_recv_captured_target.xl should fail.
    exit /b 1
)
findstr /C:"[sem]" build\test\sem_channel_recv_captured_target.err.txt >nul
if errorlevel 1 (
    echo sem_channel_recv_captured_target.xl missing semantic diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_channel_try_recv_captured_target.xl >nul 2> build\test\sem_channel_try_recv_captured_target.err.txt
if not errorlevel 1 (
    echo sem_channel_try_recv_captured_target.xl should fail.
    exit /b 1
)
findstr /C:"[sem]" build\test\sem_channel_try_recv_captured_target.err.txt >nul
if errorlevel 1 (
    echo sem_channel_try_recv_captured_target.xl missing semantic diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_channel_recv_const_target.xl >nul 2> build\test\sem_channel_recv_const_target.err.txt
if not errorlevel 1 (
    echo sem_channel_recv_const_target.xl should fail.
    exit /b 1
)
findstr /C:"const" build\test\sem_channel_recv_const_target.err.txt >nul
if errorlevel 1 (
    echo sem_channel_recv_const_target.xl missing const target diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_channel_try_recv_const_target.xl >nul 2> build\test\sem_channel_try_recv_const_target.err.txt
if not errorlevel 1 (
    echo sem_channel_try_recv_const_target.xl should fail.
    exit /b 1
)
findstr /C:"const" build\test\sem_channel_try_recv_const_target.err.txt >nul
if errorlevel 1 (
    echo sem_channel_try_recv_const_target.xl missing const target diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_channel_class_element.xl >nul 2> build\test\sem_channel_class_element.err.txt
if not errorlevel 1 (
    echo sem_channel_class_element.xl should fail.
    exit /b 1
)
findstr /C:"channel<T>" build\test\sem_channel_class_element.err.txt >nul
if errorlevel 1 (
    echo sem_channel_class_element.xl missing channel element diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_channel_function_element.xl >nul 2> build\test\sem_channel_function_element.err.txt
if not errorlevel 1 (
    echo sem_channel_function_element.xl should fail.
    exit /b 1
)
findstr /C:"channel<T>" build\test\sem_channel_function_element.err.txt >nul
if errorlevel 1 (
    echo sem_channel_function_element.xl missing channel element diagnostic.
    exit /b 1
)

echo DEMO6 suite ok: channel
exit /b 0
