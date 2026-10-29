#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-netrc-XXXXXX")
cc=${CC:-gcc}; python=${PYTHON:-python3}
cc_path=$(command -v "$cc" || true); python_path=$(command -v "$python" || true)
sequence=0
input_paths() {
    find release/extlibs/netrc release/include -type f -print0
    printf '%s\0' lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so release/libxlc.so \
        test/cases/extlib_netrc.xl release/examples/extlibs/netrc.xl release/examples/extlibs/netrc_c.c
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
    else printf '[netrc] FAIL exit=%s artifacts=%s\n' "$result" "$work" >&2; fi
    printf '[netrc] report: %s.report.txt\n' "$work"
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
    printf '\n$ timeout %ss' "$seconds" >> "$work/commands.log"
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
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt -Irelease/extlibs/netrc
       -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -pthread)
compile() { run 180 "$cc" "${flags[@]}" "$@"; }
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
export PYTHONDONTWRITEBYTECODE=1
export XLANG_PACKAGE_PATH="$root/release/extlibs"
compile -x c -fsyntax-only release/extlibs/netrc/include/xrt_netrc.h
modules=(-DXRT_MODULE_BUFFER -DXRT_MODULE_FILE_WHOLE -DXRT_MODULE_IO_BUFFER -DXRT_MODULE_IO_FILE
         -DXRT_MODULE_ONCE -DXRT_MODULE_THREAD_KEY -DXRT_MODULE_UNICODE -DXRT_MODULE_VALUE_CONTAINER)
for source in xrt_netrc_common xrt_netrc_parser xrt_netrc_writer netrc_native; do
    compile "${modules[@]}" -fsyntax-only "release/extlibs/netrc/$source.c"
done
compile -c release/extlibs/netrc/test/xrt_runtime.c -o "$work/runtime.o"
for name in test_netrc test_netrc_boundaries test_native_tls resource_limits_c stream_errors_c; do
    compile "release/extlibs/netrc/test/$name.c" "$work/runtime.o" -no-pie -lm -ldl -o "$work/$name"
    pushd "$work" >/dev/null
    run 120 "$work/$name" > "$work/$name.txt"
    popd >/dev/null
    cat "$work/$name.txt"
done
printf 'netrc resource budgets: 285 checks, 0 failures\n' > "$work/resources.expected"
printf 'netrc stream first cause: 126 checks, 0 failures\n' > "$work/streams.expected"
diff -u "$work/resources.expected" "$work/resource_limits_c.txt"
diff -u "$work/streams.expected" "$work/stream_errors_c.txt"
for pair in 'resource_limits_c resources' 'stream_errors_c streams'; do
    read -r name expected <<< "$pair"
    run 120 ./release/xl run "release/extlibs/netrc/test/$name.c" > "$work/$name.direct"
    diff -u "$work/$expected.expected" "$work/$name.direct"
done
for name in basic bounded_credentials; do
    c=release/extlibs/netrc/examples/basic.c; language=release/examples/extlibs/netrc.xl
    if [[ $name != basic ]]; then
        c=release/extlibs/netrc/examples/bounded_credentials_c.c
        language=release/extlibs/netrc/examples/bounded_credentials.xl
    fi
    sed 's/\r$//' "release/extlibs/netrc/examples/$name.expected.txt" > "$work/$name.expected"
    compile "$c" "$work/runtime.o" -no-pie -lm -ldl -o "$work/$name"
    run 90 "$work/$name" > "$work/$name.c.txt"
    run 90 ./release/xl run "$c" > "$work/$name.direct.txt"
    run 90 ./release/xl run "$language" > "$work/$name.xl.txt"
    diff -u "$work/$name.expected" "$work/$name.c.txt"
    diff -u "$work/$name.expected" "$work/$name.direct.txt"
    diff -u "$work/$name.expected" "$work/$name.xl.txt"
done
run 90 ./release/xl run release/examples/extlibs/netrc_c.c > "$work/central.c.txt"
diff -u "$work/basic.expected" "$work/central.c.txt"
run 90 "$work/basic" "$work/c.netrc" > "$work/file.c.txt"
run 90 ./release/xl run release/examples/extlibs/netrc.xl -- "$work/xl.netrc" > "$work/file.xl.txt"
diff -u "$work/basic.expected" "$work/file.c.txt"
diff -u "$work/basic.expected" "$work/file.xl.txt"
cmp "$work/c.netrc" "$work/xl.netrc"
[[ $(stat -c %a "$work/c.netrc") == 600 && $(stat -c %a "$work/xl.netrc") == 600 ]]
run 30 "$python" release/extlibs/netrc/test/interop.py "$work/c.netrc" "$work/xl.netrc"
owned=release/extlibs/netrc/test/owned_text.xl
run 90 ./release/xl run "$owned"
run 90 ./release/xl cgen release/examples/extlibs/netrc.xl --emit "$work/probe.c"
grep -Eq 'extern char\* xnetrc_error_message\(' "$work/probe.c"
grep -Eq 'v[0-9]+\.Data[[:space:]]*=[[:space:]]*xnetrc_error_message[[:space:]]*\(' "$work/probe.c"
grep -Eq 'xl6_xrt_take_cstr\(' "$work/probe.c"
grep -E '^extern .*xnetrc_[a-z_]+[(]' "$work/probe.c" | sort -u > "$work/abi.decls"
grep -oE 'xnetrc_[a-z_]+[(]' "$work/abi.decls" | tr -d '(' | sort -u > "$work/abi.actual"
sed -nE 's/^extern "(xnetrc_[a-z_]+)".*/\1/p' release/extlibs/netrc/src/netrc.xl | sort -u > "$work/abi.expected"
test "$(wc -l < "$work/abi.decls")" -eq 9
test "$(wc -l < "$work/abi.expected")" -eq 9
diff -u "$work/abi.expected" "$work/abi.actual"
printf '#include "netrc_native.c"\n' > "$work/abi.c"
cat "$work/abi.decls" >> "$work/abi.c"
compile -fsyntax-only "$work/abi.c"
languages=(test/cases/extlib_netrc.xl release/extlibs/netrc/test/boundaries.xl
           release/extlibs/netrc/test/resource_limits.xl release/extlibs/netrc/test/allocations.xl "$owned")
for source in "${languages[@]}"; do run 120 ./release/xl run "$source"; done
sources=(release/extlibs/netrc/test/resource_limits_c.c release/extlibs/netrc/test/stream_errors_c.c
         release/examples/extlibs/netrc_c.c "${languages[@]}" release/extlibs/netrc/examples/basic.c
         release/examples/extlibs/netrc.xl release/extlibs/netrc/examples/bounded_credentials_c.c
         release/extlibs/netrc/examples/bounded_credentials.xl)
index=0
for source in "${sources[@]}"; do
    index=$((index+1))
    binary="$work/aot-$index"
    run 120 ./release/xl build "$source" -o "$binary"
    run 90 "$binary" > "$work/aot-$index.txt"
    expected=''
    case "$source" in
        */resource_limits_c.c) expected=resources ;;
        */stream_errors_c.c) expected=streams ;;
        */basic.c|release/examples/extlibs/netrc.xl|release/examples/extlibs/netrc_c.c) expected=basic ;;
        */bounded_credentials_c.c|*/bounded_credentials.xl) expected=bounded_credentials ;;
    esac
    if [[ -n "$expected" ]]; then diff -u "$work/$expected.expected" "$work/aot-$index.txt"; fi
done
# 完整语言字典构造仍必须通过；不能删除 M1 正例或改为预期失败。
named=release/extlibs/netrc/test/named_type.xl
run 120 ./release/xl run "$named"
run 120 ./release/xl build "$named" -o "$work/named"
run 90 "$work/named"
printf 'netrc Linux sanitizer, resource/first-cause matrix, ABI and bilingual JIT/AOT: PASS\n'
