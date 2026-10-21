#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"
work=${GRAPH_WORK_DIR:-"$root/build/test/graph_linux_guard"}
mkdir -p "$work"
pkg=release/extlibs/graph
cc=${CC:-gcc}
snapshot() {
    local paths=()
    mapfile -d '' paths < <(find "$pkg" -type f -print0 | sort -z)
    paths+=(lib/xrt/xrt.h test/support/extlib_graph_runtime.c tools/check_extlib_graph.sh
            release/examples/extlibs/graph_basic.expected.txt)
    if [[ -n "${GRAPH_DECLARATIONS:-}" ]]; then paths+=("$GRAPH_DECLARATIONS"); fi
    sha256sum "${paths[@]}"
}
snapshot > "$work/inputs.before.sha256"
"$cc" --version > "$work/compiler.txt"
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt -I"$pkg/include"
       -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
"$cc" "${flags[@]}" -c test/support/extlib_graph_runtime.c -o "$work/runtime.o"
mapfile -t sources < <(python3 -c 'import json; print("\n".join(json.load(open("release/extlibs/graph/xlang.package.json"))["artifact"]["cSources"]))')
objects=()
for source in "${sources[@]}"; do
    object="$work/native-${#objects[@]}.o"
    "$cc" "${flags[@]}" -c "$pkg/$source" -o "$object"
    objects+=("$object")
done
for name in test_graph test_graph_alloc test_graph_threads; do
    "$cc" "${flags[@]}" -DNDEBUG "$pkg/test/$name.c" "${objects[@]}" "$work/runtime.o" -lm -ldl -o "$work/$name"
    timeout 90s "$work/$name" 2>&1 | tee "$work/$name.log"
done
"$cc" "${flags[@]}" -DNDEBUG "$pkg/examples/basic.c" "$work/runtime.o" -lm -ldl -o "$work/basic"
timeout 30s "$work/basic" > "$work/basic.log"
diff -u <(tr -d '\r' < release/examples/extlibs/graph_basic.expected.txt) "$work/basic.log"
timeout 90s release/xl run "$pkg/examples/basic.c" > "$work/basic_direct.log"
diff -u "$work/basic.log" "$work/basic_direct.log"
if [[ -n "${GRAPH_DECLARATIONS:-}" ]]; then
    "$cc" "${flags[@]}" -include "$pkg/include/graph.h" -x c -fsyntax-only "$GRAPH_DECLARATIONS"
fi
snapshot > "$work/inputs.after.sha256"
diff -u "$work/inputs.before.sha256" "$work/inputs.after.sha256"
printf 'graph Linux C ASan/UBSan gate: PASS\n'
