# Gamepad state and table geometry

`xge.gamepadGetState(index)` returns an independent `GamepadState` value with
connected state, current/pressed/released button masks and all eight axes.
`gamepadSetState(index,state)` passes a complete native record by const address.
No callback-bearing pointer, Buffer layout guess or per-method C forwarding
function is exposed. Native index failures are checked and failed reads leave
the language target unchanged. A disconnected device is a successful record
with `bConnected=0`, not a missing-result error.

SetState consumes connected/buttons/axes. It normalizes a nonzero connected
flag, derives and accumulates pressed/released masks from the previous button
state, and ignores the two incoming edge masks. Normal frame input processing
clears accumulated edges; repeated injection alone does not simulate a frame.
Disconnect clears the native state. Axes follow native copying without added
clamping; producers must supply the documented range. These global operations
retain XGE's serial engine-thread contract and add no lock or cross-thread
guarantee. They can feed the existing Actions default reader without a window
or a physical device. Input/IME event strings and missing-touch projection keep
their separate pending contracts.

`TableView` gains four direct native methods:

| Method | Result | Native meaning |
| --- | --- | --- |
| getOffset | `(c_float,c_float)` | Current clamped scroll offsets |
| getCellContentRect(row,column) | Rect | Content coordinates, before viewport/scroll translation |
| getCellRect(row,column) | Rect | Widget-local viewport coordinates after scrolling |
| getItemAt(x,y) | `(c_int,c_int)` | Merged owner cell, or `(-1,-1)` for no hit |

Offsets are floating point; Rect keeps the native integer pixel fields. A cell
can be outside the viewport while its rectangle remains a valid result. The
existing native adapter/merge provider determines row/column spans. Queries can
invoke providers; they are not guaranteed callback-free. Provider failures keep
the original language error and prevent output assignment. Invalid indices and
closed views are checked. A normal no-hit remains successful; no bool or nullable
carrier is invented. Sort column plus direction, pointer-bearing column records,
model aliases and output arrays retain their separate contracts.

The CPU fixture checks translated coordinates, offsets, a real hit, no-hit,
invalid output preservation, provider-error preservation and closed-handle
rejection. Its static-row query loop and the gamepad loop compare 1000 rounds
with handwritten C calls over the same native SDK. Each language/C hot loop
has zero XRT alloc/free/realloc; a zero allocation budget remains active during
the language loop. Adapter callback costs and cold model setup are not included
in the zero-heap result. Native scanning/merge work is still performed.

Generated C calls each named native function directly. On x64 the complete
GamepadState is 48 bytes. The generated SetState path snapshots a 48-byte input
value before passing its address; GetState uses a 48-byte output temporary.
Checked assignment can then copy that value. Rect outputs use a 16-byte
temporary, offset pairs 8 bytes and hit pairs 8 bytes, committed after native
and view-scope success. Value copies, index/scalar checks, error scopes and
Widget/Context protection remain work. Zero heap is not CPU equivalence or a
claim that these copies/checks disappear under TCC.

`examples/gamepad_state.xl` is a linear injection/read/cleanup lesson.
`examples/tableview.xl` shows the selected cell's content rectangle under the
table. The latter keeps the shared bounded window helper. Runnable CPU fixtures
are separate from lessons. Windows/Linux use qualified O2 compiler r40/r33 and
the same native Margin-owned SDKs; this batch changes no compiler or native
implementation and installs no release/IDE update. See the dated provenance
receipt for exact inputs, execution versus compile-only checks and limitations.
