#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-cli-XXXXXX")
phase=setup
cc=${CC:-gcc}
xl=release/xl
failures=0
mapfile -d '' inputs < <(
    { find release/extlibs/cli release/include -type f -print0
      printf '%s\0' lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so release/libxlc.so \
          test/cases/extlib_cli.xl dev/extlib_blockers/cli_os_args_symbol.xl \
          dev/extlib_blockers/cli_args_projection.xl dev/extlib_blockers/cli_args_projection.c
      compiler=$(command -v "$cc" || true)
      if [[ -f "$compiler" ]]; then printf '%s\0' "$compiler"; fi
    } | sort -zu
)
sha256sum "${inputs[@]}" > "$work/inputs.before.sha256"
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
    else printf '[cli] FAIL phase=%s exit=%s artifacts=%s\n' "$phase" "$result" "$work" >&2; fi
    exit "$result"
}
trap cleanup EXIT
run() {
    phase=$1
    local seconds=$2 result=0
    shift 2
    printf '[cli] %s\n' "$phase"
    printf '%q ' "$@" > "$work/$phase.command"
    printf '\n' >> "$work/$phase.command"
    timeout "${seconds}s" "$@" > "$work/$phase.stdout" 2> "$work/$phase.stderr" || result=$?
    printf '%s\n' "$result" > "$work/$phase.exit"
    printf '%s\t%s\n' "$phase" "$result" >> "$work/phases.tsv"
    cat "$work/$phase.stdout"
    cat "$work/$phase.stderr" >&2
    return "$result"
}
observe() {
    if run "$@"; then :; else failures=$((failures + 1)); fi
}
golden() {
    sed 's/\r$//' "$1" > "$work/expected.txt"
    diff -u "$work/expected.txt" "$2"
}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -DNDEBUG -Wall -Wextra -Werror -Ilib/xrt -Irelease/extlibs/cli/include
       -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
run compiler-info 120 "$cc" --version
run runtime-compile 120 "$cc" "${flags[@]}" -c release/extlibs/cli/test/xrt_runtime.c -o "$work/runtime.o"
run native-compile 120 "$cc" "${flags[@]}" -Wmissing-prototypes -c release/extlibs/cli/cli_native.c -o "$work/native.o"
for name in test_cli test_defaults; do
    run "$name-compile" 120 "$cc" "${flags[@]}" "release/extlibs/cli/test/$name.c" "$work/runtime.o" "$work/native.o" -lm -ldl -o "$work/$name"
    run "$name-run" 90 "$work/$name"
done
for name in basic options; do
    source="release/extlibs/cli/examples/${name}_c.c"
    expected="release/extlibs/cli/examples/$name.expected.txt"
    run "$name-c-compile" 120 "$cc" "${flags[@]}" "$source" "$work/runtime.o" "$work/native.o" -lm -ldl -o "$work/$name"
    run "$name-c-run" 90 "$work/$name"
    golden "$expected" "$work/$name-c-run.stdout"
    run "$name-c-direct" 90 "$xl" run "$source"
    golden "$expected" "$work/$name-c-direct.stdout"
    run "$name-c-aot-build" 120 "$xl" build "$source" -o "$work/$name-aot"
    run "$name-c-aot-run" 45 "$work/$name-aot"
    golden "$expected" "$work/$name-c-aot-run.stdout"
done
# 原始语言路径逐项要求0，不能靠去掉parseOs或改用C容器工厂换绿灯。
for name in central basic options os-args projection; do
    source="release/extlibs/cli/examples/$name.xl"
    [[ $name != central ]] || source=test/cases/extlib_cli.xl
    [[ $name != os-args ]] || source=dev/extlib_blockers/cli_os_args_symbol.xl
    [[ $name != projection ]] || source=dev/extlib_blockers/cli_args_projection.xl
    observe "$name-check" 90 "$xl" check "$source"
    observe "$name-jit" 90 "$xl" run "$source"
    if run "$name-build" 120 "$xl" build "$source" -o "$work/lang-$name"; then
        observe "$name-aot" 45 "$work/lang-$name"
        if [[ $name == basic || $name == options ]] && [[ $(< "$work/$name-aot.exit") == 0 ]]; then
            golden "release/extlibs/cli/examples/$name.expected.txt" "$work/$name-aot.stdout"
        fi
    else failures=$((failures + 1)); fi
    if [[ $name == basic || $name == options ]] && [[ $(< "$work/$name-jit.exit") == 0 ]]; then
        golden "release/extlibs/cli/examples/$name.expected.txt" "$work/$name-jit.stdout"
    fi
    run "$name-cgen" 90 "$xl" cgen "$source" --emit "$work/module-$name.c"
done
grep -hE '^extern .*xcli_parse\(' "$work"/module-*.c | sort -u > "$work/decls.txt"
[[ $(wc -l < "$work/decls.txt") == 1 ]]
printf '#include "cli_native.h"\n' > "$work/abi.c"
cat "$work/decls.txt" >> "$work/abi.c"
run generated-abi 120 "$cc" "${flags[@]}" -fsyntax-only "$work/abi.c"
phase=language-summary
if ((failures)); then printf 'CLI language boundary failures: %s\n' "$failures" >&2; exit 1; fi
printf 'cli native/XL C ASan/UBSan gate: PASS\n'
