#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"
tmp_root=$(realpath -m -- "${TMPDIR:-/tmp}")
work=$(mktemp -d "$tmp_root/xlang-sqlite-XXXXXXXX")
cleanup() {
    case "$work" in
        "$tmp_root"/xlang-sqlite-*) rm -rf -- "$work" ;;
    esac
}
trap cleanup EXIT
cc=${CC:-gcc}
pkg=release/extlibs/sqlite
mapfile -d '' inputs < <(find "$pkg" -type f \( -name '*.c' -o -name '*.h' -o -name '*.json' -o -name '*.xl' \) -print0 | sort -z)
inputs+=(lib/xrt/xrt.h test/support/extlib_sqlite_runtime.c tools/check_extlib_sqlite.sh
         release/examples/extlibs/sqlite_basic.expected.txt)
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Ilib/xrt -fsanitize=address,undefined
       -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
strict=(-Wall -Wextra -Werror)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
sha256sum "${inputs[@]}" > "$work/inputs.before.sha256"
"$cc" --version > "$work/compiler.txt"
"$cc" "${flags[@]}" -c "$pkg/third_party/sqlite3.c" -o "$work/sqlite3.o"
"$cc" "${flags[@]}" "${strict[@]}" -c "$pkg/sqlite_native.c" -o "$work/native.o"
"$cc" "${flags[@]}" "${strict[@]}" -c test/support/extlib_sqlite_runtime.c -o "$work/runtime.o"
objects=("$work/sqlite3.o" "$work/native.o" "$work/runtime.o")
for name in test_sqlite test_sqlite_alloc test_sqlite_threads; do
    test_flags=()
    if [[ $name == test_sqlite_threads ]]; then test_flags=(-DNDEBUG); fi
    "$cc" "${flags[@]}" "${strict[@]}" "${test_flags[@]}" "$pkg/test/$name.c" "${objects[@]}" -lm -ldl -o "$work/$name"
    timeout 120s "$work/$name" 2>&1 | tee "$work/$name.log"
done
"$cc" "${flags[@]}" "${strict[@]}" -DNDEBUG "$pkg/examples/basic.c" "${objects[@]}" -lm -ldl -o "$work/basic"
timeout 30s "$work/basic" > "$work/basic.log"
diff -u release/examples/extlibs/sqlite_basic.expected.txt "$work/basic.log"
timeout 30s ./release/xl run "$pkg/examples/basic.c" > "$work/basic_direct.log"
diff -u "$work/basic.log" "$work/basic_direct.log"
sha256sum "${inputs[@]}" > "$work/inputs.after.sha256"
diff -u "$work/inputs.before.sha256" "$work/inputs.after.sha256"
printf 'SQLite Linux C ASan/UBSan gate: PASS\n'
