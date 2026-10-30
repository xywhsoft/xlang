#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-linalg-XXXXXX")
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
    else printf '[linalg] FAIL exit=%s artifacts=%s\n' "$result" "$work" >&2; fi
    printf '[linalg] report: %s.report.txt\n' "$work"
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
python_path=$(command -v python3 || true)
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror
       -Ilib/xrt -Irelease/extlibs/linalg -Irelease/extlibs/linalg/include
       -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
export XLANG_PACKAGE_PATH="$root/release/extlibs"
input_paths() {
    { find release/extlibs/linalg release/include -type f -print0
      printf '%s\0' lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so release/libxlc.so \
          release/examples/extlibs/linalg.xl release/examples/extlibs/linalg_c.c test/cases/extlib_linalg.xl
      if [[ -n "$cc_path" ]]; then printf '%s\0' "$cc_path"; fi
      if [[ -n "$python_path" ]]; then printf '%s\0' "$python_path"; fi
    }
}
mapfile -d '' inputs < <(input_paths | sort -zu)
sha256sum "${inputs[@]}" > "$work/inputs.before.sha256"
"$cc" --version > "$work/compiler.txt"
compile() { timeout 180s "$cc" "${flags[@]}" "$@"; }
native=release/extlibs/linalg/linalg_native.c
compile -DXRT_MODULE_MATH -DXRT_MODULE_VALUE_CONTAINER -fsyntax-only "$native"
timeout 90s ./release/xl cgen release/extlibs/linalg/test/numeric_edges.xl --emit "$work/module.c"
grep -E '^extern .*xlin_[a-z0-9_]+[(]' "$work/module.c" | sort -u > "$work/abi.decls"
grep -oE '"xlin_[a-z0-9_]+"' release/extlibs/linalg/src/linalg.xl | tr -d '"' | sort -u > "$work/abi.expected"
grep -oE 'xlin_[a-z0-9_]+[(]' "$work/abi.decls" | tr -d '(' | sort -u > "$work/abi.actual"
test "$(wc -l < "$work/abi.decls")" -eq 13
diff -u "$work/abi.expected" "$work/abi.actual"
printf '#include "linalg_native.c"\n' > "$work/abi.c"
cat "$work/abi.decls" >> "$work/abi.c"
compile -fsyntax-only "$work/abi.c"
compile -c release/extlibs/linalg/test/xrt_runtime.c -o "$work/xrt.o"
compile -c "$native" -o "$work/native.o"
objects=("$work/native.o" "$work/xrt.o")
compile release/extlibs/linalg/test/test_linalg.c "${objects[@]}" -no-pie -lm -ldl -o "$work/test_linalg"
timeout 90s "$work/test_linalg"
numeric=release/extlibs/linalg/test/numeric_edges_c.c
compile -DNDEBUG "$numeric" "$work/xrt.o" -no-pie -lm -ldl -o "$work/numeric"
timeout 90s "$work/numeric"
timeout 90s ./release/xl run "$numeric"
if [[ -n "$python_path" ]]; then
    "$python_path" --version > "$work/python.txt"
    timeout 180s "$cc" -std=c11 -D_GNU_SOURCE -O2 -Wall -Wextra -Werror -fPIC -shared -pthread \
        -Ilib/xrt -Irelease/extlibs/linalg "$native" release/extlibs/linalg/test/xrt_runtime.c \
        -lm -ldl -o "$work/oracle.so"
    timeout 90s "$python_path" release/extlibs/linalg/test/exact_inverse.py "$work/oracle.so"
else printf '[linalg] optional Python exact oracle: SKIP (Python unavailable)\n'; fi
compile release/examples/extlibs/linalg_c.c "${objects[@]}" -no-pie -lm -ldl -o "$work/example_c"
timeout 30s "$work/example_c" > "$work/example_c.txt"
diff -u <(sed 's/\r$//' release/extlibs/linalg/test/linalg.expected.txt) "$work/example_c.txt"
timeout 90s ./release/xl run release/examples/extlibs/linalg_c.c > "$work/direct_c.txt"
timeout 90s ./release/xl run release/examples/extlibs/linalg.xl > "$work/example_xl.txt"
diff -u "$work/example_c.txt" "$work/direct_c.txt"
diff -u "$work/example_c.txt" "$work/example_xl.txt"
calibration=release/extlibs/linalg/examples/calibration_c.c
compile "$calibration" "${objects[@]}" -no-pie -lm -ldl -o "$work/calibration_c"
timeout 30s "$work/calibration_c" > "$work/calibration_c.txt"
diff -u <(sed 's/\r$//' release/extlibs/linalg/examples/calibration.expected.txt) "$work/calibration_c.txt"
timeout 90s ./release/xl run "$calibration" > "$work/calibration_direct.txt"
timeout 90s ./release/xl run release/extlibs/linalg/examples/calibration.xl > "$work/calibration_xl.txt"
diff -u "$work/calibration_c.txt" "$work/calibration_direct.txt"
diff -u "$work/calibration_c.txt" "$work/calibration_xl.txt"
for source in test/cases/extlib_linalg.xl release/extlibs/linalg/test/boundaries.xl \
    release/extlibs/linalg/test/allocations.xl release/extlibs/linalg/test/numeric_edges.xl; do
    timeout 90s ./release/xl run "$source"
done
for source in release/extlibs/linalg/test/aot_core.xl release/extlibs/linalg/test/boundaries.xl \
    release/extlibs/linalg/test/allocations.xl release/extlibs/linalg/test/numeric_edges.xl \
    "$numeric" test/cases/extlib_linalg.xl release/examples/extlibs/linalg.xl \
    release/examples/extlibs/linalg_c.c release/extlibs/linalg/examples/calibration.xl "$calibration"; do
    target="$work/$(basename "${source%.*}")"
    timeout 180s ./release/xl build "$source" -o "$target"
    timeout 90s "$target" > "$work/$(basename "$source").out"
    case "$source" in
        release/examples/extlibs/linalg*) diff -u "$work/example_c.txt" "$work/$(basename "$source").out" ;;
        */examples/calibration*) diff -u "$work/calibration_c.txt" "$work/$(basename "$source").out" ;;
    esac
done
printf 'linalg Linux C ASan/UBSan, thirteen generated prototypes, JIT/AOT and bilingual golden output: PASS\n'
