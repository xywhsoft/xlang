#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-progress-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror
       -DXRT_MODULE_MEMORY_DEBUG
       -Ilib/xrt -Irelease/extlibs/progress/include
       -fsanitize=address,undefined -fno-omit-frame-pointer
       -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

"$cc" "${flags[@]}" -fsyntax-only \
    release/extlibs/progress/test/abi_contract.c
"$cc" "${flags[@]}" -c release/extlibs/progress/test/xrt_runtime.c \
    -o "$work/xrt_runtime.o"
"$cc" "${flags[@]}" -c release/extlibs/progress/progress_native.c \
    -o "$work/progress_native.o"
"$cc" "${flags[@]}" release/extlibs/progress/test/test_progress.c \
    "$work/progress_native.o" "$work/xrt_runtime.o" -lm -ldl -o "$work/test_progress"
"$cc" "${flags[@]}" release/examples/extlibs/progress_c.c \
    "$work/xrt_runtime.o" -lm -ldl -o "$work/example_progress"
timeout 90s "$work/test_progress"
"$work/example_progress" > "$work/c.txt"

./release/xl run test/cases/extlib_progress.xl
./release/xl run release/extlibs/progress/test/boundaries.xl
./release/xl run release/extlibs/progress/test/allocation_oom.xl
if ./release/xl check release/extlibs/progress/test/private_native_negative.xl \
    > "$work/private_native.txt" 2>&1; then
    printf 'progress: raw native prefix setter must be private\n' >&2
    exit 1
fi
if ! grep -Fq 'setPrefixNative' "$work/private_native.txt" ||
   ! grep -Fq 'access' "$work/private_native.txt"; then
    cat "$work/private_native.txt" >&2
    printf 'progress: expected private-access diagnostic\n' >&2
    exit 1
fi
./release/xl run release/examples/extlibs/progress.xl > "$work/xl.txt"
./release/xl run release/examples/extlibs/progress_c.c > "$work/c_direct.txt"
sed 's/\r$//' release/extlibs/progress/test/progress.expected.txt > "$work/expected.txt"
diff -u "$work/expected.txt" "$work/c.txt"
diff -u "$work/expected.txt" "$work/xl.txt"
diff -u "$work/c.txt" "$work/xl.txt"
diff -u "$work/c.txt" "$work/c_direct.txt"
for source in release/extlibs/progress/test/boundaries.xl \
              release/extlibs/progress/test/allocation_oom.xl; do
    name=$(basename "$source" .xl)
    timeout 120s ./release/xl build "$source" -o "$work/${name}_aot"
    timeout 45s "$work/${name}_aot"
done
timeout 120s ./release/xl build release/examples/extlibs/progress.xl \
    -o "$work/progress_xl_example_aot"
timeout 45s "$work/progress_xl_example_aot" > "$work/xl_aot.txt"
diff -u "$work/c.txt" "$work/xl_aot.txt"
timeout 120s ./release/xl build release/examples/extlibs/progress_c.c \
    -o "$work/progress_c_example_aot"
timeout 45s "$work/progress_c_example_aot" > "$work/c_aot.txt"
diff -u "$work/c.txt" "$work/c_aot.txt"
bash tools/check_extlib_native_globals.sh progress
printf 'progress Linux C ASan/UBSan and XLang gate: PASS\n'
