#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/../../../.." && pwd)
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-singleinstance-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cd "$root"
mkdir "$work/home"
chmod 0700 "$work/home"
export HOME="$work/home"

flags='-std=c11 -D_DEFAULT_SOURCE -DNDEBUG -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -Ilib/xrt'
links='-fsanitize=address,undefined -pthread -ldl -lm'

gcc $flags -c release/extlibs/singleinstance/test/runtime.c -o "$work/runtime.o"
gcc $flags -c release/extlibs/singleinstance/singleinstance_native.c -o "$work/native.o"
gcc $flags release/extlibs/singleinstance/test/test_singleinstance.c \
    "$work/native.o" "$work/runtime.o" $links -o "$work/boundaries"
timeout 120s "$work/boundaries"

# 已存在的宽权限目录不能充当用户私有锁目录。
mkdir -p "$work/unsafe-home/.xlang/singleinstance"
chmod 0700 "$work/unsafe-home" "$work/unsafe-home/.xlang"
chmod 0777 "$work/unsafe-home/.xlang/singleinstance"
HOME="$work/unsafe-home" timeout 30s "$work/boundaries" --unsafe-dir
chmod 0700 "$work/unsafe-home/.xlang/singleinstance"
chmod 0777 "$work/unsafe-home/.xlang"
HOME="$work/unsafe-home" timeout 30s "$work/boundaries" --unsafe-dir

mkdir -p "$work/link-home" "$work/link-target/singleinstance"
chmod 0700 "$work/link-home" "$work/link-target" "$work/link-target/singleinstance"
ln -s "$work/link-target" "$work/link-home/.xlang"
HOME="$work/link-home" timeout 30s "$work/boundaries" --unsafe-dir

# 两份源码拥有独立静态状态，模拟同一进程的两个包/JIT 实例。
for instance in a b; do
    gcc $flags "-Dxsi_acquire=xsi_${instance}_acquire" \
        "-Dxsi_release=xsi_${instance}_release" \
        "-Dxsi_is_acquired=xsi_${instance}_is_acquired" \
        -c release/extlibs/singleinstance/singleinstance_native.c -o "$work/$instance.o"
done
gcc $flags release/extlibs/singleinstance/test/test_multiple_modules.c \
    "$work/a.o" "$work/b.o" "$work/runtime.o" $links -o "$work/modules"
timeout 60s "$work/modules"

gcc $flags release/extlibs/singleinstance/examples/singleinstance_c.c \
    "$work/native.o" "$work/runtime.o" $links -o "$work/example"
timeout 30s "$work/example" > "$work/c_example.txt"
timeout 30s ./release/xl run release/extlibs/singleinstance/examples/singleinstance_c.c \
    > "$work/c_direct.txt"
diff -u "$work/c_example.txt" "$work/c_direct.txt"
timeout 90s ./release/xl run test/cases/extlib_singleinstance.xl
timeout 90s ./release/xl run release/extlibs/singleinstance/test/boundaries.xl
timeout 90s ./release/xl run release/examples/extlibs/singleinstance.xl \
    > "$work/xl_example.txt"
diff -u "$work/c_example.txt" "$work/xl_example.txt"
diff -u release/extlibs/singleinstance/test/singleinstance.expected.txt \
    "$work/c_example.txt"
for source in test/cases/extlib_singleinstance.xl \
    release/extlibs/singleinstance/test/boundaries.xl; do
    name=$(basename "$source" .xl)
    timeout 120s ./release/xl build "$source" -o "$work/$name"
    timeout 90s "$work/$name"
done
timeout 120s ./release/xl build release/examples/extlibs/singleinstance.xl \
    -o "$work/example_xl"
timeout 30s "$work/example_xl" > "$work/example_xl.txt"
diff -u release/extlibs/singleinstance/test/singleinstance.expected.txt \
    "$work/example_xl.txt"
timeout 120s ./release/xl build \
    release/extlibs/singleinstance/examples/singleinstance_c.c \
    -o "$work/example_c_aot"
timeout 30s "$work/example_c_aot" > "$work/example_c_aot.txt"
diff -u release/extlibs/singleinstance/test/singleinstance.expected.txt \
    "$work/example_c_aot.txt"
sh release/extlibs/singleinstance/test/check_contexts.sh
printf 'singleinstance Linux C ASan/UBSan and bilingual AOT gate: PASS\n'
