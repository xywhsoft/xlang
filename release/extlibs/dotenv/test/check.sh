#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-dotenv-XXXXXX")
cc=${CC:-gcc}
cc_path=$(command -v "$cc")
cc1=$("$cc" -print-prog-name=cc1)
sequence=0
input_paths() {
    find release/extlibs/dotenv release/include -type f -print0
    printf '%s\0' lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so release/libxlc.so \
        tools/check_extlib_native_globals.ps1 tools/check_extlib_native_globals.sh \
        test/cases/extlib_dotenv.xl test/data/dotenv_test.env release/examples/extlibs/dotenv.xl
    printf '%s\0' "$cc_path" "$cc1" "$(command -v bash)"
}
finish_gate() {
    result=$?
    trap - EXIT
    set +e
    cd "$root" || exit 125
    mapfile -d '' after < <(input_paths | sort -zu)
    sha256sum "${after[@]}" > "$work/inputs.after.sha256"
    if ! diff -u "$work/inputs.before.sha256" "$work/inputs.after.sha256"; then result=125; fi
    printf '%s\n' "$result" > "$work/exit-code.txt"
    printf '%s\n' "$sequence" > "$work/phase-count.txt"
    if ((result == 0)); then rm -rf -- "$work"
    else printf '[dotenv] FAIL exit=%s artifacts=%s\n' "$result" "$work" >&2; fi
    exit "$result"
}
trap finish_gate EXIT
trap 'printf "exit=%s\nline=%s\ncommand=%s\n" "$?" "$LINENO" "$BASH_COMMAND" > "$work/failure.txt"' ERR
mapfile -d '' inputs < <(input_paths | sort -zu)
sha256sum "${inputs[@]}" > "$work/inputs.before.sha256"
run() {
    local result
    sequence=$((sequence+1))
    printf '\n$' >> "$work/commands.log"
    printf ' %q' "$@" >> "$work/commands.log"
    printf '\n' >> "$work/commands.log"
    if "$@" > "$work/$sequence.out" 2> "$work/$sequence.err"; then result=0; else result=$?; fi
    printf 'exit=%s\n' "$result" >> "$work/commands.log"
    cat "$work/$sequence.out" "$work/$sequence.err" >> "$work/commands.log"
    cat "$work/$sequence.out"
    cat "$work/$sequence.err" >&2
    return "$result"
}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror
       -DXRT_MODULE_ENVIRONMENT -DXRT_MODULE_FILE_WHOLE
       -DXRT_MODULE_DIR_TEMP
       -DXRT_MODULE_MAP -DXRT_MODULE_STRING -DXRT_MODULE_UNICODE
       -DXRT_MODULE_MEMORY_DEBUG
       -Ilib/xrt -fsanitize=address,undefined
       -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

compile() { run timeout 180s "$cc" "${flags[@]}" "$@"; }
run timeout 10s "$cc" --version > "$work/compiler.txt"
compile -c release/extlibs/dotenv/test/xrt_runtime.c -o "$work/runtime.o"
compile -c release/extlibs/dotenv/xrt_dotenv.c -o "$work/core.o"
compile -c release/extlibs/dotenv/dotenv_native.c -o "$work/native.o"
compile release/extlibs/dotenv/test/test_dotenv.c \
    "$work/core.o" "$work/runtime.o" -lm -ldl -o "$work/test_dotenv"
run timeout 90s "$work/test_dotenv"
compile release/extlibs/dotenv/test/grammar_c.c "$work/core.o" "$work/runtime.o" -lm -ldl -o "$work/grammar"
run timeout 90s "$work/grammar" > "$work/grammar.txt"
grep -Eq '^dotenv grammar: 3585 valid, 72 invalid, 144 fault runs, [1-9][0-9]* OOM, 0 failures$' "$work/grammar.txt"
compile release/extlibs/dotenv/examples/literal_values_c.c "$work/runtime.o" -lm -ldl -o "$work/literal"
run timeout 30s "$work/literal" > "$work/literal.txt"
sed 's/\r$//' release/extlibs/dotenv/examples/literal_values.expected.txt > "$work/literal.expected.txt"
diff -u "$work/literal.expected.txt" "$work/literal.txt"
compile release/extlibs/dotenv/examples/basic.c \
    "$work/runtime.o" -lm -ldl -o "$work/example"
run timeout 30s "$work/example" > "$work/example.txt"
# Windows 工作树可能把文本夹具检出为 CRLF；Linux 输出统一按 LF 核对。
sed 's/\r$//' release/extlibs/dotenv/examples/basic.expected.txt > "$work/expected.txt"
diff -u "$work/expected.txt" "$work/example.txt"
compile release/extlibs/dotenv/examples/reload_c.c "$work/runtime.o" -lm -ldl -o "$work/reload"
run timeout 30s "$work/reload" > "$work/reload-c.txt"
sed 's/\r$//' release/extlibs/dotenv/examples/reload.expected.txt > "$work/reload.expected.txt"
diff -u "$work/reload.expected.txt" "$work/reload-c.txt"

xl=release/xl
if [[ ! -x "$xl" ]]; then
    printf 'dotenv: Linux xl runner is missing: %s\n' "$xl" >&2
    exit 1
fi
# WSL 挂载盘上的独立 EXE 构建可能失败；语言门禁在本次临时原生目录运行。
stage="$work/stage"
mkdir -p "$stage/release/extlibs" "$stage/release/examples/extlibs" "$stage/test/cases" "$stage/test/data" \
    "$stage/tools" "$stage/lib/xrt" "$stage/lib/xruntime"
cp -a release/xl release/libxl.so release/libxlc.so release/include "$stage/release/"
cp -a release/extlibs/dotenv "$stage/release/extlibs/"
cp -a lib/xrt/xrt.h "$stage/lib/xrt/"
cp -a lib/xruntime/xruntime.h "$stage/lib/xruntime/"
cp -a tools/check_extlib_native_globals.ps1 tools/check_extlib_native_globals.sh "$stage/tools/"
cp -a release/examples/extlibs/dotenv.xl "$stage/release/examples/extlibs/"
cp -a test/cases/extlib_dotenv.xl "$stage/test/cases/"
cp -a test/data/dotenv_test.env "$stage/test/data/"
cd "$stage"
run timeout 90s "$xl" run release/extlibs/dotenv/test/grammar_c.c > "$work/grammar-direct.txt"
diff -u "$work/grammar.txt" "$work/grammar-direct.txt"
run timeout 90s "$xl" run release/extlibs/dotenv/test/grammar.xl > "$work/grammar-xl.txt"
printf 'dotenv language grammar: PASS\n' > "$work/grammar-xl.expected.txt"
diff -u "$work/grammar-xl.expected.txt" "$work/grammar-xl.txt"
for source in test/grammar_c.c test/grammar.xl examples/literal_values_c.c examples/literal_values.xl; do
    name=$(basename "${source%.*}")
    case "$source" in
        test/grammar_c.c) expected="$work/grammar.txt";;
        test/grammar.xl) expected="$work/grammar-xl.expected.txt";;
        *) expected="$work/literal.expected.txt"
           run timeout 90s "$xl" run "release/extlibs/dotenv/$source" > "$work/$name-jit.txt"
           diff -u "$expected" "$work/$name-jit.txt";;
    esac
    run timeout 180s "$xl" build "release/extlibs/dotenv/$source" -o "$work/$name-aot"
    run timeout 90s "$work/$name-aot" > "$work/$name-aot.txt"
    diff -u "$expected" "$work/$name-aot.txt"
done
run timeout 90s "$xl" run release/extlibs/dotenv/examples/basic.c > "$work/example_direct.txt"
diff -u "$work/example.txt" "$work/example_direct.txt"
for extension in c xl; do
    source="release/extlibs/dotenv/examples/reload.$extension"
    if [[ $extension == c ]]; then source=release/extlibs/dotenv/examples/reload_c.c; fi
    run timeout 90s "$xl" run "$source" > "$work/reload-$extension-jit.txt"
    diff -u "$work/reload.expected.txt" "$work/reload-$extension-jit.txt"
done
run timeout 90s "$xl" run test/cases/extlib_dotenv.xl
run timeout 90s "$xl" run release/extlibs/dotenv/test/limits.xl
run timeout 90s "$xl" run release/extlibs/dotenv/test/apply_oom.xl
run timeout 90s "$xl" run release/extlibs/dotenv/test/create_oom.xl
run timeout 90s "$xl" run release/extlibs/dotenv/test/replace.xl
if run timeout 90s "$xl" check release/extlibs/dotenv/test/private_native.xl > "$work/private_native.txt" 2>&1; then
    printf 'dotenv: Native methods must not be public\n' >&2
    exit 1
fi
if ! grep -Fq 'parseNative' "$work/private_native.txt" ||
   ! grep -Fq 'access' "$work/private_native.txt"; then
    cat "$work/private_native.txt" >&2
    printf 'dotenv: expected private-access diagnostic\n' >&2
    exit 1
fi
run timeout 90s "$xl" run release/examples/extlibs/dotenv.xl > "$work/example_xl.txt"
diff -u "$work/expected.txt" "$work/example_xl.txt"
diff -u "$work/example.txt" "$work/example_xl.txt"
# 把真实生成声明与原生定义放在同一只编译单元，逐原型核对而非仅比名称。
run timeout 90s "$xl" cgen test/cases/extlib_dotenv.xl --emit "$work/abi-core.c"
run timeout 90s "$xl" cgen release/extlibs/dotenv/examples/reload.xl --emit "$work/abi-reload.c"
grep -hE '^extern .*xdotenv_[a-z_]+[(]' "$work/abi-core.c" "$work/abi-reload.c" | sort -u > "$work/abi.decls"
grep -oE '"xdotenv_[a-z_]+"' release/extlibs/dotenv/src/dotenv.xl | tr -d '"' | sort -u > "$work/abi.expected"
grep -oE 'xdotenv_[a-z_]+[(]' "$work/abi.decls" | tr -d '(' | sort -u > "$work/abi.actual"
diff -u "$work/abi.expected" "$work/abi.actual"
printf '#include "dotenv_native.c"\n' > "$work/abi.c"
cat "$work/abi.decls" >> "$work/abi.c"
compile -Irelease/extlibs/dotenv -fsyntax-only "$work/abi.c"
run timeout 120s "$xl" build test/cases/extlib_dotenv.xl -o "$work/dotenv_core"
run timeout 90s "$work/dotenv_core"
for name in limits apply_oom create_oom replace; do
    run timeout 120s "$xl" build "release/extlibs/dotenv/test/$name.xl" \
        -o "$work/dotenv_$name"
    run timeout 90s "$work/dotenv_$name"
done
run timeout 120s "$xl" build release/examples/extlibs/dotenv.xl \
    -o "$work/dotenv_example"
run timeout 45s "$work/dotenv_example" > "$work/example_aot.txt"
diff -u "$work/example.txt" "$work/example_aot.txt"
run timeout 120s "$xl" build release/extlibs/dotenv/examples/basic.c -o "$work/basic-c-aot"
run timeout 45s "$work/basic-c-aot" > "$work/basic-c-aot.txt"
diff -u "$work/example.txt" "$work/basic-c-aot.txt"
for extension in c xl; do
    source="release/extlibs/dotenv/examples/reload.$extension"
    if [[ $extension == c ]]; then source=release/extlibs/dotenv/examples/reload_c.c; fi
    run timeout 120s "$xl" build "$source" -o "$work/reload-$extension"
    run timeout 45s "$work/reload-$extension" > "$work/reload-$extension-aot.txt"
    diff -u "$work/reload.expected.txt" "$work/reload-$extension-aot.txt"
done
run timeout 360s bash tools/check_extlib_native_globals.sh dotenv
printf 'dotenv Linux C ASan/UBSan, XLang JIT/AOT and parity gate: PASS\n'
