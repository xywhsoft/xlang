# x.ui.auto

`x.ui.auto` is the high-level automation layer built on `x.uia`.

The package is pure xlang. It provides locator objects plus common automation
helpers:

- `wait_exists(locator)`
- `click(locator)`
- `type_text(locator, text)`
- `read(locator)`
- `screenshot(locator, path)`
- `focused_locator_json()`
- `locator_from_json(json)`

Use `within(locator, hwnd)` to scope a locator to a known window. This keeps
desktop scans narrow and avoids accidental matches in other applications.

`locator_from_json(json)` accepts the structured locator JSON produced by
`x.uia.locator_for(element)` and `x.uia.focused_locator()`.

`screenshot(locator, path)` finds the target element, reads its UIA bounding
rectangle, captures that screen region through the separate `opencv` image
package, and writes the image file.
