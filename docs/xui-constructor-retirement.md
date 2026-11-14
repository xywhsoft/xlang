# Constructor and menu-copy retirement

The matched native SDK corrects partial initialization cleanup in Hyperlink,
Input, TagInput, TextEdit and Terminal. Each failing stage releases its own
partial resources. The generic construction chain still destroys only stages
that completed successfully. This also repairs composite constructors that
create internal Input widgets: NumericInput, InventoryGrid, TableGrid,
PropertyGrid and ComboBox.

Menu had a separate copying defect. The temporary item record initially held
the caller's borrowed label and shortcut. If label duplication failed, rollback
freed the shortcut before its own duplicate had been created. Temporary string
fields now start NULL and become owned individually as each copy succeeds.
This preserves caller storage and the old menu on preparation failure, and
fixes the shared invalid-free observed in CodeEdit and built-in edit menus.
Native SetItems/AddItem still copy text; submenu and user pointers retain their
documented native borrowing rules. No public ABI or compiler rule changes.

Native validation covers all 56 exported default Widget constructor triads,
five nonempty descriptor variants, and six menu mutations including borrowed
menu-source aliases and NULL text/shortcut fields. The budget bound is measured
from a successful warmed operation's allocation/reallocation counters, with a
margin. Every budget is exercised; an early successful allocation-failure path
does not stop the scan. Each iteration retires its complete Context, avoiding
false leak reports from capacities retained by a live event queue. Live bytes,
live allocation count, invalid-free and double-free counters must return to
their initial values. O0/O2 hosted/static callers are compiled independently.

The language fixture uses the existing direct factory declarations with real
owned CPU Proxy/Font/Surface resources. It calibrates each of the eleven
previously failing factories and Menu.addItem, preserves prior assignments and
menu state on failure, and checks complete native/foreign resource retirement
after each function returns. Its injected constructor failure starts after
Context/font setup. Unrelated operations are not assigned this fault budget.

Prepared normal operations retain their native allocations and copies. The
corrections add failure cleanup and explicit temporary pointer ownership; they
do not establish CPU-equivalent bridge costs or zero-allocation construction.
Native/GCC O2 SDKs and the existing complete O2 compiler cohorts are pinned in
the provenance receipt. Linux graphics checks, when used, run on Xvfb/Mesa;
physical GPU/driver qualification and final shipping delivery remain separate.

The Input/TextEdit source files also contain unrelated working-tree pixel
geometry edits. This batch's candidate snapshots and staged patches include
only the initializer cleanup changes. The existing geometry work and test
proxy updates are preserved for their own validation.

See `dev/release-materials/provenance/xui-constructor-retirement-20261115.json`.
This default/nonempty-descriptor fault-injection coverage does not certify all
possible descriptor values, callbacks or the complete XGE/XUI/Document surface.
