#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-sharedmem-XXXXXXXX")
cc=${CC:-gcc}; cc_path=$(command -v "$cc"); cc1=$("$cc" -print-prog-name=cc1)
python=${PYTHON:-python3}; python_path=$(command -v "$python")
pkg=release/extlibs/sharedmem
issues=0
inputs() {
    find "$pkg" release/include -type f -print0
    printf '%s\0' "$cc_path" "$cc1" "$python_path" lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so release/libxlc.so \
        test/cases/extlib_sharedmem.xl release/examples/extlibs/sharedmem.xl
}
mapfile -d '' paths < <(inputs | sort -zu)
sha256sum "${paths[@]}" > "$work/inputs.before.sha256"
finish() {
    local result=$?; trap - EXIT; set +e
    mapfile -d '' after < <(inputs | sort -zu)
    sha256sum "${after[@]}" > "$work/inputs.after.sha256"
    if ! diff -u "$work/inputs.before.sha256" "$work/inputs.after.sha256" > "$work/input-changes.txt"; then result=125; fi
    printf '%s\n' "$result" > "$work/exit.txt"
    if ((result == 0)); then rm -rf -- "$work"
    else printf '[sharedmem] FAIL exit=%s artifacts=%s\n' "$result" "$work" >&2; fi
    exit "$result"
}
trap finish EXIT
run() {
    local label=$1 expected=$2 actual=0; shift 2
    printf '%q ' "$@" > "$work/$label.command"; printf '\n' >> "$work/$label.command"
    timeout 120s "$@" > "$work/$label.stdout" 2> "$work/$label.stderr" || actual=$?
    printf '%s\t%s\t%s\n' "$label" "$expected" "$actual" >> "$work/phases.tsv"
    # 原始语料保存在stdout，不把3367条逐帧记录重复打印到门禁终端。
    if [[ $label != layout-records ]]; then cat "$work/$label.stdout"; fi
    cat "$work/$label.stderr" >&2
    printf '[sharedmem] %s exit=%s expected=%s\n' "$label" "$actual" "$expected"
    ((actual == expected))
}
golden() { diff -u <(sed 's/\r$//' "$1") <(sed 's/\r$//' "$2"); }
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt -I"$pkg"
    -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
run compiler 0 "$cc" --version
run runtime 0 "$cc" "${flags[@]}" -c "$pkg/test/xrt_runtime.c" -o "$work/runtime.o"
run native 0 "$cc" "${flags[@]}" -Wmissing-prototypes -c "$pkg/sharedmem_native.c" -o "$work/native.o"
printf '#include "include/sharedmem_native.h"\nint main(void) {return xshm_write_text(0,0,(xstrview){0,0});}\n' > "$work/header.c"
run public-header 0 "$cc" "${flags[@]}" -fsyntax-only "$work/header.c"
run core-compile 0 "$cc" "${flags[@]}" "$pkg/test/test_sharedmem.c" "$work/native.o" "$work/runtime.o" -lm -ldl -lrt -o "$work/core"
run core-real-process 0 "$work/core"
grep -q 'cross-process: real child' "$work/core-real-process.stdout"
run core-JIT-host 0 ./release/xl run "$pkg/test/test_sharedmem.c" "$pkg/sharedmem_native.c" --inc-path lib/xrt
run core-build 0 ./release/xl build "$pkg/test/test_sharedmem.c" "$pkg/sharedmem_native.c" --inc-path lib/xrt -o "$work/core.aot"
run core-AOT-real-process 0 "$work/core.aot"
grep -q 'cross-process: real child' "$work/core-AOT-real-process.stdout"
run allocation-compile 0 "$cc" "${flags[@]}" -DNDEBUG "$pkg/test/test_sharedmem_alloc.c" "$work/native.o" "$work/runtime.o" -lm -ldl -lrt -o "$work/allocations"
run allocations 0 "$work/allocations"
for name in error_contract_c layout_contract_c; do
    source="$pkg/test/$name.c"
    run "$name-compile" 0 "$cc" "${flags[@]}" "$source" "$work/runtime.o" -lm -ldl -lrt -o "$work/$name"
    run "$name-GCC" 0 "$work/$name"
    run "$name-TCC" 0 ./release/xl run "$source"
    run "$name-build" 0 ./release/xl build "$source" -o "$work/$name.aot"
    run "$name-AOT" 0 "$work/$name.aot"
    diff -u "$work/$name-GCC.stdout" "$work/$name-TCC.stdout"
    diff -u "$work/$name-GCC.stdout" "$work/$name-AOT.stdout"
    if [[ $name == layout_contract_c ]]; then
        run layout-records 0 "$work/$name" --records
        run independent-layout 0 "$python" -B "$pkg/test/check_layout.py" "$work/layout-records.stdout"
    fi
done
for source in "$pkg/examples/sharedmem_c.c" "$pkg/examples/snapshots_c.c"; do
    name=$(basename "$source" .c); expected="$pkg/examples/basic.expected.txt"
    if [[ $name == snapshots_c ]]; then expected="$pkg/examples/snapshots.expected.txt"; fi
    run "$name-compile" 0 "$cc" "${flags[@]}" "$source" "$work/native.o" "$work/runtime.o" -lm -ldl -lrt -o "$work/$name"
    run "$name-GCC" 0 "$work/$name"
    run "$name-TCC" 0 ./release/xl run "$source"
    run "$name-build" 0 ./release/xl build "$source" -o "$work/$name.aot"
    run "$name-AOT" 0 "$work/$name.aot"
    for engine in GCC TCC AOT; do golden "$expected" "$work/$name-$engine.stdout"; done
done
for source in test/cases/extlib_sharedmem.xl "$pkg/test/boundaries.xl" "$pkg/test/allocations.xl" "$pkg/examples/snapshots.xl" \
    release/examples/extlibs/sharedmem.xl "$pkg/test/global_teardown_repro.xl" "$pkg/test/local_teardown_control.xl"; do
    name=$(basename "$source" .xl)
    run "$name-JIT" 0 ./release/xl run "$source"
    if [[ $name == snapshots ]]; then golden "$pkg/examples/snapshots.expected.txt" "$work/$name-JIT.stdout"; fi
    if [[ $name == sharedmem ]]; then golden "$pkg/examples/basic.expected.txt" "$work/$name-JIT.stdout"; fi
    run "$name-build" 0 ./release/xl build "$source" -o "$work/$name.aot"
    case "$name" in
        boundaries|sharedmem|global_teardown_repro) run "$name-AOT" 0 "$work/$name.aot" || issues=$((issues+1)) ;;
        *) run "$name-AOT" 0 "$work/$name.aot"; diff -u "$work/$name-JIT.stdout" "$work/$name-AOT.stdout" ;;
    esac
done
for source in test/cases/extlib_sharedmem.xl "$pkg/test/boundaries.xl"; do
    name=$(basename "$source" .xl)
    run "cgen-$name" 0 ./release/xl cgen "$source" --emit "$work/$name.c"
done
grep -hE '^extern .*xshm_[a-z_0-9]+[(]' "$work/extlib_sharedmem.c" "$work/boundaries.c" | sort -u > "$work/decls.txt"
grep -oE 'extern "xshm_[a-z_0-9]+"' "$pkg/src/sharedmem.xl" | cut -d '"' -f 2 | sort -u > "$work/expected.txt"
grep -oE 'xshm_[a-z_0-9]+' "$work/decls.txt" | sort -u > "$work/actual.txt"
[[ $(wc -l < "$work/decls.txt") == 13 ]]
diff -u "$work/expected.txt" "$work/actual.txt"
printf '#include "include/sharedmem_native.h"\n' > "$work/abi.c"; cat "$work/decls.txt" >> "$work/abi.c"
run generated-ABI 0 "$cc" "${flags[@]}" -fsyntax-only "$work/abi.c"
sed 's/int64_t xshm_size/double xshm_size/' "$work/abi.c" > "$work/abi-bad.c"
run wrong-return-control 1 "$cc" "${flags[@]}" -fsyntax-only "$work/abi-bad.c"
run private-native 1 ./release/xl check "$pkg/test/private_native_negative.xl"
grep -q 'access' "$work/private-native.stdout" "$work/private-native.stderr"
run private-constructor 1 ./release/xl check "$pkg/test/private_constructor_negative.xl" || issues=$((issues+1))
if ((issues)); then printf 'sharedmem release blockers remain: %s\n' "$issues" >&2; exit 1; fi
printf 'sharedmem Linux sanitizer, real process, ABI and bilingual JIT/AOT gate: PASS\n'
