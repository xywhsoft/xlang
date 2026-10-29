#!/usr/bin/env bash
set -euo pipefail

# Linux 独立 C 门禁，不重建编译器，不使用共享对象目录。
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"
work="$root/build/test/mqtt_linux_guard"
mkdir -p "$work"
cc=${CC:-gcc}
package=release/extlibs/mqtt
sources=("$package/mqtt_frame.c" "$package/mqtt_build.c" "$package/mqtt_parse.c"
         "$package/mqtt_net.c" "$package/mqtt_native.c")
inputs=("${sources[@]}" "$package/mqtt_internal.h" "$package"/include/*.h
        "$package"/test/*.c "$package"/test/*.h lib/xrt/xrt.h
        release/examples/extlibs/mqtt_c.c release/examples/extlibs/mqtt_tcp_c.c
        release/examples/extlibs/mqtt.expected.txt release/examples/extlibs/mqtt_tcp.expected.txt
        tools/check_extlib_mqtt.sh)
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt
       -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

sha256sum "${inputs[@]}" > "$work/inputs.before.sha256"
"$cc" --version > "$work/compiler.txt"
"$cc" "${flags[@]}" -c "$package/test/mqtt_runtime.c" -o "$work/runtime.o"
for test in test_mqtt test_mqtt_alloc test_mqtt_net test_mqtt_exchange
do
    test_sources=("$package/test/$test.c")
    if [[ "$test" == test_mqtt_net || "$test" == test_mqtt_exchange ]]
    then
        test_sources+=("$package/test/mqtt_fixture.c")
    fi
    "$cc" "${flags[@]}" "${test_sources[@]}" "${sources[@]}" "$work/runtime.o" \
        -lm -ldl -o "$work/$test"
    timeout 60s "$work/$test" 2>&1 | tee "$work/$test.log"
done

for example in mqtt mqtt_tcp
do
    "$cc" "${flags[@]}" "release/examples/extlibs/${example}_c.c" "$work/runtime.o" -lm -ldl -o "$work/$example"
    timeout 30s "$work/$example" > "$work/$example.log"
    diff -u "release/examples/extlibs/$example.expected.txt" "$work/$example.log"
done
sha256sum "${inputs[@]}" > "$work/inputs.after.sha256"
diff -u "$work/inputs.before.sha256" "$work/inputs.after.sha256"
printf 'MQTT Linux C ASan/UBSan gate: PASS\n'
