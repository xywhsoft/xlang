#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"
work=${XREGEX_WORK_DIR:-"$root/build/test/xregex_linux_guard"}
mkdir -p "$work"
cc=${CC:-gcc}
pkg=release/extlibs/xregex
snapshot() {
    local inputs=()
    mapfile -d '' inputs < <(find "$pkg" -type f -print0 | sort -z)
    inputs+=(lib/xrt/xrt.h test/support/extlib_xregex_runtime.c tools/check_extlib_xregex.sh
             release/examples/extlibs/xregex_basic.expected.txt)
    if [[ -n "${XREGEX_DECLARATIONS:-}" ]]; then inputs+=("$XREGEX_DECLARATIONS"); fi
    sha256sum "${inputs[@]}"
}
snapshot > "$work/inputs.before.sha256"
"$cc" --version > "$work/compiler.txt"
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt -I"$pkg/include"
       -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
modules=(-DXRT_MODULE_ERROR_FORMAT -DXRT_MODULE_IO_BUFFER -DXRT_MODULE_STRING_SPLIT
         -DXRT_MODULE_UNICODE -DXRT_MODULE_VALUE_CONTAINER)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
"$cc" "${flags[@]}" -c test/support/extlib_xregex_runtime.c -o "$work/runtime.o"
mapfile -t sources < <(python3 -c 'import json; print("\n".join(json.load(open("release/extlibs/xregex/xlang.package.json"))["artifact"]["cSources"]))')
objects=()
for source in "${sources[@]}"; do
    if [[ "$source" == xregex_native.c ]]; then continue; fi
    object="$work/native-${#objects[@]}.o"
    "$cc" "${flags[@]}" "${modules[@]}" -c "$pkg/$source" -o "$object"
    objects+=("$object")
done
"$cc" "${flags[@]}" "${modules[@]}" "$pkg/test/test_xregex_native.c" "${objects[@]}" "$work/runtime.o" -lm -ldl -o "$work/native-test"
timeout 60s "$work/native-test" 2>&1 | tee "$work/native-test.log"
"$cc" "${flags[@]}" "${modules[@]}" "$pkg/examples/basic.c" "${objects[@]}" "$work/runtime.o" -lm -ldl -o "$work/basic"
timeout 30s "$work/basic" > "$work/basic.log"
diff -u <(tr -d '\r' < release/examples/extlibs/xregex_basic.expected.txt) "$work/basic.log"
# 可选输入必须来自同版 xlang cgen，验证实际声明在 LP64 下也与 C 一致。
if [[ -n "${XREGEX_DECLARATIONS:-}" ]]; then
    sha256sum "$XREGEX_DECLARATIONS" > "$work/generated-prototypes.sha256"
    "$cc" "${flags[@]}" "${modules[@]}" -include "$pkg/xregex_native.c" -x c -fsyntax-only "$XREGEX_DECLARATIONS"
fi
snapshot > "$work/inputs.after.sha256"
diff -u "$work/inputs.before.sha256" "$work/inputs.after.sha256"
printf 'xregex Linux C ASan/UBSan gate: PASS\n'
