#!/usr/bin/env sh
set -eu

cd "$(dirname "$0")"

XL5_EXE="release/bin/xl5"
XL5_TEST_EXE="release/bin/xl5_test"
BUILD_STAMP_HEADER="release/tmp/xl_test_build_stamp.h"
XL5_SMOKE_LOG="release/tmp/xl5_smoke.log"
XL5_TEST_SMOKE_LOG="release/tmp/xl5_test_smoke.log"
XL5_SMOKE_FILE="release/examples/full/for_in_range.xl"
SMOKE_SELECTOR="lower.async_generator_loop_control_hybrid_groups_json"

if [ ! -x "$XL5_EXE" ]; then
	echo "Missing $XL5_EXE. Run build_gcc.sh first."
	exit 1
fi
if [ ! -x "$XL5_TEST_EXE" ]; then
	echo "Missing $XL5_TEST_EXE. Run build_gcc.sh first."
	exit 1
fi

mkdir -p release/tmp
rm -f "$XL5_SMOKE_LOG" "$XL5_TEST_SMOKE_LOG"

echo "Running xl5 smoke test..."
if ! "$XL5_EXE" sem "$XL5_SMOKE_FILE" > "$XL5_SMOKE_LOG" 2>&1; then
	echo "xl5 smoke test failed for $XL5_SMOKE_FILE."
	if [ -f "$BUILD_STAMP_HEADER" ]; then
		cat "$BUILD_STAMP_HEADER"
	fi
	cat "$XL5_SMOKE_LOG"
	exit 1
fi

echo "Running xl5_test smoke tests..."
if ! "$XL5_TEST_EXE" "$SMOKE_SELECTOR" > "$XL5_TEST_SMOKE_LOG" 2>&1; then
	echo "xl5_test smoke test failed for $SMOKE_SELECTOR."
	if [ -f "$BUILD_STAMP_HEADER" ]; then
		cat "$BUILD_STAMP_HEADER"
	fi
	cat "$XL5_TEST_SMOKE_LOG"
	exit 1
fi
"$XL5_TEST_EXE" vararg_array_spread_basic
"$XL5_TEST_EXE" vararg_dict_spread_basic
"$XL5_TEST_EXE" extern_async_func_basic
"$XL5_TEST_EXE" extern_type_method_basic

echo "GCC smoke tests passed."
