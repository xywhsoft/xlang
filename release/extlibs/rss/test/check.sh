#!/usr/bin/env bash
set -Eeuo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-rss-XXXXXX")
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
    else printf '[rss] FAIL exit=%s artifacts=%s\n' "$code" "$work" >&2; fi
    exit "$code"
}
trap cleanup EXIT
trap 'record_failure "$?" "$LINENO" "$BASH_COMMAND"' ERR
exec > >(tee "$work/output.log") 2>&1
cc=${CC:-gcc}
run() {
    printf '%q ' "$@" > "$work/last-command.txt"
    printf '\n' >> "$work/last-command.txt"
    cat "$work/last-command.txt" >> "$work/commands.log"
    timeout 180s "$@"
}
mapfile -d '' inputs < <(find release/extlibs/rss release/extlibs/xml.dom \
    release/extlibs/xml.sax release/extlibs/_expat release/include -type f -print0)
inputs+=(lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so release/libxlc.so
    test/cases/extlib_rss.xl release/examples/extlibs/rss_c.c
    release/examples/extlibs/rss_pair.xl release/examples/extlibs/rss.xl)
sha256sum "${inputs[@]}" > "$work/inputs.before.sha256"
run "$cc" --version > "$work/compiler.txt"
cc1=$(run "$cc" -print-prog-name=cc1)
sha256sum "$(command -v "$cc")" "$cc1" "$(command -v bash)" >> "$work/inputs.before.sha256"
includes=(-Ilib/xrt -Irelease/extlibs/rss/include -Irelease/extlibs/xml.dom
    -Irelease/extlibs/xml.sax -Irelease/extlibs/_expat/vendor/expat/lib
    -Irelease/extlibs/_expat/vendor/expat)
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra
    -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie -pthread
    "${includes[@]}")
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
sources=(release/extlibs/_expat/vendor/expat/lib/xmlparse.c
    release/extlibs/_expat/vendor/expat/lib/xmlrole.c release/extlibs/_expat/vendor/expat/lib/xmltok.c
    release/extlibs/_expat/expat_random.c release/extlibs/xml.sax/xrt_xml_common.c
    release/extlibs/xml.sax/xrt_xml_sax.c release/extlibs/xml.sax/xml_sax_native.c
    release/extlibs/xml.dom/xrt_xml_dom_tree.c release/extlibs/xml.dom/xrt_xml_dom_parser.c
    release/extlibs/xml.dom/xrt_xml_dom_writer.c release/extlibs/xml.dom/xml_dom_native.c
    release/extlibs/rss/rss_parse.c release/extlibs/rss/rss_build.c)
cargs=("${sources[@]}")
for dir in lib/xrt release/extlibs/rss/include release/extlibs/xml.dom release/extlibs/xml.sax \
    release/extlibs/_expat/vendor/expat/lib release/extlibs/_expat/vendor/expat; do
    cargs+=(--inc-path "$dir")
done
run "$cc" "${flags[@]}" -Werror -fsyntax-only -DXRT_MODULE_BUFFER \
    -DXRT_MODULE_UNICODE -DXRT_MODULE_VALUE_CONTAINER \
    release/extlibs/rss/rss_parse.c release/extlibs/rss/rss_build.c
run "$cc" "${flags[@]}" -Werror -fsyntax-only release/extlibs/rss/test/test_rss.c \
    release/extlibs/rss/test/test_link_relations.c release/examples/extlibs/rss_c.c \
    release/extlibs/rss/examples/atom_author_c.c release/extlibs/rss/examples/atom_links_c.c
objects=()
all_sources=(release/extlibs/rss/test/runtime.c "${sources[@]}")
for i in "${!all_sources[@]}"; do
    object="$work/unit_$i.o"
    run "$cc" "${flags[@]}" -c "${all_sources[$i]}" -o "$object"
    objects+=("$object")
done
for name in test_rss test_link_relations; do
    run "$cc" "${flags[@]}" -Werror "release/extlibs/rss/test/$name.c" "${objects[@]}" -lm -ldl -o "$work/$name"
    run "$work/$name"
    run ./release/xl run "release/extlibs/rss/test/$name.c" "${cargs[@]}"
done
for name in link_relations implicit_alternate input_limit; do
    run ./release/xl run "release/extlibs/rss/test/$name.xl"
done

run ./release/xl cgen test/cases/extlib_rss.xl --emit "$work/generated.c"
grep -E '^extern [^;]*\bxrss_(detect|parse|parse_rss|parse_atom|build_rss|build_atom)\(' "$work/generated.c" > "$work/prototypes.txt"
[[ $(wc -l < "$work/prototypes.txt") == 6 ]]
printf '#include "rss_native.h"\n' > "$work/abi.c"
cat "$work/prototypes.txt" >> "$work/abi.c"
run "$cc" "${flags[@]}" -Werror -fsyntax-only "$work/abi.c"
sed 's/^extern .* xrss_parse(/extern int xrss_parse(/' "$work/abi.c" > "$work/wrong.c"
if run "$cc" "${flags[@]}" -Werror -fsyntax-only "$work/wrong.c" > "$work/wrong.log" 2>&1; then
    printf 'wrong parse return accepted\n' >&2; exit 1
else [[ $? == 1 ]]; fi
grep -q 'conflicting types' "$work/wrong.log"

examples=(release/examples/extlibs/rss_c.c release/extlibs/rss/examples/atom_author_c.c
    release/extlibs/rss/examples/atom_links_c.c)
xl_examples=(release/examples/extlibs/rss_pair.xl release/extlibs/rss/examples/atom_author.xl
    release/extlibs/rss/examples/atom_links.xl)
printf 'type: rss\nitems: 1\n' > "$work/expected_0.txt"
printf 'Editor\nSource\n' > "$work/expected_1.txt"
cp release/extlibs/rss/examples/atom_links.expected.txt "$work/expected_2.txt"
for i in 0 1 2; do
    run "$cc" "${flags[@]}" -Werror "${examples[$i]}" "${objects[@]}" -lm -ldl -o "$work/example_$i"
    run "$work/example_$i" > "$work/example_$i.gcc.txt"
    diff -u "$work/expected_$i.txt" "$work/example_$i.gcc.txt"
    run ./release/xl run "${examples[$i]}" "${cargs[@]}" > "$work/example_$i.cjit.txt"
    diff -u "$work/expected_$i.txt" "$work/example_$i.cjit.txt"
    run ./release/xl build "${examples[$i]}" "${cargs[@]}" -o "$work/example_$i.caot"
    run "$work/example_$i.caot" > "$work/example_$i.caot.txt"
    diff -u "$work/expected_$i.txt" "$work/example_$i.caot.txt"
    if ((i > 0)); then
        run ./release/xl run "${xl_examples[$i]}" > "$work/example_$i.xljit.txt"
        diff -u "$work/expected_$i.txt" "$work/example_$i.xljit.txt"
        run ./release/xl build "${xl_examples[$i]}" -o "$work/example_$i.xlaot"
        run "$work/example_$i.xlaot" > "$work/example_$i.xlaot.txt"
        diff -u "$work/expected_$i.txt" "$work/example_$i.xlaot.txt"
    fi
done
run ./release/xl build release/extlibs/rss/test/test_link_relations.c "${cargs[@]}" -o "$work/relations_c"
run "$work/relations_c"
run ./release/xl build release/extlibs/rss/test/link_relations.xl -o "$work/relations_xl"
run "$work/relations_xl"
sha256sum -c "$work/inputs.before.sha256" > "$work/inputs.after.txt"
printf 'rss focused C/XLang/AOT checks: PASS; full gate continues\n'

# 保留语言构造容器的原正例；不能把共享 M1 改成预期错误。
run ./release/xl run test/cases/extlib_rss.xl
run ./release/xl run release/extlibs/rss/test/author_inheritance.xl
run ./release/xl run "${xl_examples[0]}" > "$work/rss_xl.txt"
diff -u "$work/expected_0.txt" "$work/rss_xl.txt"
run ./release/xl run release/examples/extlibs/rss.xl
run ./release/xl build release/extlibs/rss/test/aot_core.xl -o "$work/core"
run "$work/core"
printf 'rss Linux C ASan/UBSan, XLang and AOT gate: PASS\n'
