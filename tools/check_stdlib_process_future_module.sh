#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
OUT="${1:?fresh output directory required}"
VARIANT="${2:-scoped}"
[[ "$VARIANT" = scoped || "$VARIANT" = globals ]] || { echo 'Choose scoped or globals' >&2; exit 1; }
FIXTURE=test/cases/xlir_process_future_module_lifetime.xl
[[ "$VARIANT" = scoped ]] || FIXTURE=test/cases/xlir_process_future_module_globals.xl
[[ "$OUT" = /* ]] || OUT="$ROOT/$OUT"
[[ ! -e "$OUT" ]] || { echo 'Preserve prior Process Future module evidence' >&2; exit 1; }
mkdir -p "$OUT"
CC="$(command -v gcc)"; checks=0
run() { local name="$1"; shift; "$@" >"$OUT/$name.log" 2>&1; checks=$((checks+1)); }
{
    find src lib release/include -type f -print0
    printf '%s\0' "$CC" release/libxl.so release/libxlc.so
    printf '%s\0' tools/check_stdlib_process_future_module.sh tools/check_stdlib_process_future_module.ps1
    printf '%s\0' tools/tcc_vfs_manifest_linux_xlc.txt tools/xrt_single_header_impl.c
    printf '%s\0' test/cases/xlir_process_future_module_lifetime.xl test/cases/xlir_process_future_module_host.c
    printf '%s\0' "$FIXTURE"
    printf '%s\0' test/cases/xlir_process_future_module_artifact.c test/cases/xlir_process_child.c
    printf '%s\0' test/cases/xlir_code_owner_binding_contract.c test/cases/xlir_owner_consumption_contract.c
    printf '%s\0' test/support/process_oom_observer.c test/support/process_module_tcc_provider.h
} | sort -zu | xargs -0 sha256sum >"$OUT/inputs.sha256"
python3 tools/stage_runtime_headers.py --root "$ROOT" --out "$OUT"
strict=(-std=c11 -D_GNU_SOURCE -Wall -Wextra -Werror -DXL_STATIC_EMBED
    -Wno-unused-function -Wno-unused-variable -Wno-unused-parameter -Wno-unused-label
    -Wno-pointer-sign -Wno-format-truncation -Wno-missing-field-initializers -Wno-unused-const-variable
    -I. -Irelease -Ilib/tcc -I"$OUT/include")
libs=(-Lrelease -Wl,-rpath,"$ROOT/release" -pthread -ldl -lm)
sanitize=(-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
run emitter-build "$CC" "${strict[@]}" test/cases/xlir_code_owner_binding_contract.c "${libs[@]}" -lxlc -lxl -o "$OUT/emitter"
run native-ir-contract "$OUT/emitter" --native-handles
grep -Fq 'Native resource policy: 3 typed fields, 3 lowered declarations, 29 rejected mutations before C/header output' "$OUT/native-ir-contract.log"
for pass in generated repeat; do
    run "$pass-emit" "$OUT/emitter" "$FIXTURE" \
        "$OUT/$pass.c" "$OUT/$pass.h" "$OUT/$pass.json" --script
done
for ext in c h json; do cmp "$OUT/generated.$ext" "$OUT/repeat.$ext"; done
# Derive only the test provider's canonical entries from verified XLIR. Never
# infer a production ABI from source spelling or inject an extra Cgen path.
python3 -c 'import json,sys; p=json.load(open(sys.argv[1])); nodes={n["id"]:n for n in p["nodes"]}; m=[m for m in p["asyncMachines"] if nodes[p["functions"][m["function"]]["declaration"]]["parent"]==1]; f=[f for f in p["functions"] if f["kindName"]=="user" and f["parameterCount"]==0]; assert len(m)==1 and len(p["asyncMachines"])==2; assert len(f)==(sys.argv[2]=="scoped"); print("#define PROCESS_RUN xlir_async_start_%d"%m[0]["targetSymbol"]); print("#define PROCESS_CREATE xlir_fn_%d"%f[0]["symbol"] if f else "#define PROCESS_GLOBAL_NATIVE 1"); policies=[t for t in p["callTargets"] if t["nativeHandlePolicyAbi"]==1 and t["reachability"]!="unreachable"]; assert len(policies)==len(set(t["nativeHandlePolicyCName"] for t in policies))==3; c=open(sys.argv[3]).read(); assert all("xl6NativeHandleTakeV1(handle, %s())"%t["nativeHandlePolicyCName"] in c and "xlir_native_handle_drop_s%d("%t["symbol"] not in c for t in policies)' \
    "$OUT/generated.json" "$VARIANT" "$OUT/generated.c" >"$OUT/binding.h"
run child-build "$CC" -std=c11 -D_GNU_SOURCE -Wall -Wextra -Werror test/cases/xlir_process_child.c -o "$OUT/child"
export XL6_PROCESS_CHILD="$OUT/child"
run observer-build "$CC" "${strict[@]}" -O0 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
    -include tools/xrt_single_header_impl.c -c test/support/process_oom_observer.c -o "$OUT/observer.o"
run host-build "$CC" "${strict[@]}" "${sanitize[@]}" -fwhole-program \
    "-DPROCESS_DLL_PATH=\"$OUT/generated.so\"" -c test/cases/xlir_process_future_module_host.c -o "$OUT/host.o"
resident=()
for source in src/runtime/*.c src/api/xl_task_group_bridge.c; do
    object="$OUT/$(basename "${source%.c}").o"
    run "$(basename "${source%.c}")-build" "$CC" "${strict[@]}" "${sanitize[@]}" -c "$source" -o "$object"
    resident+=("$object")
done
run host-link "$CC" "${sanitize[@]}" -no-pie -rdynamic "$OUT/host.o" "$OUT/observer.o" "${resident[@]}" \
    "${libs[@]}" -lxl -Wl,--wrap=xrtProcessSpawn -o "$OUT/host"
run artifact-build "$CC" "${strict[@]}" "${sanitize[@]}" -fPIC -shared -DXL6_BUILD_SHARED \
    "-DPROCESS_GENERATED_C=\"$OUT/generated.c\"" "-DPROCESS_GENERATED_H=\"$OUT/generated.h\"" \
    "-DPROCESS_BINDING_H=\"$OUT/binding.h\"" test/cases/xlir_process_future_module_artifact.c -o "$OUT/generated.so"
for mode in 0 1; do
    run "elf-mode-$mode" "$OUT/host" "$mode"
    grep -Fq "Process Future provider ELF/dlclose mode=$mode: 25 real unloads; pending/cancel isolation, independent statuses, escaped class pins, finally twice, native waiter and heap balanced" "$OUT/elf-mode-$mode.log"
    grep -Eq 'Resident native handle: 3 pre-trace refusals, invalid contract keeps slot, [1-9][0-9]* OOM prefix failures \+ first no-hit, exact ledger' "$OUT/elf-mode-$mode.log"
done
sha256sum --quiet -c "$OUT/inputs.sha256"
printf '{"Complete":true,"Checks":%d,"InputsUnchanged":true,"Variant":"%s","Scope":"Linux actual generated ELF provider and current source API/XRT/resident runtime, ASan/UBSan/leaks; native policy admission/OOM and escaped raw status handle after real unmapping"}\n' "$checks" "$VARIANT" >"$OUT/results.json"
printf 'Process Future actual ELF retirement checks: %d\n' "$checks"
