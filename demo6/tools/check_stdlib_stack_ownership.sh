#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
OUT="${1:?Provide a fresh evidence directory}"
[[ "$OUT" = /* ]] || OUT="$ROOT/$OUT"
[[ ! -e "$OUT" ]] || { printf 'Preserve prior Stack ownership evidence\n' >&2; exit 1; }
mkdir -p "$OUT/include"
CC="$(command -v gcc)"; PY="$(command -v python3)"; checks=0
FIXTURE="${2:-test/cases/stdlib_stack_ownership.xl}"
run() {
    local name="$1"; shift
    if ! "$@" >"$OUT/$name.log" 2>&1; then tail -n 30 "$OUT/$name.log" >&2; return 1; fi
    checks=$((checks+1))
}
frozen_files() {
    find src lib release/include -type f -print0
    printf '%s\0' "$CC" "$PY" release/xl release/libxl.so release/libxlc.so
    printf '%s\0' tools/check_stdlib_stack_ownership.sh tools/tcc_vfs_manifest_linux_xlc.txt
    printf '%s\0' test/cases/stdlib_stack_ownership_host.c test/cases/stdlib_stack_ownership_public.c
    printf '%s\0' test/cases/stdlib_stack_ownership.xl test/support/stack_native.c
    printf '%s\0' "$FIXTURE" test/cases/xlir_ownership_discovery_runtime.c test/support/discovery_native.c
    printf '%s\0' tools/check_stdlib_array_native.py test/cases/stdlib_array_native_host.c
    printf '%s\0' test/cases/stdlib_array_iterator_native_host.c test/support/array_native.c
    printf '%s\0' test/cases/stdlib_array_value_native_host.c test/support/array_value_native.c
    printf '%s\0' test/cases/stdlib_array_erased_native_host.c
}
frozen_files | sort -zu >"$OUT/inputs.list"
xargs -0 sha256sum <"$OUT/inputs.list" >"$OUT/inputs.sha256"
while IFS='|' read -r kind target source; do
    if [[ "$kind" = file && "$target" = include/__xl_runtime/* ]]; then
        source="${source//%XL6_ROOT%/$ROOT}"
        mkdir -p "$OUT/$(dirname "$target")"; cp "$source" "$OUT/$target"; cmp "$source" "$OUT/$target"
    fi
done < tools/tcc_vfs_manifest_linux_xlc.txt
strict=(-std=c11 -D_GNU_SOURCE -Wall -Wextra -Werror -I. -Irelease -I"$OUT/include")
sanitize=(-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
run discovery-build "$CC" "${strict[@]}" "${sanitize[@]}" test/cases/xlir_ownership_discovery_runtime.c \
    test/support/discovery_native.c src/runtime/xl6_sequence.c src/runtime/xl6_stack.c src/runtime/xl6_code_lease.c \
    src/runtime/xl6_code_finalizer.c src/runtime/xl6_ownership_plan.c src/api/xl_foreign_error.c \
    -no-pie -pthread -lm -o "$OUT/discovery"
run discovery-vectors "$OUT/discovery"
for stage in create refresh commit; do
    terminal=false
    for ((point=0;point<=2048;++point)); do
        run "discovery.$stage.oom-$point" "$OUT/discovery" "$stage" "$point"
        if grep -q 'hit=0 balanced' "$OUT/discovery.$stage.oom-$point.log"; then terminal=true; break; fi
    done
    "$terminal" || { printf 'Incomplete discovery allocation prefix\n' >&2; exit 1; }
    printf 'discovery %s %s\n' "$stage" "$point" >>"$OUT/oom-terminals.txt"
done
run native-build "$CC" "${strict[@]}" "${sanitize[@]}" test/cases/stdlib_stack_ownership_host.c \
    test/support/stack_native.c src/runtime/xl6_sequence.c src/runtime/xl6_stack.c src/runtime/xl6_ownership_plan.c -no-pie -pthread -lm -o "$OUT/native"
run native-vectors "$OUT/native"
for stage in create refresh commit; do
    terminal=false
    for ((point=0;point<=64;++point)); do
        run "native.$stage.oom-$point" "$OUT/native" "$stage" "$point"
        if grep -q 'hit=0 balanced' "$OUT/native.$stage.oom-$point.log"; then terminal=true; break; fi
    done
    "$terminal" || { printf 'Incomplete Stack ownership allocation prefix\n' >&2; exit 1; }
    printf '%s %s\n' "$stage" "$point" >>"$OUT/oom-terminals.txt"
done
# Shared public compiler/runtime lane deliberately not instrumented. It uses
# actual xlang source/XLIR/JIT and automatic module graph retirement on Linux.
run public-build "$CC" "${strict[@]}" test/cases/stdlib_stack_ownership_public.c \
    -Lrelease -Wl,-rpath,"$ROOT/release" -lxlc -pthread -ldl -lm -o "$OUT/public"
for mode in 0 1 2 3; do
    run "public.mode-$mode" "$OUT/public" "$mode" "$OUT/include" "$OUT/public-$mode.temps" \
        "$ROOT/release/libxl.so" "$ROOT/build/linux/tcc-dist/lib" "$FIXTURE"
done
# Array/cursor uses the same phased Sequence graph. Freeze and verify its real
# pruned O0/O2 closures, mixed cycles and complete allocation-failure prefixes.
run array-native "$PY" tools/check_stdlib_array_native.py --out "$OUT/array-native" --sanitize
frozen_files | sort -zu >"$OUT/inputs.after.list"
run inventory-unchanged cmp "$OUT/inputs.list" "$OUT/inputs.after.list"
run inputs-unchanged sha256sum --check "$OUT/inputs.sha256"
"$PY" -c 'import json,sys;json.dump({"Complete":True,"InputsUnchanged":True,"Checks":int(sys.argv[2]),"Fixture":sys.argv[3],"Scope":"V4 borrowed discovery with actual selected class/Stack/code-lease storage under ASan/UBSan/leaks: detached islands, original shell retired before copied island, foreign owners, alias/deep-copy abort, new finalizer islands and peer resurrection; complete create/refresh/commit OOM prefixes. Original Stack graph/thread/weak/unknown/tail population retained. Shared public source/XLIR/JIT/retire lifetimes use the recorded actual fixture with intact finalizers and exact ledger. Shared libraries not instrumented; no Linux physical unmapping measurement, borrowed finalizer self resurrection or full 53-module completion claim. Owned returns, char/bytes and batch/FixedStack remain pending."},open(sys.argv[1],"w"),indent=2)' "$OUT/results.json" "$checks" "$FIXTURE"
printf 'Stack ownership Linux: %s checks\n' "$checks"
