# Checked platform startup

XGE native commit `e1089362` fixes a failed `XOpenDisplay()` before any X11
window, cursor or graphics context has been created. The former Sokol path
logged a fatal error and then dereferenced a NULL display. It now frees its
startup buffers and returns; XGE preserves `XGE_ERROR_BACKEND_FAILED` in the
existing blocking Run outcome. No native public signature or language binding
has changed, and no new language exception adapter is needed.

With the matched SDK, `xge.run()` reports the existing checked error:

| Field | Value |
| --- | --- |
| code | -8 |
| domain | xlang.foreign |
| operation | xl6Xge3RunOwned |

No init/frame/application cleanup callback runs for the failed Sokol startup.
The ordinary callback ownership rule still applies: clear a registered frame
callback before releasing its language captures or closing the engine. The
shared XUI lesson window helper already does this in `finally`, closes its
Context and Proxy, and then closes the engine. It needs no new forwarding code.
An explicitly manual/CPU engine does not require a display service.

`foreign_xge3_x11_failure.xl` checks 12 failed runs across three engine lifetimes,
exact error code/domain/operation, no frames and manual-mode cleanup.
`foreign_xui_x11_failure.xl` checks four failures through the actual shared
window helper and subsequent reinitialization, covering its Context/Proxy and
retained callback cleanup. The Linux harness executes each with a full compiler
in JIT and independent dynamic/static AOT and checks public C/C++ headers and
imports. DISPLAY is removed only from child test processes.

Separate native C O0/O2 gates reproduce the old SIGSEGV, verify the actual new
static archive, balance the two Sokol startup allocations per failed attempt,
and recover to a real local Xvfb display in the same process. The standalone
Sokol header also passes ASan/UBSan/leak checks on the no-display path; the
complete engine and graphics drivers are not claimed to be instrumented.
Existing GL-load-failure/first-frame suppression and both-platform actual
window lessons are separate regressions. Linux graphics uses Mesa software
OpenGL, not hardware-GPU acceptance. Exact reports are indexed by
`dev/release-materials/provenance/xge-x11-startup-20261114.json`.

This is a native error-path fix. There is no extra preflight display connection,
allocation or per-frame bridge call. The fatal logger uses the existing Run
lock/error reference, and adds a source-level level-zero check; no equal-CPU
claim is made. The matched host files keep their lengths despite small text
and unwind-section growth. The detailed size comparison is in the receipt.

Later GLX/EGL/window failures, allocator panics and display disconnects still
need their own stage-aware unwind. A blanket longjmp from Sokol would skip
native cleanup and is not introduced. The default Sokol behavior without a
custom logger is unchanged. Final runtime/SDK/IDE delivery and the complete
XGE/XUI/Document integration remain separate outstanding work.

## Pre-window allocation failure

Native commit `c22608d6` also handles Sokol clipboard/drop-buffer allocation
failure before platform setup. A partial allocation is freed with the original
allocator, state is cleared, and no application init/frame/cleanup callback runs.
The signed drop-buffer extent is checked before multiplication. All six private
platform callers check the initializer's result; Windows and Linux are executed,
while Apple/Android/web changes are source-reviewed only.

The native Run status is `XGE_ERROR_OUT_OF_MEMORY` (-5). Existing language error
scopes preserve an available XRT allocation diagnostic: in the qualified fixture,
`Error.code` is 1, `domain` is `xrt.memory`, and `operation` is `allocate`.
This is distinct from synthesizing a fallback `xlang.foreign` status error when
no native diagnostic exists. The binding and runtime contracts are unchanged.

`foreign_xge3_state_oom.xl` disables fault injection before reading diagnostic
strings, checks 12 public Run failures across three lifetimes, and then opens a
real window once per lifetime. It also checks manual-mode reinitialization.
A zero-allocation language budget may fail an earlier code-lease admission;
it is not evidence that every language attempt reached Sokol. The separate
actual native C gate identifies each Sokol buffer stage and verifies exact
allocation/free counts, unchanged live storage, no frames and same-process retry.

The matched full compilers pass 13 build/run/import/header steps on each OS in
JIT and independent dynamic/static AOT. Linux also passes 11 steps each for the
existing direct missing-display and actual XUI lesson-helper cleanup fixtures.
The 48 language process rows and separate 44 native rows exclude SDK staging
and historical rejection runs. Linux window recovery uses local Xvfb/software
OpenGL. Details are pinned in `xge-startup-state-oom-20261114.json` under
`dev/release-materials/provenance/`.

The same O2 host comparison has unchanged file lengths, Windows text +128 bytes,
and Linux text -192 bytes. The checks run during startup/allocation and fatal
logging; there is no new per-frame allocation or forwarding layer, and no
CPU-time equivalence claim. Qualified object reuse rebuilds the changed aggregate
and generated static contract; this is not a complete source rebuild. Final
runtime/SDK/IDE installation and all later platform-fatal branches remain
separate work.
