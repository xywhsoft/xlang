#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-sysinfo-XXXXXX")
cleanup() {
    result=$?
    trap - EXIT
    set +e
    cd "$root" || exit 125
    mapfile -d '' after < <(input_paths | sort -zu)
    sha256sum "${after[@]}" > "$work/inputs.after.sha256"
    if ! diff -u "$work/inputs.before.sha256" "$work/inputs.after.sha256"; then result=125; fi
    printf 'exit=%s\n' "$result" > "$work.report.txt"
    cp -- "$work/inputs.before.sha256" "$work.inputs.sha256"
    if ((result == 0)); then rm -rf -- "$work" || result=$?
    else printf '[sysinfo] FAIL exit=%s artifacts=%s\n' "$result" "$work" >&2; fi
    printf '[sysinfo] report: %s.report.txt\n' "$work"
    exit "$result"
}
trap cleanup EXIT
record_failure() {
    printf 'exit=%s\nline=%s\ncommand=%s\npipeline=%s\n' "$@" > "$work/failure.txt"
    cat "$work/failure.txt" >&2
}
trap 'record_failure "$?" "$LINENO" "$BASH_COMMAND" "${PIPESTATUS[*]}"' ERR
cc=${CC:-gcc}
cc_path=$(command -v "$cc" || true)
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt
       -Irelease/extlibs/sysinfo -fsanitize=address,undefined
       -fno-omit-frame-pointer -fno-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
export XLANG_PACKAGE_PATH="$root/release/extlibs"
input_paths() {
    { find release/extlibs/sysinfo release/include -type f -print0
      printf '%s\0' lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so \
          release/libxlc.so test/cases/extlib_sysinfo.xl release/examples/extlibs/sysinfo.xl
      if [[ -n "$cc_path" ]]; then printf '%s\0' "$cc_path"; fi
    }
}
mapfile -d '' inputs < <(input_paths | sort -zu)
sha256sum "${inputs[@]}" > "$work/inputs.before.sha256"
"$cc" --version > "$work/compiler.txt"
compile() { timeout 180s "$cc" "${flags[@]}" "$@"; }

compile -DXRT_MODULE_UNICODE -DXRT_MODULE_NUMBER_INTEGER \
    -fsyntax-only release/extlibs/sysinfo/sysinfo_native.c
compile -fsyntax-only release/extlibs/sysinfo/test/abi_contract.c
timeout 90s ./release/xl cgen release/extlibs/sysinfo/test/boundaries.xl --emit "$work/owned_text.c"
grep -E '^extern .*xsysinfo_[a-z0-9_]+[(]' "$work/owned_text.c" | sort -u > "$work/abi.decls"
grep -oE '"xsysinfo_[a-z0-9_]+"' release/extlibs/sysinfo/src/sysinfo.xl | tr -d '"' | sort -u > "$work/abi.expected"
grep -oE 'xsysinfo_[a-z0-9_]+[(]' "$work/abi.decls" | tr -d '(' | sort -u > "$work/abi.actual"
test "$(wc -l < "$work/abi.decls")" -eq 11
diff -u "$work/abi.expected" "$work/abi.actual"
printf '#include "sysinfo_native.h"\n' > "$work/abi.c"
cat "$work/abi.decls" >> "$work/abi.c"
compile -fsyntax-only "$work/abi.c"
for name in os_name os_version arch hostname cpu_name; do
    grep -Eq "[.]Data[[:space:]]*=[[:space:]]*xsysinfo_$name[[:space:]]*[(]" "$work/owned_text.c"
done
if grep -Eq 'xl6_xrt_take_cstr\(|xl6CStringCopyV1' "$work/owned_text.c"; then
    printf 'sysinfo must directly adopt owned strings\n' >&2
    exit 1
fi

# 与清单相同的模块加测试诊断，公共对象只编译一次。
compile -c release/extlibs/sysinfo/test/xrt_runtime.c -o "$work/xrt.o"
compile -c release/extlibs/sysinfo/sysinfo_native.c -o "$work/native.o"
objects=("$work/native.o" "$work/xrt.o")
compile release/extlibs/sysinfo/test/test_sysinfo.c "${objects[@]}" -no-pie -lm -ldl -o "$work/test_sysinfo"
timeout 90s "$work/test_sysinfo"
compile release/extlibs/sysinfo/test/non_utf8_system_text.c "${objects[@]}" \
    -Wl,--wrap=gethostname -Wl,--wrap=__gethostname_chk -Wl,--wrap=uname -Wl,--wrap=fopen \
    -no-pie -lm -ldl -o "$work/non_utf8_system_text"
timeout 90s "$work/non_utf8_system_text"
for name in long_cpu_name meminfo_unavailable meminfo_malformed uptime_overflow; do
    compile "release/extlibs/sysinfo/test/$name.c" "${objects[@]}" \
        -Wl,--wrap=fopen -no-pie -lm -ldl -o "$work/$name"
    timeout 90s "$work/$name"
done
compile release/extlibs/sysinfo/examples/sysinfo_c.c "$work/xrt.o" -no-pie -lm -ldl -o "$work/example"
timeout 30s "$work/example" > "$work/c.txt"
compile release/extlibs/sysinfo/examples/metrics_c.c "$work/xrt.o" -no-pie -lm -ldl -o "$work/metrics"
timeout 30s "$work/metrics" > "$work/metrics_c.txt"

check_metrics() {
    test "$(wc -l < "$1")" -eq 3
    grep -Eq '^available: [0-9]+$' "$1"
    grep -Eq '^usage: ([0-9]|[1-9][0-9]|100)$' "$1"
    grep -Eq '^uptime: [0-9]+$' "$1"
}
for name in boundaries allocations; do
    timeout 90s ./release/xl run "release/extlibs/sysinfo/test/$name.xl"
done
timeout 90s ./release/xl run test/cases/extlib_sysinfo.xl
timeout 90s ./release/xl run release/examples/extlibs/sysinfo.xl
timeout 90s ./release/xl run release/extlibs/sysinfo/examples/basic.xl > "$work/xl.txt"
diff -u "$work/c.txt" "$work/xl.txt"
timeout 90s ./release/xl run release/extlibs/sysinfo/examples/sysinfo_c.c > "$work/c_direct.txt"
diff -u "$work/c.txt" "$work/c_direct.txt"
timeout 90s ./release/xl run release/extlibs/sysinfo/examples/metrics.xl > "$work/metrics_xl.txt"
timeout 90s ./release/xl run release/extlibs/sysinfo/examples/metrics_c.c > "$work/metrics_c_direct.txt"
for output in "$work/metrics_c.txt" "$work/metrics_xl.txt" "$work/metrics_c_direct.txt"; do check_metrics "$output"; done

for name in boundaries allocations; do
    timeout 180s ./release/xl build "release/extlibs/sysinfo/test/$name.xl" -o "$work/sysinfo_$name"
    timeout 90s "$work/sysinfo_$name" > "$work/$name.aot.txt"
done
grep -Fxq 'sysinfo allocations: PASS' "$work/allocations.aot.txt"
timeout 180s ./release/xl build release/extlibs/sysinfo/examples/basic.xl -o "$work/sysinfo_basic"
timeout 90s "$work/sysinfo_basic" > "$work/basic.aot.txt"
diff -u "$work/c.txt" "$work/basic.aot.txt"
timeout 180s ./release/xl build release/extlibs/sysinfo/examples/metrics.xl -o "$work/sysinfo_metrics"
timeout 90s "$work/sysinfo_metrics" > "$work/metrics_aot.txt"
check_metrics "$work/metrics_aot.txt"
timeout 180s ./release/xl build release/extlibs/sysinfo/examples/sysinfo_c.c -o "$work/sysinfo_c_aot"
timeout 90s "$work/sysinfo_c_aot" > "$work/c_aot.txt"
diff -u "$work/c.txt" "$work/c_aot.txt"
timeout 180s ./release/xl build release/extlibs/sysinfo/examples/metrics_c.c -o "$work/sysinfo_metrics_c_aot"
timeout 90s "$work/sysinfo_metrics_c_aot" > "$work/metrics_c_aot.txt"
check_metrics "$work/metrics_c_aot.txt"
for source in release/examples/extlibs/sysinfo.xl test/cases/extlib_sysinfo.xl; do
    target="$work/$(basename "${source%.*}")"
    timeout 180s ./release/xl build "$source" -o "$target"
    timeout 90s "$target"
done
printf 'sysinfo Linux C ASan/UBSan, eleven ABI prototypes, XLang JIT/AOT and parity gate: PASS\n'
