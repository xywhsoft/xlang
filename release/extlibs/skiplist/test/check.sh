#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-skiplist-XXXXXX")
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
    else printf '[skiplist] FAIL phase=%s exit=%s artifacts=%s\n' "$phase" "$result" "$work" >&2; fi
    exit "$result"
}
run() {
    phase=$1
    shift
    local result=0
    printf '[skiplist] %s\n' "$phase"
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
    # 固定范例按行比较，只统一 CRLF，不删除正文中的独立 CR。
    sed 's/\r$//' "$1" > "$work/expected.txt"
    diff -u "$work/expected.txt" "$2"
}

mapfile -d '' inputs < <(
    { find release/extlibs/skiplist release/include -type f -print0
      printf '%s\0' lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so release/libxlc.so \
          test/cases/extlib_skiplist.xl release/examples/extlibs/skiplist.xl release/examples/extlibs/skiplist_c.c
      compiler=$(command -v "$cc" || true)
      if [[ -f "$compiler" ]]; then printf '%s\0' "$compiler"; fi
    } | sort -zu
)
sha256sum "${inputs[@]}" > "$work/inputs.before.sha256"
trap cleanup EXIT
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt -Irelease/extlibs/skiplist
       -Irelease/extlibs/skiplist/include -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
xl=release/xl
run compiler-info timeout 120s "$cc" --version
run public-abi timeout 120s "$cc" "${flags[@]}" -fsyntax-only release/extlibs/skiplist/test/abi_contract.c
run runtime-compile timeout 120s "$cc" "${flags[@]}" -c release/extlibs/skiplist/test/xrt_runtime.c -o "$work/runtime.o"
run native-compile timeout 120s "$cc" "${flags[@]}" -c release/extlibs/skiplist/skiplist_native.c -o "$work/native.o"
for name in test_skiplist entropy_failure snapshot_failures example_failure; do
    objects=("$work/runtime.o")
    [[ $name != test_skiplist ]] || objects+=("$work/native.o")
    run "$name-compile" timeout 120s "$cc" "${flags[@]}" "release/extlibs/skiplist/test/$name.c" \
        "${objects[@]}" -lm -ldl -o "$work/$name"
    run "$name-run" timeout 90s "$work/$name"
done
for name in snapshot_failures example_failure; do
    run "$name-direct" timeout 90s "$xl" run "release/extlibs/skiplist/test/$name.c"
done
for name in central allocations snapshots; do
    source="release/extlibs/skiplist/test/$name.xl"
    [[ $name != central ]] || source=test/cases/extlib_skiplist.xl
    run "$name-jit" timeout 90s "$xl" run "$source"
done
status=0
run private-native timeout 90s "$xl" check release/extlibs/skiplist/test/private_native.xl || status=$?
[[ $status == 1 ]]
cat "$work/private-native.stdout" "$work/private-native.stderr" > "$work/private-native.txt"
grep -Fq setNative "$work/private-native.txt"
grep -Fq access "$work/private-native.txt"

# 真实 cgen 原型与正式头交叉编译，防止仅比符号名漏掉宽度/返回类型问题。
run binding-cgen timeout 90s "$xl" cgen release/extlibs/skiplist/examples/binary_snapshots.xl --emit "$work/module.c"
grep -E '^extern .*xskiplist_[a-z_]+\(' "$work/module.c" | sort -u > "$work/decls.txt"
[[ $(wc -l < "$work/decls.txt") == 10 ]]
grep -oE 'extern "xskiplist_[a-z_]+"' release/extlibs/skiplist/src/skiplist.xl | cut -d '"' -f 2 | sort -u > "$work/expected-symbols.txt"
grep -oE 'xskiplist_[a-z_]+' "$work/decls.txt" | sort -u > "$work/actual-symbols.txt"
diff -u "$work/expected-symbols.txt" "$work/actual-symbols.txt"
printf '#include "include/skiplist_native.h"\n' > "$work/abi.c"
cat "$work/decls.txt" >> "$work/abi.c"
run generated-abi timeout 120s "$cc" "${flags[@]}" -fsyntax-only "$work/abi.c"

for name in basic binary_snapshots; do
    c_source=release/examples/extlibs/skiplist_c.c
    xl_source=release/examples/extlibs/skiplist.xl
    golden=release/extlibs/skiplist/test/skiplist.expected.txt
    if [[ $name == binary_snapshots ]]; then
        c_source=release/extlibs/skiplist/examples/binary_snapshots_c.c
        xl_source=release/extlibs/skiplist/examples/binary_snapshots.xl
        golden=release/extlibs/skiplist/examples/binary_snapshots.expected.txt
    fi
    run "$name-c-compile" timeout 120s "$cc" "${flags[@]}" "$c_source" "$work/runtime.o" -lm -ldl -o "$work/$name"
    run "$name-c-run" timeout 90s "$work/$name"
    golden "$golden" "$work/$name-c-run.stdout"
    run "$name-direct" timeout 90s "$xl" run "$c_source"
    golden "$golden" "$work/$name-direct.stdout"
    run "$name-jit" timeout 90s "$xl" run "$xl_source"
    golden "$golden" "$work/$name-jit.stdout"
done
sources=(release/extlibs/skiplist/test/aot_core.xl test/cases/extlib_skiplist.xl
    release/extlibs/skiplist/test/snapshots.xl release/extlibs/skiplist/test/snapshot_failures.c
    release/extlibs/skiplist/test/example_failure.c release/examples/extlibs/skiplist.xl release/examples/extlibs/skiplist_c.c
    release/extlibs/skiplist/examples/binary_snapshots.xl release/extlibs/skiplist/examples/binary_snapshots_c.c)
for index in "${!sources[@]}"; do
    source=${sources[$index]}
    run "aot-$index-build" timeout 120s "$xl" build "$source" -o "$work/aot-$index"
    run "aot-$index-run" timeout 45s "$work/aot-$index"
    if [[ $source == */binary_snapshots* ]]; then
        golden release/extlibs/skiplist/examples/binary_snapshots.expected.txt "$work/aot-$index-run.stdout"
    elif [[ $source == release/examples/extlibs/skiplist* ]]; then
        golden release/extlibs/skiplist/test/skiplist.expected.txt "$work/aot-$index-run.stdout"
    fi
done
# 原顶层分配用例仍须正常退出，不改为局部对象或预期失败。
run original-global-build timeout 120s "$xl" build release/extlibs/skiplist/test/allocations.xl -o "$work/original-global"
run original-global-run timeout 45s "$work/original-global"
printf 'skiplist Linux C ASan/UBSan and bilingual AOT gate: PASS\n'
