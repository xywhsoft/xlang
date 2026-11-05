# Explicit-root JIT archive reachability

With all three conditions present (`TCC_OUTPUT_MEMORY`, `--gc-sections`, and
at least one explicit `-u` / `--undefined` root), archive selection consults a
fresh non-mutating reachability graph before choosing a member. The graph is
the same collector used by final GC: explicit roots, exports, retained sections,
constructor/destructor tables, callback/data relocations, exception ownership,
and live FDE/CIE dependencies all participate. This prevents a dead caller's
undefined reference from loading an otherwise unused member whose constructor
or TLS callback would then become an unconditional runtime obligation.

The query returns only a temporary bitmap of reachable undefined symbols. It
does not shrink sections, publish ABS-zero tombstones, compact unwind records,
rebuild hashes, or cache inferred exception-owner pointers in Section objects.
After loading one member, selection restarts with a new graph because that
member can add constructors, new transitive references or weak definitions.
Explicit unresolved entry roots can therefore select the first archive member.
Non-indexed whole-archive loading also admits ordinary AMD64 COFF through the
same reader, after reading a complete, member-bounded 20-byte header. Archive
indices/long-name tables are skipped; unsupported short-import objects fail
closed. No new input bytes are guessed from an uninitialized ELF header.

Plain C/JIT with no explicit roots, GC-disabled linking, AOT and whole-archive
selection preserve their existing rules. In particular, AOT does not query a
premature graph before its platform startup is added. The host must install
all desired JIT module roots before adding the static archives. Ordinary late
`tcc_get_symbol` lookup remains available in the default no-explicit-root mode.
A selected member's real constructor/TLS obligations are never ignored.

Collection uses temporary O(sections + symbols + unwind records) work storage
per selection pass. Restarting the archive index after each newly selected
member costs cold-link CPU time; this is not a runtime performance optimization
claim. No added per-function call wrapper or hidden runtime object is used.
Final GC rebuilds its symbol hash and count after turning discarded definitions
into local tombstones; query mode leaves both hash and definitions unchanged.

`tools/check_tcc_archive_reachability.py` creates a reversed-order archive with
a dead TLS/constructor member, a live function-pointer data edge, transitive
members and a constructor that needs another member. It checks repeated-query
byte preservation, public JIT execution, explicit live-TLS refusal on Windows,
and unchanged default/AOT selection. It also executes the existing section-GC
fixture's JIT/error/registration/index-limit cases without calling any of its
PE-output tests. Optional `--tls-inputs` reruns the dedicated static-TLS memory
layout fixture against its exact frozen GCC/MinGW inputs on Windows. No
TinyCC-produced PE file is written or executed by this tool.

COFF-to-TCC-ELF partial linking (`TCC_OUTPUT_OBJ`, including `-r`) is explicitly
rejected by the COFF reader before native sections/symbols are admitted. The
internal ELF writer does not preserve all COFF COMDAT selection/identity and
original TLS contribution-key metadata. This does not affect ordinary C-to-TCC
object precompilation, final linking of original COFF inputs, or `tcc -ar` raw
member copying. `--partial-only` runs the small admission/refusal/precompile
regression without repeating the already-qualified archive and GC matrix.
