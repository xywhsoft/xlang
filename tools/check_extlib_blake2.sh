#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"
tmp_root=$(realpath -m -- "${TMPDIR:-/tmp}")
work=$(mktemp -d "$tmp_root/xlang-blake2-XXXXXXXX")
cc=${CC:-gcc}
pkg=release/extlibs/blake2
xl=./release/xl
phase=setup
issues=0
input_paths() {
    { find "$pkg" release/include -type f -print0
      printf '%s\0' lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so release/libxlc.so \
        test/support/extlib_codec_runtime.c tools/check_extlib_blake2.sh \
        test/cases/extlib_blake2.xl test/cases/extlib_blake2_boundaries.xl test/cases/extlib_blake2_progress.xl \
        release/examples/extlibs/blake2.xl release/examples/extlibs/blake2_basic.xl release/examples/extlibs/blake2_basic.expected.txt
      compiler=$(command -v "$cc" || true)
      if [[ -f $compiler ]]; then printf '%s\0' "$compiler"; fi
    } | sort -zu
}
mapfile -d '' inputs < <(input_paths)
printf '%s\0' "${inputs[@]}" > "$work/input-paths.before"
sha256sum "${inputs[@]}" > "$work/inputs.before.sha256"
cleanup() {
    result=$?
    trap - EXIT
    set +e
    sha256sum "${inputs[@]}" > "$work/inputs.after.sha256"
    diff -u "$work/inputs.before.sha256" "$work/inputs.after.sha256" > "$work/input-changes.txt"
    changed=$?
    input_paths > "$work/input-paths.after"
    cmp -s "$work/input-paths.before" "$work/input-paths.after" || changed=1
    if ((changed != 0 && result == 0)); then result=1; fi
    printf 'exit=%s\nphase=%s\nchanged-inputs=%s\nartifacts=%s\n' "$result" "$phase" "$changed" "$work" > "$work.report.txt"
    if [[ -f $work/phases.tsv ]]; then cat "$work/phases.tsv" >> "$work.report.txt"; fi
    cat "$work/inputs.before.sha256" >> "$work.report.txt"
    if ((result == 0)); then
        case "$work" in "$tmp_root"/xlang-blake2-*) rm -rf -- "$work" || result=$? ;; esac
    else printf '[blake2] FAIL phase=%s exit=%s artifacts=%s\n' "$phase" "$result" "$work" >&2; fi
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
    printf '[blake2] %s\n' "$phase"
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
reject() {
    local name=$1 pattern=$2 status=0
    run "$name" 30 "$xl" check "$pkg/test/$name.xl" || status=$?
    cat "$work/$name.stdout" "$work/$name.stderr" > "$work/$name.diagnostic"
    if [[ $status != 1 ]] || ! grep -Eiq "$pattern" "$work/$name.diagnostic"; then
        printf '[blake2] required rejection failed: %s raw=%s\n' "$name" "$status" >&2
        issues=$((issues + 1))
    fi
}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt -Ilib/xruntime
       -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
run compiler-info 60 "$cc" --version
run runtime-compile 120 "$cc" "${flags[@]}" -DXRT_MODULE_CODEC_HEX -DXRT_MODULE_THREAD -c test/support/extlib_codec_runtime.c -o "$work/runtime.o"
sources=("$pkg/third_party/blake2.c" "$pkg/xrt_blake2.c" "$pkg/blake2_native.c")
objects=()
for index in "${!sources[@]}"; do
    object="$work/native-$index.o"
    strict=()
    if [[ $index != 0 ]]; then strict=(-Wmissing-prototypes); fi
    run "native-$index" 90 "$cc" "${flags[@]}" "${strict[@]}" -c "${sources[$index]}" -o "$object"
    objects+=("$object")
done
for name in test_blake2 test_blake2_alloc test_blake2_native test_blake2_close; do
    selected=("${objects[@]}")
    # 仅本测试编译单元替换文件打开入口，公开薄绑定本身仍真实执行。
    if [[ $name == test_blake2_close ]]; then selected=("${objects[0]}" "${objects[1]}"); fi
    run "$name-compile" 90 "$cc" "${flags[@]}" -DNDEBUG "$pkg/test/$name.c" "${selected[@]}" "$work/runtime.o" -lm -ldl -o "$work/$name"
    run "$name-run" 60 "$work/$name"
done
run basic-c-compile 90 "$cc" "${flags[@]}" -DNDEBUG "$pkg/examples/basic.c" "${objects[@]}" "$work/runtime.o" -lm -ldl -o "$work/basic"
run basic-c-run 30 "$work/basic"
golden release/examples/extlibs/blake2_basic.expected.txt "$work/basic-c-run.stdout"
run basic-c-direct 90 "$xl" run "$pkg/examples/basic.c"
golden release/examples/extlibs/blake2_basic.expected.txt "$work/basic-c-direct.stdout"
run core-c-direct 90 "$xl" run "$pkg/test/test_blake2.c" "${sources[@]}" --inc-path lib/xrt --inc-path lib/xruntime
# 含分配器钩子的测试只在独立原生进程执行，不给卸载后的宿主留下回调地址。
for name in native_b_visibility_negative native_b_keyed_visibility_negative native_s_visibility_negative native_s_keyed_visibility_negative; do
    reject "$name" 'no matching callable class member'
done
for name in keyed_b_pointer_length_negative keyed_s_pointer_length_negative; do
    reject "$name" '\[sem\].*create'
done
for name in extlib_blake2 extlib_blake2_boundaries extlib_blake2_progress vectors blake2 blake2_basic; do
    case "$name" in
        extlib_*) source="test/cases/$name.xl" ;;
        vectors) source="$pkg/test/vectors.xl" ;;
        *) source="release/examples/extlibs/$name.xl" ;;
    esac
    run "$name-jit" 90 "$xl" run "$source"
    run "$name-build" 180 "$xl" build "$source" -o "$work/$name"
    run "$name-aot" 90 "$work/$name"
    diff -u "$work/$name-jit.stdout" "$work/$name-aot.stdout"
done
golden release/examples/extlibs/blake2_basic.expected.txt "$work/blake2_basic-aot.stdout"
for name in extlib_blake2 extlib_blake2_boundaries extlib_blake2_progress; do
    run "$name-cgen" 90 "$xl" cgen "test/cases/$name.xl" --emit "$work/module-$name.c"
done
grep -hE '^extern .*xblake2[bs]?_[a-z_]+\(' "$work"/module-*.c | sort -u > "$work/decls.txt"
grep -oE 'extern "xblake2[bs]?_[a-z_]+"' "$pkg/src/blake2.xl" | cut -d '"' -f 2 | sort -u > "$work/expected-symbols.txt"
grep -oE 'xblake2[bs]?_[a-z_]+\(' "$work/decls.txt" | tr -d '(' | sort -u > "$work/actual-symbols.txt"
diff -u "$work/expected-symbols.txt" "$work/actual-symbols.txt"
[[ $(wc -l < "$work/decls.txt") == 38 && $(wc -l < "$work/expected-symbols.txt") == 38 ]]
awk '/^export const int BLAKE2/ {macro=$4; sub(/OUT_LEN/,"OUT_MAX",macro); printf "_Static_assert(%s == XRT_%s, \"%s\");\n",$6,macro,$4}' "$pkg/src/blake2.xl" > "$work/constants.txt"
[[ $(wc -l < "$work/constants.txt") == 4 ]]
printf '#include "blake2_native.h"\n#include "xrt_blake2.h"\n' > "$work/abi.c"
cat "$work/decls.txt" "$work/constants.txt" >> "$work/abi.c"
# 正式头约定在首次包含XRT前选择模块；native头先包含，不能到后面才启用IO类型。
run generated-abi 60 "$cc" "${flags[@]}" -DXRT_MODULE_IO_BUFFER -I"$pkg/include" -fsyntax-only "$work/abi.c"
run basic-c-build 180 "$xl" build "$pkg/examples/basic.c" -o "$work/basic_c_aot"
run basic-c-aot 30 "$work/basic_c_aot"
golden release/examples/extlibs/blake2_basic.expected.txt "$work/basic-c-aot.stdout"
run file-progress-c-compile 90 "$cc" "${flags[@]}" -DNDEBUG "$pkg/examples/file_progress_c.c" "${objects[@]}" "$work/runtime.o" -lm -ldl -o "$work/file-progress-gcc"
run file-progress-c-run 30 "$work/file-progress-gcc"
golden "$pkg/examples/file_progress.expected.txt" "$work/file-progress-c-run.stdout"
for language in c xl; do
    source="$pkg/examples/file_progress.xl"
    if [[ $language == c ]]; then source="$pkg/examples/file_progress_c.c"; fi
    run "file-progress-$language-jit" 90 "$xl" run "$source"
    run "file-progress-$language-build" 180 "$xl" build "$source" -o "$work/file-progress-$language"
    run "file-progress-$language-aot" 90 "$work/file-progress-$language"
    golden "$pkg/examples/file_progress.expected.txt" "$work/file-progress-$language-jit.stdout"
    golden "$pkg/examples/file_progress.expected.txt" "$work/file-progress-$language-aot.stdout"
done
# 负例只检查，不执行伪造句柄；全局对象正例不得改为局部对象来换绿灯。
reject private_adopt_constructor_negative 'access|private'
run global-lifecycle-jit 90 "$xl" run "$pkg/test/global_lifecycle.xl"
run global-lifecycle-build 180 "$xl" build "$pkg/test/global_lifecycle.xl" -o "$work/global-lifecycle"
run global-lifecycle-aot 90 "$work/global-lifecycle" || issues=$((issues + 1))
phase=assert-public-boundaries
((issues == 0))
phase=summary
printf 'BLAKE2 Linux C ASan/UBSan, XLang JIT/AOT and parity gate: PASS\n'
