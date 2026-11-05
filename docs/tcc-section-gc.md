# TinyCC independent sections and final-link GC

The feature is opt-in. Ordinary callers keep the previous layout and link
behavior. Pass these options with `tcc_set_options` or the TinyCC CLI:

```text
-ffunction-sections -fdata-sections -Wl,--gc-sections
```

`-fno-function-sections`, `-fno-data-sections`, and `-Wl,--no-gc-sections`
reverse the respective options. `-Wl,--print-gc-sections` prints discarded
nonempty sections to stderr without treating a diagnostic as a warning/error.
Use `-Wl,--undefined=entryName` (or `-Wl,-u,entryName`) for additional roots.
`tcc_native_link_features()` reports exact capability bits in `libtcc.h`;
callers must query it instead of trusting acceptance of an unknown `-f` option
by an older provider. The current GC capability is advertised for x86-64 PE/ELF;
other backends fail explicitly rather than silently claiming GC.

Those names are also real undefined references for static archive extraction;
a missing explicit root is an error. Existing `-Wl,-e,entryName` selects the
executable entry and retains its graph.

The frontend emits an individual section for each ordinary function and allocated
static/global object, preserving an explicit `section` attribute. Unique names
include a per-state serial; independent input contributions and their relocation
tables remain separate even if two object files use the same static helper name.
Common symbols requested with `-fcommon` keep their existing shared allocation.
Other preexisting shared literal pools remain shared; this is section reachability,
not string/constant deduplication or whole-program optimization.

GC runs after runtime/archive/common loading and before final undefined-symbol
resolution, GOT/PLT construction, or PE imports. It follows actual relocation
edges, including function pointers stored in data and registration tables reached
through linker `__start_*`/`__stop_*` symbols. Init/fini/preinit arrays, legacy
constructor/destructor tables, retained sections, explicit roots, PE exports,
resources, and required bounds-checker records remain roots. ELF shared-library
visible APIs remain roots. PE DLLs retain only their real explicit exports; an
unexported global is eligible for GC. `-rdynamic` deliberately preserves the
globals selected by the original platform export contract.

For JIT, ordinary public definitions remain roots unless the caller supplies an
explicit `--undefined` root list. This preserves late `tcc_get_symbol` callers.
With an explicit list, the host is responsible for naming every public entry it
will query. `tcc_run` still retains its own selected runtime entry. Constructors
and destructors remain reachable; existing run/host machinery executes them.
Relocatable object output is never collected, so it can be linked later.

Dead sections lose their allocated payload and relocations. Their symbol indices
remain stable until the normal output sorter; dead definitions and unreferenced
undefineds become local zero-valued absolute tombstones. A live unresolved symbol
still fails. This does not remove arbitrary referenced import libraries from
`DT_NEEDED`, introduce `--as-needed`, or change static archive extraction policy.

Unwind metadata is preserved. ELF CIE/FDE records are traced by initial-PC
ownership; only live FDEs and their CIE/personality/LSDA dependencies survive,
and record offsets, relocations, and linker section boundaries are updated.
Malformed or unsupported extended-length EH records fail explicitly. PE native
function records use the actual code section, per-function metadata follows that
code, live `.pdata` contributions are merged, and final RVAs are sorted before
file output or JIT registration. A required reorder with external pointers into
individual table entries is checked-rejected pending full address-map support.
The current acceptance targets x86-64 Windows and Linux; other target backends
have not gained new platform qualification merely because these generic flags
parse. Mach-O GC is not implemented by this change.

Implementation costs are cold: one extra pointer per internal Section, option/
root state, extra compiler sections/relocations, and temporary reachability/frame
indexes during final linking. On Windows x64 the internal Section grows 120 to 128 bytes (+8), and TCCState
grows 1856 to 1904 bytes (+48 after alignment). On Linux x64 the corresponding
sizes are 136 to 144 (+8) and 1792 to 1832 (+40). State growth includes the
stack-owned section-limit recovery pointer.
No runtime dispatch or user-object bookkeeping is
added. Dead source is still parsed and compiled, and existing compiler buffers
are released at state teardown; this feature promises smaller final allocated
images, not a smaller compiler peak allocation. Large `.eh_frame` tables use
binary record lookup and per-record relocation lists rather than a full scan for
every function. The ordinary off path does not construct a GC graph.

Run the standalone CPU gate (it snapshots all source inputs and preserves logs):

```text
python tools/check_tcc_section_gc.py --root /path/to/x-lang --out /new/isolated/output --cc gcc
```

The GCC-built fixture directly includes production libtcc, verifies O0 and O2,
executes small JIT callbacks and lifecycle arrays, checks Windows unwind lookup,
and inspects TCC object/AOT output without executing emitted PE images. The
cross-platform gate is separate from COFF-loader and whole XGE static-link
qualification, which must use the same final source cohort.
