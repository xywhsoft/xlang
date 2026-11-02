#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-daemon-XXXXXXXX")
cc=${CC:-gcc}
cc_path=$(command -v "$cc")
cc1=$("$cc" -print-prog-name=cc1)
input_paths() {
    find release/extlibs/daemon release/include -type f -print0
    printf '%s\0' "$cc_path" "$cc1" lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so release/libxlc.so \
        test/cases/extlib_daemon.xl release/examples/extlibs/daemon.xl release/examples/extlibs/daemon_c.c
}
mapfile -d '' inputs < <(input_paths | sort -zu)
sha256sum "${inputs[@]}" > "$work/inputs.before.sha256"
finish() {
    local result=$?; trap - EXIT; set +e
    mapfile -d '' after < <(input_paths | sort -zu)
    sha256sum "${after[@]}" > "$work/inputs.after.sha256"
    if ! diff -u "$work/inputs.before.sha256" "$work/inputs.after.sha256" > "$work/input-changes.txt"; then result=125; fi
    printf '%s\n' "$result" > "$work/exit.txt"
    if ((result == 0)); then rm -rf -- "$work"
    else printf '[daemon] FAIL exit=%s artifacts=%s\n' "$result" "$work" >&2; fi
    exit "$result"
}
trap finish EXIT
run() {
    local label=$1 expected=$2 actual=0; shift 2
    printf '%q ' "$@" > "$work/$label.command"; printf '\n' >> "$work/$label.command"
    timeout 120s "$@" > "$work/$label.stdout" 2> "$work/$label.stderr" || actual=$?
    printf '%s\t%s\t%s\n' "$label" "$expected" "$actual" >> "$work/phases.tsv"
    cat "$work/$label.stdout"; cat "$work/$label.stderr" >&2
    printf '[daemon] %s exit=%s expected=%s\n' "$label" "$actual" "$expected"
    ((actual == expected))
}
golden() { diff -u <(sed 's/\r$//' "$1") <(sed -E 's/pid=[0-9]+/pid=<pid>/g;s/\r$//' "$2"); }
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt -Irelease/extlibs/daemon/include
    -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
pkg=release/extlibs/daemon
run compiler 0 "$cc" --version
run manifest-modules 0 "$cc" -std=c11 -Wall -Wextra -Werror -fsyntax-only -Ilib/xrt -I"$pkg/include" \
    -DXRT_MODULE_ATOMIC -DXRT_MODULE_FILE_LOCK -DXRT_MODULE_PATH -DXRT_MODULE_STRING_FORMAT -DXRT_MODULE_UNICODE "$pkg/daemon_native.c"
run runtime 0 "$cc" "${flags[@]}" -c "$pkg/test/xrt_runtime.c" -o "$work/runtime.o"
run native 0 "$cc" "${flags[@]}" -Wmissing-prototypes -c "$pkg/daemon_native.c" -o "$work/native.o"
run core-compile 0 "$cc" "${flags[@]}" "$pkg/test/test_daemon.c" "$work/native.o" "$work/runtime.o" -lm -ldl -o "$work/core"
run core 0 "$work/core"
for name in cleanup_contract_c pid_liveness_c pid_identity_c; do
    source="$pkg/test/$name.c"
    run "$name-compile" 0 "$cc" "${flags[@]}" "$source" "$work/runtime.o" -lm -ldl -o "$work/$name"
    run "$name-GCC" 0 "$work/$name"
    run "$name-TCC" 0 ./release/xl run "$source"
    run "$name-build" 0 ./release/xl build "$source" -o "$work/$name.aot"
    run "$name-AOT" 0 "$work/$name.aot"
    diff -u "$work/$name-GCC.stdout" "$work/$name-TCC.stdout"
    diff -u "$work/$name-GCC.stdout" "$work/$name-AOT.stdout"
done
for source in test/cases/extlib_daemon.xl "$pkg/test/ffi_boundaries.xl"; do
    name=$(basename "$source" .xl)
    run "$name-JIT" 0 ./release/xl run "$source"
    run "$name-build" 0 ./release/xl build "$source" -o "$work/$name"
    run "$name-AOT" 0 "$work/$name"
    diff -u "$work/$name-JIT.stdout" "$work/$name-AOT.stdout"
done
run cgen 0 ./release/xl cgen test/cases/extlib_daemon.xl --emit "$work/module.c"
grep -E '^extern .*xdaemon_[a-z_]+[(]' "$work/module.c" | sort -u > "$work/decls.txt"
grep -oE 'xdaemon_[a-z_]+' "$pkg/include/daemon_native.h" | sort -u > "$work/expected.txt"
grep -oE 'xdaemon_[a-z_]+' "$work/decls.txt" | sort -u > "$work/actual.txt"
[[ $(wc -l < "$work/decls.txt") == 11 ]]
diff -u "$work/expected.txt" "$work/actual.txt"
printf '#include "daemon_native.h"\n' > "$work/abi.c"; cat "$work/decls.txt" >> "$work/abi.c"
run generated-ABI 0 "$cc" "${flags[@]}" -fsyntax-only "$work/abi.c"
sed 's/int64_t xdaemon_is_alive/double xdaemon_is_alive/' "$work/abi.c" > "$work/abi-bad.c"
run ABI-wrong-return-control 1 "$cc" "${flags[@]}" -fsyntax-only "$work/abi-bad.c"
for name in private_native_negative private_status_negative private_getter_negative unexported_constructor_negative; do
    run "$name" 1 ./release/xl check "$pkg/test/$name.xl"
    grep -Eq '\[(sem|import)\]' "$work/$name.stderr" "$work/$name.stdout"
done
for source in release/examples/extlibs/daemon_c.c "$pkg/examples/restart_c.c"; do
    name=$(basename "$source" .c)
    expected="$pkg/test/daemon.expected.txt"
    if [[ $name == restart_c ]]; then expected="$pkg/examples/restart.expected.txt"; fi
    run "$name-compile" 0 "$cc" "${flags[@]}" "$source" "$work/native.o" "$work/runtime.o" -lm -ldl -o "$work/$name"
    run "$name-GCC" 0 "$work/$name"
    run "$name-TCC" 0 ./release/xl run "$source"
    run "$name-build" 0 ./release/xl build "$source" -o "$work/$name.aot"
    run "$name-AOT" 0 "$work/$name.aot"
    for engine in GCC TCC AOT; do golden "$expected" "$work/$name-$engine.stdout"; done
done
for source in release/examples/extlibs/daemon.xl "$pkg/examples/restart.xl"; do
    name=$(basename "$source" .xl)
    expected="$pkg/test/daemon.expected.txt"
    if [[ $name == restart ]]; then expected="$pkg/examples/restart.expected.txt"; fi
    run "$name-JIT" 0 ./release/xl run "$source"
    run "$name-build" 0 ./release/xl build "$source" -o "$work/$name.aot"
    run "$name-AOT" 0 "$work/$name.aot"
    golden "$expected" "$work/$name-JIT.stdout"; golden "$expected" "$work/$name-AOT.stdout"
done
run top-level-JIT 0 ./release/xl run "$pkg/test/top_level.xl"
run top-level-build 0 ./release/xl build "$pkg/test/top_level.xl" -o "$work/top-level"
run top-level-AOT 0 "$work/top-level"
printf 'daemon Linux C sanitizer, public ABI and bilingual JIT/AOT gate: PASS\n'
