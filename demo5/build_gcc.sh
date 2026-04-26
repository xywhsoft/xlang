#!/usr/bin/env sh
set -eu

cd "$(dirname "$0")"

mkdir -p release/bin release/tmp
rm -f release/tmp/*.fallback.exe release/tmp/*.trace.txt

SRC_LIST="release/tmp/gcc_sources.rsp"
TEST_SRC_LIST="release/tmp/gcc_test_sources.rsp"
TCC_OBJ="release/tmp/libtcc.o"
XL5_BUILD_EXE="release/tmp/xl5.build"
XL5_TEST_BUILD_EXE="release/tmp/xl5_test.build"
BUILD_STAMP_HEADER="release/tmp/xl_test_build_stamp.h"
XL5_SMOKE_LOG="release/tmp/xl5_smoke.log"
XL5_TEST_SMOKE_LOG="release/tmp/xl5_test_smoke.log"
XL5_SMOKE_FILE="release/examples/full/for_in_range.xl"
SMOKE_SELECTOR="lower.async_generator_loop_control_hybrid_groups_json"

GCC_COMMON_FLAGS="-std=c11 -O2 -ffunction-sections -fdata-sections -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function -Wno-format-truncation -Wno-restrict -Wno-maybe-uninitialized -I. -Ilib/xrt -Ilib/tcc"
GCC_LINK_FLAGS="-Wl,--gc-sections -s -ldl -lm -pthread"

BUILD_ID="$(date '+%Y-%m-%d %H:%M:%S.%3N') run-${RANDOM}-${RANDOM}"
{
	printf '#ifndef XL_TEST_BUILD_ID\n'
	printf '#define XL_TEST_BUILD_ID "%s"\n' "$BUILD_ID"
	printf '#endif\n'
} > "$BUILD_STAMP_HEADER"

echo "Generating lexer with Flex..."
flex --noline -o src/lexer/xl_lexer_gen.c --header-file=src/lexer/xl_lexer_gen.h src/lexer/xl_lexer.l

echo "Generating parser with Lemon..."
(cd src/parser && lemon -q xl_parser.y)

{
	echo "main.c"
	find src -name '*.c' -print | sort
} > "$SRC_LIST"

{
	echo "test/main.c"
	echo "test/xl_test.c"
	find src -name '*.c' -print | sort
} > "$TEST_SRC_LIST"

echo "Building embedded TCC object..."
gcc -std=c11 -O2 -ffunction-sections -fdata-sections -w -Ilib/tcc -c lib/tcc/libtcc.c -o "$TCC_OBJ"

echo "Building demo5 with GCC..."
gcc $GCC_COMMON_FLAGS @"$SRC_LIST" "$TCC_OBJ" -o "$XL5_BUILD_EXE" $GCC_LINK_FLAGS

echo "Building demo5 test runner with GCC..."
gcc $GCC_COMMON_FLAGS @"$TEST_SRC_LIST" "$TCC_OBJ" -o "$XL5_TEST_BUILD_EXE" $GCC_LINK_FLAGS

echo "Building xlp example artifacts..."
gcc -std=c11 -shared -fPIC -o release/examples/xlp_native/packages/pkg.clock/bin/pkg_clock.dll release/examples/xlp_artifacts/pkg_clock_artifact.c
gcc -std=c11 -shared -fPIC -o release/examples/xlp_native/packages/pkg.probe/bin/pkg_probe.dll release/examples/xlp_artifacts/pkg_clock_artifact.c
gcc -std=c11 -shared -fPIC -o release/examples/xlp_official_native/packages/x.image.bmp/bin/x_image_bmp.dll release/examples/xlp_artifacts/x_image_bmp_artifact.c
gcc -std=c11 -shared -fPIC -o release/examples/xlp_official_native/packages/x.archive.tar/bin/x_archive_tar.dll release/examples/xlp_artifacts/x_archive_tar_artifact.c
gcc -std=c11 -shared -fPIC -o release/examples/xlp_official_native/packages/x.archive.zip/bin/x_archive_zip.dll release/examples/xlp_artifacts/x_archive_zip_artifact.c
gcc -std=c11 -shared -fPIC -o release/examples/xlp_official_native/packages/x.compress.gzip/bin/x_compress_gzip.dll release/examples/xlp_artifacts/x_compress_gzip_artifact.c

"$XL5_BUILD_EXE" build-c release/examples/xlp_artifacts/pkg_clock_artifact.c --emit release/tmp/pkg_clock_artifact.o >/dev/null
ar rcs release/tmp/pkg_clock_static.a release/tmp/pkg_clock_artifact.o
cp release/tmp/pkg_clock_static.a release/examples/xlp_static/packages/pkg.clock/lib/pkg_clock.a
cp release/tmp/pkg_clock_static.a release/examples/xlp_static/packages/pkg.probe/lib/pkg_probe.a

mv -f "$XL5_BUILD_EXE" release/bin/xl5
mv -f "$XL5_TEST_BUILD_EXE" release/bin/xl5_test

release/bin/xl5 sem "$XL5_SMOKE_FILE" > "$XL5_SMOKE_LOG" 2>&1
release/bin/xl5_test "$SMOKE_SELECTOR" > "$XL5_TEST_SMOKE_LOG" 2>&1
release/bin/xl5_test vararg_array_spread_basic
release/bin/xl5_test vararg_dict_spread_basic
release/bin/xl5_test extern_async_func_basic
release/bin/xl5_test extern_type_method_basic

echo "Build completed: release/bin/xl5"
rm -rf release/src release/lib/xrt release/tcc/xrt
mkdir -p release/tcc/support/common release/tcc/support/runtime release/tcc/support/host release/tcc/include
if [ -d examples ]; then
	rm -rf release/examples
	cp -R examples release/examples
fi
if [ ! -d release/examples ]; then
	echo "Missing examples source: neither ./examples nor ./release/examples exists."
	exit 1
fi
mkdir -p \
	release/examples/xlp_native/packages/pkg.clock/bin \
	release/examples/xlp_native/packages/pkg.probe/bin \
	release/examples/xlp_official_native/packages/x.image.bmp/bin \
	release/examples/xlp_official_native/packages/x.archive.tar/bin \
	release/examples/xlp_official_native/packages/x.archive.zip/bin \
	release/examples/xlp_official_native/packages/x.compress.gzip/bin \
	release/examples/xlp_static/packages/pkg.clock/lib \
	release/examples/xlp_static/packages/pkg.probe/lib
cp src/common/xl_common.h release/tcc/support/common/xl_common.h
cp src/runtime/xlrt_profile.h release/tcc/support/runtime/xlrt_profile.h
cp src/runtime/xlrt_host.c release/tcc/support/runtime/xlrt_host.c
cp src/runtime/xlrt_generated.h release/tcc/support/runtime/xlrt_generated.h
cp src/runtime/xlrt_generated.c release/tcc/support/runtime/xlrt_generated.c
cp src/runtime/xl_object_runtime.h release/tcc/support/runtime/xl_object_runtime.h
cp src/runtime/xl_object_runtime.c release/tcc/support/runtime/xl_object_runtime.c
cp src/runtime/xl_channel_runtime.c release/tcc/support/runtime/xl_channel_runtime.c
cp src/runtime/xl_sys_lib_runtime.c release/tcc/support/runtime/xl_sys_lib_runtime.c
cp src/host/xl_host_runtime.h release/tcc/support/host/xl_host_runtime.h
cp src/host/xl_host_runtime.c release/tcc/support/host/xl_host_runtime.c
cp lib/xrt/xrt.h release/tcc/include/xrt.h
