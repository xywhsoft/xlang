# CLI failure and native retirement

The CLI keeps the normal retirement wait after a successful entry, including
programs that deliberately return a nonzero exit code. Detached accepted work
keeps its previous successful-run lifetime behavior.

If checked execution itself fails, `xl run` reports that original error and
performs one nonwaiting `xlRuntimeDrainRetiringChecked(0)` pass. A manual
creator-thread frame, lock or similar obligation cannot be resumed by an entry
that has already failed; indefinitely polling it prevented the CLI from exiting.
Incomplete/failed retirement is reported and the process returns failure.

This changes only CLI exit waiting. It does not force native resource destruction,
unlock objects, discard code references or alter the embedding API's checked
recovery and explicit drain contracts. Individual cleanup callbacks still execute
according to their native contracts; the CLI does not preempt blocking callbacks.

`tools/check_cli_busy_error_exit.py` rebuilds O0/O2 shells against a hash-verified
complete compiler runtime. Its CPU-only native-owner fixture reproduces the old
hang under a bounded timeout, then checks repeated failed exits, original error
text, incomplete-retirement reporting, ordinary errors, successful zero/nonzero
returns and the exact normal/error wait selection. Compiler/runtime/VFS libraries
and workspace release files remain unchanged by this gate.
