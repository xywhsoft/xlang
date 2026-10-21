#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-toml-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -DXRT_IMPLEMENTATION -O1 -g
       -Wall -Wextra -Werror -Ilib/xrt -fsanitize=address,undefined
       -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

"$cc" "${flags[@]}" release/extlibs/toml/test/test_toml.c \
    -lm -ldl -o "$work/test_toml"
timeout 90s "$work/test_toml"
bash release/extlibs/toml/test/check_xlang.sh --skip-aot
printf 'toml Linux C ASan/UBSan and XLang format gate: PASS\n'
