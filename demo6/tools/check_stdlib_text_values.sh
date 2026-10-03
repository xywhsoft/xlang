#!/usr/bin/env bash
# Linux B1-B4 text acceptance. Native algorithm probes and actual pruned
# generated/runtime closures are instrumented; the shared CLI/JIT is not.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
OUT="${1:?Provide a fresh evidence directory}"
[[ "$OUT" = /* ]] || OUT="$ROOT/$OUT"
[[ ! -e "$OUT" ]] || { printf 'Preserve previous text evidence\n' >&2; exit 1; }
[[ "$(uname -m)" = x86_64 ]] && getconf GNU_LIBC_VERSION >/dev/null
[[ -x release/xl && -f release/libxl.so && -f release/libxlc.so ]]
mkdir -p "$OUT/include" "$OUT/components" "$OUT/native"
CLI="$ROOT/release/xl"
CC="$(command -v gcc)"
checks=0; component_builds=0; native_builds=0; cache_hits=0
failures=()
run() {
    local name="$1"; shift
    if ! "$@" >"$OUT/$name.log" 2>&1; then
        tail -n 25 "$OUT/$name.log" >&2
        printf 'Text Linux check failed: %s\n' "$name" >&2
        return 1
    fi
    checks=$((checks+1))
}
cases=(stdlib_text_values stdlib_text_sizes stdlib_text_lower_only cgen_extern_defaults
    stdlib_text_queries stdlib_text_binary stdlib_text_glob stdlib_text_distance
    stdlib_text_split stdlib_text_join stdlib_text_split_only stdlib_text_join_only
    stdlib_text_iterator stdlib_text_iterator_only)
negative=(bad_xlir_text_input bad_xlir_text_result bad_xlir_text_effect bad_xlir_text_no_raise
    bad_xlir_text_chars_type bad_xlir_text_byte_type bad_xlir_text_length_width
    bad_xlir_text_split_type bad_xlir_text_join_type bad_xlir_text_iterator_result
    bad_xlir_text_iterator_owner bad_xlir_text_iterator_handle_read bad_xlir_text_iterator_handle_write
    bad_extern_default_type bad_extern_default_order bad_native_extern_default_type bad_native_extern_default_order)
regressions=(cgen_overload_default_rank stdlib_string)
frozen_files() {
    find src lib release/include -type f -print0
    printf '%s\0' "$CC" release/xl release/libxl.so release/libxlc.so build_linux.sh
    printf '%s\0' tools/check_stdlib_text_values.sh tools/tcc_vfs_manifest_linux_xlc.txt
    printf '%s\0' test/cases/xlir_text_value_contract.c test/cases/xlir_artifact_runtime_contract.c
    printf '%s\0' test/cases/xlir_time_text_closure_host.c test/cases/stdlib_text_iterator_generated_host.c
    printf '%s\0' test/cases/stdlib_text_values_host.c test/support/text_selected_native.c
    printf '%s\0' test/cases/stdlib_text_queries_host.c test/support/text_queries_native.c
    printf '%s\0' test/cases/stdlib_text_split_host.c test/support/text_split_native.c
    printf '%s\0' test/cases/stdlib_text_iterator_host.c test/support/text_iterator_native.c
    printf '%s\0' test/cases/stdlib_string_support.c
    for fixture in "${cases[@]}" "${negative[@]}" "${regressions[@]}"; do
        printf '%s\0' "test/cases/$fixture.xl"
    done
}
frozen_files | sort -zu >"$OUT/inputs.list"
xargs -0 sha256sum <"$OUT/inputs.list" >"$OUT/inputs.sha256"
while IFS='|' read -r kind target source; do
    if [[ "$kind" = file && "$target" = include/__xl_runtime/* ]]; then
        source="${source//%XL6_ROOT%/$ROOT}"
        mkdir -p "$OUT/$(dirname "$target")"
        cp "$source" "$OUT/$target"
        cmp "$source" "$OUT/$target"
    fi
done < tools/tcc_vfs_manifest_linux_xlc.txt
strict=(-std=c11 -D_GNU_SOURCE -Wall -Wextra -Werror -DXL_STATIC_EMBED
    -Wno-unused-function -Wno-unused-label -Wno-unused-variable -Wno-unused-parameter
    -Wno-unused-const-variable -Wno-unused-but-set-variable -Wno-missing-field-initializers
    -Wno-pointer-sign -Wno-format-truncation -I. -Irelease -Irelease/include -I"$OUT/include")
libs=(-Lrelease -Wl,-rpath,"$ROOT/release" -pthread -ldl -lm)
# Variable-value debug tracking is unnecessary for sanitizer stack locations
# and becomes very expensive in large generated initializer CFGs. Keep line
# information, frame pointers and every sanitizer check.
sanitize=(-O1 -g -fno-var-tracking -fno-var-tracking-assignments
    -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
run contract-build "$CC" "${strict[@]}" test/cases/xlir_text_value_contract.c -lxlc "${libs[@]}" -o "$OUT/contract"
run artifact-build "$CC" "${strict[@]}" test/cases/xlir_artifact_runtime_contract.c -lxlc "${libs[@]}" -o "$OUT/artifact"
run artifact-catalog "$OUT/artifact" --catalog

native_probe() {
    local kind="$1"; shift
    local modes=() sources=() limit=256
    case "$kind" in
        value) sources=(test/cases/stdlib_text_values_host.c test/support/text_selected_native.c src/runtime/xl6_text_value.c);;
        query) sources=(test/cases/stdlib_text_queries_host.c test/support/text_queries_native.c
            src/runtime/xl6_text_query.c src/runtime/xl6_text_binary.c src/runtime/xl6_text_chars.c
            src/runtime/xl6_text_glob.c src/runtime/xl6_text_distance.c); modes=(bytes chars distance);;
        split) sources=(test/cases/stdlib_text_split_host.c test/support/text_split_native.c
            src/runtime/xl6_text_split.c src/runtime/xl6_text_join.c)
            modes=(split lines fields join split-large lines-large fields-large join-large); limit=512;;
        iterator) sources=(test/cases/stdlib_text_iterator_host.c test/support/text_iterator_native.c src/runtime/xl6_text_iterator.c)
            modes=(0 1 2); limit=32;;
        *) return 1;;
    esac
    run "$kind.native-build" "$CC" "${strict[@]}" "${sanitize[@]}" "${sources[@]}" -no-pie -pthread -lm -o "$OUT/$kind.native" || return
    run "$kind.native" "$OUT/$kind.native" || return
    if [[ "$kind" = iterator ]]; then run iterator.reentry "$OUT/iterator.native" reentry || return; fi
    local point mode terminal
    for mode in "${modes[@]}"; do
        terminal=false
        for ((point=1;point<=limit;++point)); do
            run "$kind.native.$mode.oom-$point" "$OUT/$kind.native" "$mode" "$point" || return
            if grep -q 'hit=0 balanced' "$OUT/$kind.native.$mode.oom-$point.log"; then terminal=true; break; fi
        done
        "$terminal" || { printf 'Incomplete native allocation prefix: %s/%s\n' "$kind" "$mode" >&2; return 1; }
    done
}
for kind in value query split iterator; do
    if ! native_probe "$kind"; then failures+=("native:$kind"); fi
done

native_object() {
    local name="$1" source="$2" debug="$3" key
    key="$(sha256sum "$source" | cut -d' ' -f1)-$debug"
    native_result="$OUT/native/$key.o"
    if [[ -f "$native_result" ]]; then cache_hits=$((cache_hits+1)); return; fi
    local flags=()
    if [[ "$debug" = debug ]]; then flags=(-DXRT_MODULE_MEMORY_DEBUG); fi
    run "$name.native-$debug-build" "$CC" "${strict[@]}" "${sanitize[@]}" "${flags[@]}" -Ilib/xruntime -c "$source" -o "$native_result.partial" || return
    mv "$native_result.partial" "$native_result" || return
    native_builds=$((native_builds+1))
}
fixture() {
    local name="$1" source="test/cases/$1.xl" saved component object
    run "$name.emit" "$CLI" cgen "$source" --inc-path "$OUT/include" --emit "$OUT/$name.c" || return
    run "$name.repeat" "$CLI" cgen "$source" --inc-path "$OUT/include" --emit "$OUT/$name.repeat.c" || return
    run "$name.deterministic" cmp "$OUT/$name.c" "$OUT/$name.repeat.c" || return
    if [[ "$name" != cgen_extern_defaults ]]; then
        run "$name.contract" "$OUT/contract" "$source" "$OUT/$name.contract.c" || return
        run "$name.contract-equality" cmp "$OUT/$name.c" "$OUT/$name.contract.c" || return
        if grep -Eq 'xl6_xrt_(take_cstr|string_(lower|upper|trim|filter|replace|slice|repeat|reverse|compare|char_at|byte_at|chars|bytes|pad_[a-z]+|like|approx|sim|split|split_lines|join))\(|^[[:space:]]*(static[[:space:]]+)?(xstrview|xlir_string)[[:space:]]+xl6Text[^;]*\{' "$OUT/$name.c"; then
            printf 'Generated text algorithm/legacy bridge retained: %s\n' "$name" >&2; return 1
        fi
    fi
    run "$name.jit" "$CLI" run "$source" --inc-path "$OUT/include" || return
    run "$name.aot-build" "$CLI" build "$source" --inc-path "$OUT/include" --save-temps -o "$OUT/$name.aot" || return
    run "$name.aot" "$OUT/$name.aot" || return
    run "$name.native-plan" "$OUT/artifact" --emit-native "$source" "$OUT/$name.native.c" || return
    saved="$(sed -n 's/^temporary files kept: //p' "$OUT/$name.aot-build.log")"
    [[ -n "$saved" && -f "$saved/xl6_xrt_impl.c" ]] || return 1
    run "$name.actual-native-equality" cmp "$saved/xl6_xrt_impl.c" "$OUT/$name.native.c" || return
    cp "$saved/xl6_xrt_impl.c" "$OUT/$name.actual-native.c" || return
    run "$name.plan-sources" python3 -c 'import json,sys;p=next(json.loads(s) for s in open(sys.argv[1]) if s.startswith("{\"Fixture\""));print("\n".join(p["Sources"]))' "$OUT/$name.native-plan.log" || return
    local objects=() components=()
    mapfile -t components <"$OUT/$name.plan-sources.log"
    for component in "${components[@]}"; do
        [[ "$component" = src/* && "$component" != *..* && -f "$component" ]] || return 1
        object="$OUT/components/${component//\//__}.o"
        if [[ ! -f "$object" ]]; then
            run "$name.$(basename "$component").build" "$CC" "${strict[@]}" "${sanitize[@]}" -c "$component" -o "$object.partial" || return
            mv "$object.partial" "$object" || return
            component_builds=$((component_builds+1))
        else cache_hits=$((cache_hits+1)); fi
        objects+=("$object")
    done
    native_object "$name" "$OUT/$name.native.c" normal || return
    run "$name.closure-build" "$CC" "${strict[@]}" "${sanitize[@]}" -no-pie -include "$OUT/$name.c" \
        test/cases/xlir_time_text_closure_host.c "$native_result" "${objects[@]}" -pthread -ldl -lm -o "$OUT/$name.closure" || return
    run "$name.closure" "$OUT/$name.closure" || return
    if [[ "$name" = stdlib_text_iterator || "$name" = stdlib_text_iterator_only ]]; then
        native_object "$name" "$OUT/$name.native.c" debug || return
        run "$name.managed-oom-build" "$CC" "${strict[@]}" "${sanitize[@]}" -no-pie -include "$OUT/$name.c" \
            test/cases/stdlib_text_iterator_generated_host.c "$native_result" "${objects[@]}" -pthread -ldl -lm -o "$OUT/$name.managed-oom" || return
        local point terminal=false
        for ((point=1;point<=512;++point)); do
            run "$name.managed-oom-$point" "$OUT/$name.managed-oom" "$point" || return
            if grep -q 'hit=0 balanced' "$OUT/$name.managed-oom-$point.log"; then terminal=true; break; fi
        done
        "$terminal" || return 1
    fi
    run "$name.selection" python3 -c 'import json,sys;p=next(json.loads(s) for s in open(sys.argv[1]) if s.startswith("{\"Fixture\""));n=sys.argv[2];s=p["Sources"];m=p["NativeModules"];
if n=="stdlib_text_lower_only": assert "src/runtime/xl6_text_value.c" in s and not any("xl6_text_" in x and "xl6_text_value.c" not in x for x in s)
if n=="stdlib_text_join_only": assert "src/runtime/xl6_text_join.c" in s and "src/runtime/xl6_text_split.c" not in s and "XRT_MODULE_STRING_SPLIT" not in m
if n=="stdlib_text_split_only": assert "src/runtime/xl6_text_split.c" in s and "src/runtime/xl6_text_join.c" not in s
if n=="stdlib_text_iterator_only": assert "src/runtime/xl6_text_iterator.c" in s and "src/runtime/xl6_text_split.c" not in s and "src/runtime/xl6_text_join.c" not in s' "$OUT/$name.native-plan.log" "$name" || return
}
for name in "${cases[@]}"; do
    printf 'Text Linux fixture: %s\n' "$name"
    if ! fixture "$name"; then failures+=("fixture:$name"); fi
done
reject() {
    local name="$1" marker='XLIR direct runtime bridge has no exact declared ABI|has no exact compiler-runtime bridge ABI'
    if "$CLI" cgen "test/cases/$name.xl" --inc-path "$OUT/include" --emit "$OUT/$name.c" >"$OUT/$name.log" 2>&1; then return 1; fi
    [[ ! -e "$OUT/$name.c" ]] || return 1
    case "$name" in
        *iterator_handle*) marker=private;;
        bad_extern_default_type) marker='extern default expression does not match parameter type';;
        bad_extern_default_order) marker='required extern parameter follows an optional parameter';;
        bad_native_extern_default_type) marker='默认值表达式类型和参数类型不匹配';;
        bad_native_extern_default_order) marker='必选参数不能出现在可选参数之后';;
    esac
    grep -Eq "$marker" "$OUT/$name.log" || return
    checks=$((checks+1))
}
for name in "${negative[@]}"; do
    if ! reject "$name"; then failures+=("negative:$name"); fi
done
for name in "${regressions[@]}"; do
    sidecar=()
    if [[ "$name" = stdlib_string ]]; then sidecar=(test/cases/stdlib_string_support.c); fi
    if ! run "$name.regression" "$CLI" run "test/cases/$name.xl" "${sidecar[@]}"; then failures+=("regression:$name"); fi
done
frozen_files | sort -zu >"$OUT/inputs.after.list"
if ! run inventory-unchanged cmp "$OUT/inputs.list" "$OUT/inputs.after.list"; then failures+=(inputs-changed); fi
if ! run inputs-unchanged sha256sum --check "$OUT/inputs.sha256"; then failures+=(inputs-changed); fi
python3 -c 'import json,sys;p=sys.argv; failures=p[7:];json.dump({"Complete":not failures,"Checks":int(p[2]),"PositiveFixtures":14,"NegativeFixtures":17,"ComponentCompilations":int(p[3]),"NativeCompilations":int(p[4]),"CacheHits":int(p[5]),"InputsUnchanged":p[6]=="true","Failures":failures,"Scope":"Linux B1-B4 text strict C/JIT/AOT and actual native/runtime/program closures; ASan/UBSan/leaks on native probes and standalone closures, not shared CLI/JIT; iterator current-thread allocation prefixes, no 53-module acceptance"},open(p[1],"w"),indent=2)' \
    "$OUT/results.json" "$checks" "$component_builds" "$native_builds" "$cache_hits" \
    "$( [[ ! " ${failures[*]} " = *' inputs-changed '* ]] && printf true || printf false )" "${failures[@]}"
printf 'Text Linux: %s successful checks, %s failed groups, %s component builds, %s cache hits\n' "$checks" "${#failures[@]}" "$component_builds" "$cache_hits"
[[ ${#failures[@]} = 0 ]]
