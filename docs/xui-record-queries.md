# XUI record queries

This batch reuses the qualified `c_output_field(...,"cstring")` contract.
It adds no compiler/runtime implementation or native forwarding functions.
Every query calls its native function once, copies borrowed text before its
input scope ends, then delivers the complete tuple. Failed native status,
text conversion or scope cleanup leaves the caller's previous variables
unchanged and releases partially converted strings. This does not undo a
native operation that already happened.

## Direct declarations

| Language method | Native function | Tuple fields in native order |
| --- | --- | --- |
| `CodeAnnotationStore.getDiagnostic(index)` | `xuiCodeAnnotationGetDiagnostic` | range, severity, code?, message?, source?, user ID |
| `CodeEdit.getVirtualText()` | `xuiCodeEditGetVirtualText` | kind, offset, text?, flags, user ID |
| `Widget.getAccessibleNode(index=0)` | `xuiWidgetGetAccessibleNode` | ID, parent ID, role, state, name?, value?, description?, bounds, text start/end, level, row/column, row/column counts, actions, range |
| `Context.styleGetToken(name)` | `xuiStyleGetToken` | type, integer, float, color, text? |

Integer widths and signedness follow the C records, including `uint64` node
IDs, `c_uint` flags and `c_uintptr` user IDs. `Rect` and `AccessibleRange` are
copied PODs. The ABI size member is initialized automatically. NULL is distinct
from an empty string; every non-NULL string becomes an independent snapshot
and survives later mutation or closure of its original model/Context.

Two matching input declarations use stack `c_pack` records:

```xlang
ui.styleSetToken("spacing",xui.XUI_STYLE_VALUE_INT,integer=8)
ui.styleSetToken("caption",xui.XUI_STYLE_VALUE_STRING,text="Ready")
auto kind,integer,number,color,text=ui.styleGetToken("caption")

editor.setText("abc")
editor.editSetSelection(1,1)
editor.setVirtualText(xui.XUI_CODE_VIRTUAL_TEXT_INLINE_COMPLETION,1,"completion")
auto virtualKind,offset,completion,flags,userId=editor.getVirtualText()
```

These methods preserve native rules. Setting virtual text requires a single
collapsed selection at the given offset. The current native implementation
ignores the setter's flags and user ID and reports zero for both. Absent
virtual text is `XUI_ERROR_UNSUPPORTED`, not an invented successful empty tuple.
Missing style tokens and invalid diagnostic indexes are checked errors.
Style token names and packed text reject embedded NUL before the native call.

The store is an independent owning model; its creation-thread and explicit
close rules apply. Widgets and style queries use the existing Context domain:
wrong-thread or closed/expired access is checked before the native query.
This batch does not introduce a platform accessibility bridge or a custom
accessibility provider. The default widget node is a local node, not a global
physical widget-tree enumeration.

## Qualification and costs

`tools/check_xui_record_queries.py` requires a full qualified O2 compiler and
matched host/static SDK. It checks handwritten C O0/O2, generated C/C++ public
headers, CLI JIT and standalone dynamic/static AOT on Windows and Linux.
`test/cases/foreign_xui_record_queries.*` separately cover every output field,
direct/bound/protocol calls, Chinese text, NULL/empty, native failures, thread
and expiry rejection, and 32 allocation prefixes for each of four query
families. The 17-field accessibility transaction checks preservation of all
scalar/POD/string destinations on failure. The independent native oracle
checks native statuses and fields. No network or security tests run.

For 1000 prepared calls, the equivalent handwritten C baseline makes the same
owned text snapshots from the same native object in the language test:

| Query | Language allocations/frees | C allocations/frees | Reallocations |
| --- | ---: | ---: | ---: |
| Diagnostic, three non-NULL strings | 3000 / 3000 | 3000 / 3000 | 0 |
| Virtual text, one non-NULL string | 1000 / 1000 | 1000 / 1000 | 0 |
| Accessible node, two non-NULL strings | 2000 / 2000 | 2000 / 2000 | 0 |
| String style token | 2000 / 2000 | 1000 / 1000 | 0 |

The style query allocates once more per call to validate/terminate its input
C-string name. Each non-NULL output scans and copies its C string, allocating
owned storage; empty strings remain present strings. Native getters merely
return borrowed records and do not themselves require these copies. The C
baseline deliberately performs equivalent ownership work rather than claiming
that borrowed C results have the same cost as safe language strings.

Native input scopes, resource/code lifetime checks, status/error checks,
tuple/POD copies and transactional destination writes remain. This table
measures heap operations only, not CPU equivalence or whole-API zero overhead.
Input setters can additionally copy their text into native storage. Cold
model/Context construction follows the existing owning-control contracts.

The readable `lib/bindings/xui/examples/code_annotations.xl` now publishes and
reads one diagnostic. Its printed code, message and source map directly to
the input columns. This batch does not add another example runner or framework.

## Remaining scope

Bulk diagnostic outputs still require the pending writable/output-array
contract. Nested style-property records and callback-backed providers are
separate contracts. These six declarations do not establish completion of all
annotations, style or accessibility APIs. Full XGE/XUI/Document integration
and the final matched SDK/compiler/runtime/IDE/help delivery remain active.
Qualified SDK/compiler cohorts are reused without rebuilding, and this batch
does not replace `release/` or update the current delivery index/version.
