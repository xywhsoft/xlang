#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
mode=${1:-}
if [[ -n $mode && $mode != --windows-xl ]]; then
    printf 'usage: check_live.sh [--windows-xl]\n' >&2
    exit 2
fi
command -v memcached >/dev/null || { printf 'memcached is required\n' >&2; exit 1; }
command -v ss >/dev/null || { printf 'ss is required\n' >&2; exit 1; }
if [[ $mode == --windows-xl ]]; then
    [[ -f release/xl.exe ]] || { printf 'release/xl.exe is required\n' >&2; exit 1; }
else
    [[ -x release/xl ]] || { printf 'release/xl is required\n' >&2; exit 1; }
fi

work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-memcached-live-XXXXXX")
pid=
binaryWork=
cleanup() {
    if [[ -n $pid ]]; then
        kill "$pid" 2>/dev/null || true
        wait "$pid" 2>/dev/null || true
    fi
    if [[ -n $binaryWork ]]; then
        case "$binaryWork" in
            "$root/build/test"/memcached-live-*) ;;
            *) printf 'unexpected Memcached build path: %s\n' "$binaryWork" >&2; return 1 ;;
        esac
        rm -f -- "$binaryWork/live_xl.exe" "$binaryWork/live_c.exe"
        rmdir -- "$binaryWork"
    fi
    case "$work" in
        "${TMPDIR:-/tmp}"/xlang-memcached-live-*) rm -rf -- "$work" ;;
        *) printf 'unexpected Memcached test path: %s\n' "$work" >&2; return 1 ;;
    esac
}
trap cleanup EXIT

ready=0
for attempt in {1..10}; do
    port=$((20000 + RANDOM))
    memcached -u "$(id -un)" -l 127.0.0.1 -p "$port" -U 0 -m 16 -c 64 \
        > "$work/server.log" 2>&1 &
    pid=$!
    for probe in {1..30}; do
        if kill -0 "$pid" 2>/dev/null &&
            ss -H -ltnp "sport = :$port" | grep -Fq "pid=$pid,"; then
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
if [[ $ready != 1 ]]; then
    cat "$work/server.log" >&2
    printf 'temporary Memcached did not start\n' >&2
    exit 1
fi

export MEMCACHED_LIVE_PORT="$port"
sed 's/\r$//' release/extlibs/memcached/examples/live.expected.txt > "$work/expected.txt"
if [[ $mode == --windows-xl ]]; then
    export WSLENV="${WSLENV:+$WSLENV:}MEMCACHED_LIVE_PORT/w"
    cd release
    timeout 90s ./xl.exe run extlibs/memcached/examples/live.xl | tr -d '\r' > "$work/xl.txt"
    timeout 90s ./xl.exe run extlibs/memcached/examples/live_c.c \
        extlibs/memcached/memcached_request.c \
        extlibs/memcached/memcached_response.c \
        --inc-path ../lib/xrt | tr -d '\r' > "$work/c.txt"
    mkdir -p "$root/build/test"
    binaryWork=$(mktemp -d "$root/build/test/memcached-live-XXXXXX")
    timeout 180s ./xl.exe build extlibs/memcached/examples/live.xl -o \
        "../build/test/$(basename "$binaryWork")/live_xl.exe"
    timeout 30s "$binaryWork/live_xl.exe" | tr -d '\r' > "$work/xl_aot.txt"
    timeout 180s ./xl.exe build extlibs/memcached/examples/live_c.c \
        extlibs/memcached/memcached_request.c \
        extlibs/memcached/memcached_response.c \
        --inc-path ../lib/xrt -o "../build/test/$(basename "$binaryWork")/live_c.exe"
    timeout 30s "$binaryWork/live_c.exe" | tr -d '\r' > "$work/c_aot.txt"
else
    timeout 90s release/xl run release/extlibs/memcached/examples/live.xl > "$work/xl.txt"
    timeout 90s release/xl run release/extlibs/memcached/examples/live_c.c \
        release/extlibs/memcached/memcached_request.c \
        release/extlibs/memcached/memcached_response.c \
        --inc-path lib/xrt > "$work/c.txt"
    timeout 180s release/xl build release/extlibs/memcached/examples/live.xl \
        -o "$work/live_xl"
    timeout 30s "$work/live_xl" > "$work/xl_aot.txt"
    timeout 180s release/xl build release/extlibs/memcached/examples/live_c.c \
        release/extlibs/memcached/memcached_request.c \
        release/extlibs/memcached/memcached_response.c \
        --inc-path lib/xrt -o "$work/live_c"
    timeout 30s "$work/live_c" > "$work/c_aot.txt"
fi
diff -u "$work/expected.txt" "$work/xl.txt"
diff -u "$work/expected.txt" "$work/c.txt"
diff -u "$work/expected.txt" "$work/xl_aot.txt"
diff -u "$work/expected.txt" "$work/c_aot.txt"
if [[ ${MEMCACHED_STRESS:-0} == 1 ]]; then
    if [[ $mode == --windows-xl ]]; then
        [[ $(timeout 180s ./xl.exe run extlibs/memcached/test/concurrent_server.xl -- 1000 |
            tr -d '\r') == 'memcached concurrent server: 1000 rounds ok' ]]
    else
        [[ $(timeout 180s release/xl run \
            release/extlibs/memcached/test/concurrent_server.xl -- 1000) == \
            'memcached concurrent server: 1000 rounds ok' ]]
    fi
    printf 'memcached live stress (4 x 1000): PASS\n'
fi
printf 'memcached real server C/XLang JIT/AOT (%s): PASS\n' "${mode:---linux-xl}"
