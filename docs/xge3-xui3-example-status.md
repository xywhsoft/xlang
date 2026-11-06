# XGE/XUI example wiring

Maintained examples are owned by `lib/bindings/xge/examples` and
`lib/bindings/xui/examples`. Their standalone runtime copies are installed
under `release/<platform>/examples/xge` and `examples/xui` together with the
matching `xge`, `xui` and `_xge_native` packages. Each README gives commands,
prerequisites, assertions and cleanup.

## November 6 full XUI gallery

All 48 retired XUI tutorial entrypoint names now have new declarative examples.
Together with the retained eight programs, each published XUI examples directory
contains 56 public programs and one shared helper. The aggregate gallery selects
47 scenes and explicitly closes floating resources when switching pages.

The final installed Windows cohort passed all 48 builds and bounded real windows,
including all 47 aggregate scenes and a synthetic button click. The Linux cohort
passed all 48 AOT builds and the retained CPU programs; no display is available.
The final internal-client alias refinement was tested separately and installed
byte-for-byte. The installed launchers, source copies and optional static gallery
have their own acceptance records. See `docs/reports/xui-gallery-20261106.md` and
`dev/release-materials/provenance/xui-gallery-20261106/receipt.json`.

## November 5 production CLI acceptance (historical)

Both actual production CLIs, using their statically linked TinyCC/VFS and the
final independently named host SDK, passed these nine programs:

- `headless_frame`, `headless_shape`, `headless_path`;
- `headless_cell`, `context_graph`, `manager_lifetime`;
- `document_model`, `document_ui_headless`, `data_object`.

These programs request no window, GPU, browser, network, real clipboard or OS
dialog operation. Context/Manager/Document UI cases still use the matching
Native Proxy and ownership implementation. They are actual program execution,
not declaration-only checks or a SourceDriver substitute.

The two interactive examples, `gui_shapes` and `gui_surface_draw`, passed
`check`, C generation and ABI checks; they were never executed by this CPU
acceptance. They require a working display and graphics backend.

The combined receipt is
`dev/release-materials/provenance/production-ui-examples-20261105.json`.
It pins 34 process rows across both platforms, including 18 actual headless
runs. Inputs, source copies, environment, production CLI/DLL hashes and exact
logs are recorded. These pre-installation results do not themselves establish
that a runtime directory was published; the delivery index records that step.

## Historical and specialized evidence

The earlier six-example generated-C gate remains historical evidence:
`headless-six-win-r1/results.json` under the validation root and
`/home/ubuntu/xlang-validation/headless-six-linux-r1/results.json`, with 36
process rows per platform. It used the explicitly recorded R38/sized-record
frontend over R37 and its matching Native Model SDK. Its report bytes are not
rewritten as a final production compiler run.

Fault/ownership cases under `test/cases`, including manager cycles, factory
reservation, callback epochs, runtime policies, OOM and module retirement,
need their dedicated harnesses. They are not standalone application examples
and do not add test-only native symbols to the standard library.

## Earlier wrapper tutorials

The 28 XGE and 48 XUI wrapper tutorials have an exact-byte source archive at
`dev/release-materials/sources/legacy-ui-examples-20261105`, with a SHA-256
index. They use `XgeApp`/`XuiApp`, raw handles or old tutorial callbacks and are
not counted as runnable examples for these declarative packages. No wrapper
compatibility classes are restored.

Publication retires only the enumerated legacy files and two old BAT runners;
it never recursively deletes the examples directories or their hidden user
IDE state. The maintained programs show the current owned Proxy/Context and
typed callback/record contracts. The November 6 gallery successor restores all
48 historical XUI entrypoint names with new declarative implementations; it
adds a shared owned window loop and an aggregate selector covering 47 scenes.
The original eight resource/lifetime examples remain separately documented.
See `lib/bindings/xui/examples/README.md` and the current delivery index's
`xuiGallery` receipt for the actual compiler, native cohort and acceptance scope.
The old wrapper archive and November 5 evidence remain historical bytes.
Native declaration presence, header ABI qualification, CPU execution and actual
graphics behavior remain separate evidence categories.
