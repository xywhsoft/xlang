# PE import ordering and thunk aliases

A PE import may be referenced both by an ordinary function name and by a COFF
`__imp_` IAT symbol. The order in which input objects create those symbols must
not change resolution. Each import keeps a symbol-index chain: an IAT object's
`st_value` temporarily links to the previous symbol until `pe_build_imports`
assigns the final IAT address. A newly created local `IAT.*` helper now links to
the existing head instead of replacing it with zero. Function-first and
IAT-first input orders therefore preserve every alias.

`import_symbol.thk_offset` remains its existing signed `int` field. A newly
created import explicitly initializes it to -1; a nonnegative value denotes an
existing thunk. Code offset zero is a valid thunk and must be reusable. No
persistent type, layout, section limit, import kind or export contract changes.
The correction adds one cold integer initialization per import; it introduces
no new allocation, runtime wrapper or method dispatch. Reusing a thunk at zero
avoids duplicate code and relocation data for aliases of the same import.

`tools/check_tcc_pe_iat_order.py` builds a GCC CPU host around the exact
production libtcc. A real GCC COFF object references `__imp_TlsGetValue`, while
TCC code references its ordinary function name and an explicit import alias.
Both input orders, GC and no-GC, run through PE_RUN and actual emitted PE files
when `--execute-pe` is given. The programs allocate a Windows TLS slot, set and
read its value through both forms, then release it. Additional checks cover an
import thunk at code offset zero, a real MSVCRT DATA alias, rejection of a
function declaration against DATA, and GC/no-GC missing-import diagnostics.
The optional immutable baseline must fail the import-chain and zero-offset
regressions. This focused source/backend qualification is separate from the
matched shipping compiler, VFS/precompiled objects and whole-XGE AOT acceptance.
