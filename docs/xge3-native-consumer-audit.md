# Native consumers in the full API inventory

The current committed binding population declares 3903 of the 5294 native
function names at Native b44. The other 1391 names are a review population,
not a missing-feature count or a completion percentage. This population
excludes the uncommitted Graph model candidate.

`tools/check_xge3_native_consumers.py` reads the complete surface record and
actual matched package. It checks public header hashes, replays the package
provenance checks, and uses the existing inventory parser on copies of the
package-selected declarations. An older valid package with the same native
headers cannot inherit the current inventory's declaration count.

The tool compiles all seven manifest-selected bridge translation units against
the complete qualified compiler SDK and real Native headers. Undefined object
symbols provide concrete C-consumer evidence. COFF import indirection is
normalized; names are not guessed from lifecycle suffixes. Logical source-group
selectors must have physical definitions, except explicitly reviewed header
aliases. The `xl6Xge3InitPacked` alias is proved through the actual preprocessor
definition and a C relocation to `xl6Xge3InitWithCode`.

Windows and Linux produce the same classification:

| Family | Non-direct names | Compiled bridge consumer | Existing explicit replacement | Neither evidence kind |
| --- | ---: | ---: | ---: | ---: |
| XGE, including 3D | 552 | 31 | 5 | 516 |
| XUI | 755 | 71 | 0 | 684 |
| Document | 57 | 20 | 0 | 37 |
| Document UI | 27 | 9 | 0 | 18 |
| Total | 1391 | 131 | 5 | 1255 |

The five replacement records are taken from the existing reviewed mapping
registry. Both the native target and exact language entry are checked against
the actual package declarations. This table does not establish equivalent
semantics for every replacement or safe lifetime behavior for every consumer.
Every non-direct row retains `contractReview=pending`; previous independently
qualified feature receipts remain valid within their own scope.

The complete reports retain every function's identity, feature conditions,
consumer translation units and replacement, including rows with no consumer
evidence. Missing object imports are not proof of missing functionality:
field projections, indirect tables, alternate factories and legacy interfaces
still require explicit review. The 3903 directly declared names also retain
their separate ABI, ownership, error, thread and runtime qualification duties.

Two platforms pass 48 compilation/symbol/preprocessor commands. Two additional
checks use the genuine previous package and its valid receipt; both reject
exactly the three newly added ray declarations before compiling native objects.
No production code, native library, compiler implementation or release tree is
changed by this inventory. No GPU or network test runs in this gate.

The immutable evidence index is
`dev/release-materials/provenance/xge3-native-consumer-audit-20261117.json`.
The full integration goal, all pending contracts and final matched delivery
remain open. This audit supplies concrete review evidence rather than treating
the absence of a language declaration as a decision to omit an API.
