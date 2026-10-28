#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"
packages=("$@")
if ((${#packages[@]} == 0)); then packages=(lz4 bz2 lzma miniz snappy xxhash zstd); fi
for package in "${packages[@]}"; do
    case "$package" in
        lz4|bz2|lzma|miniz|snappy|xxhash|zstd) ;;
        *) printf 'unsupported native global fixture: %s\n' "$package" >&2; exit 2 ;;
    esac
done
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-native-globals-XXXXXX")
export XLANG_PACKAGE_PATH="$root/release/extlibs"

input_paths() {
    for package in "${packages[@]}"; do
        find "release/extlibs/$package" -type f -print0
        if [[ $package == bz2 ]]; then find release/extlibs/_bzip2 -type f -print0; fi
    done
    printf '%s\0' lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so \
        release/libxlc.so tools/check_extlib_native_globals.ps1 tools/check_extlib_native_globals.sh
    find release/include -type f -print0
}

finish_gate() {
    local result=$?
    trap - EXIT
    set +e
    mapfile -d '' after < <(input_paths | sort -zu)
    sha256sum "${after[@]}" > "$work/inputs.after.sha256"
    if ! diff -u "$work/inputs.before.sha256" "$work/inputs.after.sha256"; then result=125; fi
    printf '%s\n' "$result" > "$work/exit-code.txt"
    if ((result == 0)); then
        printf 'Native global lifetime JIT/AOT checks: PASS\n'
        rm -rf -- "$work"
    else
        printf '[native-globals] FAIL exit=%s artifacts=%s\n' "$result" "$work" >&2
    fi
    exit "$result"
}
trap finish_gate EXIT
mapfile -d '' inputs < <(input_paths | sort -zu)
sha256sum "${inputs[@]}" > "$work/inputs.before.sha256"
printf '%s\n' "XLANG_PACKAGE_PATH=$XLANG_PACKAGE_PATH" > "$work/environment.txt"
printf 'phase\texit\n' > "$work/results.tsv"

# 每个失败都保留原始退出码；不因首个包失败而漏掉其余包。
run() {
    local name=$1 seconds=$2
    shift 2
    printf '%q ' timeout "${seconds}s" "$@" > "$work/$name.command"
    printf '\n' >> "$work/$name.command"
    last_status=0
    timeout "${seconds}s" "$@" > "$work/$name.stdout" 2> "$work/$name.stderr" || last_status=$?
    printf '%s\t%s\n' "$name" "$last_status" >> "$work/results.tsv"
    printf '[native-globals] %s: exit=%s\n' "$name" "$last_status"
    cat "$work/$name.stdout" "$work/$name.stderr"
}

failed=0
for package in "${packages[@]}"; do
    source="release/extlibs/$package/test/top_level.xl"
    printf '%s top-level: PASS\n' "$package" > "$work/$package.expected"
    run "$package-jit" 90 ./release/xl run "$source"
    if ((last_status != 0)) || ! cmp -s "$work/$package.expected" "$work/$package-jit.stdout"; then failed=1; fi
    run "$package-build" 180 ./release/xl build "$source" -o "$work/$package"
    if ((last_status != 0)); then failed=1; continue; fi
    run "$package-aot" 90 "$work/$package"
    if ((last_status != 0)) || ! cmp -s "$work/$package.expected" "$work/$package-aot.stdout"; then failed=1; fi
done
exit "$failed"
