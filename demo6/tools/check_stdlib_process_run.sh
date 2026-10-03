#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
OUT="${1:?fresh output directory required}"
MODE="${2:---all}"
[[ "$MODE" = --all || "$MODE" = --future-only ]] || { echo 'Expected --all or --future-only' >&2; exit 2; }
[[ "$OUT" = /* ]] || OUT="$ROOT/$OUT"
[[ ! -e "$OUT" ]] || { echo 'Preserve prior process_run evidence' >&2; exit 1; }
mkdir -p "$OUT"
CC="$(command -v gcc)"; CLI="$ROOT/release/xl"; checks=0
run() { local name="$1"; shift; "$@" >"$OUT/$name.log" 2>&1; checks=$((checks+1)); }
cases=(stdlib_process_command_only stdlib_process_command_config stdlib_process_options_only stdlib_process_run_bytes stdlib_process_run_limits stdlib_process_run_allocations stdlib_process_shell_exact xlir_process_exec_bridge xlir_process_success_bridge xlir_process_exit_code_bridge)
cases+=(stdlib_process_support stdlib_process_terminal_only stdlib_process_open_only)
cases+=(stdlib_process_live_wait stdlib_process_live_streams stdlib_process_live_drop stdlib_process_live_allocations)
cases+=(stdlib_process_future stdlib_process_future_drop stdlib_process_future_cancel)
cases+=(stdlib_process_future_allocations stdlib_process_future_drop_allocations stdlib_process_future_cancel_allocations)
cases+=(stdlib_process_pipeline_config stdlib_process_pipeline_bytes stdlib_process_pipeline_limits stdlib_process_pipeline_allocations)
cases+=(stdlib_process_file_config stdlib_process_file_bytes stdlib_process_file_allocations)
cases+=(stdlib_process_terminal_config stdlib_process_terminal_session stdlib_process_terminal_allocations)
negative=(xlir_process_bad_array xlir_process_bad_effect xlir_process_bad_width xlir_process_bad_owner)
negative+=(xlir_process_open_bad_width xlir_process_open_bad_effect xlir_process_terminal_bad_pure)
negative+=(xlir_process_live_bad_owner xlir_process_live_bad_width xlir_process_live_bad_effect xlir_process_live_bad_stream)
negative+=(xlir_process_future_bad_element xlir_process_future_bad_nullable xlir_process_future_bad_effect xlir_process_future_bad_owner)
negative+=(xlir_process_pipeline_bad_owner xlir_process_pipeline_bad_width xlir_process_pipeline_bad_effect xlir_process_pipeline_bad_input)
negative+=(xlir_process_file_bad_owner xlir_process_file_bad_width xlir_process_file_bad_effect xlir_process_file_bad_input)
negative+=(xlir_process_resize_bad_owner xlir_process_resize_bad_width xlir_process_resize_bad_effect)
if [[ "$MODE" = --future-only ]]; then
    cases=(stdlib_process_future stdlib_process_future_drop stdlib_process_future_cancel stdlib_process_future_allocations stdlib_process_future_drop_allocations stdlib_process_future_cancel_allocations)
    negative=(xlir_process_future_bad_element xlir_process_future_bad_nullable xlir_process_future_bad_effect xlir_process_future_bad_owner)
fi
{
    find src lib release/include -type f -print0
    printf '%s\0' "$CC" release/xl release/libxl.so release/libxlc.so build_linux.sh
    printf '%s\0' tools/check_stdlib_process_run.sh tools/check_stdlib_process_run.ps1 tools/tcc_vfs_manifest_linux_xlc.txt
    printf '%s\0' test/cases/xlir_process_run_host.c test/support/process_native.c test/cases/xlir_text_value_contract.c
    printf '%s\0' test/cases/xlir_process_support_host.c
    printf '%s\0' test/cases/xlir_process_live_host.c
    printf '%s\0' test/cases/xlir_process_future_host.c
    printf '%s\0' test/cases/xlir_process_future_oom_host.c
    printf '%s\0' test/cases/xlir_process_pipeline_prefix.c
    printf '%s\0' test/cases/xlir_process_pipeline_host.c
    printf '%s\0' test/cases/xlir_process_file_host.c
    printf '%s\0' test/cases/xlir_process_terminal_host.c
    printf '%s\0' test/cases/xlir_process_live_oom_host.c test/support/process_oom_observer.c
    printf '%s\0' test/cases/xlir_process_io_loan.c test/support/process_io_loan_probe.h
    printf '%s\0' test/cases/xlir_process_oom_host.c test/cases/xlir_artifact_runtime_contract.c
    printf '%s\0' test/cases/xlir_time_text_closure_host.c test/cases/xlir_process_child.c
    printf '%s\0' release/examples/stdlib_process_run.xl
    printf '%s\0' release/examples/stdlib_process_support.xl
    printf '%s\0' release/examples/stdlib_process_pipeline.xl
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
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
sanitize=(-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie)
future_resident=(src/runtime/xl6_code_lease.c src/runtime/xl6_code_finalizer.c src/runtime/xl6_continuation_queue.c
    src/runtime/xl6_continuation_executor.c src/runtime/xl6_continuation_future.c src/runtime/xl6_continuation_frame.c
    src/runtime/xl6_future_wait.c src/runtime/xl6_closure.c src/runtime/xl6_cell.c src/runtime/xl6_slot.c
    src/runtime/xl6_frame_storage.c src/runtime/xl6_task.c src/runtime/xl6_capture.c src/runtime/xl6_value_iterator.c
    src/runtime/xl6_future_callback.c src/runtime/xl6_generator.c src/runtime/xl6_future_payload.c)
run child-build "$CC" -std=c11 -D_GNU_SOURCE -Wall -Wextra -Werror test/cases/xlir_process_child.c -o "$OUT/child"
export XL6_PROCESS_CHILD="$OUT/child"
sha256sum "$OUT/child" >>"$OUT/inputs.sha256"
if [[ "$MODE" = --all ]]; then
run terminal-native-build "$CC" "${strict[@]}" "${sanitize[@]}" test/cases/xlir_process_terminal_host.c -pthread -ldl -lm -o "$OUT/terminal-native"
run terminal-native "$OUT/terminal-native" "$OUT/child"
grep -q 'terminal: real tty/merged stderr, child-observed resize, independent stream leases; 13 guards, heap/owned handles balanced; raw Win32 platform retention=0, no extra handles' "$OUT/terminal-native.log"
run file-native-build "$CC" "${strict[@]}" "${sanitize[@]}" test/cases/xlir_process_file_host.c -pthread -ldl -lm -o "$OUT/file-native"
run file-native "$OUT/file-native" "$OUT/child"
grep -Eq 'process file: independent non-inheritable handles, replacement/rollback, original close, binary child, 5 guards; OS error prefix [0-9]+ failures \+ 1 first no-hit, heap/handles balanced' "$OUT/file-native.log"
run pipeline-prefix-build "$CC" "${strict[@]}" "${sanitize[@]}" -DXL6_PIPELINE_PREFIX_FROM_XLANG=1 test/cases/xlir_process_pipeline_prefix.c -pthread -ldl -lm -o "$OUT/pipeline-prefix"
run pipeline-prefix "$OUT/pipeline-prefix"
grep -Eq 'pipeline native prefixes: [0-9]+/[0-9]+ failures \+ 2 first no-hit, unrelated stdin intact, exact binary per-stage stderr, private waiters retired, balanced' "$OUT/pipeline-prefix.log"
run pipeline-prefix-full-build "$CC" "${strict[@]}" "${sanitize[@]}" -DXRT_MODULE_ALL=1 -DXL6_PIPELINE_PREFIX_FROM_XLANG=1 test/cases/xlir_process_pipeline_prefix.c -pthread -ldl -lm -o "$OUT/pipeline-prefix-full"
run pipeline-prefix-full "$OUT/pipeline-prefix-full"
grep -Eq 'pipeline native prefixes: [0-9]+/[0-9]+ failures \+ 2 first no-hit, unrelated stdin intact, exact binary per-stage stderr, private waiters retired, balanced' "$OUT/pipeline-prefix-full.log"
run live-native-build "$CC" "${strict[@]}" "${sanitize[@]}" test/cases/xlir_process_live_host.c -pthread -ldl -lm -o "$OUT/live-native"
run live-native "$OUT/live-native" "$OUT/child"
grep -q 'exact binary streams survive parent drop; unique leases, timeout/range/state, independent status; reader/writer OOM 3/3 failures + 2 first no-hit, balanced' "$OUT/live-native.log"
run io-loan-build "$CC" "${strict[@]}" "${sanitize[@]}" test/cases/xlir_process_io_loan.c -pthread -ldl -lm -o "$OUT/io-loan"
run io-loan "$OUT/io-loan"
grep -q 'process IO loan: 3 deterministic close races, 8 failure guards, short/EOF/zero/peer guards, balanced; safe=1' "$OUT/io-loan.log"
run io-loan-terminal-build "$CC" "${strict[@]}" "${sanitize[@]}" -DXRT_MODULE_PROCESS_TERMINAL test/cases/xlir_process_io_loan.c -pthread -ldl -lm -o "$OUT/io-loan-terminal"
run io-loan-terminal "$OUT/io-loan-terminal"
run native-build "$CC" "${strict[@]}" "${sanitize[@]}" test/cases/xlir_process_run_host.c test/support/process_native.c src/runtime/xl6_process_command.c src/runtime/xl6_process_terminal_config.c src/runtime/xl6_process_options.c src/runtime/xl6_process_run.c src/runtime/xl6_process_status.c -pthread -ldl -lm -o "$OUT/native"
run native "$OUT/native" "$OUT/child"
grep -q '3 first no-hit completions, balanced' "$OUT/native.log"
grep -q 'concurrent cancellation and absolute deadline' "$OUT/native.log"
grep -Eq 'native run complete prefix: [0-9]+ failures and 1 first no-hit, balanced' "$OUT/native.log"
run pipeline-resident-build "$CC" "${strict[@]}" "${sanitize[@]}" -DXRT_MODULE_PROCESS_PIPELINE=1 test/cases/xlir_process_pipeline_host.c test/support/process_native.c src/runtime/xl6_process_command.c src/runtime/xl6_process_terminal_config.c src/runtime/xl6_process_options.c src/runtime/xl6_process_status.c src/runtime/xl6_process_pipeline_config.c src/runtime/xl6_process_pipeline.c -pthread -ldl -lm -o "$OUT/pipeline-resident"
run pipeline-resident "$OUT/pipeline-resident" "$OUT/child"
grep -Eq 'pipeline resident prefixes: [0-9]+/[0-9]+/[0-9]+/[0-9]+/[0-9]+ failures \+ 5 first no-hit; snapshots, 5 owner guards, real collectors balanced' "$OUT/pipeline-resident.log"
grep -Eq 'pipeline projections: [0-9]+/[0-9]+/[0-9]+/[0-9]+/[0-9]+/[0-9]+ failures \+ 6 first no-hit; independent binary/status, cancellation/deadline, balanced' "$OUT/pipeline-resident.log"
grep -q 'POSIX launch plan: 13 allocation failures and 2 completions, balanced' "$OUT/native.log"
run support-native-build "$CC" "${strict[@]}" "${sanitize[@]}" test/cases/xlir_process_support_host.c -pthread -ldl -lm -o "$OUT/support-native"
run support-native "$OUT/support-native"
grep -Eq '7 rejected inputs, exact prefix/clamp, native error transport; [0-9]+ allocation failures and 1 first no-hit, balanced; real terminal probe=' "$OUT/support-native.log"
grep -q 'process open error prefix: 3 failures and 3 first no-hit, causes preserved, balanced' "$OUT/support-native.log"
fi
run future-native-build "$CC" "${strict[@]}" "${sanitize[@]}" test/cases/xlir_process_future_host.c "${future_resident[@]}" -pthread -ldl -lm -o "$OUT/future-native"
run future-native "$OUT/future-native" "$OUT/child"
grep -Eq 'mapper/pending/terminal OOM 1/[0-9]+/[0-9]+ failures \+ 3 first no-hit, waiter retired, balanced' "$OUT/future-native.log"
run contract-build "$CC" "${strict[@]}" test/cases/xlir_text_value_contract.c "${libs[@]}" -lxlc -lxl -o "$OUT/contract"
run artifact-contract-build "$CC" "${strict[@]}" test/cases/xlir_artifact_runtime_contract.c "${libs[@]}" -lxlc -lxl -o "$OUT/artifact-contract"
for fixture in "${cases[@]}"; do
    source="test/cases/$fixture.xl"
    run "$fixture-emit" "$CLI" cgen "$source" --emit "$OUT/$fixture.c"
    run "$fixture-repeat" "$CLI" cgen "$source" --emit "$OUT/$fixture.repeat.c"
    cmp "$OUT/$fixture.c" "$OUT/$fixture.repeat.c"
    run "$fixture-contract" "$OUT/contract" "$source" "$OUT/$fixture.closed.c"
    cmp "$OUT/$fixture.c" "$OUT/$fixture.closed.c"
    ! grep -Eq 'xl6_xrt_process_|xrtProcess[A-Za-z]+\(|xl6_process_run_text' "$OUT/$fixture.c"
    strict_host=test/cases/xlir_process_oom_host.c
    if [[ "$fixture" = stdlib_process_live_* || "$fixture" = stdlib_process_future* || "$fixture" = stdlib_process_file* ]]; then strict_host=test/cases/xlir_time_text_closure_host.c; fi
    if [[ "$fixture" = stdlib_process_file_allocations ]]; then strict_host=test/cases/xlir_process_oom_host.c; fi
    run "$fixture-strict-build" "$CC" "${strict[@]}" -include "$OUT/$fixture.c" "$strict_host" "${libs[@]}" -lxl -o "$OUT/$fixture.strict"
    run "$fixture-strict" "$OUT/$fixture.strict"
    run "$fixture-jit" "$CLI" run "$source"
    run "$fixture-aot-build" "$CLI" build "$source" -o "$OUT/$fixture.aot"
    run "$fixture-aot" "$OUT/$fixture.aot"
    run "$fixture-native-emit" "$OUT/artifact-contract" --emit-native "$source" "$OUT/$fixture.native.c"
    if [[ "$fixture" = stdlib_process_command_only || "$fixture" = stdlib_process_options_only || "$fixture" = stdlib_process_open_only || "$fixture" = stdlib_process_terminal_only ]]; then
        leaf="${fixture#stdlib_process_}"; leaf="${leaf%_only}"
        python3 -c 'import json,sys;p=json.loads(open(sys.argv[1]).readline());selected=[s for s in p["Sources"] if "/xl6_process_" in s];assert selected==["src/runtime/xl6_process_"+sys.argv[2]+".c"],selected' "$OUT/$fixture-native-emit.log" "$leaf"
    fi
    if [[ "$fixture" = stdlib_process_pipeline_* ]]; then
        python3 -c 'import json,sys
p=json.loads(open(sys.argv[1]).readline());s=[x for x in p["Sources"] if "/xl6_process_" in x];m=p["NativeModules"]
assert "src/runtime/xl6_process_pipeline_config.c" in s,s
assert not any("/xl6_process_"+x+".c" in " ".join(s) for x in ("live","future","stream","open","terminal","run")),s
if sys.argv[2]=="stdlib_process_pipeline_config":
 assert "src/runtime/xl6_process_pipeline.c" not in s and "XRT_MODULE_PROCESS_PIPELINE" not in m,(s,m)
else:
 assert "src/runtime/xl6_process_pipeline.c" in s and "XRT_MODULE_PROCESS_PIPELINE" in m,(s,m)' "$OUT/$fixture-native-emit.log" "$fixture"
    fi
    if [[ "$fixture" = stdlib_process_terminal_config || "$fixture" = stdlib_process_terminal_session || "$fixture" = stdlib_process_terminal_allocations ]]; then
        python3 -c 'import json,sys
p=json.loads(open(sys.argv[1]).readline());case=sys.argv[2]
parts=["command","terminal_config"]
if case!="stdlib_process_terminal_config":parts+=["live","status"]
if case=="stdlib_process_terminal_session":parts+=["terminal_live","stream"]
actual=sorted(s for s in p["Sources"] if "/xl6_process_" in s)
assert actual==sorted("src/runtime/xl6_process_"+s+".c" for s in parts),actual
modules=set(p["NativeModules"])
assert "XRT_MODULE_PROCESS_TERMINAL" in modules,modules
assert not modules.intersection("XRT_MODULE_PROCESS_"+s for s in ("RUN","PIPELINE","FILE","FUTURE","OPEN")),modules' "$OUT/$fixture-native-emit.log" "$fixture"
        checks=$((checks+1))
    fi
    if [[ "$fixture" = stdlib_process_live_* || "$fixture" = stdlib_process_future* ]]; then
        python3 -c 'import json,sys
p=json.loads(open(sys.argv[1]).readline());case=sys.argv[2];future=case.startswith("stdlib_process_future")
parts=["command","live"]
if case!="stdlib_process_live_drop":parts+=["status"]
if future:parts+=["future"]
if case not in ("stdlib_process_live_wait","stdlib_process_future","stdlib_process_future_allocations"):parts+=["stream"]
actual=sorted(s for s in p["Sources"] if "/xl6_process_" in s)
assert actual==sorted("src/runtime/xl6_process_"+s+".c" for s in parts),actual
modules=set(p["NativeModules"])
assert ("XRT_MODULE_PROCESS_FUTURE" in modules)==future,modules
assert ("XRT_MODULE_IO" in modules)==("stream" in parts),modules
assert not modules.intersection("XRT_MODULE_PROCESS_"+s for s in ("RUN","PIPELINE","FILE","TERMINAL","OPEN")),modules
if future:assert "src/runtime/xl6_future_payload.c" in p["Sources"]' "$OUT/$fixture-native-emit.log" "$fixture"
        checks=$((checks+1))
    fi
    run "$fixture-native-build" "$CC" "${strict[@]}" "${sanitize[@]}" -Ilib/xruntime -c "$OUT/$fixture.native.c" -o "$OUT/$fixture.native.o"
    mapfile -t resident_sources < <(python3 -c 'import json,sys;p=json.loads(open(sys.argv[1]).readline());print("\n".join(p["Sources"]))' "$OUT/$fixture-native-emit.log")
    resident_objects=()
    for ((resident=0; resident<${#resident_sources[@]}; ++resident)); do
        object="$OUT/$fixture.resident-$resident.o"
        run "$fixture-resident-$resident-build" "$CC" "${strict[@]}" "${sanitize[@]}" -c "${resident_sources[$resident]}" -o "$object"
        resident_objects+=("$object")
    done
    run "$fixture-closure-build" "$CC" "${strict[@]}" "${sanitize[@]}" -include "$OUT/$fixture.c" test/cases/xlir_time_text_closure_host.c "$OUT/$fixture.native.o" "${resident_objects[@]}" -pthread -ldl -lm -o "$OUT/$fixture.closure"
    run "$fixture-closure" "$OUT/$fixture.closure"
    if [[ "$fixture" = stdlib_process_pipeline_allocations || "$fixture" = stdlib_process_file_allocations ]]; then
        run "$fixture-debug-native-build" "$CC" "${strict[@]}" "${sanitize[@]}" -DXRT_MODULE_MEMORY_DEBUG -Ilib/xruntime -c "$OUT/$fixture.native.c" -o "$OUT/$fixture.debug-native.o"
        run "$fixture-pipeline-oom-build" "$CC" "${strict[@]}" "${sanitize[@]}" -include "$OUT/$fixture.c" test/cases/xlir_process_oom_host.c "$OUT/$fixture.debug-native.o" "${resident_objects[@]}" -pthread -ldl -lm -o "$OUT/$fixture.pipeline-oom"
        run "$fixture-pipeline-oom-baseline" "$OUT/$fixture.pipeline-oom"
        terminal=false
        for ((point=0; point<512; ++point)); do
            run "$fixture-pipeline-oom-$point" "$OUT/$fixture.pipeline-oom" "$point"
            if grep -q 'hit=0 balanced' "$OUT/$fixture-pipeline-oom-$point.log"; then terminal=true; break; fi
        done
        "$terminal" || { echo 'Generated pipeline actual-closure allocation prefix incomplete' >&2; exit 1; }
    fi
    if [[ "$fixture" = stdlib_process_live_wait || "$fixture" = stdlib_process_live_drop || "$fixture" = stdlib_process_live_allocations || "$fixture" = stdlib_process_terminal_allocations ]]; then
        # The native debug root is added only to the failure-injection witness.
        # Normal artifact pruning above is still checked without test roots.
        run "$fixture-observer-build" "$CC" "${strict[@]}" "${sanitize[@]}" -DXRT_MODULE_MEMORY_DEBUG -Ilib/xruntime -include "$OUT/$fixture.native.c" -c test/support/process_oom_observer.c -o "$OUT/$fixture.observer.o"
        run "$fixture-live-oom-build" "$CC" "${strict[@]}" "${sanitize[@]}" -include "$OUT/$fixture.c" test/cases/xlir_process_live_oom_host.c "$OUT/$fixture.observer.o" "${resident_objects[@]}" -Wl,--wrap=xrtProcessSpawn -pthread -ldl -lm -o "$OUT/$fixture.live-oom"
        run "$fixture-live-oom-baseline" "$OUT/$fixture.live-oom"
        terminal=false; children=1
        if [[ "$fixture" = stdlib_process_live_wait ]]; then children=2; fi
        for ((point=0; point<512; ++point)); do
            run "$fixture-live-oom-$point" "$OUT/$fixture.live-oom" "$point"
            if grep -q "hit=0 children=$children waiter retired balanced" "$OUT/$fixture-live-oom-$point.log"; then terminal=true; break; fi
        done
        "$terminal" || { echo 'Generated live Process current-thread allocation prefix incomplete' >&2; exit 1; }
    fi
    if [[ "$fixture" = stdlib_process_future || "$fixture" = stdlib_process_future_allocations || "$fixture" = stdlib_process_future_drop_allocations || "$fixture" = stdlib_process_future_cancel_allocations ]]; then
        run "$fixture-future-observer-build" "$CC" "${strict[@]}" "${sanitize[@]}" -DXRT_MODULE_MEMORY_DEBUG -Ilib/xruntime -include "$OUT/$fixture.native.c" -c test/support/process_oom_observer.c -o "$OUT/$fixture.future-observer.o"
        run "$fixture-future-oom-build" "$CC" "${strict[@]}" "${sanitize[@]}" -include "$OUT/$fixture.c" test/cases/xlir_process_future_oom_host.c "$OUT/$fixture.future-observer.o" "${resident_objects[@]}" -Wl,--wrap=xrtProcessSpawn -Wl,--wrap=xrtErrorBuild -pthread -ldl -lm -o "$OUT/$fixture.future-oom"
        run "$fixture-future-oom-baseline" "$OUT/$fixture.future-oom"
        terminal=false; children=1
        if [[ "$fixture" = stdlib_process_future ]]; then children=2; fi
        for ((point=0; point<512; ++point)); do
            run "$fixture-future-oom-$point" "$OUT/$fixture.future-oom" "$point"
            if grep -q 'outcome=cancelled-diagnostic' "$OUT/$fixture-future-oom-$point.log"; then
                [[ "$fixture" = stdlib_process_future_cancel_allocations ]] || { echo 'Unexpected cancelled diagnostic fallback' >&2; exit 1; }
            fi
            if grep -q "hit=0 children=$children waiter/executor retired balanced; outcome=success cancelled-diagnostic-failures=0" "$OUT/$fixture-future-oom-$point.log"; then terminal=true; break; fi
        done
        "$terminal" || { echo 'Generated Process Future current-thread allocation prefix incomplete' >&2; exit 1; }
    fi
    if [[ "$fixture" = stdlib_process_run_allocations ]]; then
        terminal=false
        for ((point=0; point<512; ++point)); do
            run "$fixture-oom-$point" "$OUT/$fixture.strict" "$point"
            if grep -q 'hit=0 balanced' "$OUT/$fixture-oom-$point.log"; then terminal=true; break; fi
        done
        "$terminal" || { echo 'Process run language allocation sweep incomplete' >&2; exit 1; }
    fi
done
for fixture in "${negative[@]}"; do
    if "$CLI" cgen "test/cases/$fixture.xl" --emit "$OUT/$fixture.c" >"$OUT/$fixture.log" 2>&1; then exit 1; fi
    [[ ! -e "$OUT/$fixture.c" ]]
    if [[ "$fixture" = xlir_process_file_bad_input ]]; then
        grep -q 'XLIR VFS resource parameter has no exact native field borrow' "$OUT/$fixture.log"
    else
        grep -Eq 'XLIR direct runtime bridge has no exact declared ABI|has no exact compiler-runtime bridge ABI' "$OUT/$fixture.log"
    fi
    checks=$((checks+1))
done
if [[ "$MODE" = --all ]]; then
run example-run "$CLI" run release/examples/stdlib_process_run.xl
run example-build "$CLI" build release/examples/stdlib_process_run.xl -o "$OUT/example"
run example-execute "$OUT/example"
run support-example-run "$CLI" run release/examples/stdlib_process_support.xl
run support-example-build "$CLI" build release/examples/stdlib_process_support.xl -o "$OUT/support-example"
run support-example-execute "$OUT/support-example"
run pipeline-example-run "$CLI" run release/examples/stdlib_process_pipeline.xl
run pipeline-example-build "$CLI" build release/examples/stdlib_process_pipeline.xl -o "$OUT/pipeline-example"
run pipeline-example-execute "$OUT/pipeline-example"
fi
sha256sum --quiet -c "$OUT/inputs.sha256"
printf '{"Complete":true,"Checks":%d,"InputsUnchanged":true,"Mode":"%s","Scope":"Default --all checks Process configuration/run/live/Future/pipeline/File/terminal; --future-only checks six Process Future fixtures and four Future ABI negatives only, plus the native mapper/pending/terminal factory prefixes. Both lanes strictly compile and execute actual generated XRT/resident/program closures with ASan/UBSan/leaks; shared CLI/JIT uninstrumented. Four Future current-thread logical allocation prefixes observe actual Spawn and explicitly join private waiter/executor tails, including native factories and managed boxing. Future failure propagation and exact cancelled diagnostic OOM fallback checked. No other-thread allocation injection. No global Windows zero-handle-growth claim. Generated Process Future provider unload and 53-module total acceptance pending."}\n' "$checks" "$MODE" >"$OUT/results.json"
printf 'Process run Linux checks: %d\n' "$checks"
