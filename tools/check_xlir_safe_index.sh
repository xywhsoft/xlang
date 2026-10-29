#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
OUT="${1:?fresh output directory required}"
[[ "$OUT" = /* ]] || OUT="$ROOT/$OUT"
[[ ! -e "$OUT" ]] || { echo 'Preserve safe index evidence' >&2; exit 1; }
mkdir -p "$OUT"
CC="$(command -v gcc)"; CLI="$ROOT/release/xl"; checks=0
run() { local name="$1"; shift; "$@" >"$OUT/$name.log" 2>&1; checks=$((checks+1)); }
cases=(xlir_safe_index_gate xlir_safe_index_alias xlir_safe_index_managed xlir_safe_index_sequences xlir_safe_index_async nullable_safe_navigation)
regressions=(index_ownership_dynamic_array_read index_ownership_dynamic_dict_read xlir_nullable_getter xlir_bytes_dynamic_cast)
{
    find src lib release/include -type f -print0
    printf '%s\0' "$CC" release/xl release/libxl.so release/libxlc.so build_linux.sh
    printf '%s\0' tools/check_xlir_safe_index.sh tools/check_xlir_safe_index.ps1 tools/tcc_vfs_manifest_linux_xlc.txt
    printf '%s\0' test/cases/xlir_safe_index_contract.c test/cases/xlir_artifact_runtime_contract.c
    printf '%s\0' test/cases/xlir_process_oom_host.c test/cases/xlir_time_text_closure_host.c test/cases/xlir_process_child.c test/cases/sem_nullable_getter_private.xl
    for fixture in "${cases[@]}" "${regressions[@]}"; do printf '%s\0' "test/cases/$fixture.xl"; done
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
sanitize=(-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
run child-build "$CC" -std=c11 -D_GNU_SOURCE -Wall -Wextra -Werror test/cases/xlir_process_child.c -o "$OUT/child"
export XL6_PROCESS_CHILD="$OUT/child"
sha256sum "$OUT/child" >>"$OUT/inputs.sha256"
run contract-build "$CC" "${strict[@]}" -Ilib/tcc test/cases/xlir_safe_index_contract.c "${libs[@]}" -lxlc -lxl -o "$OUT/contract"
run artifact-contract-build "$CC" "${strict[@]}" -Ilib/tcc test/cases/xlir_artifact_runtime_contract.c "${libs[@]}" -lxlc -lxl -o "$OUT/artifact-contract"
for fixture in "${cases[@]}"; do
    source="test/cases/$fixture.xl"
    run "$fixture-emit" "$CLI" cgen "$source" --emit "$OUT/$fixture.c"
    run "$fixture-repeat" "$CLI" cgen "$source" --emit "$OUT/$fixture.repeat.c"
    cmp "$OUT/$fixture.c" "$OUT/$fixture.repeat.c"
    run "$fixture-contract" "$OUT/contract" "$source" "$OUT/$fixture.closed.c"
    grep -Eq 'safe index closed contract: [1-9][0-9]* gates, [1-9][0-9]* branches, [1-9][0-9]* rejected mutations' "$OUT/$fixture-contract.log"
    cmp "$OUT/$fixture.c" "$OUT/$fixture.closed.c"
    entry=(); [[ "$fixture" = xlir_safe_index_alias || "$fixture" = xlir_safe_index_async ]] || entry=(-DXL6_TIME_USER_ENTRY)
    run "$fixture-strict-build" "$CC" "${strict[@]}" "${entry[@]}" -include "$OUT/$fixture.c" test/cases/xlir_time_text_closure_host.c "${libs[@]}" -lxl -o "$OUT/$fixture.strict"
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
    run "$fixture-closure-build" "$CC" "${strict[@]}" "${sanitize[@]}" "${entry[@]}" -include "$OUT/$fixture.c" test/cases/xlir_time_text_closure_host.c "$OUT/$fixture.native.o" "${resident_objects[@]}" -pthread -ldl -lm -o "$OUT/$fixture.closure"
    run "$fixture-closure" "$OUT/$fixture.closure"
    if [[ "$fixture" = xlir_safe_index_alias ]]; then
        # The memory-debug allocator is a test-host dependency, not a program
        # root. Sweep this fixture against the full shared debug runtime;
        # its actual pruned native closure was independently sanitized above.
        run alias-oom-build "$CC" "${strict[@]}" "${sanitize[@]}" -include "$OUT/$fixture.c" test/cases/xlir_process_oom_host.c "${libs[@]}" -lxl -o "$OUT/$fixture.oom"
        terminal=false
        for ((point=0; point<512; ++point)); do
            run "alias-oom-$point" "$OUT/$fixture.oom" "$point"
            if grep -q 'hit=0 balanced' "$OUT/alias-oom-$point.log"; then terminal=true; break; fi
        done
        "$terminal" || { echo 'Safe index alias OOM prefix incomplete' >&2; exit 1; }
    fi
done
for fixture in "${regressions[@]}"; do run "$fixture-regression" "$CLI" run "test/cases/$fixture.xl"; done
if "$CLI" cgen test/cases/sem_nullable_getter_private.xl --emit "$OUT/private.c" >"$OUT/private-rejection.log" 2>&1; then exit 1; fi
[[ ! -e "$OUT/private.c" ]]; grep -Eq 'safe member was not found|private|accessible' "$OUT/private-rejection.log"; checks=$((checks+1))
sha256sum --quiet -c "$OUT/inputs.sha256"
printf '{"Complete":true,"Checks":%d,"InputsUnchanged":true,"Scope":"Builtin safe-index receiver gates for nullable array/list/dict/string/bytes and dynamic var; lazy key, exact NUL/UTF-8, managed results, alias mutation, throwing/awaiting key, bounds absence and flow refinement. Closed descriptor/branch mutations, strict C/source and formal JIT/AOT; complete logical OOM prefix for module-init alias fixture. User protocol/indexed-property gate expansion is not claimed. Not total 53-module acceptance. Six actual XRT/resident/generated closures separately ASan/UBSan/leaks. Linux logical OOM links full shared memory-debug XRT (uninstrumented shared library); CLI/JIT uninstrumented."}\n' "$checks" >"$OUT/results.json"
printf 'Safe index Linux checks: %d\n' "$checks"
