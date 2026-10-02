#!/usr/bin/env bash
# B13/B14 immutable snapshots and reusable matching; build formal binaries first.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
OUT="${1:?Provide a fresh evidence directory}"
[[ "$OUT" = /* ]] || OUT="$ROOT/$OUT"
[[ ! -e "$OUT" ]] || { printf 'Preserve regex evidence\n' >&2; exit 1; }
[[ "$(uname -m)" = x86_64 ]] && getconf GNU_LIBC_VERSION >/dev/null
mkdir -p "$OUT/include"
CC="$(command -v gcc)"; CLI="$ROOT/release/xl"; checks=0
trap 'status=$?; if ((status)); then printf "{\"Complete\":false,\"Checks\":%d,\"Exit\":%d}\n" "$checks" "$status" >"$OUT/results.json"; fi' EXIT
run() {
    local name="$1"; shift
    if ! timeout 120s "$@" >"$OUT/$name.log" 2>&1; then
        tail -n 35 "$OUT/$name.log" >&2; printf 'Regex check failed: %s\n' "$name" >&2; exit 1
    fi
    checks=$((checks+1))
}
cases=(stdlib_regex_core_only stdlib_regex_match stdlib_regex_errors stdlib_regex_iterators
    stdlib_regex_replace_only stdlib_regex_replace_buffer stdlib_regex_replace_buffer_errors stdlib_regex_split_only stdlib_regex_set_only stdlib_regex_extended_errors stdlib_regex_byte_boundaries)
negative=(xlir_regex_bad_result xlir_regex_bad_owner xlir_regex_bad_constructor xlir_regex_bad_nullable
    xlir_regex_bad_set_items xlir_regex_bad_set_hits xlir_regex_bad_iterator_owner)
privacy=(xlir_regex_private_consume xlir_regex_private_split_consume)
undeclared=(sem_native_class_extern_bad_type sem_native_class_extern_return_bad)
{
    find src lib release/include -type f -print0
    printf '%s\0' "$CC" release/xl release/libxl.so release/libxlc.so build_linux.sh
    printf '%s\0' tools/check_stdlib_regex.sh tools/tcc_vfs_manifest_linux_xlc.txt
    printf '%s\0' test/cases/xlir_regex_host.c test/support/regex_native.c
    printf '%s\0' test/cases/xlir_regex_extended_host.c
    printf '%s\0' test/cases/xlir_text_value_contract.c test/cases/xlir_artifact_runtime_contract.c
    printf '%s\0' test/cases/xlir_string_interpolation_host.c
    for fixture in "${cases[@]}" "${negative[@]}" "${privacy[@]}" "${undeclared[@]}"; do printf '%s\0' "test/cases/$fixture.xl"; done
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
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
sanitize=(-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie)
run native-build "$CC" "${strict[@]}" "${sanitize[@]}" test/cases/xlir_regex_host.c \
    test/support/regex_native.c src/runtime/xl6_regex_core.c src/runtime/xl6_regex_match.c \
    -pthread -ldl -lm -o "$OUT/native"
run native "$OUT/native"
grep -q '68 actual failures, 5 first no-hit completions' "$OUT/native.log"
run extended-native-build "$CC" "${strict[@]}" "${sanitize[@]}" test/cases/xlir_regex_extended_host.c \
    test/support/regex_native.c src/runtime/xl6_regex_core.c src/runtime/xl6_regex_match.c src/runtime/xl6_regex_iterator.c \
    src/runtime/xl6_regex_replace.c src/runtime/xl6_regex_replace_buffer.c src/runtime/xl6_regex_split.c src/runtime/xl6_regex_set.c \
    -pthread -ldl -lm -o "$OUT/extended-native"
run extended-native "$OUT/extended-native"
grep -q '222 actual failures, 9 first no-hit completions' "$OUT/extended-native.log"
run contract-build "$CC" "${strict[@]}" test/cases/xlir_text_value_contract.c -lxlc "${libs[@]}" -o "$OUT/contract"
run artifact-build "$CC" "${strict[@]}" test/cases/xlir_artifact_runtime_contract.c -lxlc "${libs[@]}" -o "$OUT/artifact"
for fixture in "${cases[@]}"; do
    source="test/cases/$fixture.xl"
    run "$fixture.cgen" "$CLI" cgen "$source" --inc-path "$OUT/include" --emit "$OUT/$fixture.c"
    run "$fixture.repeat" "$CLI" cgen "$source" --inc-path "$OUT/include" --emit "$OUT/$fixture.repeat.c"
    run "$fixture.determinism" cmp "$OUT/$fixture.c" "$OUT/$fixture.repeat.c"
    run "$fixture.contract" "$OUT/contract" "$source" "$OUT/$fixture.contract.c"
    run "$fixture.contract-equality" cmp "$OUT/$fixture.c" "$OUT/$fixture.contract.c"
    if grep -Eq 'xl6_xrt_take_cstr|xrtRegexMatcher(Find|Capture)[(]|^(static )?(void[*]|xstrview|int64_t|bool) +xl6Regex.*[{]' "$OUT/$fixture.c"; then
        printf 'Regex implementation in Cgen\n' >&2; exit 1
    fi
    run "$fixture.strict-build" "$CC" "${strict[@]}" -include "$OUT/$fixture.c" \
        test/cases/xlir_string_interpolation_host.c -lxl "${libs[@]}" -o "$OUT/$fixture.strict"
    run "$fixture.strict" "$OUT/$fixture.strict"
    if [[ "$fixture" = stdlib_regex_match || "$fixture" = stdlib_regex_iterators || "$fixture" = stdlib_regex_replace_only ||
          "$fixture" = stdlib_regex_replace_buffer || "$fixture" = stdlib_regex_split_only || "$fixture" = stdlib_regex_set_only ]]; then
        for ((point=0;point<2048;++point)); do
            name="$fixture.oom-$point"; run "$name" "$OUT/$fixture.strict" "$point"
            grep -q 'hit=0 balanced' "$OUT/$name.log" && break
        done
        ((point<2048))
    fi
    run "$fixture.jit" "$CLI" run "$source" --inc-path "$OUT/include"
    run "$fixture.aot-build" "$CLI" build "$source" --inc-path "$OUT/include" --save-temps -o "$OUT/$fixture.aot"
    run "$fixture.aot" "$OUT/$fixture.aot"
    run "$fixture.native-plan" "$OUT/artifact" --emit-native "$source" "$OUT/$fixture.native.c"
    saved="$(sed -n 's/^temporary files kept: //p' "$OUT/$fixture.aot-build.log")"
    [[ -n "$saved" && -f "$saved/xl6_xrt_impl.c" ]]
    run "$fixture.actual-native-equality" cmp "$saved/xl6_xrt_impl.c" "$OUT/$fixture.native.c"
    run "$fixture.native-build" "$CC" "${strict[@]}" -Ilib/xruntime -c "$OUT/$fixture.native.c" -o "$OUT/$fixture.native.o"
    run "$fixture.generated-build" "$CC" "${strict[@]}" -c "$OUT/$fixture.c" -o "$OUT/$fixture.generated.o"
    run "$fixture.selection" python3 -c 'import json,sys; p=next(json.loads(s) for s in open(sys.argv[1]) if s.startswith("{\"Fixture\"")); name=sys.argv[2]; expected={"stdlib_regex_core_only":["core"],"stdlib_regex_match":["core","match"],"stdlib_regex_errors":["core","match"],"stdlib_regex_iterators":["core","match","iterator"],"stdlib_regex_replace_only":["core","replace"],"stdlib_regex_replace_buffer":["core","replace_buffer"],"stdlib_regex_replace_buffer_errors":["core","replace_buffer"],"stdlib_regex_split_only":["core","split"],"stdlib_regex_set_only":["core","set"],"stdlib_regex_extended_errors":["core","match","iterator","replace","split","set"],"stdlib_regex_byte_boundaries":["core","match","replace","split"]}[name]; assert sorted(s for s in p["Sources"] if "src/runtime/xl6_regex_" in s)==sorted("src/runtime/xl6_regex_"+s+".c" for s in expected); assert "XRT_MODULE_REGEX_CORE" in p["NativeModules"]; assert ("XRT_MODULE_BUFFER" in p["NativeModules"])==(name in ("stdlib_regex_replace_buffer","stdlib_regex_replace_buffer_errors")); assert "XRT_MODULE_PATTERN" not in p["NativeModules"]' \
        "$OUT/$fixture.native-plan.log" "$fixture"
done
for fixture in "${negative[@]}"; do
    if "$CLI" cgen "test/cases/$fixture.xl" --emit "$OUT/$fixture.c" >"$OUT/$fixture.log" 2>&1; then exit 1; fi
    [[ ! -e "$OUT/$fixture.c" ]]
    grep -Eq 'XLIR direct runtime bridge has no exact declared ABI|has no exact compiler-runtime bridge ABI' "$OUT/$fixture.log"
    checks=$((checks+1))
done
for fixture in "${privacy[@]}"; do
    if "$CLI" cgen "test/cases/$fixture.xl" --emit "$OUT/$fixture.c" >"$OUT/$fixture.log" 2>&1; then exit 1; fi
    [[ ! -e "$OUT/$fixture.c" ]]
    grep -Eq 'unknown class/union field|private' "$OUT/$fixture.log"
    checks=$((checks+1))
done
for fixture in "${undeclared[@]}"; do
    if "$CLI" cgen "test/cases/$fixture.xl" --emit "$OUT/$fixture.c" >"$OUT/$fixture.log" 2>&1; then exit 1; fi
    [[ ! -e "$OUT/$fixture.c" ]]
    grep -q 'does not support typed container direct C ABI' "$OUT/$fixture.log"
    checks=$((checks+1))
done
run inputs-unchanged sha256sum --check "$OUT/inputs.sha256"
printf '{"Complete":true,"Checks":%d,"PositiveFixtures":11,"AbiNegativeFixtures":7,"PrivacyFixtures":2,"UndeclaredArrayFixtures":2,"ActualNativeClosures":11,"InputsUnchanged":true,"SanitizedScope":"XRT regex and native adapters; generated libxl hosts use logical OOM"}\n' "$checks" >"$OUT/results.json"
printf 'Regex B13/B14 Linux: %d checks passed\n' "$checks" | tee "$OUT/summary.txt"
