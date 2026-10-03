#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
OUT="${1:?fresh output directory required}"
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
    printf '%s\0' test/cases/xlir_process_future_module_artifact.c test/cases/xlir_process_child.c
    printf '%s\0' test/cases/xlir_code_owner_binding_contract.c test/cases/xlir_owner_consumption_contract.c
    printf '%s\0' test/support/process_oom_observer.c test/support/process_module_tcc_provider.h
} | sort -zu | xargs -0 sha256sum >"$OUT/inputs.sha256"
while IFS='|' read -r kind target source; do
    if [[ "$kind" = file && "$target" = include/__xl_runtime/* ]]; then
        source="${source//%XL6_ROOT%/$ROOT}"
        mkdir -p "$OUT/$(dirname "$target")"; cp "$source" "$OUT/$target"; cmp "$source" "$OUT/$target"
    fi
done <tools/tcc_vfs_manifest_linux_xlc.txt
strict=(-std=c11 -D_GNU_SOURCE -Wall -Wextra -Werror -DXL_STATIC_EMBED
    -Wno-unused-function -Wno-unused-variable -Wno-unused-parameter -Wno-unused-label
    -Wno-pointer-sign -Wno-format-truncation -Wno-missing-field-initializers -Wno-unused-const-variable
    -I. -Irelease -Ilib/tcc -I"$OUT/include")
libs=(-Lrelease -Wl,-rpath,"$ROOT/release" -pthread -ldl -lm)
sanitize=(-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
run emitter-build "$CC" "${strict[@]}" test/cases/xlir_code_owner_binding_contract.c "${libs[@]}" -lxlc -lxl -o "$OUT/emitter"
for pass in generated repeat; do
    run "$pass-emit" "$OUT/emitter" test/cases/xlir_process_future_module_lifetime.xl \
        "$OUT/$pass.c" "$OUT/$pass.h" "$OUT/$pass.json" --script
done
for ext in c h json; do cmp "$OUT/generated.$ext" "$OUT/repeat.$ext"; done
# Derive only the test provider's canonical entries from verified XLIR. Never
# infer a production ABI from source spelling or inject an extra Cgen path.
python3 -c 'import json,sys; p=json.load(open(sys.argv[1])); nodes={n["id"]:n for n in p["nodes"]}; m=[m for m in p["asyncMachines"] if nodes[p["functions"][m["function"]]["declaration"]]["parent"]==1]; f=[f for f in p["functions"] if f["kindName"]=="user" and f["parameterCount"]==0]; assert len(m)==len(f)==1 and len(p["asyncMachines"])==2; print("#define PROCESS_RUN xlir_async_start_%d\n#define PROCESS_CREATE xlir_fn_%d"%(m[0]["targetSymbol"],f[0]["symbol"]))' \
    "$OUT/generated.json" >"$OUT/binding.h"
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
done
sha256sum --quiet -c "$OUT/inputs.sha256"
printf '{"Complete":true,"Checks":%d,"InputsUnchanged":true,"Scope":"Linux actual generated ELF provider and current source API/XRT/resident runtime, ASan/UBSan/leaks; escaped ProcessExitStatus pins verified; no TCC or global opaque native handle acceptance"}\n' "$checks" >"$OUT/results.json"
printf 'Process Future actual ELF retirement checks: %d\n' "$checks"
