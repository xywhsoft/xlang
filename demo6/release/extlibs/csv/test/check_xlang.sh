#!/usr/bin/env bash
set -euo pipefail

skip_aot=false
if (( $# != 0 )); then
    if (( $# != 1 )) || [[ $1 != --skip-aot ]]; then
        printf 'usage: %s [--skip-aot]\n' "$0" >&2
        exit 2
    fi
    skip_aot=true
fi

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root/release"
if [[ ! -x ./xl ]]; then
    printf 'build Linux release/xl before running this gate\n' >&2
    exit 1
fi

work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-csv-xl-XXXXXX")
trap 'rm -rf -- "$work"' EXIT

./xl run ../test/cases/extlib_csv.xl
./xl run extlibs/csv/test/boundaries.xl
./xl run extlibs/csv/test/files.xl
if ./xl check extlibs/csv/test/private_native.xl > "$work/private_native.txt" 2>&1; then
    printf 'CSV Native methods must not be public\n' >&2
    exit 1
fi
if ! grep -Fq 'fieldNative' "$work/private_native.txt" ||
   ! grep -Fq 'access' "$work/private_native.txt"; then
    cat "$work/private_native.txt" >&2
    printf 'CSV expected private-access diagnostic\n' >&2
    exit 1
fi
./xl run examples/extlibs/csv.xl > "$work/full.txt"
for name in basic files; do
    ./xl run "examples/extlibs/csv_${name}.xl" > "$work/${name}.txt"
    diff -u "examples/extlibs/csv_${name}.expected.txt" "$work/${name}.txt"
done

if [[ $skip_aot == false ]]; then
    ./xl build extlibs/csv/test/aot_core.xl -o "$work/aot_core"
    "$work/aot_core"
    printf 'CSV Linux XLang JIT and core AOT gate: PASS\n'
else
    printf 'CSV Linux XLang JIT gate: PASS (AOT skipped)\n'
fi
