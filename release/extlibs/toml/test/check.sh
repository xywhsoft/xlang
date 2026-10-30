#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-toml-XXXXXX")
cc=${CC:-gcc}; python=${PYTHON:-python3}
cc_path=$(command -v "$cc" || true); python_path=$(command -v "$python" || true)
sequence=0
input_paths() {
    find release/extlibs/toml release/extlibs/_tomlc17 release/include -type f -print0
    printf '%s\0' lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so release/libxlc.so \
        test/cases/extlib_toml.xl release/examples/extlibs/toml.xl release/examples/extlibs/toml_c.c
    if [[ -n "$cc_path" ]]; then printf '%s\0' "$cc_path"; fi
    if [[ -n "$python_path" ]]; then printf '%s\0' "$python_path"; fi
}
cleanup() {
    result=$?; trap - EXIT; set +e
    cd "$root" || exit 125
    mapfile -d '' after < <(input_paths | sort -zu)
    sha256sum "${after[@]}" > "$work/inputs.after.sha256"
    if ! diff -u "$work/inputs.before.sha256" "$work/inputs.after.sha256"; then result=125; fi
    printf 'exit=%s\nphases=%s\n' "$result" "$sequence" > "$work.report.txt"
    if [[ -f "$work/commands.log" ]]; then cat "$work/commands.log" >> "$work.report.txt"; fi
    cp -- "$work/inputs.before.sha256" "$work.inputs.sha256"
    if ((result == 0)); then rm -rf -- "$work" || result=$?
    else printf '[toml] FAIL exit=%s artifacts=%s\n' "$result" "$work" >&2; fi
    printf '[toml] report: %s.report.txt\n' "$work"
    exit "$result"
}
trap cleanup EXIT
trap 'printf "exit=%s line=%s command=%s\n" "$?" "$LINENO" "$BASH_COMMAND" > "$work/failure.txt"' ERR
mapfile -d '' inputs < <(input_paths | sort -zu)
sha256sum "${inputs[@]}" > "$work/inputs.before.sha256"
run() {
    local seconds=$1 result; shift
    sequence=$((sequence+1))
    printf '\n$ timeout %ss' "$seconds" >> "$work/commands.log"
    printf ' %q' "$@" >> "$work/commands.log"
    printf '\n' >> "$work/commands.log"
    if timeout "${seconds}s" "$@" > "$work/$sequence.out" 2> "$work/$sequence.err"; then result=0; else result=$?; fi
    printf 'exit=%s\n' "$result" >> "$work/commands.log"
    cat "$work/$sequence.out" "$work/$sequence.err" >> "$work/commands.log"
    cat "$work/$sequence.out"; cat "$work/$sequence.err" >&2
    return "$result"
}
run 10 "$cc" --version > "$work/compiler.txt"
run 10 "$python" -c 'import sys,tomllib; print(sys.version)'
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt -Irelease/extlibs/toml
       -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -pthread)
compile() { run 180 "$cc" "${flags[@]}" "$@"; }
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
export PYTHONDONTWRITEBYTECODE=1
export XLANG_PACKAGE_PATH="$root/release/extlibs"
run 10 "$python" -c 'import json,pathlib,re,hashlib; p=pathlib.Path("release/extlibs/toml"); a=json.loads((p/"xlang.package.json").read_text())["artifact"]["xrtModules"]; b=re.findall(r"^#define (XRT_MODULE_[A-Z_]+)\s*$",(p/"xrt_toml_internal.h").read_text(),re.M); assert len(a)==12 and sorted(a)==sorted(b); q=pathlib.Path("release/extlibs/_tomlc17"); h=hashlib.sha256((q/"src/tomlc17.c").read_bytes()).hexdigest().upper(); assert "Local `src/tomlc17.c` SHA-256 (canonical LF): `"+h+"`" in (q/"VENDOR.md").read_text()'
compile -x c -fsyntax-only release/extlibs/toml/include/xrt_toml.h
for source in xrt_toml_types.c xrt_toml_parser.c xrt_toml_writer.c toml_native.c; do compile -fsyntax-only "release/extlibs/toml/$source"; done
compile -fsyntax-only release/extlibs/toml/test/abi_contract.c
mapfile -t modules < <("$python" -c 'import json; print("\n".join("-D"+x for x in json.load(open("release/extlibs/toml/xlang.package.json"))["artifact"]["xrtModules"]))')
compile "${modules[@]}" -DXRT_IMPLEMENTATION release/extlibs/toml/test/manifest_probe.c -no-pie -lm -ldl -o "$work/manifest"
run 120 "$work/manifest"
compile -c release/extlibs/toml/test/xrt_runtime.c -o "$work/runtime.o"
for name in test_toml ownership_bounds_c; do
    compile "release/extlibs/toml/test/$name.c" "$work/runtime.o" -no-pie -lm -ldl -o "$work/$name"
    arguments=(); if [[ $name == test_toml ]]; then arguments=("$work/nested.toml"); fi
    run 120 "$work/$name" "${arguments[@]}" > "$work/$name.txt"
    cat "$work/$name.txt"
done
printf 'toml ownership/calendar: 288 checks, 0 failures\n' > "$work/bounds.expected"
diff -u "$work/bounds.expected" "$work/ownership_bounds_c.txt"
for point in 0 1 2 3; do run 90 "$work/ownership_bounds_c" --cold "$point"; done
run 90 "$work/ownership_bounds_c" --records > "$work/gcc.tsv"
run 60 "$python" release/extlibs/toml/test/check_calendar_records.py "$work/gcc.tsv"
run 60 "$python" release/extlibs/toml/test/check_external.py --nested "$work/nested.toml"
source=release/extlibs/toml/test/ownership_bounds_c.c
run 120 ./release/xl run "$source" > "$work/bounds.direct"
diff -u "$work/bounds.expected" "$work/bounds.direct"
for point in 0 1 2 3; do run 120 ./release/xl run "$source" -- --cold "$point"; done
run 90 ./release/xl run "$source" -- --records > "$work/tcc.tsv"
cmp "$work/gcc.tsv" "$work/tcc.tsv"
run 60 "$python" release/extlibs/toml/test/check_calendar_records.py "$work/tcc.tsv"
run 120 ./release/xl run release/extlibs/toml/test/test_toml.c
for name in basic calendar_guard; do
    source=release/examples/extlibs/toml_c.c
    if [[ $name != basic ]]; then source=release/extlibs/toml/examples/calendar_guard_c.c; fi
    compile "$source" "$work/runtime.o" -no-pie -lm -ldl -o "$work/$name"
    run 90 "$work/$name" > "$work/$name.c.txt"
    run 90 ./release/xl run "$source" > "$work/$name.direct.txt"
    diff -u "$work/$name.c.txt" "$work/$name.direct.txt"
done
sed 's/\r$//' release/extlibs/toml/examples/calendar_guard.expected.txt > "$work/calendar.expected"
diff -u "$work/calendar.expected" "$work/calendar_guard.c.txt"
language=release/extlibs/toml/examples/calendar_guard.xl
run 120 ./release/xl run "$language" > "$work/calendar.xl.txt"
diff -u "$work/calendar.expected" "$work/calendar.xl.txt"
calendar=release/extlibs/toml/test/calendar.xl
run 120 ./release/xl run "$calendar"
run 90 ./release/xl cgen "$language" --emit "$work/probe.c"
grep -Eq 'xl6_xrt_take_cstr\(' "$work/probe.c"
grep -E '^extern .*xtoml_[a-z_]+[(]' "$work/probe.c" | sort -u > "$work/abi.decls"
grep -oE 'xtoml_[a-z_]+[(]' "$work/abi.decls" | tr -d '(' | sort -u > "$work/abi.actual"
sed -nE 's/^extern "(xtoml_[A-Za-z0-9_]+)".*/\1/p' release/extlibs/toml/src/toml.xl | sort -u > "$work/abi.expected"
test "$(wc -l < "$work/abi.decls")" -eq 16
diff -u "$work/abi.expected" "$work/abi.actual"
printf '#include "toml_native.c"\n' > "$work/abi.c"; cat "$work/abi.decls" >> "$work/abi.c"
compile -fsyntax-only "$work/abi.c"
sources=(release/extlibs/toml/test/ownership_bounds_c.c release/extlibs/toml/test/test_toml.c "$calendar" "$language"
         release/examples/extlibs/toml_c.c release/extlibs/toml/examples/calendar_guard_c.c)
index=0
for source in "${sources[@]}"; do
    index=$((index+1)); binary="$work/aot-$index"
    run 120 ./release/xl build "$source" -o "$binary"
    run 90 "$binary" > "$work/aot-$index.txt"
    case "$source" in
        */ownership_bounds_c.c)
            diff -u "$work/bounds.expected" "$work/aot-$index.txt"
            for point in 0 1 2 3; do run 90 "$binary" --cold "$point"; done
            run 90 "$binary" --records > "$work/aot.tsv"; cmp "$work/gcc.tsv" "$work/aot.tsv"
            run 60 "$python" release/extlibs/toml/test/check_calendar_records.py "$work/aot.tsv" ;;
        */toml_c.c) diff -u "$work/basic.c.txt" "$work/aot-$index.txt" ;;
        */calendar_guard_c.c|*/calendar_guard.xl) diff -u "$work/calendar.expected" "$work/aot-$index.txt" ;;
    esac
done
# 语言字典写出及原文件互通仍是必过项，不用 parse 结果替代。
for source in test/cases/extlib_toml.xl release/extlibs/toml/test/root_argument_repro.xl \
    release/extlibs/toml/test/boundaries.xl release/extlibs/toml/test/allocations.xl \
    release/extlibs/toml/test/aot_core.xl release/examples/extlibs/toml.xl; do
    run 120 ./release/xl run "$source" > "$work/complete.txt"
    if [[ $source == release/examples/extlibs/toml.xl ]]; then diff -u "$work/basic.c.txt" "$work/complete.txt"; fi
    run 120 ./release/xl build "$source" -o "$work/complete"; run 90 "$work/complete"
done
run 120 "$work/basic" "$work/c.toml" > "$work/c-file.txt"
run 120 ./release/xl run release/examples/extlibs/toml.xl -- "$work/xlang.toml" > "$work/xl-file.txt"
diff -u "$work/c-file.txt" "$work/xl-file.txt"; cmp "$work/c.toml" "$work/xlang.toml"
run 90 "$python" release/extlibs/toml/test/check_external.py "$work/c.toml" "$work/xlang.toml"
printf 'toml Linux sanitizer, cold recovery, calendar oracle and bilingual JIT/AOT: PASS\n'
