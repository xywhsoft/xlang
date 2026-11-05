# XGE/XUI example wiring

The maintained declaration examples live in `lib/bindings/xge/examples` and
`lib/bindings/xui/examples`. The runtime delivery tree is not rewritten by
this example update. Stage the source packages together with their exact native
SDK before using these scripts; the current final delivery remains a separate
acceptance step.

## Example categories

- CPU/headless: `headless_frame`, `headless_shape`, `headless_path`,
  `headless_cell`, `context_graph`, `manager_lifetime`. They request no window,
  GPU, browser, network or actual OS dialog operation. The Context/Manager
  examples still require the matching native XGE Proxy implementation.
- Interactive desktop: `gui_shapes`, `gui_surface_draw`. They explicitly need
  a working display and graphics backend. Automated ABI acceptance does not
  execute them.
- Fault/lifetime tests: `test/cases/foreign_xge3_binding_managers.xl`,
  `foreign_xge3_binding_manager_cycle.xl` and the Shape factory, foreign-result
  reservation, callback, runtime-policy and module-retirement gates. These
  require their dedicated native host/fault-injection harnesses. They are not
  standalone application examples; do not publish an OOM switch or test-only
  native symbol as a standard-library function.

Each package's example README gives commands, prerequisites, assertions and
cleanup. The compile-only checker uses normal package imports, generates C and
lowered IR, proves calls against the matched native headers and compiles the
generated translation unit with GCC `-Wall -Wextra -Werror`. Header self-
containment is checked separately in C11, C++11 and C++17. Its report explicitly
sets execution, UI/GPU and shipping-delivery acceptance to false. This work
does not refresh prior runtime/OOM/unload reports or assert GUI correctness.

The six maintained CPU/headless examples now have a separate **actual runtime**
gate: `tools/check_xge3_headless_examples.py` generates C and lowered IR,
builds and executes each example with GCC O0 and O2 on both platforms.
Windows evidence is
`F:/xlang-validation/foreign-completion-20261103/headless-six-win-r1/results.json`;
Linux evidence is
`/home/ubuntu/xlang-validation/headless-six-linux-r1/results.json`.
Each report has 36 passing process rows and unchanged pinned inputs. The
compiler is the honest R38-plus-sized-record frontend overlay over full R37,
with final matching Model Native r5/r6 and matching public packages. This is
generated-C runtime evidence, not a claim of a full compiler rebuild, refreshed
embedded TCC provider, shipped release acceptance or graphics execution.
`document_model.xl` is additionally executed by its dedicated seven-root Model
gate; it is not included in the six-example count above.

## Legacy delivery examples (read-only audit)

The read-only corpus contains 28 XGE and 48 XUI scripts. All 48 XUI scripts
use `XuiApp` and raw Context/Font handles. Of the XGE scripts, 27 use
`XgeApp`; the remaining `test_callback.xl` calls the earlier tutorial helpers
`xgeTutorialRun`/`xgeTutorialEndFrame`. The current native Frame handler must
return `c_int`; a void tutorial callback is not the same declaration contract.
Three representative scripts (`ch02`, `test_callback`, `button`) were also
actually submitted to the current compiler/packages and rejected before C
output. This is evidence of migration work, not a passing legacy test suite.

`release/win_x64/examples/xge` still contains the earlier `XgeApp`, `Shape` and
`Color` convenience API style. For example `ch02.xl` uses
`XgeApp.init/run/beginFrame/endFrame`, `Shape.rectFill` and `Color.rgba`.
The new package instead declares `initNative/run/begin/nativeEnd`,
`shapeRectFill` and `colorRGBA` directly. This is a compile-time API migration;
the old wrapper classes are not silently restored.

`release/win_x64/examples/xui` still uses `XuiApp`, `app.context()/font()` raw
`point` values, `Widget(rawHandle)` and old positional constructors such as
`Button(ctx,text,font)`. It also uses `onClick` instead of the native Owned
registration `setClick`. Replacing only an import cannot make those scripts
safe: the new design needs explicit owned Proxy/Context/Font, Context-owned
factory results, typed callbacks and exact value records.

The new examples demonstrate that safe subset. A complete migration of the
old control gallery additionally needs an application loop with actual input
pumping, viewport/resize/layout policy and fonts, and careful descriptor
configuration per control. Caller-only descriptor pointers, typed arrays
containing resource pointers, retained native text spans and still-in-progress
Document/Renderer APIs must not be filled with fake `point` ownership or
per-method C wrappers. The current inventory records those gaps independently
of these examples. Legacy release files are neither modified nor counted as
passing the new example gate.
