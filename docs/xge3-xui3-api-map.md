# XGE 3 / XUI 3 declaration inventory

The authoritative machine-readable inventory is
[`xge3-xui3-api-map.json`](xge3-xui3-api-map.json). It records the SHA-256 of
the matching Grid r5 SDK's `xge.h` and `xui.h`, every public function and typedef,
record fields, callback signatures, numeric/object macros and complete
function-macro replacement lists. Disabled-feature declarations remain in the
inventory. An entry is not a statement of runtime support.

This source candidate contains 4,454 functions, 854 types, 1,818 object
macro definitions, seven function macros and 157 callable struct fields, with
zero parse errors. The default and optional declarations map 394 XGE and
2,277 XUI native functions. The separate
[`upstream inventory`](xge3-xui3-api-map-upstream.json) retains newer native
development APIs, including the later shared Proxy resource protocol. Counts of
declarations, unique C functions and reachable probe methods differ: lifecycle
controls and callback view operations can share symbols. The JSON is the
authority when headers change.

Preprocessor build controls such as DEBUG/RELEASE/HAS_DEBUGMODE are recorded
but are not guessed from the first textual branch and exported as constants.
The matching SDK build configuration selects those branches. Numeric and text
API constants, including expanded literal RGBA constants, are declared values.

## Mapping rules

| Native contract | Language representation |
| --- | --- |
| Pointer-free complete record | Exact C record with header size/alignment/field proof |
| Context or owned native reference | Owned foreign class with checked native policy |
| Widget and derived control | Context-owned view, native lifetime watch and call scope |
| Persistent callback | Typed native Owned registration, independent closure owner per callback |
| Callback-only input/output record | Checked callback epoch view; private fields hidden |
| Synchronous pointer plus length | Explicit reviewed span; no pointer retention inferred from `const` |
| Native status and one/multiple outputs | One native call and logical value/tuple result |
| Size-prefixed native record output | Exact member initialized with native `sizeof` |
| Borrowed text/bytes output slot | Exact view-pointer ABI and atomic owned result admission |
| Native borrowed text/value result | Language value acquired before native scopes end |
| Raw descriptor with nested pointers | Explicit `Native` C interop name and caller lifetime |

Widget `close()` calls `xuiWidgetDestroyChecked`. Internal control
parts can reject destruction while retaining a usable view. Derived control
actions use `closeMenu`/`closePopup`/`closeBrowser`, so they never silently change
the inherited lifetime operation. Factory rollback uses the same checked
native destructor and preserves the original producer error.

There are no per-method C wrappers. Common runtime ownership policies and
callback thunks still perform real work. Owned construction allocates control
records, takes code ownership and registers its graph. Method scopes validate
active roots and native thread/lifetime rules. Callback installation allocates
and retains a closure; callback dispatch enters activity/epoch/error scopes.
Text or byte result materialization copies; borrowed dynamic value results
retain. These costs must not be described as universal zero overhead.

## Storage review

[`xge3-xui3-api-map-storage.json`](xge3-xui3-api-map-storage.json) records native
consumer locations, body hashes and supporting copy/helper paths. The
generator requires an explicit reviewed entry and unchanged consumer hash
before creating a pointer/length span. A matching C signature alone is not
proof of non-escape. New or changed consumers remain
`requires-pointer-storage-review`.

`xuiListViewSetSelectionBuffer` stores its external mutable array. It is not
exported as an ordinary Buffer span: retaining a Buffer would not prevent a
later reallocation. Normal list selection APIs remain available. A future
native fixed-capacity owned storage object can expose this optimization safely.
`xgeMiniProgramAudioCommand` forwards data to an arbitrary host bridge without
a documented consume/copy contract, so its bytes span is also withheld.

## Remaining ownership work

The inventory explicitly retains `requires-contract-review` entries. This
includes native internals consumed by policies, old raw callback APIs replaced
by Owned APIs, and real public gaps; the state does not mean every entry needs
a separate language function.

`xge.getBuildInfo()` uses the generic `c_output_size` contract to initialize
the exact native record size before its direct call. The compiler proves the
member type and range against the authority header. This declaration needs
the compiler cohort containing that contract; older compiler binaries cannot
consume the new source package.

Proxy functions are callable fields, not exported global symbols. XUI's Font
wrapper differs from an XGE Font, and current default/registered/widget font
slots do not retain it. TextLayout copies its text but retains raw Context and
Font addresses for later recomputation. Surface draw contexts retain raw target
addresses during drawing. The native shared ownership decision and the generic
callable-field language contract must cover these relationships before they
become safe managed resources. See
[`xui-proxy-resource-contract.md`](xui-proxy-resource-contract.md).

Constructors accepting descriptor pointers are deliberately named `Native`;
they do not constitute the final convenient managed configuration API. Other
code-editor models, renderer/backend resources, 3D resource graphs and audio
resources remain individually classified in the JSON instead of receiving
invented leaf or owner contracts.

## Validation evidence and boundaries

The R30/R31 frozen compiler cohorts were built completely on Windows/Linux.
The declaration contract gates cover field aliases, exported callback typedefs,
const pointers, authoritative headers, multiple outputs, dynamic value borrow
and copy, ownership domains and checked rollback. They include strict native
header proofs, O0/O2 execution, declaration negatives and IR corruption gates.

All-reachable API probes emit C proofs for actual calls. R15 passed 22 of 23
strict compile-only batches; its one remaining batch referenced a new Grid
draft absent from that pinned SDK. The subsequent Grid r2/r4 batches use the
matching Grid r5 SDK. An exact method/signature/expression comparison covers
all 2,663 current probe methods with successful batches;
the unrelated failed rows remain preserved. Optional 3D proofs used a
separate full-feature engine build and were never executed. This is compile
evidence, not coverage of native rendering or every ownership behavior.

Real package Frame and Terminal declarations passed headless Windows/Linux
O0/O2 execution with the matching Frame engine and one host XRT. Frame covers
escaped captured closures, replacement/self-clear, checked negative status,
language exceptions and shutdown. Terminal covers explicit UTF-8 byte length,
bytes value copying, callback epoch expiry and Context shutdown. Reports remain
at `F:/xlang-validation/foreign-completion-20261103/binding-{frame,terminal}-r31-win-r1`
and `/home/ubuntu/xlang-validation/binding-{frame,terminal}-r31-linux-r1`.
No window, GPU, network or security tests ran. Proxy resource APIs need their
own matching native candidate; older reports are preserved unchanged.
Guard's protected-child rejection/recovery and actual Merge output mutation
also passed headless Windows/Linux O0/O2 gates. The scalar callback gate covers
int/float/bool direct pointee access and rejection after callback expiry.

Grid's native v16 protocol passed 256 Windows/Linux build/run rows, recorded
in `F:/xlang-validation/foreign-completion-20261103/native-callback-grid-cohort-r1.json`.
The language declaration gate then ran actual owned adapter, config and editor
callbacks with the matching SDK on both platforms at O0/O2. It verifies copied
temporary text, enum-item multi-output strings, scalar palette reads and the
bulk palette span, plus checked access rejection after callback expiry.
Reports are `binding-grid-output-view-win-r5` under the validation root above
and `/home/ubuntu/xlang-validation/binding-grid-output-view-linux-r3`.
These gates explicitly use a compiler-only frontend overlay over pinned full
R31 runtime objects, rather than claiming another full compiler/VFS rebuild.
The separate `binding-probes-grid-r31-coverage.json` pins the successful
compile-only method coverage and its preserved historical reports.
