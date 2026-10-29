#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-trie-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror
       -Ilib/xrt -Irelease/extlibs/trie/include
       -fsanitize=address,undefined -fno-omit-frame-pointer
       -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

"$cc" "${flags[@]}" -fsyntax-only release/extlibs/trie/test/abi_contract.c
"$cc" "${flags[@]}" -c release/extlibs/trie/test/xrt_runtime.c \
    -o "$work/xrt_runtime.o"
"$cc" "${flags[@]}" release/extlibs/trie/trie_native.c \
    release/extlibs/trie/test/test_trie.c "$work/xrt_runtime.o" \
    -lm -ldl -o "$work/test_trie"
timeout 90s "$work/test_trie"
"$cc" "${flags[@]}" release/extlibs/trie/trie_native.c \
    release/examples/extlibs/trie_c.c "$work/xrt_runtime.o" \
    -lm -ldl -o "$work/trie_example"
timeout 30s "$work/trie_example" > "$work/c_example.txt"
diff -u release/extlibs/trie/test/trie.expected.txt "$work/c_example.txt"
timeout 90s ./release/xl run test/cases/extlib_trie.xl
timeout 90s ./release/xl run release/extlibs/trie/test/boundaries.xl
timeout 90s ./release/xl run release/extlibs/trie/test/allocations.xl
if timeout 90s ./release/xl check release/extlibs/trie/test/private_native.xl > "$work/private_native.txt" 2>&1; then
    printf 'trie: native methods must not be public\n' >&2
    exit 1
fi
if ! grep -Fq 'insertNative' "$work/private_native.txt" ||
   ! grep -Fq 'access' "$work/private_native.txt"; then
    cat "$work/private_native.txt" >&2
    printf 'trie: expected private-access diagnostic\n' >&2
    exit 1
fi
timeout 90s ./release/xl run release/examples/extlibs/trie.xl \
    > "$work/xl_example.txt"
diff -u "$work/c_example.txt" "$work/xl_example.txt"
timeout 90s ./release/xl run release/examples/extlibs/trie_c.c \
    > "$work/c_direct.txt"
diff -u "$work/c_example.txt" "$work/c_direct.txt"
timeout 120s ./release/xl build test/cases/extlib_trie.xl \
    -o "$work/trie_core_aot"
timeout 45s "$work/trie_core_aot"
for name in boundaries allocations; do
    timeout 120s ./release/xl build "release/extlibs/trie/test/$name.xl" \
        -o "$work/trie_${name}_aot"
    timeout 45s "$work/trie_${name}_aot"
done
timeout 120s ./release/xl build release/examples/extlibs/trie.xl \
    -o "$work/trie_xl_example_aot"
timeout 45s "$work/trie_xl_example_aot" > "$work/xl_aot.txt"
diff -u "$work/c_example.txt" "$work/xl_aot.txt"
timeout 120s ./release/xl build release/examples/extlibs/trie_c.c \
    -o "$work/trie_c_example_aot"
timeout 45s "$work/trie_c_example_aot" > "$work/c_aot.txt"
diff -u "$work/c_example.txt" "$work/c_aot.txt"
printf 'trie Linux C ASan/UBSan and XLang parity gate: PASS\n'
