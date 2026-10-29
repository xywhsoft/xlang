#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"
if [[ -n "${CSV_WORK_DIR:-}" ]]; then
    work=$CSV_WORK_DIR
    mkdir -p -- "$work"
else
    work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-csv-XXXXXX")
    trap 'rm -rf -- "$work"' EXIT
fi
pkg=release/extlibs/csv
cc=${CC:-gcc}
snapshot() {
    local paths=()
    mapfile -d '' paths < <(find "$pkg" -type f -print0 | sort -z)
    paths+=(lib/xrt/xrt.h test/support/extlib_csv_runtime.c tools/check_extlib_csv.sh
            tools/check_extlib_csv_interop.py release/examples/extlibs/csv_basic.expected.txt
            release/examples/extlibs/csv_files.expected.txt release/xl
            test/cases/extlib_csv.xl release/extlibs/csv/test/boundaries.xl
            release/extlibs/csv/test/files.xl release/examples/extlibs/csv.xl
            release/examples/extlibs/csv_basic.xl release/examples/extlibs/csv_files.xl)
    if [[ -n "${CSV_DECLARATIONS:-}" ]]; then paths+=("$CSV_DECLARATIONS"); fi
    sha256sum "${paths[@]}"
}
snapshot > "$work/inputs.before.sha256"
"$cc" --version > "$work/compiler.txt"
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt -I"$pkg/include"
       -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
"$cc" "${flags[@]}" -DXRT_MODULE_THREAD -DXRT_MODULE_ATOMIC -c test/support/extlib_csv_runtime.c -o "$work/runtime.o"
objects=("$work/runtime.o")
mapfile -t sources < <(python3 -c 'import json; print("\n".join(json.load(open("release/extlibs/csv/xlang.package.json"))["artifact"]["cSources"]))')
for source in "${sources[@]}"; do
    object="$work/${source%.c}.o"
    "$cc" "${flags[@]}" -c "$pkg/$source" -o "$object"
    objects+=("$object")
done
for name in test_csv test_csv_boundary test_csv_stream test_csv_alloc test_csv_files test_csv_contracts test_csv_threads; do
    "$cc" "${flags[@]}" "$pkg/test/$name.c" "${objects[@]}" -lm -ldl -o "$work/$name"
    timeout 90s "$work/$name" 2>&1 | tee "$work/$name.log"
done
"$cc" "${flags[@]}" "$pkg/test/csv_interop.c" "${objects[@]}" -lm -ldl -o "$work/interop"
python3 tools/check_extlib_csv_interop.py "$work/interop" "$work/interop-data" | tee "$work/interop.log"
for name in basic files; do
    "$cc" "${flags[@]}" "$pkg/examples/$name.c" "$work/runtime.o" -lm -ldl -o "$work/$name"
    timeout 30s "$work/$name" > "$work/$name.log"
    diff -u <(tr -d '\r' < "release/examples/extlibs/csv_$name.expected.txt") "$work/$name.log"
    timeout 90s release/xl run "$pkg/examples/$name.c" > "$work/${name}_direct.log"
    diff -u "$work/$name.log" "$work/${name}_direct.log"
done
xl=release/xl
if [[ ! -x "$xl" ]]; then
    printf 'CSV Linux xl runner is missing: %s\n' "$xl" >&2
    exit 1
fi
timeout 90s "$xl" run test/cases/extlib_csv.xl
timeout 90s "$xl" run "$pkg/test/boundaries.xl"
timeout 90s "$xl" run "$pkg/test/files.xl"
if timeout 90s "$xl" check "$pkg/test/private_native.xl" > "$work/private_native.txt" 2>&1; then
    printf 'CSV Native methods must not be public\n' >&2
    exit 1
fi
if ! grep -Fq 'fieldNative' "$work/private_native.txt" ||
   ! grep -Fq 'access' "$work/private_native.txt"; then
    cat "$work/private_native.txt" >&2
    printf 'CSV expected private-access diagnostic\n' >&2
    exit 1
fi
timeout 90s "$xl" run release/examples/extlibs/csv.xl > "$work/full_xl.log"
for name in basic files; do
    timeout 90s "$xl" run "release/examples/extlibs/csv_$name.xl" > "$work/${name}_xl.log"
    diff -u <(tr -d '\r' < "release/examples/extlibs/csv_$name.expected.txt") "$work/${name}_xl.log"
    diff -u "$work/$name.log" "$work/${name}_xl.log"
done
if [[ -n "${CSV_DECLARATIONS:-}" ]]; then
    "$cc" "${flags[@]}" -fsyntax-only "$CSV_DECLARATIONS"
fi
snapshot > "$work/inputs.after.sha256"
diff -u "$work/inputs.before.sha256" "$work/inputs.after.sha256"
printf 'CSV Linux C ASan/UBSan and XLang parity gate: PASS\n'
