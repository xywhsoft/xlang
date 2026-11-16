# Replayable XGE value and function contracts

The generator now preserves sixteen previously qualified direct declarations
whose semantics cannot be inferred from an integer result or pointer spelling.
`tools/xge_replay_contracts.py` records their exact physical C declarations and
their public language declarations. A missing/changed native declaration stops
generation before outputs are written. There is no added product C adapter.

The set covers title snapshots/checked setters, provider and audio checked
cleanup, sound fallback, EGL naming, bounded clipboard reads, complete Gamepad
state, five 3D matrix/camera/position/ray calls, and bounded debug output.
Clipboard/debug preserve integer count or BUFFER_TOO_SMALL results rather
than guessing a checked status contract. The native storage review for the
current clipboard branches was renewed; debug output has an explicit review.
Neither review is a live platform clipboard test.

Published supplementary source files own their existing records/functions.
Generation reads the manifest source lists and avoids emitting those bindings
again. This prevents duplicate graphics/text info records and unreviewed free
aliases for Scene, render-pipeline and matrix operations. Legacy unchecked
cleanup and process-global SVG cache calls retain their managed replacements.
Unknown/unpublished files are not treated as declaration owners.

Public integer constants are resolved before native fixed arrays are mapped.
This restores the complete GamepadState eight-axis and TerminalCell sixteen-
combining-slot records, and adds these pointer-free native values:

| Module | Value | Native storage |
| --- | --- | --- |
| xge | TouchEvent | eight inline TouchPoint records |
| xui | PropertyGridCategory | 64 ID bytes, 96 name bytes, expanded/count fields |
| xui | CodeSearchResult | ten inline capture ranges |
| xui | FindResult | ten inline capture ranges |
| xui | CodeDocumentBatchLayout | size, capture identity, document/byte counts |
| xui | CodeDocumentBatchSource | size, length and undo count |

The character arrays are raw C character storage, including NUL and remaining
slots; they are not automatic string snapshots. Batch descriptors are values,
not owners or an implemented batch transaction API. Their operation/lifetime
interfaces remain a separate integration task. Copies own independent inline
storage and keep no borrowed pointers or resource references.

`stage_xge3_replay_contracts.py` compares complete declaration units (including
record field order and every attribute/type/default). It allows these six
explicit value additions, rejects removed/changed units and stages eight
selected source files into isolated matched packages. Optional source layouts
are mapped to installed `xge3d`/`xgeDebug` package names. The full regenerated
XUI managed file remains excluded. Dependency feature metadata is preserved;
this batch does not establish loader enforcement of `requiresNativeFeatures`
or qualify debug execution against a debug-disabled native SDK.

The source gate checks altered native declarations, registry boundaries and
symbolic extents. `check_xge3_replay_values.py` tests C/C++ headers, six value
ABIs using independent C++17 O0/O2 archives, last array slots, embedded NUL,
independent copies, checked indexing and failed output rollback in real JIT,
dynamic and static AOT. The `touch_value.xl` and `search_value.xl` lessons use
plain values and assertions without test fixtures, windows or devices.

For 1000 warm FindResult output calls, language and equivalent C loops each
perform zero XRT allocations, frees or reallocations, including under a zero
allocation budget. This is a heap result for that workload. Independent value
semantics still use inline/stack copies and the existing checked call boundary;
their machine-code cost, CPU equivalence and all cold paths are not claimed.
No native binary, language runtime or compiler implementation was changed by
this batch. Source/library promotion and final shipping installation are
separate, explicitly recorded steps.

The eight selected source files were subsequently promoted after both native
OS verifiers passed: 119 real-product steps per OS (values/title/camera/ray/
Gamepad/TableView), plus 150 Windows and 151 Linux post-promotion source checks.
The pre-promotion replay was also checked but is not counted twice in the
latest 539-step total. Windows symlink creation was privilege-limited; Linux
exercised that rejection. All candidate inputs/logs/program hashes were
verified; the promoted source matches the tested candidate bytes (line endings
normalized when comparing operating systems). Only the full XUI managed
generation output still differs. It has not been applied or accepted.

The runtime candidates reuse the complete O2 compiler Winr45/Linuxr38 cohorts
and the existing matched Flow-config host/static SDKs. Debug declarations are
preserved, but default debug-disabled SDKs were not reported as debug runtime
acceptance. No final `release/`, IDE, command catalog or delivery-index update
was performed by this source batch.

The successor [managed replay batch](xui-managed-replay.md) closes the remaining
managed declaration reproduction gap. Nine selected files now preserve complete
units, including all 3,130 managed units with class membership and multiplicity.
Textual ordering remains different and is recorded separately. Source replay is
qualified; the generated package's nine runtime suites are still running.
Final release/IDE installation is still open.

The 2026-11-17 storage revalidation checks the later HostSession and Chart
additions. Eight reviews had ten stale dependency pins: three implementation
files were identical after UTF-8 line-ending normalization, and the public
header only inserted independent backend/HostSession declarations. Existing
declarations, reviewed function bodies and pointer-storage rules are unchanged.
The review preserves raw old/new bytes and the normalized diffs before updating
the exact dependency hashes; it does not weaken stale-input rejection.

Full generation again preserves all 5,570 declaration units across the nine
selected files, including all 3,139 XUI managed units. The new pointer-free
`HostSessionDesc` record has separate native C/C++ value-ABI qualification.
Real Shape/ShapeScene four-corner output and its console lesson are rechecked
with the matching latest-XRT complete compiler and native host/static SDKs.
See `dev/release-materials/provenance/xge3-storage-replay-20261117.json` for the
source/replay/runtime evidence and its limits. Runtime and IDE installation
remain separate; native binaries are reused unchanged in this batch.
