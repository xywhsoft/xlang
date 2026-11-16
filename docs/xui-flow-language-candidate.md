# Shared FlowGraph language candidate

The current source candidate maps the shared native model to `xui.FlowGraph`.
Each language view owns one actual native reference. A canvas owns its own
reference; replacing/closing it does not consume an external model view.
Model aliases remain usable after their original view, canvas and Context close.
The underlying model and ownership domain enforce creator-thread access.

Calls remain direct native declarations. Counts use the generic signed integer
lower-bound contract (zero); find queries preserve -1 as not found and reject
smaller error codes. Output records snapshot their borrowed text as one checked
group, preserving NULL and empty strings. Dynamic port and move-record columns
use the existing temporary C-array projection. This has O(n) conversion cost.
Native reference/activity tracking and managed-view checks still have CPU cost;
zero XRT allocation/reallocation in a warmed count loop is not CPU equivalence.

Windows and Linux each passed 74 runtime steps using complete O2 compiler,
TCC/precompiled/VFS products and the exact hosted/static Native SDKs. These
cover 68 model declaration variants over 65 native/runtime names, C11 body and
C/C++11/C++17 headers, JIT, independent executables, checked shared modules,
owned aliases, native Clone/Drop Busy callbacks and both integer error bounds.
They also exercise canvas get/set ownership and closed bound methods, 48-point
factory/output/command allocation-failure sweeps and 32-point shared-acquisition
sweeps, with warmed live-allocation/reference balance. The readable headless
`flow_model.xl` lesson passed JIT/dynamic/static execution on both platforms.

Generator replay passed 384 Windows and 385 Linux source checks. Two canvas
methods, their opaque native types and resolved shapes are pinned explicitly;
opaque-structure/pointer shape mutations are rejected. Existing declarations
and package manifests retain their contracts. Evidence and immutable source
witnesses are indexed by
`dev/release-materials/provenance/xui-flow-language-candidate-20261116.json`.

This is a candidate checkpoint, not a public API or release promotion. The
model declarations, runtime policy and related fixtures remain uncommitted
while the default configuration-value contract is pending. An opaque language
Dict's declared native Clone preserves reference identity. Tests explicitly use
the existing data snapshot bridge before the native configuration clone; that
path performs conversion plus a native deep clone and is not a single-copy
default API. The independent-default-snapshot/advanced-native split still needs
the outstanding user choice.

Workflow shared ownership, schema/default callback reentry, associated Graph
alias safety, persistent cross-module/cyclic resource qualification and final
matched SDK/compiler/IDE/help installation remain open. CPU proxy cases close
their resources explicitly and do not certify arbitrary provider unloading,
GPU/desktop performance or browser operation. Current execution pins qualified
XRT09d; the later additive 19775 producer has been prepared separately for final
convergence. No network/security test ran.
