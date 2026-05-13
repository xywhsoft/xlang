# x.uia

`x.uia` is the Windows UI Automation package for xlang.

The package is source-mixed: `src/uia.xl` exposes the xlang API, while
`src/x_uia_artifact.c` calls UI Automation COM interfaces directly. The local
`src/x_uia_min.h` header keeps the package buildable in the release toolchain
without depending on a full Windows SDK include path.

## Quick Smoke

From the repository root:

```bat
dev\extlibs\uia\run_x_uia_static_smoke.bat
dev\extlibs\uia\run_x_uia_smoke.bat
```

`run_x_uia_static_smoke.bat` only validates package loading and basic API shape.
It does not require an interactive desktop.

`run_x_uia_smoke.bat` requires an unlocked interactive Windows desktop because
it exercises real UI Automation tree, focus, window, value, and click behavior.
It also covers V2 pattern wrappers through a controlled test window.

## Minimal Use

```xl
import x.uia

func main() as int
	auto desktop = x.uia.desktop()
	if desktop.valid() == false then
		return 1
	end
auto snapshot = desktop.snapshot_json(1, 32)
desktop.release()
return 0
end
```

V2 pattern methods are exposed on `Element`: `toggle`, `toggle_state`,
`range_value`, `range_min`, `range_max`, `set_range_value`, `scroll`,
`close_window`, and `wait_window_closed`. Unsupported patterns return safe
defaults and set `x.uia.last_error()`.

V2 event queues are exposed through `x.uia.events()`. Subscribe with
`subscribe_focus_changed()` or `subscribe_property_changed(element, propertyId)`,
then call `poll_json(timeout_ms)` to receive compact event records. Use
`poll_callback(callback, timeout_ms)` when the next event should be dispatched
to an xlang callback on the caller thread.

Cached tree reads are exposed as `Element.cached_children_json(depth, maxNodes)`.
They return the same core JSON fields as `snapshot_json`, but use a UI Automation
cache request for the subtree before walking cached children.

Recorder helpers are exposed as `x.uia.locator_for(element)` and
`x.uia.focused_locator()`. They return structured JSON with a strategy,
confidence, stable identity fields, class path, index, hwnd, and rectangle.

The higher-level `x.ui.auto` package builds on `x.uia` with pure xlang locator
objects and helpers such as `wait_exists`, `click`, `type_text`, and `read`.

Always check `valid()` before using an element, process, condition, or test
window. Call `release()` when the handle is no longer needed. `x.uia.shutdown()`
releases the package-global UI Automation object and uninitializes COM only when
`x.uia` initialized it; later calls lazily initialize UIA again.
