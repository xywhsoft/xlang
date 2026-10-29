#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/../../../.." && pwd)
work_root=$(CDPATH= cd -- "${TMPDIR:-/tmp}" && pwd)
work=$(mktemp -d "$work_root/xlang-tar-numeric-XXXXXX")
cleanup() {
    code=$?
    if [ "$code" -eq 0 ]; then
        case "$work" in "$work_root"/xlang-tar-numeric-*) rm -rf -- "$work";; *) exit 1;; esac
    else
        printf 'TAR numeric failure artifacts retained: %s\n' "$work" >&2
    fi
}
trap cleanup EXIT
count=0
run() {
    label=$1
    shift
    count=$((count + 1))
    printf '\n[%s] ' "$label" >> "$work/commands.log"
    printf '%s ' "$@" >> "$work/commands.log"
    printf '\n' >> "$work/commands.log"
    if "$@" > "$work/run-$count.log" 2>&1; then
        cat "$work/run-$count.log" >> "$work/commands.log"
        cat "$work/run-$count.log"
        printf '[tar numeric] %s: PASS\n' "$label"
    else
        code=$?
        cat "$work/run-$count.log" >> "$work/commands.log"
        printf 'exit=%s\n' "$code" >> "$work/commands.log"
        cat "$work/commands.log" >&2
        return 1
    fi
}
find "$root/release/extlibs/tar" "$root/release/extlibs/gzip" \
    "$root/release/extlibs/bz2" "$root/release/extlibs/_bzip2" \
    "$root/release/extlibs/lzma" "$root/release/include" \
    -type f -exec sha256sum {} + > "$work/inputs.sha256"
sha256sum "$root/lib/xrt/xrt.h" "$root/lib/xruntime/xruntime.h" \
    "$root/release/xl" "$root/release/libxl.so" "$root/release/libxlc.so" \
    >> "$work/inputs.sha256"
cd "$root"
cc=${CC:-cc}
flags='-std=c11 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer'
sources='release/extlibs/tar/xrt_tar_common.c release/extlibs/tar/xrt_tar_reader.c release/extlibs/tar/xrt_tar_writer.c release/extlibs/tar/test/runtime.c'
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
run 'C compiler version' "$cc" --version
run 'Python version' python3 --version
run 'C numeric sanitizer build' "$cc" $flags -Ilib/xrt -Irelease/extlibs/tar $sources \
    release/extlibs/tar/test/test_numeric_fields.c -pthread -lm -o "$work/numeric_c"
run 'C header range and ID matrix' "$work/numeric_c"
run 'C metadata sanitizer build' "$cc" $flags -Ilib/xrt \
    release/extlibs/tar/examples/metadata_c.c release/extlibs/tar/test/runtime.c \
    -pthread -lm -o "$work/metadata_c"
expected='mtime=-1;uid=18446744073709551615;gid=9223372036854775808'
output=$("$work/metadata_c")
[ "$output" = "$expected" ] || { printf 'C metadata output differs\n' >&2; exit 1; }
for name in negative_id_type negative_gid_type; do
    if timeout 90s ./release/xl check "release/extlibs/tar/test/$name.xl" > "$work/$name.log" 2>&1; then
        printf 'negative unsigned ID was accepted\n' >&2
        exit 1
    fi
    grep -Eq '\[sem\].*TarWriter.add' "$work/$name.log"
    printf '[tar numeric] %s compile-only: PASS\n' "$name"
done
run 'XLang full-width IDs' timeout 90s ./release/xl run release/extlibs/tar/test/numeric_ids.xl
for source in metadata.xl metadata_c.c; do
    output=$(timeout 90s ./release/xl run "release/extlibs/tar/examples/$source")
    [ "$output" = "$expected" ] || { printf '%s output differs\n' "$source" >&2; exit 1; }
    printf '[tar numeric] %s JIT: PASS\n' "$source"
done
run 'numeric fields AOT build' timeout 90s ./release/xl build \
    release/extlibs/tar/test/numeric_fields.xl -o "$work/numeric_fields"
run 'IDs AOT build' timeout 90s ./release/xl build \
    release/extlibs/tar/test/numeric_ids.xl -o "$work/numeric_ids"
run 'IDs AOT' timeout 90s "$work/numeric_ids"
for source in metadata.xl metadata_c.c; do
    run "$source AOT build" timeout 90s ./release/xl build \
        "release/extlibs/tar/examples/$source" -o "$work/$source.aot"
    output=$(timeout 90s "$work/$source.aot")
    [ "$output" = "$expected" ] || { printf '%s AOT differs\n' "$source" >&2; exit 1; }
    printf '[tar numeric] %s AOT: PASS\n' "$source"
done
run 'independent Python numeric oracle' python3 release/extlibs/tar/test/numeric_fields.py \
    "$root/release" "$work/numeric_c" "$work" --aot "$work/numeric_fields"
sha256sum -c "$work/inputs.sha256" > "$work/inputs.recheck"
printf '[tar numeric] Linux sanitizer/C/JIT/AOT checks: PASS\n'
