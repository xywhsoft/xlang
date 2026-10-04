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
cases=(stdlib_stack_native stdlib_stack_scalar xlir_stack_resource_abi xlir_stack_class_default_abi stdlib_stack_values stdlib_stack_owned stdlib_stack_owned_continuation stdlib_stack_try_owned stdlib_stack_try_continuation)
negative=(char_output_type bytes_output_type bytes_input_type owned_argument owned_depth owned_result try_argument try_depth try_result try_nullable_result)
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
    printf '%s\0' test/cases/xlir_module_product_oom_host.c
    printf '%s\0' test/support/module_product_memory.c test/support/module_product_memory.h
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
        test/support/stack_native.c src/runtime/xl6_stack.c \
        -no-pie -pthread -lm -o "$OUT/native" || return
    run native-vectors "$OUT/native" || return
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
    [[ "$name" = xlir_stack_class_default_abi || "$name" = stdlib_stack_owned_continuation || "$name" = stdlib_stack_try_continuation ]] || entry=(-DXL6_TIME_USER_ENTRY)
    run "$name.emit" "$CLI" cgen "$source" --inc-path "$OUT/include" --emit "$OUT/$name.c" || return
    run "$name.repeat" "$CLI" cgen "$source" --inc-path "$OUT/include" --emit "$OUT/$name.repeat.c" || return
    run "$name.deterministic" cmp "$OUT/$name.c" "$OUT/$name.repeat.c" || return

        run "$name.contract" "$OUT/contract" "$source" "$OUT/$name.contract.c" || return
        run "$name.contract-equality" cmp "$OUT/$name.c" "$OUT/$name.contract.c" || return
    if grep -Eq 'xlir_stack_(handle|payload|create|push|clone|raise|reserve|pop|peek|equals|trim|clear|size)' "$OUT/$name.c"; then return 1; fi

    run "$name.jit" "$CLI" run "$source" --inc-path "$OUT/include" || return
    run "$name.aot-build" "$CLI" build "$source" --inc-path "$OUT/include" --save-temps -o "$OUT/$name.aot" || return
    run "$name.aot" "$OUT/$name.aot" || return
    run "$name.native-plan" "$OUT/artifact" --emit-native "$source" "$OUT/$name.native.c" || return
    saved="$(sed -n 's/^temporary files kept: //p' "$OUT/$name.aot-build.log")"
    [[ -n "$saved" && -f "$saved/xl6_xrt_impl.c" ]] || return 1
    run "$name.actual-native-equality" cmp "$saved/xl6_xrt_impl.c" "$OUT/$name.native.c" || return
    run "$name.plan-sources" "$PY" -c 'import json,sys;p=next(json.loads(s) for s in open(sys.argv[1]) if s.startswith("{\"Fixture\""));n=sys.argv[2];assert "src/runtime/xl6_stack.c" in p["Sources"] and "XRUNTIME_MODULE_TYPED_STACK" in p["NativeModules"];assert not any(s for s in p["Sources"] if any(k in s for k in ("xl6_buffer_","xl6_regex_","xl6_log_","xl6_process_")));print("\n".join(p["Sources"]))' "$OUT/$name.native-plan.log" "$name" || return
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
    if [[ "$name" = stdlib_stack_native || "$name" = stdlib_stack_values || "$name" = stdlib_stack_owned || "$name" = stdlib_stack_try_owned ]]; then
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
        owned_argument) diagnostic='\[sem\].*\[SC-ARG-COUNT\]' ;;
        owned_depth) diagnostic='\[sem\].*Stack\.peekAt.*: target int, value string' ;;
        owned_result) diagnostic='\[sem\].*: target string, value bytes' ;;
        try_argument) diagnostic='\[sem\].*\[SC-ARG-COUNT\]' ;;
        try_depth) diagnostic='\[sem\].*Stack\.peekAt.*: target int, value string' ;;
        try_result) diagnostic='\[sem\].*: target nullable<bytes>, value nullable' ;;
        try_nullable_result) diagnostic='\[sem\].*: target string, value nullable' ;;
    esac
    if "$CLI" cgen "test/fixtures/stack_value_contract/$name.xl" --emit "$target" >"$OUT/negative.$name.log" 2>&1; then return 1; fi
    [[ ! -e "$target" ]] && grep -Eq "$diagnostic" "$OUT/negative.$name.log" || return 1
    checks=$((checks+1))
}
for name in "${negative[@]}"; do
    if ! reject_fixture "$name"; then failures+=("negative:$name"); fi
done
frozen_files | sort -zu >"$OUT/inputs.after.list"
if ! run inventory-unchanged cmp "$OUT/inputs.list" "$OUT/inputs.after.list"; then failures+=(inputs-changed); fi
if ! run inputs-unchanged sha256sum --check "$OUT/inputs.sha256"; then failures+=(inputs-changed); fi
"$PY" -c 'import json,sys;p=sys.argv;f=p[4:];json.dump({"Complete":not f,"Checks":int(p[2]),"PositiveFixtures":9,"NegativeFixtures":10,"InputsUnchanged":p[3]=="true","Failures":f,"Scope":"Linux actual strict C/JIT/AOT and pruned generated/resident/native Stack ASan/UBSan/leaks: exact owned and bool/optional try results, nullable float32 and flow-refined string/bytes writeback, parameter isolation, continuation lifetime and pre-output result/effect mutation rejection. Native and calling-thread shared runtime OOM prefixes; shared runtime and CLI/JIT are not instrumented. Legacy Into retirement, batch and other 53 modules remain pending."},open(p[1],"w"),indent=2)' \
    "$OUT/results.json" "$checks" "$( [[ ! " ${failures[*]} " = *' inputs-changed '* ]] && printf true || printf false )" "${failures[@]}"
printf 'Stack native Linux: %s successful checks, %s failed groups\n' "$checks" "${#failures[@]}"
[[ ${#failures[@]} = 0 ]]
