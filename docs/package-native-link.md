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
it does not silently emit an untrimmed product.

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
