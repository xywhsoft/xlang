# Native static link requirements

A source package can opt into section-based native linking without changing
the language declarations or adding per-method wrappers:

```json
"artifact": {
  "nativeLink": {
    "functionSections": true,
    "dataSections": true,
    "gcSections": true,
    "printGcSections": false
  }
}
```

All four properties are exact JSON booleans. Unknown properties and other value
types are rejected. Printing discarded sections requires `gcSections: true` in
the same manifest. Omitting this object preserves the original link behavior.
Requirements are enable-only and combined across imported packages: `false`
does not revoke another package's requirement.

The compiler applies the selected options before compiling generated/native C.
The C environment report records both the option operations and the merged
`nativeLink` object. A provider that rejects a required option fails compilation;
it does not silently emit an untrimmed product. Required features are checked
through the optional `tcc_native_link_features()` provider query; an old
provider remains usable for ordinary packages but is rejected for requirements
it cannot certify. This also covers providers that silently ignore unknown
`-f` options. The selected compilation defines `XL6_NATIVE_LINK_SCHEMA=1` so a
static SDK can reject an older compiler that ignores the new manifest field.

Use existing `links` for real static archives or explicit object inputs and leave
`loadlibs` empty for those static inputs. `links` does not imply that every
archive member is needed. The actual linker uses undefined symbols to select
archive members and their relocations to retain dependent functions/data.
`cSourceGroups` continues to select optional source compilation units using
verified XLIR reachability. Its `requiredSymbols` still denotes resident
runtime requirements, not a manually maintained native C call graph.

Trimming retains callbacks, referenced function-pointer tables, constructors,
destructors, exports and necessary unwind metadata. A public provider table that
references all operations legitimately keeps those operations. Smaller provider
configurations need explicit native feature contracts; a linker cannot infer
that runtime input will never request a supported operation.

A static engine must use the program's one compatible XRT implementation.
Its public declarations, feature configuration, object ABI and ownership
contracts remain one matched set. A DLL import library is not a static archive
of the engine implementation. Object format compatibility and section garbage
collection are independently verified capabilities.

For language JIT modules, the host's named module ABI and the verified language
exports form explicit roots before relocation. Host lookup and root collection
share `src/api/xl_module_native_abi.inc`; optional ABI functions are selected
only when defined in the actual compiled symbol table. An arbitrary C JIT
module retains TinyCC's conservative public-symbol behavior unless its caller
explicitly supplies link roots. Code not exported or reachable is not promised
to remain available through ad hoc raw-symbol lookup in an opt-in trimmed
language module.
