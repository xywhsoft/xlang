# XUI language packs

`Context.createLanguage(code,name,fallback=0)` creates a Context-owned Language.
`Context.languagePack(id)` returns a watched pack or null. Packs keep their real
Context alive when a local Context variable goes out of scope. Explicitly
closing that Context expires all pack aliases and their methods reject access.
Dropping a Language value does not unregister the native pack. Language has no
separate public close operation.

| API | Native operation |
| --- | --- |
| Context.createLanguage | xuiCreateLanguageOwned, generic owner-view factory |
| Context.languagePack | xuiGetLanguagePack, nullable owner-view query |
| Language.id / code / name | Native ID and copied C-string metadata |
| Language.textCount / text(id) | Native own-slot count and copied own text |
| Language.textAt(index) | Checked indexed read of the stored ID and a nullable text snapshot |
| Language.context | Same Context root, retained by the existing owner-view rule |
| Context.setLanguageText | xuiLanguageSetText with checked C-string input |
| Context.clearLanguageText | Same native operation with compile-time const-char NULL |
| Context.touchLanguage | Native revision/invalidation, including its cross-Context no-op |

The existing Context setLanguage/getLanguage/getLanguageRevision/translate
methods retain their native behavior. Text IDs are the closed `XUI_TR_*` set;
slot IDs start at one. `Language.text()` reads that pack's own override: unset
is null, present empty text is `""`. `Context.translate()` treats both unset and
empty custom overrides as missing, checks the configured fallback pack once,
then English. It is not recursive fallback traversal. Clearing removes an
override; setting an empty string stores an actual empty slot.

Pass a zero-based position to `Language.textAt()` to enumerate a pack's own
slots up to `textCount`. The returned ID is the translation key. An unset slot
still returns its ID with null text. An out-of-range position raises a checked
error. The native call reads one slot directly and the language copies its
string before leaving the Context scope.

In 128 indexed reads of a nonempty translation, native C used zero heap
allocations. The language path allocated and freed one string per read on
Windows and Linux. The checked call preserves the caller's output on allocation
failure. See the [enumeration example](../lib/bindings/xui/examples/localization_entries.xl)
and [test evidence](../dev/release-materials/provenance/xui-language-entries-20261124.json).

Code/name/text inputs are copied by XUI and reject embedded NUL at the language
C-string boundary. Returned metadata and text are independent snapshots copied
before the Context call scope ends. They survive later mutation or Context
close. No mutable xarray, allocation flag or borrowed character pointer escapes.
Native C still has its original explicit array interoperability API.

The factory reserves the usual observer, admits a child under its exact
Context, and uses the native rollback protocol if language publication fails.
Native rollback rejects built-in, active and fallback-referenced packs. It
does not rewind consumed IDs, revision counters, registry capacity or already
scheduled invalidation. A failed text replacement instead leaves its previous
slot/revision intact. Callers cannot transfer a pack to another Context;
setLanguageText reports the native checked failure for that mismatch. The
common creator-thread policy rejects calls from other language threads.

Production business methods are direct declarations. The small new runtime
policy implements watch/rollback lifecycle bookkeeping only. The generic
typed-NULL compatibility fix admits the same exact `const char*` ABI as explicit
`c_cstr` input; mutable, void/other-character, nullable/view, lifetime/effect and
header differences remain rejected. There is no new syntax, per-method C
forwarder or operation dispatcher.

Scalar metadata queries and their 1000-round native/language hot comparisons
have zero XRT heap operations in Windows/Linux JIT and dynamic/static CPU
acceptance. Each round makes two native calls. Native pack
storage grows by one pointer per pack for its lifetime-link list. Cold creation
still allocates strings, slot storage, a registry entry and language controls;
the native checked constructor and ordinary managed calls also perform scope,
thread, owner, code-lease and error bookkeeping. Nonempty C-string snapshots
scan, allocate and copy. No equal-CPU or whole-binding zero-overhead claim is
made.

`localization.xl` is a short independent window lesson. The catalogue now
registers all 80 existing entries, including eight previously omitted lessons;
the 57-scene gallery is regenerated from its original standalone source bodies.
Localization stays outside that shared Context gallery so it cannot change
another scene's language or accumulate packs on each visit.

Native prerequisite `a77ee176` has Windows/Linux host C O0/O2, C++ prototypes,
48 native allocation budgets and existing editor regressions. The downstream
case covers fallback, own-slot NULL/empty, snapshots, owner retention, thread
and expiry errors, and 64 managed publication budgets. Complete O2 compilers
consume the matched host/static SDKs; diagnostic overlays are not substituted
for the actual JIT/AOT program runs. The dated provenance receipt records the
exact successful gates and compiler inputs.

Window readbacks use a hash-pinned bundled CJK font, and the Chinese label is
visually reviewed. Windows uses its actual window backend. Linux uses Xvfb
with Mesa llvmpipe software OpenGL; this does not certify a hardware GPU or all
platform backends. The shared lesson helper also finds the bundled font when
run from the source or example directory before falling back to system fonts.
The generated gallery has separate whole-source C generation and bounded
two-frame JIT checks of its initial scene; this does not re-exercise all 57
scenes or interactions.

The first Linux window attempt without DISPLAY logged XOpenDisplay failure
and then SIGSEGV. The original Xvfb success did not fix that native gap. Native
commit `e1089362` subsequently adds proper cleanup/return at that exact failed
connection stage and preserves checked Run status; the matched successor SDK
has separate no-display and actual-window regressions. See
[platform startup](xge3-platform-startup.md) for scope and remaining fatal-stage
limits. Earlier failure reports and the diagnostic trace remain unchanged
outside the repository. These batches do not install a final runtime, SDK or
IDE delivery or complete the full XGE/XUI/Document goal.
