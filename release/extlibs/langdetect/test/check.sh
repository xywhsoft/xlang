#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-langdetect-XXXXXX")
cc=${CC:-gcc}
cc_path=$(command -v "$cc" || true)
sequence=0
input_paths() {
    find release/extlibs/langdetect release/include -type f -print0
    printf '%s\0' lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so release/libxlc.so \
        test/cases/extlib_langdetect.xl test/cases/extlib_langdetect_boundaries.xl release/examples/extlibs/langdetect.xl
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
    else printf '[langdetect] FAIL exit=%s artifacts=%s\n' "$result" "$work" >&2; fi
    printf '[langdetect] report: %s.report.txt\n' "$work"
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
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt -Irelease/extlibs/langdetect
       -Irelease/extlibs/langdetect/include -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -pthread)
compile() { run 180 "$cc" "${flags[@]}" "$@"; }
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
export XLANG_PACKAGE_PATH="$root/release/extlibs"
compile -x c -fsyntax-only release/extlibs/langdetect/include/langdetect_native.h
compile -DXRT_MODULE_UNICODE -fsyntax-only release/extlibs/langdetect/langdetect_native.c
compile -fsyntax-only release/extlibs/langdetect/test/abi_contract.c
compile -c release/extlibs/langdetect/test/xrt_runtime.c -o "$work/runtime.o"
compile -c release/extlibs/langdetect/langdetect_native.c -o "$work/native.o"
for name in test_langdetect scoring_reference_c corpus_c; do
    objects=("$work/runtime.o")
    if [[ $name != scoring_reference_c ]]; then objects+=("$work/native.o"); fi
    compile -DNDEBUG "release/extlibs/langdetect/test/$name.c" "${objects[@]}" -no-pie -lm -ldl -o "$work/$name"
    run 90 "$work/$name" > "$work/$name.txt"
    cat "$work/$name.txt"
done
sed 's/\r$//' release/extlibs/langdetect/test/corpus.expected.txt > "$work/corpus.expected"
diff -u "$work/corpus.expected" "$work/corpus_c.txt"
for name in scoring_reference_c corpus_c; do
    run 90 ./release/xl run "release/extlibs/langdetect/test/$name.c" > "$work/$name.direct"
done
diff -u "$work/corpus.expected" "$work/corpus_c.direct"
sources=(test/cases/extlib_langdetect.xl test/cases/extlib_langdetect_boundaries.xl
         release/extlibs/langdetect/test/analysis_boundaries.xl release/extlibs/langdetect/test/score_boundaries.xl
         release/extlibs/langdetect/test/borrowed_probe.xl release/extlibs/langdetect/test/aot_core.xl)
for source in "${sources[@]}"; do run 90 ./release/xl run "$source"; done
run 90 ./release/xl cgen release/extlibs/langdetect/test/borrowed_probe.xl --emit "$work/probe.c"
if grep -Eq '= *xlir_string_copy *\(' "$work/probe.c"; then exit 1; fi
for name in detect code script last_error; do
    variable=$(sed -nE "s/.*const char\* (borrowed_raw_[0-9]+) = xlangdetect_$name\(.*/\1/p" "$work/probe.c")
    test "$(printf '%s\n' "$variable" | wc -l)" -eq 1
    test -n "$variable"
    test "$(grep -Fc "xlir_string_from_cstr($variable)" "$work/probe.c")" -eq 1
done
grep -E '^extern .*xlangdetect_[a-z0-9_]+[(]' "$work/probe.c" | sort -u > "$work/abi.decls"
grep -oE 'xlangdetect_[a-z0-9_]+[(]' "$work/abi.decls" | tr -d '(' | sort -u > "$work/abi.actual"
printf '%s\n' xlangdetect_code xlangdetect_confidence xlangdetect_detect xlangdetect_last_error xlangdetect_script xlangdetect_valid_utf8 > "$work/abi.expected"
test "$(wc -l < "$work/abi.decls")" -eq 6
diff -u "$work/abi.expected" "$work/abi.actual"
printf '#include "include/langdetect_native.h"\n' > "$work/abi.c"
cat "$work/abi.decls" >> "$work/abi.c"
compile -fsyntax-only "$work/abi.c"
for name in langdetect review_queue_c; do
    language=release/examples/extlibs/langdetect.xl
    if [[ $name == review_queue_c ]]; then language=release/extlibs/langdetect/examples/review_queue.xl; fi
    compile -DNDEBUG "release/extlibs/langdetect/examples/$name.c" "$work/native.o" "$work/runtime.o" -no-pie -lm -ldl -o "$work/$name"
    run 90 "$work/$name" > "$work/$name.c.txt"
    run 90 ./release/xl run "release/extlibs/langdetect/examples/$name.c" > "$work/$name.direct.txt"
    run 90 ./release/xl run "$language" > "$work/$name.xl.txt"
    diff -u "$work/$name.c.txt" "$work/$name.direct.txt"
    diff -u "$work/$name.c.txt" "$work/$name.xl.txt"
done
sed 's/\r$//' release/extlibs/langdetect/examples/review_queue.expected.txt > "$work/queue.expected"
diff -u "$work/queue.expected" "$work/review_queue_c.c.txt"
for source in "${sources[@]}" release/extlibs/langdetect/test/corpus_c.c release/extlibs/langdetect/test/scoring_reference_c.c \
    release/extlibs/langdetect/examples/langdetect.c release/examples/extlibs/langdetect.xl \
    release/extlibs/langdetect/examples/review_queue.xl release/extlibs/langdetect/examples/review_queue_c.c; do
    target="$work/$(basename "${source%.*}")-$(basename "$(dirname "$source")")"
    run 180 ./release/xl build "$source" -o "$target"
    run 90 "$target" > "$target.out"
    case "$source" in
        *review_queue*) diff -u "$work/queue.expected" "$target.out" ;;
        */examples/langdetect.c|release/examples/extlibs/langdetect.xl) diff -u "$work/langdetect.c.txt" "$target.out" ;;
        */corpus_c.c) diff -u "$work/corpus.expected" "$target.out" ;;
    esac
done
printf 'langdetect Linux sanitizer, scoring oracle, corpus, six actual prototypes and bilingual JIT/AOT: PASS\n'
