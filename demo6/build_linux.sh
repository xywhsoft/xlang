#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [[ "$(uname -m)" != "x86_64" ]]; then
    echo "error: demo6 Linux RC currently supports x86_64 only" >&2
    exit 1
fi

if ! getconf GNU_LIBC_VERSION >/dev/null 2>&1; then
    echo "error: demo6 Linux RC currently requires a glibc-based system" >&2
    exit 1
fi

BUILD_DIR="$ROOT/build/linux"
TCC_SOURCE_DIR="$BUILD_DIR/tcc-source"
TCC_DIST_DIR="$BUILD_DIR/tcc-dist"
PACK_TOOL="$BUILD_DIR/tcc_vfs_lzma_pack"
CC="${CC:-gcc}"
MAKE="${MAKE:-make}"
PYTHON="${PYTHON:-python3}"
JOBS="${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || printf '2')}"

require_tool() {
    if ! command -v "$1" >/dev/null 2>&1; then
        printf 'DEMO6 Linux build failed: required tool "%s" was not found in PATH.\n' "$1" >&2
        exit 1
    fi
}

require_tool "$CC"
require_tool "$MAKE"
require_tool "$PYTHON"

mkdir -p "$BUILD_DIR" "$TCC_DIST_DIR/include" "$TCC_DIST_DIR/lib" "$ROOT/release"

"$PYTHON" "$ROOT/tools/sync_xrt_snapshot.py" --root "$ROOT" --check
"$PYTHON" "$ROOT/tools/generate_stdlib_index.py" --root "$ROOT"
"$PYTHON" "$ROOT/tools/gen_xrt_symbol_table.py" --root "$ROOT"

printf 'Preparing native Linux TCC...\n'
rm -rf "$TCC_SOURCE_DIR"
cp -a "$ROOT/lib/tcc" "$TCC_SOURCE_DIR"
sed -i 's/\r$//' "$TCC_SOURCE_DIR/configure"
chmod +x "$TCC_SOURCE_DIR/configure"
(
    cd "$TCC_SOURCE_DIR"
    ./configure --cc="$CC" --prefix="$TCC_DIST_DIR" --tccdir="$TCC_DIST_DIR"
    # Vendored TCC is not its own Git checkout. Do not stamp the parent xlang
    # branch/date or scan its unrelated dirty tree into the compiler binary.
    "$MAKE" -j"$JOBS" GITHASH=no tcc libtcc1.a
)
cp -a "$TCC_SOURCE_DIR/include/." "$TCC_DIST_DIR/include/"
cp "$TCC_SOURCE_DIR/libtcc1.a" "$TCC_DIST_DIR/libtcc1.a"
cp "$TCC_SOURCE_DIR/libtcc1.a" "$TCC_DIST_DIR/lib/libtcc1.a"
cp "$TCC_SOURCE_DIR/tcc" "$BUILD_DIR/tcc"

# Propagate the configured target layout to libtcc embedded in user artifacts.
# The source snapshot has only generic defaults; losing CONFIG_TRIPLET breaks
# nested compilation on multiarch glibc systems. An empty triplet is valid on
# systems whose headers and startup objects use the non-multiarch layout.
TCC_CONFIGURED_TRIPLET="$(sed -n 's/^#define CONFIG_TRIPLET "\([^"]*\)"$/\1/p' "$TCC_SOURCE_DIR/config.h")"
if [[ -n "$TCC_CONFIGURED_TRIPLET" && ! "$TCC_CONFIGURED_TRIPLET" =~ ^[A-Za-z0-9_.-]+$ ]]; then
    echo 'error: unexpected configured TCC target triplet' >&2
    exit 1
fi

printf 'Building Linux XRT object and VFS packer...\n'
"$CC" -std=c11 -O2 -fPIC -I"$ROOT" \
    -c "$ROOT/tools/xrt_single_header_impl.c" \
    -o "$BUILD_DIR/xrt.o"

"$CC" -std=c11 -O2 -DZ7_ST -I"$ROOT/lib/lzma" \
    "$ROOT/tools/tcc_vfs_lzma_pack.c" \
    "$ROOT/lib/lzma/LzmaEnc.c" \
    "$ROOT/lib/lzma/LzFind.c" \
    "$ROOT/lib/lzma/CpuArch.c" \
    -o "$PACK_TOOL"

printf 'Generating Linux embedded TCC resources...\n'
"$PYTHON" "$ROOT/tools/gen_tcc_vfs_resources.py" \
    --root "$ROOT" \
    --manifest "$ROOT/tools/tcc_vfs_manifest_linux_runtime.txt" \
    --output "$BUILD_DIR/tcc_builtin_resources_runtime.c" \
    --tcc-dist "$TCC_DIST_DIR" \
    --xrt-obj "$BUILD_DIR/xrt.o" \
    --pack-tool "$PACK_TOOL"

"$PYTHON" "$ROOT/tools/gen_tcc_vfs_resources.py" \
    --root "$ROOT" \
    --manifest "$ROOT/tools/tcc_vfs_manifest_linux_xlc.txt" \
    --output "$BUILD_DIR/tcc_builtin_resources.c" \
    --tcc-dist "$TCC_DIST_DIR" \
    --xrt-obj "$BUILD_DIR/xrt.o" \
    --pack-tool "$PACK_TOOL"

COMMON_FLAGS=(
    -std=c11
    -D_GNU_SOURCE
    -Wall
    -Wno-pointer-sign
    -Wno-unused-function
    -Wno-unused-variable
    -Wno-unused-but-set-variable
    -Wno-format-truncation
    -Os
    -fPIC
    -ffunction-sections
    -fdata-sections
	-DXL_BUILD_DLL
	-DXL_STATIC_TCC
	-DLIBTCC_AS_DLL
	-DCONFIG_TCC_BUILTIN_VFS=1
    -I"$ROOT"
    -I"$ROOT/release"
    -I"$ROOT/release/include"
    -I"$ROOT/src"
    -I"$ROOT/lib/lzma"
    -I"$TCC_SOURCE_DIR"
)
if [[ -n "$TCC_CONFIGURED_TRIPLET" ]]; then
    COMMON_FLAGS+=("-DXL6_TCC_CONFIGURED_TRIPLET=\"$TCC_CONFIGURED_TRIPLET\"")
fi
SHARED_FLAGS=(-shared -s -Wl,--gc-sections -Wl,-z,defs)
SYSTEM_LIBS=(-pthread -ldl -lm)

TCC_SOURCES=(
    "$TCC_SOURCE_DIR/libtcc.c"
    "$TCC_SOURCE_DIR/tcctools.c"
    "$TCC_SOURCE_DIR/tcc_builtin_vfs.c"
    "$ROOT/lib/lzma/LzmaDec.c"
)
RUNTIME_SOURCES=(
	"$ROOT/src/api/xl_api.c"
	"$ROOT/src/api/xl_api_symbols.c"
	"$ROOT/src/api/xl_api_path.c"
	"$ROOT/src/api/xl_protocol_registry.c"
	"$ROOT/src/api/xl_channel_bridge.c"
	"$ROOT/src/api/xl_future_bridge.c"
	"$ROOT/src/api/xl_task_group_bridge.c"
	"$ROOT/src/runtime/xl6_continuation_queue.c"
	"$ROOT/src/runtime/xl6_continuation_executor.c"
	"$ROOT/src/runtime/xl6_continuation_future.c"
	"$ROOT/src/runtime/xl6_code_lease.c"
	"$ROOT/src/runtime/xl6_closure.c"
	"$ROOT/src/runtime/xl6_cell.c"
	"$ROOT/src/runtime/xl6_slot.c"
	"$ROOT/src/runtime/xl6_frame_storage.c"
	"$ROOT/src/runtime/xl6_task.c"
	"$ROOT/src/runtime/xl6_task_file.c"
	"$ROOT/src/runtime/xl6_text_value.c"
	"$ROOT/src/runtime/xl6_text_query.c"
	"$ROOT/src/runtime/xl6_text_binary.c"
	"$ROOT/src/runtime/xl6_buffer_value.c"
	"$ROOT/src/runtime/xl6_buffer_iterator.c"
	"$ROOT/src/runtime/xl6_buffer_hex.c"
	"$ROOT/src/runtime/xl6_buffer_base64.c"
	"$ROOT/src/runtime/xl6_buffer_core.c"
	"$ROOT/src/runtime/xl6_buffer_query.c"
	"$ROOT/src/runtime/xl6_stack.c"
	"$ROOT/src/runtime/xl6_sequence.c"
	"$ROOT/src/runtime/xl6_array.c"
	"$ROOT/src/runtime/xl6_array_iterator.c"
    "$ROOT/src/runtime/xl6_nullable_bool.c"
    "$ROOT/src/runtime/xl6_nullable_i64.c"
    "$ROOT/src/runtime/xl6_nullable_u64.c"
    "$ROOT/src/runtime/xl6_nullable_f32.c"
    "$ROOT/src/runtime/xl6_nullable_f64.c"
    "$ROOT/src/runtime/xl6_nullable_time.c"
    "$ROOT/src/runtime/xl6_nullable_char.c"
    "$ROOT/src/runtime/xl6_nullable_bool_value.c"
    "$ROOT/src/runtime/xl6_nullable_i64_value.c"
    "$ROOT/src/runtime/xl6_nullable_u64_value.c"
    "$ROOT/src/runtime/xl6_nullable_f32_value.c"
    "$ROOT/src/runtime/xl6_nullable_f64_value.c"
    "$ROOT/src/runtime/xl6_nullable_time_value.c"
    "$ROOT/src/runtime/xl6_nullable_char_value.c"
    "$ROOT/src/runtime/xl6_nullable_string.c"
    "$ROOT/src/runtime/xl6_string_type.c"
    "$ROOT/src/runtime/xl6_string_value.c"
    "$ROOT/src/runtime/xl6_array_value.c"
    "$ROOT/src/runtime/xl6_char_type.c"
    "$ROOT/src/runtime/xl6_char_value.c"
	"$ROOT/src/runtime/xl6_text_chars.c"
	"$ROOT/src/runtime/xl6_text_glob.c"
	"$ROOT/src/runtime/xl6_text_distance.c"
	"$ROOT/src/runtime/xl6_text_split.c"
	"$ROOT/src/runtime/xl6_text_join.c"
	"$ROOT/src/runtime/xl6_text_iterator.c"
	"$ROOT/src/runtime/xl6_number_format.c"
	"$ROOT/src/runtime/xl6_number_format_buffer.c"
	"$ROOT/src/runtime/xl6_number_integer.c"
	"$ROOT/src/runtime/xl6_number_float.c"
	"$ROOT/src/runtime/xl6_number_integer_buffer.c"
	"$ROOT/src/runtime/xl6_number_float_buffer.c"
	"$ROOT/src/runtime/xl6_unicode.c"
	"$ROOT/src/runtime/xl6_unicode_validator.c"
	"$ROOT/src/runtime/xl6_html.c"
	"$ROOT/src/runtime/xl6_encoding.c"
	"$ROOT/src/runtime/xl6_encoding_meta.c"
	"$ROOT/src/runtime/xl6_encoding_detect.c"
	"$ROOT/src/runtime/xl6_regex_core.c"
	"$ROOT/src/runtime/xl6_regex_match.c"
	"$ROOT/src/runtime/xl6_regex_iterator.c"
	"$ROOT/src/runtime/xl6_regex_replace.c"
	"$ROOT/src/runtime/xl6_regex_replace_buffer.c"
	"$ROOT/src/runtime/xl6_regex_split.c"
	"$ROOT/src/runtime/xl6_regex_set.c"
	"$ROOT/src/runtime/xl6_regex_replace_func.c"
	"$ROOT/src/runtime/xl6_regex_replace_func_buffer.c"
	"$ROOT/src/runtime/xl6_pattern_core.c"
	"$ROOT/src/runtime/xl6_pattern_builder.c"
	"$ROOT/src/runtime/xl6_time_core.c"
	"$ROOT/src/runtime/xl6_time_datetime.c"
	"$ROOT/src/runtime/xl6_time_local.c"
	"$ROOT/src/runtime/xl6_time_text.c"
	"$ROOT/src/runtime/xl6_time_datetime_text.c"
	"$ROOT/src/runtime/xl6_environment.c"
	"$ROOT/src/runtime/xl6_signal_core.c"
	"$ROOT/src/runtime/xl6_signal_queue.c"
	"$ROOT/src/runtime/xl6_signal_task_group.c"
	"$ROOT/src/runtime/xl6_process_command.c"
	"$ROOT/src/runtime/xl6_process_options.c"
	"$ROOT/src/runtime/xl6_process_run.c"
	"$ROOT/src/runtime/xl6_process_status.c"
	"$ROOT/src/runtime/xl6_process_open.c"
	"$ROOT/src/runtime/xl6_process_terminal.c"
	"$ROOT/src/runtime/xl6_process_live.c"
	"$ROOT/src/runtime/xl6_native_handle.c"
	"$ROOT/src/runtime/xl6_process_stream.c"
	"$ROOT/src/runtime/xl6_process_future.c"
	"$ROOT/src/runtime/xl6_process_pipeline_config.c"
	"$ROOT/src/runtime/xl6_process_pipeline.c"
	"$ROOT/src/runtime/xl6_process_file.c"
	"$ROOT/src/runtime/xl6_process_terminal_config.c"
	"$ROOT/src/runtime/xl6_process_terminal_live.c"
	"$ROOT/src/runtime/xl6_pattern_options.c"
	"$ROOT/src/runtime/xl6_pattern_match.c"
	"$ROOT/src/runtime/xl6_network_service.c"
	"$ROOT/src/runtime/xl6_log.c"
	"$ROOT/src/runtime/xl6_log_console.c"
	"$ROOT/src/runtime/xl6_log_record.c"
	"$ROOT/src/runtime/xl6_log_text.c"
	"$ROOT/src/runtime/xl6_log_json.c"
	"$ROOT/src/runtime/xl6_log_custom.c"
	"$ROOT/src/runtime/xl6_log_file.c"
	"$ROOT/src/runtime/xl6_log_file_text.c"
	"$ROOT/src/runtime/xl6_log_file_json.c"
	"$ROOT/src/runtime/xl6_log_async.c"
	"$ROOT/src/runtime/xl6_log_ring.c"
	"$ROOT/src/runtime/xl6_log_printf.c"
	"$ROOT/src/runtime/xl6_log_text_writer.c"
	"$ROOT/src/runtime/xl6_log_json_writer.c"
	"$ROOT/src/runtime/xl6_continuation_frame.c" "$ROOT/src/runtime/xl6_future_wait.c"
	"$ROOT/src/runtime/xl6_capture.c"
	"$ROOT/src/runtime/xl6_value_iterator.c"
	"$ROOT/src/runtime/xl6_future_callback.c"
	"$ROOT/src/runtime/xl6_ownership_plan.c"
	"$ROOT/src/runtime/xl6_generator.c"
	"$ROOT/src/runtime/xl6_code_finalizer.c"
	"$ROOT/src/runtime/xl6_future_payload.c"
	"$ROOT/src/api/xl_xrt_dll.c"
	"$ROOT/src/abi/xl6_abi_json.c"
	"$ROOT/src/abi/xl6_callable.c"
	"$ROOT/src/backend/xl6_build_backend.c"
    "$ROOT/src/compile/xl6_compile_unit.c"
    "$ROOT/src/compile/xl6_imports.c"
    "$ROOT/src/compile/xl6_xrt_trim.c"
    "$ROOT/src/common/xl6_console.c"
    "$ROOT/src/common/xl6_output.c"
    "$ROOT/src/diag/xl6_diag.c"
    "$ROOT/src/ast/xl6_ast.c"
  "$ROOT/src/ir/xl6_ir.c"
  "$ROOT/src/ir/xl6_ir_lower.c"
    "$ROOT/src/cgen/xl6_cgen_shared_runtime.c"
    "$ROOT/src/cgen/xl6_ir_cgen.c"
    "$ROOT/src/lexer/xl6_token.c"
    "$ROOT/src/lexer/xl6_lexer_api.c"
    "$ROOT/src/lexer/xl6_lexer_gen.c"
    "$ROOT/src/parser/xl6_parser.c"
    "$ROOT/src/parser/xl6_parser_api.c"
    "$ROOT/src/package/xl6_package.c"
    "$ROOT/src/stdlib/xl6_stdlib.c"
    "$ROOT/src/stdlib/xl6_stdlib_data.c"
    "$ROOT/src/type/xl6_type_model.c"
    "$ROOT/src/sem/xl6_protocol.c"
    "$ROOT/src/sem/xl6_type_registry.c"
    "$ROOT/src/sem/xl6_sem_docs.c"
    "$ROOT/src/sem/xl6_sem_field_attrs.c"
    "$ROOT/src/sem/xl6_sem.c"
)
COMPILER_ONLY_SOURCES=(
    "$ROOT/src/abi/xl6_abi_dump.c"
    "$ROOT/src/ast/xl6_ast_dump.c"
    "$ROOT/src/ir/xl6_ir_dump.c"
    "$ROOT/src/resource/xl6_win_resource.c"
)

printf 'Building libxl.so...\n'
"$CC" "${COMMON_FLAGS[@]}" "${SHARED_FLAGS[@]}" \
    -DXL_RUNTIME_ONLY_DLL \
    "${RUNTIME_SOURCES[@]}" \
    "${TCC_SOURCES[@]}" \
    "$BUILD_DIR/tcc_builtin_resources_runtime.c" \
    "${SYSTEM_LIBS[@]}" \
    -Wl,-soname,libxl.so \
    -o "$ROOT/release/libxl.so"

printf 'Building libxlc.so...\n'
"$CC" "${COMMON_FLAGS[@]}" "${SHARED_FLAGS[@]}" \
    "${RUNTIME_SOURCES[@]}" \
    "${COMPILER_ONLY_SOURCES[@]}" \
    "${TCC_SOURCES[@]}" \
    "$BUILD_DIR/tcc_builtin_resources.c" \
    "${SYSTEM_LIBS[@]}" \
    -Wl,-soname,libxlc.so \
    -o "$ROOT/release/libxlc.so"

printf 'Building xl command line tool...\n'
"$CC" -std=c11 -D_GNU_SOURCE -Wall -Os -s -I"$ROOT" -I"$ROOT/release" \
    "$ROOT/src/driver/xl6_xlc_shell.c" \
    "$ROOT/src/common/xl6_console.c" \
    -L"$ROOT/release" -lxlc \
    -Wl,-rpath,'$ORIGIN' \
    -pthread -ldl -lm \
    -o "$ROOT/release/xl"

printf 'DEMO6 Linux build ok: release/xl release/libxl.so release/libxlc.so\n'
