#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-graph-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
export GRAPH_WORK_DIR="$work"

bash tools/check_extlib_graph.sh
timeout 45s ./release/xl run release/extlibs/graph/test/boundaries.xl
output=$(timeout 45s ./release/xl run release/examples/extlibs/graph_basic.xl)
diff -u <(tr -d '\r' < release/examples/extlibs/graph_basic.expected.txt) \
    <(printf '%s\n' "$output")
diff -u "$work/basic.log" <(printf '%s\n' "$output")
printf 'graph Linux C/XLang gate: PASS\n'
