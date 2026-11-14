# XUI input results and command events

`Context.inputKeyDownEx(key, modifiers=0)`, `inputKeyUpEx(key, modifiers=0)`
and `inputTextEx(codepoint)` return a `c_uint` result mask. Combine/test the
existing `XUI_INPUT_RESULT_CONSUMED`, `FOCUS_CHANGED` and `HOTKEY` constants
using `c_uint(...)`. These methods synchronously route one native input call.
Invalid native arguments and language callback errors throw before publishing
an output; an assignment keeps its previous value. Modifiers follow native
SHIFT/CTRL/ALT/SUPER normalization. Text accepts Unicode scalar values excluding
NUL; surrogate codepoints and values above U+10FFFF are checked errors.

`inputImeCompositionEx(text, active=1, cursor=0, selectionStart=0,
selectionEnd=0, hasReplacement=0, replacementStart=0, replacementEnd=0,
attribute=0)` passes a size-prefixed `xui_ime_composition_t` on the stack.
Text is an explicitly counted UTF-8 byte span; cursor, selection and replacement
positions are byte offsets. Native validation/clamping and widget behavior stay
authoritative. Active preedit and a nonempty committed value are different;
an active empty composition does not mean end-of-composition. An empty inactive
value ends composition. Replacement positions refer to the existing document.

IME injection enqueues a copy. Call `dispatchPendingEvents()` to deliver it;
the normal example window loop already does this. The native event text capacity
is 4096 bytes, with at most 4095 payload bytes for a normal event. An oversized
composition or invalid descriptor range fails at injection. Interior NUL is
copied by the producer but rejected at event dispatch. The native pending-event
pump reports such malformed events to its error observer, skips them, continues,
and can return success. It does not promise aggregate event-handler error status.
This preserves native behavior; injection success is not a delivery acknowledgement.

`hotKeyRegisterCommand(target, key, modifiers, command, name)` registers a native
command hotkey. `target` must belong to this Context. `dispatchCommand(target,
command, name)` enqueues the same native command event. A nullable target is
passed literally: a command with no target is not redirected to the focus widget.
Handle `XUI_EVENT_COMMAND` on the target and return `XUI_EVENT_DISPATCH_STOP` when
consumed. The native registration keeps its weak target and copied name; widget
destruction removes its hotkeys. `hotKeyUnregister` removes a registration and
is idempotent. Native fixed-buffer truncation of long command names is unchanged;
prefer short stable command identifiers. An empty hotkey command name selects
the native `XUI_EVENT_HOTKEY` route rather than `XUI_EVENT_COMMAND`.

These two APIs pass an exact typed NULL for native `pData`; they store no raw
language pointers. Language handlers use the existing owned Widget callback
protocol. The raw arbitrary `xuiHotKeyRegister` callback is not exposed as an
unowned language closure. Supporting that and Context's raw error observer
requires a native owned-registration contract before safe language admission.

`CallbackConstEvent.text` copies exactly `iTextSize` bytes from the native
fixed-size `sText` field while the callback view is active. The compiled field
projection checks negative/over-capacity counts before reading. No `strlen`
truncation occurs, so a controlled foreign producer's interior NUL is preserved.
Normal SDK dispatch has the stricter validation described above. An independent
string survives callback return; the saved event view expires. Nonempty text
requires an ordinary language string allocation/copy, and OOM is checked.
The getter reuses existing general counted-field and callback-epoch contracts.

## Costs and acceptance

There is one direct C call per input/registration/dispatch operation, with no C
business forwarder, opcode dispatcher, dynamic boxing or per-IME heap descriptor.
Command-name C-string admission checks for interior NUL before native code;
those name paths scan their input. The descriptor is stack storage and the input span keeps its backing alive for
the call. Native queue copies, native registration/name storage, owned callback
registration and normal receiver/code/epoch/error checks remain real costs.
Changing a widget's actual text can allocate and shape text; these operations
are not covered by a zero-allocation metadata claim.

`tools/check_xui_input_extended.py` uses a complete qualified compiler and one
matched SDK. Its independent C clients link the actual SDK at O0/O2 in hosted
and static modes. JIT/dynamic/static language cases cover consumed/focus/hotkey
flags, Unicode Input/CodeEdit IME commits, full descriptor metadata, NULL target,
checked output preservation, bound values, cross-Context/thread rejection,
callback failure/active Context close, event expiry, bounded/NUL text and copy OOM.
The CPU backend's shaper only groups UTF-8 codepoints; it is not certification
of complex grapheme, bidi, ligature or real font rendering behavior.

In 1000 prepared rounds, four input calls plus one queue clear per round have
zero XRT alloc/free/realloc in both language and handwritten C. This deliberately
uses a focused generic Widget without text mutation or handlers and a warmed
queue, not arbitrary input workloads. No CPU equivalence or whole-binding zero
bridge overhead is claimed. `--window-tests --font ...` separately exercises the
readable `input_events.xl` lesson's real two-frame renderer/readback in all three
link modes; this is bounded rendering, not physical keyboard or OS IME certification.
No new compiler/native engine implementation or final release installation belongs
to this declaration batch. See the small provenance index for immutable evidence.

Windows and Linux each passed 30 main command rows plus eight static trimming
rows. Removing the seven unused declarations from the same package/SDK changed
neither executable bytes, text nor BSS in the existing CodeModels lesson.
All three same-platform window readbacks are byte-identical and were visually
reviewed; the platforms use different font/rendering paths and are not compared
pixel-for-pixel with one another. The final r2 package differs from the executed
r1 candidate only in gallery JSON line endings restored to the repository's
original CRLF. Exact executable/source/header/SDK bytes and JSON values match;
this equivalence check is not presented as another runtime execution.
