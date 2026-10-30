#!/usr/bin/env bash
set -Eeuo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-sitemap-XXXXXX")
record_failure() {
    local code=$1 line=$2 command=$3
    if [[ ! -f "$work/failure.txt" ]]; then
        printf 'exit=%s\nline=%s\ncommand=%s\n' "$code" "$line" "$command" > "$work/failure.txt"
    fi
}
cleanup() {
    local code=$?
    trap - EXIT
    if [[ -f "$work/inputs.before.sha256" ]]; then
        if ! sha256sum -c "$work/inputs.before.sha256" > "$work/inputs.after.txt"; then code=1; fi
    fi
    if ((code == 0)); then rm -rf -- "$work"
    else printf '[sitemap] FAIL exit=%s artifacts=%s\n' "$code" "$work" >&2; fi
    exit "$code"
}
trap cleanup EXIT
trap 'record_failure "$?" "$LINENO" "$BASH_COMMAND"' ERR
exec > >(tee "$work/output.log") 2>&1
cc=${CC:-gcc}
python=${PYTHON:-python3}
run() {
    printf '%q ' "$@" > "$work/last-command.txt"
    printf '\n' >> "$work/last-command.txt"
    cat "$work/last-command.txt" >> "$work/commands.log"
    timeout 180s "$@"
}
mapfile -d '' inputs < <(find release/extlibs/sitemap release/extlibs/xml.sax \
    release/extlibs/_expat release/include -type f -print0)
inputs+=(lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so release/libxlc.so
    test/cases/extlib_sitemap.xl release/examples/extlibs/sitemap_pair.xl release/examples/extlibs/sitemap.xl)
sha256sum "${inputs[@]}" > "$work/inputs.before.sha256"
run "$cc" --version > "$work/compiler.txt"
cc1=$(run "$cc" -print-prog-name=cc1)
run "$python" --version > "$work/python.txt"
sha256sum "$(command -v "$cc")" "$cc1" "$(command -v "$python")" >> "$work/inputs.before.sha256"
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror
    -Ilib/xrt -Irelease/extlibs/sitemap/include -Irelease/extlibs/xml.sax
    -Irelease/extlibs/_expat/vendor/expat/lib -Irelease/extlibs/_expat/vendor/expat
    -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
sources=(release/extlibs/_expat/vendor/expat/lib/xmlparse.c
    release/extlibs/_expat/vendor/expat/lib/xmlrole.c release/extlibs/_expat/vendor/expat/lib/xmltok.c
    release/extlibs/_expat/expat_random.c release/extlibs/xml.sax/xrt_xml_common.c
    release/extlibs/xml.sax/xrt_xml_sax.c release/extlibs/xml.sax/xml_sax_native.c
    release/extlibs/sitemap/sitemap_xml.c release/extlibs/sitemap/sitemap_robots.c)
cargs=("${sources[@]}")
for dir in lib/xrt release/extlibs/sitemap/include release/extlibs/xml.sax \
    release/extlibs/_expat/vendor/expat/lib release/extlibs/_expat/vendor/expat; do
    cargs+=(--inc-path "$dir")
done
run "$cc" "${flags[@]}" -fsyntax-only -include sitemap_native.h -DXRT_MODULE_IO -DXRT_MODULE_BUFFER \
    -DXRT_MODULE_STRING -DXRT_MODULE_UNICODE -DXRT_MODULE_VALUE_CONTAINER \
    release/extlibs/sitemap/sitemap_xml.c release/extlibs/sitemap/sitemap_robots.c
run "$cc" "${flags[@]}" -fsyntax-only release/extlibs/sitemap/test/test_sitemap.c \
    release/extlibs/sitemap/test/test_robots_newlines.c release/extlibs/sitemap/examples/sitemap_c.c \
    release/extlibs/sitemap/examples/robots_policy_c.c
objects=()
all_sources=(release/extlibs/sitemap/test/xrt_runtime.c "${sources[@]}")
for i in "${!all_sources[@]}"; do
    object="$work/unit_$i.o"
    run "$cc" "${flags[@]}" -c "${all_sources[$i]}" -o "$object"
    objects+=("$object")
done
for name in test_sitemap test_robots_newlines; do
    run "$cc" "${flags[@]}" "release/extlibs/sitemap/test/$name.c" "${objects[@]}" -lm -ldl -o "$work/$name"
    run "$work/$name"
    run ./release/xl run "release/extlibs/sitemap/test/$name.c" "${cargs[@]}"
done
run ./release/xl run release/extlibs/sitemap/test/robots_newlines.xl
run ./release/xl cgen test/cases/extlib_sitemap.xl --emit "$work/generated.c"
grep -E '^extern [^;]*\bxsm_(build|build_index|parse_n|parse_index_n|robots_parse_n|robots_allows_n)\(' "$work/generated.c" > "$work/prototypes.txt"
[[ $(wc -l < "$work/prototypes.txt") == 6 ]]
printf '#include "sitemap_native.h"\n' > "$work/abi.c"
cat "$work/prototypes.txt" >> "$work/abi.c"
run "$cc" "${flags[@]}" -fsyntax-only "$work/abi.c"
sed 's/^extern .* xsm_parse_n(/extern int xsm_parse_n(/' "$work/abi.c" > "$work/wrong.c"
if run "$cc" "${flags[@]}" -fsyntax-only "$work/wrong.c" > "$work/wrong.log" 2>&1; then
    printf 'wrong parse return accepted\n' >&2; exit 1
else [[ $? == 1 ]]; fi
grep -q 'conflicting types' "$work/wrong.log"

for name in sitemap robots_policy; do
    source="release/extlibs/sitemap/examples/${name}_c.c"
    run "$cc" "${flags[@]}" "$source" "${objects[@]}" -lm -ldl -o "$work/${name}_gcc"
    run "$work/${name}_gcc" > "$work/$name.gcc.txt"
    run ./release/xl run "$source" "${cargs[@]}" > "$work/$name.cjit.txt"
    diff -u "$work/$name.gcc.txt" "$work/$name.cjit.txt"
    run ./release/xl build "$source" "${cargs[@]}" -o "$work/${name}_caot"
    run "$work/${name}_caot" > "$work/$name.caot.txt"
    diff -u "$work/$name.gcc.txt" "$work/$name.caot.txt"
    if [[ $name == sitemap ]]; then
        run "$python" -I release/extlibs/sitemap/test/check_external.py "$work/$name.gcc.txt" "$work/$name.cjit.txt"
        sed 's|https://example.test/?a=1\&amp;b=2|https://example.test/wrong|' "$work/$name.gcc.txt" > "$work/bad.txt"
        if run "$python" -I release/extlibs/sitemap/test/check_external.py "$work/bad.txt" "$work/bad.txt" > "$work/bad.log" 2>&1; then
            printf 'corrupt XML golden accepted\n' >&2; exit 1
        else [[ $? == 1 ]]; fi
        grep -q 'URL or XML escaping differs' "$work/bad.log"
    else
        golden=release/extlibs/sitemap/examples/robots_policy.expected.txt
        diff -u "$golden" "$work/$name.gcc.txt"
        run ./release/xl run release/extlibs/sitemap/examples/robots_policy.xl > "$work/policy.xljit.txt"
        diff -u "$golden" "$work/policy.xljit.txt"
        run ./release/xl build release/extlibs/sitemap/examples/robots_policy.xl -o "$work/policy_xlaot"
        run "$work/policy_xlaot" > "$work/policy.xlaot.txt"
        diff -u "$golden" "$work/policy.xlaot.txt"
    fi
done
run ./release/xl build release/extlibs/sitemap/test/test_robots_newlines.c "${cargs[@]}" -o "$work/newlines_c"
run "$work/newlines_c"
run ./release/xl build release/extlibs/sitemap/test/robots_newlines.xl -o "$work/newlines_xl"
run "$work/newlines_xl"
sha256sum -c "$work/inputs.before.sha256" > "$work/inputs.after.txt"
printf 'sitemap focused C/XLang/AOT checks: PASS; full gate continues\n'

# 原语言数组生成例仍是必须通过的正例，不能用原生返回值替代。
run ./release/xl run test/cases/extlib_sitemap.xl
run ./release/xl run release/extlibs/sitemap/test/boundaries.xl
run ./release/xl run release/extlibs/sitemap/test/allocations.xl
run ./release/xl run release/extlibs/sitemap/test/protocol_limits.xl
run ./release/xl run release/examples/extlibs/sitemap_pair.xl > "$work/original.xljit.txt"
run "$python" -I release/extlibs/sitemap/test/check_external.py "$work/sitemap.gcc.txt" "$work/original.xljit.txt"
run ./release/xl build release/extlibs/sitemap/test/core_aot.xl -o "$work/core"
run "$work/core" > "$work/core.txt"
[[ ! -s "$work/core.txt" ]]
run ./release/xl build release/examples/extlibs/sitemap_pair.xl -o "$work/original_xlaot"
run "$work/original_xlaot" > "$work/original.xlaot.txt"
run "$python" -I release/extlibs/sitemap/test/check_external.py "$work/sitemap.gcc.txt" "$work/original.xlaot.txt"
printf 'sitemap Linux C ASan/UBSan, XLang and AOT gate: PASS\n'
