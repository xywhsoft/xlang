# XUI text snapshots

Text getters return an independent language string. A returned `null` is an
absent native result; a present empty string remains `""`. Mutating or closing
the original object cannot change an already copied string. Access through an
expired managed object is a checked error.

The reviewed C-string getters in XGE DataObject and XUI widgets, managers and
CodeFindScope use the general `@c_result("cstring")` contract. Each invokes the
native function once and copies its borrowed C string before the receiver
scope ends. A nonempty result needs one NUL scan, one allocation and one copy;
this is an owned snapshot, not a zero-copy borrowed string. The declaration
keeps the original nullable result and native missing-index behavior. An
invalid find-scope index, for example, returns a present empty string, whereas
an invalid DataObject format index returns `null`.

## Complete CodeEdit text

CodeEdit accepts explicit byte lengths, including embedded NUL. Consequently
its full text must not be read as a C string. `CodeEdit.getText()` directly
declares `xuiCodeEditGetTextView` using the existing general contract:

```xlang
@no_raise @c_output(1,0) @c_output_view(1,"text")
```

The physical native result is a status plus `xstrview*`. The compiler checks
status, pointer and extent, then copies exactly that byte count into a
`string?` while the receiver scope is still active. NULL with nonzero length
and an unrepresentable length are rejected. Native failure is handled before
looking at its output. Allocation failure throws and leaves an assignment's
previous destination intact. This reuses the established output-view
implementation; no new language syntax, operation dispatcher or widget
business wrapper is needed.

For an already created editor:

```xlang
assert editor.setTextLength("A\0你B")==0
string snapshot=editor.getText() as string
assert snapshot=="A\0你B"
assert editor.setTextLength("changed")==0
assert snapshot=="A\0你B"
```

`Widget.editGetText()` uses `xuiEditGetTextView`. Delegation to CodeEdit also
preserves its full byte extent. Other native edit adapters retain their
C-string storage contract. A non-editable target has an absent result. The
CodeEdit-specific native route obtains its existing document and does not scan
for NUL; the generic edit route additionally resolves the native adapter and
identifies CodeEdit through its registered widget type. Other adapters require
a NUL scan. These native operations are in addition to receiver admission and
checked owned-string copying.

XGE's new `xuiCodeDocumentGetTextView` returns a checked borrowed document
snapshot. A cold nonempty document may allocate and flatten its snapshot;
cached reads reuse that pointer without allocation. The language still makes
one independent string allocation/copy for each nonempty result. The native
empty-document view does not allocate. Views remain borrowed until document
mutation or release. The existing raw C-string APIs are retained for native C
callers; xlang's two safe full-text declarations use the checked extent APIs.

## Verification boundaries

`tools/gen_xge_cstring_probes.py` generates the catalogue in
`test/fixtures/foreign_xge_cstrings`. Its exported probes exercise every
declared direct C-string getter for compilation, linking and public C/C++ ABI
checks. They do not execute every widget or certify all model lifetimes.

`tools/check_xui_text_reads.py` requires a full qualified compiler and matched
dynamic/static SDKs. It separately executes representative real Label,
Button, Input, TextEdit, CodeEdit, MsgBox, CodeFindScope and XGE/XUI DataObject
models. The generic output-view fixture covers malformed extents, NULL/empty,
poison output on failed status, multi-output atomicity and OOM. The native gate
`tools/test/check_text_read_views.py` covers checked CodeDocument reads plus
existing generic edit/TextEdit regressions at GCC O0/O2. It measures 1000
cached native document reads without XRT heap operations.

The broad native CodeEdit UI suite originally asserted its find table before
the native 80 ms debounce update. Both previous and new SDKs reproduced that
failure; those historical reports remain unchanged. Native test successor
`25598bcf` advances the update loop by 40 ms (still pending) and another 50 ms
(three columns/three rows). The expanded native gate now passes all 16 commands
on Windows/Linux, including the complete CPU-Proxy CodeEdit suite at O0/O2.
Only the test drive changes; native find behavior and SDK bytes are unchanged.
See `xui-text-find-debounce-20261113.json`. Real graphics acceptance and final
SDK/IDE delivery remain separate from these CPU checks.
