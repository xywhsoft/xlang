#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-redis-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror
       -Ilib/xrt -Irelease/extlibs/redis -fsanitize=address,undefined
       -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

"$cc" "${flags[@]}" -DXRT_MODULE_THREAD -DXRT_MODULE_MEMORY_DEBUG \
    -c release/extlibs/redis/test/live_runtime.c \
    -o "$work/xrt_runtime.o"
"$cc" "${flags[@]}" -c release/extlibs/redis/redis_native.c \
    -o "$work/redis_native.o"
for name in test_redis test_redis_alloc test_redis_threads; do
    "$cc" "${flags[@]}" "release/extlibs/redis/test/$name.c" \
        "$work/redis_native.o" "$work/xrt_runtime.o" -lm -ldl -o "$work/$name"
    timeout 90s "$work/$name"
done
"$cc" "${flags[@]}" release/examples/extlibs/redis_c.c \
    "$work/xrt_runtime.o" -lm -ldl -o "$work/example"
timeout 30s "$work/example" > "$work/c.txt"
if [[ ${REDIS_LIVE:-0} == 1 ]]; then
    "$cc" "${flags[@]}" release/examples/extlibs/redis_live_c.c \
        "$work/xrt_runtime.o" -lm -ldl -o "$work/live_example_c"
    timeout 30s "$work/live_example_c" > "$work/live_c.txt"
    "$cc" "${flags[@]}" release/extlibs/redis/test/live_server_c.c \
        "$work/redis_native.o" "$work/xrt_runtime.o" \
        -lm -ldl -o "$work/live_server_c"
    timeout 60s "$work/live_server_c"
    printf 'redis live server C gate: PASS\n'
fi
timeout 60s ./release/xl run release/extlibs/redis/test/boundaries.xl
timeout 60s ./release/xl run release/extlibs/redis/test/allocations.xl
timeout 60s ./release/xl run release/extlibs/redis/test/preflight_repro.xl
timeout 60s ./release/xl run test/cases/extlib_redis.xl
timeout 90s ./release/xl run release/extlibs/redis/test/command_loopback.xl
timeout 60s ./release/xl run release/extlibs/redis/test/reuse_loopback.xl
timeout 60s ./release/xl run release/examples/extlibs/redis.xl > "$work/xl.txt"
diff -u "$work/c.txt" "$work/xl.txt"
if [[ ${REDIS_LIVE:-0} == 1 ]]; then
    (cd release && timeout 30s ./xl run examples/extlibs/redis_live.xl) > "$work/live_xl.txt"
    (cd release && timeout 30s ./xl run examples/extlibs/redis_live_c.c) > "$work/live_tcc.txt"
    diff -u "$work/live_c.txt" "$work/live_xl.txt"
    diff -u "$work/live_c.txt" "$work/live_tcc.txt"
    (cd release && timeout 60s ./xl run extlibs/redis/test/live_server.xl)
    if [[ ${REDIS_STRESS:-0} == 1 ]]; then
        (cd release && timeout 180s ./xl run extlibs/redis/test/concurrent_server.xl -- 10000)
    else
        (cd release && timeout 60s ./xl run extlibs/redis/test/concurrent_server.xl)
    fi
    printf 'redis live server C/XLang gate: PASS\n'
fi
printf 'redis Linux ASan/UBSan gate: PASS\n'
