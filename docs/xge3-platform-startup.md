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

The GLX/EGL/window startup branches now have the stage-aware unwind described
below. Later display disconnects and non-GL backend branches still need their
own acceptance. A blanket longjmp from Sokol would skip native cleanup and is
not introduced. The default Sokol behavior without a custom logger is unchanged.
Final runtime/SDK/IDE delivery and the complete XGE/XUI/Document integration
remain separate outstanding work.

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
runtime/SDK/IDE installation and later platform-fatal branches remain separate
work; the following subsection completes the listed GLX/EGL startup paths.

## GLX, EGL and X11 window startup

Native `dc7857c2` makes the private initializers return failure after a fatal
log and unwind their actual acquired resources. It covers library/entry-point
and version admission, FBConfig choice/allocation, visual/colormap/window,
drawable/context/current binding, plus EGL initialization/configuration/
surface/context stages. X11 asynchronous errors are synchronized; the previous
application handler is restored before error delivery. Custom cursor resources
are released before XCloseDisplay, including on normal completion. Existing
Sokol log-item numbers are preserved by appending the new MakeCurrent item.

Standalone O0/O2 clients exercise 18 GLX stages and 16 each for EGL desktop GL
and EGL GLES 3, with three failures followed by a real healthy window in each
case. C++11/C++17 implementation syntax checks also pass. The old header's
visual-failure SIGSEGV is reproduced separately for all three profiles and both
optimizations. Actual O2 static XGE clients add the 18 GLX stages at O0/O2,
native XRT live-storage checks, exact checked status and three healthy engine
lifetimes after each failure sequence. The core native checks total 150 process
rows; missing-display/GL-load regressions add 18. Twelve old-header build/
rejection rows and the environment experiment are not counted as positive tests.

The full Linux compiler consumes the real no-GLX Xvfb display in JIT and
independent dynamic/static AOT: 12 errors retain code -8, domain
`xlang.foreign`, operation `xl6Xge3RunOwned`, and suppress frames. Three actual
healthy windows recover in the same process; the manual engine still needs no
GLX. Its 11 header/build/run/import commands pass. The matched Windows SDK also
passes 24 existing CodeSelection/CodeTheme fixture and lesson commands in all
three execution modes. No language/compiler/ABI adaptation was needed.

A completed graphics Run retains the existing ENDED lifecycle; Unit/Init is
required before a further healthy Run. Pre-startup failures allow direct retry.
XGE's separate OpenGL resolver currently keeps a process-wide library cache.
Native tests count that existing owner explicitly and show no accumulating
Sokol owner. Resolver backend-switch and module-retirement ownership remains
an independent open audit, not a completed cleanup claim.

One earlier EGL O2 check encountered XOpenDisplay failure before the selected
stage. In 50 comparisons per policy, Xvfb reset allowed one such failure and
`-noreset` allowed none. Final gates use a persistent local Xvfb server with TCP
disabled, without suppressing failures or automatic retries. Historical logs
remain archived; graphics uses Mesa software rendering, not hardware acceptance.

Both SDKs keep O2. Host builds use the original full parent and compile the
aggregate plus the already-qualified Document TU (162 reused); static builds
compile the aggregate/generated contract (164 reused). Whole host file lengths
add zero bytes on both platforms; text adds Windows 0/Linux 832 and BSS 0/32.
Checks/releases occur at startup/teardown; the modified source adds no per-frame
operation or allocation. This does not prove CPU-time equivalence. Exact source,
SDK, compiler, test and size hashes are indexed in
`dev/release-materials/provenance/xge-linux-startup-unwind-20261114.json`.
Final mainline runtime/SDK/IDE installation and full-ecosystem acceptance remain
outstanding; these are isolated qualified candidates.
