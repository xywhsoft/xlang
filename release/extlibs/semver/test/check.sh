#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-semver-XXXXXX")
input_paths() {
    find release/extlibs/semver release/include -type f -print0
    printf '%s\0' lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so release/libxlc.so \
        test/cases/extlib_semver.xl
}
finish_gate() {
    result=$?
    trap - EXIT
    set +e
    cd "$root" || exit 125
    mapfile -d '' after < <(input_paths | sort -zu)
    sha256sum "${after[@]}" > "$work/inputs.after.sha256"
    if ! diff -u "$work/inputs.before.sha256" "$work/inputs.after.sha256"; then result=125; fi
    printf '%s\n' "$result" > "$work/exit-code.txt"
    if ((result == 0)); then rm -rf -- "$work"
    else printf '[semver] FAIL exit=%s artifacts=%s\n' "$result" "$work" >&2; fi
    exit "$result"
}
trap finish_gate EXIT
trap 'printf "exit=%s\nline=%s\ncommand=%s\n" "$?" "$LINENO" "$BASH_COMMAND" > "$work/failure.txt"' ERR
mapfile -d '' inputs < <(input_paths | sort -zu)
sha256sum "${inputs[@]}" > "$work/inputs.before.sha256"
golden_output() {
    diff -u <(sed 's/\r$//' "$1") "$2"
}
cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror
       -Ilib/xrt -Irelease/extlibs/semver -fsanitize=address,undefined
       -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
"$cc" --version > "$work/compiler.txt"

"$cc" "${flags[@]}" -fsyntax-only \
    release/extlibs/semver/test/abi_contract.c
"$cc" "${flags[@]}" -c release/extlibs/semver/test/xrt_runtime.c \
    -o "$work/xrt_runtime.o"
"$cc" "${flags[@]}" release/extlibs/semver/semver_native.c \
    release/extlibs/semver/test/test_semver.c "$work/xrt_runtime.o" \
    -lm -ldl -o "$work/test_semver"
timeout 90s "$work/test_semver"
"$cc" "${flags[@]}" release/extlibs/semver/examples/semver_c.c \
    "$work/xrt_runtime.o" \
    -lm -ldl -o "$work/example"
timeout 30s "$work/example" > "$work/example.txt"
golden_output release/extlibs/semver/examples/basic.expected.txt "$work/example.txt"
"$cc" "${flags[@]}" release/extlibs/semver/examples/ranges_c.c "$work/xrt_runtime.o" -lm -ldl -o "$work/ranges-c"
timeout 30s "$work/ranges-c" > "$work/ranges-c.txt"
golden_output release/extlibs/semver/examples/ranges.expected.txt "$work/ranges-c.txt"

if [[ ! -x ./release/xl ]]; then
    printf 'semver: Linux release/xl is missing\n' >&2
    exit 1
fi
timeout 30s ./release/xl run release/extlibs/semver/examples/semver_c.c \
    > "$work/direct_c.txt"
diff -u "$work/example.txt" "$work/direct_c.txt"
timeout 45s ./release/xl run test/cases/extlib_semver.xl
timeout 45s ./release/xl run release/extlibs/semver/test/boundaries.xl
timeout 45s ./release/xl run release/extlibs/semver/test/allocations.xl
timeout 45s ./release/xl run release/extlibs/semver/test/prerelease_ranges.xl
for source in ranges.xl ranges_c.c; do
    timeout 45s ./release/xl run "release/extlibs/semver/examples/$source" > "$work/$source-jit.txt"
    golden_output release/extlibs/semver/examples/ranges.expected.txt "$work/$source-jit.txt"
done
timeout 45s ./release/xl run release/extlibs/semver/examples/basic.xl > "$work/xl.txt"
golden_output release/extlibs/semver/examples/basic.expected.txt "$work/xl.txt"
diff -u "$work/example.txt" "$work/xl.txt"
timeout 45s ./release/xl cgen release/extlibs/semver/examples/basic.xl --emit "$work/module.c"
grep -E '^extern .*xsemver_[a-z_]+[(]' "$work/module.c" | sort -u > "$work/abi.decls"
grep -oE '"xsemver_[a-z_]+"' release/extlibs/semver/src/semver.xl | tr -d '"' | sort -u > "$work/abi.expected"
grep -oE 'xsemver_[a-z_]+[(]' "$work/abi.decls" | tr -d '(' | sort -u > "$work/abi.actual"
diff -u "$work/abi.expected" "$work/abi.actual"
printf '#include "semver_native.h"\n' > "$work/abi.c"
cat "$work/abi.decls" >> "$work/abi.c"
"$cc" "${flags[@]}" -fsyntax-only "$work/abi.c"
timeout 120s ./release/xl build test/cases/extlib_semver.xl \
    -o "$work/semver_central"
timeout 45s "$work/semver_central"
timeout 120s ./release/xl build release/extlibs/semver/test/boundaries.xl \
    -o "$work/semver_boundaries"
timeout 45s "$work/semver_boundaries"
timeout 120s ./release/xl build release/extlibs/semver/test/allocations.xl \
    -o "$work/semver_allocations"
timeout 45s "$work/semver_allocations"
timeout 120s ./release/xl build release/extlibs/semver/examples/basic.xl \
    -o "$work/semver_example"
timeout 45s "$work/semver_example" > "$work/xl_aot.txt"
golden_output release/extlibs/semver/examples/basic.expected.txt "$work/xl_aot.txt"
diff -u "$work/example.txt" "$work/xl_aot.txt"
timeout 120s ./release/xl build release/extlibs/semver/examples/semver_c.c \
    -o "$work/semver_c_example"
timeout 45s "$work/semver_c_example" > "$work/c_aot.txt"
golden_output release/extlibs/semver/examples/basic.expected.txt "$work/c_aot.txt"
diff -u "$work/example.txt" "$work/c_aot.txt"
timeout 120s ./release/xl build release/extlibs/semver/test/prerelease_ranges.xl -o "$work/prerelease-ranges"
timeout 45s "$work/prerelease-ranges"
for source in ranges.xl ranges_c.c; do
    binary="$work/${source//./-}"
    timeout 120s ./release/xl build "release/extlibs/semver/examples/$source" -o "$binary"
    timeout 45s "$binary" > "$work/$source-aot.txt"
    golden_output release/extlibs/semver/examples/ranges.expected.txt "$work/$source-aot.txt"
done
printf 'semver Linux C ASan/UBSan, XLang JIT/AOT and parity gate: PASS\n'
