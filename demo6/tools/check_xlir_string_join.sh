#!/usr/bin/env bash
# B11 native providers are instrumented; generated libxl hosts use logical OOM.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
OUT="${1:?Provide a fresh evidence directory}"
[[ "$OUT" = /* ]] || OUT="$ROOT/$OUT"
[[ ! -e "$OUT" ]] || { printf 'Preserve prior string evidence\n' >&2; exit 1; }
[[ "$(uname -m)" = x86_64 ]] && getconf GNU_LIBC_VERSION >/dev/null
[[ -x release/xl && -f release/libxl.so && -f release/libxlc.so ]]
mkdir -p "$OUT/include"
CLI="$ROOT/release/xl"; CC="$(command -v gcc)"; checks=0
trap 'status=$?; if ((status)); then printf "{\"Complete\":false,\"Checks\":%d,\"Exit\":%d}\n" "$checks" "$status" >"$OUT/results.json"; fi' EXIT
run() {
    local name="$1"; shift
    if ! timeout 120s "$@" >"$OUT/$name.log" 2>&1; then
        tail -n 30 "$OUT/$name.log" >&2; printf 'String Linux check failed: %s\n' "$name" >&2; exit 1
    fi
    checks=$((checks + 1))
}
cases=(xlir_string_bridge_abi xlir_string_join_failure xlir_string_repeat_abi
    stdlib_text_interpolation stdlib_text_interpolation_error xlir_string_bridge_minimal
    cgen_dynamic_to_typed_containers cgen_implicit_string_methods cgen_function_value_error_propagation)
negative=(xlir_string_bridge_bad_result xlir_string_bridge_bad_parameter
    xlir_string_join_bad_result xlir_string_join_bad_parameter
    xlir_string_bytes_bad_result xlir_string_bytes_bad_parameter
    xlir_string_chars_bad_result xlir_string_chars_bad_parameter
    xlir_string_repeat_bad_result xlir_string_repeat_bad_parameter)
{
    find src lib release/include -type f -print0
    printf '%s\0' "$CC" release/xl release/libxl.so release/libxlc.so build_linux.sh
    printf '%s\0' tools/check_xlir_string_join.sh tools/tcc_vfs_manifest_linux_xlc.txt
    printf '%s\0' test/cases/xlir_text_value_contract.c test/cases/xlir_artifact_runtime_contract.c
    printf '%s\0' test/cases/xlir_string_bridge_abi_host.c test/cases/xlir_string_repeat_host.c
    printf '%s\0' test/cases/xlir_string_interpolation_host.c test/cases/xlir_generated_module_host.c
    printf '%s\0' test/cases/stdlib_string.xl test/cases/stdlib_string_support.c test/support/string_bridge_native.c
    printf '%s\0' release/examples/stdlib_string_boundaries.xl release/examples/stdlib_string_boundaries_c.c
    for fixture in "${cases[@]}" "${negative[@]}"; do printf '%s\0' "test/cases/$fixture.xl"; done
} | sort -zu | xargs -0 sha256sum >"$OUT/inputs.sha256"
while IFS='|' read -r kind target source; do
    if [[ "$kind" = file && "$target" = include/__xl_runtime/* ]]; then
        source="${source//%XL6_ROOT%/$ROOT}"
        mkdir -p "$OUT/$(dirname "$target")"; cp "$source" "$OUT/$target"; cmp "$source" "$OUT/$target"
    fi
done <tools/tcc_vfs_manifest_linux_xlc.txt
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
sanitize=(-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie)
native=(test/support/string_bridge_native.c src/runtime/xl6_text_value.c
    src/runtime/xl6_text_binary.c src/runtime/xl6_text_chars.c src/runtime/xl6_text_split.c src/runtime/xl6_text_join.c)
for mode in bridge repeat; do
    host=test/cases/xlir_string_repeat_host.c
    [[ "$mode" != bridge ]] || host=test/cases/xlir_string_bridge_abi_host.c
    run "$mode.native-build" "$CC" "${strict[@]}" "${sanitize[@]}" -DXLIR_NATIVE_PROBE_ONLY \
        "$host" "${native[@]}" -pthread -lm -o "$OUT/$mode.native"
    run "$mode.native" "$OUT/$mode.native"
done
for fixture in "${cases[@]}"; do
    source="test/cases/$fixture.xl"
    run "$fixture.emit" "$CLI" cgen "$source" --inc-path "$OUT/include" --emit "$OUT/$fixture.c"
    run "$fixture.repeat" "$CLI" cgen "$source" --inc-path "$OUT/include" --emit "$OUT/$fixture.repeat.c"
    run "$fixture.deterministic" cmp "$OUT/$fixture.c" "$OUT/$fixture.repeat.c"
    run "$fixture.contract" "$OUT/contract" "$source" "$OUT/$fixture.contract.c"
    run "$fixture.contract-equality" cmp "$OUT/$fixture.c" "$OUT/$fixture.contract.c"
    if grep -Eq 'xl6_xrt_(string_|take_cstr)|^(static )?(xlir_string|xstrview|xvalue[*]) +xl6Text.*[{]' "$OUT/$fixture.c"; then
        printf 'Legacy/generated string implementation: %s\n' "$fixture" >&2; exit 1
    fi
    host=test/cases/xlir_generated_module_host.c; entry=()
    if [[ "$fixture" = xlir_string_bridge_abi ]]; then host=test/cases/xlir_string_bridge_abi_host.c
    elif [[ "$fixture" = xlir_string_repeat_abi ]]; then host=test/cases/xlir_string_repeat_host.c
    elif [[ "$fixture" = stdlib_text_interpolation ]]; then host=test/cases/xlir_string_interpolation_host.c
    elif [[ "$fixture" = xlir_string_join_failure || "$fixture" = cgen_dynamic_to_typed_containers ||
            "$fixture" = cgen_implicit_string_methods ]]; then entry=(-DXLIR_GENERATED_MODULE_USER_ENTRY)
    fi
    run "$fixture.strict-build" "$CC" "${strict[@]}" -include "$OUT/$fixture.c" \
        -DXLIR_GENERATED_MODULE_PREINCLUDED "${entry[@]}" "$host" -lxl "${libs[@]}" -o "$OUT/$fixture.strict"
    run "$fixture.strict" "$OUT/$fixture.strict"
    if [[ "$fixture" = stdlib_text_interpolation ]]; then
        for ((point=0; point<=256; ++point)); do
            name="$fixture.logical-oom-$point"; run "$name" "$OUT/$fixture.strict" "$point"
            grep -q 'hit=0 balanced' "$OUT/$name.log" && break
        done
        ((point<=256)) || { printf 'Incomplete logical allocation sweep\n' >&2; exit 1; }
    fi
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
    if [[ "$fixture" = xlir_string_bridge_minimal || "$fixture" = xlir_string_repeat_abi ]]; then
        run "$fixture.selection" python3 -c 'import json,sys; p=next(json.loads(s) for s in open(sys.argv[1]) if s.startswith("{\"Fixture\"")); expected="text_binary" if sys.argv[2]=="xlir_string_bridge_minimal" else "text_value"; assert "src/runtime/xl6_"+expected+".c" in p["Sources"]; assert not any("xl6_text_"+s+".c" in x for x in p["Sources"] for s in ("join","split","chars","iterator")); assert "XRT_MODULE_PROCESS" not in p["NativeModules"] and "XRT_MODULE_REGEX" not in p["NativeModules"]' \
            "$OUT/$fixture.native-plan.log" "$fixture"
    fi
done
for fixture in "${negative[@]}"; do
    if "$CLI" cgen "test/cases/$fixture.xl" --inc-path "$OUT/include" --emit "$OUT/$fixture.c" >"$OUT/$fixture.log" 2>&1; then
        printf 'Invalid string ABI accepted: %s\n' "$fixture" >&2; exit 1
    fi
    [[ ! -e "$OUT/$fixture.c" ]]
    grep -Eq 'XLIR direct runtime bridge has no exact declared ABI|has no exact compiler-runtime bridge ABI' "$OUT/$fixture.log"
    checks=$((checks + 1))
done
run stdlib-jit "$CLI" run test/cases/stdlib_string.xl test/cases/stdlib_string_support.c
run stdlib-build "$CLI" build test/cases/stdlib_string.xl test/cases/stdlib_string_support.c -o "$OUT/stdlib"
run stdlib-aot "$OUT/stdlib"
run example-xl "$CLI" run release/examples/stdlib_string_boundaries.xl
run example-c-build "$CC" "${strict[@]}" -Ilib/xrt release/examples/stdlib_string_boundaries_c.c -lxl "${libs[@]}" -o "$OUT/example-c"
run example-c "$OUT/example-c"
run examples-equal cmp "$OUT/example-xl.log" "$OUT/example-c.log"
[[ "$(wc -l <"$OUT/example-xl.log")" = 21 ]]
run inputs-unchanged sha256sum --check "$OUT/inputs.sha256"
printf '{"Complete":true,"Checks":%d,"PositiveFixtures":9,"AbiNegativeFixtures":10,"ActualNativeClosures":9,"InputsUnchanged":true,"SanitizedScope":"selected XRT/native text probes only; generated libxl hosts use logical allocation tracking"}\n' "$checks" >"$OUT/results.json"
printf 'String B11 Linux: %d checks passed; native ASan/UBSan, 2000 threaded joins, 600 repeat cases, logical OOM, strict C/JIT/AOT and 9 actual closures.\n' "$checks" | tee "$OUT/summary.txt"
