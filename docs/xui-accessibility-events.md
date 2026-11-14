# Accessibility event observers

`Context.setAccessibilityEventCallback(callback)` uses the native owned
observer API. Pass NULL to unregister it. The callback receives a
`CallbackContext`, `CallbackWidget` and `CallbackConstAccessibilityEvent`, each
valid only for that invocation. The event exposes `eventType`, unsigned 64-bit
`nodeId` and unsigned `revision`. Node IDs belong to the notifying Widget's
provider, not a global namespace. Native node 0 selects the Widget itself.

`Widget.notifyAccessibility(type,nodeId)` dispatches synchronously. Native
semantic state notifications are coalesced until the next outer update.
Recursive notifications for the same Widget/event kind are suppressed. Keep
notification callbacks small; the example records numeric state and lets a
button callback update the visible label.

Registration holds the closure before publication, and a failed allocation or
owner Hold retains the previous observer. Active snapshots keep an observer
alive through self-unregistration or replacement. Context ownership tracing
includes its registered observer, and close disables observers before dropping
their owners. Calls require the creator thread. Callback/owner cleanup failures
use checked outcomes; notification follows a change and does not roll it back.

Saved callback views expire at return. Copy numeric fields into ordinary values
when retaining them. A callback may request Context close; native operation
scopes protect the Widget/Context until native work and snapshot cleanup finish.
The callbacks use the existing generic native owner, epoch and error contracts;
no compiler/runtime contract or C forwarding wrapper is added.

Each owned registration adds one 48-byte native record plus normal language
closure/code/capture admission. The Context holds one additional pointer.
Dispatch retains the native record instead of calling arbitrary owner Hold on
each notification. A Context without an observer skips the snapshot mutation
gate. Checked scope/code/epoch/error bookkeeping remains a real cost.

This observer does not install an operating-system accessibility bridge. Safe
provider callbacks and borrowed-string node-query outputs retain their own
integration work. The source test uses the actual SDK with a CPU backend;
the readable `examples/accessibility_events.xl` exercises normal window
rendering. These do not certify screen-reader interaction.

Windows and Linux each pass 19 full-compiler language/header/real-window rows,
eight same-SDK declaration-trimming rows and 32 native owned/legacy regression
rows. All three same-platform two-frame readbacks are identical and were
visually reviewed. The controlled callback checks node IDs above both 2^32 and
2^63. Prepared 1000-call C and language counter callbacks perform zero XRT
allocation/free/reallocation; this is not a CPU speed measurement. The x64
Context grows from 147840 to 147848 bytes, with the 48-byte record unchanged.
No compiler/runtime rebuild is required by these declarations; they use the
previously qualified full O2 compiler. Final matched release installation
remains separate. See
`dev/release-materials/provenance/xui-accessibility-events-20261115.json`.
