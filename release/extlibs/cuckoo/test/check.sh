#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-cuckoo-XXXXXX")
cc=${CC:-gcc}
cc_path=$(command -v "$cc" || true)
sequence=0
input_paths() {
    find release/extlibs/cuckoo release/include -type f -print0
    printf '%s\0' lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so release/libxlc.so \
        test/cases/extlib_cuckoo.xl test/cases/extlib_cuckoo_boundaries.xl release/examples/extlibs/cuckoo.xl
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
    else printf '[cuckoo] FAIL exit=%s artifacts=%s\n' "$result" "$work" >&2; fi
    printf '[cuckoo] report: %s.report.txt\n' "$work"
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
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt -Irelease/extlibs/cuckoo
       -Irelease/extlibs/cuckoo/include -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -pthread)
compile() { run 180 "$cc" "${flags[@]}" "$@"; }
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
export XLANG_PACKAGE_PATH="$root/release/extlibs"
compile -x c -fsyntax-only release/extlibs/cuckoo/include/cuckoo_native.h
compile -DXRT_MODULE_HASH64 -fsyntax-only release/extlibs/cuckoo/cuckoo_native.c
compile -c release/extlibs/cuckoo/test/xrt_runtime.c -o "$work/runtime.o"
compile -c release/extlibs/cuckoo/cuckoo_native.c -o "$work/native.o"
objects=("$work/native.o" "$work/runtime.o")
compile -DNDEBUG release/extlibs/cuckoo/test/test_cuckoo.c "${objects[@]}" -no-pie -lm -ldl -o "$work/c_test"
run 90 "$work/c_test"
ordinary=(test/cases/extlib_cuckoo.xl test/cases/extlib_cuckoo_boundaries.xl
          release/extlibs/cuckoo/test/high_load.xl release/extlibs/cuckoo/test/allocations.xl
          release/extlibs/cuckoo/test/aot_core.xl release/extlibs/cuckoo/test/local_lifetime_control.xl)
for source in "${ordinary[@]}" release/extlibs/cuckoo/test/global_valid_only.xl release/extlibs/cuckoo/test/global_lifetime.xl; do
    run 90 ./release/xl run "$source"
done
if run 90 ./release/xl check release/extlibs/cuckoo/test/private_native_negative.xl > "$work/private.out" 2> "$work/private.err"; then
    printf 'Private native add was accepted\n' >&2
    exit 1
fi
grep -Fq 'addNative' "$work/private.err"
grep -Fq 'access' "$work/private.err"
run 90 ./release/xl cgen release/examples/extlibs/cuckoo.xl --emit "$work/module.c"
grep -E '^extern .*xcuckoo_[a-z_]+[(]' "$work/module.c" | sort -u > "$work/abi.decls"
grep -oE 'xcuckoo_[a-z_]+[(]' "$work/abi.decls" | tr -d '(' | sort -u > "$work/abi.actual"
printf '%s\n' xcuckoo_add xcuckoo_buckets xcuckoo_clear xcuckoo_contains xcuckoo_count xcuckoo_create xcuckoo_destroy xcuckoo_remove > "$work/abi.expected"
test "$(wc -l < "$work/abi.decls")" -eq 8
diff -u "$work/abi.expected" "$work/abi.actual"
printf '#include "include/cuckoo_native.h"\n' > "$work/abi.c"
cat "$work/abi.decls" >> "$work/abi.c"
compile -fsyntax-only "$work/abi.c"
compile -DNDEBUG release/extlibs/cuckoo/examples/cuckoo.c "${objects[@]}" -no-pie -lm -ldl -o "$work/example"
run 90 "$work/example" > "$work/c.txt"
sed 's/\r$//' release/extlibs/cuckoo/examples/cuckoo.expected.txt > "$work/expected.txt"
diff -u "$work/expected.txt" "$work/c.txt"
run 90 ./release/xl run release/extlibs/cuckoo/examples/cuckoo.c > "$work/direct.txt"
run 90 ./release/xl run release/examples/extlibs/cuckoo.xl > "$work/xl.txt"
diff -u "$work/c.txt" "$work/direct.txt"
diff -u "$work/c.txt" "$work/xl.txt"
for source in "${ordinary[@]}" release/extlibs/cuckoo/examples/cuckoo.c release/examples/extlibs/cuckoo.xl; do
    target="$work/$(basename "${source%.*}")-$(basename "$(dirname "$source")")"
    run 180 ./release/xl build "$source" -o "$target"
    run 90 "$target" > "$target.out"
    case "$source" in
        release/extlibs/cuckoo/examples/cuckoo.c|release/examples/extlibs/cuckoo.xl) diff -u "$work/c.txt" "$target.out" ;;
    esac
done
failed=0
for name in global_valid_only global_lifetime; do
    source="release/extlibs/cuckoo/test/$name.xl"
    run 90 ./release/xl cgen "$source" --emit "$work/$name.c"
    run 180 ./release/xl build "$source" -o "$work/$name"
    # 两条都是合法正例；收集全部失败后再让门禁非零退出，不改为预期失败。
    if run 90 "$work/$name"; then :; else failed=$((failed+1)); fi
done
test "$failed" -eq 0
printf 'cuckoo Linux sanitizer, eight actual prototypes, bilingual AOT and global lifetimes: PASS\n'
