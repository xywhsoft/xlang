#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-md4c-XXXXXX")
cleanup() {
    result=$?
    trap - EXIT
    set +e
    cd "$root" || exit 125
    mapfile -d '' after < <(input_paths | sort -zu)
    sha256sum "${after[@]}" > "$work/inputs.after.sha256"
    if ! diff -u "$work/inputs.before.sha256" "$work/inputs.after.sha256"; then result=125; fi
    printf 'exit=%s\n' "$result" > "$work.report.txt"
    cp -- "$work/inputs.before.sha256" "$work.inputs.sha256"
    if ((result == 0)); then rm -rf -- "$work" || result=$?
    else printf '[md4c] FAIL exit=%s artifacts=%s\n' "$result" "$work" >&2; fi
    printf '[md4c] report: %s.report.txt\n' "$work"
    exit "$result"
}
trap cleanup EXIT
record_failure() {
    printf 'exit=%s\nline=%s\ncommand=%s\npipeline=%s\n' "$@" > "$work/failure.txt"
    cat "$work/failure.txt" >&2
}
trap 'record_failure "$?" "$LINENO" "$BASH_COMMAND" "${PIPESTATUS[*]}"' ERR
cc=${CC:-gcc}
cc_path=$(command -v "$cc" || true)
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror
       -Ilib/xrt -Irelease/include -Irelease/extlibs/md4c
       -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -pthread)
native=(release/extlibs/md4c/md4c_native.c
        release/extlibs/md4c/third_party/md4c/md4c.c
        release/extlibs/md4c/third_party/md4c/md4c-html.c
        release/extlibs/md4c/third_party/md4c/entity.c)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
export XLANG_PACKAGE_PATH="$root/release/extlibs"

input_paths() {
    { find release/extlibs/md4c release/include -type f -print0
      printf '%s\0' lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so \
          release/libxlc.so test/cases/extlib_md4c.xl release/examples/extlibs/md4c.xl
      if [[ -n "$cc_path" ]]; then printf '%s\0' "$cc_path"; fi
    }
}
mapfile -d '' inputs < <(input_paths | sort -zu)
sha256sum "${inputs[@]}" > "$work/inputs.before.sha256"
"$cc" --version > "$work/compiler.txt"
compile() { timeout 180s "$cc" "${flags[@]}" "$@"; }

compile -DXRT_MODULE_BUFFER -DXRT_MODULE_FILE_WHOLE -DXRT_MODULE_UNICODE \
    -fsyntax-only "${native[0]}"
compile -c release/extlibs/md4c/test/xrt_runtime.c -o "$work/xrt.o"
compile "${native[@]}" release/extlibs/md4c/test/test_md4c.c "$work/xrt.o" \
    -no-pie -lm -ldl -o "$work/test_md4c"
timeout 90s "$work/test_md4c"
for name in parser_allocations_c render_options_c; do
    source="release/extlibs/md4c/test/$name.c"
    compile -DNDEBUG "$source" "$work/xrt.o" -no-pie -lm -ldl -o "$work/$name"
    timeout 90s "$work/$name"
    timeout 90s ./release/xl run "$source"
done

compile release/extlibs/md4c/examples/md4c_c.c "$work/xrt.o" \
    -no-pie -lm -ldl -o "$work/example_c"
timeout 30s "$work/example_c" > "$work/example_c.html"
timeout 90s ./release/xl run release/extlibs/md4c/examples/md4c_c.c > "$work/example_direct_c.html"
diff -u "$work/example_c.html" "$work/example_direct_c.html"
compile release/extlibs/md4c/examples/article_c.c "$work/xrt.o" \
    -no-pie -lm -ldl -o "$work/article_c"
timeout 30s "$work/article_c" > "$work/article_c.html"
diff -u <(sed 's/\r$//' release/extlibs/md4c/examples/article.expected.html) "$work/article_c.html"
timeout 90s ./release/xl run release/extlibs/md4c/examples/article_c.c > "$work/article_direct_c.html"
diff -u "$work/article_c.html" "$work/article_direct_c.html"

for name in boundaries allocations render_options; do
    timeout 90s ./release/xl run "release/extlibs/md4c/test/$name.xl"
done
timeout 90s ./release/xl run test/cases/extlib_md4c.xl
timeout 90s ./release/xl run release/extlibs/md4c/examples/md4c.xl > "$work/example_xl.html"
diff -u "$work/example_c.html" "$work/example_xl.html"
timeout 90s ./release/xl run release/extlibs/md4c/examples/article.xl > "$work/article_xl.html"
diff -u "$work/article_c.html" "$work/article_xl.html"
timeout 90s ./release/xl run release/examples/extlibs/md4c.xl > "$work/public_xl.html"

# 用实际生成声明与正式 C 头同单元编译，避免只比对函数名称。
timeout 90s ./release/xl cgen release/extlibs/md4c/test/render_options.xl --emit "$work/module.c"
grep -E '^extern .*xmd4c_[a-z0-9_]+[(]' "$work/module.c" | sort -u > "$work/abi.decls"
grep -oE '"xmd4c_[a-z0-9_]+"' release/extlibs/md4c/src/md4c.xl | tr -d '"' | sort -u > "$work/abi.expected"
grep -oE 'xmd4c_[a-z0-9_]+[(]' "$work/abi.decls" | tr -d '(' | sort -u > "$work/abi.actual"
test "$(wc -l < "$work/abi.decls")" -eq 12
diff -u "$work/abi.expected" "$work/abi.actual"
printf '#include "md4c_native.h"\n' > "$work/abi.c"
cat "$work/abi.decls" >> "$work/abi.c"
compile -fsyntax-only "$work/abi.c"
for name in html html_flags file_html; do
    grep -Eq "[.]Data[[:space:]]*=[[:space:]]*xmd4c_$name[[:space:]]*[(]" "$work/module.c"
done
if grep -Eq 'xl6_xrt_take_cstr\(|xl6CStringCopyV1' "$work/module.c"; then
    printf 'md4c must directly adopt owned strings\n' >&2
    exit 1
fi

for source in test/boundaries.xl test/allocations.xl test/render_options.xl \
    test/parser_allocations_c.c test/render_options_c.c \
    examples/md4c.xl examples/md4c_c.c examples/article.xl examples/article_c.c; do
    target="$work/$(basename "${source%.*}")"
    timeout 180s ./release/xl build "release/extlibs/md4c/$source" -o "$target"
    timeout 90s "$target" > "$work/$(basename "$source").out"
    case "$source" in
        examples/md4c*) diff -u "$work/example_c.html" "$work/$(basename "$source").out" ;;
        examples/article*) diff -u "$work/article_c.html" "$work/$(basename "$source").out" ;;
    esac
done
timeout 180s ./release/xl build release/examples/extlibs/md4c.xl -o "$work/public_example"
timeout 90s "$work/public_example" > "$work/public_aot.html"
diff -u "$work/public_xl.html" "$work/public_aot.html"
timeout 180s ./release/xl build test/cases/extlib_md4c.xl -o "$work/central"
timeout 90s "$work/central"
printf 'md4c Linux C ASan/UBSan, twelve ABI prototypes, JIT/AOT and bilingual golden output: PASS\n'
