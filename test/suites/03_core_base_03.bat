@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: core C generation and runtime baseline 3/10...

release\xl.exe check test\cases\cgen_typed_container_for_in_returned.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\cgen_typed_container_for_in_returned.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_typed_container_for_in_returned.xl --emit build\test\cgen_typed_container_for_in_returned.c
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_container_ownership_contract.ps1 -Name cgen_typed_container_for_in_returned
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror -DXLIR_GENERATED_MODULE=\"build/test/cgen_typed_container_for_in_returned.c\" test\cases\xlir_generated_module_host.c test\cases\cgen_class_host.c %XL6_CGEN_LINKS% -o build\test\cgen_typed_container_for_in_returned.exe
if errorlevel 1 exit /b 1
build\test\cgen_typed_container_for_in_returned.exe
if errorlevel 1 exit /b 1
for /L %%I in (1,1,3) do (
    build\test\cgen_typed_container_for_in_returned.exe
    if errorlevel 1 (
        echo cgen_typed_container_for_in_returned.exe repeat run %%I failed.
        exit /b 1
    )
)

release\xl.exe cgen test\cases\cgen_typed_container_box_to_var.xl --emit build\test\cgen_typed_container_box_to_var.c
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_container_ownership_contract.ps1 -Name cgen_typed_container_box_to_var
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror -DXLIR_GENERATED_MODULE=\"build/test/cgen_typed_container_box_to_var.c\" test\cases\xlir_generated_module_host.c test\cases\cgen_class_host.c %XL6_CGEN_LINKS% -o build\test\cgen_typed_container_box_to_var.exe
if errorlevel 1 exit /b 1
build\test\cgen_typed_container_box_to_var.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_type_desc_dedupe.xl --emit build\test\cgen_type_desc_dedupe.c
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_container_ownership_contract.ps1 -Name cgen_type_desc_dedupe
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror -DXLIR_GENERATED_MODULE=\"build/test/cgen_type_desc_dedupe.c\" test\cases\xlir_generated_module_host.c test\cases\cgen_class_host.c %XL6_CGEN_LINKS% -o build\test\cgen_type_desc_dedupe.exe
if errorlevel 1 exit /b 1
build\test\cgen_type_desc_dedupe.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_typed_container_copy_assign.xl --emit build\test\cgen_typed_container_copy_assign.c
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_container_ownership_contract.ps1 -Name cgen_typed_container_copy_assign
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror -DXLIR_GENERATED_MODULE=\"build/test/cgen_typed_container_copy_assign.c\" test\cases\xlir_generated_module_host.c test\cases\cgen_class_host.c %XL6_CGEN_LINKS% -o build\test\cgen_typed_container_copy_assign.exe
if errorlevel 1 exit /b 1
build\test\cgen_typed_container_copy_assign.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_typed_container_copy_objects_functions.xl --emit build\test\cgen_typed_container_copy_objects_functions.c
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_container_ownership_contract.ps1 -Name cgen_typed_container_copy_objects_functions
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror -DXLIR_GENERATED_MODULE=\"build/test/cgen_typed_container_copy_objects_functions.c\" test\cases\xlir_generated_module_host.c test\cases\cgen_class_host.c %XL6_CGEN_LINKS% -o build\test\cgen_typed_container_copy_objects_functions.exe
if errorlevel 1 exit /b 1
build\test\cgen_typed_container_copy_objects_functions.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_typed_container_functions.xl --emit build\test\cgen_typed_container_functions.c
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_container_ownership_contract.ps1 -Name cgen_typed_container_functions
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror -DXLIR_GENERATED_MODULE=\"build/test/cgen_typed_container_functions.c\" test\cases\xlir_generated_module_host.c test\cases\cgen_class_host.c %XL6_CGEN_LINKS% -o build\test\cgen_typed_container_functions.exe
if errorlevel 1 exit /b 1
build\test\cgen_typed_container_functions.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_typed_container_objects.xl --emit build\test\cgen_typed_container_objects.c
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_container_ownership_contract.ps1 -Name cgen_typed_container_objects
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror -DXLIR_GENERATED_MODULE=\"build/test/cgen_typed_container_objects.c\" test\cases\xlir_generated_module_host.c test\cases\cgen_class_host.c %XL6_CGEN_LINKS% -o build\test\cgen_typed_container_objects.exe
if errorlevel 1 exit /b 1
build\test\cgen_typed_container_objects.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_typed_container_function_field_objects.xl --emit build\test\cgen_typed_container_function_field_objects.c
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_container_ownership_contract.ps1 -Name cgen_typed_container_function_field_objects
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror -DXLIR_GENERATED_MODULE=\"build/test/cgen_typed_container_function_field_objects.c\" test\cases\xlir_generated_module_host.c test\cases\cgen_class_host.c %XL6_CGEN_LINKS% -o build\test\cgen_typed_container_function_field_objects.exe
if errorlevel 1 exit /b 1
build\test\cgen_typed_container_function_field_objects.exe
if errorlevel 1 exit /b 1


echo DEMO6 suite ok: core C generation and runtime baseline 3/10
exit /b 0
