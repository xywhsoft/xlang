# RadioGroup option creation

`group.addOption()` packs the complete `xui_radio_desc_t` on the call stack
and calls `xuiRadioGroupAddOption` once. Its nine public parameters follow
the native descriptor order: nullable text, nullable Font, text/disabled
colors, text flags, checked state, indicator size, gap and built-in atlas.
Omitted or zero fields retain native defaults. Text is copied by XGE;
embedded NUL is rejected before calling C. Font ownership is admitted by the
native Widget resource slots and must match the Context's Proxy.

The returned Radio has the Context's creator-thread and lifetime contract.
The Group owns its attached native child. Destroying the Group invalidates
the returned child view. Closing a child detaches and destroys it. The
existing factory transaction rolls back native creation if language owner
registration fails; a failed assignment preserves its previous value.

Selection creation does not synthesize notifications. A selection-change
observer receives `(Widget group, Widget? selected, c_int index)`; clearing
selection delivers NULL and -1. Its Widget views retain the existing
callback-epoch limits. The nullable declaration corrects the former public
type's rejection of this native notification.

The matched XGE SDK also fixes two pre-existing Radio defects. A failed
Radio initialization releases its partial text allocation; the generic
constructor chain continues to destroy only successfully initialized stages.
Radio's existing surface-generation probe now participates in clean-frame
preparation, so mutations of borrowed indicator surfaces rebuild the cache.
This adds a per-frame dependency probe even when the Radio is clean; it
does not imply zero CPU overhead or force unchanged caches to rebuild.

The standalone `examples/radiogroup.xl` and its generated gallery scene use
the new factory directly. Separate native and language fixtures cover all
descriptor fields, nullable selection, Proxy identity, Font references,
allocation failures, parent destruction and thread violations. Full compiler,
hosted/static SDK and runtime input hashes are recorded in the corresponding
provenance receipt. Linux window checks use Xvfb/Mesa, not physical drivers.

After warming both paths, 100 create/destroy operations perform 700 language
versus 400 C allocations and matching frees; neither path reallocates. This
includes the native constructor's text copy and the language path's managed
view, code-lease and owner-directory work. The packed descriptor itself uses
stack storage; prepared text is not mirrored by the argument projection.
Four queries per round over 1000 rounds perform zero heap operations on both
paths, but language thread/liveness checks and call bookkeeping still run.
No CPU latency or throughput equivalence is claimed. The native Radio cache
regression also checks unchanged surfaces reuse their caches before testing
generation changes.

The tiny font fixture used by the main language gate does not include CJK
glyphs. A separate bounded rerun uses an installed Chinese font without
redistributing it. Each platform's JIT/dynamic/static pixel results must match;
the Chinese labels are visually reviewed separately from the tiny-font gate.

Full XGE/XUI/Document integration and the other pending design decisions are
still open. This batch does not install a shipping release or certify all
other built-in Widget initializers against allocation failure.
