@echo off
setlocal EnableExtensions DisableDelayedExpansion
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: language frontend and type semantics...
if /I "%~1"=="after-null-abi" goto after_null_abi
if /I "%~1"=="after-default-backend" goto after_default_backend
if /I "%~1"=="xvalue-index-writes" goto xvalue_index_writes
if /I "%~1"=="typed-array-reads" goto typed_array_reads
if /I "%~1"=="after-generic-callsite" goto after_generic_callsite
if /I "%~1"=="after-native-nominal" goto after_native_nominal
if /I "%~1"=="after-nominal-index-write" goto after_nominal_index_write
if /I "%~1"=="after-array-iterator" goto after_array_iterator
if /I "%~1"=="after-dynamic-methods" goto after_dynamic_methods
if /I "%~1"=="after-u64" goto after_u64
if /I "%~1"=="after-type-value" goto after_type_value
if /I "%~1"=="after-determinism" goto after_determinism
if /I "%~1"=="after-numeric-literals" goto after_numeric_literals
if /I "%~1"=="type-name-identifiers" goto type_name_identifiers
if /I "%~1"=="after-persistence-examples" goto after_persistence_examples
if /I "%~1"=="low-level-ir" goto low_level_ir

rem XLIR production path must build directly from the semantic AST and emit
rem deterministic scalar C without using the legacy Cgen backend.
rem Exception-capable syntax must lower into canonical XLIR with its error
rem effect and result ABI fixed before C emission.  The generated C must only
rem materialize that ABI; it must not reconstruct exception control flow.
release\xl.exe ir test\cases\cgen_assert_ir_scalar.xl --lowered --emit build\test\ir_assert_scalar.lowered.xlir
if errorlevel 1 exit /b 1
findstr /C:"error-raise" build\test\ir_assert_scalar.lowered.xlir >nul
if errorlevel 1 exit /b 1
findstr /C:"throw" build\test\ir_assert_scalar.lowered.xlir >nul
if errorlevel 1 exit /b 1
release\xl.exe ir test\cases\cgen_assert_call_ir.xl --lowered --format json --emit build\test\cgen_assert_call_ir.json
if errorlevel 1 exit /b 1
findstr /C:"\"errorAbi\":\"may-raise\"" build\test\cgen_assert_call_ir.json >nul
if errorlevel 1 (
    echo XLIR did not close the assertion error effect over resolved internal calls.
    exit /b 1
)
findstr /C:"\"kindName\":\"error-test\"" build\test\cgen_assert_call_ir.json >nul
if errorlevel 1 (
    echo XLIR did not materialize the may-raise call result test CFG.
    exit /b 1
)
release\xl.exe cgen test\cases\cgen_assert_result_runtime.xl --backend xlir --emit build\test\cgen_assert_result_runtime_generated.c
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_assert_result_runtime.xl --backend xlir --emit build\test\cgen_assert_result_runtime_repeat.c
if errorlevel 1 exit /b 1
fc /b build\test\cgen_assert_result_runtime_generated.c build\test\cgen_assert_result_runtime_repeat.c >nul
if errorlevel 1 (
    echo XLIR may-raise C emission is not byte-for-byte deterministic.
    exit /b 1
)
findstr /C:"typedef struct xlir_result_i64" build\test\cgen_assert_result_runtime_generated.c >nul
if errorlevel 1 (
    echo XLIR may-raise C output is missing its explicit int64 result ABI.
    exit /b 1
)
findstr /C:"if (r" build\test\cgen_assert_result_runtime_generated.c >nul
if errorlevel 1 (
    echo XLIR may-raise C output is missing its lowered error-test branch.
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror -fsyntax-only build\test\cgen_assert_result_runtime_generated.c
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\cgen_assert_result_runtime_host.c %XL6_CGEN_LINKS% -o build\test\cgen_assert_result_runtime_host.exe
if errorlevel 1 exit /b 1
build\test\cgen_assert_result_runtime_host.exe
if errorlevel 1 exit /b 1
rem Generic calls must lower into distinct concrete callable ABIs before Cgen.
release\xl.exe cgen test\cases\cgen_generic_function.xl --backend xlir --emit build\test\xlir_generic_specialization_generated.c
if errorlevel 1 exit /b 1
findstr /C:"static int64_t xlir_fn_" build\test\xlir_generic_specialization_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"static char* xlir_fn_" build\test\xlir_generic_specialization_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtFree(s429" build\test\xlir_generic_specialization_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_generic_specialization_host.c %XL6_CGEN_LINKS% -o build\test\xlir_generic_specialization_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_generic_specialization_host.exe
if errorlevel 1 exit /b 1
rem Inferred generic calls use the same canonical monomorphization arena as
rem explicit ::<...> calls; Cgen sees only their concrete function ABIs.
release\xl.exe cgen test\cases\xlir_generic_inferred_scalar.xl --backend xlir --emit build\test\xlir_generic_inferred_scalar_generated.c
if errorlevel 1 exit /b 1
findstr /C:"static int64_t xlir_fn_" build\test\xlir_generic_inferred_scalar_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"static char* xlir_fn_" build\test\xlir_generic_inferred_scalar_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_generic_inferred_scalar_host.c %XL6_CGEN_LINKS% -o build\test\xlir_generic_inferred_scalar_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_generic_inferred_scalar_host.exe
if errorlevel 1 exit /b 1
rem Typed array generic inference uses the resolved xvalue container ABI and
rem an explicit scalar xvalue-count intrinsic for `.length`.
release\xl.exe cgen test\cases\xlir_generic_inferred_array.xl --backend xlir --emit build\test\xlir_generic_inferred_array_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xrtValueArray()" build\test\xlir_generic_inferred_array_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtValueCount(" build\test\xlir_generic_inferred_array_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_generic_inferred_array_host.c %XL6_CGEN_LINKS% -o build\test\xlir_generic_inferred_array_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_generic_inferred_array_host.exe
if errorlevel 1 exit /b 1
rem Sparse list literals lower as integer-keyed XRT maps; array rules must not
rem leak into their key or ownership ABI.
release\xl.exe cgen test\cases\xlir_generic_inferred_list.xl --backend xlir --emit build\test\xlir_generic_inferred_list_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xrtValueIntMap()" build\test\xlir_generic_inferred_list_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtValueIntMapSetNew(" build\test\xlir_generic_inferred_list_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_generic_inferred_list_host.c %XL6_CGEN_LINKS% -o build\test\xlir_generic_inferred_list_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_generic_inferred_list_host.exe
if errorlevel 1 exit /b 1
rem The broad aggregate gate fixes collection representation, typed projection,
rem managed payload ownership, failure propagation, and language-level cancel
rem normalization in verified XLIR before the C emitter is allowed to run.
release\xl.exe ir test\cases\cgen_task_aggregate_type_closure.xl --lowered --emit build\test\xlir_task_aggregate_type_closure.lowered.xlir
if errorlevel 1 exit /b 1
findstr /C:"runtime=xvalue-int-map-at-i64-integer" build\test\xlir_task_aggregate_type_closure.lowered.xlir >nul
if errorlevel 1 exit /b 1
findstr /C:"runtime=xvalue-set-has" build\test\xlir_task_aggregate_type_closure.lowered.xlir >nul
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_task_aggregate_type_closure.xl --backend xlir --emit build\test\xlir_task_aggregate_type_closure_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xlir_future_wait_checked" build\test\xlir_task_aggregate_type_closure_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"future cancel requested." build\test\xlir_task_aggregate_type_closure_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_task_aggregate_type_closure_host.c %XL6_CGEN_XLANG_LINKS% -o build\test\xlir_task_aggregate_type_closure_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_task_aggregate_type_closure_host.exe
if errorlevel 1 exit /b 1
rem Explicit and inferred calls with the same canonical type arguments reuse
rem one XLIR specialization rather than creating ABI-identical duplicates.
release\xl.exe cgen test\cases\cgen_generic_dedupe.xl --backend xlir --emit build\test\xlir_generic_dedupe_generated.c
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_generic_dedupe_host.c %XL6_CGEN_LINKS% -o build\test\xlir_generic_dedupe_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_generic_dedupe_host.exe
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_assert_dynamic_message_runtime.xl --backend xlir --emit build\test\cgen_assert_dynamic_message_runtime_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xrtSetErrorTake(xlir_error_create_at(" build\test\cgen_assert_dynamic_message_runtime_generated.c >nul
if errorlevel 1 (
    echo XLIR dynamic assertion message did not reach the fixed error runtime ABI.
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\cgen_assert_dynamic_message_runtime_host.c %XL6_CGEN_LINKS% -o build\test\cgen_assert_dynamic_message_runtime_host.exe
if errorlevel 1 exit /b 1
build\test\cgen_assert_dynamic_message_runtime_host.exe
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_try_catch_scalar.xl --backend xlir --emit build\test\xlir_try_catch_scalar_generated.c
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_try_catch_scalar.xl --backend xlir --emit build\test\xlir_try_catch_scalar_repeat.c
if errorlevel 1 exit /b 1
fc /b build\test\xlir_try_catch_scalar_generated.c build\test\xlir_try_catch_scalar_repeat.c >nul
if errorlevel 1 (
    echo XLIR try/catch C emission is not byte-for-byte deterministic.
    exit /b 1
)
findstr /C:"xrtTakeError();" build\test\xlir_try_catch_scalar_generated.c >nul
if errorlevel 1 (
    echo XLIR catch handler did not materialize its owned error payload ABI.
    exit /b 1
)
findstr /C:"xrtErrorFree(" build\test\xlir_try_catch_scalar_generated.c >nul
if errorlevel 1 (
    echo XLIR catch handler did not release its owned error payload on exit.
    exit /b 1
)
findstr /C:"if (r" build\test\xlir_try_catch_scalar_generated.c >nul
if errorlevel 1 (
    echo XLIR caught may-raise call did not preserve its explicit result test CFG.
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_try_catch_scalar_host.c %XL6_CGEN_LINKS% -o build\test\xlir_try_catch_scalar_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_try_catch_scalar_host.exe
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_try_catch_rethrow_unsupported.xl --backend xlir --emit build\test\xlir_try_catch_rethrow_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xrtSetErrorTake(v" build\test\xlir_try_catch_rethrow_generated.c >nul
if errorlevel 1 (
    echo XLIR catch rethrow did not materialize its ownership transfer ABI.
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_try_catch_rethrow_host.c %XL6_CGEN_LINKS% -o build\test\xlir_try_catch_rethrow_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_try_catch_rethrow_host.exe
if errorlevel 1 exit /b 1
rem A bare catch binding is an explicit borrowed error observer: it may be
rem evaluated and discarded, while its owning catch storage remains responsible
rem for exactly one xrtErrorFree on handler exit.
release\xl.exe cgen test\cases\xlir_catch_payload_access_unsupported.xl --backend xlir --emit build\test\xlir_catch_payload_access_unsupported_generated.c
if errorlevel 1 exit /b 1
findstr /C:"(void)v" build\test\xlir_catch_payload_access_unsupported_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtErrorFree(s" build\test\xlir_catch_payload_access_unsupported_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_catch_payload_access_unsupported_host.c %XL6_CGEN_LINKS% -o build\test\xlir_catch_payload_access_unsupported_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_catch_payload_access_unsupported_host.exe
if errorlevel 1 exit /b 1
rem Catch error.message is a typed runtime operation: it copies borrowed XRT
rem text before the handler releases its owned xerror storage.
release\xl.exe cgen test\cases\xlir_catch_error_message.xl --backend xlir --emit build\test\xlir_catch_error_message_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xrtStrDup(xrtErrorMessage(v" build\test\xlir_catch_error_message_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtErrorFree(s" build\test\xlir_catch_error_message_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_catch_error_message_host.c %XL6_CGEN_LINKS% -o build\test\xlir_catch_error_message_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_catch_error_message_host.exe
if errorlevel 1 exit /b 1
rem Catch error.code is an i64 XLIR scalar operation over borrowed xerror
rem storage; the handler remains sole owner of that storage.
release\xl.exe cgen test\cases\xlir_catch_error_code.xl --backend xlir --emit build\test\xlir_catch_error_code_generated.c
if errorlevel 1 exit /b 1
findstr /C:"(int64_t)xrtErrorCode(v" build\test\xlir_catch_error_code_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"typedef struct xvalue xvalue;" build\test\xlir_catch_error_code_generated.c >nul
if not errorlevel 1 exit /b 1
findstr /C:"xrtErrorFree(s" build\test\xlir_catch_error_code_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_catch_error_code_host.c %XL6_CGEN_LINKS% -o build\test\xlir_catch_error_code_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_catch_error_code_host.exe
if errorlevel 1 exit /b 1
rem Stable XRT error metadata fields lower to dedicated intrinsics and owned
rem strings/scalars; no source-level member dispatch remains in Cgen.
release\xl.exe cgen test\cases\xlir_catch_error_runtime_fields.xl --backend xlir --emit build\test\xlir_catch_error_runtime_fields_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xrtStrDup(xrtErrorDomain(v" build\test\xlir_catch_error_runtime_fields_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtStrDup(xrtErrorOperation(v" build\test\xlir_catch_error_runtime_fields_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtStrDup(xrtErrorData(v" build\test\xlir_catch_error_runtime_fields_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"(int64_t)xrtErrorSystemCode(v" build\test\xlir_catch_error_runtime_fields_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_catch_error_runtime_fields_host.c %XL6_CGEN_LINKS% -o build\test\xlir_catch_error_runtime_fields_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_catch_error_runtime_fields_host.exe
if errorlevel 1 exit /b 1
rem error.cause explicitly retains the borrowed XRT cause before assigning it
rem to local storage, whose normal handler exit releases that owned reference.
release\xl.exe cgen test\cases\xlir_catch_error_cause_ref.xl --backend xlir --emit build\test\xlir_catch_error_cause_ref_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xrtErrorRef(xrtErrorCause(v" build\test\xlir_catch_error_cause_ref_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtStrDup(xrtErrorDomain(v" build\test\xlir_catch_error_cause_ref_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtErrorFree(s" build\test\xlir_catch_error_cause_ref_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_catch_error_cause_ref_host.c %XL6_CGEN_LINKS% -o build\test\xlir_catch_error_cause_ref_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_catch_error_cause_ref_host.exe
if errorlevel 1 exit /b 1
rem Error kind is carried through the XRT bridge as validated metadata and is
rem materialized by a dedicated XLIR runtime operation.
release\xl.exe cgen test\cases\xlir_catch_error_kind.xl --backend xlir --emit build\test\xlir_catch_error_kind_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xlir_error_create_at(\"ValueError\"" build\test\xlir_catch_error_kind_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtStrDup(xlir_error_kind_name(v" build\test\xlir_catch_error_kind_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_catch_error_kind_host.c %XL6_CGEN_LINKS% -o build\test\xlir_catch_error_kind_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_catch_error_kind_host.exe
if errorlevel 1 exit /b 1
rem A typed catch is lowered as ERROR_TAKE, explicit kind predicate branch,
rem matched handler, and unmatched rethrow -- never as Cgen catch syntax.
release\xl.exe cgen test\cases\xlir_typed_catch_value_error.xl --backend xlir --emit build\test\xlir_typed_catch_value_error_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xlir_error_kind_is(v" build\test\xlir_typed_catch_value_error_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtSetErrorTake(v" build\test\xlir_typed_catch_value_error_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_typed_catch_value_error_host.c %XL6_CGEN_LINKS% -o build\test\xlir_typed_catch_value_error_host.exe
if errorlevel 1 exit /b 1
  build\test\xlir_typed_catch_value_error_host.exe
  if errorlevel 1 exit /b 1
  rem Typed handler tests follow the canonical xlang error ancestry, not just
  rem equality of the immediate XRT/native kind name.
  release\xl.exe cgen test\cases\xlir_typed_catch_hierarchy.xl --backend xlir --emit build\test\xlir_typed_catch_hierarchy_generated.c
  if errorlevel 1 exit /b 1
  findstr /C:"xlir_error_parent_kind" build\test\xlir_typed_catch_hierarchy_generated.c >nul
  if errorlevel 1 exit /b 1
  "%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_typed_catch_hierarchy_host.c %XL6_CGEN_LINKS% -o build\test\xlir_typed_catch_hierarchy_host.exe
  if errorlevel 1 exit /b 1
  build\test\xlir_typed_catch_hierarchy_host.exe
  if errorlevel 1 exit /b 1
  rem Typed handlers and finally use one explicit XLIR error predicate and
  rem separate matched/unmatched routes into the lowered finally CFG.
  release\xl.exe cgen test\cases\xlir_typed_catch_finally.xl --backend xlir --emit build\test\xlir_typed_catch_finally_generated.c
  if errorlevel 1 exit /b 1
  findstr /C:"xlir_error_kind_is(v" build\test\xlir_typed_catch_finally_generated.c >nul
  if errorlevel 1 exit /b 1
  "%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_typed_catch_finally_host.c %XL6_CGEN_LINKS% -o build\test\xlir_typed_catch_finally_host.exe
  if errorlevel 1 exit /b 1
  build\test\xlir_typed_catch_finally_host.exe
  if errorlevel 1 exit /b 1
  rem A typed-handler miss executes its own finally before it reaches the
  rem already-resolved outer XLIR exception target.
  release\xl.exe cgen test\cases\xlir_typed_catch_finally_outer_route.xl --backend xlir --emit build\test\xlir_typed_catch_finally_outer_route_generated.c
  if errorlevel 1 exit /b 1
  findstr /C:"xrtSetErrorTake(v" build\test\xlir_typed_catch_finally_outer_route_generated.c >nul
  if errorlevel 1 exit /b 1
  "%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_typed_catch_finally_outer_route_host.c %XL6_CGEN_LINKS% -o build\test\xlir_typed_catch_finally_outer_route_host.exe
  if errorlevel 1 exit /b 1
  build\test\xlir_typed_catch_finally_outer_route_host.exe
  if errorlevel 1 exit /b 1
  rem Multi-handler dispatch and finally are one XLIR CFG: only the matching
  rem body owns the error, and its normal path enters the shared finally body.
  release\xl.exe cgen test\cases\xlir_multi_typed_catch_finally.xl --backend xlir --emit build\test\xlir_multi_typed_catch_finally_generated.c
  if errorlevel 1 exit /b 1
  findstr /C:"xlir_error_kind_is(v" build\test\xlir_multi_typed_catch_finally_generated.c >nul
  if errorlevel 1 exit /b 1
  "%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_multi_typed_catch_finally_host.c %XL6_CGEN_LINKS% -o build\test\xlir_multi_typed_catch_finally_host.exe
  if errorlevel 1 exit /b 1
  build\test\xlir_multi_typed_catch_finally_host.exe
  if errorlevel 1 exit /b 1
  rem An unmatched multi-handler sequence restores the error, executes its
  rem finally, and then takes the outer XLIR exception target.
  release\xl.exe cgen test\cases\xlir_multi_typed_catch_finally_outer_route.xl --backend xlir --emit build\test\xlir_multi_typed_catch_finally_outer_route_generated.c
  if errorlevel 1 exit /b 1
findstr /C:"xrtSetErrorTake(v" build\test\xlir_multi_typed_catch_finally_outer_route_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtStrDup(xlir_error_kind_name(v" build\test\xlir_multi_typed_catch_finally_outer_route_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_multi_typed_catch_finally_outer_route_host.c %XL6_CGEN_LINKS% -o build\test\xlir_multi_typed_catch_finally_outer_route_host.exe
  if errorlevel 1 exit /b 1
  build\test\xlir_multi_typed_catch_finally_outer_route_host.exe
  if errorlevel 1 exit /b 1
  rem Return from a selected multi-handler is stored in the typed XLIR
  rem pending-exit slot and only materializes after its return-finally block.
  release\xl.exe cgen test\cases\xlir_multi_typed_catch_finally_return.xl --backend xlir --emit build\test\xlir_multi_typed_catch_finally_return_generated.c
  if errorlevel 1 exit /b 1
  findstr /C:"s429496729" build\test\xlir_multi_typed_catch_finally_return_generated.c >nul
  if errorlevel 1 exit /b 1
  "%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_multi_typed_catch_finally_return_host.c %XL6_CGEN_LINKS% -o build\test\xlir_multi_typed_catch_finally_return_host.exe
  if errorlevel 1 exit /b 1
  build\test\xlir_multi_typed_catch_finally_return_host.exe
  if errorlevel 1 exit /b 1
  rem A break from either multi-handler body is a resolved loop target routed
  rem through the XLIR finally barrier, never a Cgen-managed pending flag.
  release\xl.exe cgen test\cases\xlir_multi_typed_catch_finally_break.xl --backend xlir --emit build\test\xlir_multi_typed_catch_finally_break_generated.c
  if errorlevel 1 exit /b 1
  findstr /C:"goto b" build\test\xlir_multi_typed_catch_finally_break_generated.c >nul
  if errorlevel 1 exit /b 1
  "%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_multi_typed_catch_finally_break_host.c %XL6_CGEN_LINKS% -o build\test\xlir_multi_typed_catch_finally_break_host.exe
  if errorlevel 1 exit /b 1
  build\test\xlir_multi_typed_catch_finally_break_host.exe
  if errorlevel 1 exit /b 1
  rem Continue takes the other resolved loop edge through the same finally
  rem barrier and returns to the XLIR loop condition after the finally body.
  release\xl.exe cgen test\cases\xlir_multi_typed_catch_finally_continue.xl --backend xlir --emit build\test\xlir_multi_typed_catch_finally_continue_generated.c
  if errorlevel 1 exit /b 1
  findstr /C:"goto b" build\test\xlir_multi_typed_catch_finally_continue_generated.c >nul
  if errorlevel 1 exit /b 1
  "%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_multi_typed_catch_finally_continue_host.c %XL6_CGEN_LINKS% -o build\test\xlir_multi_typed_catch_finally_continue_host.exe
  if errorlevel 1 exit /b 1
  build\test\xlir_multi_typed_catch_finally_continue_host.exe
  if errorlevel 1 exit /b 1
  rem Managed string returns move through the multi-handler pending-return
  rem storage before the return-finally CFG exposes the function result ABI.
  release\xl.exe cgen test\cases\xlir_multi_typed_catch_finally_string_return.xl --backend xlir --emit build\test\xlir_multi_typed_catch_finally_string_return_generated.c
  if errorlevel 1 exit /b 1
  findstr /C:"xrtFree(s429496729" build\test\xlir_multi_typed_catch_finally_string_return_generated.c >nul
  if errorlevel 1 exit /b 1
  "%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_multi_typed_catch_finally_string_return_host.c %XL6_CGEN_LINKS% -o build\test\xlir_multi_typed_catch_finally_string_return_host.exe
  if errorlevel 1 exit /b 1
  build\test\xlir_multi_typed_catch_finally_string_return_host.exe
  if errorlevel 1 exit /b 1
  rem Multiple typed handlers share dispatch-owned error storage, transfer it to
rem exactly one matching handler, and never require a Cgen handler loop.
release\xl.exe cgen test\cases\xlir_multi_typed_catch.xl --backend xlir --emit build\test\xlir_multi_typed_catch_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xlir_error_kind_is(v" build\test\xlir_multi_typed_catch_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtSetErrorTake(v" build\test\xlir_multi_typed_catch_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_multi_typed_catch_host.c %XL6_CGEN_LINKS% -o build\test\xlir_multi_typed_catch_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_multi_typed_catch_host.exe
if errorlevel 1 exit /b 1
rem A typed-handler miss restores XRT error ownership and jumps to an active
rem outer XLIR exception target instead of returning directly from Cgen.
release\xl.exe cgen test\cases\xlir_typed_catch_outer_route.xl --backend xlir --emit build\test\xlir_typed_catch_outer_route_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xrtSetErrorTake(v" build\test\xlir_typed_catch_outer_route_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_typed_catch_outer_route_host.c %XL6_CGEN_LINKS% -o build\test\xlir_typed_catch_outer_route_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_typed_catch_outer_route_host.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\xlir_try_finally_scalar.xl --backend xlir --emit build\test\xlir_try_finally_scalar_generated.c
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_try_finally_scalar.xl --backend xlir --emit build\test\xlir_try_finally_scalar_repeat.c
if errorlevel 1 exit /b 1
fc /b build\test\xlir_try_finally_scalar_generated.c build\test\xlir_try_finally_scalar_repeat.c >nul
if errorlevel 1 (
    echo XLIR try/finally C emission is not byte-for-byte deterministic.
    exit /b 1
)
findstr /C:"xrtSetErrorTake(" build\test\xlir_try_finally_scalar_generated.c >nul
if errorlevel 1 (
    echo XLIR try/finally did not preserve its explicit error-state ABI.
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_try_finally_scalar_host.c %XL6_CGEN_LINKS% -o build\test\xlir_try_finally_scalar_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_try_finally_scalar_host.exe
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_try_finally_return_scalar.xl --backend xlir --emit build\test\xlir_try_finally_return_scalar_generated.c
if errorlevel 1 exit /b 1
findstr /C:"s4294967295" build\test\xlir_try_finally_return_scalar_generated.c >nul
if errorlevel 1 (
    echo XLIR return-through-finally did not materialize its pending scalar value ABI.
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_try_finally_return_scalar_host.c %XL6_CGEN_LINKS% -o build\test\xlir_try_finally_return_scalar_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_try_finally_return_scalar_host.exe
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_nested_try_finally_return_scalar.xl --backend xlir --emit build\test\xlir_nested_try_finally_return_scalar_generated.c
if errorlevel 1 exit /b 1
findstr /C:"s4294967295" build\test\xlir_nested_try_finally_return_scalar_generated.c >nul
if errorlevel 1 (
    echo XLIR nested return-through-finally did not materialize pending scalar storage.
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_nested_try_finally_return_scalar_host.c %XL6_CGEN_LINKS% -o build\test\xlir_nested_try_finally_return_scalar_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_nested_try_finally_return_scalar_host.exe
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_try_finally_string_return.xl --backend xlir --emit build\test\xlir_try_finally_string_return_generated.c
if errorlevel 1 exit /b 1
findstr /C:"s4294967295 = NULL;" build\test\xlir_try_finally_string_return_generated.c >nul
if errorlevel 1 (
    echo XLIR managed string return-through-finally did not transfer pending ownership.
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_try_finally_string_return_host.c %XL6_CGEN_LINKS% -o build\test\xlir_try_finally_string_return_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_try_finally_string_return_host.exe
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_try_finally_xvalue_return.xl --backend xlir --emit build\test\xlir_try_finally_xvalue_return_generated.c
if errorlevel 1 exit /b 1
findstr /C:"s4294967295 = NULL;" build\test\xlir_try_finally_xvalue_return_generated.c >nul
if errorlevel 1 (
    echo XLIR xvalue return-through-finally did not transfer pending ownership.
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_try_finally_xvalue_return_host.c %XL6_CGEN_LINKS% -o build\test\xlir_try_finally_xvalue_return_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_try_finally_xvalue_return_host.exe
if errorlevel 1 exit /b 1
rem A conditional typed dict is first converted to xvalue in its CFG branches,
rem then moved through the xvalue pending-return slot of the finally route.
release\xl.exe cgen test\cases\xlir_conditional_xvalue_finally.xl --backend xlir --emit build\test\xlir_conditional_xvalue_finally_generated.c
if errorlevel 1 exit /b 1
findstr /C:"s4294967295 = NULL;" build\test\xlir_conditional_xvalue_finally_generated.c >nul
if errorlevel 1 exit /b 1
powershell -NoProfile -Command "if ((Select-String -SimpleMatch '= xrtValueObject();' 'build\test\xlir_conditional_xvalue_finally_generated.c').Count -ne 2) { exit 1 }"
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_conditional_xvalue_finally_host.c %XL6_CGEN_LINKS% -o build\test\xlir_conditional_xvalue_finally_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_conditional_xvalue_finally_host.exe
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_nested_try_finally_string_return.xl --backend xlir --emit build\test\xlir_nested_try_finally_string_return_generated.c
if errorlevel 1 exit /b 1
findstr /C:"s4294967295 = NULL;" build\test\xlir_nested_try_finally_string_return_generated.c >nul
if errorlevel 1 (
    echo XLIR nested managed return-through-finally did not transfer pending ownership.
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_nested_try_finally_string_return_host.c %XL6_CGEN_LINKS% -o build\test\xlir_nested_try_finally_string_return_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_nested_try_finally_string_return_host.exe
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_try_finally_trivial_local.xl --backend xlir --emit build\test\xlir_try_finally_trivial_local_generated.c
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_try_finally_trivial_local_host.c %XL6_CGEN_LINKS% -o build\test\xlir_try_finally_trivial_local_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_try_finally_trivial_local_host.exe
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_try_finally_managed_local.xl --backend xlir --emit build\test\xlir_try_finally_managed_local_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xrtFree(s" build\test\xlir_try_finally_managed_local_generated.c >nul
if errorlevel 1 (
    echo XLIR managed finally local did not materialize a route-local release.
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_try_finally_managed_local_host.c %XL6_CGEN_LINKS% -o build\test\xlir_try_finally_managed_local_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_try_finally_managed_local_host.exe
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_finally_nested_managed_local.xl --backend xlir --emit build\test\xlir_finally_nested_managed_local_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xrtFree(s" build\test\xlir_finally_nested_managed_local_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_finally_nested_managed_local_host.c %XL6_CGEN_LINKS% -o build\test\xlir_finally_nested_managed_local_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_finally_nested_managed_local_host.exe
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_future_handle_lifetime.xl --backend xlir --emit build\test\xlir_future_handle_lifetime_generated.c
if errorlevel 1 exit /b 1
findstr /C:"typedef struct xfuture xfuture;" build\test\xlir_future_handle_lifetime_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtFutureDestroy(s" build\test\xlir_future_handle_lifetime_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_future_handle_lifetime_host.c %XL6_CGEN_LINKS% -o build\test\xlir_future_handle_lifetime_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_future_handle_lifetime_host.exe
if errorlevel 1 exit /b 1
rem A pure, non-raising async return produces a ready Future.  Its worker is
rem verified XLIR and resolves through XRT without occupying the task pool.
release\xl.exe ir test\cases\xlir_async_task_scalar.xl --lowered --emit build\test\xlir_async_task_scalar.lowered.xlir
if errorlevel 1 exit /b 1
findstr /C:"async-start" build\test\xlir_async_task_scalar.lowered.xlir >nul
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_async_task_scalar.xl --backend xlir --emit build\test\xlir_async_task_scalar_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xrtPromiseCreate(&future, NULL)" build\test\xlir_async_task_scalar_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtTaskSubmit" build\test\xlir_async_task_scalar_generated.c >nul
if not errorlevel 1 (
    echo XLIR ready Future scalar worker unexpectedly submitted an XRT task
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_async_task_scalar_host.c %XL6_CGEN_LINKS% -o build\test\xlir_async_task_scalar_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_async_task_scalar_host.exe
if errorlevel 1 exit /b 1
rem Differential runtime gate: scalar and managed-error fixtures must produce
rem canonical generated C across the broad migrated runtime fixture matrix.
rem Sources: test\cases\xlir_cgen_differential_scalar.xl,
rem test\cases\xlir_cgen_differential_managed_error.xl and
rem test\cases\xlir_cgen_differential_xvalue_conditional.xl;
rem async sources: test\cases\xlir_async_ready_differential.xl,
rem test\cases\xlir_await_future_xvalue.xl,
rem test\cases\xlir_async_future_xvalue_differential.xl,
rem test\cases\xlir_async_future_xvalue_local_parameter_differential.xl and
rem test\cases\xlir_async_future_direct_xvalue_differential.xl.
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_cgen_runtime_matrix.ps1
if errorlevel 1 exit /b 1
rem Keep an explicit semantic-cluster admission matrix in front of the broad
rem deterministic-C gate below; this prevents XLIR coverage from silently
rem shrinking while individual generated-C checks still pass.
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_export_boundary.ps1
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_global_export_boundary.ps1
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_extern_global_boundary.ps1
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_null_abi_boundary.ps1
if errorlevel 1 exit /b 1
:after_null_abi
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_migration_coverage.ps1
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_persistence_json_abi.ps1
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_persistence_recursive_class_values.ps1
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_persistence_projection_roots.ps1
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_persistence_recursive_collection_values.ps1
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_foreign_function_value_abi.ps1
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_foreign_string_abi.ps1
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_string_bridges.ps1
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_task_spawn_closure_abi.ps1
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_thread_resource_abi.ps1
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_channel_resource_abi.ps1
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_sync_resource_abi.ps1
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_special_queue_resource_abi.ps1
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_stack_resource_abi.ps1
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_avl_resource_abi.ps1
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_future_continuation_abi.ps1
if errorlevel 1 exit /b 1
rem The default-backend differential is a parameterized public-source worker.
rem run_xlir_guards.ps1 supplies its immutable compiler directory and private
rem evidence directory; never invoke it here without those scenario inputs.
:after_default_backend
if /I "%~2"=="trace" echo on
rem A non-suspending async worker may raise through a verified scalar result
rem carrier.  The immediate launcher must reject the Future from that carrier,
rem without a task-pool frame or reconstructed worker control flow in Cgen.
release\xl.exe cgen test\cases\xlir_async_task_worker_error.xl --backend xlir --emit build\test\xlir_async_task_worker_error_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xlir_result_i64 worker = xlir_fn_1();" build\test\xlir_async_task_worker_error_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtPromiseReject(promise, error)" build\test\xlir_async_task_worker_error_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtTaskSubmit" build\test\xlir_async_task_worker_error_generated.c >nul
if not errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_async_task_worker_error_host.c %XL6_CGEN_LINKS% -o build\test\xlir_async_task_worker_error_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_async_task_worker_error_host.exe
if errorlevel 1 exit /b 1
rem Await must bridge a failed worker Future into the caller-thread XRT error slot,
rem so the normal XLIR ERROR_TEST/typed-catch CFG observes the original error.
release\xl.exe cgen test\cases\xlir_async_task_worker_error_catch.xl --backend xlir --emit build\test\xlir_async_task_worker_error_catch_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xrtFutureError(future)" build\test\xlir_async_task_worker_error_catch_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtSetErrorTake(xrtErrorRef(error))" build\test\xlir_async_task_worker_error_catch_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_async_task_worker_error_catch_host.c %XL6_CGEN_LINKS% -o build\test\xlir_async_task_worker_error_catch_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_async_task_worker_error_catch_host.exe
if errorlevel 1 exit /b 1
rem A managed immediate worker error carries xlir_result_string through the
rem explicit launcher ABI and rejects without manufacturing a string value.
release\xl.exe cgen test\cases\xlir_async_task_string_worker_error.xl --backend xlir --emit build\test\xlir_async_task_string_worker_error_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xlir_result_string worker = xlir_fn_1();" build\test\xlir_async_task_string_worker_error_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtPromiseReject(promise, error)" build\test\xlir_async_task_string_worker_error_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_async_task_string_worker_error_host.c %XL6_CGEN_LINKS% -o build\test\xlir_async_task_string_worker_error_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_async_task_string_worker_error_host.exe
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_async_task_xvalue_worker_error.xl --backend xlir --emit build\test\xlir_async_task_xvalue_worker_error_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xlir_result_xvalue worker = xlir_fn_1();" build\test\xlir_async_task_xvalue_worker_error_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"return (xlir_result_xvalue){ false };" build\test\xlir_async_task_xvalue_worker_error_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_async_task_xvalue_worker_error_host.c %XL6_CGEN_LINKS% -o build\test\xlir_async_task_xvalue_worker_error_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_async_task_xvalue_worker_error_host.exe
if errorlevel 1 exit /b 1
rem A managed result carrier must transfer success ownership through an
rem ordinary may-raise call and preserve the failure branch for catch.
release\xl.exe cgen test\cases\xlir_error_result_string_call.xl --backend xlir --emit build\test\xlir_error_result_string_call_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xlir_result_string r" build\test\xlir_error_result_string_call_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"return (xlir_result_string){ false };" build\test\xlir_error_result_string_call_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_error_result_string_call_host.c %XL6_CGEN_LINKS% -o build\test\xlir_error_result_string_call_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_error_result_string_call_host.exe
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_error_result_xvalue_call.xl --backend xlir --emit build\test\xlir_error_result_xvalue_call_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xlir_result_xvalue r" build\test\xlir_error_result_xvalue_call_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"return (xlir_result_xvalue){ false };" build\test\xlir_error_result_xvalue_call_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_error_result_xvalue_call_host.c %XL6_CGEN_LINKS% -o build\test\xlir_error_result_xvalue_call_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_error_result_xvalue_call_host.exe
if errorlevel 1 exit /b 1
rem An exported may-raise scalar function retains its public C scalar ABI;
rem the adapter observes the verified result carrier and preserves xrt error.
release\xl.exe cgen test\cases\xlir_export_error_scalar.xl --backend xlir --emit build\test\xlir_export_error_scalar_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xlir_result_i64 result = xlir_fn_" build\test\xlir_export_error_scalar_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"return result.ok ? result.value : (int64_t){0};" build\test\xlir_export_error_scalar_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_export_error_scalar_host.c %XL6_CGEN_LINKS% -o build\test\xlir_export_error_scalar_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_export_error_scalar_host.exe
if errorlevel 1 exit /b 1
rem Void may-raise exports use the same explicit result ABI internally while
rem retaining a void C boundary and the runtime error context.
release\xl.exe cgen test\cases\xlir_export_error_void.xl --backend xlir --emit build\test\xlir_export_error_void_generated.c
if errorlevel 1 exit /b 1
findstr /C:"typedef struct xlir_result_void { bool ok; }" build\test\xlir_export_error_void_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xlir_result_void result = xlir_fn_" build\test\xlir_export_error_void_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_export_error_void_host.c %XL6_CGEN_LINKS% -o build\test\xlir_export_error_void_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_export_error_void_host.exe
if errorlevel 1 exit /b 1
release\xl.exe ir test\cases\xlir_async_task_scalar_params.xl --lowered --emit build\test\xlir_async_task_scalar_params.lowered.xlir
if errorlevel 1 exit /b 1
findstr /C:"async-start" build\test\xlir_async_task_scalar_params.lowered.xlir >nul
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_async_task_scalar_params.xl --backend xlir --emit build\test\xlir_async_task_scalar_params_generated.c
if errorlevel 1 exit /b 1
findstr /C:"*value = xlir_fn_1(a0, a1);" build\test\xlir_async_task_scalar_params_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtTaskSubmit" build\test\xlir_async_task_scalar_params_generated.c >nul
if not errorlevel 1 (
    echo XLIR ready Future scalar parameters unexpectedly submitted an XRT task
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_async_task_scalar_params_host.c %XL6_CGEN_LINKS% -o build\test\xlir_async_task_scalar_params_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_async_task_scalar_params_host.exe
if errorlevel 1 exit /b 1
release\xl.exe ir test\cases\xlir_async_task_future_param.xl --lowered --emit build\test\xlir_async_task_future_param.lowered.xlir
if errorlevel 1 exit /b 1
findstr /C:"async-start" build\test\xlir_async_task_future_param.lowered.xlir >nul
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_async_task_future_param.xl --backend xlir --emit build\test\xlir_async_task_future_param_generated.c
if errorlevel 1 exit /b 1
findstr /C:"*value = xlir_fn_2(a0);" build\test\xlir_async_task_future_param_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtTaskSubmit" build\test\xlir_async_task_future_param_generated.c >nul
if not errorlevel 1 (
    echo XLIR ready Future ignored-future worker unexpectedly submitted an XRT task
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_async_task_future_param_host.c %XL6_CGEN_LINKS% -o build\test\xlir_async_task_future_param_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_async_task_future_param_host.exe
if errorlevel 1 exit /b 1
rem A pure local dynamic result is computed before launch completes and its
rem owned value is transferred directly into a ready Future.
release\xl.exe ir test\cases\xlir_async_task_xvalue_result.xl --lowered --emit build\test\xlir_async_task_xvalue_result.lowered.xlir
if errorlevel 1 exit /b 1
findstr /C:"async-start" build\test\xlir_async_task_xvalue_result.lowered.xlir >nul
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_async_task_xvalue_result.xl --backend xlir --emit build\test\xlir_async_task_xvalue_result_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xrtPromiseResolveOwned(promise, value, xlir_async_xvalue_free, NULL)" build\test\xlir_async_task_xvalue_result_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtTaskSubmit" build\test\xlir_async_task_xvalue_result_generated.c >nul
if not errorlevel 1 (
    echo XLIR ready Future local dynamic result unexpectedly submitted an XRT task
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_async_task_xvalue_result_host.c %XL6_CGEN_LINKS% -o build\test\xlir_async_task_xvalue_result_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_async_task_xvalue_result_host.exe
if errorlevel 1 exit /b 1
rem A pure string literal worker resolves an owned string directly into a
rem ready Future; its destroy callback remains explicit without a task frame.
release\xl.exe cgen test\cases\xlir_async_task_string_result.xl --backend xlir --emit build\test\xlir_async_task_string_result_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xrtPromiseResolveOwned(promise, value, xlir_async_string_free, NULL)" build\test\xlir_async_task_string_result_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtTaskSubmit" build\test\xlir_async_task_string_result_generated.c >nul
if not errorlevel 1 (
    echo XLIR ready Future string result unexpectedly submitted an XRT task
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_async_task_string_result_host.c %XL6_CGEN_LINKS% -o build\test\xlir_async_task_string_result_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_async_task_string_result_host.exe
if errorlevel 1 exit /b 1
rem A ready string worker borrows its parameter only for the synchronous
rem verified call, then transfers the returned owned string into its Future.
release\xl.exe cgen test\cases\xlir_async_task_string_param.xl --backend xlir --emit build\test\xlir_async_task_string_param_generated.c
if errorlevel 1 exit /b 1
findstr /C:"*value = xlir_fn_1(a0);" build\test\xlir_async_task_string_param_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtTaskSubmit" build\test\xlir_async_task_string_param_generated.c >nul
if not errorlevel 1 (
    echo XLIR ready Future string worker unexpectedly submitted an XRT task
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_async_task_string_param_host.c %XL6_CGEN_LINKS% -o build\test\xlir_async_task_string_param_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_async_task_string_param_host.exe
if errorlevel 1 exit /b 1
rem A ready dynamic worker transfers the worker's retained result directly
rem into its Future and needs no deferred task frame.
release\xl.exe cgen test\cases\xlir_async_task_xvalue_param.xl --backend xlir --emit build\test\xlir_async_task_xvalue_param_generated.c
if errorlevel 1 exit /b 1
findstr /C:"*value = xlir_fn_1(a0);" build\test\xlir_async_task_xvalue_param_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtTaskSubmit" build\test\xlir_async_task_xvalue_param_generated.c >nul
if not errorlevel 1 (
    echo XLIR ready Future dynamic worker unexpectedly submitted an XRT task
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_async_task_xvalue_param_host.c %XL6_CGEN_LINKS% -o build\test\xlir_async_task_xvalue_param_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_async_task_xvalue_param_host.exe
if errorlevel 1 exit /b 1
rem Awaiting a managed string must clone the Future's result-slot payload;
rem later cleanup owns that clone rather than borrowing Future storage.
release\xl.exe cgen test\cases\xlir_await_future_string.xl --backend xlir --emit build\test\xlir_await_future_string_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xlir_future_await_string" build\test\xlir_await_future_string_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"return value != NULL && *value != NULL ? xrtStrDup(*value) : NULL;" build\test\xlir_await_future_string_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_await_future_string_host.c %XL6_CGEN_LINKS% -o build\test\xlir_await_future_string_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_await_future_string_host.exe
if errorlevel 1 exit /b 1
rem Awaiting a dynamic value retains the object held in the Future result
rem slot, so subsequent local ownership is independent from Future teardown.
release\xl.exe cgen test\cases\xlir_await_future_xvalue.xl --backend xlir --emit build\test\xlir_await_future_xvalue_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xlir_future_await_xvalue" build\test\xlir_await_future_xvalue_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"return value != NULL && *value != NULL ? xrtValueRetain(*value) : NULL;" build\test\xlir_await_future_xvalue_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_await_future_xvalue_host.c %XL6_CGEN_LINKS% -o build\test\xlir_await_future_xvalue_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_await_future_xvalue_host.exe
if errorlevel 1 exit /b 1
rem A canonical async `return await futureParam` is one suspension in the
rem verified machine. It must continue the input Future without a task-pool or
rem blocking worker fallback.
release\xl.exe ir test\cases\xlir_async_future_forward.xl --lowered --emit build\test\xlir_async_future_forward.lowered.xlir
if errorlevel 1 exit /b 1
findstr /C:"async-start" build\test\xlir_async_future_forward.lowered.xlir >nul
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_async_future_forward.xl --backend xlir --emit build\test\xlir_async_future_forward_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xlir_async_machine_resume_2_0" build\test\xlir_async_future_forward_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtFutureContinueOwned" build\test\xlir_async_future_forward_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xlir_async_machine_frame_free_2" build\test\xlir_async_future_forward_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtTaskSubmit" build\test\xlir_async_future_forward_generated.c >nul
if not errorlevel 1 (
    echo XLIR async future-forward unexpectedly submitted an XRT task
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_async_future_forward_host.c %XL6_CGEN_LINKS% -o build\test\xlir_async_future_forward_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_async_future_forward_host.exe
if errorlevel 1 exit /b 1
rem A single await followed by a scalar operation is represented by an XLIR
rem continuation ABI.  The source Future must resume through ThenOwned, never
rem through the task-pool worker path.
release\xl.exe ir test\cases\xlir_async_future_then_scalar.xl --lowered --emit build\test\xlir_async_future_then_scalar.lowered.xlir
if errorlevel 1 exit /b 1
findstr /C:"async-start" build\test\xlir_async_future_then_scalar.lowered.xlir >nul
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_async_future_then_scalar.xl --backend xlir --emit build\test\xlir_async_future_then_scalar_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xlir_async_machine_resume_2_0" build\test\xlir_async_future_then_scalar_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtFutureContinueOwned" build\test\xlir_async_future_then_scalar_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xlir_async_machine_frame_free_2" build\test\xlir_async_future_then_scalar_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtTaskSubmit" build\test\xlir_async_future_then_scalar_generated.c >nul
if not errorlevel 1 (
    echo XLIR async scalar continuation unexpectedly submitted an XRT task
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_async_future_then_scalar_host.c %XL6_CGEN_LINKS% -o build\test\xlir_async_future_then_scalar_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_async_future_then_scalar_host.exe
if errorlevel 1 exit /b 1
rem Dynamic continuations own their xvalue capture in the frame and invoke a
rem fixed XLIR runtime-op ABI, never pointer arithmetic or legacy AST Cgen.
release\xl.exe cgen test\cases\xlir_async_future_then_xvalue.xl --backend xlir --emit build\test\xlir_async_future_then_xvalue_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xlir_async_machine_frame_1" build\test\xlir_async_future_then_xvalue_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtValueRetain(a1)" build\test\xlir_async_future_then_xvalue_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xlir_xvalue_binary(" build\test\xlir_async_future_then_xvalue_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtTaskSubmit" build\test\xlir_async_future_then_xvalue_generated.c >nul
if not errorlevel 1 (
    echo XLIR async dynamic continuation unexpectedly submitted an XRT task
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_async_future_then_xvalue_host.c %XL6_CGEN_LINKS% -o build\test\xlir_async_future_then_xvalue_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_async_future_then_xvalue_host.exe
if errorlevel 1 exit /b 1
rem A pre-await dynamic local becomes an explicit declaration owner before its
rem owned value is transferred into the canonical machine frame; failure and
rem cancellation must cross the continuation unchanged.
release\xl.exe cgen test\cases\xlir_async_future_then_xvalue_local_parameter.xl --backend xlir --emit build\test\xlir_async_future_then_xvalue_local_parameter_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xlir_async_machine_frame_1" build\test\xlir_async_future_then_xvalue_local_parameter_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"= xrtValueRetain(v" build\test\xlir_async_future_then_xvalue_local_parameter_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"frame->f0 = v" build\test\xlir_async_future_then_xvalue_local_parameter_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtTaskSubmit" build\test\xlir_async_future_then_xvalue_local_parameter_generated.c >nul
if not errorlevel 1 (
    echo XLIR async dynamic local continuation unexpectedly submitted an XRT task
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_async_future_then_xvalue_local_parameter_host.c %XL6_CGEN_LINKS% -o build\test\xlir_async_future_then_xvalue_local_parameter_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_async_future_then_xvalue_local_parameter_host.exe
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror -DXLIR_XVALUE_LOCAL_PARAMETER_REJECT test\cases\xlir_async_future_then_xvalue_local_parameter_host.c %XL6_CGEN_LINKS% -o build\test\xlir_async_future_then_xvalue_local_parameter_reject_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_async_future_then_xvalue_local_parameter_reject_host.exe
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror -DXLIR_XVALUE_LOCAL_PARAMETER_CANCEL test\cases\xlir_async_future_then_xvalue_local_parameter_host.c %XL6_CGEN_LINKS% -o build\test\xlir_async_future_then_xvalue_local_parameter_cancel_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_async_future_then_xvalue_local_parameter_cancel_host.exe
if errorlevel 1 exit /b 1
rem A direct extern Future can feed the same retained dynamic continuation;
rem the copied declaration establishes its owner before the value moves into
rem the frame, and source ownership is released after continuation acceptance.
release\xl.exe cgen test\cases\xlir_async_future_direct_xvalue_capture.xl --backend xlir --emit build\test\xlir_async_future_direct_xvalue_capture_generated.c
if errorlevel 1 exit /b 1
findstr /C:"= xrtValueRetain(v" build\test\xlir_async_future_direct_xvalue_capture_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"frame->f0 = v" build\test\xlir_async_future_direct_xvalue_capture_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xlir_make_xvalue_future()" build\test\xlir_async_future_direct_xvalue_capture_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xlir_async_machine_resume_2_0" build\test\xlir_async_future_direct_xvalue_capture_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtTaskSubmit" build\test\xlir_async_future_direct_xvalue_capture_generated.c >nul
if not errorlevel 1 (
    echo XLIR async extern Future dynamic capture unexpectedly submitted an XRT task
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_async_future_direct_xvalue_capture_host.c %XL6_CGEN_LINKS% -o build\test\xlir_async_future_direct_xvalue_capture_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_async_future_direct_xvalue_capture_host.exe
if errorlevel 1 exit /b 1
rem Dynamic capture slices retain every source value and execute the ordered
rem runtime-op sequence entirely from XLIR frame metadata.
release\xl.exe cgen test\cases\xlir_async_future_xvalue_capture_chain.xl --backend xlir --emit build\test\xlir_async_future_xvalue_capture_chain_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xlir_async_machine_frame_2" build\test\xlir_async_future_xvalue_capture_chain_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtValueRetain(a1)" build\test\xlir_async_future_xvalue_capture_chain_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtValueRetain(a2)" build\test\xlir_async_future_xvalue_capture_chain_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xlir_xvalue_binary(" build\test\xlir_async_future_xvalue_capture_chain_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xlir_async_machine_resume_2_0" build\test\xlir_async_future_xvalue_capture_chain_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtTaskSubmit" build\test\xlir_async_future_xvalue_capture_chain_generated.c >nul
if not errorlevel 1 (
    echo XLIR async dynamic capture chain unexpectedly submitted an XRT task
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_async_future_xvalue_capture_chain_host.c %XL6_CGEN_LINKS% -o build\test\xlir_async_future_xvalue_capture_chain_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_async_future_xvalue_capture_chain_host.exe
if errorlevel 1 exit /b 1
rem A managed string continuation owns a copied suffix in the lowered frame,
rem concatenates it in the callback and resolves an owned string result.
release\xl.exe cgen test\cases\xlir_async_future_then_string.xl --backend xlir --emit build\test\xlir_async_future_then_string_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xlir_async_machine_frame_2" build\test\xlir_async_future_then_string_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtStrDup(a1)" build\test\xlir_async_future_then_string_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtStrConcat(" build\test\xlir_async_future_then_string_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtTaskSubmit" build\test\xlir_async_future_then_string_generated.c >nul
if not errorlevel 1 (
    echo XLIR async string continuation unexpectedly submitted an XRT task
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_async_future_then_string_host.c %XL6_CGEN_LINKS% -o build\test\xlir_async_future_then_string_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_async_future_then_string_host.exe
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror -DXLIR_THEN_STRING_REJECT test\cases\xlir_async_future_then_string_host.c %XL6_CGEN_LINKS% -o build\test\xlir_async_future_then_string_reject_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_async_future_then_string_reject_host.exe
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror -DXLIR_THEN_STRING_CANCEL test\cases\xlir_async_future_then_string_host.c %XL6_CGEN_LINKS% -o build\test\xlir_async_future_then_string_cancel_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_async_future_then_string_cancel_host.exe
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror -DXLIR_THEN_STRING_CLOSE test\cases\xlir_async_future_then_string_host.c %XL6_CGEN_LINKS% -o build\test\xlir_async_future_then_string_close_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_async_future_then_string_close_host.exe
if errorlevel 1 exit /b 1
rem The reverse operand direction is carried in XLIR state; the C emitter
rem only consumes it to order the two string views in the callback.
release\xl.exe cgen test\cases\xlir_async_future_then_string_reversed.xl --backend xlir --emit build\test\xlir_async_future_then_string_reversed_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xrtStrConcat(" build\test\xlir_async_future_then_string_reversed_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_async_future_then_string_reversed_host.c %XL6_CGEN_LINKS% -o build\test\xlir_async_future_then_string_reversed_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_async_future_then_string_reversed_host.exe
if errorlevel 1 exit /b 1
rem A local managed-string alias transfers its cleanup ownership to the copied
rem continuation frame; no worker task or source-local cleanup is emitted.
release\xl.exe cgen test\cases\xlir_async_future_then_string_local.xl --backend xlir --emit build\test\xlir_async_future_then_string_local_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xlir_async_machine_frame_2" build\test\xlir_async_future_then_string_local_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"= xrtStrDup(v" build\test\xlir_async_future_then_string_local_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"frame->f0 = v" build\test\xlir_async_future_then_string_local_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtTaskSubmit" build\test\xlir_async_future_then_string_local_generated.c >nul
if not errorlevel 1 (
    echo XLIR async local-string continuation unexpectedly submitted an XRT task
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_async_future_then_string_local_host.c %XL6_CGEN_LINKS% -o build\test\xlir_async_future_then_string_local_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_async_future_then_string_local_host.exe
if errorlevel 1 exit /b 1
rem A string literal local is canonicalized as an owned immediate frame capture;
rem the continuation starts without a worker task or source-local cleanup.
release\xl.exe cgen test\cases\xlir_async_future_then_string_immediate.xl --backend xlir --emit build\test\xlir_async_future_then_string_immediate_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xrtStrDup(\"!\")" build\test\xlir_async_future_then_string_immediate_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtTaskSubmit" build\test\xlir_async_future_then_string_immediate_generated.c >nul
if not errorlevel 1 (
    echo XLIR async immediate-string continuation unexpectedly submitted an XRT task
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_async_future_then_string_immediate_host.c %XL6_CGEN_LINKS% -o build\test\xlir_async_future_then_string_immediate_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_async_future_then_string_immediate_host.exe
if errorlevel 1 exit /b 1
rem Ordered managed captures are an XLIR slice; each frame slot owns its copy
rem and the callback applies the explicit concatenation sequence.
release\xl.exe cgen test\cases\xlir_async_future_then_string_capture_chain.xl --backend xlir --emit build\test\xlir_async_future_then_string_capture_chain_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xlir_async_machine_frame_2" build\test\xlir_async_future_then_string_capture_chain_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"= xrtStrDup(v" build\test\xlir_async_future_then_string_capture_chain_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtStrDup(\"!\")" build\test\xlir_async_future_then_string_capture_chain_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtStrConcat(" build\test\xlir_async_future_then_string_capture_chain_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtTaskSubmit" build\test\xlir_async_future_then_string_capture_chain_generated.c >nul
if not errorlevel 1 (
    echo XLIR async string capture chain unexpectedly submitted an XRT task
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_async_future_then_string_capture_chain_host.c %XL6_CGEN_LINKS% -o build\test\xlir_async_future_then_string_capture_chain_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_async_future_then_string_capture_chain_host.exe
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror -DXLIR_STRING_CAPTURE_CHAIN_REJECT test\cases\xlir_async_future_then_string_capture_chain_host.c %XL6_CGEN_LINKS% -o build\test\xlir_async_future_then_string_capture_chain_reject_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_async_future_then_string_capture_chain_reject_host.exe
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror -DXLIR_STRING_CAPTURE_CHAIN_CANCEL test\cases\xlir_async_future_then_string_capture_chain_host.c %XL6_CGEN_LINKS% -o build\test\xlir_async_future_then_string_capture_chain_cancel_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_async_future_then_string_capture_chain_cancel_host.exe
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror -DXLIR_STRING_CAPTURE_CHAIN_CLOSE test\cases\xlir_async_future_then_string_capture_chain_host.c %XL6_CGEN_LINKS% -o build\test\xlir_async_future_then_string_capture_chain_close_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_async_future_then_string_capture_chain_close_host.exe
if errorlevel 1 exit /b 1
rem Capture direction is part of XLIR: a prefix precedes the resumed input,
rem while the following suffix is appended after it.
release\xl.exe cgen test\cases\xlir_async_future_then_string_capture_chain_reversed.xl --backend xlir --emit build\test\xlir_async_future_then_string_capture_chain_reversed_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xlir_async_machine_resume_2_0" build\test\xlir_async_future_then_string_capture_chain_reversed_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtStrConcat(" build\test\xlir_async_future_then_string_capture_chain_reversed_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_async_future_then_string_capture_chain_reversed_host.c %XL6_CGEN_LINKS% -o build\test\xlir_async_future_then_string_capture_chain_reversed_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_async_future_then_string_capture_chain_reversed_host.exe
if errorlevel 1 exit /b 1
rem Direct string parameters also lower to the ordered capture slice; there is
rem no source local or worker body for Cgen to recover.
release\xl.exe cgen test\cases\xlir_async_future_then_string_parameter_chain.xl --backend xlir --emit build\test\xlir_async_future_then_string_parameter_chain_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xrtStrDup(a1)" build\test\xlir_async_future_then_string_parameter_chain_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtStrDup(a2)" build\test\xlir_async_future_then_string_parameter_chain_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtTaskSubmit" build\test\xlir_async_future_then_string_parameter_chain_generated.c >nul
if not errorlevel 1 (
    echo XLIR async direct string parameter chain unexpectedly submitted an XRT task
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_async_future_then_string_parameter_chain_host.c %XL6_CGEN_LINKS% -o build\test\xlir_async_future_then_string_parameter_chain_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_async_future_then_string_parameter_chain_host.exe
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror -DXLIR_STRING_PARAMETER_CHAIN_REJECT test\cases\xlir_async_future_then_string_parameter_chain_host.c %XL6_CGEN_LINKS% -o build\test\xlir_async_future_then_string_parameter_chain_reject_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_async_future_then_string_parameter_chain_reject_host.exe
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror -DXLIR_STRING_PARAMETER_CHAIN_CANCEL test\cases\xlir_async_future_then_string_parameter_chain_host.c %XL6_CGEN_LINKS% -o build\test\xlir_async_future_then_string_parameter_chain_cancel_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_async_future_then_string_parameter_chain_cancel_host.exe
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror -DXLIR_STRING_PARAMETER_CHAIN_CLOSE test\cases\xlir_async_future_then_string_parameter_chain_host.c %XL6_CGEN_LINKS% -o build\test\xlir_async_future_then_string_parameter_chain_close_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_async_future_then_string_parameter_chain_close_host.exe
if errorlevel 1 exit /b 1
rem The same parameter slice carries operand direction, including a prefix
rem which must precede the resumed Future result.
release\xl.exe cgen test\cases\xlir_async_future_then_string_parameter_chain_reversed.xl --backend xlir --emit build\test\xlir_async_future_then_string_parameter_chain_reversed_generated.c
if errorlevel 1 exit /b 1
findstr /C:"frame->f0 = v" build\test\xlir_async_future_then_string_parameter_chain_reversed_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xlir_async_machine_resume_2_0" build\test\xlir_async_future_then_string_parameter_chain_reversed_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtStrConcat(" build\test\xlir_async_future_then_string_parameter_chain_reversed_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_async_future_then_string_parameter_chain_reversed_host.c %XL6_CGEN_LINKS% -o build\test\xlir_async_future_then_string_parameter_chain_reversed_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_async_future_then_string_parameter_chain_reversed_host.exe
if errorlevel 1 exit /b 1
rem A single ordered slice may mix direct parameters with a local literal;
rem no Cgen source walk may distinguish their lifetime or operand order.
release\xl.exe cgen test\cases\xlir_async_future_then_string_mixed_capture_chain.xl --backend xlir --emit build\test\xlir_async_future_then_string_mixed_capture_chain_generated.c
if errorlevel 1 exit /b 1
findstr /C:"frame->f0 = v" build\test\xlir_async_future_then_string_mixed_capture_chain_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtStrDup(\"-\")" build\test\xlir_async_future_then_string_mixed_capture_chain_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtStrDup(a2)" build\test\xlir_async_future_then_string_mixed_capture_chain_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xlir_async_machine_resume_2_0" build\test\xlir_async_future_then_string_mixed_capture_chain_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtStrConcat(" build\test\xlir_async_future_then_string_mixed_capture_chain_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xlir_async_machine_frame_2" build\test\xlir_async_future_then_string_mixed_capture_chain_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_async_future_then_string_mixed_capture_chain_host.c %XL6_CGEN_LINKS% -o build\test\xlir_async_future_then_string_mixed_capture_chain_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_async_future_then_string_mixed_capture_chain_host.exe
if errorlevel 1 exit /b 1
rem A two-Future string expression copies the first resolved value into an
rem owned carry, resumes the second Future and concatenates only from XLIR.
release\xl.exe cgen test\cases\xlir_async_future_string_chain.xl --backend xlir --emit build\test\xlir_async_future_string_chain_generated.c
if errorlevel 1 exit /b 1
findstr /C:"const char* source = *(const char* const*)input->Value;" build\test\xlir_async_future_string_chain_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"source != NULL ? xrtStrDup(source) : NULL;" build\test\xlir_async_future_string_chain_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xlir_async_machine_resume_2_1" build\test\xlir_async_future_string_chain_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtTaskSubmit" build\test\xlir_async_future_string_chain_generated.c >nul
if not errorlevel 1 (
    echo XLIR async string future chain unexpectedly submitted an XRT task
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_async_future_string_chain_host.c %XL6_CGEN_LINKS% -o build\test\xlir_async_future_string_chain_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_async_future_string_chain_host.exe
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror -DXLIR_STRING_FUTURE_CHAIN_SECOND_REJECT test\cases\xlir_async_future_string_chain_host.c %XL6_CGEN_LINKS% -o build\test\xlir_async_future_string_chain_reject_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_async_future_string_chain_reject_host.exe
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror -DXLIR_STRING_FUTURE_CHAIN_SECOND_CANCEL test\cases\xlir_async_future_string_chain_host.c %XL6_CGEN_LINKS% -o build\test\xlir_async_future_string_chain_cancel_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_async_future_string_chain_cancel_host.exe
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror -DXLIR_STRING_FUTURE_CHAIN_SECOND_CLOSE test\cases\xlir_async_future_string_chain_host.c %XL6_CGEN_LINKS% -o build\test\xlir_async_future_string_chain_close_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_async_future_string_chain_close_host.exe
if errorlevel 1 exit /b 1
rem The non-terminal string state transfers a fresh owned carry to the third
rem Future state; neither source body nor task-pool scheduling is consulted.
release\xl.exe cgen test\cases\xlir_async_future_three_strings.xl --backend xlir --emit build\test\xlir_async_future_three_strings_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xlir_async_machine_resume_2_2" build\test\xlir_async_future_three_strings_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"next->f3 = frame->f3; next->i3 = frame->i3; frame->f3 = NULL; frame->i3 = 0;" build\test\xlir_async_future_three_strings_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xlir_async_chain_frame_" build\test\xlir_async_future_three_strings_generated.c >nul
if not errorlevel 1 (
    echo XLIR three-Future string target unexpectedly used the legacy async-chain renderer
    exit /b 1
)
findstr /C:"xrtTaskSubmit" build\test\xlir_async_future_three_strings_generated.c >nul
if not errorlevel 1 (
    echo XLIR three-Future string chain unexpectedly submitted an XRT task
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_async_future_three_strings_host.c %XL6_CGEN_LINKS% -o build\test\xlir_async_future_three_strings_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_async_future_three_strings_host.exe
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror -DXLIR_STRING_THIRD_REJECT test\cases\xlir_async_future_three_strings_host.c %XL6_CGEN_LINKS% -o build\test\xlir_async_future_three_strings_reject_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_async_future_three_strings_reject_host.exe
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror -DXLIR_STRING_THIRD_CANCEL test\cases\xlir_async_future_three_strings_host.c %XL6_CGEN_LINKS% -o build\test\xlir_async_future_three_strings_cancel_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_async_future_three_strings_cancel_host.exe
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror -DXLIR_STRING_THIRD_CLOSE test\cases\xlir_async_future_three_strings_host.c %XL6_CGEN_LINKS% -o build\test\xlir_async_future_three_strings_close_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_async_future_three_strings_close_host.exe
if errorlevel 1 exit /b 1
rem Operand order is explicit in the continuation ABI: the captured scalar
rem may be the left operand of a non-commutative resumed expression.
release\xl.exe cgen test\cases\xlir_async_future_then_scalar_reversed.xl --backend xlir --emit build\test\xlir_async_future_then_scalar_reversed_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xlir_async_machine_resume_2_0" build\test\xlir_async_future_then_scalar_reversed_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_async_future_then_scalar_reversed_host.c %XL6_CGEN_LINKS% -o build\test\xlir_async_future_then_scalar_reversed_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_async_future_then_scalar_reversed_host.exe
if errorlevel 1 exit /b 1
rem The continuation ABI is type-driven: float workers use the exact double
rem result/capture ABI recorded in XLIR, not an integer-only callback path.
release\xl.exe cgen test\cases\xlir_async_future_then_float.xl --backend xlir --emit build\test\xlir_async_future_then_float_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xlir_async_machine_resume_2_0" build\test\xlir_async_future_then_float_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_async_future_then_float_host.c %XL6_CGEN_LINKS% -o build\test\xlir_async_future_then_float_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_async_future_then_float_host.exe
if errorlevel 1 exit /b 1
rem Two await sites lower to a verified continuation-state sequence.  The
rem second Future is resumed through ContinueOwned and must bridge every
rem terminal state back to the public Future without a task-pool worker.
release\xl.exe cgen test\cases\xlir_async_future_two_awaits.xl --backend xlir --emit build\test\xlir_async_future_two_awaits_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xlir_async_machine_frame_2" build\test\xlir_async_future_two_awaits_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xlir_async_machine_resume_2_0" build\test\xlir_async_future_two_awaits_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xlir_async_machine_resume_2_1" build\test\xlir_async_future_two_awaits_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"next->outer = frame->outer != NULL ? frame->outer : xrtPromiseRef(output)" build\test\xlir_async_future_two_awaits_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xlir_async_chain_frame_" build\test\xlir_async_future_two_awaits_generated.c >nul
if not errorlevel 1 (
    echo XLIR two-await target unexpectedly used the legacy async-chain renderer
    exit /b 1
)
findstr /C:"xrtTaskSubmit" build\test\xlir_async_future_two_awaits_generated.c >nul
if not errorlevel 1 (
    echo XLIR async future-chain unexpectedly submitted an XRT task
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_async_future_two_awaits_host.c %XL6_CGEN_LINKS% -o build\test\xlir_async_future_two_awaits_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_async_future_two_awaits_host.exe
if errorlevel 1 exit /b 1
rem The second source Future may fail after the first state has completed.
rem The chain must reject the original public Future rather than leaving its
rem Promise pending or consulting an unrelated thread-local error slot.
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror -DXLIR_CHAIN_SECOND_REJECT test\cases\xlir_async_future_two_awaits_host.c %XL6_CGEN_LINKS% -o build\test\xlir_async_future_two_awaits_reject_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_async_future_two_awaits_reject_host.exe
if errorlevel 1 exit /b 1
rem A canonical async machine may have more than one suspension edge.
rem Three awaited Futures must generate three callbacks, retain only the
rem still-needed handles per machine frame, and propagate the final failure.
release\xl.exe ir test\cases\xlir_async_future_three_awaits.xl --lowered --emit build\test\xlir_async_future_three_awaits.lowered.xlir
if errorlevel 1 exit /b 1
findstr /C:"suspension q1 state=1" build\test\xlir_async_future_three_awaits.lowered.xlir >nul
if errorlevel 1 exit /b 1
findstr /C:"suspension q2 state=2" build\test\xlir_async_future_three_awaits.lowered.xlir >nul
if errorlevel 1 exit /b 1
findstr /C:"machine-frame f0 source=storage residence=launch-parameter" build\test\xlir_async_future_three_awaits.lowered.xlir >nul
if errorlevel 1 exit /b 1
findstr /C:"machine-frame f1 source=storage residence=launch-parameter" build\test\xlir_async_future_three_awaits.lowered.xlir >nul
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_async_future_three_awaits.xl --backend xlir --emit build\test\xlir_async_future_three_awaits_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xlir_async_machine_resume_2_2" build\test\xlir_async_future_three_awaits_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"next->outer = frame->outer != NULL ? frame->outer : xrtPromiseRef(output)" build\test\xlir_async_future_three_awaits_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xlir_async_chain_frame_" build\test\xlir_async_future_three_awaits_generated.c >nul
if not errorlevel 1 (
    echo XLIR three-await target unexpectedly used the legacy async-chain renderer
    exit /b 1
)
findstr /C:"xrtTaskSubmit" build\test\xlir_async_future_three_awaits_generated.c >nul
if not errorlevel 1 (
    echo XLIR multi-state future-chain unexpectedly submitted an XRT task
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_async_future_three_awaits_host.c %XL6_CGEN_LINKS% -o build\test\xlir_async_future_three_awaits_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_async_future_three_awaits_host.exe
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror -DXLIR_CHAIN_THIRD_REJECT test\cases\xlir_async_future_three_awaits_host.c %XL6_CGEN_LINKS% -o build\test\xlir_async_future_three_awaits_reject_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_async_future_three_awaits_reject_host.exe
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror -DXLIR_CHAIN_THIRD_CANCEL test\cases\xlir_async_future_three_awaits_host.c %XL6_CGEN_LINKS% -o build\test\xlir_async_future_three_awaits_cancel_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_async_future_three_awaits_cancel_host.exe
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror -DXLIR_CHAIN_THIRD_CLOSE test\cases\xlir_async_future_three_awaits_host.c %XL6_CGEN_LINKS% -o build\test\xlir_async_future_three_awaits_close_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_async_future_three_awaits_close_host.exe
if errorlevel 1 exit /b 1
rem Each carried state owns its own normalized opcode; a mixed left-associated
rem chain must not inherit the final operator for every resume.
release\xl.exe cgen test\cases\xlir_async_future_three_awaits_mixed.xl --backend xlir --emit build\test\xlir_async_future_three_awaits_mixed_generated.c
if errorlevel 1 exit /b 1
findstr /C:"v17 = v14 - v16;" build\test\xlir_async_future_three_awaits_mixed_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"v20 = v17 + v19;" build\test\xlir_async_future_three_awaits_mixed_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_async_future_three_awaits_mixed_host.c %XL6_CGEN_LINKS% -o build\test\xlir_async_future_three_awaits_mixed_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_async_future_three_awaits_mixed_host.exe
if errorlevel 1 exit /b 1
rem A scalar local completed before the first suspension is a canonical
rem immediate frame capture, not a task-pool fallback or Cgen local lookup.
release\xl.exe cgen test\cases\xlir_async_future_preawait_local.xl --backend xlir --emit build\test\xlir_async_future_preawait_local_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xlir_async_machine_frame_2" build\test\xlir_async_future_preawait_local_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtTaskSubmit" build\test\xlir_async_future_preawait_local_generated.c >nul
if not errorlevel 1 (
    echo XLIR pre-await immediate capture unexpectedly submitted an XRT task
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_async_future_preawait_local_host.c %XL6_CGEN_LINKS% -o build\test\xlir_async_future_preawait_local_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_async_future_preawait_local_host.exe
if errorlevel 1 exit /b 1
rem A pre-await scalar local initialized from a parameter is lowered to the
rem parameter capture itself; its local alias must not leak into Cgen.
release\xl.exe cgen test\cases\xlir_async_future_preawait_parameter_local.xl --backend xlir --emit build\test\xlir_async_future_preawait_parameter_local_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xlir_async_machine_frame_2" build\test\xlir_async_future_preawait_parameter_local_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtTaskSubmit" build\test\xlir_async_future_preawait_parameter_local_generated.c >nul
if not errorlevel 1 (
    echo XLIR pre-await parameter capture unexpectedly submitted an XRT task
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_async_future_preawait_parameter_local_host.c %XL6_CGEN_LINKS% -o build\test\xlir_async_future_preawait_parameter_local_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_async_future_preawait_parameter_local_host.exe
if errorlevel 1 exit /b 1
rem A capture slice models two completed locals in lexical resume order.  Cgen
rem must use only the typed frame slots, not recover either local from source.
release\xl.exe cgen test\cases\xlir_async_future_capture_chain.xl --backend xlir --emit build\test\xlir_async_future_capture_chain_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xlir_async_machine_frame_2" build\test\xlir_async_future_capture_chain_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xlir_async_machine_resume_2_0" build\test\xlir_async_future_capture_chain_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtFutureContinueOwned" build\test\xlir_async_future_capture_chain_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xlir_async_machine_frame_free_2" build\test\xlir_async_future_capture_chain_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtTaskSubmit" build\test\xlir_async_future_capture_chain_generated.c >nul
if not errorlevel 1 (
    echo XLIR scalar capture-chain unexpectedly submitted an XRT task
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_async_future_capture_chain_host.c %XL6_CGEN_LINKS% -o build\test\xlir_async_future_capture_chain_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_async_future_capture_chain_host.exe
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror -DXLIR_CAPTURE_CHAIN_REJECT test\cases\xlir_async_future_capture_chain_host.c %XL6_CGEN_LINKS% -o build\test\xlir_async_future_capture_chain_reject_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_async_future_capture_chain_reject_host.exe
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror -DXLIR_CAPTURE_CHAIN_CANCEL test\cases\xlir_async_future_capture_chain_host.c %XL6_CGEN_LINKS% -o build\test\xlir_async_future_capture_chain_cancel_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_async_future_capture_chain_cancel_host.exe
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror -DXLIR_CAPTURE_CHAIN_CLOSE test\cases\xlir_async_future_capture_chain_host.c %XL6_CGEN_LINKS% -o build\test\xlir_async_future_capture_chain_close_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_async_future_capture_chain_close_host.exe
if errorlevel 1 exit /b 1
rem Each capture owns its own normalized operator, preventing Cgen from
rem collapsing a mixed source chain into the final operator.
release\xl.exe cgen test\cases\xlir_async_future_capture_chain_mixed.xl --backend xlir --emit build\test\xlir_async_future_capture_chain_mixed_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xlir_async_machine_resume_2_0" build\test\xlir_async_future_capture_chain_mixed_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xlir_async_machine_frame_2" build\test\xlir_async_future_capture_chain_mixed_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_async_future_capture_chain_mixed_host.c %XL6_CGEN_LINKS% -o build\test\xlir_async_future_capture_chain_mixed_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_async_future_capture_chain_mixed_host.exe
if errorlevel 1 exit /b 1
rem Direct scalar parameters and a literal local share one ordered capture
rem slice; Cgen receives only copied frame values and normalized operations.
release\xl.exe cgen test\cases\xlir_async_future_scalar_mixed_capture_chain.xl --backend xlir --emit build\test\xlir_async_future_scalar_mixed_capture_chain_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xlir_async_machine_frame_2" build\test\xlir_async_future_scalar_mixed_capture_chain_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xlir_async_machine_resume_2_0" build\test\xlir_async_future_scalar_mixed_capture_chain_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtFutureContinueOwned" build\test\xlir_async_future_scalar_mixed_capture_chain_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xlir_async_machine_frame_free_2" build\test\xlir_async_future_scalar_mixed_capture_chain_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"frame->f" build\test\xlir_async_future_scalar_mixed_capture_chain_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtPromiseResolveOwned" build\test\xlir_async_future_scalar_mixed_capture_chain_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_async_future_scalar_mixed_capture_chain_host.c %XL6_CGEN_LINKS% -o build\test\xlir_async_future_scalar_mixed_capture_chain_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_async_future_scalar_mixed_capture_chain_host.exe
if errorlevel 1 exit /b 1
rem The innermost capture may precede await; its operand direction is XLIR
rem data and Cgen must not infer it from a source expression tree.
release\xl.exe cgen test\cases\xlir_async_future_capture_chain_await_right.xl --backend xlir --emit build\test\xlir_async_future_capture_chain_await_right_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xlir_async_machine_resume_2_0" build\test\xlir_async_future_capture_chain_await_right_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xlir_async_machine_frame_2" build\test\xlir_async_future_capture_chain_await_right_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtTaskSubmit" build\test\xlir_async_future_capture_chain_await_right_generated.c >nul
if not errorlevel 1 (
    echo XLIR right-await scalar capture-chain unexpectedly submitted an XRT task
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_async_future_capture_chain_await_right_host.c %XL6_CGEN_LINKS% -o build\test\xlir_async_future_capture_chain_await_right_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_async_future_capture_chain_await_right_host.exe
if errorlevel 1 exit /b 1
release\xl.exe ir test\cases\xlir_await_future_i64.xl --lowered --emit build\test\xlir_await_future_i64.lowered.xlir
if errorlevel 1 exit /b 1
findstr /C:"await" build\test\xlir_await_future_i64.lowered.xlir >nul
if errorlevel 1 exit /b 1
findstr /C:"error-test" build\test\xlir_await_future_i64.lowered.xlir >nul
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_await_future_i64.xl --backend xlir --emit build\test\xlir_await_future_i64_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xlir_future_await_i64" build\test\xlir_await_future_i64_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtGetError() == NULL" build\test\xlir_await_future_i64_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_await_future_i64_host.c %XL6_CGEN_LINKS% -o build\test\xlir_await_future_i64_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_await_future_i64_host.exe
if errorlevel 1 exit /b 1
rem Awaiting a direct future call uses explicit temporary future storage in
rem XLIR.  Cgen only consumes the store/load/release lifetime already fixed
rem by lowering before it emits the future await runtime call.
release\xl.exe cgen test\cases\xlir_await_future_call.xl --backend xlir --emit build\test\xlir_await_future_call_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xlir_make_i64_future();" build\test\xlir_await_future_call_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xlir_future_await_i64(v" build\test\xlir_await_future_call_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtFutureDestroy(s4294967295); s4294967295 = NULL;" build\test\xlir_await_future_call_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_await_future_call_host.c %XL6_CGEN_LINKS% -o build\test\xlir_await_future_call_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_await_future_call_host.exe
if errorlevel 1 exit /b 1
rem A zero-argument extern Future is explicit continuation input provenance;
rem the async wrapper directly forwards the resolved target call without a task.
release\xl.exe cgen test\cases\xlir_async_future_direct_call.xl --backend xlir --emit build\test\xlir_async_future_direct_call_generated.c
if errorlevel 1 exit /b 1
findstr /C:"static xfuture* xlir_async_start_2(void)" build\test\xlir_async_future_direct_call_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xlir_make_i64_future()" build\test\xlir_async_future_direct_call_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtTaskSubmit" build\test\xlir_async_future_direct_call_generated.c >nul
if not errorlevel 1 (
    echo XLIR async direct Future forward unexpectedly submitted an XRT task
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_async_future_direct_call_host.c %XL6_CGEN_LINKS% -o build\test\xlir_async_future_direct_call_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_async_future_direct_call_host.exe
if errorlevel 1 exit /b 1
rem A direct Future producer and scalar capture lower to an explicit
rem continuation input/frame ABI, without a blocking worker or task-pool hop.
release\xl.exe cgen test\cases\xlir_async_future_direct_call_capture.xl --backend xlir --emit build\test\xlir_async_future_direct_call_capture_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xlir_async_machine_frame_2" build\test\xlir_async_future_direct_call_capture_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xlir_make_i64_future()" build\test\xlir_async_future_direct_call_capture_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xlir_async_machine_resume_2_0" build\test\xlir_async_future_direct_call_capture_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtTaskSubmit" build\test\xlir_async_future_direct_call_capture_generated.c >nul
if not errorlevel 1 (
    echo XLIR async extern Future capture unexpectedly submitted an XRT task
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_async_future_direct_call_capture_host.c %XL6_CGEN_LINKS% -o build\test\xlir_async_future_direct_call_capture_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_async_future_direct_call_capture_host.exe
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror -DXLIR_ASYNC_DIRECT_CALL_REJECT test\cases\xlir_async_future_direct_call_capture_host.c %XL6_CGEN_LINKS% -o build\test\xlir_async_future_direct_call_capture_reject_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_async_future_direct_call_capture_reject_host.exe
if errorlevel 1 exit /b 1
rem A rejected direct Future becomes an explicit all-state continuation: it
rem resolves the verified catch default and never falls back to a task worker.
release\xl.exe cgen test\cases\xlir_async_future_direct_call_catch.xl --backend xlir --emit build\test\xlir_async_future_direct_call_catch_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xlir_async_machine_resume_2_0" build\test\xlir_async_future_direct_call_catch_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtTakeError()" build\test\xlir_async_future_direct_call_catch_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtTaskPoolSubmit" build\test\xlir_async_future_direct_call_catch_generated.c >nul
if not errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_async_future_direct_call_catch_host.c %XL6_CGEN_LINKS% -o build\test\xlir_async_future_direct_call_catch_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_async_future_direct_call_catch_host.exe
if errorlevel 1 exit /b 1
rem A managed capture follows the same direct-future continuation path.  The
rem frame owns a duplicated string and the source Future is released once its
rem verified continuation is attached; no worker/task-pool fallback is valid.
release\xl.exe cgen test\cases\xlir_async_future_direct_string_capture.xl --backend xlir --emit build\test\xlir_async_future_direct_string_capture_generated.c
if errorlevel 1 exit /b 1
findstr /C:"= xrtStrDup(v" build\test\xlir_async_future_direct_string_capture_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"frame->f0 = v" build\test\xlir_async_future_direct_string_capture_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xlir_make_string_future()" build\test\xlir_async_future_direct_string_capture_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xlir_async_machine_resume_2_0" build\test\xlir_async_future_direct_string_capture_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtTaskSubmit" build\test\xlir_async_future_direct_string_capture_generated.c >nul
if not errorlevel 1 (
    echo XLIR async extern Future string capture unexpectedly submitted an XRT task
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_async_future_direct_string_capture_host.c %XL6_CGEN_LINKS% -o build\test\xlir_async_future_direct_string_capture_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_async_future_direct_string_capture_host.exe
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_await_future_i64_failure_host.c %XL6_CGEN_LINKS% -o build\test\xlir_await_future_i64_failure_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_await_future_i64_failure_host.exe
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_await_future_scalar.xl --backend xlir --emit build\test\xlir_await_future_scalar_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xlir_future_await_bool" build\test\xlir_await_future_scalar_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xlir_future_await_f64" build\test\xlir_await_future_scalar_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_await_future_scalar_host.c %XL6_CGEN_LINKS% -o build\test\xlir_await_future_scalar_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_await_future_scalar_host.exe
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_await_future_i64_catch.xl --backend xlir --emit build\test\xlir_await_future_i64_catch_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xrtTakeError" build\test\xlir_await_future_i64_catch_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_await_future_i64_catch_host.c %XL6_CGEN_LINKS% -o build\test\xlir_await_future_i64_catch_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_await_future_i64_catch_host.exe
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_await_future_i64_catch_failure_host.c %XL6_CGEN_LINKS% -o build\test\xlir_await_future_i64_catch_failure_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_await_future_i64_catch_failure_host.exe
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_await_future_i64_finally.xl --backend xlir --emit build\test\xlir_await_future_i64_finally_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xrtGetError() == NULL" build\test\xlir_await_future_i64_finally_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_await_future_i64_finally_host.c %XL6_CGEN_LINKS% -o build\test\xlir_await_future_i64_finally_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_await_future_i64_finally_host.exe
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_await_future_i64_finally_failure_host.c %XL6_CGEN_LINKS% -o build\test\xlir_await_future_i64_finally_failure_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_await_future_i64_finally_failure_host.exe
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_try_finally_break_scalar.xl --backend xlir --emit build\test\xlir_try_finally_break_scalar_generated.c
if errorlevel 1 exit /b 1
release\xl.exe ir test\cases\xlir_try_finally_break_scalar.xl --lowered --emit build\test\xlir_try_finally_break_scalar.lowered.xlir
if errorlevel 1 exit /b 1
findstr /R /C:"jump .*origin=n21 .*to=b" build\test\xlir_try_finally_break_scalar.lowered.xlir >nul
if errorlevel 1 (
    echo XLIR finally break did not materialize an explicit lowered CFG jump route.
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_try_finally_break_scalar_host.c %XL6_CGEN_LINKS% -o build\test\xlir_try_finally_break_scalar_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_try_finally_break_scalar_host.exe
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_try_finally_continue_scalar.xl --backend xlir --emit build\test\xlir_try_finally_continue_scalar_generated.c
if errorlevel 1 exit /b 1
release\xl.exe ir test\cases\xlir_try_finally_continue_scalar.xl --lowered --emit build\test\xlir_try_finally_continue_scalar.lowered.xlir
if errorlevel 1 exit /b 1
findstr /R /C:"jump .*origin=n31 .*to=b" build\test\xlir_try_finally_continue_scalar.lowered.xlir >nul
if errorlevel 1 (
    echo XLIR finally continue did not materialize an explicit lowered CFG jump route.
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_try_finally_continue_scalar_host.c %XL6_CGEN_LINKS% -o build\test\xlir_try_finally_continue_scalar_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_try_finally_continue_scalar_host.exe
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_nested_try_finally_break_scalar.xl --backend xlir --emit build\test\xlir_nested_try_finally_break_scalar_generated.c
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_nested_try_finally_break_scalar_host.c %XL6_CGEN_LINKS% -o build\test\xlir_nested_try_finally_break_scalar_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_nested_try_finally_break_scalar_host.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\xlir_try_catch_finally_scalar.xl --backend xlir --emit build\test\xlir_try_catch_finally_scalar_generated.c
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_try_catch_finally_scalar.xl --backend xlir --emit build\test\xlir_try_catch_finally_scalar_repeat.c
if errorlevel 1 exit /b 1
fc /b build\test\xlir_try_catch_finally_scalar_generated.c build\test\xlir_try_catch_finally_scalar_repeat.c >nul
if errorlevel 1 (
    echo XLIR try/catch/finally C emission is not byte-for-byte deterministic.
    exit /b 1
)
findstr /C:"xrtTakeError();" build\test\xlir_try_catch_finally_scalar_generated.c >nul
if errorlevel 1 (
    echo XLIR try/catch/finally is missing its owned handler error payload ABI.
    exit /b 1
)
findstr /C:"xrtErrorFree(" build\test\xlir_try_catch_finally_scalar_generated.c >nul
if errorlevel 1 (
    echo XLIR try/catch/finally is missing catch payload cleanup.
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_try_catch_finally_scalar_host.c %XL6_CGEN_LINKS% -o build\test\xlir_try_catch_finally_scalar_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_try_catch_finally_scalar_host.exe
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_try_catch_finally_return_scalar.xl --backend xlir --emit build\test\xlir_try_catch_finally_return_scalar_generated.c
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_try_catch_finally_return_scalar_host.c %XL6_CGEN_LINKS% -o build\test\xlir_try_catch_finally_return_scalar_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_try_catch_finally_return_scalar_host.exe
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_try_catch_finally_string_return.xl --backend xlir --emit build\test\xlir_try_catch_finally_string_return_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xrtTakeError();" build\test\xlir_try_catch_finally_string_return_generated.c >nul
if errorlevel 1 (
    echo XLIR catch/finally managed return lost its handler error ownership ABI.
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_try_catch_finally_string_return_host.c %XL6_CGEN_LINKS% -o build\test\xlir_try_catch_finally_string_return_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_try_catch_finally_string_return_host.exe
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_try_catch_finally_break_scalar.xl --backend xlir --emit build\test\xlir_try_catch_finally_break_scalar_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xrtTakeError();" build\test\xlir_try_catch_finally_break_scalar_generated.c >nul
if errorlevel 1 (
    echo XLIR catch/finally break lost its owned handler payload ABI.
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_try_catch_finally_break_scalar_host.c %XL6_CGEN_LINKS% -o build\test\xlir_try_catch_finally_break_scalar_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_try_catch_finally_break_scalar_host.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\xlir_try_catch_finally_rethrow.xl --backend xlir --emit build\test\xlir_try_catch_finally_rethrow_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xrtSetErrorTake(v" build\test\xlir_try_catch_finally_rethrow_generated.c >nul
if errorlevel 1 (
    echo XLIR catch/finally rethrow did not materialize its ownership transfer ABI.
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_try_catch_finally_rethrow_host.c %XL6_CGEN_LINKS% -o build\test\xlir_try_catch_finally_rethrow_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_try_catch_finally_rethrow_host.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\xlir_direct_scalar.xl --backend xlir --emit build\test\xlir_direct_scalar_generated.c
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_direct_scalar.xl --backend xlir --emit build\test\xlir_direct_scalar_repeat.c
if errorlevel 1 exit /b 1
fc /b build\test\xlir_direct_scalar_generated.c build\test\xlir_direct_scalar_repeat.c >nul
if errorlevel 1 (
    echo XLIR C emission is not byte-for-byte deterministic.
    exit /b 1
)
findstr /C:"v3 = v1 + v2;" build\test\xlir_direct_scalar_generated.c >nul
if errorlevel 1 (
    echo XLIR direct scalar C output is missing canonical binary instruction.
    exit /b 1
)
findstr /C:"static int64_t xlir_fn_1(void);" build\test\xlir_direct_scalar_generated.c >nul
if errorlevel 1 (
    echo XLIR C output did not preserve the canonical int64 ABI.
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -fsyntax-only build\test\xlir_direct_scalar_generated.c
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% test\cases\xlir_direct_scalar_host.c %XL6_CGEN_LINKS% -o build\test\xlir_direct_scalar_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_direct_scalar_host.exe
if errorlevel 1 exit /b 1

rem Managed exits must reach typed XLIR as explicit LIFO cleanup data.  The
rem lowered backend emits that plan mechanically without a Cgen cleanup stack.
release\xl.exe ir test\cases\xlir_cleanup_plan.xl --format json --emit build\test\xlir_cleanup_plan.json
if errorlevel 1 exit /b 1
findstr /R /C:"cleanupActionCount.*2" build\test\xlir_cleanup_plan.json >nul
if errorlevel 1 (
    echo XLIR cleanup fixture is missing its two semantic cleanup actions.
    exit /b 1
)
findstr /R /C:"targetSymbol.*4" build\test\xlir_cleanup_plan.json >nul
if errorlevel 1 exit /b 1
findstr /R /C:"targetSymbol.*3" build\test\xlir_cleanup_plan.json >nul
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_cleanup_plan.xl --backend xlir --emit build\test\xlir_cleanup_plan_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xrtFree(s4); s4 = NULL;" build\test\xlir_cleanup_plan_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtFree(s3); s3 = NULL;" build\test\xlir_cleanup_plan_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% test\cases\xlir_cleanup_plan_host.c %XL6_CGEN_LINKS% -o build\test\xlir_cleanup_plan_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_cleanup_plan_host.exe
if errorlevel 1 exit /b 1

rem Local reads and writes must carry semantic target symbols into the XLIR
rem while CFG; the generated C must execute without legacy name lookup.
release\xl.exe cgen test\cases\xlir_loop_scalar.xl --backend xlir --emit build\test\xlir_loop_scalar_generated.c
if errorlevel 1 exit /b 1
findstr /C:"if (v5) goto b4; else goto b5;" build\test\xlir_loop_scalar_generated.c >nul
if errorlevel 1 (
    echo XLIR scalar loop output is missing its explicit branch ABI.
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% test\cases\xlir_loop_scalar_host.c %XL6_CGEN_LINKS% -o build\test\xlir_loop_scalar_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_loop_scalar_host.exe
if errorlevel 1 exit /b 1

rem Direct calls are fully resolved before XLIR: Cgen receives only the
rem target symbol, ordered value operands, and the result ABI.
release\xl.exe cgen test\cases\xlir_direct_call.xl --backend xlir --emit build\test\xlir_direct_call_generated.c
if errorlevel 1 exit /b 1
findstr /C:"v6 = xlir_fn_1(v4, v5);" build\test\xlir_direct_call_generated.c >nul
if errorlevel 1 (
    echo XLIR direct call output is missing its canonical target and argument ABI.
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% test\cases\xlir_direct_call_host.c %XL6_CGEN_LINKS% -o build\test\xlir_direct_call_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_direct_call_host.exe
if errorlevel 1 exit /b 1

rem Recursive calls are ordinary pre-resolved XLIR calls.  Cgen must first
rem declare the function ABI, then emit the same resolved target inside it.
release\xl.exe cgen test\cases\xlir_recursive_scalar.xl --backend xlir --emit build\test\xlir_recursive_scalar_generated.c
if errorlevel 1 exit /b 1
findstr /C:"static int64_t xlir_fn_1(int64_t s3);" build\test\xlir_recursive_scalar_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"v9 = xlir_fn_1(v8);" build\test\xlir_recursive_scalar_generated.c >nul
if errorlevel 1 (
    echo XLIR recursive C output is missing its resolved self-call ABI.
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% test\cases\xlir_recursive_scalar_host.c %XL6_CGEN_LINKS% -o build\test\xlir_recursive_scalar_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_recursive_scalar_host.exe
if errorlevel 1 exit /b 1

rem The semantic main entry is represented as a module ABI in XLIR.  Cgen
rem emits an opaque xvalue boundary and xlang_main without legacy AST lookup.
release\xl.exe cgen test\cases\xlir_entry_scalar.xl --backend xlir --emit build\test\xlir_entry_scalar_generated.c
if errorlevel 1 exit /b 1
findstr /C:"typedef struct xvalue xvalue;" build\test\xlir_entry_scalar_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"static int64_t xlir_fn_1(xvalue* s3);" build\test\xlir_entry_scalar_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"int xlang_main(xvalue* arg)" build\test\xlir_entry_scalar_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"return (int)xlir_fn_1(arg);" build\test\xlir_entry_scalar_generated.c >nul
if errorlevel 1 (
    echo XLIR module entry output is missing its explicit process ABI.
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% test\cases\xlir_entry_scalar_host.c %XL6_CGEN_LINKS% -o build\test\xlir_entry_scalar_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_entry_scalar_host.exe
if errorlevel 1 exit /b 1

rem Native extern calls are lowered into an explicit C linkage target table.
rem Cgen consumes its resolved name and scalar signature without AST lookup.
release\xl.exe cgen test\cases\xlir_extern_scalar.xl --backend xlir --emit build\test\xlir_extern_scalar_generated.c
if errorlevel 1 exit /b 1
findstr /C:"extern int64_t xlir_native_add(int64_t, int64_t);" build\test\xlir_extern_scalar_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"v3 = xlir_native_add(v1, v2);" build\test\xlir_extern_scalar_generated.c >nul
if errorlevel 1 (
    echo XLIR extern output is missing its explicit C linkage target ABI.
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% test\cases\xlir_extern_scalar_host.c %XL6_CGEN_LINKS% -o build\test\xlir_extern_scalar_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_extern_scalar_host.exe
if errorlevel 1 exit /b 1

rem Export symbols and scalar C ABI are fixed in XLIR before Cgen.  The
rem generated adapter must not inspect the source declaration or legacy names.
release\xl.exe cgen test\cases\xlir_export_scalar.xl --backend xlir --emit build\test\xlir_export_scalar_generated.c
if errorlevel 1 exit /b 1
findstr /C:"int64_t xlir_public_add(int64_t left, int64_t right)" build\test\xlir_export_scalar_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"return xlir_fn_1(left, right);" build\test\xlir_export_scalar_generated.c >nul
if errorlevel 1 (
    echo XLIR export output is missing its explicit C linkage ABI.
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% test\cases\xlir_export_scalar_host.c %XL6_CGEN_LINKS% -o build\test\xlir_export_scalar_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_export_scalar_host.exe
if errorlevel 1 exit /b 1
rem XLIR's public header is emitted from the lowered export ABI, not the
rem legacy AST header model.  Compile a separate consumer against it.
release\xl.exe cgen test\cases\xlir_export_scalar.xl --backend xlir --emit build\test\xlir_export_scalar_generated.c --emit-header build\test\xlir_export_scalar_generated.h
if errorlevel 1 exit /b 1
findstr /C:"int64_t xlir_public_add(int64_t left, int64_t right);" build\test\xlir_export_scalar_generated.h >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_export_scalar_header_host.c build\test\xlir_export_scalar_generated.c %XL6_CGEN_LINKS% -o build\test\xlir_export_scalar_header_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_export_scalar_header_host.exe
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_export_scalar.xl --backend xlir --dump-dir build\test\xlir_export_scalar_xlir_dump
if errorlevel 1 exit /b 1
findstr /C:"xlir stage=lowered" build\test\xlir_export_scalar_xlir_dump\xlir_export_scalar.lowered.xlir >nul
if errorlevel 1 exit /b 1
findstr /C:"int64_t xlir_public_add(int64_t left, int64_t right);" build\test\xlir_export_scalar_xlir_dump\xlir_export_scalar.h >nul
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\xlir_export_auto_scalar.xl --backend xlir --emit build\test\xlir_export_auto_scalar_generated.c
if errorlevel 1 exit /b 1
findstr /C:"int64_t xl6_export_answer(void)" build\test\xlir_export_auto_scalar_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% test\cases\xlir_export_auto_scalar_host.c %XL6_CGEN_LINKS% -o build\test\xlir_export_auto_scalar_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_export_auto_scalar_host.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\xlir_export_overload_scalar.xl --backend xlir --emit build\test\xlir_export_overload_scalar_generated.c
if errorlevel 1 exit /b 1
findstr /C:"int64_t xl6_export_add__i64(int64_t value)" build\test\xlir_export_overload_scalar_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"int64_t xl6_export_add__bool(bool value)" build\test\xlir_export_overload_scalar_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% test\cases\xlir_export_overload_scalar_host.c %XL6_CGEN_LINKS% -o build\test\xlir_export_overload_scalar_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_export_overload_scalar_host.exe
if errorlevel 1 exit /b 1

rem Managed string ownership is a lowered runtime ABI: Cgen only emits the
rem explicit duplication and release operations, without a source type lookup.
release\xl.exe cgen test\cases\xlir_string_local.xl --backend xlir --emit build\test\xlir_string_local_generated.c
if errorlevel 1 exit /b 1
findstr /C:"v1 = xrtStrDup(\"managed\");" build\test\xlir_string_local_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtFree(s3); s3 = NULL;" build\test\xlir_string_local_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% test\cases\xlir_string_local_host.c %XL6_CGEN_LINKS% -o build\test\xlir_string_local_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_string_local_host.exe
if errorlevel 1 exit /b 1

rem Quote style, raw prefixes and triple quoted source strings are normalized
rem into canonical C payloads during XLIR lowering.  Cgen receives no source
rem literal syntax to interpret.
release\xl.exe cgen test\cases\xlir_string_literal_forms.xl --backend xlir --emit build\test\xlir_string_literal_forms_generated.c
if errorlevel 1 exit /b 1
findstr /C:"v1 = xrtStrDup(" build\test\xlir_string_literal_forms_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"v2 = xrtStrDup(" build\test\xlir_string_literal_forms_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"v3 = xrtStrDup(" build\test\xlir_string_literal_forms_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"v4 = xrtStrDup(" build\test\xlir_string_literal_forms_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% test\cases\xlir_string_literal_forms_host.c %XL6_CGEN_LINKS% -o build\test\xlir_string_literal_forms_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_string_literal_forms_host.exe
if errorlevel 1 exit /b 1

rem Managed parameters have a frontend-fixed borrowed ABI.  The caller-owned
rem local stays live across the call and is released only on the return edge.
release\xl.exe cgen test\cases\xlir_string_borrow_call.xl --backend xlir --emit build\test\xlir_string_borrow_call_generated.c
if errorlevel 1 exit /b 1
findstr /C:"static int64_t xlir_fn_1(char* s5);" build\test\xlir_string_borrow_call_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"= xlir_fn_1(" build\test\xlir_string_borrow_call_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtFree(s6); s6 = NULL;" build\test\xlir_string_borrow_call_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% test\cases\xlir_string_borrow_call_host.c %XL6_CGEN_LINKS% -o build\test\xlir_string_borrow_call_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_string_borrow_call_host.exe
if errorlevel 1 exit /b 1

rem Returning a managed local transfers its storage into the owned return
rem value.  XLIR emits MOVE, never a release before the return instruction.
release\xl.exe cgen test\cases\xlir_string_return_local.xl --backend xlir --emit build\test\xlir_string_return_local_generated.c
if errorlevel 1 exit /b 1
findstr /C:"s5 = NULL;" build\test\xlir_string_return_local_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"return v2;" build\test\xlir_string_return_local_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtFree(s5);" build\test\xlir_string_return_local_generated.c >nul
if not errorlevel 1 (
    echo XLIR managed return released its transferred local storage.
    exit /b 1
)

rem A managed call result passed to a borrowed parameter is materialized in
rem explicit synthetic storage, then released after the call returns.
release\xl.exe cgen test\cases\xlir_string_call_result_borrow.xl --backend xlir --emit build\test\xlir_string_call_result_borrow_generated.c
if errorlevel 1 exit /b 1
findstr /C:"s4294967295 = v" build\test\xlir_string_call_result_borrow_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"= s4294967295;" build\test\xlir_string_call_result_borrow_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtFree(s4294967295); s4294967295 = NULL;" build\test\xlir_string_call_result_borrow_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% test\cases\xlir_string_call_result_borrow_host.c %XL6_CGEN_LINKS% -o build\test\xlir_string_call_result_borrow_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_string_call_result_borrow_host.exe
if errorlevel 1 exit /b 1

rem Managed string operators become fixed XLIR runtime operations.  Cgen
rem receives no binary source operator or string type inference decision.
release\xl.exe cgen test\cases\xlir_string_ops.xl --backend xlir --emit build\test\xlir_string_ops_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xrtStrConcat(xrtStrView(v4), xrtStrView(v5));" build\test\xlir_string_ops_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtStrEqual(xrtStrView(v7), xrtStrView(v8));" build\test\xlir_string_ops_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% test\cases\xlir_string_ops_host.c %XL6_CGEN_LINKS% -o build\test\xlir_string_ops_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_string_ops_host.exe
if errorlevel 1 exit /b 1

rem A borrowed string assignment must duplicate before releasing the old
rem destination, so self-aliases and later source mutation remain sound.
release\xl.exe cgen test\cases\xlir_string_assign.xl --backend xlir --emit build\test\xlir_string_assign_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xrtStrDup(v4);" build\test\xlir_string_assign_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtFree(s4); s4 = NULL;" build\test\xlir_string_assign_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% test\cases\xlir_string_assign_host.c %XL6_CGEN_LINKS% -o build\test\xlir_string_assign_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_string_assign_host.exe
if errorlevel 1 exit /b 1

rem Self-assignment is the alias boundary for managed storage: XLIR must
rem duplicate the source before the destination release instruction.
release\xl.exe cgen test\cases\xlir_string_self_assign.xl --backend xlir --emit build\test\xlir_string_self_assign_generated.c
if errorlevel 1 exit /b 1
findstr /C:"v4 = xrtStrDup(v3);" build\test\xlir_string_self_assign_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtFree(s3); s3 = NULL;" build\test\xlir_string_self_assign_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% test\cases\xlir_string_self_assign_host.c %XL6_CGEN_LINKS% -o build\test\xlir_string_self_assign_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_string_self_assign_host.exe
if errorlevel 1 exit /b 1

rem Returning a borrowed managed parameter materializes an owned result in
rem the callee before the caller can overwrite or release its original local.
release\xl.exe cgen test\cases\xlir_string_return_borrowed_param.xl --backend xlir --emit build\test\xlir_string_return_borrowed_param_generated.c
if errorlevel 1 exit /b 1
findstr /C:"v2 = xrtStrDup(v1);" build\test\xlir_string_return_borrowed_param_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% test\cases\xlir_string_return_borrowed_param_host.c %XL6_CGEN_LINKS% -o build\test\xlir_string_return_borrowed_param_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_string_return_borrowed_param_host.exe
if errorlevel 1 exit /b 1

rem Move-owned string operands are materialized in explicit temporary storage
rem before runtime string operations borrow them; each temporary has an XLIR
rem release edge, so Cgen never guesses a transient result lifetime.
release\xl.exe cgen test\cases\xlir_string_temp_operands.xl --backend xlir --emit build\test\xlir_string_temp_operands_generated.c
if errorlevel 1 exit /b 1
findstr /C:"s4294967295 = v2;" build\test\xlir_string_temp_operands_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtFree(s4294967295); s4294967295 = NULL;" build\test\xlir_string_temp_operands_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtFree(s4294967293); s4294967293 = NULL;" build\test\xlir_string_temp_operands_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% test\cases\xlir_string_temp_operands_host.c %XL6_CGEN_LINKS% -o build\test\xlir_string_temp_operands_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_string_temp_operands_host.exe
if errorlevel 1 exit /b 1

rem Dynamic var storage is an explicit xvalue ABI: scalar boxing is selected
rem in XLIR, local return transfers the handle, and non-transferred locals
rem release their handle on the return edge.
release\xl.exe cgen test\cases\xlir_xvalue_basic.xl --backend xlir --emit build\test\xlir_xvalue_basic_generated.c
if errorlevel 1 exit /b 1
findstr /C:"v2 = xrtValueInt(v1);" build\test\xlir_xvalue_basic_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"s7 = NULL;" build\test\xlir_xvalue_basic_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtValueRelease(s8); s8 = NULL;" build\test\xlir_xvalue_basic_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% test\cases\xlir_xvalue_basic_host.c %XL6_CGEN_LINKS% -o build\test\xlir_xvalue_basic_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_xvalue_basic_host.exe
if errorlevel 1 exit /b 1

rem A borrowed dynamic parameter returning as var must retain its xvalue
rem before the caller can release the original local handle.
release\xl.exe cgen test\cases\xlir_xvalue_borrow_return.xl --backend xlir --emit build\test\xlir_xvalue_borrow_return_generated.c
if errorlevel 1 exit /b 1
findstr /C:"v2 = xrtValueRetain(v1);" build\test\xlir_xvalue_borrow_return_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtValueRelease(s8); s8 = NULL;" build\test\xlir_xvalue_borrow_return_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% test\cases\xlir_xvalue_borrow_return_host.c %XL6_CGEN_LINKS% -o build\test\xlir_xvalue_borrow_return_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_xvalue_borrow_return_host.exe
if errorlevel 1 exit /b 1

rem A dynamic array literal has an explicit construction and append-failure
rem contract in XLIR; Cgen only emits the prevalidated xvalue operations.
release\xl.exe cgen test\cases\xlir_xvalue_array_literal.xl --backend xlir --emit build\test\xlir_xvalue_array_literal_generated.c
if errorlevel 1 exit /b 1
findstr /C:"v1 = xrtValueArray();" build\test\xlir_xvalue_array_literal_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"v4 = xrtValueArrayAppendNew(v1, v3);" build\test\xlir_xvalue_array_literal_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"if (!v4) abort();" build\test\xlir_xvalue_array_literal_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% test\cases\xlir_xvalue_array_literal_host.c %XL6_CGEN_LINKS% -o build\test\xlir_xvalue_array_literal_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_xvalue_array_literal_host.exe
if errorlevel 1 exit /b 1

rem Dynamic assignment retains the borrowed RHS before it releases the old
rem destination handle, making aliases and self-assignment deterministic.
release\xl.exe cgen test\cases\xlir_xvalue_assign.xl --backend xlir --emit build\test\xlir_xvalue_assign_generated.c
if errorlevel 1 exit /b 1
findstr /C:"v6 = xrtValueRetain(v5);" build\test\xlir_xvalue_assign_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtValueRelease(s5); s5 = NULL;" build\test\xlir_xvalue_assign_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% test\cases\xlir_xvalue_assign_host.c %XL6_CGEN_LINKS% -o build\test\xlir_xvalue_assign_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_xvalue_assign_host.exe
if errorlevel 1 exit /b 1

:xvalue_index_writes
if /I "%~2"=="trace" echo on
rem An index lvalue is lowered to a concrete array-set runtime operation.  The
rem signed index is resolved before the owned RHS is transferred, and failure
rem is an explicit recoverable RangeError CFG edge rather than a Cgen trap.
release\xl.exe cgen test\cases\xlir_xvalue_array_set.xl --backend xlir --emit build\test\xlir_xvalue_array_set_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xrtValueArrayResolve(const xvalue*, int64_t, size_t*);" build\test\xlir_xvalue_array_set_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"v12 = xlir_xvalue_array_set_i64_new(v10, v11, v9);" build\test\xlir_xvalue_array_set_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"if (v12) goto b3; else goto b4;" build\test\xlir_xvalue_array_set_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtSetErrorTake(xlir_error_create_at(\"RangeError\", v13" build\test\xlir_xvalue_array_set_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% test\cases\xlir_xvalue_array_set_host.c %XL6_CGEN_LINKS% -o build\test\xlir_xvalue_array_set_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_xvalue_array_set_host.exe
if errorlevel 1 exit /b 1

rem A negative array write must resolve before the owned item is consumed, and
rem the following borrowed read must be retained before local cleanup runs.
release\xl.exe cgen test\cases\xlir_xvalue_array_set_read.xl --backend xlir --emit build\test\xlir_xvalue_array_set_read_generated.c
if errorlevel 1 exit /b 1
findstr /C:"v13 = xlir_xvalue_array_set_i64_new(v10, v12, v9);" build\test\xlir_xvalue_array_set_read_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"= xlir_xvalue_index_get_xvalue_key_or_null(" build\test\xlir_xvalue_array_set_read_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"= xrtValueRetain(" build\test\xlir_xvalue_array_set_read_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% test\cases\xlir_xvalue_array_set_read_host.c %XL6_CGEN_LINKS% -o build\test\xlir_xvalue_array_set_read_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_xvalue_array_set_read_host.exe
if errorlevel 1 exit /b 1

rem Dynamic array reads return a borrowed xvalue.  XLIR must retain it before
rem it releases the receiver array on the function return edge.
release\xl.exe cgen test\cases\xlir_xvalue_array_index.xl --backend xlir --emit build\test\xlir_xvalue_array_index_generated.c
if errorlevel 1 exit /b 1
findstr /C:"= xlir_xvalue_index_get_xvalue_key_or_null(" build\test\xlir_xvalue_array_index_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"= xrtValueRetain(" build\test\xlir_xvalue_array_index_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtValueRelease(s5); s5 = NULL;" build\test\xlir_xvalue_array_index_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% test\cases\xlir_xvalue_array_index_host.c %XL6_CGEN_LINKS% -o build\test\xlir_xvalue_array_index_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_xvalue_array_index_host.exe
if errorlevel 1 exit /b 1

rem Dynamic object literals lower their string keys into canonical immediate
rem payloads.  Cgen only emits the preselected object ABI and borrow retain.
release\xl.exe cgen test\cases\xlir_xvalue_object_probe.xl --backend xlir --emit build\test\xlir_xvalue_object_probe_generated.c
if errorlevel 1 exit /b 1
findstr /C:"v4 = xrtValueObjectSetNew(v1, xrtStrView(" build\test\xlir_xvalue_object_probe_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"= xlir_xvalue_dynamic_field_get_or_null(" build\test\xlir_xvalue_object_probe_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"v7 = xrtValueRetain(v6);" build\test\xlir_xvalue_object_probe_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% test\cases\xlir_xvalue_object_probe_host.c %XL6_CGEN_LINKS% -o build\test\xlir_xvalue_object_probe_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_xvalue_object_probe_host.exe
if errorlevel 1 exit /b 1

rem Dynamic member reads are lowered to the literal-key dynamic-field ABI before
rem Cgen.  The member spelling is a canonical XLIR immediate, not a Cgen lookup.
release\xl.exe cgen test\cases\xlir_xvalue_member_read.xl --backend xlir --emit build\test\xlir_xvalue_member_read_generated.c
if errorlevel 1 exit /b 1
findstr /C:"= xlir_xvalue_dynamic_field_get_or_null(" build\test\xlir_xvalue_member_read_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"v7 = xrtValueRetain(v6);" build\test\xlir_xvalue_member_read_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% test\cases\xlir_xvalue_member_read_host.c %XL6_CGEN_LINKS% -o build\test\xlir_xvalue_member_read_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_xvalue_member_read_host.exe
if errorlevel 1 exit /b 1

rem Dynamic member writes become an explicit owned dynamic-field-set operation and a
rem checked trap edge in XLIR; Cgen has no lvalue or member classification work.
release\xl.exe cgen test\cases\xlir_xvalue_member_set.xl --backend xlir --emit build\test\xlir_xvalue_member_set_generated.c
if errorlevel 1 exit /b 1
findstr /C:"v8 = xlir_xvalue_dynamic_field_set_new(v7, xrtStrView(" build\test\xlir_xvalue_member_set_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"if (!v8) abort();" build\test\xlir_xvalue_member_set_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"v11 = xrtValueRetain(v10);" build\test\xlir_xvalue_member_set_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% test\cases\xlir_xvalue_member_set_host.c %XL6_CGEN_LINKS% -o build\test\xlir_xvalue_member_set_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_xvalue_member_set_host.exe
if errorlevel 1 exit /b 1

rem Nested dynamic member chains are recursively lowered to dynamic-field get/set
rem operations before Cgen; a borrowed intermediate is never reclassified there.
release\xl.exe cgen test\cases\xlir_xvalue_member_nested.xl --backend xlir --emit build\test\xlir_xvalue_member_nested_generated.c
if errorlevel 1 exit /b 1
findstr /C:"v11 = xlir_xvalue_dynamic_field_set_new(v10, xrtStrView(" build\test\xlir_xvalue_member_nested_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"v14 = xlir_xvalue_dynamic_field_get_or_null(v13, xrtStrView(" build\test\xlir_xvalue_member_nested_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"v15 = xrtValueRetain(v14);" build\test\xlir_xvalue_member_nested_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% test\cases\xlir_xvalue_member_nested_host.c %XL6_CGEN_LINKS% -o build\test\xlir_xvalue_member_nested_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_xvalue_member_nested_host.exe
if errorlevel 1 exit /b 1

rem Null enters dynamic storage through an explicit xvalue-null operation;
rem safe member access receives the runtime-normalized null singleton.
release\xl.exe cgen test\cases\xlir_xvalue_safe_member.xl --backend xlir --emit build\test\xlir_xvalue_safe_member_generated.c
if errorlevel 1 exit /b 1
findstr /C:"v1 = xrtValueRetain(xrtValueNull());" build\test\xlir_xvalue_safe_member_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"v3 = xlir_xvalue_dynamic_field_get_or_null(v2, xrtStrView(" build\test\xlir_xvalue_safe_member_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% test\cases\xlir_xvalue_safe_member_host.c %XL6_CGEN_LINKS% -o build\test\xlir_xvalue_safe_member_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_xvalue_safe_member_host.exe
if errorlevel 1 exit /b 1

rem Nested collections choose xvalue-null while lowering their elements, not
rem by manufacturing an untyped C null in the backend.
release\xl.exe cgen test\cases\xlir_xvalue_null_collection.xl --backend xlir --emit build\test\xlir_xvalue_null_collection_generated.c
if errorlevel 1 exit /b 1
findstr /C:"v3 = xrtValueRetain(xrtValueNull());" build\test\xlir_xvalue_null_collection_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"v4 = xrtValueArrayAppendNew(v2, v3);" build\test\xlir_xvalue_null_collection_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% test\cases\xlir_xvalue_null_collection_host.c %XL6_CGEN_LINKS% -o build\test\xlir_xvalue_null_collection_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_xvalue_null_collection_host.exe
if errorlevel 1 exit /b 1

rem A direct var reassignment lowers the null value before releasing the old
rem owned slot; Cgen only follows those explicit XLIR instructions.
release\xl.exe cgen test\cases\xlir_xvalue_null_assign.xl --backend xlir --emit build\test\xlir_xvalue_null_assign_generated.c
if errorlevel 1 exit /b 1
findstr /C:"v3 = xrtValueRetain(xrtValueNull());" build\test\xlir_xvalue_null_assign_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtValueRelease(s5); s5 = NULL;" build\test\xlir_xvalue_null_assign_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"s5 = v3;" build\test\xlir_xvalue_null_assign_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% test\cases\xlir_xvalue_null_assign_host.c %XL6_CGEN_LINKS% -o build\test\xlir_xvalue_null_assign_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_xvalue_null_assign_host.exe
if errorlevel 1 exit /b 1

rem Literal-key object assignment remains an explicit owned-value operation;
rem no container dispatch or assignment syntax reaches Cgen.
release\xl.exe cgen test\cases\xlir_xvalue_object_set.xl --backend xlir --emit build\test\xlir_xvalue_object_set_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xrtValueObjectSetNew(" build\test\xlir_xvalue_object_set_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtValueObjectGet(" build\test\xlir_xvalue_object_set_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% test\cases\xlir_xvalue_object_set_host.c %XL6_CGEN_LINKS% -o build\test\xlir_xvalue_object_set_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_xvalue_object_set_host.exe
if errorlevel 1 exit /b 1

rem Missing object keys have the XLIR null singleton contract instead of
rem leaking the runtime's nullable C lookup pointer to generated code.
release\xl.exe cgen test\cases\xlir_xvalue_object_missing.xl --backend xlir --emit build\test\xlir_xvalue_object_missing_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xlir_xvalue_dynamic_field_get_or_null" build\test\xlir_xvalue_object_missing_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"return item != NULL ? item : xrtValueNull();" build\test\xlir_xvalue_object_missing_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% test\cases\xlir_xvalue_object_missing_host.c %XL6_CGEN_LINKS% -o build\test\xlir_xvalue_object_missing_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_xvalue_object_missing_host.exe
if errorlevel 1 exit /b 1

rem A non-literal string object key is lowered to its own runtime operation.
rem Cgen receives the already typed xvalue receiver and managed-string key.
release\xl.exe cgen test\cases\xlir_xvalue_object_variable_key.xl --backend xlir --emit build\test\xlir_xvalue_object_variable_key_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xlir_xvalue_dynamic_field_get_or_null" build\test\xlir_xvalue_object_variable_key_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"v8 = xlir_xvalue_dynamic_field_get_or_null(v6, xrtStrView(v7));" build\test\xlir_xvalue_object_variable_key_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% test\cases\xlir_xvalue_object_variable_key_host.c %XL6_CGEN_LINKS% -o build\test\xlir_xvalue_object_variable_key_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_xvalue_object_variable_key_host.exe
if errorlevel 1 exit /b 1

rem Dynamic string-key writes also use a distinct owned-item runtime ABI.
rem Their recoverable RangeError branch is explicit XLIR; Cgen has no lvalue dispatch fallback.
release\xl.exe cgen test\cases\xlir_xvalue_object_variable_key_set.xl --backend xlir --emit build\test\xlir_xvalue_object_variable_key_set_generated.c
if errorlevel 1 exit /b 1
findstr /C:"v10 = xlir_xvalue_dynamic_field_set_new(v8, xrtStrView(v9), v7);" build\test\xlir_xvalue_object_variable_key_set_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"if (v10) goto b3; else goto b4;" build\test\xlir_xvalue_object_variable_key_set_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtSetErrorTake(xlir_error_create_at(\"RangeError\", v11" build\test\xlir_xvalue_object_variable_key_set_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"v14 = xlir_xvalue_dynamic_field_get_or_null(v12, xrtStrView(v13));" build\test\xlir_xvalue_object_variable_key_set_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% test\cases\xlir_xvalue_object_variable_key_set_host.c %XL6_CGEN_LINKS% -o build\test\xlir_xvalue_object_variable_key_set_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_xvalue_object_variable_key_set_host.exe
if errorlevel 1 exit /b 1

rem Managed strings enter dynamic containers through a dedicated copy operation;
rem Cgen receives the resolved xvalue-string ABI, never a source type fallback.
release\xl.exe cgen test\cases\xlir_xvalue_object_string.xl --backend xlir --emit build\test\xlir_xvalue_object_string_generated.c
if errorlevel 1 exit /b 1
findstr /C:"v3 = xrtValueString(xrtStrView(v2));" build\test\xlir_xvalue_object_string_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"v6 = xlir_xvalue_dynamic_field_get_or_null(v5, xrtStrView(" build\test\xlir_xvalue_object_string_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% test\cases\xlir_xvalue_object_string_host.c %XL6_CGEN_LINKS% -o build\test\xlir_xvalue_object_string_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_xvalue_object_string_host.exe
if errorlevel 1 exit /b 1

rem Dynamic direct calls use the resolved function ABI.  The callee retains
rem its borrowed var parameter before returning, so caller cleanup is safe.
release\xl.exe cgen test\cases\xlir_xvalue_direct_call.xl --backend xlir --emit build\test\xlir_xvalue_direct_call_generated.c
if errorlevel 1 exit /b 1
findstr /C:"static xvalue* xlir_fn_1(xvalue* s7);" build\test\xlir_xvalue_direct_call_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"v2 = xrtValueRetain(v1);" build\test\xlir_xvalue_direct_call_generated.c >nul
if errorlevel 1 exit /b 1
rem The resolved callee is ABI, while ValueId numbering is not.
findstr /C:"xlir_fn_1(" build\test\xlir_xvalue_direct_call_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% test\cases\xlir_xvalue_direct_call_host.c %XL6_CGEN_LINKS% -o build\test\xlir_xvalue_direct_call_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_xvalue_direct_call_host.exe
if errorlevel 1 exit /b 1

rem Nested source collections lower recursively to owned xvalue containers at
rem the conversion boundary; Cgen never bridges a typed-container intermediate.
release\xl.exe cgen test\cases\xlir_xvalue_nested_collection.xl --backend xlir --emit build\test\xlir_xvalue_nested_collection_generated.c
if errorlevel 1 exit /b 1
findstr /C:"v2 = xrtValueArray();" build\test\xlir_xvalue_nested_collection_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"v9 = xrtValueObjectSetNew(v1, xrtStrView(" build\test\xlir_xvalue_nested_collection_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"v11 = xlir_xvalue_dynamic_field_get_or_null(v10, xrtStrView(" build\test\xlir_xvalue_nested_collection_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% test\cases\xlir_xvalue_nested_collection_host.c %XL6_CGEN_LINKS% -o build\test\xlir_xvalue_nested_collection_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_xvalue_nested_collection_host.exe
if errorlevel 1 exit /b 1

rem The semantic range builtin is normalized into explicit scalar storage,
rem sign-aware CFG branches, and an increment block before Cgen runs.
release\xl.exe cgen test\cases\xlir_for_range_scalar.xl --backend xlir --emit build\test\xlir_for_range_scalar_generated.c
if errorlevel 1 exit /b 1
findstr /C:"if (v7) goto b4; else goto b5;" build\test\xlir_for_range_scalar_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"v19 = v17 + v18;" build\test\xlir_for_range_scalar_generated.c >nul
if errorlevel 1 (
    echo XLIR range output is missing its explicit step and increment CFG.
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% test\cases\xlir_for_range_scalar_host.c %XL6_CGEN_LINKS% -o build\test\xlir_for_range_scalar_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_for_range_scalar_host.exe
if errorlevel 1 exit /b 1

rem Negative range steps share the same lowered CFG contract but take the
rem explicit descending comparison path; no Cgen-side loop interpretation.
release\xl.exe cgen test\cases\xlir_for_range_negative_step.xl --backend xlir --emit build\test\xlir_for_range_negative_step_generated.c
if errorlevel 1 exit /b 1
findstr /R /C:"v[0-9][0-9]* = v[0-9][0-9]* > v[0-9][0-9]*;" build\test\xlir_for_range_negative_step_generated.c >nul
if errorlevel 1 (
    echo XLIR negative range output is missing its descending CFG branch.
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% test\cases\xlir_for_range_negative_step_host.c %XL6_CGEN_LINKS% -o build\test\xlir_for_range_negative_step_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_for_range_negative_step_host.exe
if errorlevel 1 exit /b 1

rem break and continue are resolved against the range loop region in XLIR.
rem Cgen receives only jumps to the pre-materialized increment and exit blocks.
release\xl.exe cgen test\cases\xlir_for_range_control.xl --backend xlir --emit build\test\xlir_for_range_control_generated.c
if errorlevel 1 exit /b 1
findstr /C:"b9:" build\test\xlir_for_range_control_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"goto b7;" build\test\xlir_for_range_control_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"goto b8;" build\test\xlir_for_range_control_generated.c >nul
if errorlevel 1 (
    echo XLIR range control output is missing resolved loop exit targets.
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% test\cases\xlir_for_range_control_host.c %XL6_CGEN_LINKS% -o build\test\xlir_for_range_control_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_for_range_control_host.exe
if errorlevel 1 exit /b 1

rem A resolved void call has no result value ABI.  The caller and callee both
rem use explicit function and global-storage symbols; Cgen does no inference.
release\xl.exe cgen test\cases\xlir_void_call_global.xl --backend xlir --emit build\test\xlir_void_call_global_generated.c
if errorlevel 1 exit /b 1
findstr /C:"static void xlir_fn_1(void);" build\test\xlir_void_call_global_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"    xlir_fn_1();" build\test\xlir_void_call_global_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"    s5 = v4;" build\test\xlir_void_call_global_generated.c >nul
if errorlevel 1 (
    echo XLIR void-call output is missing explicit call or global storage ABI.
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% test\cases\xlir_void_call_global_host.c %XL6_CGEN_LINKS% -o build\test\xlir_void_call_global_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_void_call_global_host.exe
if errorlevel 1 exit /b 1

rem A non-void expression statement is lowered to an explicit DISCARD after
rem its resolved call result, rather than becoming Cgen's unused-value guess.
release\xl.exe cgen test\cases\xlir_discard_call.xl --backend xlir --emit build\test\xlir_discard_call_generated.c
if errorlevel 1 exit /b 1
findstr /C:"    v6 = xlir_fn_1();" build\test\xlir_discard_call_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"    (void)v6;" build\test\xlir_discard_call_generated.c >nul
if errorlevel 1 (
    echo XLIR discarded-call output is missing explicit result consumption.
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% test\cases\xlir_discard_call_host.c %XL6_CGEN_LINKS% -o build\test\xlir_discard_call_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_discard_call_host.exe
if errorlevel 1 exit /b 1

rem Scalar literal match cases are lowered into EQ/BRANCH CFG before Cgen;
rem the emitter receives no source pattern or match semantics.
release\xl.exe cgen test\cases\xlir_match_scalar.xl --backend xlir --emit build\test\xlir_match_scalar_generated.c
if errorlevel 1 exit /b 1
findstr /C:"v3 = v1 == v2;" build\test\xlir_match_scalar_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"v6 = v1 == v5;" build\test\xlir_match_scalar_generated.c >nul
if errorlevel 1 (
    echo XLIR scalar match output is missing canonical comparison CFG.
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% test\cases\xlir_match_scalar_host.c %XL6_CGEN_LINKS% -o build\test\xlir_match_scalar_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_match_scalar_host.exe
if errorlevel 1 exit /b 1
rem String literal match is explicit STRING_EQUAL CFG.  Move-produced pattern
rem strings are materialized into compiler storage and released after compare.
release\xl.exe cgen test\cases\xlir_match_string.xl --backend xlir --emit build\test\xlir_match_string_generated.c
if errorlevel 1 exit /b 1
findstr /C:"= xrtStrEqual(" build\test\xlir_match_string_generated.c | find /C "=" | findstr /X "2" >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtFree(s4294967295); s4294967295 = NULL;" build\test\xlir_match_string_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtFree(s3); s3 = NULL;" build\test\xlir_match_string_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtFree(s4294967294); s4294967294 = NULL;" build\test\xlir_match_string_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_match_string_host.c %XL6_CGEN_LINKS% -o build\test\xlir_match_string_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_match_string_host.exe
if errorlevel 1 exit /b 1

rem Branches carry their condition and terminated paths in XLIR CFG; Cgen
rem only emits those explicit blocks and jumps.
release\xl.exe cgen test\cases\xlir_if_scalar.xl --backend xlir --emit build\test\xlir_if_scalar_generated.c
if errorlevel 1 exit /b 1
findstr /C:"if (v1) goto b3; else goto b4;" build\test\xlir_if_scalar_generated.c >nul
if errorlevel 1 (
    echo XLIR if output is missing its explicit branch targets.
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% test\cases\xlir_if_scalar_host.c %XL6_CGEN_LINKS% -o build\test\xlir_if_scalar_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_if_scalar_host.exe
if errorlevel 1 exit /b 1

rem Elseif chains are normalized to branch blocks during XLIR CFG lowering;
rem the C emitter must never receive an AST-shaped conditional chain.
release\xl.exe cgen test\cases\xlir_elseif_scalar.xl --backend xlir --emit build\test\xlir_elseif_scalar_generated.c
if errorlevel 1 exit /b 1
findstr /C:"if (v3) goto b3; else goto b4;" build\test\xlir_elseif_scalar_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"if (v8) goto b6; else goto b7;" build\test\xlir_elseif_scalar_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% test\cases\xlir_elseif_scalar_host.c %XL6_CGEN_LINKS% -o build\test\xlir_elseif_scalar_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_elseif_scalar_host.exe
if errorlevel 1 exit /b 1

rem Short-circuit uses explicit XLIR branch/value-merge CFG so evaluation
rem order is fixed before C emission.
release\xl.exe cgen test\cases\xlir_short_circuit_scalar.xl --backend xlir --emit build\test\xlir_short_circuit_scalar_generated.c
if errorlevel 1 exit /b 1
findstr /C:"if (v1) goto b3; else goto b4;" build\test\xlir_short_circuit_scalar_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"if (v4) goto b7; else goto b6;" build\test\xlir_short_circuit_scalar_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% test\cases\xlir_short_circuit_scalar_host.c %XL6_CGEN_LINKS% -o build\test\xlir_short_circuit_scalar_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_short_circuit_scalar_host.exe
if errorlevel 1 exit /b 1

rem Conditional expressions lower to explicit temporary storage and a merge
rem block; Cgen receives no source-level ternary decision.
release\xl.exe cgen test\cases\xlir_conditional_scalar.xl --backend xlir --emit build\test\xlir_conditional_scalar_generated.c
if errorlevel 1 exit /b 1
findstr /C:"s4294967295 = v2;" build\test\xlir_conditional_scalar_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"v4 = s4294967295;" build\test\xlir_conditional_scalar_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% test\cases\xlir_conditional_scalar_host.c %XL6_CGEN_LINKS% -o build\test\xlir_conditional_scalar_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_conditional_scalar_host.exe
if errorlevel 1 exit /b 1

rem Managed conditional values converge through one typed temporary storage
rem slot.  Each branch transfers its owned string to that slot before the
rem merged value becomes the function return ABI.
release\xl.exe cgen test\cases\xlir_conditional_string.xl --backend xlir --emit build\test\xlir_conditional_string_generated.c
if errorlevel 1 exit /b 1
findstr /C:"if (v7) goto b5; else goto b6;" build\test\xlir_conditional_string_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"s4294967294 = v8;" build\test\xlir_conditional_string_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"s4294967294 = v9;" build\test\xlir_conditional_string_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_conditional_string_host.c %XL6_CGEN_LINKS% -o build\test\xlir_conditional_string_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_conditional_string_host.exe
if errorlevel 1 exit /b 1

rem A typed collection chosen by a conditional is converted at the var return
rem boundary, so both branches materialize the xvalue object ABI in XLIR.
release\xl.exe cgen test\cases\xlir_conditional_xvalue.xl --backend xlir --emit build\test\xlir_conditional_xvalue_generated.c
if errorlevel 1 exit /b 1
findstr /C:"v4 = xrtValueObject();" build\test\xlir_conditional_xvalue_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"v8 = xrtValueObject();" build\test\xlir_conditional_xvalue_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_conditional_xvalue_host.c %XL6_CGEN_LINKS% -o build\test\xlir_conditional_xvalue_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_conditional_xvalue_host.exe
if errorlevel 1 exit /b 1

rem Module globals are declared as explicit storage and initialized by the
rem synthetic XLIR module-init function before user functions read them.
release\xl.exe cgen test\cases\xlir_global_scalar.xl --backend xlir --emit build\test\xlir_global_scalar_generated.c
if errorlevel 1 exit /b 1
findstr /C:"s3 = v1;" build\test\xlir_global_scalar_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"v2 = s3;" build\test\xlir_global_scalar_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% test\cases\xlir_global_scalar_host.c %XL6_CGEN_LINKS% -o build\test\xlir_global_scalar_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_global_scalar_host.exe
if errorlevel 1 exit /b 1

rem Break and continue carry resolved loop-region targets in XLIR and lower
rem to explicit jumps, never to a Cgen-maintained loop stack.
release\xl.exe cgen test\cases\xlir_loop_exit_scalar.xl --backend xlir --emit build\test\xlir_loop_exit_scalar_generated.c
if errorlevel 1 exit /b 1
findstr /C:"if (v5) goto b4; else goto b5;" build\test\xlir_loop_exit_scalar_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"goto b3;" build\test\xlir_loop_exit_scalar_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% test\cases\xlir_loop_exit_scalar_host.c %XL6_CGEN_LINKS% -o build\test\xlir_loop_exit_scalar_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_loop_exit_scalar_host.exe
if errorlevel 1 exit /b 1

rem F64 C ABI is fixed in XLIR types and mechanically emitted as double.
release\xl.exe cgen test\cases\xlir_float_scalar.xl --backend xlir --emit build\test\xlir_float_scalar_generated.c
if errorlevel 1 exit /b 1
findstr /C:"static double xlir_fn_1(void);" build\test\xlir_float_scalar_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"v5 = v3 / v4;" build\test\xlir_float_scalar_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% test\cases\xlir_float_scalar_host.c %XL6_CGEN_LINKS% -o build\test\xlir_float_scalar_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_float_scalar_host.exe
if errorlevel 1 exit /b 1

rem Semantic builtin conversion is a checked f64-to-i64 XLIR runtime operation.
rem Its explicit ERROR_TEST owns the success/failure CFG; Cgen never recovers
rem conversion behavior from a callee spelling or emits an unchecked C cast.
release\xl.exe cgen test\cases\xlir_float_to_int_scalar.xl --backend xlir --emit build\test\xlir_float_to_int_scalar_generated.c
if errorlevel 1 exit /b 1
rem The checked conversion ABI carries the resolved integer-width tag; Cgen
rem receives it from XLIR rather than reconstructing a destination type.
findstr /C:"xlir_f64_to_i64_checked(v1, 7);" build\test\xlir_float_to_int_scalar_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% test\cases\xlir_float_to_int_scalar_host.c %XL6_CGEN_LINKS% -o build\test\xlir_float_to_int_scalar_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_float_to_int_scalar_host.exe
if errorlevel 1 exit /b 1

rem A checked conversion failure must be caught on XLIR's explicit error edge,
rem rather than relying on implementation-defined floating-point casts.
release\xl.exe cgen test\cases\xlir_float_to_int_checked_catch.xl --backend xlir --emit build\test\xlir_float_to_int_checked_catch_generated.c
if errorlevel 1 exit /b 1
findstr /C:"if (xrtGetError() == NULL)" build\test\xlir_float_to_int_checked_catch_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% test\cases\xlir_float_to_int_checked_catch_host.c %XL6_CGEN_LINKS% -o build\test\xlir_float_to_int_checked_catch_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_float_to_int_checked_catch_host.exe
if errorlevel 1 exit /b 1

rem Dynamic integer conversion has the same verified XLIR operation, selected
rem from the xvalue operand ABI rather than source-level type/callee spelling.
release\xl.exe cgen test\cases\xlir_dynamic_int_convert.xl --backend xlir --emit build\test\xlir_dynamic_int_convert_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xlir_xvalue_to_i64_checked" build\test\xlir_dynamic_int_convert_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% test\cases\xlir_dynamic_int_convert_host.c %XL6_CGEN_LINKS% -o build\test\xlir_dynamic_int_convert_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_dynamic_int_convert_host.exe
if errorlevel 1 exit /b 1

rem Dynamic parse failure travels through ERROR_TEST and a typed ValueError catch.
release\xl.exe cgen test\cases\xlir_dynamic_int_convert_failure.xl --backend xlir --emit build\test\xlir_dynamic_int_convert_failure_generated.c
if errorlevel 1 exit /b 1
findstr /C:"dynamic value cannot convert to integer" build\test\xlir_dynamic_int_convert_failure_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% test\cases\xlir_dynamic_int_convert_failure_host.c %XL6_CGEN_LINKS% -o build\test\xlir_dynamic_int_convert_failure_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_dynamic_int_convert_failure_host.exe
if errorlevel 1 exit /b 1

rem Dynamic collection projection is a verified XLIR operation.  The generated
rem C consumes the collection/item ABI carried by IR, and a failed projection
rem must leave the previous typed destination intact before entering catch.
release\xl.exe cgen test\cases\xlir_dynamic_typed_collection_projection.xl --backend xlir --emit build\test\xlir_dynamic_typed_collection_projection_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xlir_xvalue_collection_checked_take" build\test\xlir_dynamic_typed_collection_projection_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"dynamic container contains an incompatible value" build\test\xlir_dynamic_typed_collection_projection_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_dynamic_typed_collection_projection_host.c %XL6_CGEN_LINKS% -o build\test\xlir_dynamic_typed_collection_projection_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_dynamic_typed_collection_projection_host.exe
if errorlevel 1 exit /b 1

rem The complete dynamic-container fixture covers all four collection ABIs,
rem module/class/argument conversion sites, failure atomicity, and the explicit
rem string-split-lines compiler-runtime bridge.  Generated C must be standalone
rem apart from XRT; linking xl.dll would hide a missing bridge implementation.
release\xl.exe cgen test\cases\cgen_dynamic_to_typed_containers.xl --backend xlir --emit build\test\cgen_dynamic_to_typed_containers_xlir_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xvalue* xl6_xrt_string_split_lines(const char* text)" build\test\cgen_dynamic_to_typed_containers_xlir_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtStrSplitLines(xrtStrView(text))" build\test\cgen_dynamic_to_typed_containers_xlir_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:", 9, 0);" build\test\cgen_dynamic_to_typed_containers_xlir_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:", 9, 2);" build\test\cgen_dynamic_to_typed_containers_xlir_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:", 10, 4);" build\test\cgen_dynamic_to_typed_containers_xlir_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:", 11, 2);" build\test\cgen_dynamic_to_typed_containers_xlir_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:", 12, 2);" build\test\cgen_dynamic_to_typed_containers_xlir_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror build\test\cgen_dynamic_to_typed_containers_xlir_generated.c test\cases\xlir_dynamic_to_typed_containers_host.c %XL6_CGEN_LINKS% -o build\test\cgen_dynamic_to_typed_containers_xlir.exe
if errorlevel 1 exit /b 1
build\test\cgen_dynamic_to_typed_containers_xlir.exe
if errorlevel 1 exit /b 1

rem integerTarget carries sign/width into the same checked operation: no C cast
rem may truncate a dynamic value before RangeError reaches the typed catch edge.
release\xl.exe cgen test\cases\xlir_dynamic_int_width_convert.xl --backend xlir --emit build\test\xlir_dynamic_int_width_convert_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xlir_xvalue_to_i64_checked(v3, 1);" build\test\xlir_dynamic_int_width_convert_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xlir_xvalue_to_i64_checked(v7, 2);" build\test\xlir_dynamic_int_width_convert_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% test\cases\xlir_dynamic_int_width_convert_host.c %XL6_CGEN_LINKS% -o build\test\xlir_dynamic_int_width_convert_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_dynamic_int_width_convert_host.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\xlir_dynamic_int_width_failure.xl --backend xlir --emit build\test\xlir_dynamic_int_width_failure_generated.c
if errorlevel 1 exit /b 1
findstr /C:"integer value is outside target range" build\test\xlir_dynamic_int_width_failure_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% test\cases\xlir_dynamic_int_width_failure_host.c %XL6_CGEN_LINKS% -o build\test\xlir_dynamic_int_width_failure_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_dynamic_int_width_failure_host.exe
if errorlevel 1 exit /b 1

rem The same integerTarget ABI covers all lossless i64 execution targets through
rem 32 bits; boundary values must survive without C-level narrowing.
release\xl.exe cgen test\cases\xlir_dynamic_wide_int_convert.xl --backend xlir --emit build\test\xlir_dynamic_wide_int_convert_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xlir_xvalue_to_i64_checked(v3, 3);" build\test\xlir_dynamic_wide_int_convert_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xlir_xvalue_to_i64_checked(v15, 6);" build\test\xlir_dynamic_wide_int_convert_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% test\cases\xlir_dynamic_wide_int_convert_host.c %XL6_CGEN_LINKS% -o build\test\xlir_dynamic_wide_int_convert_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_dynamic_wide_int_convert_host.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\xlir_dynamic_wide_int_failure.xl --backend xlir --emit build\test\xlir_dynamic_wide_int_failure_generated.c
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% test\cases\xlir_dynamic_wide_int_failure_host.c %XL6_CGEN_LINKS% -o build\test\xlir_dynamic_wide_int_failure_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_dynamic_wide_int_failure_host.exe
if errorlevel 1 exit /b 1

rem Supported bounded integer declarations keep distinct semantic types but
rem compare through the verified sign-extended i64 execution ABI.  This is a
rem core XLIR operator rule, not an implicit C promotion in the emitter.
release\xl.exe cgen test\cases\xlir_dynamic_width_compare.xl --backend xlir --emit build\test\xlir_dynamic_width_compare_generated.c
if errorlevel 1 exit /b 1
findstr /R /C:"v[0-9]* = v[0-9]* == v[0-9]*;" build\test\xlir_dynamic_width_compare_generated.c | find /C "==" | findstr /X "2" >nul
if errorlevel 1 exit /b 1
findstr /C:"= xlir_xvalue_to_i64_checked(" build\test\xlir_dynamic_width_compare_generated.c | find /C "=" | findstr /X "2" >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_dynamic_width_compare_host.c %XL6_CGEN_LINKS% -o build\test\xlir_dynamic_width_compare_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_dynamic_width_compare_host.exe
if errorlevel 1 exit /b 1

:typed_array_reads
if /I "%~2"=="trace" echo on
rem A typed array keeps its explicit xvalue container handle while int reads
rem unbox through a dedicated op and writes explicitly box before the
rem consuming set operation. No C array cast or generic-type recovery is used.
release\xl.exe cgen test\cases\xlir_typed_int_array_read.xl --backend xlir --emit build\test\xlir_typed_int_array_read_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xlir_xvalue_array_at_i64_integer" build\test\xlir_typed_int_array_read_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xlir_xvalue_array_set_i64_new" build\test\xlir_typed_int_array_read_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_typed_int_array_read_host.c %XL6_CGEN_LINKS% -o build\test\xlir_typed_int_array_read_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_typed_int_array_read_host.exe
if errorlevel 1 exit /b 1

rem A typed bool array must use its boolean-tag ABI rather than the i64 helper.
release\xl.exe cgen test\cases\xlir_typed_bool_array_read.xl --backend xlir --emit build\test\xlir_typed_bool_array_read_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xlir_xvalue_array_at_i64_bool" build\test\xlir_typed_bool_array_read_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtValueBool" build\test\xlir_typed_bool_array_read_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_typed_bool_array_read_host.c %XL6_CGEN_LINKS% -o build\test\xlir_typed_bool_array_read_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_typed_bool_array_read_host.exe
if errorlevel 1 exit /b 1

rem A typed float array must check its float tag and return the f64 ABI.
release\xl.exe cgen test\cases\xlir_typed_float_array_read.xl --backend xlir --emit build\test\xlir_typed_float_array_read_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xlir_xvalue_array_at_i64_f64" build\test\xlir_typed_float_array_read_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtValueFloat" build\test\xlir_typed_float_array_read_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_typed_float_array_read_host.c %XL6_CGEN_LINKS% -o build\test\xlir_typed_float_array_read_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_typed_float_array_read_host.exe
if errorlevel 1 exit /b 1

rem A typed string array uses the same explicit container ABI, but its read
rem contract makes an owned managed-string copy; no borrowed xvalue text may
rem escape into an XLIR string value.
release\xl.exe cgen test\cases\xlir_typed_string_array_read.xl --backend xlir --emit build\test\xlir_typed_string_array_read_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xlir_xvalue_array_at_i64_string_copy" build\test\xlir_typed_string_array_read_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xlir_xvalue_array_set_i64_new" build\test\xlir_typed_string_array_read_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_typed_string_array_read_host.c %XL6_CGEN_LINKS% -o build\test\xlir_typed_string_array_read_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_typed_string_array_read_host.exe
if errorlevel 1 exit /b 1

rem The existing var-conversion regression also crosses the lowered varargs
rem runtime-print bridge.  XLIR carries array<var> as an explicit xvalue ABI;
rem Cgen does not reopen the standard-library body or infer its ownership.
release\xl.exe cgen test\cases\cgen_var_numeric_convert.xl --backend xlir --emit build\test\cgen_var_numeric_convert_xlir_generated.c
if errorlevel 1 exit /b 1
findstr /C:"void xl6_xrt_print(xvalue* values, bool newline)" build\test\cgen_var_numeric_convert_xlir_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"extern void xl6_xrt_print(xvalue*, bool);" build\test\cgen_var_numeric_convert_xlir_generated.c >nul
if not errorlevel 1 exit /b 1
findstr /C:"xl6_xrt_print(v" build\test\cgen_var_numeric_convert_xlir_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_var_numeric_convert_host.c %XL6_CGEN_LINKS% -o build\test\xlir_var_numeric_convert_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_var_numeric_convert_host.exe
if errorlevel 1 exit /b 1

rem Do-while preserves its body-first CFG in XLIR; the condition branch is
rem emitted after the body and owns the back edge.
release\xl.exe cgen test\cases\xlir_do_while_scalar.xl --backend xlir --emit build\test\xlir_do_while_scalar_generated.c
if errorlevel 1 exit /b 1
findstr /C:"if (v7) goto b3; else goto b5;" build\test\xlir_do_while_scalar_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% test\cases\xlir_do_while_scalar_host.c %XL6_CGEN_LINKS% -o build\test\xlir_do_while_scalar_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_do_while_scalar_host.exe
if errorlevel 1 exit /b 1

rem A read-only lexical local uses the same explicit capture environment ABI
rem as a parameter; it must not be read from an enclosing C stack frame.
release\xl.exe cgen test\cases\xlir_closure_local_capture.xl --backend xlir --emit build\test\xlir_closure_local_capture_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xlir_closure_env_" build\test\xlir_closure_local_capture_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"environment)->c0" build\test\xlir_closure_local_capture_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_closure_local_capture_host.c %XL6_CGEN_LINKS% -o build\test\xlir_closure_local_capture_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_closure_local_capture_host.exe
if errorlevel 1 exit /b 1

rem Mutable scalar captures are shared cells: construction, access, capture
rem retain and environment destruction are all explicit XLIR/C ABIs.
release\xl.exe cgen test\cases\xlir_closure_mutable_capture.xl --backend xlir --emit build\test\xlir_closure_mutable_capture_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xlir_cell_store_i64" build\test\xlir_closure_mutable_capture_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xlir_cell_release(environment->c0);" build\test\xlir_closure_mutable_capture_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_closure_mutable_capture_host.c %XL6_CGEN_LINKS% -o build\test\xlir_closure_mutable_capture_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_closure_mutable_capture_host.exe
if errorlevel 1 exit /b 1

rem A mutable parameter is promoted at function entry to local cell storage;
rem the public C parameter ABI remains scalar and no closure borrows it.
release\xl.exe cgen test\cases\xlir_closure_mutable_parameter_capture.xl --backend xlir --emit build\test\xlir_closure_mutable_parameter_capture_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xlir_cell_new_i64(v" build\test\xlir_closure_mutable_parameter_capture_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xlir_cell_retain(s429" build\test\xlir_closure_mutable_parameter_capture_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_closure_mutable_parameter_capture_host.c %XL6_CGEN_LINKS% -o build\test\xlir_closure_mutable_parameter_capture_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_closure_mutable_parameter_capture_host.exe
if errorlevel 1 exit /b 1

rem A string cell owns its element.  Replacement consumes the new owned value,
rem frees the old element, and return copies before the creator releases cell.
release\xl.exe cgen test\cases\xlir_closure_mutable_string_capture.xl --backend xlir --emit build\test\xlir_closure_mutable_string_capture_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xlir_cell_store_string" build\test\xlir_closure_mutable_string_capture_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtFree(cell->value.string)" build\test\xlir_closure_mutable_string_capture_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_closure_mutable_string_capture_host.c %XL6_CGEN_LINKS% -o build\test\xlir_closure_mutable_string_capture_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_closure_mutable_string_capture_host.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\xlir_closure_mutable_string_parameter_capture.xl --backend xlir --emit build\test\xlir_closure_mutable_string_parameter_capture_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xlir_cell_new_string" build\test\xlir_closure_mutable_string_parameter_capture_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_closure_mutable_string_parameter_capture_host.c %XL6_CGEN_LINKS% -o build\test\xlir_closure_mutable_string_parameter_capture_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_closure_mutable_string_parameter_capture_host.exe
if errorlevel 1 exit /b 1

rem A mutable var capture stores an owned xvalue handle in the shared cell.
rem Replacing and returning it must retain before the cell's final release.
release\xl.exe cgen test\cases\xlir_closure_mutable_xvalue_capture.xl --backend xlir --emit build\test\xlir_closure_mutable_xvalue_capture_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xlir_cell_store_xvalue" build\test\xlir_closure_mutable_xvalue_capture_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtValueRelease(cell->value.xvalue)" build\test\xlir_closure_mutable_xvalue_capture_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_closure_mutable_xvalue_capture_host.c %XL6_CGEN_LINKS% -o build\test\xlir_closure_mutable_xvalue_capture_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_closure_mutable_xvalue_capture_host.exe
if errorlevel 1 exit /b 1

rem Mutable var parameters keep their callable ABI, but entry lowering retains
rem into a local cell before a nested closure can replace the dynamic value.
release\xl.exe cgen test\cases\xlir_closure_mutable_xvalue_parameter_capture.xl --backend xlir --emit build\test\xlir_closure_mutable_xvalue_parameter_capture_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xlir_cell_new_xvalue" build\test\xlir_closure_mutable_xvalue_parameter_capture_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtValueRetain(v" build\test\xlir_closure_mutable_xvalue_parameter_capture_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_closure_mutable_xvalue_parameter_capture_host.c %XL6_CGEN_LINKS% -o build\test\xlir_closure_mutable_xvalue_parameter_capture_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_closure_mutable_xvalue_parameter_capture_host.exe
if errorlevel 1 exit /b 1

rem Return through finally keeps captured cells alive until the finally body
rem has completed; it then releases the replacement independently of return.
release\xl.exe cgen test\cases\xlir_closure_mutable_xvalue_finally_return.xl --backend xlir --emit build\test\xlir_closure_mutable_xvalue_finally_return_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xlir_cell_store_xvalue(s5, v10);" build\test\xlir_closure_mutable_xvalue_finally_return_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xlir_cell_release(s5); s5 = NULL;" build\test\xlir_closure_mutable_xvalue_finally_return_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_closure_mutable_xvalue_finally_return_host.c %XL6_CGEN_LINKS% -o build\test\xlir_closure_mutable_xvalue_finally_return_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_closure_mutable_xvalue_finally_return_host.exe
if errorlevel 1 exit /b 1

rem Catch and multi-catch return-finally routes use the same frozen cell
rem cleanup ABI: finally may replace the captured var before its cell dies.
release\xl.exe cgen test\cases\xlir_closure_mutable_xvalue_catch_finally_return.xl --backend xlir --emit build\test\xlir_closure_mutable_xvalue_catch_finally_return_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xlir_cell_store_xvalue(s5" build\test\xlir_closure_mutable_xvalue_catch_finally_return_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xlir_cell_release(s5); s5 = NULL;" build\test\xlir_closure_mutable_xvalue_catch_finally_return_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_closure_mutable_xvalue_catch_finally_return_host.c %XL6_CGEN_LINKS% -o build\test\xlir_closure_mutable_xvalue_catch_finally_return_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_closure_mutable_xvalue_catch_finally_return_host.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\xlir_closure_mutable_xvalue_multi_catch_finally_return.xl --backend xlir --emit build\test\xlir_closure_mutable_xvalue_multi_catch_finally_return_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xlir_cell_store_xvalue(s5" build\test\xlir_closure_mutable_xvalue_multi_catch_finally_return_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xlir_cell_release(s5); s5 = NULL;" build\test\xlir_closure_mutable_xvalue_multi_catch_finally_return_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_closure_mutable_xvalue_multi_catch_finally_return_host.c %XL6_CGEN_LINKS% -o build\test\xlir_closure_mutable_xvalue_multi_catch_finally_return_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_closure_mutable_xvalue_multi_catch_finally_return_host.exe
if errorlevel 1 exit /b 1

rem A closure environment may retain another closure by its explicit
rem reference-count ABI; the outer local may then be released safely.
release\xl.exe cgen test\cases\xlir_closure_closure_capture.xl --backend xlir --emit build\test\xlir_closure_closure_capture_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xlir_closure_retain(v" build\test\xlir_closure_closure_capture_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xlir_closure_release(environment->c0);" build\test\xlir_closure_closure_capture_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_closure_closure_capture_host.c %XL6_CGEN_LINKS% -o build\test\xlir_closure_closure_capture_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_closure_closure_capture_host.exe
if errorlevel 1 exit /b 1

rem Closure strings have two explicit ownership boundaries: the captured
rem parameter is copied into the environment, while a string rvalue argument
rem is materialized into temporary storage and released after the invoke.
release\xl.exe cgen test\cases\xlir_closure_string_capture.xl --backend xlir --emit build\test\xlir_closure_string_capture_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xrtStrDup(v" build\test\xlir_closure_string_capture_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtFree(environment->c0);" build\test\xlir_closure_string_capture_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_closure_string_capture_host.c %XL6_CGEN_LINKS% -o build\test\xlir_closure_string_capture_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_closure_string_capture_host.exe
if errorlevel 1 exit /b 1

rem Dynamic and future captures retain a runtime reference in the closure
rem environment and release that reference only when the closure is dropped.
release\xl.exe cgen test\cases\xlir_closure_xvalue_capture.xl --backend xlir --emit build\test\xlir_closure_xvalue_capture_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xrtValueRetain(v" build\test\xlir_closure_xvalue_capture_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtValueRelease(environment->c0);" build\test\xlir_closure_xvalue_capture_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_closure_xvalue_capture_host.c %XL6_CGEN_LINKS% -o build\test\xlir_closure_xvalue_capture_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_closure_xvalue_capture_host.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\xlir_closure_future_capture.xl --backend xlir --emit build\test\xlir_closure_future_capture_generated.c
if errorlevel 1 exit /b 1
findstr /C:"xrtFutureRef(v" build\test\xlir_closure_future_capture_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtFutureDestroy(environment->c0);" build\test\xlir_closure_future_capture_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_closure_future_capture_host.c %XL6_CGEN_LINKS% -o build\test\xlir_closure_future_capture_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_closure_future_capture_host.exe
if errorlevel 1 exit /b 1

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_callsite_binding.ps1
if errorlevel 1 (
    echo XLIR fixed/default/named CallSite quality gate failed.
    exit /b 1
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_callsite_variadic.ps1
if errorlevel 1 (
    echo XLIR ARRAY/DICT variadic CallSite quality gate failed.
    exit /b 1
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_callsite_c_vararg.ps1
if errorlevel 1 (
    echo XLIR native C variadic CallSite quality gate failed.
    exit /b 1
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_callsite_generic_specialization.ps1
if errorlevel 1 (
    echo XLIR generic-specialization CallSite quality gate failed.
    exit /b 1
)

:after_generic_callsite
if /I "%~2"=="trace" echo on
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_class_protocol_syntax.ps1
if errorlevel 1 (
    echo XLIR class operator/item/field/call syntax CallSite quality gate failed.
    exit /b 1
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_native_nominal_receiver.ps1
if errorlevel 1 (
    echo XLIR native nominal receiver quality gate failed.
    exit /b 1
)

:after_native_nominal
if /I "%~2"=="trace" echo on
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_native_handle_bind.ps1
if errorlevel 1 (
    echo XLIR native handle bind/take ownership quality gate failed.
    exit /b 1
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_value_initializer.ps1
if errorlevel 1 (
    echo XLIR canonical ValueInitializer quality gate failed.
    exit /b 1
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_reachability.ps1
if errorlevel 1 (
    echo XLIR callable reachability and emission-closure quality gate failed.
    exit /b 1
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_product_result.ps1
if errorlevel 1 (
    echo XLIR canonical product-result ABI quality gate failed.
    exit /b 1
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_named_function_adapter.ps1
if errorlevel 1 (
    echo XLIR named-function adapter CFG quality gate failed.
    exit /b 1
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_open_function_abi.ps1
if errorlevel 1 (
    echo XLIR open-function erasure and runtime callable ABI quality gate failed.
    exit /b 1
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_typed_callable_container_abi.ps1
if errorlevel 1 (
    echo XLIR typed callable-container effect, projection, and expression-call ABI quality gate failed.
    exit /b 1
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_typed_container_safe_index_objects_functions_abi.ps1
if errorlevel 1 (
    echo XLIR typed safe-index nominal presence and exact projection ABI quality gate failed.
    exit /b 1
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_typed_container_index_assign_objects_functions_abi.ps1
if errorlevel 1 (
    echo XLIR typed nominal index-assignment boxing, effect, and projection ABI quality gate failed.
    exit /b 1
)

:after_nominal_index_write
if /I "%~2"=="trace" echo on
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_dynamic_key_scalar_projection.ps1
if errorlevel 1 (
    echo XLIR dynamic-key canonical lookup and scalar projection ABI quality gate failed.
    exit /b 1
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_typed_point_projection_abi.ps1
if errorlevel 1 (
    echo XLIR typed point raw-pointer projection ABI quality gate failed.
    exit /b 1
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_xvalue_scalar_bridge_abi.ps1
if errorlevel 1 (
    echo XLIR xvalue scalar bridge, flow projection, and may-raise constructor ABI quality gate failed.
    exit /b 1
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_collection_member_intrinsics.ps1
if errorlevel 1 (
    echo XLIR collection-member intrinsic and borrowed-parameter owner quality gate failed.
    exit /b 1
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_erased_collection_members.ps1
if errorlevel 1 (
    echo XLIR erased collection member contract and lifetime quality gate failed.
    exit /b 1
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_collection_algebra.ps1
if errorlevel 1 (
    echo XLIR closed collection-algebra operation quality gate failed.
    exit /b 1
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_xvalue_array_iterator_projection.ps1
if errorlevel 1 (
    echo XLIR xvalue-array iterator projection and async continuation quality gate failed.
    exit /b 1
)

:after_array_iterator
if /I "%~2"=="trace" echo on
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_erased_collection_iterator_abi.ps1
if errorlevel 1 (
    echo XLIR statically shaped erased collection iterator ABI quality gate failed.
    exit /b 1
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_dynamic_var_iterator_abi.ps1
if errorlevel 1 (
    echo XLIR runtime-checked dynamic var iterator ABI quality gate failed.
    exit /b 1
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_string_iterator_abi.ps1
if errorlevel 1 (
    echo XLIR canonical UTF-8 string iterator ABI quality gate failed.
    exit /b 1
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_protocol_iterator_abi.ps1
if errorlevel 1 (
    echo XLIR canonical class/native protocol iterator ABI quality gate failed.
    exit /b 1
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_protocol_witness_adapter_abi.ps1
if errorlevel 1 (
    echo XLIR inherited/native protocol witness adapter ABI quality gate failed.
    exit /b 1
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_collection_type_rebind.ps1
if errorlevel 1 (
    echo XLIR checked collection TypeId rebind and ownership quality gate failed.
    exit /b 1
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_nullable_container_projection.ps1
if errorlevel 1 (
    echo XLIR typed nullable container projection and ownership quality gate failed.
    exit /b 1
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_dynamic_collection_methods.ps1
if errorlevel 1 (
    echo XLIR dynamic collection-method operation ABI quality gate failed.
    exit /b 1
)

:after_dynamic_methods
if /I "%~2"=="trace" echo on
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_u64_abi.ps1
if errorlevel 1 (
    echo XLIR lossless U64 execution/runtime ABI quality gate failed.
    exit /b 1
)

:after_u64
if /I "%~2"=="trace" echo on
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_integer_conversion_abi.ps1
if errorlevel 1 (
    echo XLIR explicit checked/wrapping integer conversion ABI quality gate failed.
    exit /b 1
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_operator_execution_type.ps1
if errorlevel 1 (
    echo XLIR canonical binary operator execution-type ABI quality gate failed.
    exit /b 1
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_weak_abi.ps1
if errorlevel 1 (
    echo XLIR managed weak reference and lifecycle ABI quality gate failed.
    exit /b 1
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_generator_machine.ps1
if errorlevel 1 (
    echo XLIR generator machine, capture, and closure-call ABI quality gate failed.
    exit /b 1
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_checked_cast_abi.ps1
if errorlevel 1 (
    echo XLIR closed dynamic checked cast ABI quality gate failed.
    exit /b 1
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_nullable_cast_abi.ps1
if errorlevel 1 (
    echo XLIR closed nullable try/lift/checked-unwrap cast ABI quality gate failed.
    exit /b 1
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_nullable_coalesce_abi.ps1
if errorlevel 1 (
    echo XLIR nullable coalesce ABI quality gate failed.
    exit /b 1
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_nullable_function_value_abi.ps1
if errorlevel 1 (
    echo XLIR nullable function-value return and class-field ABI quality gate failed.
    exit /b 1
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_callable_field_frame_abi.ps1
if errorlevel 1 (
    echo XLIR callable-field frame and managed product release ABI quality gate failed.
    exit /b 1
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_internal_property_call_abi.ps1
if errorlevel 1 (
    echo XLIR internal property CallTarget and error CFG ABI quality gate failed.
    exit /b 1
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_property_compound_assign.ps1
if errorlevel 1 (
    echo XLIR property and overloaded compound assignment transaction ABI quality gate failed.
    exit /b 1
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_borrowed_call_provenance.ps1
if errorlevel 1 (
    echo XLIR borrowed-call provenance and class-field read-modify-write ABI quality gate failed.
    exit /b 1
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_nominal_cast_abi.ps1
if errorlevel 1 (
    echo XLIR closed class/protocol nominal identity-set cast ABI quality gate failed.
    exit /b 1
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_enum_abi.ps1
if errorlevel 1 (
    echo XLIR closed numeric/tagged enum layout and match ABI quality gate failed.
    exit /b 1
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_async_storage_address.ps1
if errorlevel 1 (
    echo XLIR async machine storage-address ABI quality gate failed.
    exit /b 1
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_scalar_conversion_abi.ps1
if errorlevel 1 (
    echo XLIR canonical scalar conversion family ABI quality gate failed.
    exit /b 1
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_time_conversion_abi.ps1
if errorlevel 1 (
    echo XLIR distinct time conversion and extern ABI quality gate failed.
    exit /b 1
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_type_value_abi.ps1
if errorlevel 1 (
    echo XLIR TypeId execution/runtime ABI quality gate failed.
    exit /b 1
)

:after_type_value
if /I "%~2"=="trace" echo on
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_string_conversion_abi.ps1
if errorlevel 1 (
    echo XLIR TypeId, derived class, and explicit class-protocol string conversion ABI quality gate failed.
    exit /b 1
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_class_value_identity_abi.ps1
if errorlevel 1 (
    echo XLIR class structural hash/equality and typed-set identity ABI quality gate failed.
    exit /b 1
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_native_class_object_identity_abi.ps1
if errorlevel 1 (
    echo XLIR field-only native-backed managed object identity ABI quality gate failed.
    exit /b 1
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_managed_native_extern_receiver_abi.ps1
if errorlevel 1 (
    echo XLIR managed native extern receiver physical field-borrow ABI quality gate failed.
    exit /b 1
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_managed_native_lifecycle_abi.ps1
if errorlevel 1 (
    echo XLIR managed native owned-field lifecycle ABI quality gate failed.
    exit /b 1
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_class_lifecycle_abi.ps1
if errorlevel 1 (
    echo XLIR class destroy-plan and finalizer-borrow lifecycle ABI quality gate failed.
    exit /b 1
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_cgen_determinism.ps1
if errorlevel 1 (
    echo XLIR deterministic-C quality gate failed.
    exit /b 1
)

:after_determinism
if /I "%~2"=="trace" echo on
release\xl.exe run test\cases\contextual_protocol_member.xl
if errorlevel 1 exit /b 1

release\xl.exe lex release\examples\literals.xl > build\test\literals.tokens.txt
if errorlevel 1 exit /b 1

release\xl.exe parse release\examples\literals.xl > build\test\literals.ast.txt
if errorlevel 1 exit /b 1

rem Regression: this AST has a multi-child call at the child-arena boundary;
rem `check` must complete before XLIR requests C emission.
release\xl.exe check test\cases\cgen_numeric_literals.xl
if errorlevel 1 exit /b 1

rem XLIR owns the portable C spelling of scalar constants.  The generated C
rem therefore contains no source separators or XL-only numeric prefixes.
release\xl.exe cgen test\cases\cgen_numeric_literals.xl --backend xlir --emit build\test\xlir_numeric_literals_generated.c
if errorlevel 1 exit /b 1
findstr /C:"v1 = 1000000LL;" build\test\xlir_numeric_literals_generated.c >nul
if errorlevel 1 exit /b 1
findstr /C:"v5 = 12.5e1;" build\test\xlir_numeric_literals_generated.c >nul
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_numeric_literals_host.c %XL6_CGEN_LINKS% -o build\test\xlir_numeric_literals_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_numeric_literals_host.exe
if errorlevel 1 exit /b 1

:after_numeric_literals
if /I "%~2"=="trace" echo on
release\xl.exe cgen test\cases\cgen_int_literal_overflow.xl --emit build\test\cgen_int_literal_overflow.c >nul 2> build\test\cgen_int_literal_overflow.err.txt
if not errorlevel 1 (
    echo cgen_int_literal_overflow.xl should fail.
    exit /b 1
)
findstr /C:"integer literal is too large or invalid" build\test\cgen_int_literal_overflow.err.txt >nul
if errorlevel 1 (
    echo cgen_int_literal_overflow.err.txt missing integer literal overflow diagnostic.
    exit /b 1
)
findstr /C:"code generation error" build\test\cgen_int_literal_overflow.err.txt >nul
if not errorlevel 1 (
    echo cgen_int_literal_overflow.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)

release\xl.exe check release\examples\containers.xl
if errorlevel 1 exit /b 1

release\xl.exe run release\examples\containers.xl %XL6_XL_DEFAULT_LINK_ARGS%
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\container_methods.xl
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\container_methods.xl %XL6_XL_DEFAULT_LINK_ARGS%
if errorlevel 1 exit /b 1

release\xl.exe check release\examples\container_methods.xl
if errorlevel 1 exit /b 1

release\xl.exe run release\examples\container_methods.xl %XL6_XL_DEFAULT_LINK_ARGS%
if errorlevel 1 exit /b 1

release\xl.exe check release\examples\container_ops.xl
if errorlevel 1 exit /b 1

release\xl.exe run release\examples\container_ops.xl %XL6_XL_DEFAULT_LINK_ARGS%
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\layout.xl
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\comments.xl
if errorlevel 1 exit /b 1

release\xl.exe lex test\cases\sem_unclosed_comment.xl > build\test\sem_unclosed_comment.tokens.txt 2> build\test\sem_unclosed_comment.err.txt
if not errorlevel 1 (
    echo sem_unclosed_comment.xl should fail.
    exit /b 1
)
findstr /C:"ERROR" build\test\sem_unclosed_comment.tokens.txt >nul
if errorlevel 1 (
    echo sem_unclosed_comment.tokens.txt missing ERROR token.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$bytes=[System.IO.File]::ReadAllBytes('build\test\sem_unclosed_comment.tokens.txt'); $enc=New-Object System.Text.UTF8Encoding($false,$true); $text=$enc.GetString($bytes); $expected=([char]0x672A)+([char]0x95ED)+([char]0x5408)+([char]0x7684)+([char]0x591A)+([char]0x884C)+([char]0x6CE8)+([char]0x91CA); if($text.IndexOf($expected) -lt 0){ Write-Host 'sem_unclosed_comment.tokens.txt missing unclosed-comment diagnostic.'; exit 1 }"
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\cgen_case_sensitive.xl
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\cgen_context_keywords.xl
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\cgen_context_keyword_function.xl
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\cgen_unicode_identifiers.xl
if errorlevel 1 exit /b 1

findstr /C:"xl6_runtime_console_begin" release\include\xl_runtime.h >nul
if errorlevel 1 (
    echo xl_runtime.h missing console lifecycle.
    exit /b 1
)
release\xl.exe run test\cases\cgen_console_utf8.xl > build\test\cgen_console_utf8_run.out.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -Command "$utf8 = New-Object System.Text.UTF8Encoding($false, $true); $text = $utf8.GetString([IO.File]::ReadAllBytes('build\test\cgen_console_utf8_run.out.txt')).TrimEnd([char]13, [char]10); $expected=([char]0x4E2D)+([char]0x6587)+([char]0x63A7)+([char]0x5236)+([char]0x53F0)+' UTF-8'; if ($text -cne $expected) { Write-Host ('unexpected UTF-8 run output: ' + $text); exit 1 }"
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\cgen_console_utf8.xl -o build\test\cgen_console_utf8.exe
if errorlevel 1 exit /b 1
build\test\cgen_console_utf8.exe > build\test\cgen_console_utf8_build.out.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -Command "$utf8 = New-Object System.Text.UTF8Encoding($false, $true); $text = $utf8.GetString([IO.File]::ReadAllBytes('build\test\cgen_console_utf8_build.out.txt')).TrimEnd([char]13, [char]10); $expected=([char]0x4E2D)+([char]0x6587)+([char]0x63A7)+([char]0x5236)+([char]0x53F0)+' UTF-8'; if ($text -cne $expected) { Write-Host ('unexpected UTF-8 executable output: ' + $text); exit 1 }"
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% test\cases\cgen_console_utf8_host.c src\common\xl6_console.c %XL6_CGEN_LINKS% -o build\test\cgen_console_utf8_host.exe
if errorlevel 1 exit /b 1
build\test\cgen_console_utf8_host.exe
if errorlevel 1 (
    echo cgen_console_utf8_host.exe failed.
    exit /b 1
)

release\xl.exe check test\cases\cgen_long_identifiers.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_long_identifiers.xl --backend xlir --emit build\test\cgen_long_identifiers.c --emit-header build\test\cgen_long_identifiers.h
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$c=Get-Content -Raw -Encoding UTF8 'build\test\cgen_long_identifiers.c'; $h=Get-Content -Raw -Encoding UTF8 'build\test\cgen_long_identifiers.h'; $hashes=[regex]::Matches($c, '_h[0-9A-F]{16}'); if($hashes.Count -lt 8){exit 1}; $unique=@($hashes.Value | Sort-Object -Unique); if($unique.Count -lt 8){exit 2}; $type=[regex]::Match($h, 'typedef struct (XL_Class_[A-Za-z0-9_]+)\s*\{').Groups[1].Value; if([string]::IsNullOrEmpty($type) -or $c.IndexOf('struct '+$type) -lt 0){exit 3}"
if errorlevel 1 (
    echo cgen_long_identifiers.c missing unique bounded identifier hashes.
    exit /b 1
)
release\xl.exe run test\cases\cgen_long_identifiers.xl
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\call_args.xl
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\call_defaults.xl
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\varargs.xl
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\call_named_vararg.xl
if errorlevel 1 exit /b 1

release\xl.exe parse test\cases\containers.xl > build\test\containers.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\containers.xl
if errorlevel 1 exit /b 1
release\xl.exe parse test\cases\cgen_containers.xl > build\test\cgen_containers.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\cgen_containers.xl
if errorlevel 1 exit /b 1
release\xl.exe parse test\cases\cgen_in_containers.xl > build\test\cgen_in_containers.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\cgen_in_containers.xl
if errorlevel 1 exit /b 1
release\xl.exe parse test\cases\cgen_for_in_containers.xl > build\test\cgen_for_in_containers.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\cgen_for_in_containers.xl
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\cgen_for_in_var_non_container.xl
if errorlevel 1 exit /b 1

release\xl.exe parse test\cases\control_flow.xl > build\test\control_flow.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\control_flow.xl
if errorlevel 1 exit /b 1

release\xl.exe parse test\cases\control_flow_do_while.xl > build\test\control_flow_do_while.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\control_flow_do_while.xl
if errorlevel 1 exit /b 1

release\xl.exe parse test\cases\control_flow_inline_if.xl > build\test\control_flow_inline_if.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\control_flow_inline_if.xl
if errorlevel 1 exit /b 1

release\xl.exe parse test\cases\operators.xl > build\test\operators.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\operators.xl
if errorlevel 1 exit /b 1

release\xl.exe parse test\cases\cgen_null_coalesce.xl > build\test\cgen_null_coalesce.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\cgen_null_coalesce.xl
if errorlevel 1 exit /b 1

release\xl.exe parse test\cases\member_index.xl > build\test\member_index.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\member_index.xl
if errorlevel 1 exit /b 1

release\xl.exe parse test\cases\generic_types.xl > build\test\generic_types.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\generic_types.xl
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\sem_container_literal_target_types.xl
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\sem_container_literal_target_item_mismatch.xl > build\test\sem_container_literal_target_item_mismatch.err.txt 2>&1
if not errorlevel 1 (
    echo sem_container_literal_target_item_mismatch.xl should fail.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[IO.File]::ReadAllText('build/test/sem_container_literal_target_item_mismatch.err.txt'); $count=([regex]::Matches($text,'\[sem\].*target (array|list|set|dict)<[^>]+>, value')).Count; if($count -ne 4){ Write-Host ('expected 4 container literal item mismatch diagnostics, got '+$count); exit 1 }"
if errorlevel 1 exit /b 1

:type_name_identifiers
if /I "%~2"=="trace" echo on
release\xl.exe parse test\cases\type_values.xl > build\test\type_values.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\type_values.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\cgen_type_name_identifiers.xl
if errorlevel 1 exit /b 1
release\xl.exe parse test\cases\type_aliases.xl > build\test\type_aliases.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\type_aliases.xl
if errorlevel 1 exit /b 1
release\xl.exe parse test\cases\cgen_string_concat.xl > build\test\cgen_string_concat.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\cgen_string_concat.xl
if errorlevel 1 exit /b 1

release\xl.exe parse test\cases\cgen_string_compare.xl > build\test\cgen_string_compare.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\cgen_string_compare.xl
if errorlevel 1 exit /b 1

release\xl.exe parse test\cases\cgen_string_literals.xl > build\test\cgen_string_literals.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\cgen_string_literals.xl
if errorlevel 1 exit /b 1

release\xl.exe parse test\cases\cgen_fstring.xl > build\test\cgen_fstring.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\cgen_fstring.xl
if errorlevel 1 exit /b 1

release\xl.exe parse test\cases\cgen_time_literals.xl > build\test\cgen_time_literals.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\cgen_time_literals.xl
if errorlevel 1 exit /b 1

release\xl.exe parse test\cases\cgen_auto_expressions.xl > build\test\cgen_auto_expressions.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\cgen_auto_expressions.xl
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\invalid_numeric_operator.xl > build\test\invalid_numeric_operator.diag.txt 2>&1
if not errorlevel 1 (
    echo invalid_numeric_operator.xl should fail.
    exit /b 1
)
findstr /C:"numeric operator operands must be numeric" build\test\invalid_numeric_operator.diag.txt >nul
if errorlevel 1 (
    echo invalid_numeric_operator.xl missing expected diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\invalid_integer_operator.xl > build\test\invalid_integer_operator.diag.txt 2>&1
if not errorlevel 1 (
    echo invalid_integer_operator.xl should fail.
    exit /b 1
)
findstr /C:"integer operator operands must be integer" build\test\invalid_integer_operator.diag.txt >nul
if errorlevel 1 (
    echo invalid_integer_operator.xl missing expected diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_pow_float.xl > build\test\sem_pow_float.err.txt 2>&1
if not errorlevel 1 (
    echo sem_pow_float.xl should fail.
    exit /b 1
)
findstr /C:"power operator operands must be integer" build\test\sem_pow_float.err.txt >nul
if errorlevel 1 (
    echo sem_pow_float.xl missing expected diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_pow_var.xl > build\test\sem_pow_var.err.txt 2>&1
if not errorlevel 1 (
    echo sem_pow_var.xl should fail.
    exit /b 1
)
findstr /C:"power operator operands must be integer" build\test\sem_pow_var.err.txt >nul
if errorlevel 1 (
    echo sem_pow_var.xl missing expected diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\invalid_division_by_zero.xl > build\test\invalid_division_by_zero.diag.txt 2>&1
if not errorlevel 1 (
    echo invalid_division_by_zero.xl should fail.
    exit /b 1
)
findstr /C:"division by zero literal" build\test\invalid_division_by_zero.diag.txt >nul
if errorlevel 1 (
    echo invalid_division_by_zero.xl missing expected diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\invalid_modulo_by_zero.xl > build\test\invalid_modulo_by_zero.diag.txt 2>&1
if not errorlevel 1 (
    echo invalid_modulo_by_zero.xl should fail.
    exit /b 1
)
findstr /C:"modulo by zero literal" build\test\invalid_modulo_by_zero.diag.txt >nul
if errorlevel 1 (
    echo invalid_modulo_by_zero.xl missing expected diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_compound_var_target.xl > build\test\sem_compound_var_target.err.txt 2>&1
if not errorlevel 1 (
    echo sem_compound_var_target.xl should fail.
    exit /b 1
)
findstr /C:"var compound assignment target is not supported" build\test\sem_compound_var_target.err.txt >nul
if errorlevel 1 (
    echo sem_compound_var_target.xl missing expected diagnostic.
    exit /b 1
)
findstr /C:"op +=, left var, right int" build\test\sem_compound_var_target.err.txt >nul
if errorlevel 1 (
    echo sem_compound_var_target.xl missing compound diagnostic context.
    exit /b 1
)
release\xl.exe check test\cases\sem_compound_numeric_type.xl > build\test\sem_compound_numeric_type.err.txt 2>&1
if not errorlevel 1 (
    echo sem_compound_numeric_type.xl should fail.
    exit /b 1
)
findstr /C:"numeric compound assignment operands must be numeric" build\test\sem_compound_numeric_type.err.txt >nul
if errorlevel 1 (
    echo sem_compound_numeric_type.xl missing expected diagnostic.
    exit /b 1
)
findstr /C:"op +=, left int, right bool" build\test\sem_compound_numeric_type.err.txt >nul
if errorlevel 1 (
    echo sem_compound_numeric_type.xl missing compound diagnostic context.
    exit /b 1
)
release\xl.exe check test\cases\sem_compound_float_to_int.xl > build\test\sem_compound_float_to_int.err.txt 2>&1
if not errorlevel 1 (
    echo sem_compound_float_to_int.xl should fail.
    exit /b 1
)
findstr /C:"compound assignment result type does not match target type" build\test\sem_compound_float_to_int.err.txt >nul
if errorlevel 1 (
    echo sem_compound_float_to_int.xl missing expected diagnostic.
    exit /b 1
)
findstr /C:"op +=, left int, right float" build\test\sem_compound_float_to_int.err.txt >nul
if errorlevel 1 (
    echo sem_compound_float_to_int.xl missing compound diagnostic context.
    exit /b 1
)
release\xl.exe check test\cases\sem_compound_div_zero.xl > build\test\sem_compound_div_zero.err.txt 2>&1
if not errorlevel 1 (
    echo sem_compound_div_zero.xl should fail.
    exit /b 1
)
findstr /C:"division by zero literal" build\test\sem_compound_div_zero.err.txt >nul
if errorlevel 1 (
    echo sem_compound_div_zero.xl missing expected diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_compound_mod_zero.xl > build\test\sem_compound_mod_zero.err.txt 2>&1
if not errorlevel 1 (
    echo sem_compound_mod_zero.xl should fail.
    exit /b 1
)
findstr /C:"modulo by zero literal" build\test\sem_compound_mod_zero.err.txt >nul
if errorlevel 1 (
    echo sem_compound_mod_zero.xl missing expected diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\invalid_unary_operator.xl > build\test\invalid_unary_operator.diag.txt 2>&1
if not errorlevel 1 (
    echo invalid_unary_operator.xl should fail.
    exit /b 1
)
findstr /C:"bitwise not operand must be integer" build\test\invalid_unary_operator.diag.txt >nul
if errorlevel 1 (
    echo invalid_unary_operator.xl missing expected diagnostic.
    exit /b 1
)

release\xl.exe parse test\cases\cgen_var_values.xl > build\test\cgen_var_values.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\cgen_var_values.xl
if errorlevel 1 exit /b 1
release\xl.exe parse test\cases\cgen_type_conversions.xl > build\test\cgen_type_conversions.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\cgen_type_conversions.xl
if errorlevel 1 exit /b 1
release\xl.exe parse test\cases\cgen_implicit_methods.xl > build\test\cgen_implicit_methods.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\cgen_implicit_methods.xl
if errorlevel 1 exit /b 1

release\xl.exe parse test\cases\generics.xl > build\test\generics.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\generics.xl
if errorlevel 1 exit /b 1
release\xl.exe parse test\cases\sem_generic_constraints.xl > build\test\sem_generic_constraints.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\sem_generic_constraints.xl
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\sem_generic_constraint_unsatisfied.xl > build\test\sem_generic_constraint_unsatisfied.err.txt 2>&1
if not errorlevel 1 (
    echo sem_generic_constraint_unsatisfied.xl should fail.
    exit /b 1
)
findstr /C:"does not satisfy constraint comparable" build\test\sem_generic_constraint_unsatisfied.err.txt >nul
if errorlevel 1 (
    echo sem_generic_constraint_unsatisfied.err.txt missing generic constraint diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_generic_constraint_unknown.xl > build\test\sem_generic_constraint_unknown.err.txt 2>&1
if not errorlevel 1 (
    echo sem_generic_constraint_unknown.xl should fail.
    exit /b 1
)
findstr /C:"unknown generic constraint" build\test\sem_generic_constraint_unknown.err.txt >nul
if errorlevel 1 (
    echo sem_generic_constraint_unknown.err.txt missing unknown constraint diagnostic.
    exit /b 1
)
release\xl.exe check release\examples\generics.xl
if errorlevel 1 exit /b 1
release\xl.exe check release\examples\generic_identity.xl
if errorlevel 1 exit /b 1
release\xl.exe check release\examples\generic_constraints.xl
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\generic_constraints.xl
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\generic_managed_ownership.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\generic_managed_ownership.xl
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\generic_class_iter.xl
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\runtime_type.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\runtime_var_bool_type.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\runtime_var_type_storage_matrix.xl
if errorlevel 1 exit /b 1

release\xl.exe check release\examples\native_counter.xl
if errorlevel 1 exit /b 1

release\xl.exe parse test\cases\overloads.xl > build\test\overloads.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\overloads.xl
if errorlevel 1 exit /b 1
release\xl.exe parse test\cases\overload_varargs.xl > build\test\overload_varargs.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\overload_varargs.xl
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\sem_overload_var_requires_explicit.xl > build\test\sem_overload_var_requires_explicit.err.txt 2>&1
if not errorlevel 1 (
    echo sem_overload_var_requires_explicit.xl should fail.
    exit /b 1
)
findstr /C:"dynamic var argument requires explicit 'as Type' conversion before overload resolution" build\test\sem_overload_var_requires_explicit.err.txt >nul
if errorlevel 1 (
    echo sem_overload_var_requires_explicit.err.txt missing explicit conversion diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_multi_return_type_diagnostic.xl > build\test\sem_multi_return_type_diagnostic.err.txt 2>&1
if not errorlevel 1 (
    echo sem_multi_return_type_diagnostic.xl should fail.
    exit /b 1
)
findstr /C:"multi-return assignment item 2 type mismatch: target Color, return Ray; targets (bool, Color, Ray), returns (bool, Ray, Color)" build\test\sem_multi_return_type_diagnostic.err.txt >nul
if errorlevel 1 (
    echo sem_multi_return_type_diagnostic.err.txt missing ordered signature diagnostic.
    exit /b 1
)

release\xl.exe parse test\cases\function_types.xl > build\test\function_types.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\function_types.xl
if errorlevel 1 exit /b 1

release\xl.exe parse test\cases\anonymous_functions.xl > build\test\anonymous_functions.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\anonymous_functions.xl
if errorlevel 1 exit /b 1
release\xl.exe check release\examples\anonymous_functions.xl
if errorlevel 1 exit /b 1

release\xl.exe parse test\cases\class_native_backed.xl > build\test\class_native_backed.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\class_native_backed.xl
if errorlevel 1 exit /b 1
release\xl.exe check release\examples\class_native_backed.xl
if errorlevel 1 exit /b 1
release\xl.exe check release\examples\class_native_bridge.xl
if errorlevel 1 exit /b 1
release\xl.exe check release\examples\class_point.xl
if errorlevel 1 exit /b 1
release\xl.exe check release\examples\object_protocol.xl
if errorlevel 1 exit /b 1
release\xl.exe check release\examples\oop_protocol_overload.xl
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\oop_protocol_overload.xl
if errorlevel 1 exit /b 1
release\xl.exe check release\examples\oop_property_get_let.xl
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\oop_property_get_let.xl
if errorlevel 1 exit /b 1
release\xl.exe check release\examples\oop_static_access.xl
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\oop_static_access.xl
if errorlevel 1 exit /b 1
release\xl.exe check release\examples\oop_protected_access.xl
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\oop_protected_access.xl
if errorlevel 1 exit /b 1
release\xl.exe check release\examples\oop_virtual_dispatch.xl
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\oop_virtual_dispatch.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\cgen_class_static_access.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\cgen_class_virtual.xl
if errorlevel 1 exit /b 1
release\xl.exe check release\examples\copy_clone.xl
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\class_persistence.xl
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\class_field_persistence_attrs.xl
if errorlevel 1 exit /b 1
:after_persistence_examples
if /I "%~2"=="trace" echo on
release\xl.exe check test\cases\sem_protocol_valid.xl
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\sem_protocol_unknown_reserved.xl > build\test\sem_protocol_unknown_reserved.err.txt 2>&1
if not errorlevel 1 (
    echo sem_protocol_unknown_reserved.xl should fail.
    exit /b 1
)
findstr /C:"__unknown__" build\test\sem_protocol_unknown_reserved.err.txt >nul
if errorlevel 1 (
    echo sem_protocol_unknown_reserved.err.txt missing protocol diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_protocol_item_arity.xl > build\test\sem_protocol_item_arity.err.txt 2>&1
if not errorlevel 1 (
    echo sem_protocol_item_arity.xl should fail.
    exit /b 1
)
findstr /C:"__item__" build\test\sem_protocol_item_arity.err.txt >nul
if errorlevel 1 (
    echo sem_protocol_item_arity.err.txt missing protocol arity diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_index_multi_container.xl > build\test\sem_index_multi_container.err.txt 2>&1
if not errorlevel 1 (
    echo sem_index_multi_container.xl should fail.
    exit /b 1
)
findstr /C:"exactly one key" build\test\sem_index_multi_container.err.txt >nul
if errorlevel 1 (
    echo sem_index_multi_container.err.txt missing multi-index container diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_class_property_readonly.xl > build\test\sem_class_property_readonly.err.txt 2>&1
if not errorlevel 1 (
    echo sem_class_property_readonly.xl should fail.
    exit /b 1
)
findstr /C:"class property assignment requires matching let" build\test\sem_class_property_readonly.err.txt >nul
if errorlevel 1 (
    echo sem_class_property_readonly.err.txt missing readonly property diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_class_property_compound_readonly.xl > build\test\sem_class_property_compound_readonly.err.txt 2>&1
if not errorlevel 1 (
    echo sem_class_property_compound_readonly.xl should fail.
    exit /b 1
)
findstr /C:"class property compound assignment requires matching let" build\test\sem_class_property_compound_readonly.err.txt >nul
if errorlevel 1 (
    echo sem_class_property_compound_readonly.err.txt missing readonly compound property diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_class_property_compound_writeonly.xl > build\test\sem_class_property_compound_writeonly.err.txt 2>&1
if not errorlevel 1 (
    echo sem_class_property_compound_writeonly.xl should fail.
    exit /b 1
)
findstr /C:"class property compound assignment requires matching get" build\test\sem_class_property_compound_writeonly.err.txt >nul
if errorlevel 1 (
    echo sem_class_property_compound_writeonly.err.txt missing writeonly compound property diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_class_property_compound_setter_type.xl > build\test\sem_class_property_compound_setter_type.err.txt 2>&1
if not errorlevel 1 (
    echo sem_class_property_compound_setter_type.xl should fail.
    exit /b 1
)
findstr /C:"class property compound assignment requires matching let" build\test\sem_class_property_compound_setter_type.err.txt >nul
if errorlevel 1 (
    echo sem_class_property_compound_setter_type.err.txt missing setter type diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_class_property_null_coalesce_assign.xl > build\test\sem_class_property_null_coalesce_assign.err.txt 2>&1
if not errorlevel 1 (
    echo sem_class_property_null_coalesce_assign.xl should fail.
    exit /b 1
)
findstr /C:"class property does not support ?=" build\test\sem_class_property_null_coalesce_assign.err.txt >nul
if errorlevel 1 (
    echo sem_class_property_null_coalesce_assign.err.txt missing null-coalescing property diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_class_property_compound_narrowing.xl > build\test\sem_class_property_compound_narrowing.err.txt 2>&1
if not errorlevel 1 (
    echo sem_class_property_compound_narrowing.xl should fail.
    exit /b 1
)
findstr /C:"compound assignment result type does not match target type" build\test\sem_class_property_compound_narrowing.err.txt >nul
if errorlevel 1 (
    echo sem_class_property_compound_narrowing.err.txt missing compound narrowing diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_protocol_field_key_type.xl > build\test\sem_protocol_field_key_type.err.txt 2>&1
if not errorlevel 1 (
    echo sem_protocol_field_key_type.xl should fail.
    exit /b 1
)
findstr /C:"__field__" build\test\sem_protocol_field_key_type.err.txt >nul
if errorlevel 1 (
    echo sem_protocol_field_key_type.err.txt missing protocol field key diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_protocol_copyable_return.xl > build\test\sem_protocol_copyable_return.err.txt 2>&1
if not errorlevel 1 (
    echo sem_protocol_copyable_return.xl should fail.
    exit /b 1
)
findstr /C:"__copyable__" build\test\sem_protocol_copyable_return.err.txt >nul
if errorlevel 1 (
    echo sem_protocol_copyable_return.err.txt missing protocol return diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_protocol_optional_param.xl > build\test\sem_protocol_optional_param.err.txt 2>&1
if not errorlevel 1 (
    echo sem_protocol_optional_param.xl should fail.
    exit /b 1
)
findstr /C:"__item__" build\test\sem_protocol_optional_param.err.txt >nul
if errorlevel 1 (
    echo sem_protocol_optional_param.err.txt missing protocol optional parameter diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_protocol_operator_arg_type.xl > build\test\sem_protocol_operator_arg_type.err.txt 2>&1
if not errorlevel 1 (
    echo sem_protocol_operator_arg_type.xl should fail.
    exit /b 1
)
findstr /C:"target int, value string" build\test\sem_protocol_operator_arg_type.err.txt >nul
if errorlevel 1 (
    echo sem_protocol_operator_arg_type.err.txt missing protocol operator type diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_protocol_contains_arg_type.xl > build\test\sem_protocol_contains_arg_type.err.txt 2>&1
if not errorlevel 1 (
    echo sem_protocol_contains_arg_type.xl should fail.
    exit /b 1
)
findstr /C:"target int, value string" build\test\sem_protocol_contains_arg_type.err.txt >nul
if errorlevel 1 (
    echo sem_protocol_contains_arg_type.err.txt missing protocol contains type diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_protocol_call_arg_type.xl > build\test\sem_protocol_call_arg_type.err.txt 2>&1
if not errorlevel 1 (
    echo sem_protocol_call_arg_type.xl should fail.
    exit /b 1
)
findstr /C:"target int, value string" build\test\sem_protocol_call_arg_type.err.txt >nul
if errorlevel 1 (
    echo sem_protocol_call_arg_type.err.txt missing protocol call type diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_protocol_len_readonly.xl > build\test\sem_protocol_len_readonly.err.txt 2>&1
if not errorlevel 1 (
    echo sem_protocol_len_readonly.xl should fail.
    exit /b 1
)
findstr /C:"member length is read-only" build\test\sem_protocol_len_readonly.err.txt >nul
if errorlevel 1 (
    echo sem_protocol_len_readonly.err.txt missing length readonly diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_protocol_next_returns.xl > build\test\sem_protocol_next_returns.err.txt 2>&1
if not errorlevel 1 (
    echo sem_protocol_next_returns.xl should fail.
    exit /b 1
)
findstr /C:"__next__" build\test\sem_protocol_next_returns.err.txt >nul
if errorlevel 1 (
    echo sem_protocol_next_returns.err.txt missing __next__ diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_derive_valid.xl
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\sem_derive_unknown.xl > build\test\sem_derive_unknown.err.txt 2>&1
if not errorlevel 1 (
    echo sem_derive_unknown.xl should fail.
    exit /b 1
)
findstr /C:"derive" build\test\sem_derive_unknown.err.txt >nul
if errorlevel 1 (
    echo sem_derive_unknown.err.txt missing derive diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_derive_duplicate.xl > build\test\sem_derive_duplicate.err.txt 2>&1
if not errorlevel 1 (
    echo sem_derive_duplicate.xl should fail.
    exit /b 1
)
findstr /C:"derive" build\test\sem_derive_duplicate.err.txt >nul
if errorlevel 1 (
    echo sem_derive_duplicate.err.txt missing derive duplicate diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_derive_move_conflict.xl > build\test\sem_derive_move_conflict.err.txt 2>&1
if not errorlevel 1 (
    echo sem_derive_move_conflict.xl should fail.
    exit /b 1
)
findstr /C:"moveOnly" build\test\sem_derive_move_conflict.err.txt >nul
if errorlevel 1 (
    echo sem_derive_move_conflict.err.txt missing derive conflict diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_derive_copy_noncopyable_field.xl > build\test\sem_derive_copy_noncopyable_field.err.txt 2>&1
if not errorlevel 1 (
    echo sem_derive_copy_noncopyable_field.xl should fail.
    exit /b 1
)
findstr /C:"derive copy/clone" build\test\sem_derive_copy_noncopyable_field.err.txt >nul
if errorlevel 1 (
    echo sem_derive_copy_noncopyable_field.err.txt missing copy field diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_derive_value_native_class_field.xl > build\test\sem_derive_value_native_class_field.err.txt 2>&1
if not errorlevel 1 (
    echo sem_derive_value_native_class_field.xl should fail.
    exit /b 1
)
findstr /C:"derive value/json/xson" build\test\sem_derive_value_native_class_field.err.txt >nul
if errorlevel 1 (
    echo sem_derive_value_native_class_field.err.txt missing persistence field diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_json_parse_without_derive.xl > build\test\sem_json_parse_without_derive.err.txt 2>&1
if not errorlevel 1 (
    echo sem_json_parse_without_derive.xl should fail.
    exit /b 1
)
findstr /C:"derive value/json/xson" build\test\sem_json_parse_without_derive.err.txt >nul
if errorlevel 1 (
    echo sem_json_parse_without_derive.err.txt missing persistence target diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_json_stringify_without_derive.xl > build\test\sem_json_stringify_without_derive.err.txt 2>&1
if not errorlevel 1 (
    echo sem_json_stringify_without_derive.xl should fail.
    exit /b 1
)
findstr /C:"derive value/json/xson" build\test\sem_json_stringify_without_derive.err.txt >nul
if errorlevel 1 (
    echo sem_json_stringify_without_derive.err.txt missing persistence stringify diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_field_attr_unknown.xl > build\test\sem_field_attr_unknown.err.txt 2>&1
if not errorlevel 1 (
    echo sem_field_attr_unknown.xl should fail.
    exit /b 1
)
findstr /C:"unknown field persistence annotation" build\test\sem_field_attr_unknown.err.txt >nul
if errorlevel 1 (
    echo sem_field_attr_unknown.err.txt missing unknown annotation diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_field_attr_conflict.xl > build\test\sem_field_attr_conflict.err.txt 2>&1
if not errorlevel 1 (
    echo sem_field_attr_conflict.xl should fail.
    exit /b 1
)
findstr /C:"skip conflicts with include" build\test\sem_field_attr_conflict.err.txt >nul
if errorlevel 1 (
    echo sem_field_attr_conflict.err.txt missing conflict diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_field_attr_static.xl > build\test\sem_field_attr_static.err.txt 2>&1
if not errorlevel 1 (
    echo sem_field_attr_static.xl should fail.
    exit /b 1
)
findstr /C:"static field cannot use persistence annotation" build\test\sem_field_attr_static.err.txt >nul
if errorlevel 1 (
    echo sem_field_attr_static.err.txt missing static field diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_field_attr_bad_name.xl > build\test\sem_field_attr_bad_name.err.txt 2>&1
if not errorlevel 1 (
    echo sem_field_attr_bad_name.xl should fail.
    exit /b 1
)
findstr /C:"name option must be a string" build\test\sem_field_attr_bad_name.err.txt >nul
if errorlevel 1 (
    echo sem_field_attr_bad_name.err.txt missing name option diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\cgen_contextual_identifiers.xl
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -Command "[System.IO.File]::WriteAllText('build\test\source_without_final_newline.xl','println(1 + 2)',(New-Object System.Text.UTF8Encoding($false)))"
if errorlevel 1 exit /b 1
release\xl.exe check build\test\source_without_final_newline.xl
if errorlevel 1 (
    echo source without final newline should pass parser.
    exit /b 1
)
release\xl.exe run build\test\source_without_final_newline.xl > build\test\source_without_final_newline.out.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\source_without_final_newline.out.txt'); if($text.TrimEnd([char[]]@([char]13,[char]10)) -ne '3'){ exit 1 }"
if errorlevel 1 (
    echo source without final newline produced unexpected output.
    exit /b 1
)
release\xl.exe check release\examples\second_stage.xl
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\second_stage.xl %XL6_XL_DEFAULT_LINK_ARGS%
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\sem_undeclared_identifier.xl > build\test\sem_undeclared_identifier.err.txt 2>&1
if not errorlevel 1 (
    echo sem_undeclared_identifier.xl should fail.
    exit /b 1
)
findstr /C:"test\cases\sem_undeclared_identifier.xl:2:19: [sem] undeclared identifier 'missingValue'" build\test\sem_undeclared_identifier.err.txt >nul
if errorlevel 1 (
    echo sem_undeclared_identifier.err.txt missing source diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_fstring_undeclared_identifier.xl > build\test\sem_fstring_undeclared_identifier.err.txt 2>&1
if not errorlevel 1 (
    echo sem_fstring_undeclared_identifier.xl should fail.
    exit /b 1
)
findstr /C:"test\cases\sem_fstring_undeclared_identifier.xl:2:28: [sem] undeclared identifier 'missingValue'" build\test\sem_fstring_undeclared_identifier.err.txt >nul
if errorlevel 1 (
    echo sem_fstring_undeclared_identifier.err.txt missing rebased source diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_fstring_multiline_undeclared_identifier.xl > build\test\sem_fstring_multiline_undeclared_identifier.err.txt 2>&1
if not errorlevel 1 (
    echo sem_fstring_multiline_undeclared_identifier.xl should fail.
    exit /b 1
)
findstr /C:"test\cases\sem_fstring_multiline_undeclared_identifier.xl:3:8: [sem] undeclared identifier 'missingValue'" build\test\sem_fstring_multiline_undeclared_identifier.err.txt >nul
if errorlevel 1 (
    echo sem_fstring_multiline_undeclared_identifier.err.txt missing multiline rebased diagnostic.
    exit /b 1
)

release\xl.exe run release\examples\top_level_main.xl %XL6_XL_DEFAULT_LINK_ARGS% > build\test\top_level_main.out.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$lines=Get-Content 'build\test\top_level_main.out.txt'; if($lines.Count -lt 2 -or $lines[0] -ne 'top-level init' -or $lines[1] -ne 'main body'){ exit 1 }"
if errorlevel 1 (
    echo release\examples\top_level_main.xl output order failed.
    exit /b 1
)

:low_level_ir
if /I "%~2"=="trace" echo on
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -o build\test\xlir_function_lower.exe ^
    test\cases\xlir_function_lower.c ^
    src\ast\xl6_ast.c ^
    src\type\xl6_type_model.c ^
    src\ir\xl6_ir.c src\ir\xl6_ir_lower.c ^
    src\diag\xl6_diag.c ^
    src\common\xl6_output.c ^
    src\common\xl6_console.c ^
    %XL6_CGEN_LINKS%
if errorlevel 1 exit /b 1
build\test\xlir_function_lower.exe
if errorlevel 1 exit /b 1

"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -o build\test\xlir_cfg_lower.exe ^
    test\cases\xlir_cfg_lower.c ^
    src\ast\xl6_ast.c ^
    src\type\xl6_type_model.c ^
    src\ir\xl6_ir.c src\ir\xl6_ir_lower.c ^
    src\cgen\xl6_ir_cgen.c ^
    src\cgen\xl6_cgen_shared_runtime.c ^
    src\diag\xl6_diag.c ^
    src\common\xl6_output.c ^
    src\common\xl6_console.c ^
    %XL6_CGEN_LINKS%
if errorlevel 1 exit /b 1
build\test\xlir_cfg_lower.exe
if errorlevel 1 exit /b 1

"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -o build\test\xlir_loop_lower.exe ^
    test\cases\xlir_loop_lower.c ^
    src\ast\xl6_ast.c ^
    src\type\xl6_type_model.c ^
    src\ir\xl6_ir.c src\ir\xl6_ir_lower.c ^
    src\diag\xl6_diag.c ^
    src\common\xl6_output.c ^
    src\common\xl6_console.c ^
    %XL6_CGEN_LINKS%
if errorlevel 1 exit /b 1
build\test\xlir_loop_lower.exe
if errorlevel 1 exit /b 1

"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -o build\test\xlir_do_while_lower.exe ^
    test\cases\xlir_do_while_lower.c ^
    src\ast\xl6_ast.c ^
    src\type\xl6_type_model.c ^
    src\ir\xl6_ir.c src\ir\xl6_ir_lower.c ^
    src\cgen\xl6_ir_cgen.c ^
    src\cgen\xl6_cgen_shared_runtime.c ^
    src\diag\xl6_diag.c ^
    src\common\xl6_output.c ^
    src\common\xl6_console.c ^
    %XL6_CGEN_LINKS%
if errorlevel 1 exit /b 1
build\test\xlir_do_while_lower.exe
if errorlevel 1 exit /b 1

"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -o build\test\xlir_throw_lower.exe ^
    test\cases\xlir_throw_lower.c ^
    src\ast\xl6_ast.c ^
    src\type\xl6_type_model.c ^
    src\ir\xl6_ir.c src\ir\xl6_ir_lower.c ^
    src\cgen\xl6_ir_cgen.c ^
    src\cgen\xl6_cgen_shared_runtime.c ^
    src\diag\xl6_diag.c ^
    src\common\xl6_output.c ^
    src\common\xl6_console.c ^
    %XL6_CGEN_LINKS%
if errorlevel 1 exit /b 1
build\test\xlir_throw_lower.exe
if errorlevel 1 exit /b 1

"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -o build\test\xlir_cgen.exe ^
    test\cases\xlir_cgen.c ^
    src\ast\xl6_ast.c ^
    src\type\xl6_type_model.c ^
    src\ir\xl6_ir.c src\ir\xl6_ir_lower.c ^
    src\cgen\xl6_ir_cgen.c ^
    src\cgen\xl6_cgen_shared_runtime.c ^
    src\diag\xl6_diag.c ^
    src\common\xl6_output.c ^
    src\common\xl6_console.c ^
    %XL6_CGEN_LINKS%
if errorlevel 1 exit /b 1
build\test\xlir_cgen.exe
if errorlevel 1 exit /b 1

"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -o build\test\xlir_short_circuit_cgen.exe ^
    test\cases\xlir_short_circuit_cgen.c ^
    src\ast\xl6_ast.c ^
    src\type\xl6_type_model.c ^
    src\ir\xl6_ir.c src\ir\xl6_ir_lower.c ^
    src\cgen\xl6_ir_cgen.c ^
    src\cgen\xl6_cgen_shared_runtime.c ^
    src\diag\xl6_diag.c ^
    src\common\xl6_output.c ^
    src\common\xl6_console.c ^
    %XL6_CGEN_LINKS%
if errorlevel 1 exit /b 1
build\test\xlir_short_circuit_cgen.exe
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% test\cases\xlir_short_circuit_host.c -o build\test\xlir_short_circuit_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_short_circuit_host.exe
if errorlevel 1 exit /b 1

"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -o build\test\xlir_conditional_cgen.exe ^
    test\cases\xlir_conditional_cgen.c ^
    src\ast\xl6_ast.c ^
    src\type\xl6_type_model.c ^
    src\ir\xl6_ir.c src\ir\xl6_ir_lower.c ^
    src\cgen\xl6_ir_cgen.c ^
    src\cgen\xl6_cgen_shared_runtime.c ^
    src\diag\xl6_diag.c ^
    src\common\xl6_output.c ^
    src\common\xl6_console.c ^
    %XL6_CGEN_LINKS%
if errorlevel 1 exit /b 1
build\test\xlir_conditional_cgen.exe
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% test\cases\xlir_conditional_host.c -o build\test\xlir_conditional_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_conditional_host.exe
if errorlevel 1 exit /b 1

"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -o build\test\xlir_module_init_cgen.exe ^
    test\cases\xlir_module_init_cgen.c ^
    src\ast\xl6_ast.c ^
    src\type\xl6_type_model.c ^
    src\ir\xl6_ir.c src\ir\xl6_ir_lower.c ^
    src\cgen\xl6_ir_cgen.c ^
    src\cgen\xl6_cgen_shared_runtime.c ^
    src\diag\xl6_diag.c ^
    src\common\xl6_output.c ^
    src\common\xl6_console.c ^
    %XL6_CGEN_LINKS%
if errorlevel 1 exit /b 1
build\test\xlir_module_init_cgen.exe
if errorlevel 1 exit /b 1

rem Every specialized XLIR semantic guard that is not already named in the
rem regression suites is discovered and executed here.  This keeps new IR ABI
rem contracts in the full regression without duplicating a hand-maintained list.
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_xlir_guards.ps1 -Mode FullRegressionSupplement
if errorlevel 1 exit /b 1

echo DEMO6 suite ok: language frontend and type semantics
exit /b 0

