# ELF input-section budget

The ELF object reader's merge loop can create at most one `Section` for each
non-null input section header. Relocation sections have their own headers in
`e_shnum`; they are not extra sections beyond that count. Symbol/string tables
map to existing sections or are skipped, and the later symbol/relocation passes
do not create additional sections.

The admission bound is therefore `e_shnum - 1`, with the existing 32-section
headroom for later linker metadata. The former doubled estimate rejected a
20,189-header sectioned runtime object when the state already held 25,000
sections, although the actual combined directory remained representable.
Zero/extended section-table counts are explicitly unsupported by the current
16-bit section-index model and are rejected before subtracting one.

`new_section` remains the final hard guard at `SHN_LORESERVE`; neither the
reserved index range nor its controlled error jump changes. This is an input
budget correction, not support for ELF `SHN_XINDEX` or a higher section limit.

The focused test builds valid ELF64 inputs with real data/relocation header
pairs. It admits 20,189 headers on top of 25,000 existing sections and at the
exact one-times admission boundary, rejects a genuinely overflowing directory
before any input sections/symbols are published, recovers by loading a small
object, rejects unsupported zero/extended counts, and exercises the unchanged
hard-limit jump and subsequent state cleanup. `--actual-object` also loads the
exact full sectioned runtime object into a state with 25,000 existing sections.
GCC builds the host fixture; it emits and executes no PE file.
