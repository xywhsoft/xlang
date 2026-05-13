# x.uia V2 Spec

Status legend:

- [ ] not started
- [~] in progress
- [x] complete
- [!] blocked or needs decision

V2 builds on the closed V1 package in `demo5/release/libs/x.uia`. The goal is to
turn the low-level UIA wrapper into a more capable automation substrate without
making the default regression suite depend on an interactive desktop.

## V2.1 Snapshot JSON

Status: [x] complete

Add accessibility tree snapshots for recorder/debug tooling.

Native API:

```c
const char* x_uia_element_snapshot_json(void* element, int depth, int max_nodes);
```

xlang API:

```xl
func Element.snapshot_json(int depth, int max_nodes) as string
```

Output shape:

```json
{
  "name": "Window title",
  "automationId": "",
  "className": "Notepad",
  "controlType": 50032,
  "hwnd": "0x0000000000012345",
  "rect": {"left":0,"top":0,"right":800,"bottom":600},
  "children": []
}
```

Constraints:

- `depth <= 0` returns only the current node.
- `max_nodes <= 0` uses a conservative default.
- Traversal must stop when `max_nodes` is reached and mark the result truncated.
- JSON string storage uses a package-global scratch buffer for V2.1, matching the
  current property string pattern.
- V2.1 uses ControlViewWalker so snapshots align with user-facing controls.

Acceptance:

- [x] A controlled test window snapshot includes the top window plus edit and button
  children.
- [x] Invalid element returns `{}` with `last_error()` set.
- [x] Snapshot smoke is interactive and stays out of default unattended tests.

Verification:

- `gcc -std=c11 -Wall -Wextra -Werror -D_CRT_SECURE_NO_WARNINGS -c release\libs\x.uia\src\x_uia_artifact.c -o release\tmp\x_uia_artifact.o`
  passed.
- `release\xldbg.exe sem release\examples\xle_official_native\use_x_uia_snapshot.xl`
  passed.
- `release\xldbg.exe run release\examples\xle_official_native\use_x_uia_snapshot.xl`
  returned `RunResult exitCode=0`.
- `cmd /c dev\extlibs\uia\run_x_uia_smoke.bat` passed.

## V2.2 Additional Patterns

Status: [x] complete

Add common UIA patterns in the same thin wrapper style as V1 actions.

Target patterns:

- Toggle: `toggle()` and `toggle_state()`.
- RangeValue: `range_value()`, `range_min()`, `range_max()`, `set_range_value()`.
- Scroll: `scroll(horizontal, vertical)`.
- Window: `close_window()`, `wait_window_closed(timeout_ms)`.

Acceptance:

- [x] Unsupported patterns return safe defaults and populate `last_error()`.
- [x] At least one controlled or OS-stable smoke covers each landed pattern.

Implementation:

- Toggle is covered by a controlled Win32 checkbox in `use_x_uia_patterns.xl`.
- Window close/wait is covered by the same controlled test window.
- RangeValue and Scroll use safe unsupported-pattern probes against a controlled
  edit control, validating return values and `last_error()` text without relying
  on OS-specific controls.
- `x.uia` exports Toggle and Scroll constants, but the V2.2 smoke uses numeric
  values until xlang module-qualified constant codegen is fixed for expressions
  like `x.uia.TOGGLE_OFF`.

Verification:

- `gcc -std=c11 -Wall -Wextra -Werror -D_CRT_SECURE_NO_WARNINGS -c release\libs\x.uia\src\x_uia_artifact.c -o release\tmp\x_uia_artifact.o`
  passed.
- `release\xldbg.exe sem release\examples\xle_official_native\use_x_uia_patterns.xl`
  passed.
- `release\xldbg.exe run release\examples\xle_official_native\use_x_uia_patterns.xl`
  returned `RunResult exitCode=0`.
- `cmd /c dev\extlibs\uia\run_x_uia_static_smoke.bat` passed.
- `cmd /c dev\extlibs\uia\run_x_uia_smoke.bat` passed.
- `cmd /c build_gcc.bat` passed.
- `cmd /c test_gcc.bat` passed.

## V2.3 Event Queue

Status: [x] complete

Add polling-friendly event support before exposing xlang callbacks.

Native API sketch:

```c
void* x_uia_events_create(void);
int x_uia_events_subscribe_focus_changed(void* queue);
int x_uia_events_subscribe_property_changed(void* queue, void* element, int property_id);
const char* x_uia_events_poll_json(void* queue, int timeout_ms);
int x_uia_events_release(void* queue);
```

Design:

- Native callback handlers push compact event records into a bounded queue.
- xlang polls JSON records; direct xlang callbacks are deferred until callback
  ABI behavior is stable enough for native-to-script invocation.
- Queue release unregisters every active handler.

Acceptance:

- [x] Focus-change event smoke runs on an interactive desktop.
- [x] Releasing a queue unregisters handlers and does not crash after further UI
  changes.

Implementation:

- `Events` is a bounded native queue exposed to xlang through `events()`.
- `poll_json(timeout_ms)` returns one compact JSON record at a time and returns
  `{}` with `last_error()` set on timeout.
- Property changed events use UI Automation `AddPropertyChangedEventHandlerNativeArray`.
- Focus changed events use a WinEvent focus hook as the stable V2.3 transport;
  the UIA focus handler is also registered when available, but the hook is the
  smoke-tested path because embedded fallback execution did not reliably receive
  `AddFocusChangedEventHandler` callbacks for controlled Win32 child focus.
- Queue release unregisters property handlers, focus handlers, and the focus
  hook before invalidating the queue.

Verification:

- `gcc -std=c11 -Wall -Wextra -Werror -D_CRT_SECURE_NO_WARNINGS -c release\libs\x.uia\src\x_uia_artifact.c -o release\tmp\x_uia_artifact.o`
  passed.
- `release\xldbg.exe sem release\examples\xle_official_native\use_x_uia_events.xl`
  passed.
- `release\xldbg.exe run release\examples\xle_official_native\use_x_uia_events.xl`
  returned `RunResult exitCode=0`.
- `cmd /c dev\extlibs\uia\run_x_uia_static_smoke.bat` passed.
- `cmd /c dev\extlibs\uia\run_x_uia_smoke.bat` passed.
- `cmd /c build_gcc.bat` passed.
- `cmd /c test_gcc.bat` passed.

## V2.4 Cache Requests

Status: [x] complete

Use `IUIAutomationCacheRequest` for faster broad tree reads and recorder scans.

Target API:

```xl
func Element.cached_children_json(int depth, int max_nodes) as string
```

Acceptance:

- [x] Cached snapshot returns the same core fields as V2.1.
- [x] Large desktop tree scans do fewer per-node COM calls than uncached property
  reads.

Implementation:

- `cached_children_json(depth, max_nodes)` builds one cache request with
  `TreeScope_Subtree`.
- The cache includes name, automation id, class name, control type, native hwnd,
  and bounding rectangle.
- The native implementation calls `BuildUpdatedCache` once for the root element,
  then recursively reads `GetCachedChildren` and cached property getters.
- The JSON shape intentionally matches V2.1 snapshot core fields so recorder and
  diagnostics code can switch between cached and uncached readers.

Verification:

- `gcc -std=c11 -Wall -Wextra -Werror -D_CRT_SECURE_NO_WARNINGS -c release\libs\x.uia\src\x_uia_artifact.c -o release\tmp\x_uia_artifact.o`
  passed.
- `release\xldbg.exe sem release\examples\xle_official_native\use_x_uia_cached.xl`
  passed.
- `release\xldbg.exe run release\examples\xle_official_native\use_x_uia_cached.xl`
  returned `RunResult exitCode=0`.
- `cmd /c dev\extlibs\uia\run_x_uia_static_smoke.bat` passed.
- `cmd /c dev\extlibs\uia\run_x_uia_smoke.bat` passed.
- `cmd /c build_gcc.bat` passed.
- `cmd /c test_gcc.bat` passed.

## V2.5 Recorder Helpers

Status: [x] complete

Add helpers that produce stable locator suggestions for focused/selected
elements.

xlang API:

```xl
func locator_for(Element element) as string
func focused_locator() as string
```

Locator priority:

1. AutomationId + control type.
2. ClassName + name + control type.
3. ProcessId + class path + index.

Acceptance:

- [x] `focused_locator()` returns a valid expression or structured JSON that can be
  used by a follow-up script.
- [x] Localized text is treated as weaker than automation id or class identity.

Implementation:

- `locator_for(element)` and `focused_locator()` return structured JSON.
- The returned JSON includes `strategy`, `confidence`, `expression`,
  `automationId`, `className`, `name`, `controlType`, `processId`, `hwnd`,
  `classPath`, `index`, and `rect`.
- Strategy selection follows the V2.5 priority order: automation id plus control
  type first, class/name/control type second, process/class path/index last.
- `classPath` is generated with ControlViewWalker and sibling indexes so replay
  tooling has a deterministic fallback when stronger identities are missing.

Verification:

- `gcc -std=c11 -Wall -Wextra -Werror -D_CRT_SECURE_NO_WARNINGS -c release\libs\x.uia\src\x_uia_artifact.c -o release\tmp\x_uia_artifact.o`
  passed.
- `release\xldbg.exe sem release\examples\xle_official_native\use_x_uia_locator.xl`
  passed.
- `release\xldbg.exe run release\examples\xle_official_native\use_x_uia_locator.xl`
  returned `RunResult exitCode=0`.
- `cmd /c dev\extlibs\uia\run_x_uia_static_smoke.bat` passed.
- `cmd /c dev\extlibs\uia\run_x_uia_smoke.bat` passed.
- `cmd /c build_gcc.bat` passed.
- `cmd /c test_gcc.bat` passed.

## V2.6 High-Level x.ui.auto

Status: [x] implemented

Add a higher-level package after the V2.1-V2.5 primitives are stable.

Target capabilities:

- Retry and wait helpers around `x.uia`.
- Locator objects.
- `click(locator)`, `type_text(locator, text)`, `read(locator)`.
- Recorder output that maps directly to these helpers.

Acceptance:

- [x] High-level helpers are implemented in xlang where possible.
- [x] Native code remains in `x.uia` unless a feature requires direct COM access.
- [x] Recorder locator JSON maps directly into `x.ui.auto` locator factories.

Implemented V2.6.1:

- Added pure xlang package `demo5/release/libs/x.ui.auto`.
- Added `Locator` data object plus `by_automation_id`, `by_class_name`,
  `by_name`, `by_control_type`, `by_process_id`, `within`, and `with_timeout`.
- Added high-level helpers `wait_exists(locator)`, `click(locator)`,
  `type_text(locator, text)`, `read(locator)`, and `focused_locator_json()`.
- Added `locator_from_json(json)` to convert V2.5 recorder locator JSON into a
  reusable `Locator`.
- The first smoke uses automation ids emitted by the controlled Win32 test
  window because automation id is the strongest V2.5 locator strategy.
- The smoke replays `x.uia.locator_for(element)` output through
  `locator_from_json(json)`, then verifies type/read against the same control.
- Internal `Element` helper functions use the imported source type name rather
  than `x.uia.Element`; module-qualified type names are not accepted by the
  current parser.
- `TreeScope_Descendants` is currently passed as numeric `4` in `x.ui.auto`
  because module-qualified constants such as `x.uia.TREE_SCOPE_DESCENDANTS`
  still generate invalid C.

Verification:

- `release\xldbg.exe sem release\examples\xle_official_native\use_x_ui_auto_smoke.xl`
  passed.
- `release\xldbg.exe run release\examples\xle_official_native\use_x_ui_auto_smoke.xl`
  returned `RunResult exitCode=0`.
- `cmd /c dev\extlibs\uia\run_x_uia_static_smoke.bat` passed with the
  `x.ui.auto` sem check included.
- `cmd /c dev\extlibs\uia\run_x_uia_smoke.bat` passed with
  `use_x_ui_auto_smoke.xl` included in the interactive matrix.
- `cmd /c build_gcc.bat` passed from `demo5`.
- `cmd /c test_gcc.bat` passed from `demo5`.
- Final verification after `locator_from_json(json)` landed:
  `cmd /c build_gcc.bat`, `cmd /c test_gcc.bat`,
  `cmd /c dev\extlibs\uia\run_x_uia_static_smoke.bat`, and
  `cmd /c dev\extlibs\uia\run_x_uia_smoke.bat` all passed.

## V2.7 Screenshot/Crop Integration

Status: [x] implemented

Keep pixel capture and image encoding in the separate `opencv` package. UIA
continues to own element discovery, rectangles, HWNDs, and automation actions.

Implemented:

- Added Windows capture support to the `opencv` package:
  `capture_rect(x, y, width, height)` and `capture_window(hwnd)`.
- Rebuilt `demo5/release/libs/opencv/bin/opencv_xlang.dll` from the STB/GDI
  backend.
- Added `x.ui.auto.screenshot(locator, path)`.
- `x.ui.auto.screenshot` resolves the locator, reads the UIA bounding
  rectangle, prefers HWND-backed window capture, and falls back to screen-rect
  capture for non-HWND elements.
- Extended `use_x_ui_auto_smoke.xl` to save and reload a screenshot through
  `opencv`, validating that the generated image is readable and non-empty.

Notes:

- On the current desktop session, direct full-screen `BitBlt` capture can fail
  with `BitBlt failed`; HWND-backed capture works for the controlled Win32
  test controls and is the preferred path for UI automation screenshots.

Verification:

- `cmd /c release\libs\opencv\native\build_gcc.bat` passed.
- `release\xldbg.exe sem release\examples\xle_official_native\use_x_ui_auto_smoke.xl`
  passed.
- `release\xldbg.exe run release\examples\xle_official_native\use_x_ui_auto_smoke.xl`
  returned `RunResult exitCode=0`.
- `cmd /c dev\extlibs\uia\run_x_uia_static_smoke.bat` passed.
- `cmd /c dev\extlibs\uia\run_x_uia_smoke.bat` passed.
- `cmd /c build_gcc.bat` passed from `demo5`.
- `cmd /c test_gcc.bat` passed from `demo5`.

## V2.8 Caller-Thread Event Callback Dispatch

Status: [x] implemented

Close the callback follow-up without making native COM event callbacks invoke
xlang script functions directly.

Implemented:

- Added `Events.poll_callback(callback, timeout_ms)` to `x.uia`.
- The native side remains a bounded event queue. `poll_callback` polls one JSON
  event through `x_uia_events_poll_json`, then invokes the supplied xlang
  callback on the caller's xlang thread.
- Timeout returns `0`, matching the polling API's `{}` timeout sentinel.
- Added `use_x_uia_event_callback.xl` to exercise focus-event callback dispatch
  against the controlled Win32 test window.
- Added the callback example to both static semantic smoke and interactive UIA
  smoke runners.

Boundary:

- Direct COM-thread native-to-script callbacks are intentionally not exposed in
  V2.8. Manual native extern parameters of xlang `function<...>` values are
  passed as xlang values, not as raw C function pointers. Binding-generated
  callbacks can bridge this shape, but `x.uia` keeps event delivery on the
  polling queue for deterministic lifetime, threading, and shutdown behavior.

Verification:

- `release\xldbg.exe sem release\examples\xle_official_native\use_x_uia_event_callback.xl`
  passed.
- `release\xldbg.exe run release\examples\xle_official_native\use_x_uia_event_callback.xl`
  returned `RunResult exitCode=0`.
- Final verification with callback dispatch included:
  `cmd /c build_gcc.bat`, `cmd /c test_gcc.bat`,
  `cmd /c dev\extlibs\uia\run_x_uia_static_smoke.bat`, and
  `cmd /c dev\extlibs\uia\run_x_uia_smoke.bat` all passed.
