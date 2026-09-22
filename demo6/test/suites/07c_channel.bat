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

release\xl.exe check test\cases\sem_channel_recv_timeout_named_argument.xl >nul 2> build\test\sem_channel_recv_timeout_named_argument.err.txt
if not errorlevel 1 (
    echo sem_channel_recv_timeout_named_argument.xl should fail.
    exit /b 1
)
findstr /C:"channel.recvTimeoutInto" build\test\sem_channel_recv_timeout_named_argument.err.txt >nul
if errorlevel 1 (
    echo sem_channel_recv_timeout_named_argument.xl missing channel recvTimeoutInto named argument diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_channel_recv_timeout_type.xl >nul 2> build\test\sem_channel_recv_timeout_type.err.txt
if not errorlevel 1 (
    echo sem_channel_recv_timeout_type.xl should fail.
    exit /b 1
)
findstr /C:"timeoutMs" build\test\sem_channel_recv_timeout_type.err.txt >nul
if errorlevel 1 (
    echo sem_channel_recv_timeout_type.xl missing channel recvTimeoutInto timeout diagnostic.
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

release\xl.exe check test\cases\sem_channel_recv_timeout_target_type.xl >nul 2> build\test\sem_channel_recv_timeout_target_type.err.txt
if not errorlevel 1 (
    echo sem_channel_recv_timeout_target_type.xl should fail.
    exit /b 1
)
findstr /C:"channel.recvTimeoutInto" build\test\sem_channel_recv_timeout_target_type.err.txt >nul
if errorlevel 1 (
    echo sem_channel_recv_timeout_target_type.xl missing channel recvTimeoutInto target type method.
    exit /b 1
)
findstr /C:"target int, value float" build\test\sem_channel_recv_timeout_target_type.err.txt >nul
if errorlevel 1 (
    echo sem_channel_recv_timeout_target_type.xl missing channel recvTimeoutInto target type context.
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

release\xl.exe check test\cases\sem_channel_recv_captured_target.xl
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\sem_channel_try_recv_captured_target.xl
if errorlevel 1 exit /b 1

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
