#!/usr/bin/env bash
set -euo pipefail

# 独立 C/内存检查，不重建编译器，也不替换运行库快照。
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"
work_root="$root/build/test/argon2_bloom_linux_guard"
mkdir -p "$work_root"
work=$(mktemp -d "$work_root/run_XXXXXX")
cleanup() {
    status=$?
    if ((status == 0)); then
        case "$work" in
            "$work_root"/run_*) rm -rf -- "$work" ;;
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
        test/cases/extlib_argon2_bloom_alloc.c tools/check_extlib_argon2_bloom.sh)
for name in argon2 bloom; do
    inputs+=("release/examples/extlibs/${name}_c.c" "release/examples/extlibs/${name}_basic.expected.txt")
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
    diff -u "release/examples/extlibs/${name}_basic.expected.txt" "$work/$name-example.log"
done
"$cc" "${flags[@]}" test/cases/extlib_argon2_bloom_alloc.c "${objects[@]}" \
    "$work/runtime.o" -lm -ldl -o "$work/allocation"
timeout 30s "$work/allocation" 2>&1 | tee "$work/allocation.log"
sha256sum "${inputs[@]}" > "$work/inputs.after.sha256"
diff -u "$work/inputs.before.sha256" "$work/inputs.after.sha256"
printf 'Argon2/Bloom Linux C ASan/UBSan gate: PASS\n'
