#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
XL="$ROOT/release/xl"
TEST_DIR="$ROOT/build/linux/test"
RELEASE_TMP="$ROOT/release/tmp"
BUILD_TMP="$ROOT/build/tmp"

if [[ ! -x "$XL" ]]; then
    "$ROOT/build_linux.sh"
fi

mkdir -p "$TEST_DIR"

snapshot_runtime_temp() {
    {
        if [[ -d "$RELEASE_TMP" ]]; then
            find "$RELEASE_TMP" -type f -printf '%p|%s|%T@\n'
        fi
        if [[ -d "$BUILD_TMP" ]]; then
            find "$BUILD_TMP" -type f -printf '%p|%s|%T@\n'
        fi
    } | LC_ALL=C sort
}

printf 'Running Linux CLI and in-memory VFS smoke tests...\n'
"$XL" --help >/dev/null
"$XL" check "$ROOT/release/examples/hello.xl"
snapshot_runtime_temp >"$TEST_DIR/temp.before"
"$XL" run "$ROOT/release/examples/hello.xl"
snapshot_runtime_temp >"$TEST_DIR/temp.after"
cmp "$TEST_DIR/temp.before" "$TEST_DIR/temp.after"

printf 'Running Linux ELF build tests...\n'
"$XL" build "$ROOT/release/examples/hello.xl" -o "$TEST_DIR/hello"
"$TEST_DIR/hello"

printf 'Running Linux C and XRT integration tests...\n'
"$XL" run "$ROOT/release/examples/stdlib_math_c.c"
"$XL" build "$ROOT/release/examples/stdlib_math_c.c" -o "$TEST_DIR/stdlib_math_c"
"$TEST_DIR/stdlib_math_c"

printf 'Running Linux UTF-8 path tests...\n'
mkdir -p "$TEST_DIR/中文路径"
cp "$ROOT/release/examples/hello.xl" "$TEST_DIR/中文路径/你好.xl"
"$XL" check "$TEST_DIR/中文路径/你好.xl"
"$XL" run "$TEST_DIR/中文路径/你好.xl"
"$XL" build "$TEST_DIR/中文路径/你好.xl" -o "$TEST_DIR/中文路径/你好"
"$TEST_DIR/中文路径/你好"

printf 'Running Linux import and package tests...\n'
"$XL" run "$ROOT/test/cases/import_source_use.xl"
"$XL" run "$ROOT/release/examples/extlibs/pkg_math.xl"

printf 'Running Linux shared-library host API tests...\n'
"${CC:-gcc}" -std=c11 -Wall -I"$ROOT/release" \
    "$ROOT/release/examples/xl_dll_embed_string.c" \
    -L"$ROOT/release" -lxl -pthread -ldl -lm \
    -o "$TEST_DIR/xl_embed_host"
LD_LIBRARY_PATH="$ROOT/release${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
    "$TEST_DIR/xl_embed_host"

"${CC:-gcc}" -std=c11 -Wall -I"$ROOT/release" \
    "$ROOT/release/examples/xlc_api_embed.c" \
    -L"$ROOT/release" -lxlc -pthread -ldl -lm \
    -o "$TEST_DIR/xlc_embed_host"
LD_LIBRARY_PATH="$ROOT/release${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
    "$TEST_DIR/xlc_embed_host"

printf 'DEMO6 Linux compatibility tests passed.\n'
