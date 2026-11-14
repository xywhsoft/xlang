# Menu item input and submenu links

`Menu.setItemsColumns(texts,shortcuts,types,states,values,icons,submenus,size=-1)`
directly calls `xuiMenuSetItems` once, using the existing generic record-array
projection. Text and shortcut columns are `array<string?>`; submenu entries
are `array<Menu?>`. The four numeric columns use `c_int` / `c_uint` exactly as
the C record. The private user pointer is initialized NULL. Managed values
must not escape into it; explicit raw C interoperation retains that native API.

`Menu.addItem` keeps its existing parameters and adds a final optional
`submenu=null`. It builds one native record on the stack and calls
`xuiMenuAddItem` once. There is no per-method C forwarding function, new runtime
operation or language contract. The native API copies text, and preserves NULL
versus empty strings in the bulk operation. Embedded NUL is rejected before
the native call; UTF-8 strings are supplied without an extra character copy.

With `size=-1`, all seven columns must have equal lengths. An explicit
nonnegative size selects the minimum of that size and all column lengths.
Other negative sizes fail. Empty arrays clear the menu. Native item-capacity,
type, state and submenu checks remain authoritative. Failed preparation keeps
the old items and caller storage; native layout/callback side effects are not
promised to roll back in general.

Submenu links are native weak references within one Context, not ownership
transfers. A submenu must be live, in the same Context, and different from its
parent for a submenu item. Closing a submenu clears native referring slots
and disables those items. Closing a parent leaves the submenu usable. All
widgets still belong to their Context, whose close retires the whole tree.
Projected submenu views are held and checked for the synchronous call only.
Closed views and calls from a different thread fail before native access.

The focused [lesson](../lib/bindings/xui/examples/menu_items.xl) shows one
replacement operation, check-state and a nested command menu. Each native
menu can have its own selection callback; those callbacks use the existing
owned-closure and checked-error rules. The normal introductory `menu.xl`
remains the simpler sequential-add example.

Costs are explicit. For a nonempty bulk call the bridge acquires seven
read-only input pins, allocates one native record array and one submenu
call-carrier array, checks C-string NULs, snapshots submenu owners, projects
fields and releases the pins and temporaries after the call. No string bytes
are copied by that projection; the native menu owns its own text duplicates.
Empty arrays allocate neither temporary. Packing one `addItem` uses stack
storage, C-string validation and the existing view-call checks.
In the calibrated single-item fixture, native addition allocates once and the
language operation twice. Its inline `"More"` argument is first materialized
as an owned language string (`xlir_string_from_utf8` in generated C), whereas
the independent C fixture passes a static string. That extra string allocation
and copy is not an allocation by the stack record packer.

In the pinned three-row CPU workload, 1000 identical replacements use
alloc/free 2000/2000 in the language and 0/0 in independent C. Changing a value
each iteration uses 8000/8000 versus 5000/5000; 1000 of the language allocations
are measured independently as typed-array element updates, and 2000 are the
projection temporaries. Reallocation is zero. These are XRT heap counters,
not an elapsed-time or CPU-equivalence claim. Native field comparisons, string
scans/copies, foreign call checks and owner/pin bookkeeping still execute.

Acceptance uses the matched constructor/menu-fix SDK, independent O0/O2
hosted/static C clients, complete O2 compiler cohorts, C/C++ header checks,
JIT/dynamic/static language tests and a bounded real-window lesson. Calibrated
budgets exercise every allocation prefix, preserve old items and caller
strings, and compare complete retirement heap/resource counters. Raw output
record string getters remain outside this batch's input-only declarations.
See `dev/release-materials/provenance/xui-menu-items-20261115.json`.

The original Menu input qualification used an older compiler that rejected
literal `null` in `array<Menu?>.push`; its capacity test used an explicitly
typed nullable local. That immutable receipt retains the original limitation
and failing probe. The successor compiler repair covers literal/conditional
null and the `var` domain; see
[contextual insertion](foreign-nullable-insertion.md) and
`dev/release-materials/provenance/foreign-nullable-insertion-20261115.json`.
Nullable entries passed directly to the record-array input already worked.
