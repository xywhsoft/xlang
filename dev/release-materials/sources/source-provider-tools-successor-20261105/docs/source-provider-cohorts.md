# Matched source compiler and Only-TCC provider cohorts

`tools/stage_source_tcc_provider.py` retains its original `--base`,
`--precompiled`, `--out` full-build invocation. A successor may instead supply
`--overlay` describing actual replacement objects over that completed base.
The tool verifies the parent inputs, objects, headers and driver, the selected
replacement objects and driver, and the producer's source fingerprint.

Example for an explicitly partial successor:

```text
python -B tools/stage_source_tcc_provider.py \
  --base /path/to/verified-full-build \
  --overlay /path/to/compiler-successor/overlay.json \
  --root /path/to/prepared-producer \
  --precompiled /path/to/matched-target-objects \
  --producer-tool /path/to/actual-tcc-producer.exe \
  --match-provider-imports \
  --out /path/to/fresh-provider
```

Prepare the producer completely, verify its frozen source file set and bytes,
then run the normal precompiler. Never modify SDK inputs while precompilation
is running. A failed final source-fingerprint check rejects the run; it does
not produce an accepted precompiled manifest.

The producer keeps the current `release/win_x64` layout. Runtime VFS declarations
are derived from `src/runtime` by the original resource manifests. Do not copy
the full generated compiler-source SDK back into the producer's include tree.
The staged output SDK maps every alias to its selected frozen source and records
its hash, including successor runtime declarations. Neither the frozen source
cohort nor the workspace release is modified.

For an overlay, both output `overlay.json` and compatibility `build.json`
explicitly contain `fullRebuild: false`. They retain the real completed parent
and actual replacement objects; they contain no synthetic `Complete` full-build
claim. Pass these manifests to validation tools with `--overlay`. The original
full-build path keeps its existing full manifest. Resident-runtime DLL bytes
remain unchanged. By default the driver is copied; `--match-provider-imports`
instead relinks its verified objects with the current provider, without
recompilation. The new provider contains TinyCC and
serialized resources; symbol inspection rejects resident xlang/XRT code.

Matched-link mode is limited to the stock diagnostic SourceDriver, whose API
provider path is fixed to adjacent `xlc.dll`. The actual PE import table must
bind `tcc_delete` and every imported `tcc_*`/`_tcc_*` function to that provider.
The observed MinGW direct-DLL input rule is last-wins; the IAT check is the
authority, rather than an assumed order. This does not establish safety for an
arbitrary host selecting another `tccDllPath` or swapping providers. Normal
static production profiles bind their factory and destructor to one static
TinyCC core; the diagnostic host must satisfy the same instance contract.

`check_source_provider_nested_windows.py` covers JIT, modules, standalone output,
dependency inspection and relocated execution. Its relocated environment has no
XL6 overrides or adjacent compiler SDK/provider. The outer diagnostic compiler
still uses its explicitly copied standard C distribution. The record/size test
adds its declared native test sidecar; this is not a compiler-source fallback.

`check_tcc_optional_vfs.py` checks optional VFS signatures and a genuine provider
with VFS disabled. `check_tcc_provider_loaded_libraries.py` checks actual selected
provider load/unload references. A TCC state must be created, relocated and
destroyed by the same provider instance, including native-handle finalizers.

An authoritative source candidate can be assembled with
`assemble_source_candidate.py` from exact tracked Git blobs and explicit
material allowlists. It excludes Python caches and validation outputs. Git
archive attribute conversion is checked against each blob and corrected to
the authoritative bytes. `build_source_candidate.py` compares the actual
parent sources, flags, compiler and include closure, producing an explicit
rebuild/reuse plan. Additive-header inheritance requires separately pinned
old/new qualification and a forbidden-reference audit; it is never inferred
from unchanged source alone.

The Linux stager uses an explicit native configuration and current
`release/linux_x64` provider path. It includes additional runtime objects from
the overlay and derives only the diagnostic driver's platform-provider path.
An interrupted stage whose precompile/catalog/provider steps completed may
be resumed into a new directory with `--reuse-provider-stage` and the exact
archived `--prior-tool`. This verifies prior inputs, target objects, resource
stamps and logs, preserving the original failed report and avoiding a second
precompile. Normal cache hit/rebuild counts remain those actually observed.

SourceDriver profiles are distinct from production `XL_BUILD_DLL` profiles.
Their object reuse proves only the original flags. Production images require
their actual static-TCC factory/destructor profile, with independent build and
CPU acceptance evidence. Provider verification never updates active release
files or the delivery index.

A header-only SDK is not a declaration-only SDK. Standalone emission in
`src/compile/xl6_xrt_trim.c` defines `XRT_IMPLEMENTATION` and
`XRUNTIME_IMPLEMENTATION` before including the complete dependency headers.
Keep these implementation-capable SDK headers and include fragments; the
source-isolation guarantee excludes loose compiler/runtime `.c` and `.o`
files, not definitions contained in required SDK headers. Every compiler
header alias is refreshed from the authoritative frozen source, alongside
public and resident runtime headers.
