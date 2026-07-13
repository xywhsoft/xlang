@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: OOP ABI export and interop baseline 8/8...

release\xl.exe cgen test\cases\cgen_class_field_and_container_param.xl --emit build\test\cgen_class_field_and_container_param.c
if errorlevel 1 exit /b 1
findstr /C:"xl6_missing_to_value" build\test\cgen_class_field_and_container_param.c >nul
if not errorlevel 1 (
    echo cgen_class_field_and_container_param.c should box nested class fields as records.
    exit /b 1
)
findstr /C:"xl6_missing_from_value" build\test\cgen_class_field_and_container_param.c >nul
if not errorlevel 1 (
    echo cgen_class_field_and_container_param.c should unbox nested class fields as records.
    exit /b 1
)
powershell -NoProfile -Command "$lines=Get-Content -LiteralPath 'build\test\cgen_class_field_and_container_param.c'; $forward=($lines | Select-String -SimpleMatch 'typedef struct XL_Class_Buffer XL_Class_Buffer;' | Select-Object -First 1).LineNumber; $proto=($lines | Select-String -SimpleMatch 'xl_class_ContainerParamBox__bufferLength' | Select-Object -First 1).LineNumber; $layout=($lines | Select-String -SimpleMatch 'struct XL_Class_Buffer {' | Select-Object -First 1).LineNumber; if (!$forward -or !$proto -or !$layout -or $forward -ge $proto -or $proto -ge $layout) { exit 1 }"
if errorlevel 1 (
    echo cgen_class_field_and_container_param.c has invalid class declaration ordering.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_class_field_and_container_param.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_class_field_and_container_param.exe
if errorlevel 1 exit /b 1
build\test\cgen_class_field_and_container_param.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_typed_container_param_mutation.xl --emit build\test\cgen_typed_container_param_mutation.c
if errorlevel 1 exit /b 1
findstr /C:"mutateArray(arrayValue);" build\test\cgen_typed_container_param_mutation.c >nul
if errorlevel 1 (
    echo cgen_typed_container_param_mutation.c should borrow named typed-container arguments.
    exit /b 1
)
findstr /C:"xl_fn_mutateArray((*((XL_Class_Store*)xl6_class_data(store" build\test\cgen_typed_container_param_mutation.c >nul
if errorlevel 1 (
    echo cgen_typed_container_param_mutation.c should borrow typed-container field arguments.
    exit /b 1
)
findstr /C:"consumeArray(xl6_xrt_array_varargs" build\test\cgen_typed_container_param_mutation.c >nul
if not errorlevel 1 (
    echo cgen_typed_container_param_mutation.c passed a dynamic literal to a static container parameter.
    exit /b 1
)
findstr /C:"nestedValue = (xl_fn_consumeArray(__xl6_call_arg_" build\test\cgen_typed_container_param_mutation.c >nul
if errorlevel 1 (
    echo cgen_typed_container_param_mutation.c missing nested assignment argument lowering.
    exit /b 1
)
findstr /C:"xrtTypedArrayDestroy(__xl6_call_arg_" build\test\cgen_typed_container_param_mutation.c >nul
if errorlevel 1 (
    echo cgen_typed_container_param_mutation.c missing owned call argument cleanup.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_typed_container_param_mutation.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_typed_container_param_mutation.exe
if errorlevel 1 exit /b 1
build\test\cgen_typed_container_param_mutation.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\sem_native_class_const_point.xl >nul 2> build\test\sem_native_class_const_point.err.txt
if not errorlevel 1 (
    echo sem_native_class_const_point.xl should reject ownership transfer from const point.
    exit /b 1
)
findstr /C:"owning native-backed class cannot consume a const point handle" build\test\sem_native_class_const_point.err.txt >nul
if errorlevel 1 (
    echo sem_native_class_const_point.xl missing const ownership diagnostic.
    exit /b 1
)

release\xl.exe cgen test\cases\cgen_class_param_value_ownership.xl --emit build\test\cgen_class_param_value_ownership.c
if errorlevel 1 exit /b 1
findstr /C:"xl_class_ValueBox______xl6__copy__value" build\test\cgen_class_param_value_ownership.c >nul
if errorlevel 1 (
    echo cgen_class_param_value_ownership.c missing byval class copy path.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_class_param_value_ownership.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_class_param_value_ownership.exe
if errorlevel 1 exit /b 1
build\test\cgen_class_param_value_ownership.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_export_typed_container_abi.xl --emit build\test\cgen_export_typed_container_abi.c --emit-header build\test\cgen_export_typed_container_abi.h
if errorlevel 1 exit /b 1
findstr /C:"xtarray values" build\test\cgen_export_typed_container_abi.c >nul
if errorlevel 1 (
    echo cgen_export_typed_container_abi.c missing static container ABI.
    exit /b 1
)
findstr /C:"xl6_export_sumInternal(xvalue values)" build\test\cgen_export_typed_container_abi.c >nul
if not errorlevel 1 (
    echo cgen_export_typed_container_abi.c regressed to dynamic container ABI.
    exit /b 1
)
"%XL6_BOOTSTRAP_CC%" -std=c11 -Werror=incompatible-pointer-types -Ibuild\test -fsyntax-only build\test\cgen_export_typed_container_abi.c
if errorlevel 1 exit /b 1
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_export_typed_container_abi.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_export_typed_container_abi.exe
if errorlevel 1 exit /b 1
build\test\cgen_export_typed_container_abi.exe
if errorlevel 1 exit /b 1


echo DEMO6 suite ok: OOP ABI export and interop baseline 8/8
exit /b 0
