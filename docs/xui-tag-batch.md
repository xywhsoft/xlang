# Batch tag input

`TagInput.setTags(array<string> tags,int size=-1)` calls `xuiTagInputSetTags`
once, using the existing typed C string-array projection. The native widget
copies its text; changing or releasing the source array later cannot change
the tags. The original order and duplicate tags are preserved.

Each selected string is trimmed using the native leading/trailing characters
space, tab, CR, LF, comma and semicolon. This does not split internal delimiters
like interactive text commit does. Empty or fully trimmed items are skipped;
the resulting tag count can differ from the input count. Maximum text length
uses the native byte limit and grapheme-boundary clamp. Batch capacity and the
widget's configured maximum tag count still apply.

`size=-1` selects the whole array. A nonnegative size selects the available
prefix, including zero to clear the widget; an oversized request clamps to the
array length. Other negative sizes, embedded NUL in selected text, allocation
failure and native capacity/validation errors are checked failures. Preparation
and allocation failures retain the previous tags. A notification failure after
native publication does not roll back the new tags.

The public array contains strings, matching other tag/text-list APIs. Native
NULL and empty entries have exactly the same skip behavior, so empty strings
express that behavior without introducing another public array shape. Arrays
with nullable elements are not silently converted. The existing compiler still
rejects distinct string-array projection shapes declared against the same C
target; this batch adds no compiler rule or C forwarding alias to bypass it.

Array storage stays read-pinned through the call and cleanup. Callback attempts
to change it fail checked. Existing owned change observers can unregister,
throw or close their widget; owner/thread and expired bound-call checks remain.
The independent C client also checks native self-close and final heap cleanup.

This is not zero-cost marshalling: each call prepares one native pointer array,
checks/scans selected strings for NUL and retains their backing. String bytes
are not copied by the projection. Native trim/clamp/copy work is unchanged.
In 1000 prepared replacements of three existing equal tags, language and C
allocate/free 4000 versus 3000 objects, with zero reallocations: one extra
pointer-array allocation per language call. Native SetTags still allocates its
three temporary strings even when the resulting content compares equal. This
heap comparison does not establish equal CPU cost.

`examples/taginput.xl` initializes its three labels in one call; the generated
gallery reuses that initializer. Separate fixtures check copies, trimming,
duplicates, empty/prefix inputs, UTF-8 clamping, limits, failed publication,
32 allocation budgets, read pins, observer errors/self-close, foreign threads
and expired bound calls. No child process, clipboard or network is involved.

Windows and Linux each pass 30 functional, C/C++ header, native O0/O2
hosted/static and language JIT/dynamic/static command steps. The lesson renders
two frames in each mode; same-platform readbacks match and the images were
inspected. Linux uses Xvfb/Mesa software OpenGL, without physical-driver claims.
The generated gallery source is checked against the standalone initializer;
this batch does not claim a new full-gallery slideshow run.

The final package changes only the README from the executed package. A single
check-script metadata label was corrected from a copied-record label to a
string-pointer-array label; exact script comparison verifies that no command,
assertion or runtime input changed. These equivalence checks are not counted
as new executions. Pinned reports and trim results are indexed in
`dev/release-materials/provenance/xui-tag-batch-20261115.json`.

Each platform also passes 8 unused-declaration trim steps: file bytes, text and
BSS deltas are zero. The 76-command total counts actual successful acceptance
steps only, excluding diagnostics, staging, inventory and equivalence checks.
Final matched release installation and the wider integration goal remain open.
