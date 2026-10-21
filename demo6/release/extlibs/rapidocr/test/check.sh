#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/../../../.." && pwd)
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-rapidocr-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cd "$root"

cc=${CC:-cc}
flags='-std=c11 -O2 -Wall -Wextra -Werror -DXRT_MODULE_VALUE_CONTAINER -DXRT_MODULE_JSON -DXRT_MODULE_BUFFER -DXRT_MODULE_FILE_WHOLE -DXRT_MODULE_PATH_SYSTEM -DXRT_MODULE_ENVIRONMENT -DXRT_MODULE_ONCE -DXRT_MODULE_THREAD_KEY -DXRT_MODULE_DIR'
includes='-Ilib/xrt -Irelease/extlibs/rapidocr/include -Irelease/extlibs/rapidocr/src'
sources='release/extlibs/rapidocr/src/rapidocr_artifact.c release/extlibs/rapidocr/src/rapidocr_json.c release/extlibs/rapidocr/test/runtime.c'

# 仅验证 C API；XLang 与真实模型门禁在 Windows 的 check.ps1 中。
$cc $flags $includes $sources release/extlibs/rapidocr/test/test_rapidocr.c \
    -ldl -pthread -lm -o "$work/test_rapidocr_linux"
"$work/test_rapidocr_linux"
$cc -std=c11 -O2 -Wall -Wextra -Werror -fPIC -shared \
    release/extlibs/rapidocr/test/partial_shim.c -o "$work/partial_rapidocr.so"
$cc $flags $includes $sources release/extlibs/rapidocr/test/partial_probe.c \
    -ldl -pthread -lm -o "$work/partial_probe_linux"
RAPIDOCR_XLANG_DLL="$work/partial_rapidocr.so" "$work/partial_probe_linux"
$cc -std=c11 -O2 -Wall -Wextra -Werror -fPIC -shared \
    -Irelease/extlibs/rapidocr/include \
    release/extlibs/rapidocr/test/complete_shim.c -o "$work/complete_rapidocr.so"
$cc $flags $includes $sources release/extlibs/rapidocr/test/complete_probe.c \
    -ldl -pthread -lm -o "$work/complete_probe_linux"
RAPIDOCR_XLANG_DLL="$work/complete_rapidocr.so" "$work/complete_probe_linux"
$cc $flags $includes $sources release/extlibs/rapidocr/examples/postprocess_c.c \
    -ldl -pthread -lm -o "$work/postprocess_c_linux"
"$work/postprocess_c_linux"
