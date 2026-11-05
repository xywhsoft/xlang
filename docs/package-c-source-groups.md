# Conditional native C source groups

`artifact.cSources` and `artifact.runtimeSymbols` remain unconditional. A source
package can additionally declare a cold, opt-in dependency group:

```json
"cSourceGroups": [{
  "source": "src/transfer.c",
  "providedSymbols": ["xl6Xge3RunOwned", "xl6Xge3DragBegin",
    "xl6Xge3CopyToClipboard", "xl6Xui3CopyToClipboard"],
  "requiredSymbols": ["xl6CodeLeaseAcquireScoped", "xl6CodeLeaseRelease",
    "xl6TransferServicesFromLeaseV1", "xl6TransferServicesProcessResidentV1"]
}]
```

`source` is a package-relative `.c` path without empty, dot, parent, absolute
or drive segments. Both symbol arrays are nonempty, unique ASCII C identifiers
without embedded NUL. A package must not also list the source as unconditional;
provided symbols cannot overlap across groups. Conflicting imported groups are
rejected; repeated imports of the same exact group are deduplicated. Unknown
resident symbol names fail before C compilation, including inactive groups.

After lowering and verification, the compiler intersects each group's provided
symbols with reachable external C/native-factory call targets. A declaration
alone does not select a group. Bound methods use the same verified target
reachability. Selection atomically publishes both the C source and resident
requirements. Allocation failure leaves the previous lists and resolution
state intact. Actual C compilation, resident artifact selection, and the native
C environment report share these lists. The report adds `cSourceGroups` with a
`selected` flag, `selectedCSources`, and `requiredRuntimeSymbols`.

This is explicit metadata, not C-body dependency inference. Each selected source
must contain or explicitly otherwise supply all its native C dependencies;
`requiredSymbols` identifies resident runtime APIs, not conditional peer source
groups. Group selection is at translation-unit granularity: all functions in a
selected group remain available. Existing unconditional sources, links, load
libraries and XRT-module declarations keep their contracts.

TinyCC's normal non-inline functions share `.text`; its linker rejects
`--gc-sections`. An unused non-inline function's unresolved reference still
requires a provider. GNU section garbage collection therefore cannot prove
TinyCC artifact size. `_xge_native` keeps the normal binder in unconditional
`runtime.c`; the four transfer helpers share a conditional `transfer.c` group.
An ordinary drawing import does not select the resident COM component. Once a
transfer helper is reachable, the whole group and the component's real dependency
closure are selected. The existing full resident host may already contain these
APIs; this optimization concerns source-artifact deployment, not shrinking that
host DLL. There is no new ordinary UI method branch, wrapper or runtime lookup.

The focused CPU checker exercises malformed metadata, exact selection,
unknown resident names, allocation-failure rollback, unused source rejection,
direct and bound TinyCC calls, real artifact plans, and report consistency.
Final Native SDK and serialized-object/VFS qualification remain separate gates.
