#!/usr/bin/env bash
# Linux x86_64 Unicode/HTML B7 acceptance. Build build_linux.sh first.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
OUT="${1:?Provide a fresh evidence directory}"
[[ "$OUT" = /* ]] || OUT="$ROOT/$OUT"
[[ ! -e "$OUT" ]] || { printf 'Preserve prior Unicode/HTML evidence\n' >&2; exit 1; }
[[ "$(uname -m)" = x86_64 ]] && getconf GNU_LIBC_VERSION >/dev/null
[[ -x release/xl && -f release/libxl.so && -f release/libxlc.so ]]
mkdir -p "$OUT/include"
CLI="$ROOT/release/xl"
CC="$(command -v gcc)"
checks=0
run() {
    local name="$1"; shift
    if ! "$@" >"$OUT/$name.log" 2>&1; then
        tail -n 35 "$OUT/$name.log" >&2
        printf 'Unicode/HTML Linux check failed: %s\n' "$name" >&2
        exit 1
    fi
    checks=$((checks + 1))
}
cases=(stdlib_unicode stdlib_unicode_only stdlib_unicode_validator stdlib_html stdlib_html_only)
negative=(bad_xlir_unicode_bytes bad_xlir_unicode_char bad_xlir_unicode_width
    bad_xlir_unicode_validator_owner bad_xlir_unicode_validator_handle_read bad_xlir_html_result)
regressions=(stdlib_number_values stdlib_text_iterator stdlib_text_queries)
{
    find src lib release/include -type f -print0
    printf '%s\0' "$CC" release/xl release/libxl.so release/libxlc.so build_linux.sh
    printf '%s\0' tools/check_stdlib_unicode.sh tools/tcc_vfs_manifest_linux_xlc.txt
    printf '%s\0' test/cases/stdlib_unicode_host.c test/support/unicode_native.c
    printf '%s\0' test/cases/stdlib_text_iterator_generated_host.c test/cases/xlir_generated_module_host.c
    printf '%s\0' test/cases/xlir_text_value_contract.c test/cases/xlir_artifact_runtime_contract.c
    printf '%s\0' test/cases/stdlib_string_support.c
    for fixture in "${cases[@]}" "${negative[@]}" "${regressions[@]}"; do
        printf '%s\0' "test/cases/$fixture.xl"
    done
} | sort -zu | xargs -0 sha256sum >"$OUT/inputs.sha256"
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
run contract-build "$CC" "${strict[@]}" test/cases/xlir_text_value_contract.c -lxlc "${libs[@]}" -o "$OUT/contract"
run artifact-build "$CC" "${strict[@]}" test/cases/xlir_artifact_runtime_contract.c -lxlc "${libs[@]}" -o "$OUT/artifact"
run catalog "$OUT/artifact" --catalog
fixtures=(); for fixture in "${cases[@]}"; do fixtures+=("test/cases/$fixture.xl"); done
run artifact-contract "$OUT/artifact" "${fixtures[@]}"
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
sanitize=(-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer)
run native-build "$CC" "${strict[@]}" "${sanitize[@]}" test/cases/stdlib_unicode_host.c \
    test/support/unicode_native.c src/runtime/xl6_unicode.c src/runtime/xl6_unicode_validator.c \
    src/runtime/xl6_html.c -pthread -lm -o "$OUT/native"
run native "$OUT/native"
for ((mode=0; mode<=4; ++mode)); do
    for ((point=1; point<=32; ++point)); do
        name="native.$mode.oom-$point"
        run "$name" "$OUT/native" "$mode" "$point"
        grep -q 'hit=0 balanced' "$OUT/$name.log" && break
    done
    ((point<=32)) || { printf 'Incomplete native failure sweep\n' >&2; exit 1; }
done
for fixture in "${cases[@]}"; do
    source="test/cases/$fixture.xl"
    run "$fixture.emit" "$CLI" cgen "$source" --inc-path "$OUT/include" --emit "$OUT/$fixture.c"
    run "$fixture.repeat" "$CLI" cgen "$source" --inc-path "$OUT/include" --emit "$OUT/$fixture.repeat.c"
    run "$fixture.deterministic" cmp "$OUT/$fixture.c" "$OUT/$fixture.repeat.c"
    run "$fixture.contract" "$OUT/contract" "$source" "$OUT/$fixture.contract.c"
    run "$fixture.contract-equality" cmp "$OUT/$fixture.c" "$OUT/$fixture.contract.c"
    if grep -Eq '^([[:space:]]*static[[:space:]]+)?(xstrview|xlir_string)[[:space:]]+xl6(Unicode|Html)[a-zA-Z0-9_]*\([^;]*\)[[:space:]]*\{' "$OUT/$fixture.c"; then
        printf 'Generated Unicode/HTML implementation retained\n' >&2; exit 1
    fi
    run "$fixture.strict-build" "$CC" "${strict[@]}" -include "$OUT/$fixture.c" \
        -DXLIR_GENERATED_MODULE_PREINCLUDED test/cases/stdlib_text_iterator_generated_host.c \
        -lxl "${libs[@]}" -o "$OUT/$fixture.strict"
    run "$fixture.strict" "$OUT/$fixture.strict"
    for ((point=1; point<=256; ++point)); do
        name="$fixture.managed-oom-$point"
        run "$name" "$OUT/$fixture.strict" "$point"
        grep -q 'hit=0 balanced' "$OUT/$name.log" && break
    done
    ((point<=256)) || { printf 'Incomplete managed failure sweep\n' >&2; exit 1; }
    run "$fixture.jit" "$CLI" run "$source" --inc-path "$OUT/include"
    run "$fixture.aot-build" "$CLI" build "$source" --inc-path "$OUT/include" --save-temps -o "$OUT/$fixture.aot"
    run "$fixture.aot" "$OUT/$fixture.aot"
    run "$fixture.native-plan" "$OUT/artifact" --emit-native "$source" "$OUT/$fixture.native.c"
    saved="$(sed -n 's/^temporary files kept: //p' "$OUT/$fixture.aot-build.log")"
    [[ -n "$saved" && -f "$saved/xl6_xrt_impl.c" ]]
    run "$fixture.actual-native-equality" cmp "$saved/xl6_xrt_impl.c" "$OUT/$fixture.native.c"
    cp "$saved/xl6_xrt_impl.c" "$OUT/$fixture.actual-native.c"
    run "$fixture.native-build" "$CC" "${strict[@]}" -Ilib/xruntime -c "$OUT/$fixture.native.c" -o "$OUT/$fixture.native.o"
    run "$fixture.generated-build" "$CC" "${strict[@]}" -c "$OUT/$fixture.c" -o "$OUT/$fixture.generated.o"
    mapfile -t components < <(python3 -c 'import json,sys; p=next(json.loads(s) for s in open(sys.argv[1]) if s.startswith("{\"Fixture\"")); [print(s) for s in p["Sources"]]' "$OUT/$fixture.native-plan.log")
    objects=()
    for component in "${components[@]}"; do
        [[ "$component" = src/* && "$component" != *..* && -f "$component" ]]
        object="$OUT/$fixture.$(basename "$component" .c).o"
        run "$fixture.$(basename "$component").build" "$CC" "${strict[@]}" -c "$component" -o "$object"
        objects+=("$object")
    done
    (( ${#objects[@]} > 0 ))
    nm -g --defined-only "$OUT/$fixture.native.o" | awk '/[TRD] xrt/{print $3}' | sort -u >"$OUT/$fixture.defined"
    nm -u "$OUT/$fixture.generated.o" "$OUT/$fixture.native.o" "${objects[@]}" | \
        awk '/ U xrt/{print $2}' | sort -u >"$OUT/$fixture.required"
    comm -23 "$OUT/$fixture.required" "$OUT/$fixture.defined" >"$OUT/$fixture.missing"
    [[ ! -s "$OUT/$fixture.missing" ]] || { cat "$OUT/$fixture.missing" >&2; exit 1; }
    if [[ "$fixture" = stdlib_unicode_only || "$fixture" = stdlib_html_only || "$fixture" = stdlib_unicode_validator ]]; then
        run "$fixture.selection" python3 -c 'import json,re,sys; p=next(json.loads(s) for s in open(sys.argv[1]) if s.startswith("{\"Fixture\"")); name=sys.argv[2]; component={"stdlib_unicode_only":"unicode","stdlib_html_only":"html","stdlib_unicode_validator":"unicode_validator"}[name]; assert "src/runtime/xl6_"+component+".c" in p["Sources"]; unwanted={"unicode","unicode_validator","html"}-{component}; assert not any("src/runtime/xl6_"+x+".c" in p["Sources"] for x in unwanted); assert not any(re.search(r"xl6_(number_|text_)",s) for s in p["Sources"]); assert "XRT_MODULE_CHARSET" not in p["NativeModules"] and "XRT_MODULE_BUFFER" not in p["NativeModules"]' \
            "$OUT/$fixture.native-plan.log" "$fixture"
    fi
done
for fixture in "${negative[@]}"; do
    if "$CLI" cgen "test/cases/$fixture.xl" --inc-path "$OUT/include" --emit "$OUT/$fixture.c" >"$OUT/$fixture.log" 2>&1; then
        printf 'Invalid Unicode/HTML ABI accepted: %s\n' "$fixture" >&2; exit 1
    fi
    [[ ! -e "$OUT/$fixture.c" ]]
    marker='XLIR direct runtime bridge has no exact declared ABI|has no exact compiler-runtime bridge ABI'
    [[ "$fixture" != *handle_read ]] || marker='private'
    grep -Eq "$marker" "$OUT/$fixture.log"
    checks=$((checks + 1))
done
for fixture in "${regressions[@]}"; do
    run "$fixture.regression" "$CLI" run "test/cases/$fixture.xl" test/cases/stdlib_string_support.c
done
run inputs-unchanged sha256sum --check "$OUT/inputs.sha256"
printf '{"Complete":true,"Checks":%d,"PositiveFixtures":5,"AbiNegativeFixtures":6,"ActualNativeClosures":5,"InputsUnchanged":true,"SanitizedScope":"native Unicode/HTML probes only; managed wrappers use logical allocation tracking"}\n' "$checks" >"$OUT/results.json"
printf 'Unicode/HTML Linux: %s checks passed; 5 strict C/JIT/AOT fixtures, 6 ABI negatives, ASan/UBSan native + managed OOM, actual closures and frozen inputs.\n' "$checks" | tee "$OUT/summary.txt"
