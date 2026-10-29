#!/usr/bin/env bash
set -euo pipefail

if (( $# != 1 )); then
    printf 'usage: %s TOML_TEST_DIR\n' "$0" >&2
    exit 2
fi
corpus=$(realpath -- "$1")
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-toml-conformance-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cd "$root"

"${CC:-gcc}" -std=c11 -D_GNU_SOURCE -DXRT_IMPLEMENTATION -O0 \
    -Wall -Wextra -Werror -Ilib/xrt \
    release/extlibs/toml/test/conformance_syntax.c \
    -lm -ldl -pthread -o "$work/probe"
python3 release/extlibs/toml/test/check_conformance.py "$work/probe" "$corpus"
