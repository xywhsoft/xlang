#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-imagehash-XXXXXX")
cleanup() {
    result=$?
    trap - EXIT
    set +e
    if ((result == 0)); then
        rm -rf -- "$work" || result=$?
    else
        printf '[imagehash] FAIL exit=%s artifacts=%s\n' "$result" "$work" >&2
    fi
    exit "$result"
}
trap cleanup EXIT
record_failure() {
    printf 'exit=%s\nline=%s\ncommand=%s\npipeline=%s\n' "$@" > "$work/failure.txt"
    cat "$work/failure.txt" >&2
}
trap 'record_failure "$?" "$LINENO" "$BASH_COMMAND" "${PIPESTATUS[*]}"' ERR
exec > >(tee "$work/output.log") 2>&1
cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror
       -Ilib/xrt -Irelease/extlibs/imagehash
       -Irelease/extlibs/imagehash/include
       -ffunction-sections -fdata-sections
       -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
mapfile -d '' inputs < <(find release/extlibs/imagehash release/extlibs/stb_image \
    release/extlibs/stb_image_resize2 release/include -type f -print0)
inputs+=(lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so release/libxlc.so
          test/cases/extlib_imagehash.xl release/examples/extlibs/imagehash.xl
          release/examples/extlibs/assets/tiny.png release/examples/games/assets/player.png)
sha256sum "${inputs[@]}" > "$work/inputs.before.sha256"
"$cc" --version > "$work/compiler.txt"
golden="$work/imagehash.expected.txt"
sed 's/\r$//' release/extlibs/imagehash/examples/imagehash.expected.txt > "$golden"

"$cc" "${flags[@]}" -c release/extlibs/imagehash/test/runtime.c -o "$work/runtime.o"
"$cc" "${flags[@]}" -c release/extlibs/imagehash/imagehash_native.c \
    -o "$work/imagehash.o"
"$cc" "${flags[@]}" -c release/extlibs/stb_image/stb_image_native.c \
    -o "$work/stb_image.o"
"$cc" "${flags[@]}" -c release/extlibs/stb_image_resize2/stb_image_resize2_native.c \
    -o "$work/stb_image_resize2.o"
for name in test_error_threads test_imagehash; do
    "$cc" "${flags[@]}" -c \
        "release/extlibs/imagehash/test/$name.c" -o "$work/$name.o"
    "$cc" -fsanitize=address,undefined -no-pie -Wl,--gc-sections \
        "$work/$name.o" "$work/imagehash.o" "$work/stb_image.o" \
        "$work/stb_image_resize2.o" "$work/runtime.o" -pthread -lm -ldl -o "$work/$name"
    timeout 90s "$work/$name"
done

# 固定预算只比较普通构建；原有全部故障点和图像边界仍由上面的 sanitizer 验证。
budget_flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt -pthread)
"$cc" "${budget_flags[@]}" release/extlibs/imagehash/test/runtime.c \
    release/extlibs/imagehash/test/test_owned_text.c release/extlibs/imagehash/imagehash_native.c \
    release/extlibs/stb_image/stb_image_native.c \
    release/extlibs/stb_image_resize2/stb_image_resize2_native.c -lm -ldl -o "$work/budget_c"
timeout 90s "$work/budget_c"

xl=./release/xl
if [[ ! -x "$xl" ]]; then
    printf 'missing Linux xlang runner: %s\n' "$xl" >&2
    exit 1
fi
timeout 90s "$xl" run release/extlibs/imagehash/test/test_owned_text.c
timeout 90s "$xl" run test/cases/extlib_imagehash.xl
timeout 90s "$xl" run release/extlibs/imagehash/test/allocations.xl
timeout 90s "$xl" run release/extlibs/imagehash/test/owned_results.xl -- 14
timeout 90s "$xl" cgen release/extlibs/imagehash/test/owned_probe.xl --emit "$work/owned_probe.c"
for symbol in ahash dhash phash; do
    count=$(grep -Ec "[[:alnum:]_]+\.Data[[:space:]]*=[[:space:]]*xihash_${symbol}[[:space:]]*\(" \
        "$work/owned_probe.c")
    if [[ $count != 1 ]]; then
        printf 'imagehash probe must adopt %s exactly once\n' "$symbol" >&2
        exit 1
    fi
done
if grep -Eq '= *xlir_string_copy *\(' "$work/owned_probe.c"; then
    printf 'imagehash copied the adopted result in the complete public chain\n' >&2
    exit 1
fi

"$cc" "${flags[@]}" -c \
    release/extlibs/imagehash/examples/imagehash.c -o "$work/example.o"
"$cc" -fsanitize=address,undefined -no-pie -Wl,--gc-sections \
    "$work/example.o" "$work/imagehash.o" "$work/stb_image.o" \
    "$work/stb_image_resize2.o" "$work/runtime.o" -pthread -lm -ldl -o "$work/example"
timeout 90s "$work/example" > "$work/c.out"
timeout 90s "$xl" run release/examples/extlibs/imagehash.xl > "$work/xlang.out"
diff -u "$work/c.out" "$work/xlang.out"
diff -u "$golden" "$work/c.out"
timeout 90s "$xl" run release/extlibs/imagehash/examples/imagehash.c > "$work/direct.out"
diff -u "$work/c.out" "$work/direct.out"
timeout 90s "$xl" build release/extlibs/imagehash/test/aot_core.xl -o "$work/imagehash_core"
timeout 30s "$work/imagehash_core"
timeout 90s "$xl" build release/extlibs/imagehash/test/allocations.xl -o "$work/imagehash_allocations"
timeout 30s "$work/imagehash_allocations"
timeout 90s "$xl" build test/cases/extlib_imagehash.xl -o "$work/imagehash_central"
timeout 30s "$work/imagehash_central"
timeout 90s "$xl" build release/extlibs/imagehash/test/owned_results.xl -o "$work/imagehash_owned"
timeout 30s "$work/imagehash_owned" 14
timeout 90s "$xl" build release/extlibs/imagehash/test/test_owned_text.c -o "$work/imagehash_owned_c"
timeout 30s "$work/imagehash_owned_c"
timeout 90s "$xl" build release/examples/extlibs/imagehash.xl -o "$work/imagehash_xlang"
timeout 30s "$work/imagehash_xlang" > "$work/xlang_aot.out"
diff -u "$golden" "$work/xlang_aot.out"
timeout 90s "$xl" build release/extlibs/imagehash/examples/imagehash.c -o "$work/imagehash_c"
timeout 30s "$work/imagehash_c" > "$work/c_aot.out"
diff -u "$golden" "$work/c_aot.out"
sha256sum -c "$work/inputs.before.sha256" > "$work/inputs.after.txt"
printf 'imagehash Linux C ASan/UBSan, XLang JIT and bilingual AOT gate: PASS\n'
