# Native capability requirements

Source packages can require native capabilities with `requiresNativeFeatures`.
The selected native artifact advertises its capabilities in `native.features`.
Keys are case-sensitive, nonempty ASCII tokens containing letters, digits,
underscore, dot, colon or hyphen. Duplicate keys, embedded NUL, whitespace,
nontext entries and invalid array shapes are rejected.

```json
{
  "requiresNativeFeatures": ["XGE_ENABLE_3D"],
  "dependencies": [
    {"name": "_xge_native", "version": "3.0.0", "abiVersion": "2"}
  ]
}
```

A requirement must be provided by the package itself or its manifest-declared
dependency closure. An unrelated package imported by the same program cannot
satisfy it. Dependency cycles and diamonds terminate during this validation.
The native variant selected for JIT/AOT supplies the capability list; variants
must preserve the required set as well as the existing package identity,
language-source and dependency contracts. Reordering requirements is harmless.

Matched hosted/static SDK staging now records the features from its native
build report, including the debug capability only for a debug-enabled build.
This metadata complements the existing native hash, symbol, ABI and runtime
initialization checks; declaring a feature is not a substitute for those checks.

The first implementation validates source-package expansion after the complete
outermost dependency graph has been collected. Legacy packages with no
requirements allocate no validator scratch storage. A graph containing
requirements allocates two scratch arrays once per expansion and scans its
declared closure. Parsing copies each validated capability string once. These
are cold compilation costs, with no added drawing, input or audio call path.

The prerequisite gate passed 150 steps on each actual OS, Windows and Linux:
74 parser/closure/variant checks, 18 real frontend import checks, 52 existing
native-link parser checks, and six matched SDK staging/import checks. Both O0
and O2 are covered. Allocation failure prefixes check parsing and validator
rollback with real XRT heap accounting. The default SDK provides 3D and does
not provide the debug capability; these checks execute the actual frontend,
without claiming a graphics/backend test.

The source and SDK reports were independently checked against input, log,
binary and package hashes and the source inputs were frozen externally. Exact
paths are in
`dev/release-materials/provenance/package-native-features-source-20261116.json`.

This is a qualified source/frontend prerequisite. Compiled-package interfaces
must also preserve their selected capability metadata and declared closure;
that wire-path implementation and its full compiler/context-reuse acceptance
remain pending. No compiler, SDK, IDE or runtime was installed by this batch.
Debug-enabled runtime/backend qualification and the rest of the full XGE/XUI/
Document integration remain separate requirements.
