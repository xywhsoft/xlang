#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"
work_root=$(realpath -m -- "${TMPDIR:-/tmp}")
mkdir -p "$work_root"
work=$(mktemp -d "$work_root/xlang-argon2-bloom-XXXXXXXX")
stage="$work/stage"
argon=release/extlibs/argon2
bloom=release/extlibs/bloom
cc=${CC:-gcc}
compiler=$(command -v "$cc" || true)
if [[ -f $compiler ]]; then cc=$(realpath -- "$compiler"); compiler=$cc; fi
phase=setup
source_paths() {
    { find "$argon" "$bloom" release/include -type f -print0
      printf '%s\0' lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so release/libxlc.so \
        test/support/extlib_scalar_runtime.c test/cases/extlib_argon2_bloom_alloc.c tools/check_extlib_argon2_bloom.sh
      for name in argon2 bloom; do
          printf '%s\0' "test/cases/extlib_$name.xl" "release/examples/extlibs/$name.xl" \
            "release/examples/extlibs/${name}_basic.xl" "release/examples/extlibs/${name}_c.c" \
            "release/examples/extlibs/${name}_basic.expected.txt"
      done
    } | sort -zu
}
mapfile -d '' sources_to_copy < <(source_paths)
inputs=("${sources_to_copy[@]}")
if [[ -f $compiler ]]; then inputs+=("$compiler"); fi
printf '%s\0' "${sources_to_copy[@]}" > "$work/input-paths.before"
sha256sum "${inputs[@]}" > "$work/inputs.before.sha256"
cleanup() {
    result=$?
    trap - EXIT
    set +e
    cd "$root" || exit 1
    sha256sum "${inputs[@]}" > "$work/inputs.after.sha256"
    diff -u "$work/inputs.before.sha256" "$work/inputs.after.sha256" > "$work/source-changes.txt"
    changed=$?
    source_paths > "$work/input-paths.after"
    cmp -s "$work/input-paths.before" "$work/input-paths.after" || changed=1
    if [[ -f $work/stage.before.sha256 ]]; then
        (cd "$stage" && sha256sum "${inputs[@]}") > "$work/stage.after.sha256"
        diff -u "$work/stage.before.sha256" "$work/stage.after.sha256" > "$work/stage-changes.txt" || changed=1
        (cd "$stage" && source_paths) > "$work/stage-paths.after"
        cmp -s "$work/input-paths.before" "$work/stage-paths.after" || changed=1
    fi
    if ((changed != 0 && result == 0)); then result=1; fi
    printf 'exit=%s\nphase=%s\nchanged-inputs=%s\nartifacts=%s\n' "$result" "$phase" "$changed" "$work" > "$work.report.txt"
    if [[ -f $work/phases.tsv ]]; then cat "$work/phases.tsv" >> "$work.report.txt"; fi
    cat "$work/inputs.before.sha256" >> "$work.report.txt"
    if ((result == 0)); then
        case "$work" in "$work_root"/xlang-argon2-bloom-*) rm -rf -- "$work" || result=$? ;; esac
    else printf '[argon2/bloom] FAIL phase=%s exit=%s artifacts=%s\n' "$phase" "$result" "$work" >&2; fi
    exit "$result"
}
trap cleanup EXIT
record_failure() {
    printf 'exit=%s\nline=%s\ncommand=%s\npipeline=%s\nphase=%s\n' "$@" "$phase" > "$work/failure.txt"
    cat "$work/failure.txt" >&2
}
trap 'record_failure "$?" "$LINENO" "$BASH_COMMAND" "${PIPESTATUS[*]}"' ERR
run() {
    phase=$1
    local seconds=$2 result=0
    shift 2
    printf '[argon2/bloom] %s\n' "$phase"
    printf '%q ' "$@" > "$work/$phase.command"
    printf '\n' >> "$work/$phase.command"
    timeout "${seconds}s" "$@" > "$work/$phase.stdout" 2> "$work/$phase.stderr" || result=$?
    printf '%s\n' "$result" > "$work/$phase.exit"
    printf '%s\t%s\n' "$phase" "$result" >> "$work/phases.tsv"
    cat "$work/$phase.stdout"
    cat "$work/$phase.stderr" >&2
    return "$result"
}
golden() {
    sed 's/\r$//' "$1" > "$work/expected.txt"
    diff -u "$work/expected.txt" "$2"
}
run compiler-info 60 "$cc" --version
# 所有编译和运行统一使用一份逐文件校验的独占副本，避免WSL挂载盘上的编译IO。
mkdir -p "$stage"
cp --parents "${sources_to_copy[@]}" "$stage"
cd "$stage"
chmod +x release/xl
sha256sum "${inputs[@]}" > "$work/stage.before.sha256"
diff -u "$work/inputs.before.sha256" "$work/stage.before.sha256" > "$work/copy-changes.txt"
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt -Irelease/include
       -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
run runtime-compile 90 "$cc" "${flags[@]}" -c test/support/extlib_scalar_runtime.c -o "$work/runtime.o"
sources=("$argon/argon2_native.c" "$argon/vendor/argon2.c" "$argon/vendor/core.c"
         "$argon/vendor/encoding.c" "$argon/vendor/ref.c" "$argon/vendor/blake2/blake2b.c" "$bloom/bloom_native.c")
objects=()
for index in "${!sources[@]}"; do
    source="${sources[$index]}"
    strict=(-Wmissing-prototypes)
    if [[ $source == */vendor/* ]]; then strict=(-Wno-type-limits); fi
    object="$work/native-$index.o"
    run "native-$index" 90 "$cc" "${flags[@]}" "${strict[@]}" -c "$source" -o "$object"
    objects+=("$object")
done
for name in argon2 bloom; do
    test_flags=()
    if [[ $name == argon2 ]]; then test_flags=(-DNDEBUG); fi
    run "$name-c-build" 90 "$cc" "${flags[@]}" "${test_flags[@]}" "release/extlibs/$name/test/test_$name.c" "${objects[@]}" "$work/runtime.o" -lm -ldl -o "$work/$name"
    run "$name-c" 30 "$work/$name"
    run "$name-example-gcc-build" 90 "$cc" "${flags[@]}" "release/examples/extlibs/${name}_c.c" "$work/runtime.o" -lm -ldl -o "$work/$name-example"
    run "$name-example-gcc" 30 "$work/$name-example"
    golden "release/examples/extlibs/${name}_basic.expected.txt" "$work/$name-example-gcc.stdout"
done
run allocation-build 90 "$cc" "${flags[@]}" test/cases/extlib_argon2_bloom_alloc.c "${objects[@]}" "$work/runtime.o" -lm -ldl -o "$work/allocation"
run allocation 30 "$work/allocation"
# 分配器/故障钩子仅在独立原生进程运行，不能向可卸载脚本宿主留下回调。
run argon2-phc-c-build 90 "$cc" "${flags[@]}" -DNDEBUG "$argon/test/test_argon2_phc.c" "${objects[@]}" "$work/runtime.o" -lm -ldl -o "$work/argon2-phc"
run argon2-phc-c 30 "$work/argon2-phc"
run phc-policy-gcc-build 90 "$cc" "${flags[@]}" -DNDEBUG "$argon/examples/phc_policy_c.c" "${objects[@]}" "$work/runtime.o" -lm -ldl -o "$work/phc-policy-gcc"
run phc-policy-gcc 30 "$work/phc-policy-gcc"
golden "$argon/examples/phc_policy.expected.txt" "$work/phc-policy-gcc.stdout"
xl=./release/xl
for name in argon2 bloom; do
    for kind in central basic tutorial c; do
        case "$kind" in
            central) source="test/cases/extlib_$name.xl" ;;
            basic) source="release/examples/extlibs/${name}_basic.xl" ;;
            tutorial) source="release/examples/extlibs/$name.xl" ;;
            c) source="release/examples/extlibs/${name}_c.c" ;;
        esac
        run "$name-$kind-jit" 180 "$xl" run "$source"
        run "$name-$kind-build" 180 "$xl" build "$source" -o "$work/$name-$kind"
        run "$name-$kind-aot" 120 "$work/$name-$kind"
        if [[ $kind == basic || $kind == c ]]; then
            golden "release/examples/extlibs/${name}_basic.expected.txt" "$work/$name-$kind-jit.stdout"
            golden "release/examples/extlibs/${name}_basic.expected.txt" "$work/$name-$kind-aot.stdout"
        fi
    done
    run "$name-cgen" 90 "$xl" cgen "test/cases/extlib_$name.xl" --emit "$work/$name-module.c"
    prefix=xargon2_
    header=argon2_native.h
    if [[ $name == bloom ]]; then prefix='(xbloom_|xcounting_bloom_)'; header=bloom.h; fi
    grep -E "^extern .*$prefix[a-z_]+\\(" "$work/$name-module.c" | sort -u > "$work/$name-decls.txt"
    grep -oE "extern[[:space:]]+\"$prefix[a-z_]+\"" "release/extlibs/$name/src/$name.xl" | cut -d '"' -f 2 | sort -u > "$work/$name-expected-symbols.txt"
    grep -oE "$prefix[a-z_]+\\(" "$work/$name-decls.txt" | tr -d '(' | sort -u > "$work/$name-actual-symbols.txt"
    diff -u "$work/$name-expected-symbols.txt" "$work/$name-actual-symbols.txt"
    [[ $(wc -l < "$work/$name-decls.txt") == $(wc -l < "$work/$name-expected-symbols.txt") ]]
    printf '#include <stdint.h>\n#include "%s"\n' "$header" > "$work/$name-abi.c"
    cat "$work/$name-decls.txt" >> "$work/$name-abi.c"
    run "$name-abi" 60 "$cc" "${flags[@]}" -I"release/extlibs/$name/include" -fsyntax-only "$work/$name-abi.c"
done
# 四条@owned出口必须直接接管，不让测试只验证结果而漏掉重复字符串复制。
for symbol in xargon2_hash xargon2_hash_raw xargon2_extract xargon2_generate_salt; do
    grep -Eq "v[0-9]+\\.Data[[:space:]]*=[[:space:]]*$symbol[[:space:]]*\\(" "$work/argon2-module.c"
done
for source in "$argon/test/ownership.xl" "$argon/test/phc.xl" "$bloom/test/nul_keys.xl"; do
    label=$(basename "$(dirname "$source")")-$(basename "$source" .xl)
    run "$label-jit" 120 "$xl" run "$source"
    run "$label-build" 180 "$xl" build "$source" -o "$work/$label"
    run "$label-aot" 120 "$work/$label"
done
for language in c xl; do
    source="$argon/examples/phc_policy.xl"
    if [[ $language == c ]]; then source="$argon/examples/phc_policy_c.c"; fi
    run "phc-policy-$language-jit" 90 "$xl" run "$source"
    run "phc-policy-$language-build" 180 "$xl" build "$source" -o "$work/phc-policy-$language"
    run "phc-policy-$language-aot" 90 "$work/phc-policy-$language"
    golden "$argon/examples/phc_policy.expected.txt" "$work/phc-policy-$language-jit.stdout"
    golden "$argon/examples/phc_policy.expected.txt" "$work/phc-policy-$language-aot.stdout"
done
phase=summary
printf 'Argon2/Bloom Linux C ASan/UBSan, XLang JIT/AOT and parity gate: PASS\n'
