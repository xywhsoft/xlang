# x.uia Smoke Tests

The `x.uia` examples require an interactive Windows desktop. Do not add them to
the default non-interactive `xl5_test` baseline until the harness can explicitly
detect and skip desktop-only tests.

Run the non-interactive package load smoke from the repository root:

```bat
dev\extlibs\uia\run_x_uia_static_smoke.bat
```

Run the focused interactive smoke set from the repository root:

```bat
dev\extlibs\uia\run_x_uia_smoke.bat
```

Equivalent manual commands from `demo5`:

```bat
release\xldbg.exe run release\examples\xle_official_native\use_x_uia_version.xl
release\xldbg.exe run release\examples\xle_official_native\use_x_uia_smoke.xl
release\xldbg.exe run release\examples\xle_official_native\use_x_uia_timeout.xl
release\xldbg.exe run release\examples\xle_official_native\use_x_uia_focused.xl
release\xldbg.exe run release\examples\xle_official_native\use_x_uia_tree.xl
release\xldbg.exe run release\examples\xle_official_native\use_x_uia_lifetime.xl
release\xldbg.exe run release\examples\xle_official_native\use_x_uia_shutdown.xl
release\xldbg.exe run release\examples\xle_official_native\use_x_uia_snapshot.xl
release\xldbg.exe run release\examples\xle_official_native\use_x_uia_patterns.xl
release\xldbg.exe run release\examples\xle_official_native\use_x_uia_events.xl
release\xldbg.exe run release\examples\xle_official_native\use_x_uia_event_callback.xl
release\xldbg.exe run release\examples\xle_official_native\use_x_uia_cached.xl
release\xldbg.exe run release\examples\xle_official_native\use_x_uia_locator.xl
release\xldbg.exe run release\examples\xle_official_native\use_x_ui_auto_smoke.xl
release\xldbg.exe run release\examples\xle_official_native\use_x_uia_test_window_value.xl
release\xldbg.exe run release\examples\xle_official_native\use_x_uia_test_window_click.xl
```

`use_x_uia_version.xl` is the non-interactive package load smoke. It checks the
version and error state without touching the desktop tree.

`use_x_uia_test_window_value.xl` is the stable value/text action smoke. It uses
a controlled Win32 test window created by the package, then validates
`from_hwnd`, `set_value`, `value`, and `text`.

`use_x_uia_test_window_click.xl` is the stable click/invoke action smoke. It
clicks a controlled button through UIA and verifies the button command updated
the controlled edit field.

`use_x_uia_shutdown.xl` verifies deterministic global UIA shutdown and lazy
reinitialization.

`use_x_uia_snapshot.xl` verifies `Element.snapshot_json(depth, max_nodes)` on a
controlled Win32 test window and checks that the window, edit, and button are
visible in the generated JSON.

`use_x_uia_patterns.xl` verifies V2.2 pattern wrappers. It toggles a controlled
checkbox, closes a controlled window through WindowPattern, and validates safe
unsupported-pattern behavior for RangeValue and Scroll on a controlled edit.

`use_x_uia_events.xl` verifies V2.3 event queues. It subscribes to focus
changes, triggers focus changes from the controlled Win32 test-window thread,
subscribes to a Value property change, polls compact JSON events, and releases
the queue before another UI change.

`use_x_uia_event_callback.xl` verifies V2.8 caller-thread callback dispatch. It
polls one focus event from the same native event queue, then invokes a supplied
xlang callback with the event JSON.

`use_x_uia_cached.xl` verifies V2.4 cache requests. It reads cached JSON from a
controlled Win32 test window and checks the same core fields used by
`snapshot_json`.

`use_x_uia_locator.xl` verifies V2.5 recorder helpers. It builds a structured
locator JSON for a controlled edit element, checks the stable identity fields,
then verifies `focused_locator()` after focusing the same control.

`use_x_ui_auto_smoke.xl` verifies the first V2.6 high-level automation layer.
It imports `x.ui.auto`, scopes automation-id locators to a controlled window,
then validates `wait_exists`, `type_text`, `read`, `click`, and focused locator
JSON passthrough. It also converts `x.uia.locator_for(element)` output through
`x.ui.auto.locator_from_json(json)` and replays that locator against the same
controlled edit control. The same smoke saves an element screenshot through
`x.ui.auto.screenshot(locator, path)` and reloads it with `opencv`.

`use_x_uia_launch_notepad.xl` is a manual real-app probe. It validates process
launch, PID locator, and process cleanup when the target Windows image exposes a
stable Notepad process/window pair. It is intentionally not part of the default
smoke runner because modern Windows builds can redirect Notepad through a
single-instance or packaged app model.

The batch runners parse `RunResult exitCode=0`; a zero `xldbg.exe` process exit
alone is not treated as success.
