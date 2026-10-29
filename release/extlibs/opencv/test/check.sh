#!/bin/sh
set -eu
cd "$(dirname "$0")/../../../.."
gcc -std=c11 -Wall -Wextra -Werror \
    -DXRT_MODULE_ONCE -DXRT_MODULE_CHARSET -DXRT_MODULE_PATH_SYSTEM \
    -DXRT_MODULE_ENVIRONMENT -DXRT_MODULE_STRING -Ilib/xrt -fsyntax-only \
    release/extlibs/opencv/opencv_artifact.c \
    release/extlibs/opencv/examples/image_info_c.c \
    release/extlibs/opencv/test/boundary_c.c \
    release/extlibs/opencv/test/utf8_path_c.c
printf '%s\n' 'opencv Linux syntax: PASS (native DLL runtime not exercised)'
