#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
command -v redis-server >/dev/null || { printf 'redis-server is required\n' >&2; exit 1; }
command -v redis-cli >/dev/null || { printf 'redis-cli is required\n' >&2; exit 1; }

work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-redis-live-XXXXXX")
pid=
cleanup() {
    if [[ -n $pid ]]; then
        kill "$pid" 2>/dev/null || true
        wait "$pid" 2>/dev/null || true
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

REDIS_LIVE=1 REDIS_LIVE_PORT="$port" bash release/extlibs/redis/test/check.sh
