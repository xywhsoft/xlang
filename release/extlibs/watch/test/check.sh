#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-watch-XXXXXX")
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
    else printf '[watch] FAIL exit=%s artifacts=%s\n' "$result" "$work" >&2; fi
    printf '[watch] report: %s.report.txt\n' "$work"
    exit "$result"
}
trap cleanup EXIT
record_failure() {
    printf 'exit=%s\nline=%s\ncommand=%s\npipeline=%s\n' "$@" > "$work/failure.txt"
    cat "$work/failure.txt" >&2
}
trap 'record_failure "$?" "$LINENO" "$BASH_COMMAND" "${PIPESTATUS[*]}"' ERR
mkdir -p "$work/case/build/test/watch_guard"
cc=${CC:-gcc}
cc_path=$(command -v "$cc" || true)
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror
       -Ilib/xrt -Irelease/extlibs/watch -fsanitize=address,undefined
       -fno-omit-frame-pointer -fno-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
export XLANG_PACKAGE_PATH="$root/release/extlibs"
input_paths() {
    { find release/extlibs/watch release/include -type f -print0
      printf '%s\0' lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so \
          release/libxlc.so release/examples/extlibs/watch.xl
      if [[ -n "$cc_path" ]]; then printf '%s\0' "$cc_path"; fi
    }
}
mapfile -d '' inputs < <(input_paths | sort -zu)
sha256sum "${inputs[@]}" > "$work/inputs.before.sha256"
"$cc" --version > "$work/compiler.txt"
compile() { timeout 180s "$cc" "${flags[@]}" "$@"; }

compile -DXRT_MODULE_DIR -DXRT_MODULE_PATH -DXRT_MODULE_HASH64 \
    -fsyntax-only release/extlibs/watch/watch_native.c
timeout 90s ./release/xl cgen release/extlibs/watch/test/boundaries.xl --emit "$work/owned_event.c"
grep -E '^extern .*xwatch_[a-z0-9_]+[(]' "$work/owned_event.c" | sort -u > "$work/abi.decls"
grep -oE '"xwatch_[a-z0-9_]+"' release/extlibs/watch/src/watch.xl | tr -d '"' | sort -u > "$work/abi.expected"
grep -oE 'xwatch_[a-z0-9_]+[(]' "$work/abi.decls" | tr -d '(' | sort -u > "$work/abi.actual"
test "$(wc -l < "$work/abi.decls")" -eq 8
diff -u "$work/abi.expected" "$work/abi.actual"
printf '#include "watch_native.h"\n' > "$work/abi.c"
cat "$work/abi.decls" >> "$work/abi.c"
compile -fsyntax-only "$work/abi.c"
grep -Eq '[.]Data[[:space:]]*=[[:space:]]*xwatch_poll[[:space:]]*[(]' "$work/owned_event.c"
if grep -Eq 'xl6_xrt_take_cstr\(|xl6CStringCopyV1' "$work/owned_event.c"; then
    printf 'watch poll must directly adopt the owned event\n' >&2
    exit 1
fi

compile -c release/extlibs/watch/test/xrt_runtime.c -o "$work/xrt.o"
compile -c release/extlibs/watch/watch_native.c -o "$work/native.o"
objects=("$work/native.o" "$work/xrt.o")
compile release/extlibs/watch/test/test_watch.c "${objects[@]}" -no-pie -lm -ldl -o "$work/test_watch"
(cd "$work/case" && timeout 90s "$work/test_watch")
compile release/extlibs/watch/test/test_directory_race.c "${objects[@]}" \
    -Wl,--wrap=xrtDirOpen -no-pie -lm -ldl -o "$work/test_directory_race"
(cd "$work/case" && timeout 30s "$work/test_directory_race")
digest=release/extlibs/watch/test/directory_digest_c.c
compile -DNDEBUG "$digest" "$work/xrt.o" -no-pie -lm -ldl -o "$work/digest"
timeout 90s "$work/digest"
timeout 90s ./release/xl run "$digest"
compile release/extlibs/watch/examples/watch_c.c "$work/xrt.o" -no-pie -lm -ldl -o "$work/example"
timeout 30s "$work/example" > "$work/example.txt"
diff -u <(printf 'count: 1\nexists: true\npath matches: true\nafter remove: 0\nslot reused: true\ncreated: true\nmodified: true\ndeleted: true\n') "$work/example.txt"
timeout 90s ./release/xl run release/extlibs/watch/examples/watch_c.c > "$work/example_direct.txt"
diff -u "$work/example.txt" "$work/example_direct.txt"
compile release/extlibs/watch/examples/directory_c.c "$work/xrt.o" -no-pie -lm -ldl -o "$work/directory_c"
timeout 30s "$work/directory_c" > "$work/directory_c.txt"
diff -u <(sed 's/\r$//' release/extlibs/watch/examples/directory.expected.txt) "$work/directory_c.txt"
timeout 90s ./release/xl run release/extlibs/watch/examples/directory_c.c > "$work/directory_direct.txt"
diff -u "$work/directory_c.txt" "$work/directory_direct.txt"
timeout 90s ./release/xl run release/extlibs/watch/examples/directory.xl > "$work/directory_xl.txt"
diff -u "$work/directory_c.txt" "$work/directory_xl.txt"

for name in boundaries cwd_snapshot allocations owned_events; do
    (cd "$work/case" && timeout 90s "$root/release/xl" run "$root/release/extlibs/watch/test/$name.xl")
done
(cd "$root/release" && timeout 90s ./xl run extlibs/watch/examples/watch.xl) > "$work/xl_example.txt"
diff -u "$work/example.txt" "$work/xl_example.txt"
if timeout 90s ./release/xl check release/extlibs/watch/test/private_native.xl > "$work/private_native.txt" 2>&1; then
    private_status=0
else
    private_status=$?
fi
test "$private_status" -eq 1
grep -Fq 'addNative' "$work/private_native.txt"
grep -Fq 'access' "$work/private_native.txt"
for name in boundaries cwd_snapshot allocations owned_events; do
    timeout 180s ./release/xl build "release/extlibs/watch/test/$name.xl" -o "$work/watch_$name"
    (cd "$work/case" && timeout 90s "$work/watch_$name")
done
timeout 180s ./release/xl build release/extlibs/watch/examples/watch.xl -o "$work/watch_example_xl"
(cd "$root/release" && timeout 90s "$work/watch_example_xl") > "$work/example_aot.txt"
diff -u "$work/example.txt" "$work/example_aot.txt"
timeout 180s ./release/xl build release/examples/extlibs/watch.xl -o "$work/watch_public_xl"
(cd "$root/release" && timeout 90s "$work/watch_public_xl") > "$work/public_aot.txt"
(cd "$root/release" && timeout 90s ./xl run examples/extlibs/watch.xl) > "$work/public_jit.txt"
diff -u "$work/public_jit.txt" "$work/public_aot.txt"
timeout 180s ./release/xl build release/extlibs/watch/examples/watch_c.c -o "$work/watch_example_c_aot"
timeout 90s "$work/watch_example_c_aot" > "$work/example_c_aot.txt"
diff -u "$work/example.txt" "$work/example_c_aot.txt"
for source in "$digest" release/extlibs/watch/examples/directory.xl release/extlibs/watch/examples/directory_c.c; do
    target="$work/$(basename "${source%.*}")"
    timeout 180s ./release/xl build "$source" -o "$target"
    timeout 90s "$target" > "$work/$(basename "$source").out"
    if [[ "$source" == */examples/* ]]; then diff -u "$work/directory_c.txt" "$work/$(basename "$source").out"; fi
done

# 全局对象必须自然退役；已知主线退出错误也要保留非零门禁结果。
global_source=release/extlibs/watch/test/top_level.xl
timeout 90s ./release/xl run "$global_source"
timeout 180s ./release/xl build "$global_source" -o "$work/global"
timeout 90s "$work/global"
printf 'watch Linux ASan/UBSan, ABI, XLang JIT/AOT, global retirement and bilingual parity: PASS\n'
