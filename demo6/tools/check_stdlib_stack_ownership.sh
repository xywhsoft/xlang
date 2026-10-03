#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
OUT="${1:?Provide a fresh evidence directory}"
[[ "$OUT" = /* ]] || OUT="$ROOT/$OUT"
[[ ! -e "$OUT" ]] || { printf 'Preserve prior Stack ownership evidence\n' >&2; exit 1; }
mkdir -p "$OUT/include"
CC="$(command -v gcc)"; PY="$(command -v python3)"; checks=0
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
run native-build "$CC" "${strict[@]}" "${sanitize[@]}" test/cases/stdlib_stack_ownership_host.c \
    test/support/stack_native.c src/runtime/xl6_stack.c src/runtime/xl6_ownership_plan.c -no-pie -pthread -lm -o "$OUT/native"
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
        "$ROOT/release/libxl.so" "$ROOT/build/linux/tcc-dist/lib"
done
frozen_files | sort -zu >"$OUT/inputs.after.list"
run inventory-unchanged cmp "$OUT/inputs.list" "$OUT/inputs.after.list"
run inputs-unchanged sha256sum --check "$OUT/inputs.sha256"
"$PY" -c 'import json,sys;json.dump({"Complete":True,"InputsUnchanged":True,"Checks":int(sys.argv[2]),"Scope":"Stack ownership prerequisite: actual selected native ASan/UBSan/leaks cycles, graph shape, rooted abort, weak quarantine, unknown refusal, native-thread freeze and terminal callback tails; create/refresh/commit real OOM prefixes through no-hit. Shared public compiler source/XLIR/JIT/retire MODULE-GLOBAL-REACHABLE class/Stack cycles with intact-field finalizers and allocator ledger. Detached code-lease islands remain a known pending discovery issue. Shared libraries are not instrumented, and no Linux MEM_FREE/unmapping measurement is claimed. Owned returns, char/bytes, batch/FixedStack and other 53-module work remain pending."},open(sys.argv[1],"w"),indent=2)' "$OUT/results.json" "$checks"
printf 'Stack ownership Linux: %s checks\n' "$checks"
