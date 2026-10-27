#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-dotenv-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror
       -DXRT_MODULE_ENVIRONMENT -DXRT_MODULE_FILE_WHOLE
       -DXRT_MODULE_DIR_TEMP
       -DXRT_MODULE_MAP -DXRT_MODULE_STRING -DXRT_MODULE_UNICODE
       -DXRT_MODULE_MEMORY_DEBUG
       -Ilib/xrt -fsanitize=address,undefined
       -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
sources=(release/extlibs/dotenv/xrt_dotenv.c
         release/extlibs/dotenv/test/xrt_runtime.c)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

"$cc" "${flags[@]}" release/extlibs/dotenv/test/test_dotenv.c \
    "${sources[@]}" -lm -ldl -o "$work/test_dotenv"
timeout 90s "$work/test_dotenv"
"$cc" "${flags[@]}" release/extlibs/dotenv/examples/basic.c \
    release/extlibs/dotenv/test/xrt_runtime.c -lm -ldl -o "$work/example"
timeout 30s "$work/example" > "$work/example.txt"
# Windows 工作树可能把文本夹具检出为 CRLF；Linux 输出统一按 LF 核对。
sed 's/\r$//' release/extlibs/dotenv/examples/basic.expected.txt > "$work/expected.txt"
diff -u "$work/expected.txt" "$work/example.txt"

xl=release/xl
if [[ ! -x "$xl" ]]; then
    printf 'dotenv: Linux xl runner is missing: %s\n' "$xl" >&2
    exit 1
fi
# WSL 挂载盘上的独立 EXE 构建可能失败；语言门禁在本次临时原生目录运行。
stage="$work/stage"
mkdir -p "$stage/release/extlibs" "$stage/release/examples/extlibs" "$stage/test/cases" "$stage/test/data"
cp -a release/xl release/libxlc.so "$stage/release/"
cp -a release/extlibs/dotenv "$stage/release/extlibs/"
cp -a release/examples/extlibs/dotenv.xl "$stage/release/examples/extlibs/"
cp -a test/cases/extlib_dotenv.xl "$stage/test/cases/"
cp -a test/data/dotenv_test.env "$stage/test/data/"
cd "$stage"
timeout 90s "$xl" run release/extlibs/dotenv/examples/basic.c > "$work/example_direct.txt"
diff -u "$work/example.txt" "$work/example_direct.txt"
timeout 90s "$xl" run test/cases/extlib_dotenv.xl
timeout 90s "$xl" run release/extlibs/dotenv/test/limits.xl
timeout 90s "$xl" run release/extlibs/dotenv/test/apply_oom.xl
timeout 90s "$xl" run release/extlibs/dotenv/test/create_oom.xl
timeout 90s "$xl" run release/extlibs/dotenv/test/replace.xl
if timeout 90s "$xl" check release/extlibs/dotenv/test/private_native.xl > "$work/private_native.txt" 2>&1; then
    printf 'dotenv: Native methods must not be public\n' >&2
    exit 1
fi
if ! grep -Fq 'parseNative' "$work/private_native.txt" ||
   ! grep -Fq 'access' "$work/private_native.txt"; then
    cat "$work/private_native.txt" >&2
    printf 'dotenv: expected private-access diagnostic\n' >&2
    exit 1
fi
timeout 90s "$xl" run release/examples/extlibs/dotenv.xl > "$work/example_xl.txt"
diff -u "$work/expected.txt" "$work/example_xl.txt"
diff -u "$work/example.txt" "$work/example_xl.txt"
timeout 120s "$xl" build test/cases/extlib_dotenv.xl -o "$work/dotenv_core"
timeout 90s "$work/dotenv_core"
for name in limits apply_oom create_oom replace; do
    timeout 120s "$xl" build "release/extlibs/dotenv/test/$name.xl" \
        -o "$work/dotenv_$name"
    timeout 90s "$work/dotenv_$name"
done
timeout 120s "$xl" build release/examples/extlibs/dotenv.xl \
    -o "$work/dotenv_example"
timeout 45s "$work/dotenv_example" > "$work/example_aot.txt"
diff -u "$work/example.txt" "$work/example_aot.txt"
printf 'dotenv Linux C ASan/UBSan, XLang JIT/AOT and parity gate: PASS\n'
