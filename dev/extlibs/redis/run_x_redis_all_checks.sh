#!/usr/bin/env sh
set -eu

root="$(cd "$(dirname "$0")/../../.." && pwd)"

cd "$root"
node dev/extlibs/redis/check_x_redis_exports.js

cd "$root/demo5"
release/xl.exe build release/examples/redis/redis_basic.xl --emit release/tmp/redis_basic.exe

out="${TMPDIR:-/tmp}/x_redis_offline_$$.log"
set +e
release/xl.exe run release/examples/redis/redis_offline_codec.xl > "$out" 2>&1
rc=$?
set -e
cat "$out"
run_rc="$(sed -n 's/.*RunResult exitCode=\([0-9][0-9]*\).*/\1/p' "$out" | tail -n 1)"
rm -f "$out"
if [ -n "$run_rc" ]; then
	rc="$run_rc"
fi
if [ "$rc" != "0" ]; then
	exit "$rc"
fi

cd "$root"
demo5/release/xl5_test.exe runtime.redis_offline_codec
sh dev/extlibs/redis/run_x_redis_fake_smoke.sh
