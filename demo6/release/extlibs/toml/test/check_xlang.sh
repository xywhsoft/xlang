#!/usr/bin/env bash
set -euo pipefail

skip_aot=false
if [[ ${1:-} == --skip-aot && $# -eq 1 ]]; then
    skip_aot=true
elif (( $# != 0 )); then
    printf 'usage: %s [--skip-aot]\n' "$0" >&2
    exit 2
fi

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root/release"
if [[ ! -x ./xl ]]; then
    printf 'build Linux release/xl before running this gate\n' >&2
    exit 1
fi

work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-toml-xl-XXXXXX")
trap 'rm -rf -- "$work"' EXIT

./xl run ../test/cases/extlib_toml.xl
./xl run extlibs/toml/test/boundaries.xl
./xl run extlibs/toml/test/allocations.xl
./xl run examples/extlibs/toml.xl > "$work/xlang.txt"
./xl run examples/extlibs/toml_c.c > "$work/c.txt"
diff -u "$work/c.txt" "$work/xlang.txt"

if ! command -v python3 >/dev/null || ! python3 -c 'import tomllib' >/dev/null 2>&1; then
    printf 'Python 3.11+ with tomllib is required for the TOML format gate\n' >&2
    exit 1
fi
./xl run examples/extlibs/toml.xl -- "$work/xlang.toml" > "$work/xlang_file.txt"
./xl run examples/extlibs/toml_c.c -- "$work/c.toml" > "$work/c_file.txt"
diff -u "$work/c_file.txt" "$work/xlang_file.txt"
python3 extlibs/toml/test/check_external.py "$work/c.toml" "$work/xlang.toml"

if [[ $skip_aot == false ]]; then
    ./xl build extlibs/toml/test/aot_core.xl -o "$work/aot_core"
    "$work/aot_core"
fi
printf 'toml Linux XLang/C JIT and external format gate: PASS\n'
