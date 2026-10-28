#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"
tmp_root=$(realpath -m -- "${TMPDIR:-/tmp}")
work=$(mktemp -d "$tmp_root/xlang-sqlite-XXXXXXXX")
cleanup() {
    result=$?
    trap - EXIT
    set +e
    if ((result == 0)); then
        case "$work" in
            "$tmp_root"/xlang-sqlite-*) rm -rf -- "$work" || result=$? ;;
        esac
    else
        printf '[sqlite] FAIL exit=%s artifacts=%s\n' "$result" "$work" >&2
    fi
    exit "$result"
}
trap cleanup EXIT

# 保留失败命令和管道中各进程的状态，避免无输出失败被清理掉。
record_failure() {
    printf 'exit=%s\nline=%s\ncommand=%s\npipeline=%s\n' "$@" > "$work/failure.txt"
    cat "$work/failure.txt" >&2
}
trap 'record_failure "$?" "$LINENO" "$BASH_COMMAND" "${PIPESTATUS[*]}"' ERR

cc=${CC:-gcc}
pkg=release/extlibs/sqlite
mapfile -d '' inputs < <(find "$pkg" -type f \( -name '*.c' -o -name '*.h' -o -name '*.json' -o -name '*.xl' \) -print0 | sort -z)
inputs+=(lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so release/libxlc.so
         release/include/xrt_decl.h release/include/xruntime_decl.h release/include/xl_extlib_abi.h
         test/support/extlib_sqlite_runtime.c tools/check_extlib_sqlite.sh
         test/cases/extlib_sqlite.xl release/examples/extlibs/sqlite.xl
         release/examples/extlibs/sqlite_basic.xl
         release/examples/extlibs/sqlite_basic.expected.txt)
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Ilib/xrt -fsanitize=address,undefined
       -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
strict=(-Wall -Wextra -Werror)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
sha256sum "${inputs[@]}" > "$work/inputs.before.sha256"
"$cc" --version > "$work/compiler.txt"
"$cc" "${flags[@]}" -c "$pkg/third_party/sqlite3.c" -o "$work/sqlite3.o"
"$cc" "${flags[@]}" "${strict[@]}" -c "$pkg/sqlite_native.c" -o "$work/native.o"
"$cc" "${flags[@]}" "${strict[@]}" -c test/support/extlib_sqlite_runtime.c -o "$work/runtime.o"
objects=("$work/sqlite3.o" "$work/native.o" "$work/runtime.o")
for name in test_sqlite test_sqlite_alloc test_sqlite_threads; do
    test_flags=()
    if [[ $name == test_sqlite_threads ]]; then test_flags=(-DNDEBUG); fi
    "$cc" "${flags[@]}" "${strict[@]}" "${test_flags[@]}" "$pkg/test/$name.c" "${objects[@]}" -lm -ldl -o "$work/$name"
    timeout 120s "$work/$name" 2>&1 | tee "$work/$name.log"
done
"$cc" "${flags[@]}" "${strict[@]}" -DNDEBUG "$pkg/examples/basic.c" "${objects[@]}" -lm -ldl -o "$work/basic"
timeout 30s "$work/basic" > "$work/basic.log"
diff -u release/examples/extlibs/sqlite_basic.expected.txt "$work/basic.log"
timeout 30s ./release/xl run "$pkg/examples/basic.c" > "$work/basic_direct.log"
diff -u "$work/basic.log" "$work/basic_direct.log"

xl=./release/xl
for source in test/cases/extlib_sqlite.xl "$pkg/test/boundaries.xl" \
    "$pkg/test/file.xl" "$pkg/test/nul_inputs.xl"; do
    name=$(basename "$source" .xl)
    timeout 90s "$xl" run "$source" > "$work/$name.jit.log"
    timeout 120s "$xl" build "$source" -o "$work/$name"
    timeout 30s "$work/$name" > "$work/$name.aot.log"
    diff -u "$work/$name.jit.log" "$work/$name.aot.log"
done
for name in sqlite sqlite_basic; do
    source="release/examples/extlibs/$name.xl"
    timeout 90s "$xl" run "$source" > "$work/$name.jit.log"
    timeout 120s "$xl" build "$source" -o "$work/$name"
    timeout 30s "$work/$name" > "$work/$name.aot.log"
    diff -u "$work/$name.jit.log" "$work/$name.aot.log"
done
diff -u release/examples/extlibs/sqlite_basic.expected.txt \
    "$work/sqlite_basic.aot.log"
diff -u "$work/basic.log" "$work/sqlite_basic.aot.log"
timeout 120s "$xl" build "$pkg/examples/basic.c" -o "$work/basic_c_aot"
timeout 30s "$work/basic_c_aot" > "$work/basic_c_aot.log"
diff -u "$work/basic.log" "$work/basic_c_aot.log"

"$cc" "${flags[@]}" "${strict[@]}" -DNDEBUG "$pkg/examples/text_bytes_c.c" \
    "${objects[@]}" -lm -ldl -o "$work/text_bytes_gcc"
timeout 30s "$work/text_bytes_gcc" > "$work/text_bytes_gcc.log"
diff -u <(printf 'TEXT: 3 bytes\nBLOB: 3 bytes\nmiddle byte: 0\n') "$work/text_bytes_gcc.log"
for language in c xl; do
    source="$pkg/examples/text_bytes.xl"
    if [[ $language == c ]]; then source="$pkg/examples/text_bytes_c.c"; fi
    timeout 90s "$xl" run "$source" > "$work/text_bytes_$language.jit.log"
    timeout 120s "$xl" build "$source" -o "$work/text_bytes_$language"
    timeout 30s "$work/text_bytes_$language" > "$work/text_bytes_$language.aot.log"
    diff -u "$work/text_bytes_gcc.log" "$work/text_bytes_$language.jit.log"
    diff -u "$work/text_bytes_gcc.log" "$work/text_bytes_$language.aot.log"
done

sha256sum "${inputs[@]}" > "$work/inputs.after.sha256"
diff -u "$work/inputs.before.sha256" "$work/inputs.after.sha256"
printf 'SQLite Linux C ASan/UBSan and bilingual AOT gate: PASS\n'
