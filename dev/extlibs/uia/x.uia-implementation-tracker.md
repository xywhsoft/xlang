# x.uia Implementation Tracker

Status legend:

- [ ] not started
- [~] in progress
- [x] complete
- [!] blocked or needs decision

Implementation target: `demo5/release/libs/x.uia`
Example target: `demo5/release/examples/xle_official_native`
Design target: `dev/extlibs/uia`

V1 status: closed. V2 implementation has started in `x.uia-v2-spec.md`.

## M0: Toolchain Probe

- [x] Create a temporary UIA compile probe using Windows SDK headers.
- [x] Verify whether `#include <uiautomation.h>` compiles in the current demo5
  toolchain.
- [x] Verify whether `CoCreateInstance(CLSID_CUIAutomation, ...)` links with
  available libraries.
- [x] Decide between direct Windows SDK headers and local `x_uia_min.h`.
- [x] Record exact compiler/linker errors if the SDK route fails.

Result:

- TCC package path: `uiautomation.h` is not present.
- GCC/w64devkit path: header is present, but GUID symbols require additional
  handling/imports.
- V1 package uses local `src/x_uia_min.h`.

Acceptance:

- One small native probe proves the selected UIA include/link strategy.
- The selected strategy is documented in `x.uia-v1-spec.md` if it changes.

## M1: Package Skeleton

- [x] Create `demo5/release/libs/x.uia/package.json`.
- [x] Create `demo5/release/libs/x.uia/src/uia.xl`.
- [x] Create `demo5/release/libs/x.uia/src/x_uia_artifact.c`.
- [x] Add `version()` and `last_error()`.
- [x] Add a compile-only example that imports `x.uia`.
- [x] Validate package discovery from `demo5/release`.

Acceptance:

- `import x.uia` compiles.
- `x.uia.version()` returns a non-empty string.
- Failure in a native probe path updates `x.uia.last_error()`.

## M2: COM Initialization and Desktop Root

- [x] Implement lazy `CoInitializeEx`.
- [x] Implement package shutdown/refcount discipline.
- [x] Implement `x_uia_desktop`.
- [x] Implement `x_uia_focused`.
- [x] Implement native `XUiaElement` allocation and release.
- [x] Implement `Element.valid()` and destructor path.

Lifetime note:

- V1 release uses a tombstone wrapper strategy: release invalidates the native
  COM pointer and magic field but keeps the small wrapper allocation alive, so a
  stale xlang `point` can return invalid instead of reading freed memory.
- `x.uia.shutdown()` releases the package-global `IUIAutomation` object and
  uninitializes COM only when `x.uia` initialized it. Later UIA calls lazily
  initialize the global object again.

Acceptance:

- `use_x_uia_smoke.xl` can obtain the desktop root.
- Repeated desktop root create/destroy loop does not crash.
- Invalid/null element calls return safe defaults.
- Explicit shutdown and lazy reinitialization are covered by
  `use_x_uia_shutdown.xl`.

## M3: Properties and Encoding

- [x] Implement UTF-8 to UTF-16 conversion.
- [x] Implement UTF-16/BSTR to UTF-8 conversion.
- [x] Implement `name()`.
- [x] Implement `automation_id()`.
- [x] Implement `class_name()`.
- [x] Implement `control_type()`.
- [x] Implement `hwnd()`.
- [x] Implement `rect_json()`.

Acceptance:

- Desktop/focused element properties are readable.
- Empty/null BSTR values return empty strings without crash.
- A non-ASCII window/control name returns valid UTF-8 when available.

## M4: Conditions and Find

- [x] Implement `Condition` wrapper in `uia.xl`.
- [x] Implement `by_name`.
- [x] Implement `by_automation_id`.
- [x] Implement `by_class_name`.
- [x] Implement `by_control_type`.
- [x] Implement `all_of`.
- [x] Implement `any_of`.
- [x] Implement `not_condition`.
- [x] Implement `Element.find_first(condition, timeout_ms)`.
- [x] Implement deterministic timeout error messages.

Acceptance:

- Missing element search times out and sets `last_error()`.
- A real known window/control can be found by at least one property.
- Combined condition can find a control more narrowly than name alone.

## M5: Tree Traversal

- [x] Implement `Element.parent()`.
- [x] Implement `Element.children()`.
- [x] Implement `Element.find_all()`.
- [x] Decide final `array<var>` wrapping convention for returned elements.
- [x] Add tree smoke script.

Result:

- `find_all()` and `children()` return `array<var>` containing raw `point`
  handles.
- Callers convert with a helper such as `as_point(var) as point`, then wrap with
  `x.uia.element(point)`.

Acceptance:

- Desktop children can be enumerated without crash.
- Array item count and element wrapper conversion are tested.
- Releasing returned child elements does not double-release parent/root.

## M6: Actions

- [x] Implement `set_focus()`.
- [x] Implement `invoke()`.
- [x] Implement `click()` using InvokePattern first.
- [x] Implement fallback click through bounding rectangle center.
- [x] Implement `value()`.
- [x] Implement `set_value()`.
- [x] Implement `text()`.

Note:

- `text()` now tries TextPattern document range first, then ValuePattern, then
  NameProperty.
- `click()` is covered by `use_x_uia_test_window_click.xl`, which clicks a
  controlled Win32 button and verifies the edit field changed.

Acceptance:

- Calculator or a local test window can be clicked through UIA.
- Notepad or a controlled edit field can be set/read.
- Unsupported actions return `false` and set `last_error()`.

## M7: Examples and Smoke Tests

- [x] Add `use_x_uia_smoke.xl`.
- [x] Add `use_x_uia_focused.xl`.
- [x] Add `use_x_uia_timeout.xl`.
- [x] Add `use_x_uia_tree.xl`.
- [x] Add `use_x_uia_lifetime.xl`.
- [x] Add `use_x_uia_shutdown.xl`.
- [x] Add `use_x_uia_snapshot.xl`.
- [x] Add `use_x_uia_patterns.xl`.
- [x] Add `use_x_uia_events.xl`.
- [x] Add `use_x_uia_event_callback.xl`.
- [x] Add `use_x_uia_cached.xl`.
- [x] Add `use_x_uia_locator.xl`.
- [x] Add `use_x_ui_auto_smoke.xl`.
- [x] Add `use_x_uia_version.xl`.
- [x] Add `use_x_uia_launch_notepad.xl`.
- [x] Add `use_x_uia_test_window_value.xl`.
- [x] Add `use_x_uia_test_window_click.xl`.
- [x] Keep Calculator as a future manual real-app probe because locators are not
  stable across target Windows images.
- [x] Separate compile-only smokes from interactive desktop smokes.
- [x] Document how to run each smoke.

Note:

- `use_x_uia_focused.xl` is intentionally tolerant: some desktop states have no
  stable focused element when run from batch automation.

Current verification:

- `release\xldbg.exe run release\examples\xle_official_native\use_x_uia_smoke.xl`
  returned `RunResult exitCode=0`.
- `release\xldbg.exe run release\examples\xle_official_native\use_x_uia_timeout.xl`
  returned `RunResult exitCode=0`.
- `release\xldbg.exe run release\examples\xle_official_native\use_x_uia_focused.xl`
  returned `RunResult exitCode=0`.
- `release\xldbg.exe run release\examples\xle_official_native\use_x_uia_tree.xl`
  returned `RunResult exitCode=0`.
- `release\xldbg.exe run release\examples\xle_official_native\use_x_uia_lifetime.xl`
  returned `RunResult exitCode=0`.
- `release\xldbg.exe run release\examples\xle_official_native\use_x_uia_shutdown.xl`
  returned `RunResult exitCode=0`.
- `release\xldbg.exe run release\examples\xle_official_native\use_x_uia_snapshot.xl`
  returned `RunResult exitCode=0`.
- `release\xldbg.exe run release\examples\xle_official_native\use_x_uia_patterns.xl`
  returned `RunResult exitCode=0`.
- `release\xldbg.exe run release\examples\xle_official_native\use_x_uia_events.xl`
  returned `RunResult exitCode=0`.
- `release\xldbg.exe run release\examples\xle_official_native\use_x_uia_event_callback.xl`
  returned `RunResult exitCode=0`.
- `release\xldbg.exe run release\examples\xle_official_native\use_x_uia_cached.xl`
  returned `RunResult exitCode=0`.
- `release\xldbg.exe run release\examples\xle_official_native\use_x_uia_locator.xl`
  returned `RunResult exitCode=0`.
- `release\xldbg.exe run release\examples\xle_official_native\use_x_ui_auto_smoke.xl`
  returned `RunResult exitCode=0`.
- `release\xldbg.exe run release\examples\xle_official_native\use_x_uia_launch_notepad.xl`
  returned `RunResult exitCode=0`.
- `release\xldbg.exe run release\examples\xle_official_native\use_x_uia_test_window_value.xl`
  returned `RunResult exitCode=0`.
- `release\xldbg.exe run release\examples\xle_official_native\use_x_uia_test_window_click.xl`
  returned `RunResult exitCode=0`.
- `release\xldbg.exe run release\examples\xle_official_native\use_x_uia_version.xl`
  returned `RunResult exitCode=0`.
- `cmd /c dev\extlibs\uia\run_x_uia_static_smoke.bat` passed the
  non-interactive package load smoke.
- `cmd /c dev\extlibs\uia\run_x_uia_smoke.bat` passed the stable listed UIA
  examples, including V2.5 `use_x_uia_locator.xl`.

Acceptance:

- Compile-only examples pass in non-interactive environments.
- Interactive examples pass on a normal Windows desktop.
- Each failure path prints or exposes useful `last_error()` text.

## M8: Regression Integration

- [x] Add package examples to the appropriate demo5 regression list if one
  exists for official native XLE examples.
- [x] Ensure non-interactive CI does not hang waiting for desktop UI.
- [x] Add skip rules or separate command for interactive UIA tests.
- [x] Run `build_gcc.bat`.
- [x] Run targeted XLE official native examples.

Current status:

- `cmd /c build_gcc.bat` passed after stopping stale `xl.exe` browser CDP smoke
  processes that were holding `release\xl.exe` open.
- `cmd /c test_gcc.bat` passed.
- Targeted `x.uia` examples were rerun afterward with the rebuilt `xldbg.exe`
  and passed.
- Added `dev/extlibs/uia/run_x_uia_smoke.bat` as the separate interactive smoke
  entrypoint. It now parses `RunResult exitCode=0`, not just the `xldbg.exe`
  process exit code.
- Added `dev/extlibs/uia/run_x_uia_static_smoke.bat` as the non-interactive
  package load smoke entrypoint.
- Added `sem.xle_official_native_uia_version` and
  `runtime.xle_official_native_uia_version` to the default `xl5_test` registry.
  The test harness artifact collector was updated to match the CLI path by
  adding package `cSources` as `c-source` artifacts and accepting
  `c-source-bridge`.
- `release\xl5_test.exe sem.xle_official_native_uia_version` passed with
  `summary: total=1 pass=1 fail=0`.
- `release\xl5_test.exe runtime.xle_official_native_uia_version` passed with
  `summary: total=1 pass=1 fail=0`.
- `cmd /c dev\extlibs\uia\run_x_uia_static_smoke.bat` passed after the full
  rebuild.
- `cmd /c dev\extlibs\uia\run_x_uia_smoke.bat` passed after the full rebuild.
- V2.5 was rerun through the same static and interactive smoke runners after
  `build_gcc.bat` and `test_gcc.bat`; both runners passed with the rebuilt
  `xldbg.exe`.
- `use_x_uia_launch_notepad.xl` remains a manual real-app probe because modern
  Notepad can redirect through packaged/single-instance behavior and fail PID
  window discovery on otherwise valid UIA setups.

Acceptance:

- Normal build remains green.
- UIA interactive tests are available but do not destabilize unattended builds.
- No unrelated generated binary or release artifact churn is introduced unless
  the build workflow intentionally regenerates it.

## M9: Documentation Closeout

- [x] Add package usage notes near release examples or package docs.
- [x] Document UAC/integrity limitations.
- [x] Document locked-session/headless limitations.
- [x] Document locator stability guidance.
- [x] Document timeout defaults.
- [x] Update this tracker statuses.

Acceptance:

- A new user can discover `x.uia`, run a smoke, and understand common failure
  modes without reading native source.
- The remaining V2 gaps are explicit.

## V2 Backlog

Detailed design is tracked in `x.uia-v2-spec.md`.

- [x] V2.1 accessibility tree snapshot as JSON.
- [x] V2.2 additional UIA patterns: Toggle, RangeValue, Scroll, Window.
- [x] V2.3 UIA event registration with a polling queue.
- [x] V2.4 cache requests for faster bulk property reads.
- [x] V2.5 recorder helper that prints stable locators for selected/focused
  element.
- [x] V2.6 high-level `x.ui.auto` package on top of `x.uia`.
  - [x] Pure xlang package scaffold.
  - [x] Locator object and locator factories.
  - [x] `wait_exists`, `click`, `type_text`, `read`, and focused locator JSON
    passthrough.
  - [x] Controlled smoke for wait/type/read/click.
  - [x] Full `build_gcc.bat`, `test_gcc.bat`, static smoke, and interactive
    smoke pass with `x.ui.auto` included.
  - [x] Map V2.5 locator JSON directly into `x.ui.auto` locator factories.
- [x] Screenshot/crop integration through a separate graphics package.
  - [x] `opencv.capture_rect` and `opencv.capture_window` added to the
    separate image package.
  - [x] `x.ui.auto.screenshot(locator, path)` added on top of locator
    resolution plus UIA rectangles/HWNDs.
  - [x] Controlled smoke saves and reloads a screenshot through `opencv`.
  - [x] `opencv` native shim rebuild, `build_gcc.bat`, `test_gcc.bat`, static
    smoke, and interactive smoke passed.
- [x] Callback integration through safe caller-thread event dispatch.
  - [x] `Events.poll_callback(callback, timeout_ms)` added.
  - [x] Controlled focus-event callback smoke added.
  - [x] `build_gcc.bat`, `test_gcc.bat`, static smoke, and interactive smoke
    passed with callback dispatch included.
  - [x] Direct COM-thread native-to-script callback remains intentionally
    unsupported in V2 because manual extern function parameters are xlang
    values, not raw C function pointers.
