#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-xml-trees-XXXXXX")
cleanup() {
    result=$?
    trap - EXIT
    set +e
    if ((result == 0)); then rm -rf -- "$work" || result=$?
    else printf 'XML FAIL exit=%s artifacts=%s\n' "$result" "$work" >&2; fi
    exit "$result"
}
trap cleanup EXIT
trap 'printf "exit=%s\nline=%s\ncommand=%s\npipeline=%s\n" "$?" "$LINENO" "$BASH_COMMAND" "${PIPESTATUS[*]}" > "$work/failure.txt"' ERR
exec > >(tee "$work/output.log") 2>&1
cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror
       -Ilib/xrt -Irelease/extlibs/xml.sax/include
       -Irelease/extlibs/xml.dom/include
       -Irelease/extlibs/xml.etree/include
       -fsanitize=address,undefined -fno-omit-frame-pointer
       -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
mapfile -d '' inputs < <(
    { find release/extlibs/xml.sax release/extlibs/xml.dom release/extlibs/xml.etree release/extlibs/_expat -type f -print0
      printf '%s\0' lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so release/libxlc.so \
          test/cases/extlib_xml_tree_interop.xl
      for kind in sax dom etree; do
          printf '%s\0' "test/cases/extlib_xml_$kind.xl" "release/examples/extlibs/xml_$kind.xl" "release/examples/extlibs/xml_${kind}_c.c"
      done
      printf '%s\0' dev/extlib_blockers/xml_{sax_reader,dom_node,etree_element}_type.xl
    } | sort -zu
)
sha256sum "${inputs[@]}" > "$work/inputs.before.sha256"
timeout 30s "$cc" --version > "$work/compiler.txt"

timeout 120s "$cc" "${flags[@]}" -c release/extlibs/xml.sax/test/xrt_runtime.c \
    -o "$work/xrt_runtime.o"
timeout 120s "$cc" "${flags[@]}" release/extlibs/xml.sax/test/test_bridge_state.c \
    "$work/xrt_runtime.o" -lm -ldl -o "$work/bridge_state"
timeout 90s "$work/bridge_state" threads
for kind in {0..14}; do
    complete=false
    for budget in {0..127}; do
        timeout 30s "$work/bridge_state" "$kind" "$budget" "$work/state-$kind-$budget.xml" > "$work/cold.txt"
        cat "$work/cold.txt" >> "$work/cold-all.txt"
        if grep -q 'injected=0 PASS$' "$work/cold.txt"; then complete=true; break; fi
    done
    if [[ $complete != true ]]; then printf 'cold state %s did not complete\n' "$kind" >&2; exit 1; fi
done
timeout 120s "$cc" "${flags[@]}" release/extlibs/xml.sax/examples/catalog_stream_c.c \
    "$work/xrt_runtime.o" -lm -ldl -o "$work/catalog_c"
timeout 30s "$work/catalog_c" > "$work/catalog_c.txt"
diff -u release/extlibs/xml.sax/examples/catalog_stream.expected.txt "$work/catalog_c.txt"
timeout 90s release/xl run release/extlibs/xml.sax/test/test_bridge_state.c
timeout 90s release/xl run release/extlibs/xml.sax/test/test_bridge_state.c -- threads
timeout 120s release/xl build release/extlibs/xml.sax/test/test_bridge_state.c -o "$work/state_aot"
timeout 45s "$work/state_aot"
timeout 45s "$work/state_aot" threads
for source in catalog_stream.xl catalog_stream_c.c; do
    timeout 90s release/xl run "release/extlibs/xml.sax/examples/$source" > "$work/$source.jit.txt"
    timeout 120s release/xl build "release/extlibs/xml.sax/examples/$source" -o "$work/$source.aot"
    timeout 45s "$work/$source.aot" > "$work/$source.aot.txt"
    diff -u release/extlibs/xml.sax/examples/catalog_stream.expected.txt "$work/$source.jit.txt"
    diff -u "$work/$source.jit.txt" "$work/$source.aot.txt"
done
for package in xml.sax xml.dom xml.etree; do
    source="release/extlibs/$package/test/test_${package/xml./xml_}.c"
    binary="$work/test_${package/xml./xml_}"
    "$cc" "${flags[@]}" "$source" "$work/xrt_runtime.o" \
        -lm -ldl -o "$binary"
    timeout 90s "$binary"
done

xl=release/xl
if [[ ! -x "$xl" ]]; then
    printf 'XML trees: Linux xl runner is missing: %s\n' "$xl" >&2
    exit 1
fi
for kind in sax dom etree; do
    timeout 90s "$xl" run "test/cases/extlib_xml_$kind.xl"
    timeout 90s "$xl" run "release/extlibs/xml.$kind/test/boundaries.xl"
    timeout 90s "$xl" run "release/extlibs/xml.$kind/test/allocations.xl"
    if [[ $kind == dom ]]; then
        for case in private_native_negative private_pointer_negative; do
            if timeout 90s "$xl" check "release/extlibs/xml.dom/test/$case.xl" > "$work/$case.txt" 2>&1; then
                printf 'xml.dom: %s must be rejected\n' "$case" >&2
                exit 1
            fi
        done
        if ! grep -Fq 'createElementNative' "$work/private_native_negative.txt" ||
           ! grep -Fq 'access' "$work/private_native_negative.txt" ||
           ! grep -Fq 'appendChildNative' "$work/private_pointer_negative.txt" ||
           ! grep -Fq 'access' "$work/private_pointer_negative.txt"; then
            cat "$work/private_native_negative.txt" "$work/private_pointer_negative.txt" >&2
            printf 'xml.dom: expected private-access diagnostics\n' >&2
            exit 1
        fi
    fi
    if [[ $kind == etree ]]; then
        for case in constructor_access_negative private_native_negative private_pointer_negative; do
            if timeout 90s "$xl" check "release/extlibs/xml.etree/test/$case.xl" > "$work/etree_$case.txt" 2>&1; then
                printf 'xml.etree: %s must be rejected\n' "$case" >&2
                exit 1
            fi
        done
        if ! grep -Fq 'nameNative' "$work/etree_private_pointer_negative.txt" ||
           ! grep -Fq 'access' "$work/etree_private_pointer_negative.txt"; then
            cat "$work/etree_private_pointer_negative.txt" >&2
            printf 'xml.etree: expected private pointer diagnostic\n' >&2
            exit 1
        fi
    fi
    timeout 90s "$xl" run "release/examples/extlibs/xml_${kind}_c.c" \
        > "$work/${kind}_c.txt"
    timeout 90s "$xl" run "release/examples/extlibs/xml_${kind}.xl" \
        > "$work/${kind}_xl.txt"
    diff -u "release/extlibs/xml.sax/test/xml_${kind}.expected.txt" \
        "$work/${kind}_c.txt"
    diff -u "$work/${kind}_c.txt" "$work/${kind}_xl.txt"
done
timeout 90s "$xl" run test/cases/extlib_xml_tree_interop.xl
for kind in sax dom etree; do
    for case in boundaries allocations; do
        timeout 120s "$xl" build "release/extlibs/xml.$kind/test/$case.xl" \
            -o "$work/${kind}_${case}_aot"
        timeout 45s "$work/${kind}_${case}_aot" > "$work/${kind}_${case}_aot.txt"
        if [[ $case == boundaries ]]; then
            printf 'xml.%s boundaries: ok\n' "$kind" > "$work/expected.txt"
        else
            printf 'xml.%s allocation: PASS\n' "$kind" > "$work/expected.txt"
        fi
        diff -u "$work/expected.txt" "$work/${kind}_${case}_aot.txt"
    done
    timeout 120s "$xl" build "test/cases/extlib_xml_$kind.xl" \
        -o "$work/${kind}_central_aot"
    timeout 45s "$work/${kind}_central_aot"
    for language in xl c; do
        if [[ $language == xl ]]; then
            source="release/examples/extlibs/xml_${kind}.xl"
        else
            source="release/examples/extlibs/xml_${kind}_c.c"
        fi
        timeout 120s "$xl" build "$source" \
            -o "$work/${kind}_${language}_example_aot"
        timeout 45s "$work/${kind}_${language}_example_aot" \
            > "$work/${kind}_${language}_aot.txt"
        diff -u "$work/${kind}_c.txt" "$work/${kind}_${language}_aot.txt"
    done
done
timeout 120s "$xl" build test/cases/extlib_xml_tree_interop.xl \
    -o "$work/xml_tree_interop_aot"
timeout 45s "$work/xml_tree_interop_aot"
sha256sum -c "$work/inputs.before.sha256" > "$work/inputs.checked.txt"
# 与 Windows 一致保留公开类型正例；M9 未关闭时整包门禁必须红。
timeout 90s "$xl" check dev/extlib_blockers/xml_sax_reader_type.xl
printf 'XML SAX/trees Linux C ASan/UBSan and XLang JIT/AOT golden gate: PASS\n'
