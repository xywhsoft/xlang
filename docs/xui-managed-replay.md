# Reproducible XUI managed declarations

The managed generator now preserves all 3,130 existing declaration units in
`lib/bindings/xui/src/managed.xl`. A unit includes its owning class, attributes,
types, nullable markers, parameter names, defaults and overload multiplicity.
Class and method order, comments and line endings are compared separately.
This is preservation of the current API, not proof that every upstream feature
has been integrated or that every existing method has runtime acceptance.

The previous full replay dropped or weakened contracts added after its initial
implementation. These included 61 borrowed-C-string return declarations, sized
editor text reads, nullable RadioGroup selection, callback-epoch Context/Widget
types, error-record text snapshots, localization, extended input/IME, search,
typed icon models, menu submenus, geometry outputs and default Context options.

`tools/xui_managed_contracts.py` records 78 reviewed native/runtime entries
with 80 logical declarations, three callback typedefs and four callback view
classes. It validates their physical signatures, 16 related record declarations
and 19 resolved record/callback shapes. Resolved-shape checks also cover forward
typedefs: matching `typedef struct name name` alone cannot certify the fields.
The existing runtime clipboard declaration and inline drag-event helper have
separate witnesses. The generator does not read `managed.xl` to authorize or
reconstruct these overrides; that file is only the comparison baseline.

Ordinary borrowed `const char*` results use the already accepted `cstring`
contract. The legacy `xuiEditGetText` / `xuiCodeEditGetText` producers have explicit
replacement mappings to their sized `*GetTextView` entries, preserving NUL and
checked read failure. The browser event callback remains inventoried as pending
native lifecycle/backend acceptance; inference does not publish it as a new
method merely because its C signature can be translated.

`stage_xge3_replay_contracts.py` now accepts nine source files, including the
complete managed file. Its comparison preserves whole declarations and class
membership, rejects removed/changed/extra units and retains duplicate counts.
Unknown bodies or syntax fail closed. The existing six plain-value additions
remain the only approved additions for their respective record files.

The strict source gate covers missing/changed function signatures, missing/
changed type declarations, resolved layout changes with identical forward
typedefs, the inline/runtime witnesses, class membership, nullability, effects,
defaults, duplicates and invalid syntax. It also performs two complete native
inventories/generations and requires deterministic output and unchanged inputs.
Windows and Linux pass 374 and 375 checks respectively; Linux additionally
exercises the filesystem symlink escape case. All nine selected files preserve
their existing units. The managed file still differs in textual order/spacing;
that difference is not hidden or reported as byte equality.

The regenerated files are staged into separate hosted/static candidates using
the unchanged, previously qualified Flow-canvas SDKs and full O2 compiler
cohorts. All nine runtime suites have passed on Windows and Linux.
They exercise text reads, editor search, extended
input, error reporting, accessibility observers, icon formatting, menu items,
RadioGroup options and localization. It includes JIT, independent dynamic/static
AOT, C/C++ headers, negative/expiry/OOM checks and bounded window lessons where
supported by those suites. Each OS passed 222 runtime steps and produced 15
bounded window readbacks. Including the 749 source checks, this batch has 1,193
steps. Independent verification checked 3,495 Windows and 3,460 Linux input,
log, program and package files, including all 30 readbacks. Exact paths and
per-suite limits are indexed by the successor
`dev/release-materials/provenance/xui-managed-replay-runtime-20261116.json`.
The original source-only receipt remains unchanged. These are staged candidate
packages, not an installed release or acceptance of all 3,130 methods.

This batch changes tooling and declaration reproduction. It adds no native
business wrapper and changes no compiler/runtime/native implementation. Existing
string copies, array projections, lifetime checks and callback bookkeeping keep
their costs; source equivalence is not a new CPU zero-overhead measurement.
Final SDK/compiler/runtime/IDE/help installation, latest XRT synchronization,
safe standalone Graph/Workflow ownership and other pending full-coverage work
remain separate requirements. External reports and candidates stay under
`F:/xlang-validation/xge-full-20261110`; runtime bytes are not added to Git.
