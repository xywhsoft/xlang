# Canonical PE unwind records

PE x64/ARM64 frontend unwind rows now append to one `.pdata` section and one
relocation section. The ELF reader coalesces compatible `.pdata`, `.pdata.*`
and `.pdata$*` contributions into that pair before assigning new section IDs.
The AMD64 COFF reader does the same for non-COMDAT contributions. COMDAT
metadata retains its independent identity and selection rules. Unsupported
shape/alignment inputs remain separate and conservative; they are not silently
reinterpreted as valid unwind rows.

The loader budgets the resulting sections before publishing new sections or
symbols. `SHN_LORESERVE`, the existing metadata headroom, and the final
`new_section` hard guard remain unchanged. This is a representation change,
not a larger section-index format or an early destructive collection pass.
The old 41,142-header Windows XRT object can be loaded without rewriting it:
in the focused test, a state with 25,000 sections becomes 45,689 sections after
loading that exact object. The discarded per-contribution header count does
not imply discarded executable code or unwind data.

GC builds a temporary record graph: 12-byte rows on x64 and 8-byte rows on
ARM64. A live executable owner activates its row, whose relocations can retain
unwind information, handlers and chained metadata. Reachability iterates to a
fixed point; a nonmutating archive query uses the same graph without compacting
or changing symbols, offsets or input records. Unknown/opaque metadata is
conservative. Real metadata roots, including explicit `-u`, retained sections,
exports, and external references into the table, retain the whole table and
preserve offsets. Automatically generated `__start_pdata`/`__stop_pdata`
boundaries alone are excluded from implicit default-memory symbol-query roots;
explicit references to those symbols still retain the table.

The final GC mutation compacts only tables whose offsets can safely move,
updates row relocations and metadata symbols, then preserves the existing PE
RVA sort and JIT unwind-registration paths. No-GC and relocatable-object paths
keep all input records for future consumers. Ordinary default JIT still keeps
public definitions for late `tcc_get_symbol`; it discards unreferenced static
functions, including dead references to unavailable symbols.

This removes two `Section` directory entries per eligible additional unwind
contribution. There is no additional runtime object, allocation or call.
Collection and archive queries allocate temporary per-table/record graph
metadata and scan its relocations. Frontend/ELF/COFF loaders perform extra
bounded header classification and append/copy the metadata bytes at link time.
The representation does not eliminate the unwind rows or change their ABI.

Focused gates cover row owners, callback edges, handlers/chains, opaque rows,
external metadata references, repeated nonmutating queries, object round trips,
no-GC preservation, implicit/explicit boundary roots and real JIT unwind lookup.
The original section-GC gate separately checks default JIT, STABS/DWARF2/4/5,
PE exports and `rdynamic`. Windows COFF and static-TLS tools additionally emit
and run fresh PE CPU programs. These source/CLI gates remain separate from a
matched shipping compiler, precompiled runtime and whole-XGE application.
