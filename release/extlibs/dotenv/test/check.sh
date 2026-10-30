#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-dotenv-XXXXXX")
input_paths() {
    find release/extlibs/dotenv release/include -type f -print0
    printf '%s\0' lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so release/libxlc.so \
        tools/check_extlib_native_globals.ps1 tools/check_extlib_native_globals.sh \
        test/cases/extlib_dotenv.xl test/data/dotenv_test.env release/examples/extlibs/dotenv.xl
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
    if ((result == 0)); then rm -rf -- "$work"
    else printf '[dotenv] FAIL exit=%s artifacts=%s\n' "$result" "$work" >&2; fi
    exit "$result"
}
trap finish_gate EXIT
trap 'printf "exit=%s\nline=%s\ncommand=%s\n" "$?" "$LINENO" "$BASH_COMMAND" > "$work/failure.txt"' ERR
mapfile -d '' inputs < <(input_paths | sort -zu)
sha256sum "${inputs[@]}" > "$work/inputs.before.sha256"
cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror
       -DXRT_MODULE_ENVIRONMENT -DXRT_MODULE_FILE_WHOLE
       -DXRT_MODULE_DIR_TEMP
       -DXRT_MODULE_MAP -DXRT_MODULE_STRING -DXRT_MODULE_UNICODE
       -DXRT_MODULE_MEMORY_DEBUG
       -Ilib/xrt -fsanitize=address,undefined
       -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

"$cc" --version > "$work/compiler.txt"
"$cc" "${flags[@]}" -c release/extlibs/dotenv/test/xrt_runtime.c -o "$work/runtime.o"
"$cc" "${flags[@]}" -c release/extlibs/dotenv/xrt_dotenv.c -o "$work/core.o"
"$cc" "${flags[@]}" -c release/extlibs/dotenv/dotenv_native.c -o "$work/native.o"
"$cc" "${flags[@]}" release/extlibs/dotenv/test/test_dotenv.c \
    "$work/core.o" "$work/runtime.o" -lm -ldl -o "$work/test_dotenv"
timeout 90s "$work/test_dotenv"
"$cc" "${flags[@]}" release/extlibs/dotenv/examples/basic.c \
    "$work/runtime.o" -lm -ldl -o "$work/example"
timeout 30s "$work/example" > "$work/example.txt"
# Windows 工作树可能把文本夹具检出为 CRLF；Linux 输出统一按 LF 核对。
sed 's/\r$//' release/extlibs/dotenv/examples/basic.expected.txt > "$work/expected.txt"
diff -u "$work/expected.txt" "$work/example.txt"
"$cc" "${flags[@]}" release/extlibs/dotenv/examples/reload_c.c "$work/runtime.o" -lm -ldl -o "$work/reload"
timeout 30s "$work/reload" > "$work/reload-c.txt"
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
timeout 90s "$xl" run release/extlibs/dotenv/examples/basic.c > "$work/example_direct.txt"
diff -u "$work/example.txt" "$work/example_direct.txt"
for extension in c xl; do
    source="release/extlibs/dotenv/examples/reload.$extension"
    if [[ $extension == c ]]; then source=release/extlibs/dotenv/examples/reload_c.c; fi
    timeout 90s "$xl" run "$source" > "$work/reload-$extension-jit.txt"
    diff -u "$work/reload.expected.txt" "$work/reload-$extension-jit.txt"
done
timeout 90s "$xl" run test/cases/extlib_dotenv.xl
timeout 90s "$xl" run release/extlibs/dotenv/test/limits.xl
timeout 90s "$xl" run release/extlibs/dotenv/test/apply_oom.xl
timeout 90s "$xl" run release/extlibs/dotenv/test/create_oom.xl
timeout 90s "$xl" run release/extlibs/dotenv/test/replace.xl
if timeout 90s "$xl" check release/extlibs/dotenv/test/private_native.xl > "$work/private_native.txt" 2>&1; then
    printf 'dotenv: Native methods must not be public\n' >&2
    exit 1
fi
if ! grep -Fq 'parseNative' "$work/private_native.txt" ||
   ! grep -Fq 'access' "$work/private_native.txt"; then
    cat "$work/private_native.txt" >&2
    printf 'dotenv: expected private-access diagnostic\n' >&2
    exit 1
fi
timeout 90s "$xl" run release/examples/extlibs/dotenv.xl > "$work/example_xl.txt"
diff -u "$work/expected.txt" "$work/example_xl.txt"
diff -u "$work/example.txt" "$work/example_xl.txt"
# 把真实生成声明与原生定义放在同一只编译单元，逐原型核对而非仅比名称。
timeout 90s "$xl" cgen test/cases/extlib_dotenv.xl --emit "$work/abi-core.c"
timeout 90s "$xl" cgen release/extlibs/dotenv/examples/reload.xl --emit "$work/abi-reload.c"
grep -hE '^extern .*xdotenv_[a-z_]+[(]' "$work/abi-core.c" "$work/abi-reload.c" | sort -u > "$work/abi.decls"
grep -oE '"xdotenv_[a-z_]+"' release/extlibs/dotenv/src/dotenv.xl | tr -d '"' | sort -u > "$work/abi.expected"
grep -oE 'xdotenv_[a-z_]+[(]' "$work/abi.decls" | tr -d '(' | sort -u > "$work/abi.actual"
diff -u "$work/abi.expected" "$work/abi.actual"
printf '#include "dotenv_native.c"\n' > "$work/abi.c"
cat "$work/abi.decls" >> "$work/abi.c"
"$cc" "${flags[@]}" -Irelease/extlibs/dotenv -fsyntax-only "$work/abi.c"
timeout 120s "$xl" build test/cases/extlib_dotenv.xl -o "$work/dotenv_core"
timeout 90s "$work/dotenv_core"
for name in limits apply_oom create_oom replace; do
    timeout 120s "$xl" build "release/extlibs/dotenv/test/$name.xl" \
        -o "$work/dotenv_$name"
    timeout 90s "$work/dotenv_$name"
done
timeout 120s "$xl" build release/examples/extlibs/dotenv.xl \
    -o "$work/dotenv_example"
timeout 45s "$work/dotenv_example" > "$work/example_aot.txt"
diff -u "$work/example.txt" "$work/example_aot.txt"
timeout 120s "$xl" build release/extlibs/dotenv/examples/basic.c -o "$work/basic-c-aot"
timeout 45s "$work/basic-c-aot" > "$work/basic-c-aot.txt"
diff -u "$work/example.txt" "$work/basic-c-aot.txt"
for extension in c xl; do
    source="release/extlibs/dotenv/examples/reload.$extension"
    if [[ $extension == c ]]; then source=release/extlibs/dotenv/examples/reload_c.c; fi
    timeout 120s "$xl" build "$source" -o "$work/reload-$extension"
    timeout 45s "$work/reload-$extension" > "$work/reload-$extension-aot.txt"
    diff -u "$work/reload.expected.txt" "$work/reload-$extension-aot.txt"
done
bash tools/check_extlib_native_globals.sh dotenv
printf 'dotenv Linux C ASan/UBSan, XLang JIT/AOT and parity gate: PASS\n'
