#!/usr/bin/env bash
set -euo pipefail

# 独立 C/内存检查，不重建编译器，也不替换运行库快照。
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"
work_root="${TMPDIR:-/tmp}"
mkdir -p "$work_root"
work=$(mktemp -d "$work_root/xlang-argon2-bloom-XXXXXX")
cleanup() {
    status=$?
    if ((status == 0)); then
        case "$work" in
            "$work_root"/xlang-argon2-bloom-*) rm -rf -- "$work" ;;
            *) printf 'unsafe Argon2/Bloom work path: %s\n' "$work" >&2; return 1 ;;
        esac
    else
        printf 'Argon2/Bloom Linux failure artifacts: %s\n' "$work" >&2
    fi
}
trap cleanup EXIT
cc=${CC:-gcc}
argon=release/extlibs/argon2
bloom=release/extlibs/bloom
sources=("$argon/argon2_native.c" "$argon/vendor/argon2.c" "$argon/vendor/core.c"
         "$argon/vendor/encoding.c" "$argon/vendor/ref.c" "$argon/vendor/blake2/blake2b.c"
         "$bloom/bloom_native.c")
inputs=("${sources[@]}" "$argon"/include/*.h "$argon"/vendor/*.h "$argon"/vendor/blake2/*.h
        "$bloom"/include/*.h "$argon/test/test_argon2.c" "$bloom/test/test_bloom.c"
        "$argon/xlang.package.json" "$bloom/xlang.package.json"
        lib/xrt/xrt.h release/include/xrt_decl.h test/support/extlib_scalar_runtime.c
        test/cases/extlib_argon2_bloom_alloc.c tools/check_extlib_argon2_bloom.sh
        release/xl release/libxlc.so "$argon/src/argon2.xl" "$bloom/src/bloom.xl"
        "$argon/test/ownership.xl"
        "$bloom/test/nul_keys.xl")
for name in argon2 bloom; do
    inputs+=("test/cases/extlib_$name.xl" "release/examples/extlibs/$name.xl"
             "release/examples/extlibs/${name}_basic.xl"
             "release/examples/extlibs/${name}_c.c"
             "release/examples/extlibs/${name}_basic.expected.txt")
done
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt -Irelease/include
       -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
sha256sum "${inputs[@]}" > "$work/inputs.before.sha256"
"$cc" --version > "$work/compiler.txt"
"$cc" "${flags[@]}" -c test/support/extlib_scalar_runtime.c -o "$work/runtime.o"
objects=()
for index in "${!sources[@]}"; do
    vendor_flags=()
    if [[ ${sources[$index]} == */vendor/* ]]; then vendor_flags=(-Wno-type-limits); fi
    object="$work/native-$index.o"
    "$cc" "${flags[@]}" "${vendor_flags[@]}" -c "${sources[$index]}" -o "$object"
    objects+=("$object")
done

for name in argon2 bloom; do
    test_flags=()
    if [[ $name == argon2 ]]; then test_flags=(-DNDEBUG); fi
    "$cc" "${flags[@]}" "${test_flags[@]}" "release/extlibs/$name/test/test_$name.c" "${objects[@]}" \
        "$work/runtime.o" -lm -ldl -o "$work/$name"
    timeout 30s "$work/$name" 2>&1 | tee "$work/$name.log"
    "$cc" "${flags[@]}" "release/examples/extlibs/${name}_c.c" "$work/runtime.o" \
        -lm -ldl -o "$work/$name-example"
    timeout 30s "$work/$name-example" > "$work/$name-example.log"
    sed 's/\r$//' "release/examples/extlibs/${name}_basic.expected.txt" > "$work/$name.expected.txt"
    diff -u "$work/$name.expected.txt" "$work/$name-example.log"
done
"$cc" "${flags[@]}" test/cases/extlib_argon2_bloom_alloc.c "${objects[@]}" \
    "$work/runtime.o" -lm -ldl -o "$work/allocation"
timeout 30s "$work/allocation" 2>&1 | tee "$work/allocation.log"
# 只暂存语言阶段所需发布输入，避免 WSL 挂载盘的编译 I/O 超时。
stage="$work/stage"
mkdir -p "$stage/release/extlibs" "$stage/release/examples/extlibs" \
    "$stage/release/include" "$stage/test/cases"
cp -a release/xl release/libxlc.so "$stage/release/"
cp -a "$argon" "$bloom" "$stage/release/extlibs/"
cp -a release/include/xrt_decl.h "$stage/release/include/"
for name in argon2 bloom; do
    cp -a "test/cases/extlib_$name.xl" "$stage/test/cases/"
    cp -a "release/examples/extlibs/$name.xl" "release/examples/extlibs/${name}_basic.xl" \
        "release/examples/extlibs/${name}_c.c" "$stage/release/examples/extlibs/"
done
cd "$stage"
for name in argon2 bloom; do
    c_example="release/examples/extlibs/${name}_c.c"
    golden="$work/$name.expected.txt"
    timeout 90s ./release/xl run "$c_example" > "$work/$name-c-jit.log"
    diff -u "$golden" "$work/$name-c-jit.log"
    timeout 120s ./release/xl run "test/cases/extlib_$name.xl" > "$work/$name-central-jit.log"
    timeout 120s ./release/xl run "release/examples/extlibs/${name}_basic.xl" > "$work/$name-basic-jit.log"
    diff -u "$golden" "$work/$name-basic-jit.log"
    timeout 180s ./release/xl run "release/examples/extlibs/$name.xl" > "$work/$name-tutorial-jit.log"

    printf '[argon2/bloom] AOT %s\n' "$name"
    for kind in central basic tutorial c; do
        case "$kind" in
            central) source="test/cases/extlib_$name.xl" ;;
            basic) source="release/examples/extlibs/${name}_basic.xl" ;;
            tutorial) source="release/examples/extlibs/$name.xl" ;;
            c) source="$c_example" ;;
        esac
        timeout 180s ./release/xl build "$source" -o "$work/$name-$kind"
        timeout 120s "$work/$name-$kind" > "$work/$name-$kind-aot.log"
    done
    diff -u "$golden" "$work/$name-basic-aot.log"
    diff -u "$golden" "$work/$name-c-aot.log"
done
timeout 90s ./release/xl cgen "$argon/test/ownership.xl" --emit "$work/argon2-owned.c"
for symbol in xargon2_hash xargon2_hash_raw xargon2_extract xargon2_generate_salt; do
    if ! grep -Eq "v[0-9]+\\.Data[[:space:]]*=[[:space:]]*$symbol[[:space:]]*\\(" "$work/argon2-owned.c"; then
        printf 'argon2: %s must adopt its native string without a copy\n' "$symbol" >&2
        exit 1
    fi
done
timeout 120s ./release/xl run "$argon/test/ownership.xl"
timeout 180s ./release/xl build "$argon/test/ownership.xl" -o "$work/argon2-owned"
timeout 120s "$work/argon2-owned"
timeout 120s ./release/xl run "$bloom/test/nul_keys.xl"
timeout 180s ./release/xl build "$bloom/test/nul_keys.xl" -o "$work/bloom-nul-keys"
timeout 120s "$work/bloom-nul-keys"
cd "$root"
sha256sum "${inputs[@]}" > "$work/inputs.after.sha256"
diff -u "$work/inputs.before.sha256" "$work/inputs.after.sha256"
printf 'Argon2/Bloom Linux C ASan/UBSan, XLang JIT/AOT and parity gate: PASS\n'
