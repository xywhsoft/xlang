#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-lru-XXXXXX")
phase=setup
cc=${CC:-gcc}
cleanup() {
    result=$?
    trap - EXIT
    set +e
    sha256sum "${inputs[@]}" > "$work/inputs.after.sha256"
    diff -u "$work/inputs.before.sha256" "$work/inputs.after.sha256" > "$work/input-changes.txt"
    changed=$?
    printf 'exit=%s\nphase=%s\nchanged-inputs=%s\nartifacts=%s\n' "$result" "$phase" "$changed" "$work" > "$work.report.txt"
    if ((changed != 0 && result == 0)); then result=1; fi
    if ((result == 0)); then rm -rf -- "$work" || result=$?;
    else printf '[lru] FAIL phase=%s exit=%s artifacts=%s\n' "$phase" "$result" "$work" >&2; fi
    exit "$result"
}
run() {
    phase=$1
    shift
    local result=0
    printf '[lru] %s\n' "$phase"
    printf '%q ' "$@" > "$work/$phase.command"
    printf '\n' >> "$work/$phase.command"
    "$@" > "$work/$phase.stdout" 2> "$work/$phase.stderr" || result=$?
    printf '%s\t%s\n' "$phase" "$result" >> "$work/phases.tsv"
    cat "$work/$phase.stdout"
    cat "$work/$phase.stderr" >&2
    return "$result"
}

mapfile -d '' inputs < <(
    { find release/extlibs/lru release/include -type f -print0
      printf '%s\0' lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so release/libxlc.so \
          test/cases/extlib_lru.xl release/examples/extlibs/lru.xl
      compiler=$(command -v "$cc" || true)
      if [[ -f "$compiler" ]]; then printf '%s\0' "$compiler"; fi
    } | sort -zu
)
sha256sum "${inputs[@]}" > "$work/inputs.before.sha256"
trap cleanup EXIT
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt -Irelease/extlibs/lru
       -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
xl=release/xl
run compiler-info timeout 120s "$cc" --version
run public-header timeout 120s "$cc" "${flags[@]}" -x c -fsyntax-only release/extlibs/lru/include/lru_native.h
run runtime-compile timeout 120s "$cc" "${flags[@]}" -c release/extlibs/lru/test/xrt_runtime.c -o "$work/runtime.o"
run native-compile timeout 120s "$cc" "${flags[@]}" -c release/extlibs/lru/lru_native.c -o "$work/native.o"
for name in test_lru test_threads input_budgets_c; do
    run "$name-compile" timeout 120s "$cc" "${flags[@]}" "release/extlibs/lru/test/$name.c" \
        "$work/native.o" "$work/runtime.o" -lm -ldl -o "$work/$name"
    run "$name-run" timeout 90s "$work/$name"
done
run c-budgets-direct timeout 90s "$xl" run release/extlibs/lru/test/input_budgets_c.c
for name in central boundaries allocations input_budgets input_lifetime owned_probe; do
    source="release/extlibs/lru/test/$name.xl"
    [[ $name != central ]] || source=test/cases/extlib_lru.xl
    run "$name-jit" timeout 90s "$xl" run "$source"
done
status=0
run private-native timeout 90s "$xl" check release/extlibs/lru/test/private_native.xl || status=$?
[[ $status == 1 ]]
cat "$work/private-native.stdout" "$work/private-native.stderr" > "$work/private-native.txt"
grep -Fq putNative "$work/private-native.txt"
grep -Fq access "$work/private-native.txt"

# 真实生成原型必须完整，并由正式 C 头核对所有类型。
run binding-cgen timeout 90s "$xl" cgen release/extlibs/lru/test/owned_probe.xl --emit "$work/module.c"
grep -E '^extern .*xlru_[a-z0-9_]+\(' "$work/module.c" | sort -u > "$work/decls.txt"
[[ $(wc -l < "$work/decls.txt") == 11 ]]
grep -oE 'extern "xlru_[a-z0-9_]+"' release/extlibs/lru/src/lru.xl | cut -d '"' -f 2 | sort -u > "$work/expected-symbols.txt"
grep -oE 'xlru_[a-z0-9_]+' "$work/decls.txt" | sort -u > "$work/actual-symbols.txt"
diff -u "$work/expected-symbols.txt" "$work/actual-symbols.txt"
if grep -Eq '=[[:space:]]*xlir_string_copy[[:space:]]*\(' "$work/module.c"; then
    printf 'LRU validation or binding return still copies payload text\n' >&2
    exit 1
fi
printf '#include "include/lru_native.h"\n' > "$work/abi.c"
cat "$work/decls.txt" >> "$work/abi.c"
run generated-abi timeout 120s "$cc" "${flags[@]}" -fsyntax-only "$work/abi.c"

for name in basic snapshots; do
    c_source=release/extlibs/lru/examples/lru_c.c
    xl_source=release/examples/extlibs/lru.xl
    if [[ $name == snapshots ]]; then
        c_source=release/extlibs/lru/examples/snapshots_c.c
        xl_source=release/extlibs/lru/examples/snapshots.xl
    fi
    sed 's/\r$//' "release/extlibs/lru/examples/$name.expected.txt" > "$work/$name.expected.txt"
    run "$name-compile" timeout 120s "$cc" "${flags[@]}" "$c_source" "$work/runtime.o" -lm -ldl -o "$work/$name"
    run "$name-gcc" timeout 90s "$work/$name"
    run "$name-direct" timeout 90s "$xl" run "$c_source"
    run "$name-xl" timeout 90s "$xl" run "$xl_source"
    for kind in gcc direct xl; do diff -u "$work/$name.expected.txt" "$work/$name-$kind.stdout"; done
done

# 先验可运行独立程序，最后保留原顶层正例的真实退出结果。
sources=(test/cases/extlib_lru.xl release/extlibs/lru/test/allocations.xl
    release/extlibs/lru/test/input_budgets.xl release/extlibs/lru/test/input_lifetime.xl
    release/extlibs/lru/test/owned_probe.xl release/extlibs/lru/test/input_budgets_c.c
    release/examples/extlibs/lru.xl release/extlibs/lru/examples/lru_c.c
    release/extlibs/lru/examples/snapshots.xl release/extlibs/lru/examples/snapshots_c.c)
for index in "${!sources[@]}"; do
    source=${sources[index]}
    run "aot-$index-build" timeout 120s "$xl" build "$source" -o "$work/aot-$index"
    run "aot-$index-run" timeout 90s "$work/aot-$index"
    if [[ $source == *snapshots* ]]; then diff -u "$work/snapshots.expected.txt" "$work/aot-$index-run.stdout";
    elif [[ $source == */lru.xl || $source == */lru_c.c ]]; then diff -u "$work/basic.expected.txt" "$work/aot-$index-run.stdout"; fi
done
run original-global-build timeout 120s "$xl" build release/extlibs/lru/test/boundaries.xl -o "$work/boundaries"
run original-global-run timeout 90s "$work/boundaries"
printf 'lru complete Linux gate: PASS\n'
