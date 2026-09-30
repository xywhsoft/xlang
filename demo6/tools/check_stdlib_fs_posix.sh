#!/usr/bin/env bash
# Native POSIX validation of generated filesystem C; no TinyCC bootstrap required.
set -euo pipefail
task_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$task_root"
task_out="$task_root/build/test/fs-posix"
mkdir -p "$task_out"
task_flags=(-std=c11 -D_GNU_SOURCE -O1 -ffunction-sections -fdata-sections
    -I. -Isrc -Irelease/include -Ibuild/test)
task_sources=(
    src/runtime/xl6_ownership_plan.c src/runtime/xl6_code_lease.c
    src/runtime/xl6_closure.c src/runtime/xl6_cell.c src/runtime/xl6_slot.c
    src/runtime/xl6_frame_storage.c src/runtime/xl6_continuation_frame.c
    src/runtime/xl6_future_wait.c src/runtime/xl6_task.c src/runtime/xl6_capture.c
    src/runtime/xl6_value_iterator.c src/runtime/xl6_future_callback.c
    src/runtime/xl6_generator.c src/runtime/xl6_code_finalizer.c
    src/runtime/xl6_continuation_queue.c src/runtime/xl6_continuation_executor.c
    src/runtime/xl6_continuation_future.c src/runtime/xl6_future_payload.c
)
task_objects=()
for task_source in tools/xrt_single_header_impl.c "${task_sources[@]}"; do
    task_object="$task_out/$(basename "${task_source%.c}").o"
    gcc "${task_flags[@]}" -c "$task_source" -o "$task_object"
    task_objects+=("$task_object")
done
task_strict=(-Wall -Wextra -Werror -Wno-unused-function -Wno-unused-label
    -Wno-unused-variable -Wno-unused-parameter -Wno-unused-const-variable
    -Wno-missing-field-initializers)
task_fixtures=(stdlib_vfs_memory stdlib_vfs_overlay stdlib_vfs_disk
    stdlib_vfs_limits stdlib_vfs_minimal stdlib_vfs_pack stdlib_vfs_pack_limits
    stdlib_vfs_lifetime stdlib_fs_handles stdlib_fs_native stdlib_fs_resources
    stdlib_fs_root stdlib_fs_allocation stdlib_fs_path stdlib_fs_whole
    stdlib_file_bytes_only stdlib_file_read_text_binary_reject
    stdlib_fs_path_whole_allocation)
for task_case in "${task_fixtures[@]}"; do
    task_generated="$task_root/build/test/$task_case.c"
    test -f "$task_generated" || { echo "Run check_stdlib_vfs.ps1 first: missing $task_generated"; exit 1; }
    gcc "${task_flags[@]}" "${task_strict[@]}" \
        "-DXLIR_GENERATED_MODULE=\"$task_generated\"" \
        test/cases/xlir_generated_module_host.c "${task_objects[@]}" \
        -Wl,--gc-sections -pthread -ldl -lm -o "$task_out/$task_case"
    timeout 60 "$task_out/$task_case"
    printf '%s: native POSIX strict generated C passed\n' "$task_case"
done
echo 'Native POSIX filesystem gate passed; this does not claim Linux TinyCC JIT coverage.'
