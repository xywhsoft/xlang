# Final C interop analysis cohort

The standalone Windows language server uses the independently qualified
`final-c-interop-20261105` frontend from the final Core r2 snapshot. Existing
C-interop and async-result pin dictionaries remain unchanged. The new JSON
inventory pins 455 source/SDK inputs, including newly named contract headers
and all historical role dependencies. Preparation copies exact bytes into a
private frontend and explicitly projects six SDK headers from
`release/win_x64/include` to its analysis-only `release/include` layout.
It neither changes the original checkout nor treats a new hash as an old ABI.

The AST node layout is unchanged; the additional FOREIGN_FIELD flag is covered
by the existing complete scalar flag copies and keys. The regenerated Lemon
parser has AST union member yy454, contract member yy424 and destructor symbol
139. The new discarded-stack destructor retains the exact original explicit
contract destructor and grammar actions. The final IR cancellation chain has
36 explicitly enumerated phases; cancellation uses the original complete
module-free error branch, including newly owned sparse tables.

The raw IR codec normalizes all 17 scalar fields of the 88-byte foreign call
fact record, including its internal two-byte padding. There are no pointers
in this record. rollbackSymbol still uses the existing lexical/entity identity
remapping; the other fields are scalar enums, bitmaps and parameter/physical
indexes. Six additional type-prefix facts are included: lifetime, owner TypeId,
and policy/scope/watch/domain strings. Strings enter the key as nullness and
copied bytes, never allocation addresses. Type strings remain owned by the
ordinary native type pool.

The final profile explicitly declines old raw/persistent fragment reuse for
C attributes/records/types. All 23 current sparse foreign table counts are
also enumerated before publishing such cached owners. Those tables are not
serialized or flattened into pointers. The existing unreviewed-derived-pair
path runs the complete original native pure passes. Full parser, semantics,
IR construction and independent verification remain in effect. This is a
correctness fallback, not a performance optimization or complete restoration
of foreign IR caching. Ordinary module parsing and semantic reuse still work.

Relocated native translation units retain their original quote-include
directory through per-unit `-iquote`; this is required for newly split local
compiler .inc files. The language server owns its compiler XRT in its separate
process and does not import XGE or xlc.dll. GUI runtime ownership is unchanged.

Focused checks cover code-owner injection, typed Array projection, output
records/sizes, Pack/POD and NativeSequence, negative diagnostics, completion,
and actual stdio requests. The actual generated IR decoder fixture covers
17 field mutations, padding, six type keys, all 23 table gates and rollback
identity rebinding across two real semantic snapshots. Historical cohorts and
all initial failed probe/fixture reports are preserved.

In the bounded final XGE/XUI package observation, 27 source/manifest inputs
were captured. Initial stdio analysis parsed 24 modules; the following two
comment edits reused all 24 module parses and reused semantic functions while
declining raw IR fragments. Recorded stdio times were approximately 1094,
828 and 844 ms on this machine. These are whole package analysis observations,
not UI latency promises; complete native IR/index work remains. There is no
full repository discovery scan, program execution, window, GPU, browser,
network or actual clipboard operation in these checks.
