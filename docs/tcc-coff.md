# Native AMD64 COFF inputs

The Windows x64 TinyCC linker accepts ordinary GCC C COFF objects and indexed
static archives alongside its existing ELF target objects. The objects keep
their native O2 code, symbols, relocation edges and independently collectible
sections; no source wrapper or object-format conversion step is involved.

The supported contract includes ADDR64/ADDR32/ADDR32NB, REL32 through REL32_5,
SECTION/SECREL, common symbols, defined weak aliases and the NODUPLICATES, ANY,
SAME_SIZE and ASSOCIATIVE COMDAT rules. Default GCC constructor/destructor
arrays are normalized to the linker's init/fini arrays. Non-default constructor
priorities, other COMDAT rules and unsupported input forms fail explicitly.
This is not a claim of arbitrary MSVC/C++/LTO or exception-personality support.
COFF inputs are admitted for final image/JIT linking. COFF-to-ELF partial linking
(`-r`/object output) is rejected: COMDAT identity and PE section-layout metadata
do not have a qualified serialized ELF contract. Normal C-to-TCC-object
precompilation and archive creation by copying original objects remain valid.

Keep GNU import-table fragments out of native code archives. The package builder
preserves the original compatibility code and expresses actual DLL imports as
separate `.def` data. The reader rejects `.idata$*` fragments rather than merging
them as ordinary program data and silently producing an invalid import table.

Import definitions retain the local-to-DLL name mapping, for example:

```text
LIBRARY msvcrt.dll
EXPORTS
localAbs=labs
localHuge=_HUGE DATA
localOrdinal @42 NONAME
```

The symbol lookup uses the local name; both AOT import names and JIT
GetProcAddress use the declared DLL name or NONAME ordinal. DATA imports require
the caller's dllimport declaration and cannot be used as functions. The current
input grammar requires LIBRARY and EXPORTS on their own lines and one export
per subsequent line. Unknown suffixes and malformed ordinals are rejected.

Windows static TLS uses the PE loader's template/index/callback contract.
Live TLS in an in-memory module is rejected when that loader contract is
unavailable; dynamic TLS slots are not substituted. Unreachable TLS can be
discarded by section GC. See the exact acceptance receipts before treating a
particular compiler/CRT/Native cohort as qualified.

The libtcc CPU fixture explicitly calls initializer/finalizer arrays. Those
linker/ABI checks are separate from a language module's initialization contract.

`tools/check_tcc_coff.py` uses a GCC-built libtcc API host and real GCC archives
for CPU JIT, callbacks, aggregate ABI, constructors, imports and invalid inputs.
PE emission and PE execution are separate opt-ins. Passing the JIT gate does
not imply that an AOT image was emitted, inspected or executed.
