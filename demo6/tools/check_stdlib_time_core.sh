#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
OUT="${1:?fresh output directory required}"
[[ "$OUT" = /* ]] || OUT="$ROOT/$OUT"
[[ ! -e "$OUT" ]] || { echo 'Preserve prior time evidence' >&2; exit 1; }
mkdir -p "$OUT"
CC="$(command -v gcc)"; CLI="$ROOT/release/xl"; checks=0
run() { local name="$1"; shift; "$@" >"$OUT/$name.log" 2>&1; checks=$((checks+1)); }
cases=(stdlib_time_core stdlib_time_core_only)
negative=(xlir_time_bad_result xlir_time_bad_input xlir_time_bad_component xlir_time_bad_tolerance xlir_time_bad_effect)
{
    find src lib release/include -type f -print0
    printf '%s\0' "$CC" release/xl release/libxl.so release/libxlc.so build_linux.sh
    printf '%s\0' tools/check_stdlib_time_core.sh tools/check_stdlib_time_core.ps1 tools/tcc_vfs_manifest_linux_xlc.txt
    printf '%s\0' test/cases/xlir_time_core_host.c test/support/time_native.c test/cases/xlir_text_value_contract.c
    printf '%s\0' test/cases/xlir_string_interpolation_host.c test/cases/xlir_artifact_runtime_contract.c
    printf '%s\0' test/cases/xlir_time_closure_host.c
    printf '%s\0' release/examples/stdlib_time_core.xl
    for fixture in "${cases[@]}" "${negative[@]}"; do printf '%s\0' "test/cases/$fixture.xl"; done
} | sort -zu | xargs -0 sha256sum >"$OUT/inputs.sha256"
python3 tools/stage_runtime_headers.py --root "$ROOT" --out "$OUT"
strict=(-std=c11 -D_GNU_SOURCE -Wall -Wextra -Werror -DXL_STATIC_EMBED
    -Wno-unused-function -Wno-unused-label -Wno-unused-variable -Wno-unused-parameter
    -Wno-unused-const-variable -Wno-unused-but-set-variable -Wno-missing-field-initializers
    -Wno-pointer-sign -Wno-format-truncation -I. -Irelease -Irelease/include -I"$OUT/include")
libs=(-Lrelease -Wl,-rpath,"$ROOT/release" -pthread -ldl -lm)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
sanitize=(-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie)
run native-build "$CC" "${strict[@]}" "${sanitize[@]}" test/cases/xlir_time_core_host.c test/support/time_native.c src/runtime/xl6_time_core.c -pthread -ldl -lm -o "$OUT/native"
run native "$OUT/native"
grep -q '5 actual allocation failures, 8 first no-hit completions, balanced' "$OUT/native.log"
run contract-build "$CC" "${strict[@]}" test/cases/xlir_text_value_contract.c "${libs[@]}" -lxlc -lxl -o "$OUT/contract"
run artifact-contract-build "$CC" "${strict[@]}" test/cases/xlir_artifact_runtime_contract.c "${libs[@]}" -lxlc -lxl -o "$OUT/artifact-contract"
for fixture in "${cases[@]}"; do
    source="test/cases/$fixture.xl"
    run "$fixture-emit" "$CLI" cgen "$source" --emit "$OUT/$fixture.c"
    run "$fixture-repeat" "$CLI" cgen "$source" --emit "$OUT/$fixture.repeat.c"
    cmp "$OUT/$fixture.c" "$OUT/$fixture.repeat.c"
    run "$fixture-contract" "$OUT/contract" "$source" "$OUT/$fixture.closed.c"
    cmp "$OUT/$fixture.c" "$OUT/$fixture.closed.c"
    run "$fixture-strict-build" "$CC" "${strict[@]}" -include "$OUT/$fixture.c" test/cases/xlir_string_interpolation_host.c "${libs[@]}" -lxl -o "$OUT/$fixture.strict"
    run "$fixture-strict" "$OUT/$fixture.strict"
    run "$fixture-jit" "$CLI" run "$source"
    run "$fixture-aot-build" "$CLI" build "$source" -o "$OUT/$fixture.aot"
    run "$fixture-aot" "$OUT/$fixture.aot"
    run "$fixture-native-emit" "$OUT/artifact-contract" --emit-native "$source" "$OUT/$fixture.native.c"
    run "$fixture-native-build" "$CC" "${strict[@]}" "${sanitize[@]}" -Ilib/xruntime -c "$OUT/$fixture.native.c" -o "$OUT/$fixture.native.o"
    mapfile -t resident_sources < <(python3 -c 'import json,sys;p=json.loads(open(sys.argv[1]).readline());print("\n".join(p["Sources"]))' "$OUT/$fixture-native-emit.log")
    resident_objects=()
    for ((resident=0; resident<${#resident_sources[@]}; ++resident)); do
        object="$OUT/$fixture.resident-$resident.o"
        run "$fixture-resident-$resident-build" "$CC" "${strict[@]}" "${sanitize[@]}" -c "${resident_sources[$resident]}" -o "$object"
        resident_objects+=("$object")
    done
    run "$fixture-closure-build" "$CC" "${strict[@]}" "${sanitize[@]}" -include "$OUT/$fixture.c" test/cases/xlir_time_closure_host.c "$OUT/$fixture.native.o" "${resident_objects[@]}" -pthread -ldl -lm -o "$OUT/$fixture.closure"
    run "$fixture-closure" "$OUT/$fixture.closure"
    python3 -c 'import json,sys;p=json.loads(open(sys.argv[1]).readline());assert "src/runtime/xl6_time_core.c" in p["Sources"];assert "XRT_MODULE_TIME" in p["NativeModules"];assert not any(s.startswith(("XRT_MODULE_TIME_LOCAL","XRT_MODULE_PATTERN","XRT_MODULE_REGEX_","XRT_MODULE_PROCESS")) for s in p["NativeModules"]);assert not any("src/runtime/xl6_"+s in f for f in p["Sources"] for s in ("pattern_","regex_","time_local","time_text"))' "$OUT/$fixture-native-emit.log"
    if [[ "$fixture" = stdlib_time_core ]]; then
        terminal=false
        for ((point=0; point<1024; ++point)); do
            run "$fixture-oom-$point" "$OUT/$fixture.strict" "$point"
            if grep -q 'hit=0 balanced' "$OUT/$fixture-oom-$point.log"; then terminal=true; break; fi
        done
        "$terminal" || { echo 'Time language allocation sweep incomplete' >&2; exit 1; }
    fi
done
for fixture in "${negative[@]}"; do
    if "$CLI" cgen "test/cases/$fixture.xl" --emit "$OUT/$fixture.c" >"$OUT/$fixture.log" 2>&1; then exit 1; fi
    [[ ! -e "$OUT/$fixture.c" ]]
    grep -Eq 'XLIR direct runtime bridge has no exact declared ABI|has no exact compiler-runtime bridge ABI' "$OUT/$fixture.log"
    checks=$((checks+1))
done
run example-run "$CLI" run release/examples/stdlib_time_core.xl
run example-build "$CLI" build release/examples/stdlib_time_core.xl -o "$OUT/example"
run example-execute "$OUT/example"
sha256sum --quiet -c "$OUT/inputs.sha256"
printf '{"Complete":true,"Checks":%d,"InputsUnchanged":true,"Scope":"Time core subset only. Native adapter test plus two generated-program actual XRT/resident/C closures ASan/UBSan/leaks. Shared language libxl, CLI/JIT are uninstrumented; their allocation sweep uses a logical ledger only. No time/local/text/53-module completion claim."}\n' "$checks" >"$OUT/results.json"
printf 'Time core Linux checks: %d\n' "$checks"
