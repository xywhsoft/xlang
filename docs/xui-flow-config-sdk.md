# FlowGraph configuration fix in matched SDK candidates

The committed XGE fix `df03691f` is now present in separately staged Windows
and Linux host/static SDK candidates. A custom XRT handle Clone that removes
the target node no longer leaves `SetNodeConfig` using the old node position.
The native result is `XUI_ERROR_INVALID_STATE`; the language status adapter
delivers that same code as a checked error. Rejected copies are released once.
Replacing a configuration installs its successor before the old value's Drop.

These candidates reuse the original complete O2 Provider-owned SDK build,
with exact dependency verification. Hosted builds compile two translation
units and reuse 162; static builds compile three objects and reuse 163.
The existing Provider CodeEdit successor is retained, and its recompiled
object is byte-identical to the last qualified successor on both platforms
and both linkage modes. These are qualified source-unit updates, not full
engine rebuilds. Public headers and the canonical resident XRT contract stay
matched to the complete compiler/runtime cohorts.

`tools/check_xui_flow_config_sdk.py` executes real hosted and static libraries,
not a separately compiled replacement for the function under test:

- The new upstream configuration Clone/Drop regression and existing FlowGraph
  model suite run under GCC O0/O2 with the same resident XRT. Live allocation,
  invalid-free and double-free checks cover each client.
- A test-only language prototype exercises direct sized node inputs,
  transferred config outputs, explicit self-reference cleanup and checked
  clone invalidation through JIT, dynamic AOT and static AOT.
- C11/C++11/C++17 public headers compile. AOT imports show the dynamic engine
  only for the dynamic variant; static executables import neither the engine
  DLL/shared library nor xlang/TinyCC runtime libraries.
- Every input hash and the complete frozen source tree are rechecked on the
  actual test OS. Private runtime headers come from the complete compiler's
  selected 516-header manifest, not a stale copied SDK.

The gate has 27 steps per OS, 54 total. The small provenance receipt records
the builds, staging reports, exact paths and hashes; runtime bytes and raw
reports remain external. No release installation or current-delivery change
is implied.

This update does not add a public `xui.FlowGraph` class. Its fixture's custom
C value deliberately borrows a raw model while the outer test owner stays
alive; it is not a safe public constructor for arbitrary retained callbacks
or a proof of complete resource/code ownership. The native getter itself
deep-clones configs. The language owning-output projection adopts the returned
reference without another clone; scope/status/code activity and heap work
still remain. Balanced heaps do not establish CPU equivalence or zero bridge
overhead.

Uniform same-model callback reentry admission, the remaining config consumers,
command history, implicit cycle retirement and Widget/Workflow aliases remain
open. The standalone prototype is intentionally absent from the default
package until those contracts and tests are complete. The full XGE/XUI/
Document goal and matching final SDK/compiler/runtime/IDE/help delivery are
still active.
