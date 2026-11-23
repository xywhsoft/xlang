# Code command maps and standalone dispatch

`createCodeCommandMap()` creates an owned `CodeCommandMap` and loads the native
default editor bindings. No Context, window or graphics backend is required.
Use `bind(key,modifiers,command)`, `unbind(key,modifiers)`, `find(key,modifiers)`,
`getCount()`, `getBinding(index)` and `loadDefaults()`; close in `finally`.

Keys and command IDs must be positive C integers; modifiers use only the native
SHIFT/CTRL/ALT/SUPER bits. The map stores exact key values: lowercase and
uppercase letters are different. Binding the same key/modifier pair replaces
its command without adding a row. Native command IDs are positive numbers;
this storage API does not execute or validate a command implementation.

`find` returns the command after checked success. A missing binding, including
an already removed `unbind`, preserves native checked UNSUPPORTED behavior;
it is not converted into NULL or an ambiguous boolean. Invalid indices and
arguments are checked failures. A failed output leaves the destination intact.
`getBinding` returns an independent `CodeKeyBinding` value, including the native
size, key, modifiers, command and `iWhen` fields. Mutating that value does not
change the map. `loadDefaults` allocates the new table before replacing the old
one, so OOM retains existing bindings.

The native map owns only a numeric array. It has no language, document or
callback edges, and uses the established independent-leaf policy with creator
thread checks, logical close and deferred physical destruction during active
foreign calls. A stored bound method cannot access a closed map. CodeEdit's
internal map is a live view owned by the editor. Standalone map retirement
uses the native checked destroy function.

Each business operation calls its native target directly; there is no per-method
C business wrapper or opcode dispatcher. The policy has a small native destroy
hook, and ownership/code/scope/error checks remain. Native linear lookup,
table growth/memmove and the copied 20-byte binding are retained. In measured
prepared creation/close, language and C allocate/free 4 versus 2 objects,
respectively. A prepared 1000-round count/find/getBinding/rebind loop performs
zero XRT alloc/free/realloc on both sides. These heap counts do not establish
CPU equivalence or remove the record-value copy.

`tools/check_xui_code_commands.py` uses the matched full O2 compiler and existing
O2 SDK, independent hosted/static C O0/O2 clients, three C/C++ header modes and
language JIT/dynamic/static programs. It checks native defaults, exact keys,
record copies, checked output rollback, 96 language OOM prefixes, native failure
atomicity, active close, foreign threads, expired bound calls and heap costs.
`examples/code_commands.xl` is a small window-free lesson.

Windows and Linux each passed 30 functional/header/link/client command steps
and 8 unused-declaration trim steps (76 total). The trim control removes only
the new source from the package manifest; executable size, text and BSS deltas
are zero on both platforms. Final package revision 2 changes only the README
from the executed revision 1, with all executable inputs byte-identical; that
comparison is not counted as another execution. See the small receipt
`dev/release-materials/provenance/xui-code-commands-20261115.json` for pinned
external reports, source hashes and the final candidate package paths.

## Execute commands without a CodeEdit widget

`xui.executeCodeCommand(command, document, selection)` calls the native editing
dispatcher for standalone CodeDocument and CodeSelectionModel objects. It
returns the native handled integer on success and raises checked errors from
the dispatcher. For undo or redo, pass only the document. For fold and unfold,
pass a standalone `foldState`. The `pageLines`, `indent`, `indentColumns` and
`lineComment` options use the native command settings.

Set `readOnly=1` to reject edits, including undo and redo, in the built-in
dispatcher. Selection and navigation still work. Pass a `proxy` for clipboard
commands. A typed `providers` set can handle a custom command using the
supplied `widget`. It can return `XUI_ERROR_UNSUPPORTED` to defer to the built-in
command. Use `editor.executeCommand()` to apply CodeEdit's selection, command
guards and editor-specific commands.

The compiler fills the native descriptor on the stack and protects each
provided view until the synchronous call ends. The generated code calls
`xuiCodeCommandExecute` directly. The typed provider's widget and result
views expire when its callback returns. The native C provider API still
accepts the opaque `pCommandData` pointer.

[Standalone command example](../lib/bindings/xui/examples/standalone_code_commands.xl)

The matched Windows and Linux tests exercise direct dispatch, callback
fallback, original callback errors, expired views, injected allocation
failure and read-only history. The separate C clients also exercise native
clipboard and opaque command payloads.

In each linkage mode, C and xlang execute 1,000 selection moves on the same
document and record zero XRT allocations, frees and reallocations. The
observed per-loop times span 0.015–0.020 ms for C and 1.413–6.586 ms for
xlang. The C loop reuses its descriptor. The xlang loop builds the
descriptor and performs checked view admission on each call.

[Standalone dispatch acceptance](../dev/release-materials/provenance/xui-standalone-code-commands-20261124.json)
