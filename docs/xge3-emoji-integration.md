# Emoji integration prerequisites

The complete safe Emoji language interface is still open. The current source
package exposes `emojiPackClearDefault()`; TextShape can also use Emoji through
the native text engine. This is not a substitute for a complete owned pack API.

Native XGE `2574c579` fixes four prerequisites: atomic pack references, UTF-8
edit transactions that cannot leave orphan payloads, retryable SVG-cache OOM,
and cold built-in loading/error callbacks outside the global publication spin.
Actual old-code assertions, fully instrumented XRT/aggregate TSan races and a
bounded callback-deadlock control precede the fixes. Upstream XRT and native
public headers are unchanged.

`xge-emoji-native-20261112.json` pins 109 passing command checks: 52 Windows and
57 Linux. Complete O2 host/static products on both platforms, plus Linux EGL
host/static, include 876 translation-unit occurrences (875 fresh and one
dependency-checked HarfBuzz reuse). Actual C11 O0/O2 and C++11/C++17 clients
exercise the public host/static API; aggregate tests separately check private
invariants, OOM, concurrency, reentry and arithmetic. The matching SVG GPU
regressions use actual OpenGL/software EGL. They do not qualify full Emoji
rendering or replace language JIT/AOT/shared-module acceptance.

Linux TSan instruments the aggregate and canonical XRT, including allocator,
locks and atomics; other native archive objects remain uninstrumented. UBSan
instruments the aggregate only. All frozen, gate and log inputs were reverified.
The native Linux pack remains 112 requested bytes by typed DWARF. Warm native
matching/acquisition loops perform no XRT heap operations, but still execute
UTF-8/trie traversal, a short acquisition slot lock and atomic reference work.
Cold competing builders can duplicate parsing/CRC/context allocation. No
throughput, RSS, all-allocator or zero-total-cost claim is made.

The nine native APIs cover creation, built-in/default acquisition, source
insertion, matching, default publication/clear and reference ownership. The
language design must map acquisition/destruction to owned classes, preserve
exact byte-prefix lengths and native diagnostics, and provide useful match
results. It should directly declare native calls rather than add per-method
forwarding bodies. Reference-management support need not become manual user
methods.

Before that interface is completed:

- Decide frozen publication versus matching/mutation serialization. A separate
  TSan control reproduces the existing mutable default race even with valid
  strong references; the current fix does not claim that sharing is safe.
- Review cold GetDefault errors and typed not-found results.
- Finish the pending engine pre-init/global cleanup, queued GPU references and
  module/code residency rules. Current cold Unit skips the built-in singleton;
  the public native tests use manual Init/Unit without starting a backend.
- Finish the generic package-initializer contract required by actual xge
  DLL/DSO imports, then add language ownership/callback/OOM tests and readable
  examples with matched SDK/compiler/runtime products.

No new Emoji language declarations, native image installation, release/IDE
replacement or publication are performed by this prerequisite batch.
