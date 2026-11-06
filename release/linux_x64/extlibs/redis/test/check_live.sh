#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
mode=${1:-}
if [[ -n $mode && $mode != --windows-xl ]]; then
    printf 'usage: check_live.sh [--windows-xl]\n' >&2
    exit 2
fi
command -v redis-server >/dev/null || { printf 'redis-server is required\n' >&2; exit 1; }
command -v redis-cli >/dev/null || { printf 'redis-cli is required\n' >&2; exit 1; }
if [[ $mode == --windows-xl ]]; then
    command -v cmd.exe >/dev/null || { printf 'WSL Windows interop is required\n' >&2; exit 1; }
    [[ -f release/xl.exe ]] || { printf 'release/xl.exe is required\n' >&2; exit 1; }
fi

work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-redis-live-XXXXXX")
pid=
binaryWork=
cleanup() {
    if [[ -n $pid ]]; then
        kill "$pid" 2>/dev/null || true
        wait "$pid" 2>/dev/null || true
    fi
    if [[ -n $binaryWork ]]; then
        case "$binaryWork" in
            "$root/build/test"/redis-live-*) ;;
            *) printf 'unexpected Redis build path: %s\n' "$binaryWork" >&2; return 1 ;;
        esac
        rm -f -- "$binaryWork/redis_live.exe"
        rmdir -- "$binaryWork"
    fi
    rm -rf -- "$work"
}
trap cleanup EXIT

ready=0
for attempt in {1..10}; do
    port=$((20000 + RANDOM))
    # 端口只用于本次实例；已有 Redis 或占用端口都不能成为测试对象。
    if redis-cli -h 127.0.0.1 -p "$port" ping 2>/dev/null | grep -Fxq PONG; then
        continue
    fi
    redis-server --bind 127.0.0.1 --port "$port" --save '' --appendonly no \
        > "$work/server.log" 2>&1 &
    pid=$!
    for probe in {1..30}; do
        if kill -0 "$pid" 2>/dev/null &&
            redis-cli --raw -h 127.0.0.1 -p "$port" INFO server 2>/dev/null |
                tr -d '\r' | grep -Fxq "process_id:$pid"; then
            ready=1
            break
        fi
        if ! kill -0 "$pid" 2>/dev/null; then break; fi
        sleep 0.1
    done
    if [[ $ready == 1 ]]; then break; fi
    kill "$pid" 2>/dev/null || true
    wait "$pid" 2>/dev/null || true
    pid=
done
if [[ $ready != 1 ]] || ! kill -0 "$pid" 2>/dev/null; then
    cat "$work/server.log" >&2
    printf 'temporary Redis did not start\n' >&2
    exit 1
fi

if [[ $mode == --windows-xl ]]; then
    export REDIS_LIVE_PORT="$port"
    export WSLENV="${WSLENV:+$WSLENV:}REDIS_LIVE_PORT/w"
    cd "$root/release"
    ./xl.exe run examples/extlibs/redis_live.xl |
        tr -d '\r' > "$work/example_xl.txt"
    ./xl.exe run examples/extlibs/redis_live_c.c |
        tr -d '\r' > "$work/example_c.txt"
    diff -u "$work/example_c.txt" "$work/example_xl.txt"
    [[ $(<"$work/example_xl.txt") == $'PING: PONG\nGET: hello\nEXISTS: 1\nINCR: 1\nEXISTS after DEL: 0' ]] || {
        printf 'Redis live example output differs from fixed expectation\n' >&2
        exit 1
    }
    mkdir -p "$root/build/test"
    binaryWork=$(mktemp -d "$root/build/test/redis-live-XXXXXX")
    ./xl.exe build examples/extlibs/redis_live.xl -o \
        "../build/test/$(basename "$binaryWork")/redis_live.exe"
    "$binaryWork/redis_live.exe" | tr -d '\r' > "$work/example_aot.txt"
    diff -u "$work/example_xl.txt" "$work/example_aot.txt"
    [[ $(./xl.exe run extlibs/redis/test/live_server.xl) == 'redis real server: ok' ]]
    [[ $(./xl.exe run extlibs/redis/test/live_server_c.c \
        extlibs/redis/redis_native.c | tr -d '\r') == 'redis real server C: ok' ]]
    [[ $(./xl.exe run extlibs/redis/test/concurrent_server.xl | tr -d '\r') == \
        'redis concurrent server: 50 rounds ok' ]]
    if [[ ${REDIS_STRESS:-0} == 1 ]]; then
        [[ $(./xl.exe run extlibs/redis/test/concurrent_server.xl -- 10000 |
            tr -d '\r') == 'redis concurrent server: 10000 rounds ok' ]]
        printf 'redis Windows live stress (4 x 10000): PASS\n'
    fi
    printf 'redis Windows xl.exe / WSL live server gate: PASS\n'
else
    REDIS_LIVE=1 REDIS_LIVE_PORT="$port" bash release/extlibs/redis/test/check.sh
fi
