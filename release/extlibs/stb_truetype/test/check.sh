#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-stbtt-XXXXXXXX")
pkg=release/extlibs/stb_truetype;cc=${CC:-gcc};cc_path=$(command -v "$cc");cc1=$("$cc" -print-prog-name=cc1)
inputs() {
    find "$pkg" release/include -type f -print0
    printf '%s\0' lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so release/libxlc.so \
        test/cases/extlib_stb_truetype.xl release/examples/extlibs/stb_truetype.xl release/examples/extlibs/stb_truetype_c.c "$cc_path" "$cc1" /usr/bin/bash
}
mapfile -d '' paths < <(inputs|sort -zu);sha256sum "${paths[@]}" > "$work/inputs.before.sha256"
finish() {
    local result=$?;trap - EXIT;set +e
    mapfile -d '' after < <(inputs|sort -zu);sha256sum "${after[@]}" > "$work/inputs.after.sha256"
    if ! diff -u "$work/inputs.before.sha256" "$work/inputs.after.sha256" > "$work/input-changes.txt";then result=125;fi
    printf '%s\n' "$result" > "$work/exit.txt"
    if ((result==0));then rm -rf -- "$work";else printf '[stbtt] FAIL exit=%s artifacts=%s\n' "$result" "$work" >&2;fi
    exit "$result"
}
trap finish EXIT
run() {
    local label=$1 expected=$2 actual=0;shift 2
    printf '%q ' "$@" > "$work/$label.command";printf '\n' >> "$work/$label.command"
    timeout 120s "$@" > "$work/$label.stdout" 2> "$work/$label.stderr" || actual=$?
    printf '%s\t%s\t%s\n' "$label" "$expected" "$actual" >> "$work/phases.tsv"
    cat "$work/$label.stdout";cat "$work/$label.stderr" >&2
    printf '[stbtt] %s exit=%s expected=%s\n' "$label" "$actual" "$expected"
    ((actual==expected))
}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt -I"$pkg/include"
    -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
run compiler 0 "$cc" --version
run runtime 0 "$cc" "${flags[@]}" -c "$pkg/test/xrt_runtime.c" -o "$work/runtime.o"
run native 0 "$cc" "${flags[@]}" -Wmissing-prototypes -c "$pkg/stb_truetype_native.c" -o "$work/native.o"
run core-compile 0 "$cc" "${flags[@]}" "$pkg/test/test_stb_truetype.c" "$work/native.o" "$work/runtime.o" -lm -ldl -o "$work/core"
run core 0 "$work/core"
run discovery-compile 0 "$cc" "${flags[@]}" "$pkg/test/discovery_contract_c.c" "$work/runtime.o" -lm -ldl -o "$work/discovery"
run discovery-GCC 0 "$work/discovery"
run discovery-TCC 0 ./release/xl run "$pkg/test/discovery_contract_c.c"
run discovery-build 0 ./release/xl build "$pkg/test/discovery_contract_c.c" -o "$work/discovery.aot"
run discovery-AOT 0 "$work/discovery.aot"
diff -u "$work/discovery-GCC.stdout" "$work/discovery-TCC.stdout"
diff -u "$work/discovery-GCC.stdout" "$work/discovery-AOT.stdout"
font="$root/$pkg/test/assets/DejaVuSans-test.ttf"
run old-C-compile 0 "$cc" "${flags[@]}" release/examples/extlibs/stb_truetype_c.c "$work/runtime.o" -lm -ldl -o "$work/old-example"
run old-C-GCC 0 "$work/old-example" "$font"
run old-C-TCC 0 ./release/xl run release/examples/extlibs/stb_truetype_c.c -- "$font"
run old-XL-JIT 0 ./release/xl run release/examples/extlibs/stb_truetype.xl -- "$font"
diff -u "$work/old-C-GCC.stdout" "$work/old-C-TCC.stdout";diff -u "$work/old-C-GCC.stdout" "$work/old-XL-JIT.stdout"
run mask-compile 0 "$cc" "${flags[@]}" "$pkg/examples/mask_c.c" "$work/native.o" "$work/runtime.o" -lm -ldl -o "$work/mask"
run mask-GCC 0 "$work/mask" "$font"
run mask-TCC 0 ./release/xl run "$pkg/examples/mask_c.c" -- "$font"
for engine in GCC TCC;do diff -u <(sed 's/\r$//' "$pkg/examples/mask.expected.txt") "$work/mask-$engine.stdout";done
for source in test/cases/extlib_stb_truetype.xl "$pkg/test/boundaries.xl" "$pkg/test/allocations.xl" "$pkg/test/api_surface.xl" "$pkg/examples/mask.xl";do
    name=$(basename "$source" .xl)
    run "$name-JIT" 0 ./release/xl run "$source"
    run "$name-build" 0 ./release/xl build "$source" -o "$work/$name.aot"
    run "$name-AOT" 0 "$work/$name.aot"
    diff -u "$work/$name-JIT.stdout" "$work/$name-AOT.stdout"
    if [[ $name == mask ]];then diff -u <(sed 's/\r$//' "$pkg/examples/mask.expected.txt") "$work/$name-JIT.stdout";fi
done
for source in release/examples/extlibs/stb_truetype_c.c release/examples/extlibs/stb_truetype.xl "$pkg/examples/mask_c.c";do
    name=$(basename "$source" .xl);name=${name%.c}
    run "$name-build" 0 ./release/xl build "$source" -o "$work/$name.aot"
    run "$name-AOT" 0 "$work/$name.aot" "$font"
    if [[ $name == mask_c ]];then diff -u <(sed 's/\r$//' "$pkg/examples/mask.expected.txt") "$work/$name-AOT.stdout";else diff -u "$work/old-C-GCC.stdout" "$work/$name-AOT.stdout";fi
done
run cgen 0 ./release/xl cgen "$pkg/test/api_surface.xl" --emit "$work/module.c"
grep -E '^extern .*xstbtt_[a-z_]+[(]' "$work/module.c"|sort -u > "$work/decls.txt"
grep -oE 'extern "xstbtt_[a-z_]+"' "$pkg/src/stb_truetype.xl"|cut -d '"' -f 2|sort -u > "$work/expected.txt"
grep -oE 'xstbtt_[a-z_]+' "$work/decls.txt"|sort -u > "$work/actual.txt"
[[ $(wc -l < "$work/decls.txt") == 25 ]];diff -u "$work/expected.txt" "$work/actual.txt"
printf '#include "stb_truetype_native.h"\n' > "$work/abi.c";cat "$work/decls.txt" >> "$work/abi.c"
run generated-ABI 0 "$cc" "${flags[@]}" -fsyntax-only "$work/abi.c"
sed 's/double xstbtt_font_scale/int xstbtt_font_scale/' "$work/abi.c" > "$work/bad-abi.c"
run wrong-return-control 1 "$cc" "${flags[@]}" -fsyntax-only "$work/bad-abi.c"
run private-method 1 ./release/xl check "$pkg/test/private_bitmap_negative.xl"
grep -q bitmapNative "$work/private-method.stdout" "$work/private-method.stderr"
grep -q access "$work/private-method.stdout" "$work/private-method.stderr"
issues=0
for type in font bitmap;do
    run "private-$type-constructor" 1 ./release/xl check "$pkg/test/private_${type}_constructor_negative.xl" || issues=$((issues+1))
    actual=$(tail -n 1 "$work/phases.tsv"|cut -f 3)
    if [[ $actual != 0 ]];then
        grep -iq "$type" "$work/private-$type-constructor.stdout" "$work/private-$type-constructor.stderr"
        grep -q access "$work/private-$type-constructor.stdout" "$work/private-$type-constructor.stderr"
    fi
done
if ((issues));then printf 'stbtt private constructor blockers: %s\n' "$issues" >&2;exit 1;fi
printf 'stbtt full sanitizer, bilingual, ABI and access gate: PASS\n'
