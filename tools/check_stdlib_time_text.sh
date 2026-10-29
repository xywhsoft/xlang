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
case_run() { run "$@"; }
cases=(stdlib_time_text stdlib_time_protocol stdlib_time_fields_text stdlib_time_text_only stdlib_time_runtime stdlib_time_core_only stdlib_time_calendar_only stdlib_time_datetime)
negative=(xlir_time_text_bad_result xlir_time_text_bad_owner xlir_time_text_bad_effect xlir_time_text_bad_width)
{
    find src lib release/include -type f -print0
    printf '%s\0' "$CC" release/xl release/libxl.so release/libxlc.so build_linux.sh
    printf '%s\0' tools/check_stdlib_time_text.sh tools/check_stdlib_time_text.ps1 tools/tcc_vfs_manifest_linux_xlc.txt
    printf '%s\0' test/cases/xlir_time_core_host.c test/support/time_native.c test/cases/xlir_text_value_contract.c
    printf '%s\0' test/cases/xlir_string_interpolation_host.c test/cases/xlir_artifact_runtime_contract.c
    printf '%s\0' test/cases/xlir_time_text_closure_host.c
    printf '%s\0' release/examples/stdlib_time_core.xl release/examples/stdlib_time_calendar.xl
    printf '%s\0' test/cases/xlir_time_text_host.c test/support/time_text_native.c release/examples/stdlib_time.xl
    printf '%s\0' release/examples/stdlib_time_c.c test/support/time_expect.c test/cases/stdlib_time.xl test/cases/stdlib_time_extra.xl
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
run native-build "$CC" "${strict[@]}" "${sanitize[@]}" test/cases/xlir_time_text_host.c test/support/time_text_native.c src/runtime/xl6_time_text.c src/runtime/xl6_time_datetime_text.c src/runtime/xl6_time_datetime.c -pthread -ldl -lm -o "$OUT/native"
run native "$OUT/native"
grep -q '11 actual allocation failures, 11 first no-hit completions, balanced' "$OUT/native.log"
run contract-build "$CC" "${strict[@]}" test/cases/xlir_text_value_contract.c "${libs[@]}" -lxlc -lxl -o "$OUT/contract"
run artifact-contract-build "$CC" "${strict[@]}" test/cases/xlir_artifact_runtime_contract.c "${libs[@]}" -lxlc -lxl -o "$OUT/artifact-contract"
for fixture in "${cases[@]}"; do
    source="test/cases/$fixture.xl"
    case_run "$fixture-emit" "$CLI" cgen "$source" --emit "$OUT/$fixture.c"
    case_run "$fixture-repeat" "$CLI" cgen "$source" --emit "$OUT/$fixture.repeat.c"
    cmp "$OUT/$fixture.c" "$OUT/$fixture.repeat.c"
    case_run "$fixture-contract" "$OUT/contract" "$source" "$OUT/$fixture.closed.c"
    cmp "$OUT/$fixture.c" "$OUT/$fixture.closed.c"
    entry=(); hostSource=test/cases/xlir_string_interpolation_host.c
    if [[ "$fixture" = stdlib_time_runtime ]]; then entry=(-DXL6_TIME_USER_ENTRY); hostSource=test/cases/xlir_time_text_closure_host.c; fi
    case_run "$fixture-strict-build" "$CC" "${strict[@]}" "${entry[@]}" -include "$OUT/$fixture.c" "$hostSource" "${libs[@]}" -lxl -o "$OUT/$fixture.strict"
    case_run "$fixture-strict" "$OUT/$fixture.strict"
    case_run "$fixture-jit" "$CLI" run "$source"
    case_run "$fixture-aot-build" "$CLI" build "$source" -o "$OUT/$fixture.aot"
    case_run "$fixture-aot" "$OUT/$fixture.aot"
    case_run "$fixture-native-emit" "$OUT/artifact-contract" --emit-native "$source" "$OUT/$fixture.native.c"
    case_run "$fixture-native-build" "$CC" "${strict[@]}" "${sanitize[@]}" -Ilib/xruntime -c "$OUT/$fixture.native.c" -o "$OUT/$fixture.native.o"
    mapfile -t resident_sources < <(python3 -c 'import json,sys;p=json.loads(open(sys.argv[1]).readline());print("\n".join(p["Sources"]))' "$OUT/$fixture-native-emit.log")
    resident_objects=()
    for ((resident=0; resident<${#resident_sources[@]}; ++resident)); do
        object="$OUT/$fixture.resident-$resident.o"
        case_run "$fixture-resident-$resident-build" "$CC" "${strict[@]}" "${sanitize[@]}" -c "${resident_sources[$resident]}" -o "$object"
        resident_objects+=("$object")
    done
    case_run "$fixture-closure-build" "$CC" "${strict[@]}" "${sanitize[@]}" "${entry[@]}" -include "$OUT/$fixture.c" test/cases/xlir_time_text_closure_host.c "$OUT/$fixture.native.o" "${resident_objects[@]}" -pthread -ldl -lm -o "$OUT/$fixture.closure"
    case_run "$fixture-closure" "$OUT/$fixture.closure"
    python3 -c 'import json,sys;p=json.loads(open(sys.argv[1]).readline());f=sys.argv[2];sources=p["Sources"];assert not any(s.startswith(("XRT_MODULE_PATTERN","XRT_MODULE_REGEX_","XRT_MODULE_PROCESS")) for s in p["NativeModules"]);assert ("src/runtime/xl6_time_text.c" in sources)==(f in ("stdlib_time_text","stdlib_time_protocol","stdlib_time_text_only","stdlib_time_runtime"));assert ("src/runtime/xl6_time_datetime_text.c" in sources)==(f=="stdlib_time_fields_text");assert ("src/runtime/xl6_time_local.c" in sources)==(f=="stdlib_time_runtime")' "$OUT/$fixture-native-emit.log" "$fixture"
    if [[ "$fixture" = stdlib_time_text || "$fixture" = stdlib_time_protocol || "$fixture" = stdlib_time_fields_text || "$fixture" = stdlib_time_datetime ]]; then
        terminal=false
        for ((point=0; point<1024; ++point)); do
            case_run "$fixture-oom-$point" "$OUT/$fixture.strict" "$point"
            if grep -q 'hit=0 balanced' "$OUT/$fixture-oom-$point.log"; then terminal=true; break; fi
        done
        "$terminal" || { echo 'Time language allocation sweep incomplete' >&2; exit 1; }
    fi
done
for fixture in "${negative[@]}"; do
    if "$CLI" cgen "test/cases/$fixture.xl" --emit "$OUT/$fixture.c" >"$OUT/$fixture.log" 2>&1; then exit 1; fi
    [[ ! -e "$OUT/$fixture.c" ]]
    marker='XLIR direct runtime bridge has no exact declared ABI|has no exact compiler-runtime bridge ABI|C ABI extern product requires a closed primitive product profile'
    [[ "$fixture" != xlir_time_datetime_private ]] || marker='private'
    [[ "$fixture" != xlir_time_datetime_readonly ]] || marker='read-only|readonly|read only|not writable|cannot assign'
    grep -Eq "$marker" "$OUT/$fixture.log"
    checks=$((checks+1))
done
run example-run "$CLI" run release/examples/stdlib_time_core.xl
run example-build "$CLI" build release/examples/stdlib_time_core.xl -o "$OUT/example"
run example-execute "$OUT/example"
run calendar-example-run "$CLI" run release/examples/stdlib_time_calendar.xl
run calendar-example-build "$CLI" build release/examples/stdlib_time_calendar.xl -o "$OUT/calendar-example"
run calendar-example-execute "$OUT/calendar-example"
run text-example-run "$CLI" run release/examples/stdlib_time.xl
run text-example-build "$CLI" build release/examples/stdlib_time.xl -o "$OUT/text-example"
run text-example-execute "$OUT/text-example"
run text-companion-build "$CC" "${strict[@]}" -Ilib/xrt release/examples/stdlib_time_c.c "${libs[@]}" -lxl -o "$OUT/text-companion"
run text-companion-run "$OUT/text-companion"
cmp "$OUT/text-example-run.log" "$OUT/text-companion-run.log"
run legacy-helper-build "$CC" "${strict[@]}" -c test/support/time_expect.c -o "$OUT/legacy-helper.o"
for fixture in stdlib_time stdlib_time_extra; do
    source="test/cases/$fixture.xl"
    run "$fixture-emit" "$CLI" cgen "$source" --emit "$OUT/$fixture.c"
    run "$fixture-contract" "$OUT/contract" "$source" "$OUT/$fixture.closed.c"
    cmp "$OUT/$fixture.c" "$OUT/$fixture.closed.c"
    run "$fixture-strict-build" "$CC" "${strict[@]}" -include "$OUT/$fixture.c" test/cases/xlir_time_text_closure_host.c "$OUT/legacy-helper.o" "${libs[@]}" -lxl -o "$OUT/$fixture.strict"
    run "$fixture-strict" "$OUT/$fixture.strict"
    run "$fixture-jit" "$CLI" run "$source" test/support/time_expect.c
    run "$fixture-aot-build" "$CLI" build "$source" test/support/time_expect.c -o "$OUT/$fixture.aot"
    run "$fixture-aot" "$OUT/$fixture.aot"
done
sha256sum --quiet -c "$OUT/inputs.sha256"
printf '{"Complete":true,"Checks":%d,"InputsUnchanged":true,"Scope":"Time text/protocol/field text subset and core/calendar regression. Native adapter and eight actual generated-program XRT/resident/C closures ASan/UBSan/leaks. Shared libxl CLI/JIT uninstrumented; their allocation sweep is a logical ledger. Signal/process/containers and cross-group 53-module acceptance remain outside this gate."}\n' "$checks" >"$OUT/results.json"
printf 'Time text Linux checks: %d\n' "$checks"
