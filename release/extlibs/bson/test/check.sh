#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-bson-XXXXXX")
cc=${CC:-gcc}
cc_path=$(command -v "$cc" || true)
sequence=0
input_paths() {
    find release/extlibs/bson release/include -type f -print0
    printf '%s\0' lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so release/libxlc.so \
        test/cases/extlib_bson.xl release/examples/extlibs/bson.xl
    if [[ -n "$cc_path" ]]; then printf '%s\0' "$cc_path"; fi
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
    else printf '[bson] FAIL exit=%s artifacts=%s\n' "$result" "$work" >&2; fi
    printf '[bson] report: %s.report.txt\n' "$work"
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
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt -Irelease/extlibs/bson
       -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -pthread)
compile() { run 180 "$cc" "${flags[@]}" "$@"; }
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
export XLANG_PACKAGE_PATH="$root/release/extlibs"
compile -fsyntax-only release/extlibs/bson/test/abi_contract.c
compile -x c -fsyntax-only release/extlibs/bson/bson_native.h
compile -DXRT_MODULE_VALUE_CONTAINER -DXRT_MODULE_FILE_WHOLE -DXRT_MODULE_TIME -DXRT_MODULE_UNICODE \
    -DXRT_MODULE_RANDOM_SECURE -fsyntax-only release/extlibs/bson/bson_native.c
compile -c release/extlibs/bson/test/xrt_runtime.c -o "$work/runtime.o"
compile -c release/extlibs/bson/bson_native.c -o "$work/native.o"
for name in test_bson_smoke test_boundaries numeric_reads_c; do
    objects=("$work/runtime.o"); links=(); arguments=()
    if [[ $name != numeric_reads_c ]]; then objects+=("$work/native.o"); fi
    if [[ $name == test_boundaries ]]; then links+=(-Wl,--wrap=clock_gettime); arguments=("$work"); fi
    compile "release/extlibs/bson/test/$name.c" "${objects[@]}" "${links[@]}" -no-pie -lm -ldl -o "$work/$name"
    run 120 "$work/$name" "${arguments[@]}" > "$work/$name.txt"
    cat "$work/$name.txt"
done
printf 'bson numeric reads: 1080 vectors, 72 OOM paths, 0 failures\n' > "$work/numeric.expected"
diff -u "$work/numeric.expected" "$work/numeric_reads_c.txt"
numeric=release/extlibs/bson/test/numeric_reads_c.c
run 120 ./release/xl run "$numeric" > "$work/numeric.direct"
diff -u "$work/numeric.expected" "$work/numeric.direct"
for name in bson numeric_fields; do
    golden=$name
    if [[ $name == bson ]]; then golden=basic; fi
    sed 's/\r$//' "release/extlibs/bson/examples/$golden.expected.txt" > "$work/$name.expected"
    source="release/extlibs/bson/examples/${name}_c.c"
    compile "$source" "$work/runtime.o" -no-pie -lm -ldl -o "$work/$name"
    run 90 "$work/$name" > "$work/$name.c.txt"
    run 90 ./release/xl run "$source" > "$work/$name.direct.txt"
    diff -u "$work/$name.expected" "$work/$name.c.txt"
    diff -u "$work/$name.expected" "$work/$name.direct.txt"
done
if run 30 "$work/bson" extra extra; then exit 1; else test "$?" -eq 1; fi
if run 30 "$work/bson" "$work/missing/output.bson"; then exit 1; else test "$?" -eq 1; fi
run 90 "$work/bson" "$work/c.bson" > "$work/c-file.txt"
{ cat "$work/bson.expected"; printf 'file=ok\n'; } > "$work/file.expected"
diff -u "$work/file.expected" "$work/c-file.txt"
language=release/extlibs/bson/examples/numeric_fields.xl
run 90 ./release/xl run "$language" > "$work/numeric_fields.xl.txt"
diff -u "$work/numeric_fields.expected" "$work/numeric_fields.xl.txt"
paths=release/extlibs/bson/test/numeric_reads.xl
run 120 ./release/xl run "$paths"
run 90 ./release/xl cgen "$language" --emit "$work/probe.c"
grep -Eq 'xl6_xrt_take_cstr\(' "$work/probe.c"
grep -E '^extern .*xbson_[a-z_]+[(]' "$work/probe.c" | sort -u > "$work/abi.decls"
grep -oE 'xbson_[a-z_]+[(]' "$work/abi.decls" | tr -d '(' | sort -u > "$work/abi.actual"
sed -nE 's/^extern "(xbson_[A-Za-z0-9_]+)".*/\1/p' release/extlibs/bson/src/bson.xl | sort -u > "$work/abi.expected"
test "$(wc -l < "$work/abi.decls")" -eq 15
diff -u "$work/abi.expected" "$work/abi.actual"
printf '#include "bson_native.c"\n' > "$work/abi.c"
cat "$work/abi.decls" >> "$work/abi.c"
compile -fsyntax-only "$work/abi.c"
sources=("$numeric" "$paths" "$language" release/extlibs/bson/examples/bson_c.c release/extlibs/bson/examples/numeric_fields_c.c)
index=0
for source in "${sources[@]}"; do
    index=$((index+1))
    binary="$work/aot-$index"
    run 120 ./release/xl build "$source" -o "$binary"
    run 90 "$binary" > "$work/aot-$index.txt"
    case "$source" in
        */numeric_reads_c.c) diff -u "$work/numeric.expected" "$work/aot-$index.txt" ;;
        */bson_c.c) diff -u "$work/bson.expected" "$work/aot-$index.txt" ;;
        */numeric_fields_c.c|*/numeric_fields.xl) diff -u "$work/numeric_fields.expected" "$work/aot-$index.txt" ;;
    esac
done
# 原始语言字典编码、完整范例和双语文件比较保留为必过正例，不降为预期失败。
for source in test/cases/extlib_bson.xl release/extlibs/bson/test/aot_core.xl \
    release/extlibs/bson/test/boundaries.xl release/extlibs/bson/test/bson_test.xl \
    release/extlibs/bson/test/allocations.xl release/extlibs/bson/examples/bson.xl release/examples/extlibs/bson.xl; do
    run 120 ./release/xl run "$source" > "$work/complete.txt"
    if [[ $source == release/extlibs/bson/examples/bson.xl ]]; then diff -u "$work/bson.expected" "$work/complete.txt"; fi
    run 120 ./release/xl build "$source" -o "$work/complete"
    run 90 "$work/complete"
done
run 120 ./release/xl run release/extlibs/bson/examples/bson.xl -- "$work/xlang.bson" > "$work/xl-file.txt"
diff -u "$work/file.expected" "$work/xl-file.txt"
cmp "$work/c.bson" "$work/xlang.bson"
printf 'bson Linux sanitizer, numeric framing, actual ABI and bilingual JIT/AOT: PASS\n'
