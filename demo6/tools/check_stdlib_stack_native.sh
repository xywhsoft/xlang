#!/usr/bin/env bash
# Actual selected closures are sanitized; CLI/JIT and full shared XRT are not.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
OUT="${1:?Provide a fresh evidence directory}"
[[ "$OUT" = /* ]] || OUT="$ROOT/$OUT"
[[ ! -e "$OUT" ]] || { printf 'Preserve prior Stack evidence\n' >&2; exit 1; }
[[ "$(uname -m)" = x86_64 ]] && getconf GNU_LIBC_VERSION >/dev/null
[[ -x release/xl && -f release/libxl.so && -f release/libxlc.so ]]
mkdir -p "$OUT/include" "$OUT/components"
CLI="$ROOT/release/xl"; CC="$(command -v gcc)"; PY="$(command -v python3)"
checks=0; failures=()
cases=(stdlib_stack_native stdlib_stack_scalar xlir_stack_resource_abi xlir_stack_class_default_abi stdlib_stack_values stdlib_stack_owned stdlib_stack_owned_continuation stdlib_stack_try_owned stdlib_stack_try_continuation
    stdlib_stack_nullable_scalar stdlib_stack_nullable_continuation stdlib_stack_nullable_i64 stdlib_stack_nullable_bool stdlib_stack_nullable_u64 stdlib_stack_nullable_f32 stdlib_stack_nullable_f64 stdlib_stack_nullable_time stdlib_stack_nullable_char stdlib_sequence_nullable_identity
    xlir_nullable_scalar_no_resource stdlib_nullable_resource_transport stdlib_nullable_queue_transport stdlib_nullable_text_resource_transport stdlib_nullable_text_batch_projection xlir_nullable_string_equal
    stdlib_nullable_projection_bool stdlib_nullable_projection_i64 stdlib_nullable_projection_u64 stdlib_nullable_projection_f32 stdlib_nullable_projection_f64 stdlib_nullable_projection_time stdlib_nullable_projection_char stdlib_nullable_projection_oom stdlib_scalar_projection_char stdlib_scalar_projection_text stdlib_scalar_projection_oom)
negative=(char_output_type bytes_output_type bytes_input_type owned_argument owned_depth owned_result try_argument try_depth try_result try_nullable_result nullable_literal_range nullable_variable_narrow nullable_identity retired_pop_into retired_peek_into retired_peek_at_into)
nullable_sources=(src/runtime/xl6_nullable_bool.c src/runtime/xl6_nullable_i64.c src/runtime/xl6_nullable_u64.c src/runtime/xl6_nullable_f32.c src/runtime/xl6_nullable_f64.c src/runtime/xl6_nullable_time.c src/runtime/xl6_nullable_char.c src/runtime/xl6_nullable_string.c
    src/runtime/xl6_nullable_bool_value.c src/runtime/xl6_nullable_i64_value.c src/runtime/xl6_nullable_u64_value.c src/runtime/xl6_nullable_f32_value.c src/runtime/xl6_nullable_f64_value.c src/runtime/xl6_nullable_time_value.c src/runtime/xl6_nullable_char_value.c)
run() {
    local name="$1"; shift
    if ! "$@" >"$OUT/$name.log" 2>&1; then
        tail -n 25 "$OUT/$name.log" >&2
        printf 'Stack Linux failed: %s\n' "$name" >&2; return 1
    fi
    checks=$((checks+1))
}
frozen_files() {
    find src lib release/include -type f -print0
    printf '%s\0' "$CC" "$PY" release/xl release/libxl.so release/libxlc.so build_linux.sh
    printf '%s\0' tools/check_stdlib_stack_native.sh
    printf '%s\0' tools/tcc_vfs_manifest_linux_xlc.txt test/cases/xlir_artifact_runtime_contract.c
    printf '%s\0' test/cases/xlir_stack_native_contract.c test/cases/xlir_time_text_closure_host.c
    printf '%s\0' test/cases/stdlib_stack_native_host.c test/support/stack_native.c
    printf '%s\0' test/support/stack_take_found.h test/cases/stdlib_sequence_take_status_native_host.c test/cases/stdlib_stack_retired_try_abi.c
    printf '%s\0' test/cases/xlir_nullable_scalar_native.c
    printf '%s\0' tools/check_stdlib_sequence_clear.py test/support/sequence_clear_probe.c test/support/array_value_native.c
    printf '%s\0' test/cases/stdlib_sequence_clear_native_host.c test/cases/xlir_time_text_closure_host.c
    printf '%s\0' test/cases/stdlib_stack_clear_minimal.xl test/cases/stdlib_stack_clear_error.xl test/cases/stdlib_stack_clear_error_continuation.xl
    printf '%s\0' tools/check_stdlib_collection_admission.py test/cases/stdlib_collection_admission.xl test/cases/stdlib_collection_nominal_admission.xl
    printf '%s\0' test/cases/stdlib_array_defaults.xl test/cases/stdlib_array_defaults_continuation.xl
    printf '%s\0' test/cases/stdlib_array_layouts.xl
    printf '%s\0' tools/check_xlir_function_value_calls.py test/cases/xlir_function_value_calls.xl test/cases/xlir_function_value_calls_contract.c
    printf '%s\0' tools/check_stdlib_callable_scalars.py test/cases/stdlib_callable_scalars.xl test/cases/stdlib_callable_scalars_unused.xl
    printf '%s\0' test/cases/stdlib_callable_scalars_contract.c test/cases/stdlib_callable_scalars_host.c test/cases/xlir_callable_bridge_mutations.h
    printf '%s\0' test/cases/stdlib_callable_integer_domains.xl test/cases/stdlib_callable_integer_domains_host.c
    printf '%s\0' test/cases/stdlib_callable_native_scalars.xl test/cases/stdlib_callable_native_scalars_host.c
    printf '%s\0' test/cases/xlir_module_product_oom_host.c
    printf '%s\0' test/support/module_product_memory.c test/support/module_product_memory.h
    printf '%s\0' tools/check_stdlib_array_input.py test/cases/stdlib_array_input_host.c
    printf '%s\0' test/cases/stdlib_array_future_reference.xl tools/check_stdlib_array_future.py test/cases/stdlib_array_future_native_host.c test/support/array_future_native.c test/cases/stdlib_sequence_retired_admission_abi.c
    printf '%s\0' tools/check_stdlib_array_shuffle.py test/cases/stdlib_array_shuffle_native_host.c test/support/array_shuffle_native.c
    printf '%s\0' tools/check_stdlib_collection_count.py test/cases/stdlib_collection_count_native_host.c test/support/collection_count_native.c
    printf '%s\0' test/cases/stdlib_collection_count_header.c
    printf '%s\0' src/runtime/xl6_collection_count.c test/cases/stdlib_collection_count.xl test/cases/stdlib_collection_count_continuation.xl
    printf '%s\0' test/cases/xlir_collection_count_contract.c test/cases/stdlib_collection_count_projection_host.c
    printf '%s\0' tools/check_stdlib_array_scalar_read.py test/cases/stdlib_array_scalar_read_native_host.c test/cases/stdlib_array_scalar_read_header.c
    printf '%s\0' src/runtime/xl6_array_scalar_read.c test/cases/stdlib_array_scalar_read.xl test/cases/stdlib_array_scalar_read_continuation.xl
    printf '%s\0' test/cases/xlir_array_scalar_read_contract.c test/cases/stdlib_array_scalar_read_projection_host.c
    printf '%s\0' test/cases/stdlib_array_shuffle_default_only.xl test/cases/stdlib_array_shuffle_explicit_only.xl test/cases/stdlib_random_extended.xl
    printf '%s\0' test/cases/stdlib_future_metadata_host.c test/cases/xlir_future_metadata_contract.c
    printf '%s\0' test/cases/stdlib_future_aggregate_variadic_only.xl test/cases/stdlib_future_aggregate_array_only.xl test/cases/stdlib_future_metadata_only.xl
    printf '%s\0' test/cases/stdlib_text_join_only.xl test/cases/stdlib_text_join.xl test/cases/stdlib_regex_set_only.xl
    printf '%s\0' test/cases/stdlib_pattern_builder.xl test/cases/stdlib_pattern_builder_lifetime.xl
    printf '%s\0' test/cases/xlir_task_aggregate_array_abi.xl test/cases/xlir_task_when_any_array_abi.xl
    for fixture in "${cases[@]}"; do printf '%s\0' "test/cases/$fixture.xl"; done
    for fixture in "${negative[@]}"; do printf '%s\0' "test/fixtures/stack_value_contract/$fixture.xl"; done
}
frozen_files | sort -zu >"$OUT/inputs.list"
xargs -0 sha256sum <"$OUT/inputs.list" >"$OUT/inputs.sha256"
while IFS='|' read -r kind target source; do
    if [[ "$kind" = file && "$target" = include/__xl_runtime/* ]]; then
        source="${source//%XL6_ROOT%/$ROOT}"
        mkdir -p "$OUT/$(dirname "$target")"; cp "$source" "$OUT/$target"; cmp "$source" "$OUT/$target"
    fi
done < tools/tcc_vfs_manifest_linux_xlc.txt
strict=(-std=c11 -D_GNU_SOURCE -Wall -Wextra -Werror -DXL_STATIC_EMBED
    -Wno-unused-function -Wno-unused-label -Wno-unused-variable -Wno-unused-parameter
    -Wno-unused-const-variable -Wno-unused-but-set-variable -Wno-missing-field-initializers
    -Wno-pointer-sign -Wno-format-truncation -I. -Irelease -Irelease/include -I"$OUT/include")
libs=(-Lrelease -Wl,-rpath,"$ROOT/release" -pthread -ldl -lm)
sanitize=(-O1 -g -fno-var-tracking -fno-var-tracking-assignments
    -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
run artifact-build "$CC" "${strict[@]}" test/cases/xlir_artifact_runtime_contract.c -lxlc "${libs[@]}" -o "$OUT/artifact"
run contract-build "$CC" "${strict[@]}" test/cases/xlir_stack_native_contract.c -lxlc "${libs[@]}" -o "$OUT/contract"
run debug-host-build "$CC" "${strict[@]}" "${sanitize[@]}" -c test/support/module_product_memory.c -o "$OUT/debug-host.o"
native_probe() {
    run native-build "$CC" "${strict[@]}" "${sanitize[@]}" test/cases/stdlib_stack_native_host.c \
        test/support/stack_native.c src/runtime/xl6_sequence.c src/runtime/xl6_stack.c \
        -no-pie -pthread -lm -o "$OUT/native" || return
    run native-vectors "$OUT/native" || return
    run retired-try.compile "$CC" -std=c11 -Wall -Wextra -Werror -I. -Irelease \
        -c test/cases/stdlib_stack_retired_try_abi.c -o "$OUT/retired-try.o" || return
    if "$CC" "$OUT/retired-try.o" -lxl "${libs[@]}" -o "$OUT/retired-try" >"$OUT/retired-try.link.log" 2>&1; then
        printf 'Retired presence-only Stack ABI linked unexpectedly\n' >&2; return 1
    fi
    grep -Eq 'undefined reference.*xl6StackTryTakeV1' "$OUT/retired-try.link.log" || return
    checks=$((checks+1))
    for level in 0 2; do
        run "take-status.O$level.build" "$CC" -std=c11 -Wall -Wextra -Werror -O"$level" -g \
            -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie -D_GNU_SOURCE \
            -DXL6_TEST_ERASED_STACK -I. -Irelease -Irelease/include \
            test/cases/stdlib_sequence_take_status_native_host.c test/support/array_value_native.c \
            src/runtime/xl6_sequence.c src/runtime/xl6_array.c src/runtime/xl6_stack.c \
            src/runtime/xl6_ownership_plan.c -pthread -lm -o "$OUT/take-status.O$level" || return
        run "take-status.O$level.vectors" "$OUT/take-status.O$level" || return
    done
    run nullable-native-build "$CC" "${strict[@]}" "${sanitize[@]}" test/cases/xlir_nullable_scalar_native.c \
        test/support/stack_native.c src/runtime/xl6_sequence.c src/runtime/xl6_stack.c "${nullable_sources[@]}" \
        -no-pie -pthread -lm -o "$OUT/nullable-native" || return
    run nullable-native-vectors "$OUT/nullable-native" || return
    local operation point terminal
    for operation in create reserve push peek clone trim owned-peek try-peek; do
        terminal=false
        for ((point=0;point<=64;++point)); do
            run "native.$operation.oom-$point" "$OUT/native" "$operation" "$point" || return
            if grep -q 'hit=0 balanced' "$OUT/native.$operation.oom-$point.log"; then terminal=true; break; fi
        done
        "$terminal" || { printf 'Incomplete native core prefix: %s\n' "$operation" >&2; return 1; }
        printf '%s %s\n' "$operation" "$point" >>"$OUT/oom-terminals.txt"
    done
}
if ! native_probe; then failures+=(native); fi
fixture() {
    local name="$1" source="test/cases/$1.xl" saved component object point terminal
    local objects=() components=() entry=()
    [[ "$name" = xlir_stack_class_default_abi || "$name" = stdlib_stack_owned_continuation || "$name" = stdlib_stack_try_continuation || "$name" = stdlib_stack_nullable_continuation ]] || entry=(-DXL6_TIME_USER_ENTRY)
    run "$name.emit" "$CLI" cgen "$source" --inc-path "$OUT/include" --emit "$OUT/$name.c" || return
    run "$name.repeat" "$CLI" cgen "$source" --inc-path "$OUT/include" --emit "$OUT/$name.repeat.c" || return
    run "$name.deterministic" cmp "$OUT/$name.c" "$OUT/$name.repeat.c" || return

    if [[ "$name" != stdlib_nullable_resource_transport && "$name" != stdlib_nullable_queue_transport && "$name" != stdlib_nullable_text_resource_transport && "$name" != stdlib_nullable_text_batch_projection ]]; then
        run "$name.contract" "$OUT/contract" "$source" "$OUT/$name.contract.c" || return
        run "$name.contract-equality" cmp "$OUT/$name.c" "$OUT/$name.contract.c" || return
    fi
    if grep -Eq 'xlir_stack_(handle|payload|create|push|clone|raise|reserve|pop|peek|equals|trim|clear|size)' "$OUT/$name.c"; then return 1; fi
    if grep -Eq 'xl6Stack(PopOrPeek|PeekAt)V1' "$OUT/$name.c"; then return 1; fi
    run "$name.take-status" "$PY" -c 'import re,sys; s=open(sys.argv[1],encoding="utf-8-sig").read(); outcomes=re.findall(r"bool projection_ok_(\d+) = xl6Stack(?:TakeV1|TryTakeV2)\(",s); assert "xl6StackTryTakeV1" not in s; assert len(outcomes)==len(re.findall(r"xl6Stack(?:TakeV1|TryTakeV2)\(v",s)); assert all(re.search(r"if \(projection_ok_"+n+r"\)",s) for n in outcomes)' "$OUT/$name.c" || return

    run "$name.jit" "$CLI" run "$source" --inc-path "$OUT/include" || return
    run "$name.aot-build" "$CLI" build "$source" --inc-path "$OUT/include" --save-temps -o "$OUT/$name.aot" || return
    run "$name.aot" "$OUT/$name.aot" || return
    run "$name.native-plan" "$OUT/artifact" --emit-native "$source" "$OUT/$name.native.c" || return
    saved="$(sed -n 's/^temporary files kept: //p' "$OUT/$name.aot-build.log")"
    [[ -n "$saved" && -f "$saved/xl6_xrt_impl.c" ]] || return 1
    run "$name.actual-native-equality" cmp "$saved/xl6_xrt_impl.c" "$OUT/$name.native.c" || return
    run "$name.plan-sources" "$PY" -c 'import json,sys;p=next(json.loads(s) for s in open(sys.argv[1]) if s.startswith("{\"Fixture\""));n=sys.argv[2];families={"bool","i64","u64","f32","f64","time","char"};family=n.removeprefix("stdlib_nullable_projection_");project=n.startswith("stdlib_nullable_projection_") or n in ("stdlib_scalar_projection_char","stdlib_scalar_projection_text","stdlib_scalar_projection_oom");expected=(families|{f+"_value" for f in families}) if family=="oom" else {family,family+"_value"} if project and family in families else {"string"} if n in ("stdlib_sequence_nullable_identity","stdlib_nullable_text_resource_transport","stdlib_nullable_text_batch_projection","xlir_nullable_string_equal") else families if n=="stdlib_stack_nullable_scalar" else {"i64"} if n in ("stdlib_stack_nullable_continuation","stdlib_nullable_resource_transport") else {n.removeprefix("stdlib_stack_nullable_")} if n.removeprefix("stdlib_stack_nullable_") in families else set();selected={s.removeprefix("src/runtime/xl6_nullable_").removesuffix(".c") for s in p["Sources"] if s.startswith("src/runtime/xl6_nullable_")};assert selected==expected,(selected,expected);assert project or n in ("xlir_nullable_scalar_no_resource","stdlib_nullable_resource_transport","stdlib_nullable_queue_transport","stdlib_nullable_text_resource_transport","stdlib_nullable_text_batch_projection","xlir_nullable_string_equal") or ("src/runtime/xl6_stack.c" in p["Sources"] and "src/runtime/xl6_sequence.c" in p["Sources"] and "src/runtime/xl6_array.c" not in p["Sources"] and "XRUNTIME_MODULE_TYPED_STACK" in p["NativeModules"]);assert not any(s for s in p["Sources"] if any(k in s for k in ("xl6_buffer_","xl6_regex_","xl6_log_","xl6_process_")));assert n not in ("stdlib_scalar_projection_char","stdlib_scalar_projection_oom") or {"src/runtime/xl6_char_type.c","src/runtime/xl6_char_value.c"}<=set(p["Sources"]);assert n!="stdlib_stack_values" or ("src/runtime/xl6_char_type.c" in p["Sources"] and "src/runtime/xl6_char_value.c" not in p["Sources"]);assert n not in ("stdlib_scalar_projection_text","stdlib_scalar_projection_oom") or {"src/runtime/xl6_string_type.c","src/runtime/xl6_string_value.c"}<=set(p["Sources"]);assert n!="stdlib_stack_native" or ("src/runtime/xl6_string_type.c" in p["Sources"] and "src/runtime/xl6_string_value.c" not in p["Sources"]);print("\n".join(p["Sources"]))' "$OUT/$name.native-plan.log" "$name" || return
    mapfile -t components <"$OUT/$name.plan-sources.log"
    for component in "${components[@]}"; do
        [[ "$component" = src/* && "$component" != *..* && -f "$component" ]] || return 1
        object="$OUT/components/${component//\//__}.o"
        if [[ ! -f "$object" ]]; then
            run "$name.$(basename "$component").build" "$CC" "${strict[@]}" "${sanitize[@]}" -c "$component" -o "$object.partial" || return
            mv "$object.partial" "$object" || return
        fi
        objects+=("$object")
    done
    run "$name.native-build" "$CC" "${strict[@]}" "${sanitize[@]}" -Ilib/xruntime -c "$OUT/$name.native.c" -o "$OUT/$name.native.o" || return
    run "$name.closure-build" "$CC" "${strict[@]}" "${sanitize[@]}" -no-pie -include "$OUT/$name.c" \
        "${entry[@]}" test/cases/xlir_time_text_closure_host.c "$OUT/$name.native.o" "${objects[@]}" -pthread -ldl -lm -o "$OUT/$name.closure" || return
    run "$name.closure" "$OUT/$name.closure" || return
    run "$name.retired-symbols" bash -c '! nm -g --defined-only "$1" | grep -Eq "xl6Stack(PopOrPeek|PeekAt|TryTake)V1$"' _ "$OUT/$name.closure" || return
    if [[ "$name" = stdlib_stack_native || "$name" = stdlib_stack_values || "$name" = stdlib_stack_owned || "$name" = stdlib_stack_try_owned || "$name" = stdlib_stack_nullable_scalar || "$name" = stdlib_sequence_nullable_identity || "$name" = stdlib_nullable_text_resource_transport || "$name" = xlir_nullable_string_equal || "$name" = stdlib_nullable_projection_oom || "$name" = stdlib_scalar_projection_oom ]]; then
        run "$name.factory-oom-build" "$CC" "${strict[@]}" "${sanitize[@]}" -no-pie -include "$OUT/$name.c" \
            -DXL6_PRODUCT_USER_ENTRY test/cases/xlir_module_product_oom_host.c "$OUT/debug-host.o" -lxl "${libs[@]}" -o "$OUT/$name.oom" || return
        run "$name.factory-oom-normal" "$OUT/$name.oom" || return
        terminal=false
        for ((point=0;point<=4096;++point)); do
            run "$name.factory.oom-$point" "$OUT/$name.oom" "$point" || return
            if grep -q 'hit=0 balanced' "$OUT/$name.factory.oom-$point.log"; then terminal=true; break; fi
        done
        "$terminal" || { printf 'Incomplete managed factory allocation prefix\n' >&2; return 1; }
        printf '%s %s\n' "$name" "$point" >>"$OUT/oom-terminals.txt"
    fi
}
for name in "${cases[@]}"; do
    printf 'Stack Linux fixture: %s\n' "$name"
    if ! fixture "$name"; then failures+=("fixture:$name"); fi
done
reject_fixture() {
    local name="$1" target="$OUT/negative.$1.c"
    local diagnostic='\[sem\].*Stack.*: target (char|bytes), value (uint32|var|string)'
    case "$name" in
        char_output_type) diagnostic='\[sem\].*: target Stack<uint32>, value Stack<char>' ;;
        bytes_output_type) diagnostic='\[sem\].*: target string, value bytes' ;;
        retired_*) diagnostic='\[sem\].*unknown Stack method' ;;
        owned_argument) diagnostic='\[sem\].*\[SC-ARG-COUNT\]' ;;
        owned_depth) diagnostic='\[sem\].*Stack\.peekAt.*: target int, value string' ;;
        owned_result) diagnostic='\[sem\].*: target string, value bytes' ;;
        try_argument) diagnostic='\[sem\].*\[SC-ARG-COUNT\]' ;;
        try_depth) diagnostic='\[sem\].*Stack\.peekAt.*: target int, value string' ;;
        try_result) diagnostic='\[sem\].*: target nullable<bytes>, value nullable' ;;
        try_nullable_result) diagnostic='\[sem\].*: target string, value nullable' ;;
        nullable_literal_range|nullable_variable_narrow) diagnostic='\[sem\].*Stack.push.*target nullable<int32>, value int' ;;
        nullable_identity) diagnostic='\[sem\].*Stack.push.*target nullable<char>' ;;
    esac
    if "$CLI" cgen "test/fixtures/stack_value_contract/$name.xl" --emit "$target" >"$OUT/negative.$name.log" 2>&1; then return 1; fi
    [[ ! -e "$target" ]] && grep -Eq "$diagnostic" "$OUT/negative.$name.log" || return 1
    checks=$((checks+1))
}
for name in "${negative[@]}"; do
    if ! reject_fixture "$name"; then failures+=("negative:$name"); fi
done
if ! run sequence-clear "$PY" tools/check_stdlib_sequence_clear.py --out "$OUT/sequence-clear" --sanitize; then failures+=(sequence-clear); fi
for mode in base nominal defaults layouts; do
    arguments=(tools/check_stdlib_collection_admission.py --out "$OUT/collection-$mode" --sanitize)
    [[ "$mode" = base ]] || arguments+=("--$mode")
    if ! run "collection-$mode" "$PY" "${arguments[@]}"; then failures+=("collection-$mode"); fi
done
if ! run array-input "$PY" tools/check_stdlib_array_input.py --out "$OUT/array-input" --sanitize; then failures+=(array-input); fi
if ! run array-future "$PY" tools/check_stdlib_array_future.py --out "$OUT/array-future" --sanitize; then failures+=(array-future); fi
if ! run array-shuffle "$PY" tools/check_stdlib_array_shuffle.py --out "$OUT/array-shuffle" --sanitize --language; then failures+=(array-shuffle); fi
if ! run collection-count "$PY" tools/check_stdlib_collection_count.py --out "$OUT/collection-count" --sanitize --language; then failures+=(collection-count); fi
if ! run array-scalar-read "$PY" tools/check_stdlib_array_scalar_read.py --out "$OUT/array-scalar-read" --sanitize --language; then failures+=(array-scalar-read); fi
frozen_files | sort -zu >"$OUT/inputs.after.list"
if ! run inventory-unchanged cmp "$OUT/inputs.list" "$OUT/inputs.after.list"; then failures+=(inputs-changed); fi
if ! run inputs-unchanged sha256sum --check "$OUT/inputs.sha256"; then failures+=(inputs-changed); fi
"$PY" -c 'import json,sys;p=sys.argv;f=p[5:];json.dump({"Complete":not f,"Checks":int(p[2]),"PositiveFixtures":int(p[4]),"NegativeFixtures":16,"InputsUnchanged":p[3]=="true","Failures":f,"Scope":"Linux actual strict C/JIT/AOT and pruned generated/resident/native Stack ASan/UBSan/leaks: owned/try and seven tagged nullable stored profiles plus nullable string presence/equality/hash/match and Queue/AVL transport/projection, canonical carrier identity, padding-independent compare/hash, literal fit/widening, parameter isolation, continuation/finally lifetime and pre-output mutation rejection. Nullable value-only/raw queue exclude descriptors; consumers select only used descriptors. Native and calling-thread shared runtime OOM prefixes; shared runtime and CLI/JIT are not instrumented. Old Into methods, IR and native exports are retired and rejected; actual native symbols exclude them. General batch and other 53 modules remain pending."},open(p[1],"w"),indent=2)' \
    "$OUT/results.json" "$checks" "$( [[ ! " ${failures[*]} " = *' inputs-changed '* ]] && printf true || printf false )" "${#cases[@]}" "${failures[@]}"
printf 'Stack native Linux: %s successful checks, %s failed groups\n' "$checks" "${#failures[@]}"
[[ ${#failures[@]} = 0 ]]
