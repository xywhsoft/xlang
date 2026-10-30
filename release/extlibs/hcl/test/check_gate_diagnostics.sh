#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/hcl-diagnostics-XXXXXX")
cleanup() {
    result=$?
    trap - EXIT
    set +e
    if ((result == 0)); then
        rm -rf -- "$work" || result=$?
    else
        printf 'hcl diagnostic test artifacts: %s\n' "$work" >&2
    fi
    exit "$result"
}
trap cleanup EXIT
cc=${CC:-gcc}
"$cc" -std=c11 -Wall -Wextra -Werror test/cases/c_run_exit_code.c -o "$work/exit37"

expect_failure() {
    local name=$1 compiler=$2 expected=$3 result=0 artifact
    TMPDIR="$work" CC="$compiler" bash release/extlibs/hcl/test/check.sh \
        > "$work/$name.stdout" 2> "$work/$name.stderr" || result=$?
    [[ $result == "$expected" ]]
    artifact=$(sed -n "s/^\[hcl\] FAIL stage=compiler-info exit=$expected artifacts=//p" "$work/$name.stderr")
    case "$artifact" in
        "$work"/xlang-hcl-*) ;;
        *) cat "$work/$name.stderr" >&2; return 1 ;;
    esac
    [[ -d "$artifact" && $(cat "$artifact/failed-stage.txt") == compiler-info ]]
    [[ $(cat "$artifact/exit-code.txt") == "$expected" ]]
    [[ -s "$artifact/compiler-info.command" && -s "$artifact/fingerprint.stdout" ]]
    grep -Fq '  release/libxlc.so' "$artifact/fingerprint.stdout"
    grep -Fxq $'compiler-info\t'"$expected" "$artifact/phases.tsv"
    if [[ $name == silent ]]; then
        [[ ! -s "$artifact/compiler-info.stdout" && ! -s "$artifact/compiler-info.stderr" ]]
    else
        grep -Fq "$compiler" "$artifact/compiler-info.stderr"
    fi
}

# 复用无输出且返回 37 的已有 C 夹具，另验证启动失败不会被归一化为 1。
expect_failure silent "$work/exit37" 37
expect_failure missing "$work/missing-compiler" 127
printf 'hcl gate diagnostics: PASS\n'
