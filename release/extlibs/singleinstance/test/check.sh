#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-singleinstance-XXXXXXXX")
pkg=release/extlibs/singleinstance
cc=${CC:-gcc};cc_path=$(command -v "$cc");cc1=$("$cc" -print-prog-name=cc1)
inputs() {
    find "$pkg" release/include -type f -print0
    printf '%s\0' lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so release/libxlc.so \
        test/cases/extlib_singleinstance.xl release/examples/extlibs/singleinstance.xl "$cc_path" "$cc1" /usr/bin/bash
}
mapfile -d '' paths < <(inputs|sort -zu)
sha256sum "${paths[@]}" > "$work/inputs.before.sha256"
finish() {
    local result=$?;trap - EXIT;set +e
    mapfile -d '' after < <(inputs|sort -zu)
    sha256sum "${after[@]}" > "$work/inputs.after.sha256"
    if ! diff -u "$work/inputs.before.sha256" "$work/inputs.after.sha256" > "$work/input-changes.txt";then result=125;fi
    printf '%s\n' "$result" > "$work/exit.txt"
    if ((result==0));then rm -rf -- "$work";else printf '[singleinstance] FAIL exit=%s artifacts=%s\n' "$result" "$work" >&2;fi
    exit "$result"
}
trap finish EXIT
run() {
    local label=$1 actual=0;shift
    printf '%q ' "$@" > "$work/$label.command";printf '\n' >> "$work/$label.command"
    timeout 120s "$@" > "$work/$label.stdout" 2> "$work/$label.stderr" || actual=$?
    printf '%s\t0\t%s\n' "$label" "$actual" >> "$work/phases.tsv"
    cat "$work/$label.stdout";cat "$work/$label.stderr" >&2
    printf '[singleinstance] %s exit=%s required=0\n' "$label" "$actual"
    ((actual==0))
}
mkdir "$work/home";chmod 0700 "$work/home"
export HOME="$work/home" XLANG_SI_TEST_HOME="$work/home" XLANG_PACKAGE_PATH="$root/release/extlibs"
flags=(-std=c11 -D_DEFAULT_SOURCE -DNDEBUG -O1 -g -Wall -Wextra -Werror -Ilib/xrt -I"$pkg"
    -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
links=(-ldl -lm)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
run compiler "$cc" --version
run runtime "$cc" "${flags[@]}" -c "$pkg/test/runtime.c" -o "$work/runtime.o"
run native "$cc" "${flags[@]}" -Wmissing-prototypes -c "$pkg/singleinstance_native.c" -o "$work/native.o"
run core-compile "$cc" "${flags[@]}" "$pkg/test/test_singleinstance.c" "$work/native.o" "$work/runtime.o" "${links[@]}" -o "$work/core"
run core-real-process "$work/core"
grep -q 'cross-process: real child' "$work/core-real-process.stdout"
run core-build ./release/xl build "$pkg/test/test_singleinstance.c" "$pkg/singleinstance_native.c" --inc-path lib/xrt -o "$work/core.aot"
run core-AOT "$work/core.aot"
grep -q 'cross-process: real child' "$work/core-AOT.stdout"
run cleanup-compile "$cc" "${flags[@]}" "$pkg/test/cleanup_contract_c.c" "$work/runtime.o" "${links[@]}" -o "$work/cleanup"
run cleanup-GCC "$work/cleanup"
run cleanup-TCC ./release/xl run "$pkg/test/cleanup_contract_c.c"
run cleanup-build ./release/xl build "$pkg/test/cleanup_contract_c.c" -o "$work/cleanup.aot"
run cleanup-AOT "$work/cleanup.aot"
diff -u "$work/cleanup-GCC.stdout" "$work/cleanup-TCC.stdout"
diff -u "$work/cleanup-GCC.stdout" "$work/cleanup-AOT.stdout"
# 宽权限与符号链接拒绝保持为真实文件系统场景。
mkdir -p "$work/unsafe-home/.xlang/singleinstance"
chmod 0700 "$work/unsafe-home" "$work/unsafe-home/.xlang";chmod 0777 "$work/unsafe-home/.xlang/singleinstance"
run unsafe-leaf env HOME="$work/unsafe-home" "$work/core" --unsafe-dir
chmod 0700 "$work/unsafe-home/.xlang/singleinstance";chmod 0777 "$work/unsafe-home/.xlang"
run unsafe-parent env HOME="$work/unsafe-home" "$work/core" --unsafe-dir
mkdir -p "$work/link-home" "$work/link-target/singleinstance"
chmod 0700 "$work/link-home" "$work/link-target" "$work/link-target/singleinstance"
ln -s "$work/link-target" "$work/link-home/.xlang"
run unsafe-symlink env HOME="$work/link-home" "$work/core" --unsafe-dir
for instance in a b;do
    run "module-$instance" "$cc" "${flags[@]}" "-Dxsi_acquire=xsi_${instance}_acquire" "-Dxsi_release=xsi_${instance}_release" \
        "-Dxsi_is_acquired=xsi_${instance}_is_acquired" -c "$pkg/singleinstance_native.c" -o "$work/$instance.o"
done
run modules-compile "$cc" "${flags[@]}" "$pkg/test/test_multiple_modules.c" "$work/a.o" "$work/b.o" "$work/runtime.o" "${links[@]}" -o "$work/modules"
run modules "$work/modules"
for source in "$pkg/examples/singleinstance_c.c" "$pkg/examples/recovery_c.c";do
    name=$(basename "$source" .c);expected="$pkg/test/singleinstance.expected.txt"
    if [[ $name == recovery_c ]];then expected="$pkg/examples/recovery.expected.txt";fi
    run "$name-compile" "$cc" "${flags[@]}" "$source" "$work/native.o" "$work/runtime.o" "${links[@]}" -o "$work/$name"
    run "$name-GCC" "$work/$name"
    run "$name-TCC" ./release/xl run "$source"
    run "$name-build" ./release/xl build "$source" -o "$work/$name.aot"
    run "$name-AOT" "$work/$name.aot"
    for engine in GCC TCC AOT;do diff -u <(sed 's/\r$//' "$expected") "$work/$name-$engine.stdout";done
done
for source in test/cases/extlib_singleinstance.xl "$pkg/test/boundaries.xl" release/examples/extlibs/singleinstance.xl "$pkg/examples/recovery.xl";do
    name=$(basename "$source" .xl)
    run "$name-JIT" ./release/xl run "$source"
    run "$name-build" ./release/xl build "$source" -o "$work/$name.aot"
    run "$name-AOT" "$work/$name.aot"
    diff -u "$work/$name-JIT.stdout" "$work/$name-AOT.stdout"
    if [[ $name == recovery ]];then diff -u <(sed 's/\r$//' "$pkg/examples/recovery.expected.txt") "$work/$name-JIT.stdout";fi
    if [[ $name == singleinstance ]];then diff -u <(sed 's/\r$//' "$pkg/test/singleinstance.expected.txt") "$work/$name-JIT.stdout";fi
done
run cgen ./release/xl cgen "$pkg/test/boundaries.xl" --emit "$work/module.c"
grep -E '^extern .*xsi_[a-z_]+[(]' "$work/module.c"|sort -u > "$work/decls.txt"
[[ $(wc -l < "$work/decls.txt") == 3 ]]
grep -oE 'extern "xsi_[a-z_]+"' "$pkg/src/singleinstance.xl"|cut -d '"' -f 2|sort -u > "$work/expected.txt"
grep -oE 'xsi_[a-z_]+' "$work/decls.txt"|sort -u > "$work/actual.txt"
diff -u "$work/expected.txt" "$work/actual.txt"
printf '#include "include/singleinstance_native.h"\n' > "$work/abi.c";cat "$work/decls.txt" >> "$work/abi.c"
run generated-ABI "$cc" "${flags[@]}" -fsyntax-only "$work/abi.c"
run contexts-compile "$cc" -std=c11 -O2 -Wall -Wextra -Werror -Irelease/include -Ilib/xrt "$pkg/test/test_contexts.c" \
    -Lrelease -lxl "-Wl,-rpath,$root/release" -pthread -o "$work/context"
run real-contexts "$work/context" "$root/release" "$root/$pkg/test/context_hold.xl" "$root/$pkg/test/context_probe.xl" \
    "$root/$pkg/test/context_after.xl" "$work/ready" "$work/done"
printf 'singleinstance Linux sanitizer, process, module, ABI and real context gate: PASS\n'
