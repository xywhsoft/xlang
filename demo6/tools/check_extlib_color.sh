#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"
if [[ -n "${COLOR_WORK_DIR:-}" ]]; then
    work=$COLOR_WORK_DIR
else
    work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-color-XXXXXX")
    trap 'rm -rf -- "$work"' EXIT
fi
mkdir -p "$work"
pkg=release/extlibs/color
cc=${CC:-gcc}
snapshot() {
    local paths=()
    mapfile -d '' paths < <(find "$pkg" -type f -print0 | sort -z)
    paths+=(lib/xrt/xrt.h test/support/extlib_color_runtime.c tools/check_extlib_color.sh
            tools/check_extlib_color_oracle.py release/examples/extlibs/color_basic.expected.txt
            release/examples/extlibs/color_basic.xl release/examples/extlibs/color.xl
            test/cases/extlib_color.xl release/xl)
    if [[ -n "${COLOR_DECLARATIONS:-}" ]]; then paths+=("$COLOR_DECLARATIONS"); fi
    sha256sum "${paths[@]}"
}
snapshot > "$work/inputs.before.sha256"
"$cc" --version > "$work/compiler.txt"
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
for name in test_color test_threads; do
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
if [[ -n "${COLOR_DECLARATIONS:-}" ]]; then "$cc" "${flags[@]}" -fsyntax-only "$COLOR_DECLARATIONS"; fi
snapshot > "$work/inputs.after.sha256"
diff -u "$work/inputs.before.sha256" "$work/inputs.after.sha256"
printf 'color Linux C ASan/UBSan/float-cast, XLang JIT/AOT and parity gate: PASS\n'
