#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-pinyin-XXXXXX")
cc=${CC:-gcc}
cc_path=$(command -v "$cc" || true)
sequence=0
input_paths() {
    find release/extlibs/pinyin release/include -type f -print0
    printf '%s\0' lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so release/libxlc.so \
        test/cases/extlib_pinyin.xl release/examples/extlibs/pinyin.xl release/examples/extlibs/pinyin_c.c
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
    else printf '[pinyin] FAIL exit=%s artifacts=%s\n' "$result" "$work" >&2; fi
    printf '[pinyin] report: %s.report.txt\n' "$work"
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
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt -Irelease/extlibs/pinyin
       -Irelease/extlibs/pinyin/include -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -pthread)
compile() { run 180 "$cc" "${flags[@]}" "$@"; }
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
export XLANG_PACKAGE_PATH="$root/release/extlibs"
compile -x c -fsyntax-only release/extlibs/pinyin/include/pinyin_native.h
compile -DXRT_MODULE_BUFFER -DXRT_MODULE_UNICODE -fsyntax-only release/extlibs/pinyin/pinyin_native.c
compile -c release/extlibs/pinyin/test/xrt_runtime.c -o "$work/runtime.o"
compile -c release/extlibs/pinyin/pinyin_native.c -o "$work/native.o"
for name in test_pinyin readings_c; do
    compile -DNDEBUG "release/extlibs/pinyin/test/$name.c" "$work/native.o" "$work/runtime.o" -no-pie -lm -ldl -o "$work/$name"
    run 90 "$work/$name" > "$work/$name.txt"
    cat "$work/$name.txt"
done
printf 'pinyin readings: 55328 checks, 0 failures\n' > "$work/readings.expected"
diff -u "$work/readings.expected" "$work/readings_c.txt"
run 90 ./release/xl run release/extlibs/pinyin/test/readings_c.c > "$work/readings.direct"
diff -u "$work/readings.expected" "$work/readings.direct"
sources=(test/cases/extlib_pinyin.xl release/extlibs/pinyin/test/boundaries.xl
         release/extlibs/pinyin/test/allocations.xl release/extlibs/pinyin/test/owned_probe.xl)
for source in "${sources[@]}"; do run 90 ./release/xl run "$source"; done
run 90 ./release/xl cgen release/extlibs/pinyin/test/owned_probe.xl --emit "$work/probe.c"
# 四个返回函数不可复制结果；调用方使用已持有字符串的语义不在此冒充零开销。
awk '/^static xlir_result_string xlir_fn_[0-9]+\(.*\)$/ { active=1; ++count }
    active { print } active && /^}/ { active=0 }
    END { if (count != 4) exit 1 }' "$work/probe.c" > "$work/return_functions.c"
if grep -Eq '= *xlir_string_copy *\(' "$work/return_functions.c"; then exit 1; fi
for name in convert_n initials_n; do
    test "$(grep -Ec "[[:alnum:]_]+\.Data[[:space:]]*=[[:space:]]*xpinyin_$name[[:space:]]*\(" "$work/probe.c")" -eq 1
done
grep -E '^extern .*xpinyin_[a-z_]+[(]' "$work/probe.c" | sort -u > "$work/abi.decls"
grep -oE 'xpinyin_[a-z_]+[(]' "$work/abi.decls" | tr -d '(' | sort -u > "$work/abi.actual"
printf '%s\n' xpinyin_convert_n xpinyin_initials_n > "$work/abi.expected"
test "$(wc -l < "$work/abi.decls")" -eq 2
diff -u "$work/abi.expected" "$work/abi.actual"
printf '#include "include/pinyin_native.h"\n' > "$work/abi.c"
cat "$work/abi.decls" >> "$work/abi.c"
compile -fsyntax-only "$work/abi.c"
for name in old keys; do
    objects=("$work/runtime.o")
    if [[ $name == old ]]; then
        c=release/examples/extlibs/pinyin_c.c; language=release/examples/extlibs/pinyin.xl
        expected=release/extlibs/pinyin/test/pinyin.expected.txt
    else
        c=release/extlibs/pinyin/examples/search_keys_c.c; language=release/extlibs/pinyin/examples/search_keys.xl
        expected=release/extlibs/pinyin/examples/search_keys.expected.txt; objects+=("$work/native.o")
    fi
    sed 's/\r$//' "$expected" > "$work/$name.expected"
    compile -DNDEBUG "$c" "${objects[@]}" -no-pie -lm -ldl -o "$work/$name"
    run 90 "$work/$name" > "$work/$name.c.txt"
    run 90 ./release/xl run "$c" > "$work/$name.direct.txt"
    run 90 ./release/xl run "$language" > "$work/$name.xl.txt"
    for actual in c direct xl; do diff -u "$work/$name.expected" "$work/$name.$actual.txt"; done
done
for source in "${sources[@]}" release/extlibs/pinyin/test/readings_c.c release/examples/extlibs/pinyin_c.c \
    release/examples/extlibs/pinyin.xl release/extlibs/pinyin/examples/search_keys.xl release/extlibs/pinyin/examples/search_keys_c.c; do
    target="$work/$(basename "${source%.*}")-$(basename "$(dirname "$source")")"
    run 180 ./release/xl build "$source" -o "$target"
    run 90 "$target" > "$target.out"
    case "$source" in
        *search_keys*) diff -u "$work/keys.expected" "$target.out" ;;
        release/examples/extlibs/pinyin.xl|release/examples/extlibs/pinyin_c.c) diff -u "$work/old.expected" "$target.out" ;;
        */readings_c.c) diff -u "$work/readings.expected" "$target.out" ;;
    esac
done
printf 'pinyin Linux sanitizer, 55328 reading checks, owned returns, actual prototypes and bilingual JIT/AOT: PASS\n'
