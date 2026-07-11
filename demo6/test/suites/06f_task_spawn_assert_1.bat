@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: task spawn cgen assertions 1/4...

if not "%XL6_TASK_SPAWN_CGEN_READY%"=="1" (
    release\xl.exe check test\cases\cgen_task_spawn.xl
    if errorlevel 1 exit /b 1
    release\xl.exe cgen test\cases\cgen_task_spawn.xl --emit build\test\cgen_task_spawn.c
    if errorlevel 1 exit /b 1
)

if errorlevel 1 exit /b 1

findstr /C:"xl6_task_spawn_values" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing task.spawn runtime helper.
    exit /b 1
)
findstr /C:"static xvalue xl6_function_call_argv_borrow(XL6_Function fn, int argc, xvalue* argv)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing borrowed argv function bridge.
    exit /b 1
)
findstr /C:"value = xl6_function_call_argv_borrow(arg->fn, arg->argc, arg->argv);" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should call task.spawn functions through borrowed argv bridge.
    exit /b 1
)
findstr /C:"void xrtTypeDropValue(const xrt_type_desc* pType, void* pObj);" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing XRT typed value drop declaration.
    exit /b 1
)
findstr /C:"static void* xl6_async_typed_box_alloc" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed result box allocator.
    exit /b 1
)
findstr /C:"xrtTypeDropValue(box->type, pValue);" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed result box drop.
    exit /b 1
)
findstr /C:"box = xl6_async_typed_box_alloc(arg->resultType, arg->resultSize);" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed result box allocation.
    exit /b 1
)
findstr /C:"pOut->pfnFreeValue = xl6_async_free_typed_value_box;" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed result box free hook.
    exit /b 1
)
findstr /C:"value = va_arg(ap, xvalue);" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing task.spawn vararg failure cleanup value read.
    exit /b 1
)
findstr /C:"if ( value != 0 ) { xvoUnref(value); }" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing task.spawn vararg failure cleanup unref.
    exit /b 1
)
findstr /C:"arg->fn.invoke_var == 0 && arg->fn.invoke_xcall == 0" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should accept invoke_xcall-only function adapters.
    exit /b 1
)
findstr /C:"static xfuture* xl6_task_spawn_start(XL6_Function fn, int retainBorrow, int argc, xvalue* argv" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing task.spawn retainBorrow start signature.
    exit /b 1
)
findstr /C:"static xfuture* xl6_task_spawn_values(XL6_Function fn, const xrt_type_desc* resultType, size_t resultSize, const char* file, int line, int column, int retainBorrow, int argc, ...)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing task.spawn retainBorrow values signature.
    exit /b 1
)
findstr /C:"if ( retainBorrow && fn.retain != 0 && fn.env != 0 )" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing borrowed task.spawn function retain.
    exit /b 1
)
findstr /C:"if ( !retainBorrow && fn.release != 0 && fn.env != 0 ) { fn.release(fn.env); }" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing owned task.spawn function failure release.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); $idx=$text.IndexOf('static xfuture* xl6_task_spawn_values('); if($idx -lt 0){exit 1}; $end=$text.IndexOf('future = xl6_task_spawn_start', $idx); if($end -lt 0){exit 1}; $slice=$text.Substring($idx,$end-$idx); if($slice -match 'fn\.retain\(fn\.env\)'){exit 1}"
if errorlevel 1 (
    echo cgen_task_spawn.c should not retain task.spawn functions before ownership classification.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'static xfuture\* xl6_task_spawn_values\([\s\S]*?if \( argv == 0 \) \{[\s\S]*?va_start\(ap, argc\);[\s\S]*?value = va_arg\(ap, xvalue\);[\s\S]*?if \( value != 0 \) \{ xvoUnref\(value\); \}[\s\S]*?va_end\(ap\);[\s\S]*?if \( !retainBorrow && fn\.release != 0 && fn\.env != 0 \) \{ fn\.release\(fn\.env\); \}[\s\S]*?out of memory while preparing task\.spawn arguments'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing task.spawn argv allocation failure cleanup.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'static xfuture\* xl6_task_spawn_start\([\s\S]*?if \( arg == 0 \) \{[\s\S]*?if \( fn\.release != 0 && fn\.env != 0 \) \{ fn\.release\(fn\.env\); \}[\s\S]*?out of memory while starting task\.spawn' -or $text -notmatch 'static xfuture\* xl6_task_spawn_start\([\s\S]*?if \( arg->argv == 0 \) \{[\s\S]*?xl6_task_spawn_arg_release\(arg\);[\s\S]*?out of memory while copying task\.spawn arguments' -or $text -notmatch 'static xfuture\* xl6_task_spawn_start\([\s\S]*?if \( future == 0 \) \{[\s\S]*?xl6_task_spawn_arg_release\(arg\);[\s\S]*?return xl6_async_require_future\(future, file, line, column\);'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing task.spawn start failure cleanup.
    exit /b 1
)
findstr /C:"if ( arg->resultSize == 0 )" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing no-return task.spawn success branch.
    exit /b 1
)
findstr /C:"voidSpawnFuture = xl6_task_spawn_values(voidJob, xrtTypeNull(), 0" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing no-return task.spawn future fixture.
    exit /b 1
)
findstr /C:"xl6_future_await_value(voidSpawnFuture" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing no-return task.spawn void await.
    exit /b 1
)
findstr /C:"cancelTemporaryVoidSpawn = xl6_task_cancel_value(xl6_task_spawn_values(voidJob, xrtTypeNull(), 0" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing temporary no-return task.spawn cancel fixture.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'xl6_task_cancel_value\(\(chooseConditionalVoidCancel \? xl6_future_add_ref_if_needed\(conditionalVoidCancelSource\) : xl6_task_spawn_values\(voidJob, xrtTypeNull\(\), 0,[\s\S]*?\), 1\)' -or $text -notmatch 'conditionalVoidOwnedCancel\s*=\s*xl6_task_cancel_value\(\(chooseConditionalVoidCancel \? xl6_future_add_ref_if_needed\(conditionalVoidCancelSource\) : xl6_task_spawn_values\(voidJob, xrtTypeNull\(\), 0,[\s\S]*?\), 1\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing conditional no-return task.cancel ownership normalization.
    exit /b 1
)
findstr /C:"if ( arg->itemType == xrtTypeNull() )" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing void task.all runtime branch.
    exit /b 1
)
findstr /C:"voidAllFuture = xl6_task_all_values(xrtTypeNull()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing no-return task.all future fixture.
    exit /b 1
)
findstr /C:"voidRaceFuture = xl6_task_race_values" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing no-return task.race future fixture.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'xl6_future_await_void_release\(xl6_task_all_values\(xrtTypeNull\(\),[\s\S]*?\(chooseConditionalVoidAll \? xl6_future_add_ref_if_needed\(conditionalVoidAllSource\) : xl6_task_spawn_values\(voidJob, xrtTypeNull\(\), 0,[\s\S]*?\), 1,[\s\S]*?\(0 \? xl6_future_add_ref_if_needed\(conditionalVoidAllSource\) : xl6_task_spawn_values\(voidJob, xrtTypeNull\(\), 0,[\s\S]*?\), 1' -or $text -notmatch 'xl6_future_await_void_release\(xl6_task_race_values\([\s\S]*?\(chooseConditionalVoidRace \? xl6_future_add_ref_if_needed\(conditionalVoidRaceSource\) : xl6_task_spawn_values\(voidJob, xrtTypeNull\(\), 0,[\s\S]*?\), 1'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing conditional no-return task.all/race ownership normalization.
    exit /b 1
)
findstr /C:"typedVoidAllFuture = xl6_task_all_from_array((xtarray)(typedVoidAllJobs), xrtTypeNull()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array no-return task.all fixture.
    exit /b 1
)
findstr /C:"typedVoidRaceFuture = xl6_task_race_from_array((xtarray)(typedVoidRaceJobs)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array no-return task.race fixture.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); $checks=@('xrtTypedArrayCreate(xrtTypeFuture(), XRT_OBJMODE_SHARED)','chooseTypedConditionalVoidAll ? xl6_future_add_ref_if_needed(typedConditionalVoidAllSource) : xl6_task_spawn_values(voidJob, xrtTypeNull(), 0','0 ? xl6_future_add_ref_if_needed(typedConditionalVoidAllSource) : xl6_task_spawn_values(voidJob, xrtTypeNull(), 0','xl6_task_all_from_array((xtarray)(typedConditionalVoidAllJobs), xrtTypeNull()','chooseTypedConditionalVoidRace ? xl6_future_add_ref_if_needed(typedConditionalVoidRaceSource) : xl6_task_spawn_values(voidJob, xrtTypeNull(), 0','xl6_task_race_from_array((xtarray)(typedConditionalVoidRaceJobs)'); foreach($p in $checks){ if($text.IndexOf($p) -lt 0){ exit 1 } }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array conditional no-return future ownership normalization.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'if \(!xrtTypedArrayAppend\([^,]+, &(__xl6_typed_future_\d+)\)\) \{[\s\S]*?xFutureRelease\(\1\);[\s\S]*?xl6_runtime_fatal\(3\);[\s\S]*?\} else \{[\s\S]*?xFutureRelease\(\1\);'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array conditional no-return append failure cleanup.
    exit /b 1
)
findstr /C:"emptyVoidJobs = __xl6_global_init_" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing empty no-return future array fixture.
    exit /b 1
)
findstr /C:"xl6_future_await_void_release(xl6_task_all_from_array((xtarray)(emptyVoidJobs), xrtTypeNull()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing empty no-return task.all typed-array await path.
    exit /b 1
)
findstr /C:"xl6_future_await_void_release(xl6_task_race_from_array((xtarray)(emptyVoidJobs)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing empty no-return task.race typed-array await path.
    exit /b 1
)
findstr /C:"defaultVoidJobs = xrtTypedArrayCreate(xrtTypeFuture(), XRT_OBJMODE_SHARED)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing default no-return future array fixture.
    exit /b 1
)
findstr /C:"xl6_future_await_void_release(xl6_task_all_from_array((xtarray)(defaultVoidJobs), xrtTypeNull()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing default no-return task.all typed-array await path.
    exit /b 1
)
findstr /C:"xl6_future_await_void_release(xl6_task_race_from_array((xtarray)(defaultVoidJobs)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing default no-return task.race typed-array await path.
    exit /b 1
)
findstr /C:"pendingVoidSpawn = xl6_task_spawn_values(slowVoidTask, xrtTypeNull(), 0" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing pending no-return task.spawn fixture.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(pendingVoidSpawn, 0)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should cancel pending no-return task.spawn future as borrowed.
    exit /b 1
)
findstr /C:"pendingVoidAllGroup = xl6_task_all_values(xrtTypeNull()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing pending no-return task.all fixture.
    exit /b 1
)
findstr /C:"pendingVoidRaceGroup = xl6_task_race_values" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing pending no-return task.race fixture.
    exit /b 1
)
findstr /C:"failedVoidSpawn = xl6_task_spawn_values(throwingVoidJob, xrtTypeNull(), 0" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing failed no-return task.spawn fixture.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(failedVoidSpawn, 0)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should pass failed no-return task.spawn future cancel as borrowed.
    exit /b 1
)
findstr /C:"failedVoidAll = xl6_task_all_values(xrtTypeNull()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing failed no-return task.all fixture.
    exit /b 1
)
findstr /C:"failedVoidRace = xl6_task_race_values" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing failed no-return task.race fixture.
    exit /b 1
)
findstr /C:"defaultJob = xl6_fnvalue_xl_fn_addDefault_arity1_make();" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing optional-parameter adapter task.spawn case.
    exit /b 1
)
findstr /C:"defaultFuture = xl6_task_spawn_values(defaultJob, xrtTypeInt(), sizeof(long long)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing named optional adapter task.spawn future.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'defaultFuture\s*=\s*xl6_task_spawn_values\(defaultJob,\s*xrtTypeInt\(\),\s*sizeof\(long long\),\s*\"\",\s*\d+,\s*\d+,\s*1,\s*1,\s*xvoCreateInt\(\(long long\)\(41LL\)\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing named optional adapter task.spawn argv bridge.
    exit /b 1
)
findstr /C:"defaultFutureA = (*(long long*)xl6_future_await_value(defaultFuture" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing named optional adapter task.spawn first await.
    exit /b 1
)
findstr /C:"defaultFutureB = (*(long long*)xl6_future_await_value(defaultFuture" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing named optional adapter task.spawn repeated await.
    exit /b 1
)
findstr /C:"!xl6_task_cancel_value(defaultFuture, 0)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing completed optional adapter task.spawn cancel guard.
    exit /b 1
)
findstr /C:"defaultFutureAfterCancel = (*(long long*)xl6_future_await_value(defaultFuture" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing post-cancel optional adapter task.spawn await.
    exit /b 1
)
findstr /C:"XL6_Function missingJob;" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing empty function value task.spawn fixture.
    exit /b 1
)
findstr /C:"missingJobValue = xl6_future_await_i64_release(xl6_task_spawn_values(missingJob" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing empty function value task.spawn call.
    exit /b 1
)
findstr /C:"task.spawn requires a callable func value" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing empty function value task.spawn diagnostic.
    exit /b 1
)
findstr /C:"XL6_Function missingStringJob;" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing empty string function value task.spawn fixture.
    exit /b 1
)
findstr /C:"missingStringJobValue = xrtCopyStr(xl6_future_await_cstr_release(xl6_task_spawn_values(missingStringJob, xrtTypeString()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing empty string function value task.spawn call.
    exit /b 1
)
findstr /C:"missingStringJobState == 42LL" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing empty string function value task.spawn assertion.
    exit /b 1
)
findstr /C:"XL6_Function missingBoolJob;" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing empty bool function value task.spawn fixture.
    exit /b 1
)
findstr /C:"missingBoolJobValue = xl6_future_await_i32_release(xl6_task_spawn_values(missingBoolJob, xrtTypeBool()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing empty bool function value task.spawn call.
    exit /b 1
)
findstr /C:"missingBoolJobState == 42LL" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing empty bool function value task.spawn assertion.
    exit /b 1
)
findstr /C:"XL6_Function missingFloatJob;" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing empty float function value task.spawn fixture.
    exit /b 1
)
findstr /C:"missingFloatJobValue = xl6_future_await_f64_release(xl6_task_spawn_values(missingFloatJob, xrtTypeFloat()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing empty float function value task.spawn call.
    exit /b 1
)
findstr /C:"missingFloatJobState == 42LL" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing empty float function value task.spawn assertion.
    exit /b 1
)
findstr /C:"repeatedSpawn = xl6_task_spawn_values(addJob" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing named task.spawn future initialization.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'xl6_task_spawn_values\(addJob, xrtTypeInt\(\), sizeof\(long long\), [^,]*, \d+, 27, 1, 2'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c should retain borrowed named function values for task.spawn.
    exit /b 1
)
findstr /C:"repeatedB = (*(long long*)xl6_future_await_value(repeatedSpawn" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing repeated await for named task.spawn future.
    exit /b 1
)
findstr /C:"postCancelRepeatedSpawn = (*(long long*)xl6_future_await_value(repeatedSpawn" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep completed task.spawn int future awaitable after cancel.
    exit /b 1
)
findstr /C:"directClosureValue = xl6_future_await_i64_release(xl6_task_spawn_values(xl_fn_makeAdder(40LL)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing direct closure expression task.spawn await.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'xl6_task_spawn_values\(xl_fn_makeAdder\(40LL\), xrtTypeInt\(\), sizeof\(long long\), [^,]*, \d+, 37, 0, 1'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c should consume owned direct closure function values for task.spawn.
    exit /b 1
)
findstr /C:"directClosureFuture = xl6_task_spawn_values(xl_fn_makeAdder(41LL)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing direct closure expression named future.
    exit /b 1
)
findstr /C:"directClosureA = (*(long long*)xl6_future_await_value(directClosureFuture" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing first repeated await for direct closure future.
    exit /b 1
)
findstr /C:"directClosureB = (*(long long*)xl6_future_await_value(directClosureFuture" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing second repeated await for direct closure future.
    exit /b 1
)
findstr /C:"!xl6_task_cancel_value(directClosureFuture, 0)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should cancel completed direct closure future as borrowed.
    exit /b 1
)
findstr /C:"directClosureAfterCancel = (*(long long*)xl6_future_await_value(directClosureFuture" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep completed direct closure future awaitable after cancel.
    exit /b 1
)
findstr /C:"directClosureAllJobs = __xl6_global_init_" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed future array for direct closure task.all.
    exit /b 1
)
findstr /C:"xl6_task_spawn_values(xl_fn_makeAdder(50LL)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing first direct closure task.all source.
    exit /b 1
)
findstr /C:"xrtTypedArrayAppend(__xl6_global_init_" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed array append for direct closure task.all.
    exit /b 1
)
findstr /C:"xFutureRelease(__xl6_typed_future_" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should release temporary typed futures after direct closure task.all append.
    exit /b 1
)
findstr /C:"xl6_task_all_from_array((xtarray)(directClosureAllJobs), xrtTypeInt()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array task.all await for direct closure futures.
    exit /b 1
)
findstr /C:"directClosureRaceValue = xl6_future_await_i64_release(xl6_task_race_values" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing literal task.race await for direct closure future.
    exit /b 1
)
findstr /C:"xl6_task_spawn_values(xl_fn_makeAdder(70LL)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing direct closure task.race source.
    exit /b 1
)
findstr /C:"awaitedClosureValue = xl6_future_await_i64_release(xl6_task_spawn_values(xl6_future_await_function_release(xl_fn_makeAsyncAdder(80LL)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing awaited closure expression task.spawn await.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'xl6_task_spawn_values\(xl6_future_await_function_release\(xl_fn_makeAsyncAdder\(80LL\), [^,]*, \d+, 44\), xrtTypeInt\(\), sizeof\(long long\), [^,]*, \d+, 38, 0, 1'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c should consume owned awaited closure function values for task.spawn.
    exit /b 1
)
findstr /C:"awaitedClosureFuture = xl6_task_spawn_values(xl6_future_await_function_release(xl_fn_makeAsyncAdder(81LL)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing awaited closure expression named future.
    exit /b 1
)
findstr /C:"awaitedClosureA = (*(long long*)xl6_future_await_value(awaitedClosureFuture" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing first repeated await for awaited closure future.
    exit /b 1
)
findstr /C:"awaitedClosureB = (*(long long*)xl6_future_await_value(awaitedClosureFuture" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing second repeated await for awaited closure future.
    exit /b 1
)
findstr /C:"!xl6_task_cancel_value(awaitedClosureFuture, 0)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should cancel completed awaited closure future as borrowed.
    exit /b 1
)
findstr /C:"awaitedClosureAfterCancel = (*(long long*)xl6_future_await_value(awaitedClosureFuture" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep completed awaited closure future awaitable after cancel.
    exit /b 1
)
findstr /C:"awaitedClosureAllJobs = __xl6_global_init_" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed future array for awaited closure task.all.
    exit /b 1
)
findstr /C:"xl6_task_spawn_values(xl6_future_await_function_release(xl_fn_makeAsyncAdder(90LL)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing first awaited closure task.all source.
    exit /b 1
)
findstr /C:"xrtTypedArrayAppend(__xl6_global_init_" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed array append for awaited closure task.all.
    exit /b 1
)
findstr /C:"xl6_task_all_from_array((xtarray)(awaitedClosureAllJobs), xrtTypeInt()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array task.all await for awaited closure futures.
    exit /b 1
)
findstr /C:"awaitedClosureRaceValue = xl6_future_await_i64_release(xl6_task_race_values" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing literal task.race await for awaited closure future.
    exit /b 1
)
findstr /C:"xl6_task_spawn_values(xl6_future_await_function_release(xl_fn_makeAsyncAdder(110LL)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing awaited closure task.race source.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'conditionalClosureValue\s*=\s*xl6_future_await_i64_release\(xl6_task_spawn_values\(\(1 \? xl_fn_makeAdder\(150LL\) : xl_fn_makeAdder\(160LL\)\)' -or $text -notmatch 'conditionalClosureFuture\s*=\s*xl6_task_spawn_values\(\(chooseFirstClosure \? xl_fn_makeAdder\(150LL\) : xl_fn_makeAdder\(160LL\)\)' -or $text -notmatch 'xrtTypedArrayCreate\(xrtTypeFuture\(\), XRT_OBJMODE_SHARED\)' -or $text -notmatch 'xl6_future_await_i64_release\(xl6_task_race_values[\s\S]*?xl6_task_spawn_values\(\(1 \? xl_fn_makeAdder\(210LL\) : xl_fn_makeAdder\(220LL\)\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing conditional closure task.spawn source.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'xl6_task_spawn_values\(\(1 \? xl_fn_makeAdder\(150LL\) : xl_fn_makeAdder\(160LL\)\), xrtTypeInt\(\), sizeof\(long long\), [^,]*, \d+, 42, 0, 1' -or $text -notmatch 'xl6_task_spawn_values\(\(chooseFirstClosure \? xl_fn_makeAdder\(150LL\) : xl_fn_makeAdder\(160LL\)\), xrtTypeInt\(\), sizeof\(long long\), [^,]*, \d+, 45, 0, 1'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c should consume owned conditional closure function values for task.spawn.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'borrowedConditionalClosureValue\s*=\s*xl6_future_await_i64_release\(xl6_task_spawn_values\(\(chooseBorrowedClosure \? borrowedClosureLeft : borrowedClosureRight\)' -or $text -notmatch 'borrowedConditionalClosureFuture\s*=\s*xl6_task_spawn_values\(\(chooseBorrowedClosure \? borrowedClosureLeft : borrowedClosureRight\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing borrowed conditional closure task.spawn source.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'xl6_task_spawn_values\(\(chooseBorrowedClosure \? borrowedClosureLeft : borrowedClosureRight\), xrtTypeInt\(\), sizeof\(long long\), [^,]*, \d+, 50, 1, 1' -or $text -notmatch 'xl6_task_spawn_values\(\(chooseBorrowedClosure \? borrowedClosureLeft : borrowedClosureRight\), xrtTypeInt\(\), sizeof\(long long\), [^,]*, \d+, 53, 1, 1'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c should retain borrowed conditional closure function values for task.spawn.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'mixedTaskOwnedClosureValue\s*=\s*xl6_future_await_i64_release\(xl6_task_spawn_values\(\(chooseMixedTaskClosure \? xl6_function_copy_value\(mixedTaskClosureSource\) : xl_fn_makeAdder\(270LL\)\)' -or $text -notmatch 'mixedTaskBorrowedClosureValue\s*=\s*xl6_future_await_i64_release\(xl6_task_spawn_values\(\(chooseMixedTaskClosure \? xl6_function_copy_value\(mixedTaskClosureSource\) : xl_fn_makeAdder\(280LL\)\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing mixed conditional closure branch copy for task.spawn.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'xl6_task_spawn_values\(\(chooseMixedTaskClosure \? xl6_function_copy_value\(mixedTaskClosureSource\) : xl_fn_makeAdder\(270LL\)\), xrtTypeInt\(\), sizeof\(long long\), [^,]*, \d+, 45, 0, 1' -or $text -notmatch 'xl6_task_spawn_values\(\(chooseMixedTaskClosure \? xl6_function_copy_value\(mixedTaskClosureSource\) : xl_fn_makeAdder\(280LL\)\), xrtTypeInt\(\), sizeof\(long long\), [^,]*, \d+, 48, 0, 1'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c should consume normalized mixed conditional function values for task.spawn.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'memberClosureValue\s*=\s*xl6_future_await_i64_release\(xl6_task_spawn_values\(memberClosureBox\.op,' -or $text -notmatch 'memberClosureFuture\s*=\s*xl6_task_spawn_values\(memberClosureBox\.op,'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing class function field task.spawn source.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'xl6_task_spawn_values\(memberClosureBox\.op, xrtTypeInt\(\), sizeof\(long long\), [^,]*, \d+, 37, 1, 1' -or $text -notmatch 'xl6_task_spawn_values\(memberClosureBox\.op, xrtTypeInt\(\), sizeof\(long long\), [^,]*, \d+, 40, 1, 1'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c should retain borrowed class function fields for task.spawn.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'borrowedMemberConditionalValue\s*=\s*xl6_future_await_i64_release\(xl6_task_spawn_values\(\(chooseMemberClosure \? memberClosureBox\.op : memberClosureAlt\.op\)' -or $text -notmatch 'xl6_task_spawn_values\(\(chooseMemberClosure \? memberClosureBox\.op : memberClosureAlt\.op\), xrtTypeInt\(\), sizeof\(long long\), [^,]*, \d+, 49, 1, 1'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c should retain borrowed conditional class function fields for task.spawn.
    exit /b 1
)
findstr /C:"indexedClosureFuncs = __xl6_global_init_" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed function array for indexed closure task.spawn.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'indexedClosureValue\s*=\s*xl6_future_await_i64_release\(xl6_task_spawn_values\(xl6_function_from_callable\(\*\(xrt_callable\*\*\).*indexedClosureFuncs' -or $text -notmatch 'indexedClosureFuture\s*=\s*xl6_task_spawn_values\(xl6_function_from_callable\(\*\(xrt_callable\*\*\).*indexedClosureFuncs'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing indexed function item task.spawn source.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'indexedClosureValue\s*=\s*xl6_future_await_i64_release\(xl6_task_spawn_values\(xl6_function_from_callable\([\s\S]*?indexedClosureFuncs[\s\S]*?, xrtTypeInt\(\), sizeof\(long long\), [^,]*, \d+, 38, 0, 1' -or $text -notmatch 'indexedClosureFuture\s*=\s*xl6_task_spawn_values\(xl6_function_from_callable\([\s\S]*?indexedClosureFuncs[\s\S]*?, xrtTypeInt\(\), sizeof\(long long\), [^,]*, \d+, 41, 0, 1'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c should consume owned indexed callable conversions for task.spawn.
    exit /b 1
)
findstr /C:"indexedClosureAllJobs = __xl6_global_init_" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed future array for indexed closure task.all.
    exit /b 1
)
findstr /C:"indexedClosureRaceValue = xl6_future_await_i64_release(xl6_task_race_values" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing literal task.race await for indexed closure future.
    exit /b 1
)
findstr /C:"repeatedFailedSpawn = xl6_task_spawn_values(throwingJob" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing repeated failed task.spawn future fixture.
    exit /b 1
)
findstr /C:"failedSpawnB = (*(long long*)xl6_future_await_value(repeatedFailedSpawn" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing repeated await for failed task.spawn future.
    exit /b 1
)
findstr /C:"cancelRepeatedFailedSpawn = xl6_task_cancel_value(repeatedFailedSpawn, 0)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should pass failed task.spawn future cancel as borrowed.
    exit /b 1
)
findstr /C:"postCancelFailedSpawn = (*(long long*)xl6_future_await_value(repeatedFailedSpawn" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep failed task.spawn future awaitable after cancel.
    exit /b 1
)
findstr /C:"spawnAllFailureGroup = xl6_task_all_values(xrtTypeInt()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing task.all fast-failure over task.spawn fixture.
    exit /b 1
)
findstr /C:"spawnAllFailureSlowSource, 0" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should pass slow task.spawn source as borrowed to task.all fast-failure fixture.
    exit /b 1
)
findstr /C:"spawnAllFailureSlowValue = (*(long long*)xl6_future_await_value(spawnAllFailureSlowSource" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep slow task.spawn source awaitable after task.all failure.
    exit /b 1
)
findstr /C:"typedSpawnAllFailureGroup = xl6_task_all_from_array((xtarray)(typedSpawnAllFailureJobs), xrtTypeInt()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array task.all fast-failure over task.spawn fixture.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_array_copy_mode(xrtTypeInt(), (*(xtarray*)xl6_future_await_value(typedSpawnAllFailureGroup" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array task.all fast-failure group await.
    exit /b 1
)
findstr /C:"typedSpawnAllFailureSlowValue = (*(long long*)xl6_future_await_value(typedSpawnAllFailureSlowSource" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep typed-array slow task.spawn source awaitable after task.all failure.
    exit /b 1
)
findstr /C:"!xl6_task_cancel_value(typedSpawnAllFailureSlowSource, 0)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep typed-array task.all slow task.spawn source completed after group failure.
    exit /b 1
)
findstr /C:"spawnRaceFailureGroup = xl6_task_race_values" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing task.race fast-failure over task.spawn fixture.
    exit /b 1
)
findstr /C:"spawnRaceFailureSlowLoser, 0" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should pass slow task.spawn loser as borrowed to task.race fast-failure fixture.
    exit /b 1
)
findstr /C:"spawnRaceFailureLoserValueB = (*(long long*)xl6_future_await_value(spawnRaceFailureSlowLoser" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing repeated await for canceled task.spawn race loser.
    exit /b 1
)
findstr /C:"spawnRaceFailureLoserStateB == 42LL" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should preserve canceled task.spawn race loser after worker completes.
    exit /b 1
)
findstr /C:"typedSpawnRaceFailureGroup = xl6_task_race_from_array((xtarray)(typedSpawnRaceFailureJobs)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array task.race fast-failure over task.spawn fixture.
    exit /b 1
)
findstr /C:"typedSpawnRaceFailureValue = (*(long long*)xl6_future_await_value(typedSpawnRaceFailureGroup" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array task.race fast-failure group await.
    exit /b 1
)
findstr /C:"typedSpawnRaceFailureLoserValueA = (*(long long*)xl6_future_await_value(typedSpawnRaceFailureSlowLoser" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing first await for canceled typed-array task.spawn race loser.
    exit /b 1
)
findstr /C:"typedSpawnRaceFailureLoserValueB = (*(long long*)xl6_future_await_value(typedSpawnRaceFailureSlowLoser" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing repeated await for canceled typed-array task.spawn race loser.
    exit /b 1
)
findstr /C:"typedSpawnRaceFailureLoserStateB == 42LL" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should preserve canceled typed-array task.spawn race loser after worker completes.
    exit /b 1
)
findstr /C:"missingSpawnFuture = 0;" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing null task.spawn aggregation source fixture.
    exit /b 1
)
findstr /C:"xl6_task_all_values(xrtTypeInt()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing task.all null source fixture over task.spawn.
    exit /b 1
)
findstr /C:"xvoCreateInt((long long)(140LL)), xvoCreateInt((long long)(1LL))), 1, missingSpawnFuture, 0" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should pass owned task.spawn then borrowed null source to task.all.
    exit /b 1
)
findstr /C:"nullSpawnRace = xl6_future_await_i64_release(xl6_task_race_values" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing task.race null source fixture over task.spawn.
    exit /b 1
)
findstr /C:"xvoCreateInt((long long)(150LL)), xvoCreateInt((long long)(1LL))), 1, missingSpawnFuture, 0" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should pass owned task.spawn then borrowed null source to task.race.
    exit /b 1
)
findstr /C:"xl6_task_all_values(xrtTypeInt()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing null-first task.all fixture over owned task.spawn.
    exit /b 1
)
findstr /C:"missingSpawnFuture, 0, xl6_task_spawn_values(addJob, xrtTypeInt(), sizeof(long long)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should pass borrowed null source before owned task.spawn to task.all/race.
    exit /b 1
)
findstr /C:"xvoCreateInt((long long)(160LL)), xvoCreateInt((long long)(1LL))), 1" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing owned task.spawn source after null source for task.all.
    exit /b 1
)
findstr /C:"nullFirstSpawnRace = xl6_future_await_i64_release(xl6_task_race_values" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing null-first task.race fixture over owned task.spawn.
    exit /b 1
)
findstr /C:"xvoCreateInt((long long)(170LL)), xvoCreateInt((long long)(1LL))), 1" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing owned task.spawn source after null source for task.race.
    exit /b 1
)

echo DEMO6 suite ok: task spawn cgen assertions 1/4
exit /b 0

