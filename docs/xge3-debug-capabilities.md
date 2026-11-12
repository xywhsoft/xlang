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
