#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-gettext-XXXXXX")
cleanup() {
    result=$?
    trap - EXIT
    set +e
    if ((result == 0)); then
        rm -rf -- "$work" || result=$?
    else
        printf '[gettext] FAIL exit=%s artifacts=%s\n' "$result" "$work" >&2
    fi
    exit "$result"
}
trap cleanup EXIT
record_failure() {
    printf 'exit=%s\nline=%s\ncommand=%s\npipeline=%s\n' "$@" > "$work/failure.txt"
    cat "$work/failure.txt" >&2
}
trap 'record_failure "$?" "$LINENO" "$BASH_COMMAND" "${PIPESTATUS[*]}"' ERR
cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror
       -Ilib/xrt -Irelease/extlibs/gettext
       -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

mapfile -d '' inputs < <(
    { find release/extlibs/gettext -type f -print0
      printf '%s\0' lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so \
          release/libxlc.so release/include/xrt_decl.h release/include/xruntime_decl.h \
          release/include/xl_extlib_abi.h test/cases/extlib_gettext.xl \
          test/fixtures/gettext_mini.mo release/examples/extlibs/gettext.xl
    } | sort -zu
)
sha256sum "${inputs[@]}" > "$work/inputs.before.sha256"
"$cc" --version > "$work/compiler.txt"

# 独立编译共同对象，边界、范例与外部目录测试不再反复编译整份 XRT。
"$cc" "${flags[@]}" -c release/extlibs/gettext/test/xrt_runtime.c -o "$work/xrt.o"
"$cc" "${flags[@]}" -c release/extlibs/gettext/gettext_native.c -o "$work/native.o"
"$cc" "${flags[@]}" -c release/extlibs/gettext/gettext_plural.c -o "$work/plural.o"
native=("$work/native.o" "$work/plural.o" "$work/xrt.o")
"$cc" "${flags[@]}" release/extlibs/gettext/test/test_gettext.c "${native[@]}" \
    -no-pie -lm -ldl -o "$work/test_gettext"
timeout 90s "$work/test_gettext" "$work"

"$cc" "${flags[@]}" release/extlibs/gettext/test/test_context_queries.c "${native[@]}" \
    -no-pie -lm -ldl -o "$work/test_context_queries"
timeout 30s "$work/test_context_queries"
timeout 90s ./release/xl run test/cases/extlib_gettext.xl
timeout 90s ./release/xl run release/extlibs/gettext/test/duplicates.xl
timeout 90s ./release/xl run release/extlibs/gettext/test/boundaries.xl
timeout 90s ./release/xl run release/extlibs/gettext/test/allocations.xl
timeout 90s ./release/xl run release/extlibs/gettext/test/input_limits.xl

"$cc" "${flags[@]}" release/extlibs/gettext/examples/gettext_c.c "$work/xrt.o" \
    -no-pie -lm -ldl -o "$work/example"
timeout 30s "$work/example" > "$work/example_c.txt"
timeout 90s ./release/xl run release/extlibs/gettext/examples/gettext_c.c > "$work/example_direct_c.txt"
diff -u "$work/example_c.txt" "$work/example_direct_c.txt"
( cd release && timeout 90s ./xl run extlibs/gettext/examples/gettext.xl ) > "$work/example_xl.txt"
diff -u "$work/example_c.txt" "$work/example_xl.txt"
( cd release && timeout 90s ./xl run examples/extlibs/gettext.xl ) > "$work/public_xl.txt"
for name in duplicates boundaries allocations input_limits; do
    timeout 120s ./release/xl build "release/extlibs/gettext/test/$name.xl" \
        -o "$work/gettext_$name"
    timeout 90s "$work/gettext_$name"
done
timeout 120s ./release/xl build test/cases/extlib_gettext.xl \
    -o "$work/gettext_central"
timeout 90s "$work/gettext_central"
( cd release && timeout 120s ./xl build extlibs/gettext/examples/gettext.xl \
    -o "$work/gettext_example" )
( cd release && timeout 90s "$work/gettext_example" ) > "$work/example_aot.txt"
diff -u "$work/example_c.txt" "$work/example_aot.txt"
timeout 120s ./release/xl build release/examples/extlibs/gettext.xl \
    -o "$work/gettext_public_example"
( cd release && timeout 90s "$work/gettext_public_example" ) > "$work/public_aot.txt"
diff -u "$work/public_xl.txt" "$work/public_aot.txt"
timeout 120s ./release/xl build release/extlibs/gettext/examples/gettext_c.c \
    -o "$work/gettext_c_example"
timeout 90s "$work/gettext_c_example" > "$work/example_c_aot.txt"
diff -u "$work/example_c.txt" "$work/example_c_aot.txt"
if command -v msgfmt >/dev/null 2>&1; then
    msgfmt --check -o "$work/external.mo" release/extlibs/gettext/test/external.po
    "$cc" "${flags[@]}" release/extlibs/gettext/test/external_mo_c.c "${native[@]}" \
        -no-pie -lm -ldl -o "$work/external_c"
    timeout 90s "$work/external_c" "$work/external.mo"
    timeout 90s ./release/xl run release/extlibs/gettext/test/external_mo.xl \
        -- "$work/external.mo"
    timeout 120s ./release/xl build release/extlibs/gettext/test/external_mo.xl \
        -o "$work/external_xl"
    timeout 90s "$work/external_xl" "$work/external.mo"
else
    printf 'gettext GNU msgfmt interop: SKIP (msgfmt unavailable)\n'
fi
sha256sum "${inputs[@]}" > "$work/inputs.after.sha256"
diff -u "$work/inputs.before.sha256" "$work/inputs.after.sha256"
printf 'gettext Linux C ASan/UBSan, XLang JIT/AOT and parity gate: PASS\n'
