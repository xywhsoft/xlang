#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
XL="$ROOT/release/xl"
TEST_DIR="$ROOT/build/linux/test"
RELEASE_TMP="$ROOT/release/tmp"
BUILD_TMP="$ROOT/build/tmp"
export XLANG_PACKAGE_PATH="$ROOT/release/extlibs:$ROOT/test/fixtures"

if [[ ! -x "$XL" ]]; then
    "$ROOT/build_linux.sh"
fi

mkdir -p "$TEST_DIR"
cd "$ROOT"

snapshot_runtime_temp() {
    {
        if [[ -d "$RELEASE_TMP" ]]; then
            find "$RELEASE_TMP" -type f -printf '%p|%s|%T@\n'
        fi
        if [[ -d "$BUILD_TMP" ]]; then
            find "$BUILD_TMP" -type f -printf '%p|%s|%T@\n'
        fi
    } | LC_ALL=C sort
}

printf 'Running Linux CLI and in-memory VFS smoke tests...\n'
"$XL" --help >/dev/null
"$XL" check "$ROOT/release/examples/hello.xl"
snapshot_runtime_temp >"$TEST_DIR/temp.before"
"$XL" run "$ROOT/release/examples/hello.xl"
snapshot_runtime_temp >"$TEST_DIR/temp.after"
cmp "$TEST_DIR/temp.before" "$TEST_DIR/temp.after"

printf 'Running Linux ELF build tests...\n'
"$XL" build "$ROOT/release/examples/hello.xl" -o "$TEST_DIR/hello"
"$TEST_DIR/hello"

printf 'Running Linux C and XRT integration tests...\n'
# Two-thread component check: restoring a snapshot on the wrong native thread
# must fail without changing that thread's floating-point environment.
gcc -std=c11 -Wall -Wextra -Werror -frounding-math -fno-builtin \
    "$ROOT/test/fixtures/crt_float_env_posix/float_env_native_test.c" \
    "$ROOT/release/extlibs/floatEnv/float_env_native.c" \
    -pthread -lm -o "$TEST_DIR/float_env_native_test"
"$TEST_DIR/float_env_native_test"
"$XL" run "$ROOT/release/examples/stdlib_math_c.c"
"$XL" build "$ROOT/release/examples/stdlib_math_c.c" -o "$TEST_DIR/stdlib_math_c"
"$TEST_DIR/stdlib_math_c"

printf 'Running Linux numeric declaration, sanitizer and artifact contracts...\n'
bash "$ROOT/tools/check_stdlib_number_values.sh" \
    "$TEST_DIR/numeric-$(date +%Y%m%d-%H%M%S)-$$"

printf 'Running Linux Unicode/HTML declaration, sanitizer and artifact contracts...\n'
bash "$ROOT/tools/check_stdlib_unicode.sh" \
    "$TEST_DIR/unicode-$(date +%Y%m%d-%H%M%S)-$$"

printf 'Running Linux encoding declaration, sanitizer and artifact contracts...\n'
bash "$ROOT/tools/check_stdlib_encoding.sh" \
    "$TEST_DIR/encoding-$(date +%Y%m%d-%H%M%S)-$$"

printf 'Running Linux scalar constant and component graph contracts...\n'
bash "$ROOT/tools/check_xlir_scalar_component_kernel.sh" \
    "$TEST_DIR/scalar-component-$(date +%Y%m%d-%H%M%S)-$$"

bash "$ROOT/tools/check_xlir_string_join.sh" \
    "$TEST_DIR/string-contract-$(date +%Y%m%d-%H%M%S)-$$"

bash "$ROOT/tools/check_stdlib_regex.sh" \
    "$TEST_DIR/regex-contract-$(date +%Y%m%d-%H%M%S)-$$"

printf 'Running Linux UTF-8 path tests...\n'
"$XL" run "$ROOT/test/cases/stdlib_release_string_abi.xl"
"$XL" build "$ROOT/test/cases/stdlib_release_string_abi.xl" \
    -o "$TEST_DIR/stdlib_release_string_abi"
"$TEST_DIR/stdlib_release_string_abi"
mkdir -p "$TEST_DIR/中文路径"
cp "$ROOT/release/examples/hello.xl" "$TEST_DIR/中文路径/你好.xl"
"$XL" check "$TEST_DIR/中文路径/你好.xl"
"$XL" run "$TEST_DIR/中文路径/你好.xl"
"$XL" build "$TEST_DIR/中文路径/你好.xl" -o "$TEST_DIR/中文路径/你好"
"$TEST_DIR/中文路径/你好"

printf 'Running Linux import and package tests...\n'
"$XL" run "$ROOT/test/cases/import_source_use.xl"
"$XL" run "$ROOT/release/examples/extlibs/pkg_math.xl"
"$XL" run "$ROOT/test/cases/import_package_include_dir.xl"
"$XL" build "$ROOT/test/cases/import_package_include_dir.xl" \
    -o "$TEST_DIR/import_package_include_dir"
"$TEST_DIR/import_package_include_dir"
"$XL" run "$ROOT/release/extlibs/xregex/test/xregex_test.xl"
"$XL" build "$ROOT/release/extlibs/xregex/test/xregex_test.xl" \
    -o "$TEST_DIR/xregex_test"
"$TEST_DIR/xregex_test"
"$XL" run "$ROOT/release/extlibs/xssh/test/xssh_test.xl"
"$XL" build "$ROOT/release/extlibs/xssh/test/xssh_test.xl" \
    -o "$TEST_DIR/xssh_test"
"$TEST_DIR/xssh_test"
"$XL" run "$ROOT/release/extlibs/xssh/test/test_xssh.c" \
    "$ROOT/release/extlibs/xssh/xssh_native.c" \
    --inc-path "$ROOT/release/extlibs/xssh/include"

printf 'Running Linux embedded host string boundary tests...\n'
# Include a self-contained embedded host, not only the outer JIT's symbols.
"$XL" run "$ROOT/test/cases/stdlib_string_bridge_boundaries.xl"
"$XL" build "$ROOT/test/cases/stdlib_string_bridge_boundaries.xl" \
    -o "$TEST_DIR/stdlib_string_bridge_boundaries"
"$TEST_DIR/stdlib_string_bridge_boundaries"
printf 'Running Linux network engine trim tests...\n'
"$XL" cgen "$ROOT/test/cases/stdlib_net_engine_trim.xl" \
    --emit "$TEST_DIR/stdlib_net_engine_trim.c"
grep -q 'xl6_xrt_net_engine_stats(' \
    "$TEST_DIR/stdlib_net_engine_trim.c"
if grep -Eq 'xl6_net_resolver|xl6_xrt_(tcp_|udp_|net_resolv)|XRT_MODULE_NET_RESOLVER|XRT_MODULE_NET_TCP_|XRT_MODULE_NET_UDP' \
    "$TEST_DIR/stdlib_net_engine_trim.c"; then
    printf 'network engine stats unexpectedly keep resolver or transport modules.\n' >&2
    exit 1
fi
"$XL" run "$ROOT/test/cases/stdlib_net_engine_trim.xl"
"$XL" build "$ROOT/test/cases/stdlib_net_engine_trim.xl" \
    -o "$TEST_DIR/stdlib_net_engine_trim"
"$TEST_DIR/stdlib_net_engine_trim"

"$XL" run "$ROOT/release/extlibs/xmail/test/xmail_test.xl"
"$XL" build "$ROOT/release/extlibs/xmail/test/xmail_test.xl" \
    -o "$TEST_DIR/xmail_test"
"$TEST_DIR/xmail_test"
for protocol in smtp pop3 imap
do
    "$XL" run "$ROOT/release/extlibs/xmail/test/xmail_${protocol}_loopback_test.xl"
    "$XL" build "$ROOT/release/extlibs/xmail/test/xmail_${protocol}_loopback_test.xl" \
        -o "$TEST_DIR/xmail_${protocol}_loopback_test"
    "$TEST_DIR/xmail_${protocol}_loopback_test"
done

printf 'Running Linux async task runtime tests...\n'
"$XL" run "$ROOT/release/examples/async_task_wait.xl"
"$XL" run "$ROOT/release/examples/task_api.xl"

printf 'Running Linux TCP file fast-path tests...\n'
"$XL" cgen "$ROOT/test/cases/stdlib_net_send_file.xl" \
    --emit "$TEST_DIR/stdlib_net_send_file.c"
grep -q 'xrtNetStreamSendFile' "$TEST_DIR/stdlib_net_send_file.c"
"$XL" cgen "$ROOT/test/cases/stdlib_net_extended_io.xl" \
    --emit "$TEST_DIR/stdlib_net_extended_io_trim.c"
if grep -Eq 'XRT_MODULE_NET_TCP_FILE|xrtNetStreamSendFile' "$TEST_DIR/stdlib_net_extended_io_trim.c"; then
    printf 'ordinary TCP source unexpectedly keeps TCP file module.\n' >&2
    exit 1
fi
"$XL" run "$ROOT/test/cases/stdlib_net_send_file.xl"
"$XL" build "$ROOT/test/cases/stdlib_net_send_file.xl" \
    -o "$TEST_DIR/stdlib_net_send_file"
"$TEST_DIR/stdlib_net_send_file"

printf 'Running Linux network interface snapshot tests...\n'
"$XL" cgen "$ROOT/test/cases/stdlib_net_interfaces.xl" \
    --emit "$TEST_DIR/stdlib_net_interfaces.c"
grep -q 'xrtNetInterfaceList(' \
    "$TEST_DIR/stdlib_net_interfaces.c"
if grep -q 'XRT_MODULE_NET_INTERFACE_TEXT' "$TEST_DIR/stdlib_net_interfaces.c"; then
    printf 'raw interface enumeration unexpectedly keeps interface text helpers.\n' >&2
    exit 1
fi
if grep -q 'xl6_net_engine' "$TEST_DIR/stdlib_net_interfaces.c"; then
    printf 'raw interface enumeration unexpectedly creates a network engine.\n' >&2
    exit 1
fi
if grep -q 'xl6_net_resolver' "$TEST_DIR/stdlib_net_interfaces.c"; then
    printf 'raw interface enumeration unexpectedly creates a DNS resolver.\n' >&2
    exit 1
fi
"$XL" run "$ROOT/test/cases/stdlib_net_interfaces.xl"
"$XL" build "$ROOT/test/cases/stdlib_net_interfaces.xl" \
    -o "$TEST_DIR/stdlib_net_interfaces"
"$TEST_DIR/stdlib_net_interfaces"

printf 'Running Linux HTTP and WebSocket fast-path tests...\n'
for case_name in \
    stdlib_http1_core \
    stdlib_websocket_core \
    stdlib_websocket_upgrade_loopback \
    stdlib_websocket_upgrade_tls_loopback
do
    "$XL" run "$ROOT/test/cases/$case_name.xl"
    "$XL" build "$ROOT/test/cases/$case_name.xl" \
        -o "$TEST_DIR/$case_name"
    "$TEST_DIR/$case_name"
done

printf 'Running Linux shared-library host API tests...\n'
"${CC:-gcc}" -std=c11 -Wall -I"$ROOT/release" \
    "$ROOT/release/examples/xl_dll_embed_string.c" \
    -L"$ROOT/release" -lxl -pthread -ldl -lm \
    -o "$TEST_DIR/xl_embed_host"
LD_LIBRARY_PATH="$ROOT/release${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
    "$TEST_DIR/xl_embed_host"

"${CC:-gcc}" -std=c11 -Wall -I"$ROOT/release" \
    "$ROOT/release/examples/xlc_api_embed.c" \
    -L"$ROOT/release" -lxlc -pthread -ldl -lm \
    -o "$TEST_DIR/xlc_embed_host"
LD_LIBRARY_PATH="$ROOT/release${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
    "$TEST_DIR/xlc_embed_host"

bash "$ROOT/tools/check_stdlib_time_calendar.sh" \
    "$TEST_DIR/time-calendar-$(date +%Y%m%d-%H%M%S)-$$"
bash "$ROOT/tools/check_stdlib_time_text.sh" \
    "$TEST_DIR/time-text-$(date +%Y%m%d-%H%M%S)-$$"
bash "$ROOT/tools/check_stdlib_environment.sh" \
    "$TEST_DIR/environment-$(date +%Y%m%d-%H%M%S)-$$"
bash "$ROOT/tools/check_stdlib_signal_core.sh" \
    "$TEST_DIR/signal-core-$(date +%Y%m%d-%H%M%S)-$$"
bash "$ROOT/tools/check_stdlib_signal_queue.sh" \
    "$TEST_DIR/signal-queue-$(date +%Y%m%d-%H%M%S)-$$"
bash "$ROOT/tools/check_stdlib_process_run.sh" \
    "$TEST_DIR/process-run-$(date +%Y%m%d-%H%M%S)-$$"
bash "$ROOT/tools/check_stdlib_process_future_module.sh" \
    "$TEST_DIR/process-future-module-$(date +%Y%m%d-%H%M%S)-$$"
bash "$ROOT/tools/check_xlir_nullable_getter.sh" \
    "$TEST_DIR/nullable-getter-$(date +%Y%m%d-%H%M%S)-$$"
bash "$ROOT/tools/check_xlir_safe_index.sh" \
    "$TEST_DIR/safe-index-$(date +%Y%m%d-%H%M%S)-$$"
bash "$ROOT/tools/check_xlir_future_cancel_core.sh" \
    "$TEST_DIR/future-cancel-$(date +%Y%m%d-%H%M%S)-$$"

printf 'DEMO6 Linux compatibility tests passed.\n'
