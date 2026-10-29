#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-redis-XXXXXX")
cleanup() {
    result=$?
    trap - EXIT
    set +e
    if ((result == 0)); then
        rm -rf -- "$work" || result=$?
    else
        printf '[redis] FAIL exit=%s artifacts=%s\n' "$result" "$work" >&2
    fi
    exit "$result"
}
trap cleanup EXIT
record_failure() {
    printf 'exit=%s\nline=%s\ncommand=%s\npipeline=%s\n' "$@" > "$work/failure.txt"
    cat "$work/failure.txt" >&2
}
trap 'record_failure "$?" "$LINENO" "$BASH_COMMAND" "${PIPESTATUS[*]}"' ERR

cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror
       -Ilib/xrt -Irelease/extlibs/redis -fsanitize=address,undefined
       -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

# 固定包输入和实际 CLI/运行库，失败时连同编译器版本保留现场。
mapfile -d '' inputs < <(
    { find release/extlibs/redis -type f -print0
      printf '%s\0' lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxlc.so \
          release/include/xrt_decl.h release/include/xruntime_decl.h release/include/xl_extlib_abi.h \
          test/cases/extlib_redis.xl release/examples/extlibs/redis.xl \
          release/examples/extlibs/redis_c.c release/examples/extlibs/redis_live.xl \
          release/examples/extlibs/redis_live_c.c
    } | sort -zu
)
sha256sum "${inputs[@]}" > "$work/inputs.before.sha256"
"$cc" --version > "$work/compiler.txt"

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
"$cc" "${flags[@]}" release/extlibs/redis/test/test_example_lifetime.c \
    "$work/xrt_runtime.o" -lm -ldl -o "$work/example_lifetime"
timeout 90s "$work/example_lifetime"
"$cc" "${flags[@]}" release/examples/extlibs/redis_c.c \
    "$work/xrt_runtime.o" -lm -ldl -o "$work/example"
timeout 30s "$work/example" > "$work/c.txt"
timeout 30s ./release/xl run release/examples/extlibs/redis_c.c > "$work/c_direct.txt"
diff -u "$work/c.txt" "$work/c_direct.txt"
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
timeout 120s ./release/xl build release/extlibs/redis/test/aot_core.xl \
    -o "$work/redis_core"
timeout 45s "$work/redis_core"
printf '[redis] Linux core AOT: PASS\n'
for name in preflight_repro command_loopback reuse_loopback; do
    printf '[redis] Linux %s AOT build\n' "$name"
    build_timeout=120s
    if [[ $name == command_loopback || $name == reuse_loopback ]]; then
        build_timeout=600s
    fi
    if timeout "$build_timeout" ./release/xl build "release/extlibs/redis/test/$name.xl" \
        -o "$work/redis_$name"; then
        :
    else
        status=$?
        printf '[redis] Linux %s AOT build exited %s\n' "$name" "$status" >&2
        exit "$status"
    fi
    if timeout 90s "$work/redis_$name" > "$work/$name.txt" \
        2> "$work/$name.err"; then
        printf '[redis] Linux %s AOT ran\n' "$name"
    else
        status=$?
        cat "$work/$name.err" >&2
        cat "$work/$name.txt" >&2
        printf '[redis] Linux %s AOT exited %s\n' "$name" "$status" >&2
        exit "$status"
    fi
done
diff -u <(printf 'redis preflight: ok\n') "$work/preflight_repro.txt"
diff -u <(printf 'redis command loopback: ok\n') "$work/command_loopback.txt"
diff -u <(printf 'redis reuse loopback: 500\n') "$work/reuse_loopback.txt"
timeout 240s ./release/xl build release/examples/extlibs/redis.xl \
    -o "$work/redis_example"
timeout 45s "$work/redis_example" > "$work/example_aot.txt"
diff -u "$work/c.txt" "$work/example_aot.txt"
timeout 240s ./release/xl build release/examples/extlibs/redis_c.c \
    -o "$work/redis_c_example"
timeout 45s "$work/redis_c_example" > "$work/c_example_aot.txt"
diff -u "$work/c.txt" "$work/c_example_aot.txt"
if [[ ${REDIS_LIVE:-0} == 1 ]]; then
    timeout 240s ./release/xl build release/examples/extlibs/redis_live.xl \
        -o "$work/redis_live_example"
    timeout 45s "$work/redis_live_example" > "$work/live_aot.txt"
    diff -u "$work/live_c.txt" "$work/live_aot.txt"
fi
sha256sum "${inputs[@]}" > "$work/inputs.after.sha256"
diff -u "$work/inputs.before.sha256" "$work/inputs.after.sha256"
printf 'redis Linux C ASan/UBSan, XLang JIT/AOT and parity gate: PASS\n'
