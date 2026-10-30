#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-imagehash-XXXXXX")
cleanup() {
    result=$?
    trap - EXIT
    set +e
    if ((result == 0)); then
        rm -rf -- "$work" || result=$?
    else
        printf '[imagehash] FAIL exit=%s artifacts=%s\n' "$result" "$work" >&2
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
cc=${CC:-gcc}
python=${PYTHON:-python3}
compile() { timeout 180s "$cc" "$@"; }
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror
       -Ilib/xrt -Irelease/extlibs/imagehash
       -Irelease/extlibs/imagehash/include
       -ffunction-sections -fdata-sections
       -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
mapfile -d '' inputs < <(find release/extlibs/imagehash release/extlibs/stb_image \
    release/extlibs/stb_image_resize2 release/include -type f -print0)
inputs+=(lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so release/libxlc.so
          test/cases/extlib_imagehash.xl release/examples/extlibs/imagehash.xl
          release/examples/extlibs/assets/tiny.png release/examples/games/assets/player.png)
sha256sum "${inputs[@]}" > "$work/inputs.before.sha256"
timeout 30s "$cc" --version > "$work/compiler.txt"
compiler=$(command -v "$cc")
cc1=$(timeout 30s "$cc" -print-prog-name=cc1)
python_path=$(command -v "$python")
sha256sum "$compiler" "$cc1" "$python_path" >> "$work/inputs.before.sha256"
timeout 30s "$python" --version > "$work/python.txt"
golden="$work/imagehash.expected.txt"
sed 's/\r$//' release/extlibs/imagehash/examples/imagehash.expected.txt > "$golden"

compile "${flags[@]}" -c release/extlibs/imagehash/test/runtime.c -o "$work/runtime.o"
compile "${flags[@]}" -c release/extlibs/imagehash/imagehash_native.c \
    -o "$work/imagehash.o"
compile "${flags[@]}" -c release/extlibs/stb_image/stb_image_native.c \
    -o "$work/stb_image.o"
compile "${flags[@]}" -c release/extlibs/stb_image_resize2/stb_image_resize2_native.c \
    -o "$work/stb_image_resize2.o"
for name in test_error_threads test_imagehash; do
    compile "${flags[@]}" -c \
        "release/extlibs/imagehash/test/$name.c" -o "$work/$name.o"
    compile -fsanitize=address,undefined -no-pie -Wl,--gc-sections \
        "$work/$name.o" "$work/imagehash.o" "$work/stb_image.o" \
        "$work/stb_image_resize2.o" "$work/runtime.o" -pthread -lm -ldl -o "$work/$name"
    timeout 90s "$work/$name"
done

compile "${flags[@]}" -c release/extlibs/imagehash/test/test_cold_state.c -o "$work/cold.o"
compile -fsanitize=address,undefined -no-pie -Wl,--gc-sections "$work/cold.o" \
    "$work/stb_image.o" "$work/stb_image_resize2.o" "$work/runtime.o" \
    -pthread -lm -ldl -o "$work/cold"
for kind in {0..5}; do
    complete=false
    for budget in {0..127}; do
        output=$(timeout 30s "$work/cold" "$kind" "$budget")
        printf '%s\n' "$output"
        if [[ "$output" == *'injected=0 PASS' ]]; then complete=true; break; fi
    done
    if [[ "$complete" != true ]]; then printf 'cold state budget incomplete\n' >&2; exit 1; fi
done

# 固定预算只比较普通构建；原有全部故障点和图像边界仍由上面的 sanitizer 验证。
budget_flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt -pthread)
compile "${budget_flags[@]}" release/extlibs/imagehash/test/runtime.c \
    release/extlibs/imagehash/test/test_owned_text.c release/extlibs/imagehash/imagehash_native.c \
    release/extlibs/stb_image/stb_image_native.c \
    release/extlibs/stb_image_resize2/stb_image_resize2_native.c -lm -ldl -o "$work/budget_c"
timeout 90s "$work/budget_c"

xl=./release/xl
if [[ ! -x "$xl" ]]; then
    printf 'missing Linux xlang runner: %s\n' "$xl" >&2
    exit 1
fi
timeout 90s "$xl" run release/extlibs/imagehash/test/test_owned_text.c
for kind in {0..5}; do
    timeout 90s "$xl" run release/extlibs/imagehash/test/test_cold_state.c -- "$kind" 0
done
timeout 90s "$xl" build release/extlibs/imagehash/test/cold_start.xl -o "$work/cold_xlang"
for engine in JIT AOT; do
    for kind in {0..2}; do
        complete=false
        for budget in {0..63}; do
            if [[ "$engine" == JIT ]]; then
                output=$(timeout 90s "$xl" run release/extlibs/imagehash/test/cold_start.xl -- "$kind" "$budget")
            else output=$(timeout 30s "$work/cold_xlang" "$kind" "$budget"); fi
            printf '%s\n' "$output"
            if [[ "$output" =~ injected=[[:space:]]*false[[:space:]]+PASS$ ]]; then complete=true; break; fi
        done
        if [[ "$complete" != true ]]; then printf 'XLang cold budget incomplete\n' >&2; exit 1; fi
    done
done

compile "${flags[@]}" -c release/extlibs/imagehash/test/hash_vectors.c -o "$work/vectors.o"
compile -fsanitize=address,undefined -no-pie -Wl,--gc-sections "$work/vectors.o" \
    "$work/imagehash.o" "$work/stb_image.o" "$work/stb_image_resize2.o" \
    "$work/runtime.o" -pthread -lm -ldl -o "$work/vectors"
timeout 90s "$xl" build release/extlibs/imagehash/test/hash_vectors.c -o "$work/vectors_aot"
for engine in GCC JIT AOT; do
    if [[ "$engine" == GCC ]]; then timeout 90s "$work/vectors" "$work/$engine.pgm" > "$work/$engine.jsonl"
    elif [[ "$engine" == JIT ]]; then timeout 90s "$xl" run release/extlibs/imagehash/test/hash_vectors.c -- "$work/$engine.pgm" > "$work/$engine.jsonl"
    else timeout 90s "$work/vectors_aot" "$work/$engine.pgm" > "$work/$engine.jsonl"; fi
    timeout 90s "$python" -I release/extlibs/imagehash/test/check_hash_vectors.py "$work/$engine.jsonl"
done
timeout 90s "$xl" run test/cases/extlib_imagehash.xl
timeout 90s "$xl" run release/extlibs/imagehash/test/allocations.xl
timeout 90s "$xl" run release/extlibs/imagehash/test/owned_results.xl -- 14
timeout 90s "$xl" cgen release/extlibs/imagehash/test/owned_probe.xl --emit "$work/owned_probe.c"
grep -E '^extern [^;]*\bxihash_[a-z_]+\(' "$work/owned_probe.c" > "$work/prototypes.txt"
[[ $(wc -l < "$work/prototypes.txt") == 8 ]]
printf '#include "imagehash_native.h"\n' > "$work/abi.c"
cat "$work/prototypes.txt" >> "$work/abi.c"
compile "${flags[@]}" -fsyntax-only "$work/abi.c"
sed 's/char[[:space:]]*\*[[:space:]]*xihash_ahash(/bool xihash_ahash(/' "$work/abi.c" > "$work/abi_wrong.c"
if compile "${flags[@]}" -fsyntax-only "$work/abi_wrong.c" > "$work/abi_wrong.log" 2>&1; then
    printf 'wrong owned return was accepted\n' >&2; exit 1
else [[ $? == 1 ]]; fi
for symbol in ahash dhash phash; do
    count=$(grep -Ec "[[:alnum:]_]+\.Data[[:space:]]*=[[:space:]]*xihash_${symbol}[[:space:]]*\(" \
        "$work/owned_probe.c")
    if [[ $count != 1 ]]; then
        printf 'imagehash probe must adopt %s exactly once\n' "$symbol" >&2
        exit 1
    fi
done
if grep -Eq '= *xlir_string_copy *\(' "$work/owned_probe.c"; then
    printf 'imagehash copied the adopted result in the complete public chain\n' >&2
    exit 1
fi

compile "${flags[@]}" -c \
    release/extlibs/imagehash/examples/imagehash.c -o "$work/example.o"
compile -fsanitize=address,undefined -no-pie -Wl,--gc-sections \
    "$work/example.o" "$work/imagehash.o" "$work/stb_image.o" \
    "$work/stb_image_resize2.o" "$work/runtime.o" -pthread -lm -ldl -o "$work/example"
timeout 90s "$work/example" > "$work/c.out"
timeout 90s "$xl" run release/examples/extlibs/imagehash.xl > "$work/xlang.out"
diff -u "$work/c.out" "$work/xlang.out"
diff -u "$golden" "$work/c.out"
timeout 90s "$xl" run release/extlibs/imagehash/examples/imagehash.c > "$work/direct.out"
diff -u "$work/c.out" "$work/direct.out"
timeout 90s "$xl" build release/extlibs/imagehash/test/aot_core.xl -o "$work/imagehash_core"
timeout 30s "$work/imagehash_core"
timeout 90s "$xl" build release/extlibs/imagehash/test/allocations.xl -o "$work/imagehash_allocations"
timeout 30s "$work/imagehash_allocations"
timeout 90s "$xl" build test/cases/extlib_imagehash.xl -o "$work/imagehash_central"
timeout 30s "$work/imagehash_central"
timeout 90s "$xl" build release/extlibs/imagehash/test/owned_results.xl -o "$work/imagehash_owned"
timeout 30s "$work/imagehash_owned" 14
timeout 90s "$xl" build release/extlibs/imagehash/test/test_owned_text.c -o "$work/imagehash_owned_c"
timeout 30s "$work/imagehash_owned_c"
timeout 90s "$xl" build release/examples/extlibs/imagehash.xl -o "$work/imagehash_xlang"
timeout 30s "$work/imagehash_xlang" > "$work/xlang_aot.out"
diff -u "$golden" "$work/xlang_aot.out"
timeout 90s "$xl" build release/extlibs/imagehash/examples/imagehash.c -o "$work/imagehash_c"
timeout 30s "$work/imagehash_c" > "$work/c_aot.out"
diff -u "$golden" "$work/c_aot.out"

compile "${flags[@]}" -c release/extlibs/imagehash/examples/similarity_c.c -o "$work/similarity.o"
compile -fsanitize=address,undefined -no-pie -Wl,--gc-sections "$work/similarity.o" \
    "$work/imagehash.o" "$work/stb_image.o" "$work/stb_image_resize2.o" \
    "$work/runtime.o" -pthread -lm -ldl -o "$work/similarity_gcc"
for language in c xlang; do
    if [[ "$language" == c ]]; then source=release/extlibs/imagehash/examples/similarity_c.c
    else source=release/extlibs/imagehash/examples/similarity.xl; fi
    timeout 90s "$xl" build "$source" -o "$work/similarity_$language"
    for engine in GCC JIT AOT; do
        if [[ "$engine" == GCC && "$language" != c ]]; then continue; fi
        for self in false true; do
            args=()
            if [[ "$self" == true ]]; then args=(release/examples/extlibs/assets/tiny.png release/examples/extlibs/assets/tiny.png); fi
            if [[ "$engine" == GCC ]]; then timeout 30s "$work/similarity_gcc" "${args[@]}" > "$work/similarity.out"
            elif [[ "$engine" == JIT ]]; then timeout 90s "$xl" run "$source" -- "${args[@]}" > "$work/similarity.out"
            else timeout 30s "$work/similarity_$language" "${args[@]}" > "$work/similarity.out"; fi
            if [[ "$self" == true ]]; then printf 'pHash distance: 0\nwithin threshold 4: true\n' > "$work/similarity.expected"
            else cp release/extlibs/imagehash/examples/similarity.expected.txt "$work/similarity.expected"; fi
            diff -u "$work/similarity.expected" "$work/similarity.out"
        done
        if [[ "$engine" == GCC ]]; then command=("$work/similarity_gcc")
        elif [[ "$engine" == JIT ]]; then command=("$xl" run "$source" --)
        else command=("$work/similarity_$language"); fi
        if timeout 90s "${command[@]}" "$work/missing-image.png" release/examples/extlibs/assets/tiny.png \
            > "$work/missing.out" 2> "$work/missing.err"; then
            printf 'missing first image was accepted\n' >&2; exit 1
        else [[ $? == 1 ]]; fi
        cat "$work/missing.out" "$work/missing.err" | grep -q 'cannot read or decode image'
    done
done
sha256sum -c "$work/inputs.before.sha256" > "$work/inputs.after.txt"
printf 'imagehash Linux C ASan/UBSan, XLang JIT and bilingual AOT gate: PASS\n'
