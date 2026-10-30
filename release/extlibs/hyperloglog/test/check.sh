#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-hll-XXXXXX")
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
    else printf '[hyperloglog] FAIL phase=%s exit=%s artifacts=%s\n' "$phase" "$result" "$work" >&2; fi
    exit "$result"
}
run() {
    phase=$1
    shift
    local result=0
    printf '[hyperloglog] %s\n' "$phase"
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
    # 仅统一固定输出的文本行尾，不删除正文中的独立 CR。
    sed 's/\r$//' "$1" > "$work/expected.txt"
    diff -u "$work/expected.txt" "$2"
}

mapfile -d '' inputs < <(
    { find release/extlibs/hyperloglog release/include -type f -print0
      printf '%s\0' lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so release/libxlc.so \
          test/cases/extlib_hyperloglog.xl release/examples/extlibs/hyperloglog.xl
      compiler=$(command -v "$cc" || true)
      if [[ -f "$compiler" ]]; then printf '%s\0' "$compiler"; fi
    } | sort -zu
)
sha256sum "${inputs[@]}" > "$work/inputs.before.sha256"
trap cleanup EXIT
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt -Irelease/extlibs/hyperloglog
       -Irelease/extlibs/hyperloglog/include -fsanitize=address,undefined,float-cast-overflow
       -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
xl=release/xl
run compiler-info timeout 120s "$cc" --version
run manifest-native timeout 120s "$cc" "${flags[@]}" -DXRT_MODULE_HASH64 -fsyntax-only release/extlibs/hyperloglog/hyperloglog_native.c
run public-abi timeout 120s "$cc" "${flags[@]}" -fsyntax-only release/extlibs/hyperloglog/test/abi_contract.c
run runtime-compile timeout 120s "$cc" "${flags[@]}" -c release/extlibs/hyperloglog/test/xrt_runtime.c -o "$work/runtime.o"
run native-compile timeout 120s "$cc" "${flags[@]}" -c release/extlibs/hyperloglog/hyperloglog_native.c -o "$work/native.o"
for name in test_hyperloglog test_threads register_vectors; do
    objects=("$work/runtime.o")
    [[ $name == register_vectors ]] || objects+=("$work/native.o")
    run "$name-compile" timeout 120s "$cc" "${flags[@]}" "release/extlibs/hyperloglog/test/$name.c" \
        "${objects[@]}" -lm -ldl -o "$work/$name"
    run "$name-run" timeout 90s "$work/$name"
done
run controlled-vectors-direct timeout 90s "$xl" run release/extlibs/hyperloglog/test/register_vectors.c
for name in central boundaries named_type allocations zero_allocations; do
    source="release/extlibs/hyperloglog/test/$name.xl"
    [[ $name != central ]] || source=test/cases/extlib_hyperloglog.xl
    run "$name-jit" timeout 90s "$xl" run "$source"
done
for method in add merge; do
    status=0
    run "private-$method" timeout 90s "$xl" check "release/extlibs/hyperloglog/test/private_${method}_negative.xl" || status=$?
    [[ $status == 1 ]]
    cat "$work/private-$method.stdout" "$work/private-$method.stderr" > "$work/private-$method.txt"
    grep -Fq "${method}Native" "$work/private-$method.txt"
    grep -Fq access "$work/private-$method.txt"
done

# 真实八个原型交正式头检查全部参数和返回类型，不只核对符号名。
run binding-cgen timeout 90s "$xl" cgen release/extlibs/hyperloglog/test/zero_allocations.xl --emit "$work/module.c"
grep -E '^extern .*xhll_[a-z_]+\(' "$work/module.c" | sort -u > "$work/decls.txt"
[[ $(wc -l < "$work/decls.txt") == 8 ]]
grep -oE 'extern "xhll_[a-z_]+"' release/extlibs/hyperloglog/src/hyperloglog.xl | cut -d '"' -f 2 | sort -u > "$work/expected-symbols.txt"
grep -oE 'xhll_[a-z_]+' "$work/decls.txt" | sort -u > "$work/actual-symbols.txt"
diff -u "$work/expected-symbols.txt" "$work/actual-symbols.txt"
printf '#include "include/hyperloglog_native.h"\n' > "$work/abi.c"
cat "$work/decls.txt" >> "$work/abi.c"
run generated-abi timeout 120s "$cc" "${flags[@]}" -fsyntax-only "$work/abi.c"

for name in basic shard_union; do
    c_source=release/extlibs/hyperloglog/examples/hyperloglog_c.c
    xl_source=release/examples/extlibs/hyperloglog.xl
    golden=release/extlibs/hyperloglog/test/hyperloglog.expected.txt
    if [[ $name == shard_union ]]; then
        c_source=release/extlibs/hyperloglog/examples/shard_union_c.c
        xl_source=release/extlibs/hyperloglog/examples/shard_union.xl
        golden=release/extlibs/hyperloglog/examples/shard_union.expected.txt
    fi
    run "$name-c-compile" timeout 120s "$cc" "${flags[@]}" "$c_source" "$work/runtime.o" "$work/native.o" -lm -ldl -o "$work/$name"
    run "$name-c-run" timeout 90s "$work/$name"
    golden "$golden" "$work/$name-c-run.stdout"
    run "$name-direct" timeout 90s "$xl" run "$c_source"
    golden "$golden" "$work/$name-direct.stdout"
    run "$name-jit" timeout 90s "$xl" run "$xl_source"
    golden "$golden" "$work/$name-jit.stdout"
done
sources=(test/cases/extlib_hyperloglog.xl release/extlibs/hyperloglog/test/zero_allocations.xl
    release/extlibs/hyperloglog/test/register_vectors.c release/examples/extlibs/hyperloglog.xl
    release/extlibs/hyperloglog/examples/hyperloglog_c.c release/extlibs/hyperloglog/examples/shard_union.xl
    release/extlibs/hyperloglog/examples/shard_union_c.c)
for index in "${!sources[@]}"; do
    source=${sources[$index]}
    run "aot-$index-build" timeout 120s "$xl" build "$source" -o "$work/aot-$index"
    run "aot-$index-run" timeout 45s "$work/aot-$index"
    if [[ $source == */shard_union* ]]; then
        golden release/extlibs/hyperloglog/examples/shard_union.expected.txt "$work/aot-$index-run.stdout"
    elif [[ $source == release/examples/extlibs/hyperloglog.xl || $source == */examples/hyperloglog_c.c ]]; then
        golden release/extlibs/hyperloglog/test/hyperloglog.expected.txt "$work/aot-$index-run.stdout"
    fi
done
# 各顶层原正例独立执行并聚合失败，每项仍要求0，不把M2当成预期非零。
global_failed=0
for name in boundaries named_type allocations; do
    if run "$name-global-build" timeout 120s "$xl" build "release/extlibs/hyperloglog/test/$name.xl" -o "$work/global-$name"; then
        if run "$name-global-run" timeout 45s "$work/global-$name"; then :;
        else printf '%s\t%s\n' "$name" "$?" >> "$work/global-failures.tsv"; global_failed=1; fi
    else printf '%s-build\t%s\n' "$name" "$?" >> "$work/global-failures.tsv"; global_failed=1; fi
done
phase=global-summary
if ((global_failed)); then cat "$work/global-failures.tsv" >&2; exit 1; fi
printf 'hyperloglog Linux C/XL ASan/UBSan gate: PASS\n'
