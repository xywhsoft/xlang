#!/usr/bin/env bash
# Real Linux products and selected generated/resident/native closures. The
# separate logical OOM harness deliberately links the full shared debug XRT.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
OUT="${1:?Provide a fresh evidence directory}"
[[ "$OUT" = /* ]] || OUT="$ROOT/$OUT"
[[ ! -e "$OUT" ]] || { printf 'Preserve module product evidence\n' >&2; exit 1; }
[[ "$(uname -m)" = x86_64 ]] && getconf GNU_LIBC_VERSION >/dev/null
[[ -x release/xl && -f release/libxl.so && -f release/libxlc.so ]]
mkdir -p "$OUT/include" "$OUT/components"
CLI="$ROOT/release/xl"; CC="$(command -v gcc)"; PY="$(command -v python3)"
checks=0; failures=()
cases=(xlir_module_product_initializer xlir_module_product_globals xlir_module_product_closure
    xlir_module_product_entry cgen_module_globals cgen_module_global_closure cgen_generic_multi_return
    stdlib_buffer_value stdlib_buffer_snapshot)
negative=(sem_module_product_duplicate sem_module_product_conflict sem_module_product_forward_initializer
    bad_xlir_print_no_raise bad_xlir_print_input)
run() {
    local name="$1"; shift
    if ! "$@" >"$OUT/$name.log" 2>&1; then
        tail -n 25 "$OUT/$name.log" >&2
        printf 'Module product Linux failed: %s\n' "$name" >&2
        return 1
    fi
    checks=$((checks+1))
}
frozen_files() {
    find src lib release/include -type f -print0
    printf '%s\0' "$CC" "$PY" release/xl release/libxl.so release/libxlc.so build_linux.sh
    printf '%s\0' tools/check_xlir_module_product_globals.sh tools/check_xlir_module_product_ir.py
    printf '%s\0' tools/tcc_vfs_manifest_linux_xlc.txt test/cases/xlir_artifact_runtime_contract.c
    printf '%s\0' test/cases/xlir_module_product_oom_host.c test/cases/xlir_time_text_closure_host.c
    printf '%s\0' test/support/module_product_memory.c test/support/module_product_memory.h
    for fixture in "${cases[@]}" "${negative[@]}"; do printf '%s\0' "test/cases/$fixture.xl"; done
}
frozen_files | sort -zu >"$OUT/inputs.list"
xargs -0 sha256sum <"$OUT/inputs.list" >"$OUT/inputs.sha256"
python3 tools/stage_runtime_headers.py --root "$ROOT" --out "$OUT"
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
run debug-host-build "$CC" "${strict[@]}" "${sanitize[@]}" -c test/support/module_product_memory.c -o "$OUT/debug-host.o"
fixture() {
    local name="$1" source="test/cases/$1.xl" saved component object point terminal
    local entry=() oom_entry=() objects=() components=()
    if [[ "$name" = xlir_module_product_entry || "$name" = cgen_module_globals ]]; then entry=(-DXL6_TIME_USER_ENTRY); fi
    run "$name.emit" "$CLI" cgen "$source" --inc-path "$OUT/include" --emit "$OUT/$name.c" || return
    run "$name.repeat" "$CLI" cgen "$source" --inc-path "$OUT/include" --emit "$OUT/$name.repeat.c" || return
    run "$name.deterministic" cmp "$OUT/$name.c" "$OUT/$name.repeat.c" || return
    run "$name.jit" "$CLI" run "$source" --inc-path "$OUT/include" || return
    run "$name.aot-build" "$CLI" build "$source" --inc-path "$OUT/include" --save-temps -o "$OUT/$name.aot" || return
    run "$name.aot" "$OUT/$name.aot" || return
    run "$name.native-plan" "$OUT/artifact" --emit-native "$source" "$OUT/$name.native.c" || return
    saved="$(sed -n 's/^temporary files kept: //p' "$OUT/$name.aot-build.log")"
    [[ -n "$saved" && -f "$saved/xl6_xrt_impl.c" ]] || return 1
    run "$name.actual-native-equality" cmp "$saved/xl6_xrt_impl.c" "$OUT/$name.native.c" || return
    run "$name.plan-sources" "$PY" -c 'import json,sys;p=next(json.loads(s) for s in open(sys.argv[1]) if s.startswith("{\"Fixture\""));print("\n".join(p["Sources"]))' "$OUT/$name.native-plan.log" || return
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
    run "$name.closure-build" "$CC" "${strict[@]}" "${sanitize[@]}" "${entry[@]}" -no-pie -include "$OUT/$name.c" \
        test/cases/xlir_time_text_closure_host.c "$OUT/$name.native.o" "${objects[@]}" -pthread -ldl -lm -o "$OUT/$name.closure" || return
    run "$name.closure" "$OUT/$name.closure" || return
    if [[ "$name" = xlir_module_product_* ]]; then
        run "$name.ir-emit" "$CLI" ir "$source" --lowered --format json --emit "$OUT/$name.json" || return
        run "$name.ir" "$PY" tools/check_xlir_module_product_ir.py "$name" "$OUT/$name.json" || return
        if [[ "$name" = xlir_module_product_entry ]]; then oom_entry=(-DXL6_PRODUCT_USER_ENTRY); fi
        run "$name.oom-build" "$CC" "${strict[@]}" "${sanitize[@]}" "${oom_entry[@]}" -no-pie \
            -include "$OUT/$name.c" test/cases/xlir_module_product_oom_host.c "$OUT/debug-host.o" \
            -lxl "${libs[@]}" -o "$OUT/$name.oom" || return
        run "$name.oom-normal" "$OUT/$name.oom" || return
        terminal=false
        for ((point=0;point<=4096;++point)); do
            run "$name.oom-$point" "$OUT/$name.oom" "$point" || return
            if grep -q 'hit=0 balanced' "$OUT/$name.oom-$point.log"; then terminal=true; break; fi
        done
        "$terminal" || { printf 'Incomplete allocation prefix: %s\n' "$name" >&2; return 1; }
        printf '%s %s\n' "$name" "$point" >>"$OUT/oom-terminals.txt"
    fi
}
for name in "${cases[@]}"; do
    printf 'Module product Linux fixture: %s\n' "$name"
    if ! fixture "$name"; then failures+=("fixture:$name"); fi
done
reject() {
    local name="$1" marker
    if "$CLI" cgen "test/cases/$name.xl" --inc-path "$OUT/include" --emit "$OUT/$name.c" >"$OUT/$name.log" 2>&1; then return 1; fi
    [[ ! -e "$OUT/$name.c" ]] || return 1
    case "$name" in
        sem_module_product_forward_initializer) marker="undeclared identifier 'second'";;
        bad_xlir_print_*) marker='XLIR direct runtime bridge has no exact declared ABI|has no exact compiler-runtime bridge ABI';;
        *) marker='重复|already|duplicate|conflict|redeclar';;
    esac
    grep -Eq "$marker" "$OUT/$name.log" || return
    checks=$((checks+1))
}
for name in "${negative[@]}"; do if ! reject "$name"; then failures+=("negative:$name"); fi; done
frozen_files | sort -zu >"$OUT/inputs.after.list"
if ! run inventory-unchanged cmp "$OUT/inputs.list" "$OUT/inputs.after.list"; then failures+=(inputs-changed); fi
if ! run inputs-unchanged sha256sum --check "$OUT/inputs.sha256"; then failures+=(inputs-changed); fi
"$PY" -c 'import json,sys;p=sys.argv;f=p[4:];json.dump({"Complete":not f,"Checks":int(p[2]),"PositiveFixtures":9,"NegativeFixtures":5,"InputsUnchanged":p[3]=="true","Failures":f,"Scope":"Linux canonical module product scope/capture/cell construction and checked print propagation. Actual strict C/JIT/AOT/pruned closure ASan/UBSan/leaks. Separate full shared memory-debug XRT logical allocation prefix; shared libraries and CLI/JIT are not instrumented. Not all 53 modules."},open(p[1],"w"),indent=2)' \
    "$OUT/results.json" "$checks" "$( [[ ! " ${failures[*]} " = *' inputs-changed '* ]] && printf true || printf false )" "${failures[@]}"
printf 'Module product Linux: %s successful checks, %s failed groups\n' "$checks" "${#failures[@]}"
[[ ${#failures[@]} = 0 ]]
