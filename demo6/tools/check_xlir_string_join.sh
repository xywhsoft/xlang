#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"
compiler=${1:?Expected public-source build directory}
generated=${2:?Expected string gate directory}
out=${3:?Expected fresh evidence directory}
compiler=$(realpath "$compiler")
generated=$(realpath "$generated")
if [[ -e "$out" ]]; then echo 'Choose a fresh evidence directory' >&2; exit 2; fi
mkdir -p "$out"
out=$(realpath "$out")
work=$(mktemp -d /tmp/xlir-string-XXXXXX)
trap 'status=$?; cp "$work"/*.log "$work"/*.sha256 "$work"/compiler.txt "$out"/ 2>/dev/null || true; printf "%s\n" "$status" > "$out/exit-code.txt"' EXIT
sources=(tools/xrt_single_header_impl.c src/runtime/*.c)
snapshot() {
    local paths=()
    mapfile -d '' paths < <(find src/runtime src/api "$compiler/include" -type f -print0 | sort -z)
    paths+=(lib/xrt/xrt.h lib/xruntime/xruntime.h tools/xrt_single_header_impl.c
            tools/check_xlir_string_join.sh test/cases/xlir_string_bridge_abi_host.c
            "$generated/module.c")
    sha256sum "${paths[@]}"
}
snapshot > "$work/inputs.before.sha256"
cc=${CC:-gcc}
"$cc" --version > "$work/compiler.txt"
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Wno-unused-function
       -Wno-unused-parameter -Wno-unused-variable -Wno-unused-label -Wno-unused-const-variable
       -Wno-missing-field-initializers -I. -Irelease/include -Ilib/xrt -Ilib/xruntime
       -I"$compiler/include" -pthread -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie)
objects=()
for source in "${sources[@]}"; do
    object="$work/$(basename "${source%.c}").o"
    "$cc" "${flags[@]}" -c "$source" -o "$object" > "$work/$(basename "${source%.c}").build.log" 2>&1
    objects+=("$object")
done
"$cc" "${flags[@]}" -Werror -include "$generated/module.c" -c test/cases/xlir_string_bridge_abi_host.c \
    -o "$work/host.o" > "$work/host.build.log" 2>&1
"$cc" "${flags[@]}" "$work/host.o" "${objects[@]}" -lm -ldl -o "$work/host" > "$work/link.log" 2>&1
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
timeout 120s "$work/host" > "$work/run.log" 2>&1
cat "$work/run.log"
snapshot > "$work/inputs.after.sha256"
diff -u "$work/inputs.before.sha256" "$work/inputs.after.sha256"
printf 'String generated C on Linux: ASan/UBSan, allocation failure and threads PASS\n'
