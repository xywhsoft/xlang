#!/usr/bin/env bash
# Native POSIX validation of generated filesystem C, plus optional host coverage.
set -euo pipefail
task_fixtures=(stdlib_vfs_memory stdlib_vfs_overlay stdlib_vfs_disk
    stdlib_vfs_limits stdlib_vfs_minimal stdlib_vfs_pack stdlib_vfs_pack_limits
    stdlib_vfs_lifetime stdlib_fs_handles stdlib_fs_native stdlib_fs_resources
    stdlib_fs_root stdlib_fs_allocation stdlib_fs_path stdlib_fs_whole
    stdlib_file_bytes_only stdlib_file_read_text_binary_reject
    stdlib_fs_path_whole_allocation stdlib_fs_platform stdlib_fs_tree stdlib_fs_temp stdlib_fs_tree_temp_allocation
    stdlib_vfs_pack_full stdlib_vfs_pack_allocation stdlib_fs_names
    stdlib_fs_walk stdlib_fs_glob_only stdlib_fs_walk_allocation xlir_bytes_dynamic_cast xlir_directory_bridge
    stdlib_file_async stdlib_fs_async_minimal stdlib_fs_async_retirement stdlib_fs_async_cancel stdlib_fs_async_allocation stdlib_fs_task_isolation stdlib_path_random stdlib_fs_metadata_times)
task_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$task_root"
task_out="$task_root/build/test/fs-posix"
mkdir -p "$task_out"
# Real host coverage deliberately runs without a global TMPDIR override: the
# file tests need native Linux temporary directories, not DrvFS rename semantics.
if [[ "${1:-}" == --host-only ]]; then
    task_out="${2:-$task_root/build/acceptance/fs-linux-host}"
    [[ ! -e "$task_out" ]] || { echo 'Use a fresh evidence directory'; exit 1; }
    mkdir -p "$task_out"
    task_out="$(cd "$task_out" && pwd)"
    task_work="$task_out/work"
    mkdir -p "$task_work/build/test"
    test -x release/xl || { echo 'Build the Linux host first'; exit 1; }
    for task_fixture in vfs-disk.bin vfs-pack.bin vfs-pack-corrupt.bin vfs-pack-slice.bin vfs-pack-full-1.bin vfs-pack-full-2.bin; do
        test -f "build/test/$task_fixture" || { echo "Run check_stdlib_vfs.ps1 first: missing $task_fixture"; exit 1; }
        cp "build/test/$task_fixture" "$task_work/build/test/$task_fixture"
    done
    printf 'case\tjit_exit\tbuild_exit\taot_exit\n' > "$task_out/results.tsv"
    task_failed=0
    for task_case in "${task_fixtures[@]}"; do
        task_jit=0; task_build=0; task_aot=125
        # Directory fixtures enumerate build/test. Keep their data away from
        # concurrent compiler gates deleting/replacing files in the shared tree.
        (cd "$task_work" && timeout 60 "$task_root/release/xl" run "$task_root/test/cases/$task_case.xl") > "$task_out/$task_case.jit.log" 2>&1 || task_jit=$?
        (cd "$task_work" && timeout 60 "$task_root/release/xl" build "$task_root/test/cases/$task_case.xl" -o "$task_out/$task_case") > "$task_out/$task_case.build.log" 2>&1 || task_build=$?
        if [[ "$task_build" == 0 ]]; then
            task_aot=0
            (cd "$task_work" && timeout 60 "$task_out/$task_case") > "$task_out/$task_case.aot.log" 2>&1 || task_aot=$?
        fi
        printf '%s\t%s\t%s\t%s\n' "$task_case" "$task_jit" "$task_build" "$task_aot" >> "$task_out/results.tsv"
        printf '%s: JIT=%s build=%s AOT=%s\n' "$task_case" "$task_jit" "$task_build" "$task_aot"
        if [[ "$task_jit" != 0 || "$task_build" != 0 || "$task_aot" != 0 ]]; then task_failed=1; fi
    done
    if [[ "$task_failed" != 0 ]]; then echo 'Linux host filesystem failures retained'; exit 1; fi
    echo 'Linux filesystem TinyCC JIT and standalone ELF AOT gate passed'
    exit 0
fi

task_flags=(-std=c11 -D_GNU_SOURCE -O1 -ffunction-sections -fdata-sections
    -I. -Isrc -Irelease/include -Ibuild/test)
task_sources=(
    src/runtime/xl6_ownership_plan.c src/runtime/xl6_code_lease.c
    src/runtime/xl6_closure.c src/runtime/xl6_cell.c src/runtime/xl6_slot.c
    src/runtime/xl6_frame_storage.c src/runtime/xl6_continuation_frame.c
    src/runtime/xl6_future_wait.c src/runtime/xl6_task.c src/runtime/xl6_task_pool.c src/runtime/xl6_task_file.c src/runtime/xl6_capture.c
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
gcc "${task_flags[@]}" "${task_strict[@]}" test/cases/xlir_fs_async_resident.c \
    "${task_objects[@]}" -Wl,--gc-sections -pthread -ldl -lm -o "$task_out/async-resident"
timeout 60 "$task_out/async-resident"
if [[ "${1:-}" == --resident-only ]]; then exit 0; fi
for task_case in "${task_fixtures[@]}"; do
    task_generated="$task_root/build/test/$task_case.c"
    test -f "$task_generated" || { echo "Run check_stdlib_vfs.ps1 first: missing $task_generated"; exit 1; }
    task_host=test/cases/xlir_generated_module_host.c
    case "$task_case" in
        stdlib_file_async|stdlib_fs_async_minimal|stdlib_fs_async_retirement|stdlib_fs_async_cancel|stdlib_fs_async_allocation|stdlib_fs_task_isolation)
            task_host=test/cases/xlir_fs_async_module_host.c;;
    esac
    gcc "${task_flags[@]}" "${task_strict[@]}" \
        "-DXLIR_GENERATED_MODULE=\"$task_generated\"" \
        "$task_host" "${task_objects[@]}" \
        -Wl,--gc-sections -pthread -ldl -lm -o "$task_out/$task_case"
    timeout 60 "$task_out/$task_case"
    if [[ "$task_case" == stdlib_fs_async_allocation ]]; then
        # The module host has joined native cleanup before removing trial files.
        for task_point in {0..79}; do
            rm -f -- "$task_root/build/test/fs-async-allocation.bin.atomic.$task_point"
        done
    fi
    if [[ "$task_case" == stdlib_fs_names ]]; then
        python3 - "$task_out/$task_case" <<'PY'
import os, subprocess, sys, tempfile
with tempfile.TemporaryDirectory(prefix='xl-fs-names-') as directory:
    with open(os.fsencode(directory) + b'/\xff', 'wb') as out:
        out.write(b'bad utf8 filename')
    subprocess.run([sys.argv[1]], env=dict(os.environ, XL_FS_INVALID_DIR=directory),
        check=True, timeout=60)
PY
    fi
    printf '%s: native POSIX strict generated C passed\n' "$task_case"
done
echo 'Native POSIX filesystem gate passed; this does not claim Linux TinyCC JIT coverage.'
