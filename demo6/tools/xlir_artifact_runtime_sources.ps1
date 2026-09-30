# Runtime closure for standalone AOT hosts that include generated XLIR C.
# JIT can resolve these resident entry points from xl.dll; a packaged
# executable must compile its own copies into the artifact.
function Get-XlirArtifactRuntimeSources {
    @(
        'src/runtime/xl6_ownership_plan.c',
        'src/runtime/xl6_code_lease.c',
        'src/runtime/xl6_closure.c',
        'src/runtime/xl6_cell.c',
        'src/runtime/xl6_slot.c',
        'src/runtime/xl6_frame_storage.c',
        'src/runtime/xl6_continuation_frame.c',
        'src/runtime/xl6_future_wait.c',
        'src/runtime/xl6_task.c',
        'src/runtime/xl6_capture.c',
        'src/runtime/xl6_value_iterator.c',
        'src/runtime/xl6_future_callback.c',
        'src/runtime/xl6_generator.c',
        'src/runtime/xl6_code_finalizer.c',
        'src/runtime/xl6_continuation_queue.c',
        'src/runtime/xl6_continuation_executor.c',
        'src/runtime/xl6_continuation_future.c',
        'src/runtime/xl6_future_payload.c'
    )
}
