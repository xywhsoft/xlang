#!/usr/bin/env sh
set -eu

if [ "${XL_REDIS_REAL:-}" != "1" ]; then
	echo "Skipping x.redis real smoke. Set XL_REDIS_REAL=1 to run against Redis."
	exit 0
fi

cd "$(dirname "$0")/../../../demo5"
out="${TMPDIR:-/tmp}/x_redis_smoke_$$.log"
set +e
release/xl.exe run release/examples/redis/redis_basic.xl > "$out" 2>&1
rc=$?
set -e
cat "$out"
run_rc="$(sed -n 's/.*RunResult exitCode=\([0-9][0-9]*\).*/\1/p' "$out" | tail -n 1)"
rm -f "$out"
if [ -n "$run_rc" ]; then
	rc="$run_rc"
fi
exit "$rc"
