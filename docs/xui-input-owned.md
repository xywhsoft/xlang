# Owned input callbacks and error observations

This requires the matched XUI SDK with `xuiHotKeyRegisterOwned` and
`xuiSetErrorCallbackOwned`, plus the compiler's C string pointer-field snapshot
contract. The existing raw native APIs remain available to C; safe language
closures use these owned registrations directly.

## Calls and lifetime

`Context.hotKeyRegister(target,key,modifiers,callback)` retains a nullable
`WidgetEventProc`. Its callback receives an owned Widget argument and a
callback-scoped `CallbackConstEvent`; the event type is `XUI_EVENT_HOTKEY`.
Returning `XUI_EVENT_DISPATCH_STOP` consumes it. A NULL callback retains the
native Widget HOTKEY routing behavior; use `hotKeyUnregister` or
`Widget.hotKeyClearWidget()` to remove registrations. Target destruction also clears
its hotkeys. A target must belong to the receiving Context.

`Context.setErrorCallback(callback)` retains a nullable `ErrorProc`. NULL
unregisters the observer. It receives `CallbackContext` and
`CallbackConstErrorInfo`, both valid only during the callback. The latter
exposes numeric code/stage/recoverable, a copied rectangle and nullable Widget,
operation and message properties. Its Widget is another callback view, not an
independent owned Widget. `CallbackContext.getLanguage()` and `requestClose()`
operate in that same epoch; a captured ordinary Context can use its other
checked creator-thread methods.

`operation` and `message` snapshot borrowed C string fields as `string?`.
NULL is absent, an empty non-NULL field is present, and OOM throws without
replacing an existing assignment. Saved strings survive callback return.
Saving an ErrorInfo, Context or Widget view does not extend the epoch;
accessing it afterward throws a checked state error.

All registrations and removals require the Context's creator thread. Native
publication retains the new owner before replacing the old registration.
Allocation/owner-hold failure leaves the old callback in place. Each selected
hotkey and active error observer retains its registration record through the
invocation. Callbacks can unregister or replace themselves without freeing
their active closure. Native ownership tracing visits these registered owners,
and retirement waits for active operations. Closure, Context and code ownership
still follow the general language resource/collector contracts.

## Error delivery

This observer receives native framework reports such as invalid queued IME
events; it is not a global hook for every checked API exception. A native pump
may report and skip a malformed event, then return success. Its observer must
not treat that return as an aggregate delivery acknowledgement. Normal language
callback exceptions retain their checked delivery contract.

An observer may request or perform Context close. The native pump now stops
after a report if that observer logically closes the Context; it no longer
dispatches the next queued command after close. The standalone native fix was
reproduced through the old public API and committed before the owned APIs.

## Costs and tests

These are direct declarations of native owned APIs, not C business forwarders.
Each native owned registration allocates one 48-byte record and holds its owner
once. Replacing a registration retires its old record after active snapshots
finish. A hotkey table entry grows from 56 to 64 bytes; the Context grows by
136 bytes, including its 16 inline entries and observer pointer. Selecting up
to eight hotkeys uses local storage; larger snapshots can allocate a native
temporary. Existing copied command names retain their native costs.

Invocation pins the native record rather than calling arbitrary owner Hold for
each dispatch. It still executes receiver/code/epoch/error and lifetime checks.
Error string snapshots add a NUL scan, allocation and copy per nonempty field.
Neither all callbacks nor the entire binding are claimed zero-cost.

`tools/check_xui_input_owned.py` tests a full compiler and matched SDK through
JIT, independent dynamic/static programs and three C/C++ header modes. It checks
metadata, view expiry, self-removal/replacement, 32 allocation budgets for each
registration family with transactional replacement, hotkey/observer exceptions
and foreign-thread refusal.
In 1000 prepared hotkey calls, language and handwritten C owned callbacks both
perform zero XRT allocation/free/reallocation; this is not a CPU-speed claim.
Optional two-frame rendering/readback runs the readable
`lib/bindings/xui/examples/input_callbacks.xl` lesson. It does not certify OS
keyboard/IME input. Native source tests separately cover graph references,
reentrant drops, callback errors, observer close, allocation prefixes and bulk
clear. Final SDK/compiler/IDE installation is a separate delivery stage.

Windows and Linux each pass 19 main language rows, nine dedicated callback-close
rows, eight unused-declaration trimming rows, 40 native ownership/edit/cost rows
and four standalone native close-fix rows. Removing these unused owned callback
declarations and callback-only types changes neither file bytes, text nor BSS
in the same SDK/compiler's CodeModels program. All three same-platform two-frame
readbacks are identical and were visually reviewed; platforms are not compared
pixel-for-pixel. The small provenance index is
`dev/release-materials/provenance/xui-input-owned-20261114.json`.
