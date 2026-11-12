# M7b5l: IDE startup and XUI diagnostic ownership

The toolchain comprehensive fixture exposed a real startup failure: a handled XRT diagnostic on the UI thread entered TextEdit construction and was treated as a new callback failure. The same failure occurred with the preceding committed GUI cohort. A native GPU probe reproduced it at both 100% and 125% scale, with the exact workspace description control failing at creation.

The matched XGE compatibility update isolates all widget constructors, retires successfully initialized stages when a fresh diagnostic refuses creation, watches output lifetime through deferred cleanup and restores 36 previously dropped control receipts. Public headers and ABI remain unchanged. Source changes are also maintained in the XGE producer repository; the SDK retains its pinned producer base and explicit patch metadata. In particular its MessageList body is the pinned body plus the receipt delta, rather than unrelated newer producer changes.

Restoring diagnostics exposed a second integration defect: workspace search retained NULL optional selection/text values. Decoder admission now skips absent values and refuses a failed non-NULL retain. Native coverage checks an inherited diagnostic by pointer; the actual workspace-search window checks that a successful first search leaves no new diagnostic.

Font-change refusal also has an explicit model-error boundary: settings/file failures are reported after rollback and their native diagnostic is consumed, restoring the entry diagnostic. Unexpected fresh diagnostics on an otherwise successful change still propagate. The locked-settings rollback fixture checks pointer identity and verifies there was no earlier pending error.

The comprehensive toolchain fixture now opens the real toolbar More menu and clicks fixed-footer actions using their actual containing window. The large-font test follows the same fixed-footer layout; cache, failure, cancellation, C/xlang mixed build/run and document-preservation assertions remain active.

The stage uses a complete rebuilt matched SDK and a fresh 124-unit GUI cohort paired with the preceding accepted native language service. All runtime execution stays in the root release tree using exact private aliases to protect the last working delivery and compare DLL versions. After qualification the complete developer SDK is adopted together with its dependency-owned provenance; the release runtime and active delivery index remain unchanged.

Detailed build, positive/negative regression and unchanged-runtime evidence is in the stage material index. Earlier native CodeEdit find-table and lifetime-observer assertions reproduce with their preceding implementations and remain unresolved; they are not counted as passed. DAP, complete source-transaction host activation, remaining rename/applyEdit policies, full performance and final delivery qualification remain outside this stage and open in the long-running goal.
