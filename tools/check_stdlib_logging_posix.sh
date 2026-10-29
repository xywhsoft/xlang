#!/usr/bin/env bash
# Current Linux host, strict generated C, JIT and independently linked ELF.
set -euo pipefail
task_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$task_root"
task_out="${1:-$task_root/build/test/logging-posix-$(date +%Y%m%d-%H%M%S)}"
[[ ! -e "$task_out" ]] || { echo 'Choose a fresh evidence directory'; exit 1; }
mkdir -p "$task_out/work/build/test" "$task_out/include/__xl_runtime"
task_out="$(cd "$task_out" && pwd)"
task_work="$task_out/work"
test -x release/xl
cp src/runtime/xl6_*.h "$task_out/include/__xl_runtime/"
cp release/include/xl_runtime.h release/include/xl_extlib_abi.h \
    release/include/xrt_decl.h release/include/xruntime_decl.h "$task_out/include/__xl_runtime/"
task_flags=(-std=c11 -D_GNU_SOURCE -Wall -Wextra -Werror -Wno-unused-function
    -Wno-unused-variable -Wno-unused-parameter -Wno-unused-label
    -Wno-unused-but-set-variable -Wno-missing-field-initializers
    -I. -Irelease/include "-I$task_out/include")
task_libs=("-L$task_root/release" -lxl "-Wl,-rpath,$task_root/release" -pthread -ldl -lm)
task_cases=(stdlib_log_core stdlib_log_console stdlib_log_global_only stdlib_log_records
    stdlib_log_callbacks stdlib_log_queues stdlib_log_writers stdlib_log_printf
    stdlib_log_files stdlib_log_vfs stdlib_log_default_cleanup stdlib_log_exit
    stdlib_log stdlib_log_object stdlib_log_async_failure)
# Evidence digest includes the actual Linux compiler, ELF host/runtime, source
# inputs and fixture batch. Concurrent unrelated source edits invalidate it.
find src -type f -print0 | sort -z | xargs -0 sha256sum > "$task_out/inputs.sha256"
sha256sum release/xl release/libxl.so release/libxlc.so "$0" "$(command -v gcc)" >> "$task_out/inputs.sha256"
for task_case in "${task_cases[@]}"; do sha256sum "test/cases/$task_case.xl" >> "$task_out/inputs.sha256"; done
sha256sum test/cases/stdlib_log_cleanup_failure.xl test/cases/stdlib_log_cleanup_failure_host.c \
    release/examples/stdlib_log_formats.xl lib/xruntime/xruntime.h lib/xrt/xrt.h >> "$task_out/inputs.sha256"
printf 'fixture\tstrict\tjit\taot\n' > "$task_out/results.tsv"
for task_case in "${task_cases[@]}"; do
    task_source="$task_root/test/cases/$task_case.xl"
    timeout 45 release/xl cgen "$task_source" --emit "$task_out/$task_case.c" > "$task_out/$task_case.cgen.log" 2>&1
    task_entry=()
    if [[ "$task_case" == stdlib_log_exit ]]; then task_entry=(-DXLIR_LOG_USER_ENTRY); fi
    gcc "${task_flags[@]}" -include "$task_out/$task_case.c" "${task_entry[@]}" \
        test/cases/stdlib_log_generated_host.c "${task_libs[@]}" -o "$task_out/$task_case.strict" \
        > "$task_out/$task_case.strict-build.log" 2>&1
    (cd "$task_work" && timeout 45 "$task_out/$task_case.strict") > "$task_out/$task_case.strict.out" 2> "$task_out/$task_case.strict.err"
    (cd "$task_work" && timeout 45 "$task_root/release/xl" run "$task_source") > "$task_out/$task_case.jit.out" 2> "$task_out/$task_case.jit.err"
    (cd "$task_work" && timeout 45 "$task_root/release/xl" build "$task_source" -o "$task_out/$task_case.aot") > "$task_out/$task_case.aot-build.log" 2>&1
    (cd "$task_work" && timeout 45 "$task_out/$task_case.aot") > "$task_out/$task_case.aot.out" 2> "$task_out/$task_case.aot.err"
    cmp "$task_out/$task_case.strict.out" "$task_out/$task_case.jit.out"
    cmp "$task_out/$task_case.jit.out" "$task_out/$task_case.aot.out"
    printf '%s\t0\t0\t0\n' "$task_case" >> "$task_out/results.tsv"
    printf '%s: strict C/JIT/ELF AOT passed\n' "$task_case"
done
printf 'before exit\nfinally\n' > "$task_out/exit.expected"
cmp "$task_out/exit.expected" "$task_work/build/test/log-complete-exit.txt"
task_failure="$task_root/test/cases/stdlib_log_cleanup_failure.xl"
timeout 45 release/xl cgen "$task_failure" --emit "$task_out/cleanup-failure.c" > "$task_out/cleanup-failure.cgen.log" 2>&1
gcc "${task_flags[@]}" -include "$task_out/cleanup-failure.c" test/cases/stdlib_log_cleanup_failure_host.c \
    "${task_libs[@]}" -o "$task_out/cleanup-failure.strict" > "$task_out/cleanup-failure.strict-build.log" 2>&1
(cd "$task_work" && timeout 45 "$task_out/cleanup-failure.strict") > "$task_out/cleanup-failure.strict.log" 2>&1
if (cd "$task_work" && timeout 45 "$task_root/release/xl" run "$task_failure") > "$task_out/cleanup-failure.jit.log" 2>&1; then
    echo 'Lost module finalizer failure in JIT'; exit 1
fi
grep -F 'final flush failure' "$task_out/cleanup-failure.jit.log" > /dev/null
(cd "$task_work" && timeout 45 "$task_root/release/xl" build "$task_failure" -o "$task_out/cleanup-failure.aot") > "$task_out/cleanup-failure.aot-build.log" 2>&1
if (cd "$task_work" && timeout 45 "$task_out/cleanup-failure.aot") > "$task_out/cleanup-failure.aot.log" 2>&1; then
    echo 'Lost module finalizer failure in ELF'; exit 1
fi
grep -F 'final flush failure' "$task_out/cleanup-failure.aot.log" > /dev/null
task_example="$task_root/release/examples/stdlib_log_formats.xl"
(cd "$task_work" && timeout 45 "$task_root/release/xl" run "$task_example") > "$task_out/formats-example.jit.log" 2>&1
(cd "$task_work" && timeout 45 "$task_root/release/xl" build "$task_example" -o "$task_out/formats-example.aot") > "$task_out/formats-example.aot-build.log" 2>&1
(cd "$task_work" && timeout 45 "$task_out/formats-example.aot") > "$task_out/formats-example.aot.log" 2>&1
sha256sum -c "$task_out/inputs.sha256" > "$task_out/inputs-verified.log"
echo 'Linux x86_64 logging batch passed; not every architecture or terminal backend.'
