#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-consistent-hash-XXXXXX")
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
    else printf '[consistent_hash] FAIL phase=%s exit=%s artifacts=%s\n' "$phase" "$result" "$work" >&2; fi
    exit "$result"
}
run() {
    phase=$1
    shift
    local result=0
    printf '[consistent_hash] %s\n' "$phase"
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
    # 固定范例统一文本行尾，不删除正文中的独立 CR。
    sed 's/\r$//' "$1" > "$work/expected.txt"
    diff -u "$work/expected.txt" "$2"
}

mapfile -d '' inputs < <(
    { find release/extlibs/consistent_hash release/include -type f -print0
      printf '%s\0' lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so release/libxlc.so \
          test/cases/extlib_consistent_hash.xl release/examples/extlibs/consistent_hash.xl
      compiler=$(command -v "$cc" || true)
      if [[ -f "$compiler" ]]; then printf '%s\0' "$compiler"; fi
    } | sort -zu
)
sha256sum "${inputs[@]}" > "$work/inputs.before.sha256"
trap cleanup EXIT
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt -Irelease/extlibs/consistent_hash
       -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
xl=release/xl
run compiler-info timeout 120s "$cc" --version
run public-header timeout 120s "$cc" "${flags[@]}" -x c -fsyntax-only release/extlibs/consistent_hash/include/consistent_hash_native.h
run runtime-compile timeout 120s "$cc" "${flags[@]}" -c release/extlibs/consistent_hash/test/xrt_runtime.c -o "$work/runtime.o"
run native-compile timeout 120s "$cc" "${flags[@]}" -c release/extlibs/consistent_hash/consistent_hash_native.c -o "$work/native.o"
for name in test_consistent_hash test_threads owned_budgets_c; do
    run "$name-compile" timeout 120s "$cc" "${flags[@]}" "release/extlibs/consistent_hash/test/$name.c" \
        "$work/runtime.o" "$work/native.o" -lm -ldl -o "$work/$name"
    run "$name-run" timeout 90s "$work/$name"
done
run c-budgets-direct timeout 90s "$xl" run release/extlibs/consistent_hash/test/owned_budgets_c.c
for name in central boundaries allocations layout owned_budgets owned_probe owned_lifetime; do
    source="release/extlibs/consistent_hash/test/$name.xl"
    [[ $name != central ]] || source=test/cases/extlib_consistent_hash.xl
    run "$name-jit" timeout 90s "$xl" run "$source"
done
status=0
run private-native timeout 90s "$xl" check release/extlibs/consistent_hash/test/private_native.xl || status=$?
[[ $status == 1 ]]
cat "$work/private-native.stdout" "$work/private-native.stderr" > "$work/private-native.txt"
grep -Fq addNative "$work/private-native.txt"
grep -Fq access "$work/private-native.txt"

# 既要求拥有型直接接管，也要求后续返回没有再次复制正文。
run binding-cgen timeout 90s "$xl" cgen release/extlibs/consistent_hash/test/owned_probe.xl --emit "$work/module.c"
grep -Fq 'extern char* xchash_get_node(' "$work/module.c"
grep -Eq '\.Data[[:space:]]*=[[:space:]]*xchash_get_node[[:space:]]*\(' "$work/module.c"
if grep -Eq '=[[:space:]]*xlir_string_copy[[:space:]]*\(' "$work/module.c"; then
    printf 'ConsistentHash return still copies payload\n' >&2
    exit 1
fi
grep -E '^extern .*xchash_[a-z_]+\(' "$work/module.c" | sort -u > "$work/decls.txt"
[[ $(wc -l < "$work/decls.txt") == 7 ]]
grep -oE 'extern "xchash_[a-z_]+"' release/extlibs/consistent_hash/src/consistent_hash.xl | cut -d '"' -f 2 | sort -u > "$work/expected-symbols.txt"
grep -oE 'xchash_[a-z_]+' "$work/decls.txt" | sort -u > "$work/actual-symbols.txt"
diff -u "$work/expected-symbols.txt" "$work/actual-symbols.txt"
printf '#include "include/consistent_hash_native.h"\n' > "$work/abi.c"
cat "$work/decls.txt" >> "$work/abi.c"
run generated-abi timeout 120s "$cc" "${flags[@]}" -fsyntax-only "$work/abi.c"

for name in basic routing_changes; do
    c_source=release/extlibs/consistent_hash/examples/consistent_hash_c.c
    xl_source=release/examples/extlibs/consistent_hash.xl
    golden=release/extlibs/consistent_hash/examples/consistent_hash.expected.txt
    if [[ $name == routing_changes ]]; then
        c_source=release/extlibs/consistent_hash/examples/routing_changes_c.c
        xl_source=release/extlibs/consistent_hash/examples/routing_changes.xl
        golden=release/extlibs/consistent_hash/examples/routing_changes.expected.txt
    fi
    run "$name-c-compile" timeout 120s "$cc" "${flags[@]}" "$c_source" "$work/runtime.o" -lm -ldl -o "$work/$name"
    run "$name-c-run" timeout 90s "$work/$name"
    golden "$golden" "$work/$name-c-run.stdout"
    run "$name-direct" timeout 90s "$xl" run "$c_source"
    golden "$golden" "$work/$name-direct.stdout"
    run "$name-jit" timeout 90s "$xl" run "$xl_source"
    golden "$golden" "$work/$name-jit.stdout"
done
sources=(test/cases/extlib_consistent_hash.xl release/extlibs/consistent_hash/test/owned_budgets.xl
    release/extlibs/consistent_hash/test/owned_probe.xl release/extlibs/consistent_hash/test/owned_lifetime.xl
    release/extlibs/consistent_hash/test/owned_budgets_c.c release/examples/extlibs/consistent_hash.xl
    release/extlibs/consistent_hash/examples/consistent_hash_c.c release/extlibs/consistent_hash/examples/routing_changes.xl
    release/extlibs/consistent_hash/examples/routing_changes_c.c)
for index in "${!sources[@]}"; do
    source=${sources[$index]}
    run "aot-$index-build" timeout 120s "$xl" build "$source" -o "$work/aot-$index"
    run "aot-$index-run" timeout 45s "$work/aot-$index"
    if [[ $source == */routing_changes* ]]; then
        golden release/extlibs/consistent_hash/examples/routing_changes.expected.txt "$work/aot-$index-run.stdout"
    elif [[ $source == release/examples/extlibs/consistent_hash.xl || $source == */examples/consistent_hash_c.c ]]; then
        golden release/extlibs/consistent_hash/examples/consistent_hash.expected.txt "$work/aot-$index-run.stdout"
    fi
done
# 所有原顶层正例独立执行并聚合失败；每项仍要求0，绝不把M2当预期非零。
global_failed=0
for name in boundaries allocations layout; do
    if run "$name-global-build" timeout 120s "$xl" build "release/extlibs/consistent_hash/test/$name.xl" -o "$work/global-$name"; then
        if run "$name-global-run" timeout 45s "$work/global-$name"; then :;
        else printf '%s\t%s\n' "$name" "$?" >> "$work/global-failures.tsv"; global_failed=1; fi
    else printf '%s-build\t%s\n' "$name" "$?" >> "$work/global-failures.tsv"; global_failed=1; fi
done
phase=global-summary
if ((global_failed)); then cat "$work/global-failures.tsv" >&2; exit 1; fi
printf 'consistent_hash Linux C/XL ASan/UBSan gate: PASS\n'
