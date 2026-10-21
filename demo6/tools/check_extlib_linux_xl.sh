#!/usr/bin/env bash
set -euo pipefail

package=${1:-}
case "$package" in
    blake2|blake3|xxhash) ;;
    *) printf 'usage: %s blake2|blake3|xxhash\n' "$0" >&2; exit 2 ;;
esac

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"
test -x ./release/xl

work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-${package}-xl-XXXXXX")
trap 'rm -rf -- "$work"' EXIT

for name in "extlib_$package" "extlib_${package}_boundaries" "extlib_${package}_progress"; do
    timeout 90s ./release/xl run "test/cases/$name.xl"
done
timeout 90s ./release/xl run "release/examples/extlibs/${package}_basic.xl" > "$work/output.txt"
diff -u "release/examples/extlibs/${package}_basic.expected.txt" "$work/output.txt"
printf '%s Linux XLang JIT gate: PASS\n' "$package"
