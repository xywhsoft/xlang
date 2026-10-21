#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"
work=${CHARDET_WORK_DIR:-"$root/build/test/chardet_linux_guard"}
mkdir -p "$work"
pkg=release/extlibs/chardet
cc=${CC:-gcc}
snapshot() {
    local paths=()
    mapfile -d '' paths < <(find "$pkg" -type f -print0 | sort -z)
    paths+=(lib/xrt/xrt.h test/support/extlib_chardet_runtime.c tools/check_extlib_chardet.sh
            release/examples/extlibs/chardet_basic.expected.txt)
    if [[ -n "${CHARDET_DECLARATIONS:-}" ]]; then paths+=("$CHARDET_DECLARATIONS"); fi
    sha256sum "${paths[@]}"
}
snapshot > "$work/inputs.before.sha256"
"$cc" --version > "$work/compiler.txt"
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt -I"$pkg/include"
       -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
"$cc" "${flags[@]}" -c test/support/extlib_chardet_runtime.c -o "$work/runtime.o"
"$cc" "${flags[@]}" -c "$pkg/chardet_native.c" -o "$work/native.o"
for name in test_chardet test_threads; do
    "$cc" "${flags[@]}" "$pkg/test/$name.c" "$work/native.o" "$work/runtime.o" -lm -ldl -o "$work/$name"
    timeout 90s "$work/$name" 2>&1 | tee "$work/$name.log"
done
"$cc" "${flags[@]}" "$pkg/examples/basic.c" "$work/native.o" "$work/runtime.o" -lm -ldl -o "$work/basic"
timeout 30s "$work/basic" > "$work/basic.log"
diff -u <(tr -d '\r' < release/examples/extlibs/chardet_basic.expected.txt) "$work/basic.log"
if [[ -n "${CHARDET_DECLARATIONS:-}" ]]; then
    "$cc" "${flags[@]}" -include "$pkg/include/chardet.h" -x c -fsyntax-only "$CHARDET_DECLARATIONS"
fi
snapshot > "$work/inputs.after.sha256"
diff -u "$work/inputs.before.sha256" "$work/inputs.after.sha256"
printf 'chardet Linux C ASan/UBSan gate: PASS\n'
