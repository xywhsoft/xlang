# Bulk navigation, menu-bar and status-bar items

`Breadcrumb.setItemsColumns`, `MenuBar.setItemsColumns` and
`StatusBar.setItemsColumns` call their original XUI functions once. They reuse
the existing synchronous typed-array projection; there is no product-specific
C forwarding function, new compiler rule or second widget implementation.
The scoped generator definitions and managed declarations are checked together.

| Control | Columns, in order |
| --- | --- |
| Breadcrumb | Nullable text, clickable, value |
| MenuBar | Nullable text, state, value, mnemonic, optional Menu |
| StatusBar | Nullable text, type, section, state, value, width, flex, minimum, maximum, progress |

Each index forms one native record. `size=-1` requires equal column lengths;
nonnegative size selects a bounded common prefix. Other negative values fail
checked. Empty inputs supply NULL records and zero count. Native capacity,
state, mnemonic and finite-geometry checks still apply. C-string columns reject
embedded NUL before entering native code, then native code copies the accepted
labels. NULL labels retain native empty-label behavior. Later input-array edits
cannot alter the controls' labels.

Menu entries must reference Menu widgets in the same Context. Per-element
scopes keep them valid through the call; native slots remain weak references.
Explicitly closing a Menu clears every referencing MenuBar slot and disables
those items. Destroying the MenuBar leaves its menus alive. These operations
do not introduce another destroy right. Native layout-cache fields are ignored
inputs and stay under native control. Raw `pUser` is zero; language callbacks
capture their own typed state and use the exposed item values instead of an
untracked pointer.

Breadcrumb normalizes nonzero clickable values. MenuBar normalizes ASCII
mnemonics. StatusBar clears clickable/enabled flags on spacers and clamps
progress to the supplied range. The binding preserves these native results.
It reports checked failure without inventing a different transaction policy;
the OOM gate separately checks that its exercised failed allocations preserve
the old items. Creator-thread and expired-widget admission remain in force.

`lib/bindings/xui/examples/control_items.xl` shows the three controls in one
window. Its grouped named StatusBar columns make the visible text and progress
values explicit. The separate existing single-control lessons remain available.

Projection costs are explicit: a nonempty call allocates and fills one native
record array in O(n), pins the source columns and checks C strings for NUL.
MenuBar additionally snapshots/scopes the resource column in a temporary carrier
array. Record payloads on the qualified x64 ABI are 16, 56 and 72 bytes per
item respectively. For 1,000 warmed, equal-data calls with three items, the
language makes 1,000 / 2,000 / 1,000 XRT allocations and corresponding frees;
the handwritten C stack-record baselines make none. There are no reallocations.
These counts exclude input-container setup and native CRT allocations. Native
label copying still occurs on changed-data calls; no cold-path or CPU-time
equivalence is claimed.

The exact generated Menu carrier layout is 128 bytes per selected element,
including its owner reference, checked call and 48-byte native Context scope.
For three items, MenuBar therefore requests 168 record bytes plus 384 carrier
bytes in two temporary allocations. Breadcrumb requests 48 record bytes and
StatusBar 216. These are requested payload sizes, excluding allocator headers
and rounding. All paths still perform reference/pin, thread/state and checked
cleanup work; the allocation counters do not measure that CPU cost.

`tools/check_xui_control_items.py` requires an actual complete O2 compiler and
matched hosted/static SDK. It executes an independent C oracle at O0/O2 and the
language boundary script in JIT/dynamic/static AOT, validates C11/C++11/C++17
public headers and import tables, and runs the real-window lesson in all three
modes. The native oracle checks alias copies, bounds, normalized fields, weak
menus and allocation-failure prefixes. Language checks add projection OOM,
copied labels, NUL/length rejection, cross-Context menus, independent menu
lifetime, wrong-thread access and expiry. Test-only C readers do not add an
unsafe public pointer-record getter.

The dated provenance receipt records the actual platform results, source and
SDK/compiler/package hashes, allocation counts and rendered readbacks. This is
an incremental batch; the full XGE/XUI/Document goal and final matched
runtime/IDE/help installation remain active. No release binary, current-delivery,
version or online publication is changed by this batch.

Windows and Linux each pass 30 functional/native/ABI/render commands and two
independent C layout-probe commands; the full-surface delta audit adds one,
for 65 process checks. Each platform's JIT/dynamic/static lesson frames match
byte-for-byte and both previews were visually inspected. Evidence is indexed
by `dev/release-materials/provenance/xui-control-items-20261115.json`.
