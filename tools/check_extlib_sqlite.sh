#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"
tmp_root=$(realpath -m -- "${TMPDIR:-/tmp}")
work=$(mktemp -d "$tmp_root/xlang-sqlite-XXXXXXXX")
phase=setup
cc=${CC:-gcc}
pkg=release/extlibs/sqlite
xl=./release/xl
input_paths() {
    { find "$pkg" release/include -type f -print0
      printf '%s\0' lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so release/libxlc.so \
          test/support/extlib_sqlite_runtime.c tools/check_extlib_sqlite.sh \
          test/cases/extlib_sqlite.xl release/examples/extlibs/sqlite.xl \
          release/examples/extlibs/sqlite_basic.xl release/examples/extlibs/sqlite_basic.expected.txt
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
    # 成功可清理大体积产物，但必须留下逐阶段状态与输入身份。
    if [[ -f $work/phases.tsv ]]; then cat "$work/phases.tsv" >> "$work.report.txt"; fi
    cat "$work/inputs.before.sha256" >> "$work.report.txt"
    if ((result == 0)); then
        case "$work" in "$tmp_root"/xlang-sqlite-*) rm -rf -- "$work" || result=$? ;; esac
    else printf '[sqlite] FAIL phase=%s exit=%s artifacts=%s\n' "$phase" "$result" "$work" >&2; fi
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
    printf '[sqlite] %s\n' "$phase"
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
    # 只统一文本行尾，不能删除正文中的独立CR。
    sed 's/\r$//' "$1" > "$work/expected.txt"
    diff -u "$work/expected.txt" "$2"
}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Ilib/xrt -fsanitize=address,undefined
       -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
strict=(-Wall -Wextra -Werror)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
run compiler-info 120 "$cc" --version
run vendor-compile 240 "$cc" "${flags[@]}" -c "$pkg/third_party/sqlite3.c" -o "$work/sqlite3.o"
run native-compile 120 "$cc" "${flags[@]}" "${strict[@]}" -Wmissing-prototypes -c "$pkg/sqlite_native.c" -o "$work/native.o"
run runtime-compile 120 "$cc" "${flags[@]}" "${strict[@]}" -c test/support/extlib_sqlite_runtime.c -o "$work/runtime.o"
objects=("$work/sqlite3.o" "$work/native.o" "$work/runtime.o")
for name in test_sqlite test_sqlite_alloc test_sqlite_threads test_sqlite_error_priority; do
    test_flags=()
    if [[ $name == test_sqlite_threads ]]; then test_flags=(-DNDEBUG); fi
    run "$name-compile" 120 "$cc" "${flags[@]}" "${strict[@]}" "${test_flags[@]}" "$pkg/test/$name.c" "${objects[@]}" -lm -ldl -o "$work/$name"
    run "$name-run" 120 "$work/$name"
done
# 分配器回调必须活到最后一块内存释放，钩子测试只用于独立原生进程。
for name in test_sqlite test_sqlite_threads; do
    run "$name-direct" 120 "$xl" run "$pkg/test/$name.c" "$pkg/sqlite_native.c" "$pkg/third_party/sqlite3.c" --inc-path lib/xrt
done
run basic-c-compile 120 "$cc" "${flags[@]}" "${strict[@]}" -DNDEBUG "$pkg/examples/basic.c" "${objects[@]}" -lm -ldl -o "$work/basic"
run basic-c-run 90 "$work/basic"
golden release/examples/extlibs/sqlite_basic.expected.txt "$work/basic-c-run.stdout"
run basic-c-direct 90 "$xl" run "$pkg/examples/basic.c"
golden release/examples/extlibs/sqlite_basic.expected.txt "$work/basic-c-direct.stdout"

for source in test/cases/extlib_sqlite.xl "$pkg/test/boundaries.xl" \
    "$pkg/test/file.xl" "$pkg/test/nul_inputs.xl" "$pkg/test/owned_results.xl"; do
    name=$(basename "$source" .xl)
    run "$name-jit" 90 "$xl" run "$source"
    run "$name-build" 120 "$xl" build "$source" -o "$work/$name"
    run "$name-aot" 45 "$work/$name"
    diff -u "$work/$name-jit.stdout" "$work/$name-aot.stdout"
    run "$name-cgen" 90 "$xl" cgen "$source" --emit "$work/module-$name.c"
done
# 真实生成声明在LP64下对正式头检查，常量也按当前vendor验证。
grep -hE '^extern .*xsqlite[a-zA-Z0-9_]*\(' "$work"/module-*.c | sort -u > "$work/decls.txt"
grep -oE 'extern "xsqlite[a-zA-Z0-9_]+"' "$pkg/src/sqlite.xl" | cut -d '"' -f 2 | sort -u > "$work/expected-symbols.txt"
grep -oE 'xsqlite[a-zA-Z0-9_]*\(' "$work/decls.txt" | tr -d '(' | sort -u > "$work/actual-symbols.txt"
diff -u "$work/expected-symbols.txt" "$work/actual-symbols.txt"
[[ $(wc -l < "$work/decls.txt") == 44 && $(wc -l < "$work/expected-symbols.txt") == 44 ]]
awk '/^export const int SQLITE_/ {printf "_Static_assert(%s == %s, \"%s\");\n", $4, $6, $4}' "$pkg/src/sqlite.xl" > "$work/constants.txt"
[[ $(wc -l < "$work/constants.txt") == 45 ]]
printf '#include "sqlite_native.h"\n#include "sqlite3.h"\n' > "$work/abi.c"
cat "$work/decls.txt" "$work/constants.txt" >> "$work/abi.c"
run generated-abi 120 "$cc" "${flags[@]}" "${strict[@]}" -I"$pkg/include" -I"$pkg/third_party" -fsyntax-only "$work/abi.c"

for name in sqlite sqlite_basic; do
    source="release/examples/extlibs/$name.xl"
    run "$name-example-jit" 90 "$xl" run "$source"
    run "$name-example-build" 120 "$xl" build "$source" -o "$work/$name-example"
    run "$name-example-aot" 45 "$work/$name-example"
    diff -u "$work/$name-example-jit.stdout" "$work/$name-example-aot.stdout"
done
golden release/examples/extlibs/sqlite_basic.expected.txt "$work/sqlite_basic-example-aot.stdout"
run basic-c-aot-build 120 "$xl" build "$pkg/examples/basic.c" -o "$work/basic_c_aot"
run basic-c-aot 45 "$work/basic_c_aot"
golden release/examples/extlibs/sqlite_basic.expected.txt "$work/basic-c-aot.stdout"

for name in text_bytes column_storage transactions; do
    run "$name-c-compile" 120 "$cc" "${flags[@]}" "${strict[@]}" -DNDEBUG "$pkg/examples/${name}_c.c" "${objects[@]}" -lm -ldl -o "$work/$name-gcc"
    run "$name-c-run" 90 "$work/$name-gcc"
    case "$name" in
        text_bytes) printf 'TEXT: 3 bytes\nBLOB: 3 bytes\nmiddle byte: 0\n' ;;
        column_storage) printf 'text: 你好\ninteger: 9223372036854775807\nutf8 bytes: 6\nzero blob: 4096\n' ;;
        transactions) sed 's/\r$//' "$pkg/examples/transactions.expected.txt" ;;
    esac > "$work/$name.expected"
    golden "$work/$name.expected" "$work/$name-c-run.stdout"
    for language in c xl; do
        source="$pkg/examples/$name.xl"
        if [[ $language == c ]]; then source="$pkg/examples/${name}_c.c"; fi
        run "$name-$language-jit" 90 "$xl" run "$source"
        run "$name-$language-build" 120 "$xl" build "$source" -o "$work/$name-$language"
        run "$name-$language-aot" 45 "$work/$name-$language"
        golden "$work/$name.expected" "$work/$name-$language-jit.stdout"
        golden "$work/$name.expected" "$work/$name-$language-aot.stdout"
    done
done
printf 'import sqlite\nsqlite.SQLITE_OK = 1\n' > "$work/const-write.xl"
status=0
run constant-write-rejected 30 "$xl" check "$work/const-write.xl" || status=$?
[[ $status == 1 ]]
cat "$work/constant-write-rejected.stdout" "$work/constant-write-rejected.stderr" > "$work/const-write.txt"
grep -iq const "$work/const-write.txt"
phase=summary
printf 'SQLite Linux C ASan/UBSan and bilingual AOT gate: PASS\n'
