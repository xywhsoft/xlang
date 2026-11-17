# XGE debug capabilities

Use the matched SDK built with `XGE_DEBUGMODE=1`, then import both `xge` and
`xgeDebug`. The explicit `xge` import supplies the types and constants used by
the debug surface. Ordinary SDKs do not publish this optional package.

All five native debug functions have direct declarations: statistics, capability
text and dirty rectangle count/get/clear. `debugDumpCaps(Buffer)` writes into
the caller's initialized storage and returns the native status unchanged.
`XGE_ERROR_BUFFER_TOO_SMALL` means the NUL-terminated report was truncated;
the caller can resize and retry. The binding does not allocate an output payload,
grow the Buffer or reinterpret a partial report as success. The readable
`lib/bindings/xge/debug/examples/capabilities.xl` example performs its text
conversion and terminator removal explicitly.

The native dynamic builder now includes its debug mode in the generated SDK
configuration. Stage it with `tools/stage_xge3_packages.py --debug` and its
`build-report-debug.json`; an ordinary build uses the default option and report.
First-include configuration, native image identity and ABI fingerprint checks
remain mandatory. Static staging obtains the configuration from its own matched
build. No feature macros are patched into an already built SDK.

Windows r31 full O2 compiler qualification passes 19 commands: real JIT, fixture
and example, independent hosted/static programs and C11/C++11/C++17 public
headers. Exact O2 hosted and static SDKs build and stage on Windows/Linux;
both SDK configurations pass six native header modes per platform. Ordinary
SDK staging also passes with the default options. Evidence is recorded in
`dev/release-materials/provenance/xge-debug-capabilities-windows-20261113.json`.

These tests run before platform/GPU initialization. Debug operations after
engine initialization belong on its creator thread. Live rendering, concurrent
global debug calls, Linux language execution and actual package DLL/DSO
initialization require their own qualification. The final release/IDE has not
been replaced.

Linux full r24 subsequently passes the same 19 language checks against its
matched hosted/static SDK, including actual ELF execution and dependency
inspection. Its successor evidence is
`xge-debug-capabilities-linux-20261113.json`; the Windows receipt retains its
historical limits. Live GPU and package shared initialization remain separate.

## Live contexts and shared modules, 2026-11-16

The successor 09d-XRT debug SDKs close those two qualification gaps. Each host
configuration builds 163 units and reuses the dependency-validated HarfBuzz
object; each direct-XRT static SDK builds all 166 units. All retain O2 and 3D.
The frozen native source excludes concurrent geometry/text working changes.
Headers, configuration, image/archive, package manifests and XRT agree; an
ordinary SDK is not converted into a debug SDK by changing consumer macros.

On Windows and Linux, the actual JIT, hosted/static EXE and hosted/static
DLL/DSO execute a bounded OpenGL frame. They verify live texture counts and
memory, a clean GL error, independent statistics/dirty-rectangle snapshots,
capability text, exact render-target pixels, and Busy engine shutdown with a
held texture followed by successful release/retry. Shared modules initialize
and complete checked retirement after callbacks/resources close. Generated C
and C11/C++11/C++17 headers pass their checks. Linux uses Xvfb and software
OpenGL, not a hardware-GPU acceptance claim.

Warm language and handwritten C queries each call the same native stats
function 1,000 times and both show zero XRT allocation/free/reallocation.
The warmed stats output also succeeds with a zero XRT allocation budget.
This does not measure driver allocations, CPU equivalence, stack/value copies,
all graphics backends, concurrent global calls, or arbitrary provider unloading.
The native creator-thread precondition remains in effect after initialization.

The existing 19-command cold/bounded-output suite passes on each current SDK.
The new `examples/live_stats.xl` lesson passes JIT and hosted/static AOT, and
both normal hosted/static SDKs reject the actual debug package before Cgen or
linking. Each platform has 48 final checks: 18 live-context/module/header/import,
19 cold/output-boundary, seven lesson, and four expected feature rejections.
The pre-existing historical receipts are not rewritten.

The complete input/log/program/package verification is indexed by
`dev/release-materials/provenance/xge-debug-live-20261116.json`. These are
isolated candidates. Latest-XRT convergence, remaining XGE/XUI/Document work
and the final matched release/IDE/help installation are still open.

## Native b44 and XRT4641 convergence, 2026-11-17

The current debug candidates now use the same committed Native b44 and
XRT4641 baseline as the normal SDK candidates. Each platform uses an
independent 3810-file native source witness, preserving unrelated native
working changes. Both hosted SDKs compile all 164 units and both direct-XRT
static SDKs compile all 166 units at O2, with no object reuse. Debug and 3D
features are selected before compilation and remain in the staged SDK
configuration and fingerprint.

`tools/check_xge3_debug_delivery.py` composes the existing live/cold gates with
the readable live-statistics lesson and actual normal-SDK feature rejection.
It verifies the complete staged file population and every input/output pin,
not only the files exercised by the lesson. Twelve local positive/negative
stage-receipt checks per platform cover incomplete/failed/shipping receipts,
changed or absent inputs/outputs, extra files and duplicate normalized names.

Windows and Linux each pass 48 final commands: 18 live graphics/module/header/
import checks, 19 cold/bounded-output checks, seven lesson commands and four
expected ordinary-SDK rejections. The real JIT, hosted/static executables and
hosted/static shared modules execute a graphics frame and complete checked
retirement. The tests include render-target pixels, held-texture Busy shutdown
and successful release/retry, independent snapshots, warm C/language heap
checks and zero-allocation-budget stats output. Linux uses Xvfb/software GL;
Windows uses its native OpenGL backend.

Independent verification rechecks source witnesses, every native object and
dependency record, compiler/package inputs, logs, program bytes and import
tables. It indexes 8399 Windows and 8306 Linux files, including the 660 freshly
compiled native units across the four builds. File counts are provenance
accounting, not functional coverage claims. The immutable successor receipt is
`dev/release-materials/provenance/xge-debug-current-20261117.json`; historical
receipts and their bytes are preserved.

This closes the current debug configuration's older-XRT qualification gap.
It does not qualify concurrent global operations, other graphics backends,
arbitrary provider/module cycles, or all remaining 3D/UI/Document features.
The owning Engine/controller and other pending contracts remain open. Normal
`release/`, IDE and the mutable shipping delivery index are not replaced by
this isolated candidate batch. No network or security test ran.
