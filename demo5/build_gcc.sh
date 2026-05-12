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

GCC_COMMON_FLAGS="-std=c11 -O2 -ffunction-sections -fdata-sections -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function -Wno-format-truncation -Wno-restrict -Wno-maybe-uninitialized -I. -Ilib/xrt -Ilib/tcc"
GCC_LINK_FLAGS="-Wl,--gc-sections -s -ldl -lm -pthread"

BUILD_ID="$(date '+%Y-%m-%d %H:%M:%S') run-$$"
{
	printf '#ifndef XL_TEST_BUILD_ID\n'
	printf '#define XL_TEST_BUILD_ID "%s"\n' "$BUILD_ID"
	printf '#endif\n'
} > "$BUILD_STAMP_HEADER"

rm -rf release/src release/lib/xrt release/tcc/xrt
mkdir -p release/tcc/support/common release/tcc/support/runtime release/tcc/support/host release/tcc/include release/include
if [ -d examples ]; then
	rm -rf release/examples
	cp -R examples release/examples
fi
if [ ! -d release/examples ]; then
	echo "Missing examples source: neither ./examples nor ./release/examples exists."
	exit 1
fi
mkdir -p \
	release/examples/xle_native/libs/pkg.clock/bin \
	release/examples/xle_native/libs/pkg.probe/bin \
	release/examples/xle_official_native/libs/x.image.bmp/bin \
	release/examples/xle_official_native/libs/x.archive.tar/bin \
	release/examples/xle_official_native/libs/x.archive.zip/bin \
	release/examples/xle_official_native/libs/x.compress.gzip/bin \
	release/examples/xle_static/libs/pkg.clock/lib \
	release/examples/xle_static/libs/pkg.probe/lib

cp src/common/xl_common.h release/tcc/support/common/xl_common.h
cp src/common/xl_config.h release/tcc/support/common/xl_config.h
cp src/common/xl_rc.h release/tcc/support/common/xl_rc.h
cp src/runtime/xlrt_profile.h release/tcc/support/runtime/xlrt_profile.h
cp src/runtime/xlrt_host.c release/tcc/support/runtime/xlrt_host.c
cp src/runtime/xlrt_generated.h release/tcc/support/runtime/xlrt_generated.h
cp src/runtime/xlrt_generated.c release/tcc/support/runtime/xlrt_generated.c
cp src/runtime/xl_object_runtime.h release/tcc/support/runtime/xl_object_runtime.h
cp src/runtime/xl_object_runtime.c release/tcc/support/runtime/xl_object_runtime.c
cp src/runtime/xl_channel_runtime.c release/tcc/support/runtime/xl_channel_runtime.c
cp src/runtime/xl_sys_lib_runtime.c release/tcc/support/runtime/xl_sys_lib_runtime.c
cp src/runtime/xl_url_runtime.c release/tcc/support/runtime/xl_url_runtime.c
cp src/runtime/xl_http_runtime.c release/tcc/support/runtime/xl_http_runtime.c
cp src/runtime/xl_httpd_runtime.c release/tcc/support/runtime/xl_httpd_runtime.c
cp src/runtime/xl_mime_runtime.c release/tcc/support/runtime/xl_mime_runtime.c
cp src/runtime/xl_multipart_runtime.c release/tcc/support/runtime/xl_multipart_runtime.c
cp src/runtime/xl_net_dns_runtime.c release/tcc/support/runtime/xl_net_dns_runtime.c
cp src/runtime/xl_net_ip_runtime.c release/tcc/support/runtime/xl_net_ip_runtime.c
cp src/runtime/xl_net_tcp_runtime.c release/tcc/support/runtime/xl_net_tcp_runtime.c
cp src/runtime/xl_net_udp_runtime.c release/tcc/support/runtime/xl_net_udp_runtime.c
cp src/runtime/xl_net_tls_runtime.c release/tcc/support/runtime/xl_net_tls_runtime.c
cp src/runtime/xl_net_proxy_runtime.c release/tcc/support/runtime/xl_net_proxy_runtime.c
cp src/runtime/xl_net_ws_runtime.c release/tcc/support/runtime/xl_net_ws_runtime.c
cp src/host/xl_host_runtime.h release/tcc/support/host/xl_host_runtime.h
cp src/host/xl_host_runtime.c release/tcc/support/host/xl_host_runtime.c
cp lib/xrt/xrt.h release/tcc/include/xrt.h
cp src/sdk/xlang.h release/include/xlang.h

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

echo "Building xle example artifacts..."
gcc -std=c11 -shared -fPIC -o release/examples/xle_native/libs/pkg.clock/bin/pkg_clock.dll release/examples/xle_artifacts/pkg_clock_artifact.c
gcc -std=c11 -shared -fPIC -o release/examples/xle_native/libs/pkg.probe/bin/pkg_probe.dll release/examples/xle_artifacts/pkg_clock_artifact.c
gcc -std=c11 -shared -fPIC -o release/examples/xle_official_native/libs/x.image.bmp/bin/x_image_bmp.dll release/examples/xle_artifacts/x_image_bmp_artifact.c
gcc -std=c11 -shared -fPIC -o release/examples/xle_official_native/libs/x.archive.tar/bin/x_archive_tar.dll release/examples/xle_artifacts/x_archive_tar_artifact.c
gcc -std=c11 -shared -fPIC -o release/examples/xle_official_native/libs/x.archive.zip/bin/x_archive_zip.dll release/examples/xle_artifacts/x_archive_zip_artifact.c
gcc -std=c11 -shared -fPIC -o release/examples/xle_official_native/libs/x.compress.gzip/bin/x_compress_gzip.dll release/examples/xle_artifacts/x_compress_gzip_artifact.c

"$XL5_BUILD_EXE" build-c release/examples/xle_artifacts/pkg_clock_artifact.c --emit release/tmp/pkg_clock_artifact.o >/dev/null
ar rcs release/tmp/pkg_clock_static.a release/tmp/pkg_clock_artifact.o
cp release/tmp/pkg_clock_static.a release/examples/xle_static/libs/pkg.clock/lib/pkg_clock.a
cp release/tmp/pkg_clock_static.a release/examples/xle_static/libs/pkg.probe/lib/pkg_probe.a

mv -f "$XL5_BUILD_EXE" release/bin/xl5
mv -f "$XL5_TEST_BUILD_EXE" release/bin/xl5_test

echo "Build completed: release/bin/xl5"
echo "Run test_gcc.sh to execute GCC smoke tests."
