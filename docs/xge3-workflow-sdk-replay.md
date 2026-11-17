# Binding replay against the Workflow SDK

The Native b44 Workflow additions changed `include/xui.h`. Two existing text
storage reviews therefore correctly rejected that header: `xui_proxy_t.textDraw`
and `xui_proxy_t.drawTextShapeRangeSpans`. Their reviewed implementation bodies
and pointer-storage rules have not changed.

Admission compares the old and new header ASTs with the actual portable ABI
typedef dependencies. Every old function, type, constant and callable-field
declaration remains. Exactly sixteen approved Workflow ownership functions are
added. Removing just those declarations also leaves every other C token,
including preprocessor directives, identical after comments/whitespace are
ignored. Only the two dependency SHA-256 values are rebased. The strict storage
checker and its rejection rules remain unchanged.

Windows/Linux source gates pass 393/394 checks, including stale dependency,
declaration, array extent and callback contract rejection; strict generation;
an identical second replay; and manifest preservation. All 5570 complete units
in the nine selected declaration files are preserved. Generator and checker
inputs come from an isolated committed source witness. The pending FlowGraph
candidate and other overlapping working bytes are preserved.

Matched hosted/static package candidates are staged against the newly built
Native b44 SDKs and the qualified complete O2 compiler/XRT4641 cohort on both
platforms. Staging changes declarations in the isolated package tree; native
images and archives retain their build hashes. Normal `release/` is untouched.

Chart qualification now accepts an explicit full-replay receipt in addition to
its original six-method incremental receipt. Full-replay validation checks all
source input hashes, all selected outputs, declaration counts, the exact file
population and every unchanged package byte against its native package origin.
Sixteen positive/negative checks per platform cover missing/extra/changed files,
incorrect output/input pins, false declaration counts, incomplete receipts and
wrong package roles. The existing staging functions remain AST-identical.

Runtime qualification uses actual matching packages. TextLayout covers counted
NUL text, owned resources, checked queries, OOM, thread/lifetime boundaries,
JIT and dynamic/static AOT; its lesson is compiled with window execution
disabled. Six Chart queries run through C O0/O2 oracles, JIT and both AOT modes,
including normal false flags, negative errors and independent output values.
There are 80 build/run/header/import commands across both platforms. Warm heap
checks retain their zero-XRT-allocation observations; CPU equivalence is not
claimed. These CPU cases do not certify GPU rendering or arbitrary module cycles.

The immutable receipt is
`dev/release-materials/provenance/xge3-workflow-sdk-replay-20261117.json`.
It indexes 819 source/provenance checks and 80 runtime/build commands, header
admission, source witnesses, staging reports, actual SDK/compiler identities and
unchanged working-tree evidence. Large generated candidates and raw results
remain at their recorded external validation paths.

This is generator/package convergence, not completion of the full integration.
Workflow language/schema callbacks and their pending semantics, claimed-callback
cycles, the owning Engine controller, stable parent-owned views, remaining
configuration/per-API review and final matched IDE/runtime/help/examples
installation remain. No network/security test or normal release installation
ran in this batch.
