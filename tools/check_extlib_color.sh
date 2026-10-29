#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"
if [[ -n "${COLOR_WORK_DIR:-}" ]]; then
    work=$COLOR_WORK_DIR
    owned_work=0
else
    work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-color-XXXXXX")
    owned_work=1
fi
mkdir -p "$work"
pkg=release/extlibs/color
cc=${CC:-gcc}
snapshot() {
    local paths=()
    mapfile -d '' paths < <(find "$pkg" -type f -print0 | sort -z)
    mapfile -d '' headers < <(find release/include -type f -print0 | sort -z)
    paths+=("${headers[@]}")
    paths+=(lib/xrt/xrt.h lib/xruntime/xruntime.h test/support/extlib_color_runtime.c tools/check_extlib_color.sh
            tools/check_extlib_color_oracle.py release/examples/extlibs/color_basic.expected.txt
            release/examples/extlibs/color_basic.xl release/examples/extlibs/color.xl
            test/cases/extlib_color.xl release/xl release/libxl.so release/libxlc.so
            "$(command -v "$cc")" "$(command -v python3)")
    if [[ -n "${COLOR_DECLARATIONS:-}" ]]; then paths+=("$COLOR_DECLARATIONS"); fi
    sha256sum "${paths[@]}"
}
finish_gate() {
    result=$?
    trap - EXIT
    set +e
    cd "$root" || exit 125
    snapshot > "$work/inputs.after.sha256"
    if ! diff -u "$work/inputs.before.sha256" "$work/inputs.after.sha256"; then result=125; fi
    printf '%s\n' "$result" > "$work/exit-code.txt"
    if ((result != 0)); then printf '[color] FAIL exit=%s artifacts=%s\n' "$result" "$work" >&2
    elif ((owned_work)); then rm -rf -- "$work"; fi
    exit "$result"
}
trap finish_gate EXIT
trap 'printf "exit=%s\nline=%s\ncommand=%s\n" "$?" "$LINENO" "$BASH_COMMAND" > "$work/failure.txt"' ERR
snapshot > "$work/inputs.before.sha256"
"$cc" --version > "$work/compiler.txt"
python3 --version > "$work/python.txt"
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt -I"$pkg/include"
       -fsanitize=address,undefined,float-cast-overflow -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
"$cc" "${flags[@]}" -c test/support/extlib_color_runtime.c -o "$work/runtime.o"
objects=("$work/runtime.o")
mapfile -t sources < <(python3 -c 'import json; print("\n".join(json.load(open("release/extlibs/color/xlang.package.json"))["artifact"]["cSources"]))')
for source in "${sources[@]}"; do
    object="$work/${source%.c}.o"
    "$cc" "${flags[@]}" -c "$pkg/$source" -o "$object"
    objects+=("$object")
done
for name in test_color test_contracts test_threads; do
    "$cc" "${flags[@]}" -DNDEBUG "$pkg/test/$name.c" "${objects[@]}" -lm -ldl -o "$work/$name"
    timeout 90s "$work/$name" 2>&1 | tee "$work/$name.log"
done
"$cc" "${flags[@]}" "$pkg/test/oracle.c" "${objects[@]}" -lm -ldl -o "$work/oracle"
python3 tools/check_extlib_color_oracle.py "$work/oracle" "$work/oracle.json" | tee "$work/oracle.log"
"$cc" "${flags[@]}" -DNDEBUG "$pkg/examples/basic.c" "$work/runtime.o" -lm -ldl -o "$work/basic"
timeout 30s "$work/basic" > "$work/basic.log"
diff -u <(tr -d '\r' < release/examples/extlibs/color_basic.expected.txt) "$work/basic.log"
timeout 90s release/xl run "$pkg/examples/basic.c" > "$work/basic_direct.log"
diff -u "$work/basic.log" "$work/basic_direct.log"
if [[ ! -x release/xl ]]; then
    printf 'missing Linux release/xl for color XLang gate\n' >&2
    exit 1
fi
timeout 90s release/xl run test/cases/extlib_color.xl
timeout 90s release/xl run "$pkg/test/boundaries.xl"
timeout 90s release/xl run "$pkg/test/allocations.xl"
timeout 90s release/xl run release/examples/extlibs/color_basic.xl > "$work/basic_xl.log"
diff -u "$work/basic.log" "$work/basic_xl.log"
timeout 90s release/xl run release/examples/extlibs/color.xl > "$work/palette_xl.log"
timeout 90s release/xl run "$pkg/examples/theme.xl" > "$work/theme_xl.log"
diff -u <(sed 's/\r$//' "$pkg/examples/theme.expected.txt") "$work/theme_xl.log"
"$cc" "${flags[@]}" -DNDEBUG "$pkg/examples/theme_c.c" "$work/runtime.o" -lm -ldl -o "$work/theme-c"
timeout 30s "$work/theme-c" > "$work/theme_c.log"
diff -u "$work/theme_xl.log" "$work/theme_c.log"
timeout 90s release/xl run "$pkg/examples/theme_c.c" > "$work/theme_direct.log"
diff -u "$work/theme_xl.log" "$work/theme_direct.log"

# 核对实际生成的全部 21 个声明，不依赖由 Windows 导入的可选声明文件。
timeout 90s release/xl cgen test/cases/extlib_color.xl --emit "$work/module.c"
grep -E '^extern .*xcolor_[a-z_]+[(]' "$work/module.c" | sort -u > "$work/abi.decls"
grep -oE '"xcolor_[a-z_]+"' "$pkg/src/color.xl" | tr -d '"' | sort -u > "$work/abi.expected"
grep -oE 'xcolor_[a-z_]+[(]' "$work/abi.decls" | tr -d '(' | sort -u > "$work/abi.actual"
test "$(wc -l < "$work/abi.decls")" -eq 21
diff -u "$work/abi.expected" "$work/abi.actual"
printf '#include "color_native.h"\n' > "$work/abi.c"
cat "$work/abi.decls" >> "$work/abi.c"
"$cc" "${flags[@]}" -fsyntax-only "$work/abi.c"
for name in hex_text hex_a_text normalize_text from_hsv_text from_hsl_text mix_text from_name_text to_name_text; do
    grep -Eq "[.]Data[[:space:]]*=[[:space:]]*xcolor_$name[[:space:]]*[(]" "$work/module.c"
done
timeout 120s release/xl build test/cases/extlib_color.xl \
    -o "$work/color_central_xl"
timeout 90s "$work/color_central_xl"
for name in boundaries allocations; do
    timeout 120s release/xl build "$pkg/test/$name.xl" -o "$work/color_$name"
    timeout 90s "$work/color_$name"
done
timeout 120s release/xl build release/examples/extlibs/color_basic.xl \
    -o "$work/color_basic_xl"
timeout 45s "$work/color_basic_xl" > "$work/basic_aot.log"
diff -u "$work/basic.log" "$work/basic_aot.log"
timeout 120s release/xl build release/examples/extlibs/color.xl \
    -o "$work/color_palette_xl"
timeout 45s "$work/color_palette_xl" > "$work/palette_aot.log"
diff -u "$work/palette_xl.log" "$work/palette_aot.log"
timeout 120s release/xl build "$pkg/examples/basic.c" \
    -o "$work/color_basic_c_aot"
timeout 45s "$work/color_basic_c_aot" > "$work/basic_c_aot.log"
diff -u "$work/basic.log" "$work/basic_c_aot.log"
for source in theme.xl theme_c.c; do
    binary="$work/${source//./-}"
    timeout 120s release/xl build "$pkg/examples/$source" -o "$binary"
    timeout 45s "$binary" > "$work/$source-aot.log"
    diff -u "$work/theme_xl.log" "$work/$source-aot.log"
done
if [[ -n "${COLOR_DECLARATIONS:-}" ]]; then "$cc" "${flags[@]}" -fsyntax-only "$COLOR_DECLARATIONS"; fi
printf 'color Linux C ASan/UBSan/float-cast, XLang JIT/AOT and parity gate: PASS\n'
