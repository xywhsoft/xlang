# Independent code command maps

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
internal map alias requires its separate long-lived owner-alias contract;
`xuiCodeCommandExecute` and provider callbacks retain their own integration
work. This independent model does not expose their raw pointers.

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
`examples/code_commands.xl` is a small window-free lesson. Native code and the
compiler are unchanged; final matched release installation is separate.

Windows and Linux each passed 30 functional/header/link/client command steps
and 8 unused-declaration trim steps (76 total). The trim control removes only
the new source from the package manifest; executable size, text and BSS deltas
are zero on both platforms. Final package revision 2 changes only the README
from the executed revision 1, with all executable inputs byte-identical; that
comparison is not counted as another execution. See the small receipt
`dev/release-materials/provenance/xui-code-commands-20261115.json` for pinned
external reports, source hashes and the final candidate package paths.
