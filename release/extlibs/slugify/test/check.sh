#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-slugify-XXXXXX")
cleanup() {
    result=$?
    trap - EXIT
    set +e
    if ((result == 0)); then
        rm -rf -- "$work" || result=$?
    else
        printf '[slugify] FAIL exit=%s artifacts=%s\n' "$result" "$work" >&2
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
cd "$root"

cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt
       -Irelease/extlibs/slugify -fsanitize=address,undefined
       -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
mapfile -d '' inputs < <(find release/extlibs/slugify release/include -type f -print0)
inputs+=(lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so release/libxlc.so
          test/cases/extlib_slugify.xl release/examples/extlibs/slugify.xl)
sha256sum "${inputs[@]}" > "$work/inputs.before.sha256"
"$cc" --version > "$work/compiler.txt"

"$cc" -std=c11 -Wall -Wextra -Werror -fsyntax-only -Ilib/xrt \
    -Irelease/extlibs/slugify -DXRT_MODULE_BUFFER -DXRT_MODULE_UNICODE \
    release/extlibs/slugify/slugify_native.c

"$cc" "${flags[@]}" -c release/extlibs/slugify/test/xrt_runtime.c \
    -o "$work/xrt_runtime.o"
"$cc" "${flags[@]}" release/extlibs/slugify/slugify_native.c \
    release/extlibs/slugify/test/test_slugify.c "$work/xrt_runtime.o" \
    -lm -ldl -o "$work/test_slugify"
timeout 90s "$work/test_slugify"
"$cc" "${flags[@]}" release/extlibs/slugify/slugify_native.c \
    release/extlibs/slugify/test/test_threads.c "$work/xrt_runtime.o" \
    -lm -ldl -o "$work/test_threads"
timeout 90s "$work/test_threads"
"$cc" "${flags[@]}" release/extlibs/slugify/slugify_native.c \
    release/extlibs/slugify/examples/slugify_c.c "$work/xrt_runtime.o" \
    -lm -ldl -o "$work/example_c"
timeout 30s "$work/example_c" > "$work/example_c.txt"
timeout 30s release/xl run release/extlibs/slugify/examples/slugify_c.c > "$work/example_c_direct.txt"
diff -u "$work/example_c.txt" "$work/example_c_direct.txt"

if [[ ! -x release/xl ]]; then
    printf 'missing Linux release/xl for slugify XLang gate\n' >&2
    exit 1
fi
timeout 90s release/xl run test/cases/extlib_slugify.xl
timeout 90s release/xl run release/extlibs/slugify/test/boundaries.xl
timeout 90s release/xl run release/extlibs/slugify/test/owned_results.xl
timeout 90s release/xl cgen release/extlibs/slugify/test/owned_probe.xl --emit "$work/owned_probe.c"
count=$(grep -Ec '[[:alnum:]_]+\.Data[[:space:]]*=[[:space:]]*xsl_slugify[[:space:]]*\(' \
    "$work/owned_probe.c")
if [[ $count != 1 ]] || grep -Eq '= *xlir_string_copy *\(' "$work/owned_probe.c"; then
    printf 'slugify: adopted text was copied again after the native call\n' >&2
    exit 1
fi
timeout 30s release/xl run release/examples/extlibs/slugify.xl > "$work/example_xl.txt"
diff -u "$work/example_c.txt" "$work/example_xl.txt"
timeout 120s release/xl build test/cases/extlib_slugify.xl \
    -o "$work/slugify_core_aot"
timeout 45s "$work/slugify_core_aot"
timeout 120s release/xl build release/extlibs/slugify/test/owned_results.xl \
    -o "$work/slugify_owned_aot"
timeout 45s "$work/slugify_owned_aot"
timeout 120s release/xl build release/extlibs/slugify/test/boundaries.xl \
    -o "$work/slugify_boundaries_aot"
timeout 45s "$work/slugify_boundaries_aot" > "$work/boundaries_aot.txt"
printf 'slugify boundaries: PASS\n' > "$work/boundaries_expected.txt"
diff -u "$work/boundaries_expected.txt" "$work/boundaries_aot.txt"
timeout 120s release/xl build release/examples/extlibs/slugify.xl \
    -o "$work/slugify_example_aot"
timeout 45s "$work/slugify_example_aot" > "$work/example_xl_aot.txt"
diff -u "$work/example_c.txt" "$work/example_xl_aot.txt"
timeout 120s release/xl build release/extlibs/slugify/examples/slugify_c.c \
    -o "$work/slugify_c_aot"
timeout 45s "$work/slugify_c_aot" > "$work/example_c_aot.txt"
diff -u "$work/example_c.txt" "$work/example_c_aot.txt"
sha256sum -c "$work/inputs.before.sha256" > "$work/inputs.after.txt"
printf 'slugify Linux C ASan/UBSan, threads and XLang JIT/AOT gate: PASS\n'
