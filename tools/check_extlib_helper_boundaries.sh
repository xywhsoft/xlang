#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"
work="$root/build/test/partial_helpers_linux"
mkdir -p "$work"
input_paths() {
    find release/extlibs/mysql release/extlibs/faiss -type f -print0
    printf '%s\0' lib/xrt/xrt.h test/support/extlib_helper_runtime.c \
        test/cases/extlib_partial_helper_boundaries_c.c tools/check_extlib_helper_boundaries.sh
}
mapfile -d '' inputs < <(input_paths | sort -z)
sha256sum "${inputs[@]}" > "$work/inputs.before.sha256"
cc=${CC:-gcc}
"$cc" --version > "$work/compiler.txt"
"$cc" -std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt \
    -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie \
    test/cases/extlib_partial_helper_boundaries_c.c \
    release/extlibs/mysql/mysql_native.c release/extlibs/faiss/faiss_native.c \
    test/support/extlib_helper_runtime.c -lm -ldl -pthread -o "$work/test"
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
timeout 30s "$work/test" 2>&1 | tee "$work/test.log"
"$cc" -std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt \
    -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie \
    release/extlibs/mysql/examples/basic.c release/extlibs/mysql/mysql_native.c \
    test/support/extlib_helper_runtime.c -lm -ldl -pthread -o "$work/example"
timeout 30s "$work/example" > "$work/example.log"
diff -u release/extlibs/mysql/examples/basic.expected.txt "$work/example.log"
mapfile -d '' after < <(input_paths | sort -z)
sha256sum "${after[@]}" > "$work/inputs.after.sha256"
diff -u "$work/inputs.before.sha256" "$work/inputs.after.sha256"
printf 'Partial helper Linux C ASan/UBSan gate: PASS\n'
