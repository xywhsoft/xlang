#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"
work="$root/build/test/aho_linux_guard"
mkdir -p "$work"
cc=${CC:-gcc}
pkg=release/extlibs/aho_corasick
mapfile -d '' inputs < <(find "$pkg" -type f \( -name '*.c' -o -name '*.h' -o -name '*.json' -o -name '*.xl' \) -print0 | sort -z)
inputs+=(lib/xrt/xrt.h test/support/extlib_aho_runtime.c tools/check_extlib_aho.sh
         release/examples/extlibs/aho_corasick_c.c release/examples/extlibs/aho_corasick_basic.expected.txt)
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt
       -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
sha256sum "${inputs[@]}" > "$work/inputs.before.sha256"
"$cc" --version > "$work/compiler.txt"
"$cc" "${flags[@]}" -c test/support/extlib_aho_runtime.c -o "$work/runtime.o"
"$cc" "${flags[@]}" -c "$pkg/xrt_aho_corasick.c" -o "$work/core.o"
"$cc" "${flags[@]}" -c "$pkg/aho_corasick_native.c" -o "$work/native.o"
objects=("$work/core.o" "$work/native.o" "$work/runtime.o")
"$cc" "${flags[@]}" "$pkg/test/test_aho_corasick.c" "$work/runtime.o" -lm -ldl -o "$work/test_aho_corasick"
timeout 60s "$work/test_aho_corasick" 2>&1 | tee "$work/test_aho_corasick.log"
for name in test_aho_native test_aho_alloc test_aho_threads; do
    "$cc" "${flags[@]}" "$pkg/test/$name.c" "${objects[@]}" -lm -ldl -o "$work/$name"
    timeout 60s "$work/$name" 2>&1 | tee "$work/$name.log"
done
"$cc" "${flags[@]}" "$pkg/examples/basic.c" "${objects[@]}" -lm -ldl -o "$work/basic"
timeout 30s "$work/basic" > "$work/basic.log"
diff -u release/examples/extlibs/aho_corasick_basic.expected.txt "$work/basic.log"
"$cc" "${flags[@]}" release/examples/extlibs/aho_corasick_c.c "${objects[@]}" -lm -ldl -o "$work/tutorial"
timeout 30s "$work/tutorial" > "$work/tutorial.log"
for name in private_add_pattern_native private_feed_native; do
    if timeout 30s ./release/xl check "$pkg/test/$name.xl" > "$work/$name.log" 2>&1; then
        printf 'aho: %s must be rejected\n' "$name" >&2
        exit 1
    fi
    grep -Fq 'access' "$work/$name.log"
done
sha256sum "${inputs[@]}" > "$work/inputs.after.sha256"
diff -u "$work/inputs.before.sha256" "$work/inputs.after.sha256"
printf 'Aho Linux independent C ASan/UBSan gate: PASS\n'
