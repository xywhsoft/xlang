# Responsive IDE dialog acceptance

This is a local GUI delivery. Runtime execution uses `release/win_x64`; source
and acceptance materials stay outside that directory. The language server,
compiler and matched XGE runtime are retained by hash.

The source snapshot covers every resizable IDE business dialog, the command
source viewer, About minimum sizing, native file chooser bounds/separator, and
message/overwrite/updater/failure prompt presentation. Native XUI flow layout
owns form measurement. Content and footer are separate siblings. The compact
action strip shares width across action groups, wraps without hiding commands,
and remains inside a 700 × 460 host even at 40 DIP. Long diagnostic messages
scroll separately from native confirmation buttons.

The receipt links the exact source overlay, matched SDK, full 106-unit `O2`
build inputs, and focused native acceptance. Tests load the real runtime assets
through temporary owned executables; these executables are removed afterward.
User preferences and sessions use dedicated test paths. No external publication
or network acceptance is part of this delivery.

The acceptance is scoped to dialog/layout and affected editing behavior. It is
not a claim that the entire compiler or IDE suite passes. Three broader legacy
fixtures have matching failures with both the original GUI objects and the new
objects:

* `global_recovery_view_test`: startup preference/session-index expectation,
  before the recovery dialog is opened.
* `resource_view_test`: invalid resource document/message expectation. Focused
  resource creation, layout and resource reference lifetime coverage passes.
* `toolchain_test`: workspace creation returns `XUI_ERROR_CALLBACK_FAILED`,
  before any toolchain dialog is created.

Their original/new logs are retained separately. Initial test-driver failures
from missing helper programs and old footer coordinate assumptions were
corrected before the final acceptance batch. See `acceptance.json` for the
actual accepted fixtures and `publication-receipt.json` for the installed GUI
identity and smoke result.
