#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/../../../.." && pwd)
work_root=$(CDPATH= cd -- "${TMPDIR:-/tmp}" && pwd)
work=$(mktemp -d "$work_root/xlang-tar-build-XXXXXX")
run=$(mktemp -d "$work_root/xlang-tar-run-XXXXXX")
cleanup() {
    code=$?
    if [ "$code" -eq 0 ]; then
        case "$work" in "$work_root"/xlang-tar-build-*) ;; *) exit 1;; esac
        case "$run" in "$work_root"/xlang-tar-run-*) ;; *) exit 1;; esac
        rm -rf -- "$work" "$run"
    else
        printf 'TAR failure artifacts retained: %s %s\n' "$work" "$run" >&2
    fi
}
trap cleanup EXIT
cd "$root"
sh release/extlibs/tar/test/check_numeric.sh

cc=${CC:-cc}
flags='-std=c11 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer'
sources='release/extlibs/tar/xrt_tar_common.c release/extlibs/tar/xrt_tar_reader.c release/extlibs/tar/xrt_tar_writer.c release/extlibs/tar/xrt_tar_extract.c release/extlibs/tar/test/runtime.c'
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
$cc $flags -fsyntax-only -Ilib/xrt -Ilib/xruntime -Irelease/extlibs/tar \
    -DXRT_MODULE_FILE_ROOT -DXRT_MODULE_FILE_TEMP -DXRT_MODULE_FILE_TREE \
    -DXRT_MODULE_IO_FILE -DXRT_MODULE_PATH_SAFE -DXRT_MODULE_STRING_FORMAT \
    -DXRT_MODULE_SET -DXRT_MODULE_HASH64 \
    -DXRT_MODULE_NUMBER_INTEGER \
    release/extlibs/tar/xrt_tar_common.c release/extlibs/tar/xrt_tar_reader.c \
    release/extlibs/tar/xrt_tar_writer.c release/extlibs/tar/xrt_tar_extract.c
$cc $flags -Ilib/xrt -Irelease/extlibs/tar $sources \
    release/extlibs/tar/test/test_tar_c.c -pthread -lm -o "$work/test_tar"
(cd "$run" && timeout 180s "$work/test_tar")
$cc $flags -Ilib/xrt -Irelease/extlibs/tar \
    release/extlibs/tar/examples/basic_c.c release/extlibs/tar/test/runtime.c \
    -pthread -lm -o "$work/basic"
(cd "$run" && timeout 30s "$work/basic" > "$run/c_example.txt")
$cc $flags -Ilib/xrt -Irelease/extlibs/tar \
    release/extlibs/tar/examples/append_c.c release/extlibs/tar/test/runtime.c \
    -pthread -lm -o "$work/append"
(cd "$run" && timeout 30s "$work/append" > "$run/append_c.txt")

if [ ! -x "$root/release/xl" ]; then
    printf 'tar: Linux xl runner is missing: %s\n' "$root/release/xl" >&2
    exit 1
fi
# 校验生成原型与包桥接在 LP64 上的 C ABI。
timeout 90s "$root/release/xl" run release/extlibs/tar/test/test_read_file_limit.c \
    release/extlibs/tar/tar_native.c \
    release/extlibs/tar/xrt_tar_common.c \
    release/extlibs/tar/xrt_tar_reader.c \
    release/extlibs/tar/xrt_tar_writer.c \
    release/extlibs/tar/xrt_tar_extract.c \
    --inc-path lib/xrt --inc-path lib/xruntime \
    --inc-path release/extlibs/tar
(cd "$root/release" && \
    timeout 180s ./xl run extlibs/tar/test/boundaries.xl && \
    timeout 90s ./xl run extlibs/tar/test/named_type.xl && \
    timeout 30s ./xl run extlibs/tar/examples/basic_c.c > "$run/jit_c_example.txt" && \
    timeout 90s ./xl run extlibs/tar/examples/basic.xl > "$run/xl_example.txt" && \
    timeout 30s ./xl run extlibs/tar/examples/append_c.c > "$run/append_jit_c.txt" && \
    timeout 90s ./xl run extlibs/tar/examples/append.xl > "$run/append_xl.txt")
if (cd "$root/release" && timeout 90s ./xl check extlibs/tar/test/private_native.xl) > "$run/private_native.txt" 2>&1; then
    printf 'tar: native methods must not be public\n' >&2
    exit 1
fi
if ! grep -Fq 'addStrNative' "$run/private_native.txt" ||
   ! grep -Fq 'access' "$run/private_native.txt"; then
    cat "$run/private_native.txt" >&2
    printf 'tar: expected private-access diagnostic\n' >&2
    exit 1
fi
if (cd "$root/release" && timeout 90s ./xl check extlibs/tar/test/private_reader_constructor.xl) > "$run/private_reader_constructor.txt" 2>&1; then
    printf 'tar: raw Reader constructor must not be exported\n' >&2
    exit 1
fi
if ! grep -Fq 'no such exported type' "$run/private_reader_constructor.txt"; then
    cat "$run/private_reader_constructor.txt" >&2
    printf 'tar: expected non-exported Reader diagnostic\n' >&2
    exit 1
fi
diff -u "$run/c_example.txt" "$run/xl_example.txt"
diff -u "$run/c_example.txt" "$run/jit_c_example.txt"
printf 'first\nsecond\n' > "$run/append.expected.txt"
diff -u "$run/append.expected.txt" "$run/append_c.txt"
diff -u "$run/append_c.txt" "$run/append_jit_c.txt"
diff -u "$run/append_c.txt" "$run/append_xl.txt"
timeout 90s "$root/release/xl" run release/examples/extlibs/tar.xl > "$run/tar_xl.txt"
printf 'tar example: PASS\n' > "$run/tar_xl.expected.txt"
diff -u "$run/tar_xl.expected.txt" "$run/tar_xl.txt"
timeout 30s "$root/release/xl" run release/examples/extlibs/tar_c.c > "$run/tar_c.txt"
printf 'tar C example: PASS\n' > "$run/tar_c.expected.txt"
diff -u "$run/tar_c.expected.txt" "$run/tar_c.txt"
timeout 30s "$root/release/xl" run release/examples/extlibs/tar_compressed_c.c > "$run/tar_compressed_c.txt"
printf 'compressed TAR C example: PASS\n' > "$run/tar_compressed_c.expected.txt"
diff -u "$run/tar_compressed_c.expected.txt" "$run/tar_compressed_c.txt"
if command -v python3 >/dev/null 2>&1; then
    timeout 180s python3 release/extlibs/tar/test/interop.py release
else
    printf 'tar: Python 3 unavailable; skipping independent XZ interop check\n' >&2
fi
printf 'tar Linux C ASan/UBSan and XLang parity gate: PASS\n'
