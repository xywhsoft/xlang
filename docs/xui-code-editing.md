# Selection editing commands

`CodeDocument` directly exposes the native editing helpers with a
`CodeSelectionModel` argument. It uses the original byte positions, grapheme
boundaries, history and notification semantics; it does not copy the text to
another editor implementation.

| Method | Native function |
| --- | --- |
| `insertSelectedText(selection,text,readonly=0)` | `xuiCodeEditingInsertText` |
| `deleteBackward(selection,readonly=0)` | `xuiCodeEditingDeleteBackward` |
| `deleteForward(selection,readonly=0)` | `xuiCodeEditingDeleteForward` |
| `deleteWordBackward(selection,readonly=0)` | `xuiCodeEditingDeleteWordBackward` |
| `deleteWordForward(selection,readonly=0)` | `xuiCodeEditingDeleteWordForward` |
| `indent(selection,indent="\t",readonly=0)` | `xuiCodeEditingIndentSelection` |
| `outdent(selection,columns=4,readonly=0)` | `xuiCodeEditingOutdentSelection` |
| `toggleLineComment(selection,marker="//",readonly=0)` | `xuiCodeEditingToggleLineComment` |

Each call uses the existing checked native status declaration. Readonly
rejection is `XUI_ERROR_UNSUPPORTED`, without modifying the document. Invalid
markers fail before the edit. The document and selection remain creator-thread
objects, protected for the complete call. Closing either owner invalidates its
aliases and bound method values. Native history still groups multiline indent,
outdent and comment commands into one undo step.

Text arguments use the original C-string API. `@c_cstr` scans for embedded NUL
and rejects truncation without creating a C-string copy. Native helpers also
perform their own length/marker scans. Existing counted document bytes,
including NUL, remain intact. Use the document's counted `setText` when the
replacement itself needs embedded NUL; these helpers do not invent a new sized
native insertion API.

An exception does not promise rollback. Native edits may have committed before
notification fails, or some selected lines may have changed before OOM. Query
the document after catching an error; a failed notification can leave the
previous selection state. Explicit partial-commit carriers and worker batch
handoff remain separate pending contracts.

The native repair in `73d10642` stops indent/outdent/comment from ignoring
EndEdit failure and then using a possibly closed document. Earlier body status
and diagnostic, including NULL, take precedence over later cleanup failure.
Actual hosted/static O0/O2 clients verify callback failures, close, grouped
history and forty allocation budgets per compound command. Old archives
reproduce the swallowed status; no unsafe old-close execution is required.

The linear [lesson](../lib/bindings/xui/examples/code_editing.xl) replaces
`world`, indents, undoes and comments the result. It needs no window, external
resource, manual DLL loading or network. Fault injection and performance checks
stay in separate fixtures.

Both full compilers pass 24 fixture/lesson build, run, public C/C++11/C++17 and
import checks per platform, in JIT and independent dynamic/static AOT. Another
11 steps per platform check all eight bound method values, throwing change
callbacks and an expired retained receiver. Native acceptance contributes 64
positive process steps; four old builds/four defect rejections are separate.

For a prepared empty document/selection, 1000 rounds of backspace, forward
delete, word-backward, word-forward and outdent make 5000 calls with zero XRT
alloc/free/realloc in both xlang and handwritten C, in all three modes. This is
the no-change path. Real edits allocate native history/text storage; language
payload/closure construction, receiver/error scopes, code leases, native pins,
mutation gates, scans and cold initialization remain costs. There is no
per-method forwarding implementation and no CPU-equivalence claim.

Bound method values are ordinary language callables. Their generated capture
environment retains the document; runtime creation also builds a closure record
and acquires its code lease. Invocation goes through the compiler's callable
adapter and an indirect call. Those formation/call costs are outside the direct
5000-call heap interval and are not claimed to be zero or equivalent to raw C.

A selected Slider build uses the same static archive/compiler before and after
adding only these declarations. File and all text-section size deltas are zero
on both platforms (12 build/check steps); no unused-interface size guarantee
for other programs is implied. The native O2 host repair itself adds 128 text
bytes, zero file bytes and zero BSS bytes on each platform. Host builds compile
three units/reuse 161; static builds compile two/reuse 164, not full rebuilds.

`tools/check_xui_code_editing.py` runs the primary gate. The separate method-value
fixture is runnable directly with the same matched SDK:

```powershell
xl run test/cases/foreign_xui_code_editing_values.xl --inc-path .
```

Exact source, standalone qualification runner, compiler, SDK, cases and size
hashes are pinned in `dev/release-materials/provenance/xui-code-editing-20261114.json`.
CommandMap lookup still needs the existing pending conditional-result contract;
MarginModel callbacks are not treated as numeric leaf ownership. Those gaps and
the final matched runtime/IDE/help installation remain in the full goal.
