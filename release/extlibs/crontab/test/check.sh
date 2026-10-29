#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-crontab-XXXXXX")
cc=${CC:-gcc}
python=${PYTHON:-python3}
cc_path=$(command -v "$cc" || true)
python_path=$(command -v "$python" || true)
sequence=0
export TZDIR=/usr/share/zoneinfo PYTHONTZPATH=/usr/share/zoneinfo TZ=UTC
input_paths() {
    find release/extlibs/crontab release/include -type f -print0
    printf '%s\0' lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so release/libxlc.so \
        test/cases/extlib_crontab.xl test/cases/extlib_crontab_boundaries.xl release/examples/extlibs/crontab.xl \
        /usr/share/zoneinfo/UTC /usr/share/zoneinfo/America/New_York \
        /usr/share/zoneinfo/Australia/Lord_Howe /usr/share/zoneinfo/Asia/Kathmandu
    if [[ -n "$cc_path" ]]; then printf '%s\0' "$cc_path"; fi
    if [[ -n "$python_path" ]]; then printf '%s\0' "$python_path"; fi
}
cleanup() {
    result=$?
    trap - EXIT
    set +e
    cd "$root" || exit 125
    mapfile -d '' after < <(input_paths | sort -zu)
    sha256sum "${after[@]}" > "$work/inputs.after.sha256"
    if ! diff -u "$work/inputs.before.sha256" "$work/inputs.after.sha256"; then result=125; fi
    printf 'exit=%s\nphases=%s\n' "$result" "$sequence" > "$work.report.txt"
    if [[ -f "$work/commands.log" ]]; then cat "$work/commands.log" >> "$work.report.txt"; fi
    cp -- "$work/inputs.before.sha256" "$work.inputs.sha256"
    if ((result == 0)); then rm -rf -- "$work" || result=$?
    else printf '[crontab] FAIL exit=%s artifacts=%s\n' "$result" "$work" >&2; fi
    printf '[crontab] report: %s.report.txt\n' "$work"
    exit "$result"
}
trap cleanup EXIT
trap 'printf "exit=%s line=%s command=%s\n" "$?" "$LINENO" "$BASH_COMMAND" > "$work/failure.txt"' ERR
mapfile -d '' inputs < <(input_paths | sort -zu)
sha256sum "${inputs[@]}" > "$work/inputs.before.sha256"
run() {
    local seconds=$1 result
    shift
    sequence=$((sequence+1))
    printf '\n$ TZ=%q timeout %ss' "$TZ" "$seconds" >> "$work/commands.log"
    printf ' %q' "$@" >> "$work/commands.log"
    printf '\n' >> "$work/commands.log"
    if timeout "${seconds}s" "$@" > "$work/$sequence.out" 2> "$work/$sequence.err"; then result=0; else result=$?; fi
    printf 'exit=%s\n' "$result" >> "$work/commands.log"
    cat "$work/$sequence.out" "$work/$sequence.err" >> "$work/commands.log"
    cat "$work/$sequence.out"
    cat "$work/$sequence.err" >&2
    return "$result"
}
run 10 "$cc" --version > "$work/compiler.txt"
run 10 "$python" --version
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt -Irelease/extlibs/crontab
       -Irelease/extlibs/crontab/include -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -pthread)
compile() { run 180 "$cc" "${flags[@]}" "$@"; }
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
export XLANG_PACKAGE_PATH="$root/release/extlibs"
compile -x c -fsyntax-only release/extlibs/crontab/include/crontab_native.h
compile -DXRT_MODULE_TIME -DXRT_MODULE_TIME_LOCAL -fsyntax-only release/extlibs/crontab/crontab_native.c
compile -c release/extlibs/crontab/test/xrt_runtime.c -o "$work/runtime.o"
compile -c release/extlibs/crontab/crontab_native.c -o "$work/native.o"
objects=("$work/native.o" "$work/runtime.o")
for name in test_crontab test_dst test_half_hour_dst test_long_horizon input_boundaries_c; do
    compile -DNDEBUG "release/extlibs/crontab/test/$name.c" "${objects[@]}" -no-pie -lm -ldl -o "$work/$name"
done
run 120 "$work/test_crontab"
TZ=America/New_York run 90 "$work/test_dst"
TZ=Australia/Lord_Howe run 90 "$work/test_half_hour_dst"
run 180 "$work/test_long_horizon"
run 90 "$work/input_boundaries_c"
# 独立Python/IANA只验证日历结果；原生边界的内存安全另由上面的sanitizer覆盖。
run 180 "$cc" -std=c11 -D_GNU_SOURCE -O2 -Wall -Wextra -Werror -fPIC -shared -pthread \
    -Ilib/xrt -Irelease/extlibs/crontab/include release/extlibs/crontab/crontab_native.c \
    release/extlibs/crontab/test/xrt_runtime.c -lm -ldl -o "$work/libcrontab.so"
run 120 "$python" release/extlibs/crontab/test/timezone_oracle.py "$work/libcrontab.so"
run 90 ./release/xl run release/extlibs/crontab/test/input_boundaries_c.c
sources=(test/cases/extlib_crontab.xl test/cases/extlib_crontab_boundaries.xl
         release/extlibs/crontab/test/allocations.xl release/extlibs/crontab/test/owned_probe.xl
         release/extlibs/crontab/test/aot_core.xl)
for source in "${sources[@]}"; do run 90 ./release/xl run "$source"; done
TZ=America/New_York run 90 ./release/xl run release/extlibs/crontab/test/historical_timezone.xl
TZ=Australia/Lord_Howe run 90 ./release/xl run release/extlibs/crontab/test/half_hour_timezone.xl
run 90 ./release/xl cgen release/extlibs/crontab/test/owned_probe.xl --emit "$work/probe.c"
if grep -Eq '= *xlir_string_copy *\(' "$work/probe.c"; then exit 1; fi
test "$(grep -Ec '[[:alnum:]_]+\.Data[[:space:]]*=[[:space:]]*xcron_describe[[:space:]]*\(' "$work/probe.c")" -eq 1
run 90 ./release/xl cgen release/examples/extlibs/crontab.xl --emit "$work/example.c"
grep -E '^extern .*xcron_[a-z_]+[(]' "$work/example.c" | sort -u > "$work/abi.decls"
grep -oE 'xcron_[a-z_]+[(]' "$work/abi.decls" | tr -d '(' | sort -u > "$work/abi.actual"
printf '%s\n' xcron_describe xcron_is_valid xcron_matches xcron_next > "$work/abi.expected"
test "$(wc -l < "$work/abi.decls")" -eq 4
diff -u "$work/abi.expected" "$work/abi.actual"
printf '#include "include/crontab_native.h"\n' > "$work/abi.c"
cat "$work/abi.decls" >> "$work/abi.c"
compile -fsyntax-only "$work/abi.c"
compile -DNDEBUG release/extlibs/crontab/examples/crontab.c "${objects[@]}" -no-pie -lm -ldl -o "$work/example"
run 90 "$work/example" > "$work/c.txt"
run 90 ./release/xl run release/extlibs/crontab/examples/crontab.c > "$work/direct.txt"
run 90 ./release/xl run release/examples/extlibs/crontab.xl > "$work/xl.txt"
diff -u "$work/c.txt" "$work/direct.txt"
diff -u "$work/c.txt" "$work/xl.txt"
for source in "${sources[@]}" release/extlibs/crontab/test/input_boundaries_c.c \
    release/extlibs/crontab/examples/crontab.c release/examples/extlibs/crontab.xl \
    release/extlibs/crontab/test/historical_timezone.xl release/extlibs/crontab/test/half_hour_timezone.xl; do
    target="$work/$(basename "${source%.*}")-$(basename "$(dirname "$source")")"
    run 180 ./release/xl build "$source" -o "$target"
    case "$source" in
        */historical_timezone.xl) TZ=America/New_York run 90 "$target" > "$target.out" ;;
        */half_hour_timezone.xl) TZ=Australia/Lord_Howe run 90 "$target" > "$target.out" ;;
        *) run 90 "$target" > "$target.out" ;;
    esac
    case "$source" in
        */examples/crontab.c|release/examples/extlibs/crontab.xl) diff -u "$work/c.txt" "$target.out" ;;
    esac
done
printf 'crontab Linux sanitizer, Python/IANA oracle, four actual prototypes and bilingual JIT/AOT: PASS\n'
