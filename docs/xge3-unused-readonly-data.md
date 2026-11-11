# Follow-up: unused readonly data in a static XGE client

This is a binary/source diagnosis, not a completed trimming fix. The preceding
image qualification and its raw reports remain unchanged.

The same matrix-only source and complete compiler, rebuilt against the new
static XGE SDK, run successfully and have no XGE host or xl/xlc image dependency.
Their remaining file growth is entirely outside machine code:

| Platform | File change | Identified section change |
| --- | ---: | --- |
| Windows | +6144 bytes | `.rdata` +5888; `.reloc` -44; remaining file alignment |
| Linux | +22240 bytes | `.data.ro` +22224; remaining file alignment/name bytes |

Windows code section sizes are unchanged. Linux aggregate code is exactly
434726 bytes in both products; normalizing compiler-generated numeric section
suffixes leaves identical code sizes (one function identity is renumbered).
There is no evidence here of unused Deflate machine code surviving collection.

Linux's readonly pool adds 44 distinct printable strings (1511 bytes), removes
one (27 bytes), and contains the new Deflate errors and miniz assertion strings.
The other readonly bytes have not yet been attributed to exact constants or
symbols. The data must not all be labeled strings or compiler contract records.

Two source routes explain a real retention mechanism in TinyCC:

- `lib/tcc/tccgen.c`, `case TOK_STR`/`str_init`, explicitly selects the common
  `rodata_section`. The later `decl_initializer_alloc` split requires
  `!ad->section`, so compiler-created string literals bypass data splitting.
- `gv` also writes floating constants directly into the common readonly pool.

Reachable references keep that entire pool alive after unused function
sections are collected. These routes identify a general compiler/linker issue;
they do not by themselves account for every added byte. No compiler source was
modified by this diagnosis.

The next candidate should give compiler-created literals independent
collection units while preserving explicit user section attributes, literal
lifetime/alignment and relocation addends. Live fragments also need compact
final output: emitting one final ELF section header per tiny literal can cost
more than the data saved. Changes belong in the common compiler/linker, with
C literal/float/relocation and JIT/static format tests, not individual XGE API
wrappers or special cases that hide this module.

The small evidence index is
`dev/release-materials/provenance/xge-unused-readonly-data-20261112.json`.
Native PNG/image qualification remains valid, but perfect unused-code/data
trimming and full matched delivery remain unfinished.
