# IDE dialog layout

The IDE owns the composition; XUI owns row/column/flow measurement, sizing,
scrolling, hit testing, and native window resizing. No XGE ABI change is needed.

Resizable forms use a column with two siblings:

1. A growing vertical ScrollView containing the form.
2. A fixed footer containing a separator and a right-aligned action strip.
   Declaration groups share spare width and wrap together when necessary.

Fields have a fixed label column and a growing input column. Narrow forms place
the label above the input. Browse buttons stay next to their field; primary
actions stay in the footer. Preview/list blocks have minimum heights and grow
with spare room. Text wraps, text inputs scroll internally, and the outer form
does not acquire a horizontal scrollbar. Dropdown popups are bounded by the IDE
viewport even when an item contains a very long path.

Minimum window sizes are logical DIP values, scaled with the UI font and bounded
by the available IDE viewport:

| Dialog | Minimum width × height |
| --- | --- |
| Code/UI font | 480 × 340 |
| Legacy import | 600 × 420 |
| Rename/project migration | 500 × 300 |
| Go to line/column | 380 × 250 |
| File conflicts | 600 × 400 |
| Session/recovery | 600 × 420 |
| New project | 560 × 400 |
| Toolchain information | 560 × 400 |
| Project configuration | 600 × 420 |
| Project references | 600 × 420 |
| Windows resources | 600 × 460 |
| Settings | 640 × 440 |
| Command source/document viewer | 560 × 380 |
| About | 400 × 320 |
| Native file chooser | 560 × 400 |

Settings uses an actual-viewport layout callback for its two-column content.
Its category list and settings fields scroll independently; previews and notices
belong to the field content. The shared footer stays outside both regions.
The About dialog retains its existing centered responsive layout and has no
body action buttons.

Native message boxes autosize from their message, so their resize/maximize
controls are disabled. They have a footer separator. Long diagnostic messages
use a read-only, selectable scrolling text area; confirmation buttons retain
the native result/default-action contracts. Overwrite, updater, and failure
prompts use the same presentation policy.

`dialog_layout_test` checks twelve form families at 16/20/24/40 DIP and three
window size targets plus a 700 × 460 host at 40 DIP, including minima, footer containment, content-only
scrolling, long input/dropdown paths, and long native messages. Existing
functional fixtures exercise the actual buttons, menus, drafts, modal guards,
input/IME, font/DPI changes, and resource operations. Their shared test helper
distinguishes footer buttons from scroll content and converts nested list/menu
coordinates through the actual native widget hierarchy.
