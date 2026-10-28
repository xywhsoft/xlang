#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-watch-XXXXXX")
cleanup() {
    result=$?
    trap - EXIT
    set +e
    if ((result == 0)); then
        rm -rf -- "$work" || result=$?
    else
        printf '[watch] FAIL exit=%s artifacts=%s\n' "$result" "$work" >&2
    fi
    exit "$result"
}
trap cleanup EXIT
record_failure() {
    printf 'exit=%s\nline=%s\ncommand=%s\npipeline=%s\n' "$@" > "$work/failure.txt"
    cat "$work/failure.txt" >&2
}
trap 'record_failure "$?" "$LINENO" "$BASH_COMMAND" "${PIPESTATUS[*]}"' ERR
exec > >(tee "$work/output.log") 2>&1
mkdir -p "$work/case/build/test/watch_guard"
cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror
       -Ilib/xrt -Irelease/extlibs/watch
       -fsanitize=address,undefined -fno-omit-frame-pointer
       -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

mapfile -d '' inputs < <(find release/extlibs/watch release/include -type f -print0)
inputs+=(lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so release/libxlc.so
          release/examples/extlibs/watch.xl)
sha256sum "${inputs[@]}" > "$work/inputs.before.sha256"
"$cc" --version > "$work/compiler.txt"

"$cc" "${flags[@]}" -c release/extlibs/watch/test/xrt_runtime.c \
    -o "$work/xrt_runtime.o"
"$cc" "${flags[@]}" release/extlibs/watch/watch_native.c \
    release/extlibs/watch/test/test_watch.c "$work/xrt_runtime.o" \
    -lm -ldl -o "$work/test_watch"
(cd "$work/case" && timeout 90s "$work/test_watch")
"$cc" "${flags[@]}" release/extlibs/watch/watch_native.c \
    release/extlibs/watch/test/test_directory_race.c "$work/xrt_runtime.o" \
    -Wl,--wrap=xrtDirOpen -lm -ldl -o "$work/test_directory_race"
(cd "$work/case" && timeout 30s "$work/test_directory_race")
"$cc" "${flags[@]}" release/extlibs/watch/examples/watch_c.c \
    "$work/xrt_runtime.o" \
    -lm -ldl -o "$work/example"
timeout 30s "$work/example" > "$work/example.txt"
diff -u <(printf 'count: 1\nexists: true\npath matches: true\nafter remove: 0\nslot reused: true\ncreated: true\nmodified: true\ndeleted: true\n') "$work/example.txt"
timeout 90s ./release/xl run release/extlibs/watch/examples/watch_c.c > "$work/example_direct.txt"
diff -u "$work/example.txt" "$work/example_direct.txt"
(cd "$work/case" && timeout 90s "$root/release/xl" run "$root/release/extlibs/watch/test/boundaries.xl")
(cd "$work/case" && timeout 90s "$root/release/xl" run "$root/release/extlibs/watch/test/cwd_snapshot.xl")
(cd "$work/case" && timeout 90s "$root/release/xl" run "$root/release/extlibs/watch/test/allocations.xl")
(cd "$work/case" && timeout 90s "$root/release/xl" run "$root/release/extlibs/watch/test/owned_events.xl")
(cd "$root/release" && timeout 90s ./xl run extlibs/watch/examples/watch.xl > "$work/xl_example.txt")
diff -u "$work/example.txt" "$work/xl_example.txt"
if timeout 90s ./release/xl check release/extlibs/watch/test/private_native.xl > "$work/private_native.txt" 2>&1; then
    printf 'watch: Native methods must not be public\n' >&2
    exit 1
fi
if ! grep -Fq 'addNative' "$work/private_native.txt" ||
   ! grep -Fq 'access' "$work/private_native.txt"; then
    cat "$work/private_native.txt" >&2
    printf 'watch: expected private-access diagnostic\n' >&2
    exit 1
fi
for name in boundaries cwd_snapshot allocations owned_events; do
    timeout 120s ./release/xl build "release/extlibs/watch/test/$name.xl" \
        -o "$work/watch_$name"
    (cd "$work/case" && timeout 90s "$work/watch_$name")
done
timeout 120s ./release/xl build release/extlibs/watch/examples/watch.xl \
    -o "$work/watch_example_xl"
(cd "$root/release" && timeout 45s "$work/watch_example_xl" > "$work/example_aot.txt")
diff -u "$work/example.txt" "$work/example_aot.txt"
timeout 120s ./release/xl build release/examples/extlibs/watch.xl \
    -o "$work/watch_public_xl"
(cd "$root/release" && timeout 45s "$work/watch_public_xl" > "$work/public_aot.txt")
(cd "$root/release" && timeout 45s ./xl run examples/extlibs/watch.xl > "$work/public_jit.txt")
diff -u "$work/public_jit.txt" "$work/public_aot.txt"
timeout 120s ./release/xl build release/extlibs/watch/examples/watch_c.c \
    -o "$work/watch_example_c_aot"
timeout 45s "$work/watch_example_c_aot" > "$work/example_c_aot.txt"
diff -u "$work/example.txt" "$work/example_c_aot.txt"
sha256sum -c "$work/inputs.before.sha256" > "$work/inputs.after.txt"
printf 'watch Linux ASan/UBSan, XLang JIT/AOT and C/XLang parity gate: PASS\n'
