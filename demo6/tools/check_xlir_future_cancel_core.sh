#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
OUT="${1:?fresh output directory required}"
[[ "$OUT" = /* ]] || OUT="$ROOT/$OUT"
[[ ! -e "$OUT" ]] || { echo 'Preserve prior Future cancellation evidence' >&2; exit 1; }
mkdir -p "$OUT/include/__xl_runtime"
CC="$(command -v gcc)"; checks=0; complete=false
resident=(src/runtime/xl6_code_lease.c src/runtime/xl6_code_finalizer.c src/runtime/xl6_continuation_queue.c
    src/runtime/xl6_continuation_executor.c src/runtime/xl6_continuation_future.c src/runtime/xl6_continuation_frame.c
    src/runtime/xl6_future_wait.c src/runtime/xl6_closure.c src/runtime/xl6_cell.c src/runtime/xl6_slot.c
    src/runtime/xl6_frame_storage.c src/runtime/xl6_task.c src/runtime/xl6_capture.c src/runtime/xl6_value_iterator.c
    src/runtime/xl6_future_callback.c src/runtime/xl6_generator.c src/runtime/xl6_future_payload.c)
{
    find src lib release/include -type f -print0
    printf '%s\0' "$CC" release/libxl.so release/libxlc.so tools/xrt_single_header_impl.c
    printf '%s\0' tools/check_xlir_future_cancel_core.sh tools/check_xlir_future_cancel_core.ps1
    printf '%s\0' tools/tcc_vfs_manifest_linux_xlc.txt build/linux/xrt.o build/linux/tcc
    printf '%s\0' test/cases/xlir_future_producer_cancel.c test/cases/xlir_continuation_publication_contract.c
    printf '%s\0' test/cases/xlir_async_cancel_watch_contract.c test/cases/xlir_async_cancel_watch.xl
    printf '%s\0' test/cases/xlir_continuation_owner_test.h test/cases/xlir_async_cancel_wait_runtime.c test/cases/xlir_async_cancel_wait_host.c
} | sort -zu | xargs -0 sha256sum > "$OUT/inputs.sha256"
finish() {
    local status=$? unchanged=false
    if sha256sum --quiet -c "$OUT/inputs.sha256"; then unchanged=true; else complete=false; fi
    python3 -c 'import json,sys; json.dump(dict(Complete=sys.argv[2]=="true",InputsUnchanged=sys.argv[3]=="true",Checks=int(sys.argv[4]),Scope="Future producer/native publication and canonical generated cancellation; GCC O0/O2/TCC plus full XRT/resident/generated kernel ASan/UBSan/leaks. Compiler emitter and shared CLI/JIT not instrumented. Full kernel closure, not per-program pruned closure or Process/53-module acceptance."),open(sys.argv[1],"w"),indent=2)' "$OUT/results.json" "$complete" "$unchanged" "$checks"
    [[ "$unchanged" = true ]] || status=1
    exit "$status"
}
trap finish EXIT
run() { local name="$1"; shift; "$@" > "$OUT/$name.log" 2>&1; checks=$((checks+1)); }
strict=(-std=c11 -Wall -Wextra -Werror -Wno-unused-function -Wno-unused-variable -Wno-unused-parameter
    -Wno-unused-label -Wno-unused-but-set-variable -Wno-missing-field-initializers -Wno-pointer-sign
    -I. "-I$OUT" "-I$OUT/include")
libs=(-pthread -ldl -lm)
while IFS='|' read -r kind target source; do
    if [[ "$kind" = file && "$target" = include/__xl_runtime/* ]]; then
        source="${source//%XL6_ROOT%/$ROOT}"
        mkdir -p "$OUT/$(dirname "$target")"; cp "$source" "$OUT/$target"; cmp "$source" "$OUT/$target"
    fi
done < tools/tcc_vfs_manifest_linux_xlc.txt
run contract-build "$CC" "${strict[@]}" -Ilib/tcc test/cases/xlir_async_cancel_watch_contract.c \
    -Lrelease -lxlc -lxl -Wl,-rpath,"$ROOT/release" "${libs[@]}" -o "$OUT/contract"
for repeat in '' '.repeat'; do
    run "emit$repeat" "$OUT/contract" "$OUT/xlir_async_cancel_watch$repeat.c" "$OUT/xlir_async_cancel_watch$repeat.h" "$OUT/xlir_async_cancel_watch$repeat.json"
    grep -F '15 C/header-gated mutations' "$OUT/emit$repeat.log" >/dev/null
done
for suffix in c h json h.bind.h; do cmp "$OUT/xlir_async_cancel_watch.$suffix" "$OUT/xlir_async_cancel_watch.repeat.$suffix"; done
for opt in O0 O2; do
    run "producer-$opt-build" "$CC" "${strict[@]}" "-$opt" test/cases/xlir_future_producer_cancel.c "${libs[@]}" -o "$OUT/producer-$opt"
    run "producer-$opt-run" "$OUT/producer-$opt"
    grep -F '300 terminal-publication races, balanced' "$OUT/producer-$opt-run.log" >/dev/null
    run "publication-$opt-build" "$CC" "${strict[@]}" "-$opt" test/cases/xlir_continuation_publication_contract.c src/runtime/xl6_code_lease.c src/runtime/xl6_continuation_queue.c build/linux/xrt.o "${libs[@]}" -o "$OUT/publication-$opt"
    run "publication-$opt-run" "$OUT/publication-$opt"
    grep -F '400 two-thread commit/source/close handshakes' "$OUT/publication-$opt-run.log" >/dev/null
    run "generated-$opt-build" "$CC" "${strict[@]}" "-$opt" test/cases/xlir_async_cancel_wait_runtime.c test/cases/xlir_async_cancel_wait_host.c "${resident[@]}" build/linux/xrt.o "${libs[@]}" -o "$OUT/generated-$opt"
    run "generated-$opt-run" "$OUT/generated-$opt"
    grep -F 'Async cancel wait: 2106 generated interruptions, 300 three-thread' "$OUT/generated-$opt-run.log" >/dev/null
    grep -F 'Explicit Future cancellation: 150 public requests, 50 active-predecessor/provisional-successor handshakes, 200 native reentries' "$OUT/generated-$opt-run.log" >/dev/null
done
run generated-tcc-build build/linux/tcc -std=c11 -I. "-I$OUT" "-I$OUT/include" \
    test/cases/xlir_async_cancel_wait_runtime.c test/cases/xlir_async_cancel_wait_host.c "${resident[@]}" \
    tools/xrt_single_header_impl.c "${libs[@]}" -o "$OUT/generated-tcc"
run generated-tcc-run "$OUT/generated-tcc"
grep -F 'Async cancel wait: 2106 generated interruptions, 300 three-thread' "$OUT/generated-tcc-run.log" >/dev/null
grep -F 'Explicit Future cancellation: 150 public requests, 50 active-predecessor/provisional-successor handshakes, 200 native reentries' "$OUT/generated-tcc-run.log" >/dev/null
san=(-O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined)
run producer-san-build "$CC" "${strict[@]}" "${san[@]}" test/cases/xlir_future_producer_cancel.c "${libs[@]}" -o "$OUT/producer-san"
run producer-san-run env ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 "$OUT/producer-san"
run xrt-san-build "$CC" "${strict[@]}" "${san[@]}" -c tools/xrt_single_header_impl.c -o "$OUT/xrt-san.o"
run publication-san-build "$CC" "${strict[@]}" "${san[@]}" test/cases/xlir_continuation_publication_contract.c src/runtime/xl6_code_lease.c src/runtime/xl6_continuation_queue.c "$OUT/xrt-san.o" "${libs[@]}" -o "$OUT/publication-san"
run publication-san-run env ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 "$OUT/publication-san"
run generated-san-build "$CC" "${strict[@]}" "${san[@]}" test/cases/xlir_async_cancel_wait_runtime.c test/cases/xlir_async_cancel_wait_host.c "${resident[@]}" "$OUT/xrt-san.o" "${libs[@]}" -o "$OUT/generated-san"
run generated-san-run env ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 "$OUT/generated-san"
grep -F 'Explicit Future cancellation: 150 public requests, 50 active-predecessor/provisional-successor handshakes, 200 native reentries' "$OUT/generated-san-run.log" >/dev/null
grep -F 'Async cancel wait: 2106 generated interruptions, 300 three-thread' "$OUT/generated-san-run.log" >/dev/null
complete=true
printf 'Future cancellation Linux checks: %s\n' "$checks"
