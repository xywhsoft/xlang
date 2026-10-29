#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-xxhash-XXXXXX")
cleanup() {
    result=$?
    trap - EXIT
    if ((result == 0)); then rm -rf -- "$work"
    else printf '[xxhash] FAIL exit=%s artifacts=%s\n' "$result" "$work" >&2; fi
    exit "$result"
}
trap cleanup EXIT
cc=${CC:-gcc}
pkg=release/extlibs/xxhash
input_paths() {
    find "$pkg" -type f -print0
    printf '%s\0' .gitattributes lib/xrt/xrt.h lib/xruntime/xruntime.h \
        test/support/extlib_codec_runtime.c tools/check_extlib_xxhash.sh \
        tools/check_extlib_native_globals.ps1 tools/check_extlib_native_globals.sh \
        release/xl release/libxlc.so test/cases/extlib_xxhash.xl \
        test/cases/extlib_xxhash_boundaries.xl \
        test/cases/extlib_xxhash_progress.xl \
        release/examples/extlibs/xxhash.xl \
        release/examples/extlibs/xxhash_basic.xl \
        release/examples/extlibs/xxhash_basic.expected.txt
}
mapfile -d '' inputs < <(input_paths | sort -z)
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt -Ilib/xruntime
       -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
sha256sum "${inputs[@]}" > "$work/inputs.before.sha256"
"$cc" --version > "$work/compiler.txt"
"$cc" "${flags[@]}" -DXRT_MODULE_THREAD -c test/support/extlib_codec_runtime.c -o "$work/runtime.o"
objects=()
for source in xrt_xxhash.c xxhash_native.c third_party/xxhash.c; do
    object="$work/native-${#objects[@]}.o"
    "$cc" "${flags[@]}" -c "$pkg/$source" -o "$object"
    objects+=("$object")
done
for name in test_xxhash test_xxhash_native; do
    selected=("${objects[@]}")
    if [[ "$name" == test_xxhash ]]; then selected=("${objects[2]}"); fi
    "$cc" "${flags[@]}" -DNDEBUG "$pkg/test/$name.c" "${selected[@]}" "$work/runtime.o" -lm -ldl -o "$work/$name"
    timeout 60s "$work/$name" 2>&1 | tee "$work/$name.log"
done
"$cc" "${flags[@]}" -DNDEBUG "$pkg/examples/basic.c" "${objects[@]}" "$work/runtime.o" -lm -ldl -o "$work/basic"
timeout 30s "$work/basic" > "$work/basic.log"
sed 's/\r$//' release/examples/extlibs/xxhash_basic.expected.txt > "$work/basic.expected.txt"
diff -u "$work/basic.expected.txt" "$work/basic.log"
if [[ ! -x release/xl ]]; then
    printf 'xxhash Linux xl runner is missing: release/xl\n' >&2
    exit 1
fi
# 保留相同发布输入，在原生临时目录执行语言阶段，避开 WSL 挂载盘 I/O 超时。
stage="$work/stage"
mkdir -p "$stage/release/extlibs" "$stage/release/examples/extlibs" "$stage/test/cases"
cp -a release/xl release/libxlc.so "$stage/release/"
cp -a "$pkg" "$stage/release/extlibs/"
cp -a test/cases/extlib_xxhash.xl test/cases/extlib_xxhash_boundaries.xl \
    test/cases/extlib_xxhash_progress.xl "$stage/test/cases/"
cp -a release/examples/extlibs/xxhash.xl release/examples/extlibs/xxhash_basic.xl \
    release/examples/extlibs/xxhash_basic.expected.txt "$stage/release/examples/extlibs/"
cd "$stage"
timeout 90s ./release/xl run "$pkg/examples/basic.c" > "$work/basic_direct.log"
diff -u "$work/basic.log" "$work/basic_direct.log"
for name in private_update_negative private_string_length_negative; do
    if timeout 30s ./release/xl check "$pkg/test/$name.xl" > "$work/$name.log" 2>&1; then
        printf 'xxhash private member escaped its class: %s\n' "$name" >&2
        exit 1
    fi
    grep -Fq 'access' "$work/$name.log"
done
for name in invalid_seed_negative invalid_seed32_overflow; do
    if timeout 30s ./release/xl check "$pkg/test/$name.xl" > "$work/$name.log" 2>&1; then
        printf 'xxhash invalid seed accepted: %s\n' "$name" >&2
        exit 1
    fi
    grep -Fq '[sem]' "$work/$name.log"
done
for name in extlib_xxhash extlib_xxhash_boundaries extlib_xxhash_progress; do
    timeout 90s ./release/xl run "test/cases/$name.xl"
done
for name in seeded_boundaries stream_limit xxh3 ownership; do
    timeout 90s ./release/xl run "$pkg/test/$name.xl"
done
timeout 90s ./release/xl cgen "$pkg/test/ownership.xl" --emit "$work/ownership.c"
for symbol in xxxhash_hash64_hex xxxhash_hash64_str_hex xxxhash_hash32_hex \
    xxxhash_hash32_str_hex xxxhash_hash3_64_hex xxxhash_hash3_64_str_hex \
    xxxhash_hash3_128_hex xxxhash_hash3_128_str_hex xxxhash_file64_hex \
    xxxhash_hasher_digest_hex; do
    if ! grep -Eq "v[0-9]+\\.Data[[:space:]]*=[[:space:]]*$symbol[[:space:]]*\\(" "$work/ownership.c"; then
        printf 'xxhash: %s must transfer its native string without a copy\n' "$symbol" >&2
        exit 1
    fi
done
timeout 90s ./release/xl run release/examples/extlibs/xxhash.xl > "$work/full_jit.log"
timeout 90s ./release/xl run release/examples/extlibs/xxhash_basic.xl > "$work/basic_jit.log"
diff -u "$work/basic.log" "$work/basic_jit.log"
printf '[xxhash] XLang AOT gate\n'
for name in extlib_xxhash extlib_xxhash_boundaries extlib_xxhash_progress seeded_boundaries stream_limit xxh3 ownership xxhash xxhash_basic; do
    printf '[xxhash] AOT %s\n' "$name"
    case "$name" in
        extlib_*) source="test/cases/$name.xl" ;;
        seeded_boundaries|stream_limit|xxh3|ownership) source="$pkg/test/$name.xl" ;;
        *) source="release/examples/extlibs/$name.xl" ;;
    esac
    timeout 180s ./release/xl build "$source" -o "$work/$name"
    timeout 90s "$work/$name" > "$work/${name}_aot.log"
done
diff -u "$work/basic.log" "$work/xxhash_basic_aot.log"
diff -u "$work/full_jit.log" "$work/xxhash_aot.log"
cd "$root"
bash "$root/tools/check_extlib_native_globals.sh" xxhash
mapfile -d '' after < <(input_paths | sort -z)
sha256sum "${after[@]}" > "$work/inputs.after.sha256"
diff -u "$work/inputs.before.sha256" "$work/inputs.after.sha256"
printf 'xxhash Linux C ASan/UBSan, XLang JIT/AOT and parity gate: PASS\n'
