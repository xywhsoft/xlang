#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-rtree-XXXXXX")
phase=setup
cc=${CC:-gcc}
cleanup() {
    result=$?
    trap - EXIT
    set +e
    sha256sum "${inputs[@]}" > "$work/inputs.after.sha256"
    diff -u "$work/inputs.before.sha256" "$work/inputs.after.sha256" > "$work/input-changes.txt"
    changed=$?
    if ((changed != 0 && result == 0)); then result=1; fi
    printf 'exit=%s\nphase=%s\nchanged-inputs=%s\nartifacts=%s\n' "$result" "$phase" "$changed" "$work" > "$work.report.txt"
    if ((result == 0)); then rm -rf -- "$work" || result=$?;
    else printf '[rtree] FAIL phase=%s exit=%s artifacts=%s\n' "$phase" "$result" "$work" >&2; fi
    exit "$result"
}
run() {
    phase=$1
    shift
    local result=0
    printf '[rtree] %s\n' "$phase"
    printf '%q ' "$@" > "$work/$phase.command"
    printf '\n' >> "$work/$phase.command"
    "$@" > "$work/$phase.stdout" 2> "$work/$phase.stderr" || result=$?
    printf '%s\t%s\n' "$phase" "$result" >> "$work/phases.tsv"
    cat "$work/$phase.stdout"
    cat "$work/$phase.stderr" >&2
    return "$result"
}
golden() {
    phase="$phase-output"
    # 固定范例是逐行文本；只统一 CRLF，不删除正文中的独立 CR。
    sed 's/\r$//' "$1" > "$work/expected.txt"
    diff -u "$work/expected.txt" "$2"
}

mapfile -d '' inputs < <(
    { find release/extlibs/rtree release/include -type f -print0
      printf '%s\0' lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so release/libxlc.so \
          test/cases/extlib_rtree.xl release/examples/extlibs/rtree.xl release/examples/extlibs/rtree_c.c
      compiler=$(command -v "$cc" || true)
      if [[ -f "$compiler" ]]; then printf '%s\0' "$compiler"; fi
    } | sort -zu
)
sha256sum "${inputs[@]}" > "$work/inputs.before.sha256"
trap cleanup EXIT
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt -Irelease/extlibs/rtree
       -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
xl=release/xl
run compiler-info timeout 120s "$cc" --version
run public-abi timeout 120s "$cc" "${flags[@]}" -fsyntax-only release/extlibs/rtree/test/abi_contract.c
run runtime-compile timeout 120s "$cc" "${flags[@]}" -c release/extlibs/rtree/test/xrt_runtime.c -o "$work/runtime.o"
modes=(native)
phase=narrow-capability
if printf '%s\n' '_Static_assert(sizeof(long double) == sizeof(double), "narrow");' |
    timeout 30s "$cc" -std=c11 -mlong-double-64 -x c -fsyntax-only - > "$work/narrow.stdout" 2> "$work/narrow.stderr"; then
    modes+=(narrow)
fi
printf '%s\n' "${modes[*]}" > "$work/modes.txt"
for mode in "${modes[@]}"; do
    extra=()
    if [[ "$mode" == narrow ]]; then extra=(-mlong-double-64); fi
    run "$mode-native-compile" timeout 120s "$cc" "${flags[@]}" "${extra[@]}" \
        -c release/extlibs/rtree/rtree_native.c -o "$work/native_$mode.o"
    for name in test_rtree split_seeds cascade_rollback; do
        objects=("$work/runtime.o")
        [[ $name != test_rtree ]] || objects+=("$work/native_$mode.o")
        run "$mode-$name-compile" timeout 120s "$cc" "${flags[@]}" "${extra[@]}" \
            "release/extlibs/rtree/test/$name.c" "${objects[@]}" -lm -ldl -o "$work/${mode}_$name"
        run "$mode-$name-run" timeout 90s "$work/${mode}_$name"
    done
done
run cascade-direct timeout 90s "$xl" run release/extlibs/rtree/test/cascade_rollback.c
for name in central boundaries allocations; do
    source="release/extlibs/rtree/test/$name.xl"
    [[ $name != central ]] || source=test/cases/extlib_rtree.xl
    run "$name-jit" timeout 90s "$xl" run "$source"
done
status=0
run private-native timeout 90s "$xl" check release/extlibs/rtree/test/private_native_negative.xl || status=$?
[[ $status == 1 ]]
cat "$work/private-native.stdout" "$work/private-native.stderr" > "$work/private-native.txt"
grep -Fq searchNative "$work/private-native.txt"
grep -Fq access "$work/private-native.txt"

# 生成原型必须完整，并由正式 C 头核对类型而不只比名字。
run binding-cgen timeout 90s "$xl" cgen release/extlibs/rtree/examples/scene.xl --emit "$work/module.c"
grep -E '^extern .*xrtree_[a-z_]+\(' "$work/module.c" | sort -u > "$work/decls.txt"
[[ $(wc -l < "$work/decls.txt") == 8 ]]
grep -oE 'extern "xrtree_[a-z_]+"' release/extlibs/rtree/src/rtree.xl | cut -d '"' -f 2 | sort -u > "$work/expected-symbols.txt"
grep -oE 'xrtree_[a-z_]+' "$work/decls.txt" | sort -u > "$work/actual-symbols.txt"
diff -u "$work/expected-symbols.txt" "$work/actual-symbols.txt"
printf '#define XRT_MODULE_VALUE_CONTAINER\n#include "xrt.h"\n#include "include/rtree_native.h"\n' > "$work/abi.c"
cat "$work/decls.txt" >> "$work/abi.c"
run generated-abi timeout 120s "$cc" "${flags[@]}" -fsyntax-only "$work/abi.c"

for name in basic scene; do
    c_source=release/examples/extlibs/rtree_c.c
    xl_source=release/examples/extlibs/rtree.xl
    golden=release/extlibs/rtree/test/rtree.expected.txt
    if [[ $name == scene ]]; then
        c_source=release/extlibs/rtree/examples/scene_c.c
        xl_source=release/extlibs/rtree/examples/scene.xl
        golden=release/extlibs/rtree/examples/scene.expected.txt
    fi
    run "$name-c-compile" timeout 120s "$cc" "${flags[@]}" "$c_source" "$work/runtime.o" -lm -ldl -o "$work/$name"
    run "$name-c-run" timeout 90s "$work/$name"
    golden "$golden" "$work/$name-c-run.stdout"
    run "$name-direct" timeout 90s "$xl" run "$c_source"
    golden "$golden" "$work/$name-direct.stdout"
    run "$name-jit" timeout 90s "$xl" run "$xl_source"
    golden "$golden" "$work/$name-jit.stdout"
done
sources=(test/cases/extlib_rtree.xl release/extlibs/rtree/test/boundaries.xl
    release/extlibs/rtree/test/cascade_rollback.c release/examples/extlibs/rtree.xl release/examples/extlibs/rtree_c.c
    release/extlibs/rtree/examples/scene.xl release/extlibs/rtree/examples/scene_c.c)
for index in "${!sources[@]}"; do
    source=${sources[$index]}
    run "aot-$index-build" timeout 120s "$xl" build "$source" -o "$work/aot-$index"
    run "aot-$index-run" timeout 45s "$work/aot-$index"
    if [[ $source == */scene* ]]; then
        golden release/extlibs/rtree/examples/scene.expected.txt "$work/aot-$index-run.stdout"
    elif [[ $source == release/examples/extlibs/rtree* ]]; then
        golden release/extlibs/rtree/test/rtree.expected.txt "$work/aot-$index-run.stdout"
    fi
done
# 保留原顶层正例为真正必过项；失败不删现场，也不作预期失败。
run original-global-build timeout 120s "$xl" build release/extlibs/rtree/test/allocations.xl -o "$work/original-global"
run original-global-run timeout 45s "$work/original-global"
printf 'rtree Linux C/XL %s ASan/UBSan gate: PASS\n' "${modes[*]}"
