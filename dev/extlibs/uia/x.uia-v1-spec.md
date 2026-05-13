# x.uia V1 Design Spec

Status: design ready for implementation
Last updated: 2026-05-13
Design owner directory: `dev/extlibs/uia`
Implementation target: `demo5/release/libs/x.uia`
Primary module: `x.uia`

## 1. Goal

`x.uia` provides a first-party Windows UI Automation library for xlang. It should
let xlang scripts find native desktop UI elements, inspect properties, wait for
state, and perform basic actions such as click, focus, invoke, and value input.

The V1 target is a practical automation foundation, not a full RPA product. It
must be small enough to land in the current demo5 XLE package system and strong
enough to automate real Windows applications in smoke tests.

## 2. Non Goals

- No cross-platform abstraction in V1. Windows UI Automation is the concrete
  backend.
- No image recognition or OCR in V1. Those belong in separate packages such as
  `x.image.*`, `x.ocr.*`, or a higher-level RPA package.
- No browser automation replacement. Browser automation should stay in the CDP
  browser extension line.
- No event callback ABI dependency in V1. Polling and explicit waits are enough
  for the first release.
- No direct dependency on Node.js, Playwright, AutoIt, pywinauto, or Python.
- No hidden code under `dev/extlibs/uia`; live package code belongs in
  `demo5/release/libs/x.uia`.

## 3. Package Shape

Use a source-mixed XLE package:

```text
demo5/release/libs/x.uia/
  package.json
  src/uia.xl
  src/x_uia_artifact.c
  src/x_uia_min.h
```

The package manifest should follow the `x.com` source artifact pattern:

```json
{
  "format": "xle-1",
  "package": {
    "name": "x.uia",
    "version": "0.1.0",
    "abiVersion": "1",
    "xlangAbi": "1",
    "abiId": "x.uia@0.1.0#abi1"
  },
  "artifact": {
    "kind": "source",
    "path": "src\\uia.xl",
    "cSources": [
      "src\\x_uia_artifact.c"
    ],
    "linkLibs": [
      "ole32",
      "oleaut32"
    ]
  },
  "dependencies": [],
  "modules": [
    {
      "key": "x.uia",
      "abiId": "x.uia@0.1.0#abi1::x.uia",
      "file": "src\\uia.xl",
      "root": 0,
      "native": 0,
      "binding": "",
      "imports": [],
      "exports": [
        { "name": "version", "symbol": "version", "kind": "function", "overloadCount": 1 },
        { "name": "last_error", "symbol": "last_error", "kind": "function", "overloadCount": 1 },
        { "name": "launch", "symbol": "launch", "kind": "function", "overloadCount": 1 },
        { "name": "test_window", "symbol": "test_window", "kind": "function", "overloadCount": 1 },
        { "name": "desktop", "symbol": "desktop", "kind": "function", "overloadCount": 1 },
        { "name": "focused", "symbol": "focused", "kind": "function", "overloadCount": 1 },
        { "name": "from_hwnd", "symbol": "from_hwnd", "kind": "function", "overloadCount": 1 },
        { "name": "from_point", "symbol": "from_point", "kind": "function", "overloadCount": 1 },
        { "name": "element", "symbol": "element", "kind": "function", "overloadCount": 1 },
        { "name": "by_name", "symbol": "by_name", "kind": "function", "overloadCount": 1 },
        { "name": "by_process_id", "symbol": "by_process_id", "kind": "function", "overloadCount": 1 },
        { "name": "by_automation_id", "symbol": "by_automation_id", "kind": "function", "overloadCount": 1 },
        { "name": "by_class_name", "symbol": "by_class_name", "kind": "function", "overloadCount": 1 },
        { "name": "by_control_type", "symbol": "by_control_type", "kind": "function", "overloadCount": 1 },
        { "name": "all_of", "symbol": "all_of", "kind": "function", "overloadCount": 1 },
        { "name": "any_of", "symbol": "any_of", "kind": "function", "overloadCount": 1 },
        { "name": "not_condition", "symbol": "not_condition", "kind": "function", "overloadCount": 1 }
      ]
    }
  ],
  "functions": []
}
```

If MinGW provides a usable `uiautomationcore` import library, add it to
`linkLibs`. If it does not, keep the first implementation on `ole32` and
`oleaut32` and call `CoCreateInstance` through explicitly declared CLSIDs and
interfaces.

## 4. Native Boundary

The native layer owns all Windows COM objects. Xlang receives opaque `point`
handles wrapped in xlang classes.

Native handle types:

```c
typedef struct XUiaApp XUiaApp;
typedef struct XUiaElement XUiaElement;
typedef struct XUiaCondition XUiaCondition;
typedef struct XUiaRect XUiaRect;
```

Rules:

- Every returned `point` must either be owned by an xlang wrapper object or be
  explicitly disposable.
- Every COM pointer must be released exactly once.
- `CoInitializeEx(NULL, COINIT_APARTMENTTHREADED)` is initialized lazily and
  protected by a package-level reference count.
- Returned strings are UTF-8 `string` values created through the xlang runtime.
- Repeated failure details are available through `x.uia.last_error()`.
- V1 uses sync calls and timeout polling. It does not expose native callbacks.

## 5. Native Exports

The XLang facade in `src/uia.xl` should wrap these C exports:

```xl
extern "x_uia_version" func x_uia_version() as string
extern "x_uia_last_error" func x_uia_last_error() as string
extern "x_uia_launch" func x_uia_launch(string command) as point
extern "x_uia_test_window_create" func x_uia_test_window_create() as point
extern "x_uia_test_window_valid" func x_uia_test_window_valid(point window) as bool
extern "x_uia_test_window_hwnd" func x_uia_test_window_hwnd(point window) as point
extern "x_uia_test_window_edit_hwnd" func x_uia_test_window_edit_hwnd(point window) as point
extern "x_uia_test_window_button_hwnd" func x_uia_test_window_button_hwnd(point window) as point
extern "x_uia_test_window_close" func x_uia_test_window_close(point window) as bool
extern "x_uia_test_window_release" func x_uia_test_window_release(point window) as bool
extern "x_uia_process_valid" func x_uia_process_valid(point process) as bool
extern "x_uia_process_id" func x_uia_process_id(point process) as int
extern "x_uia_process_close" func x_uia_process_close(point process) as bool
extern "x_uia_process_release" func x_uia_process_release(point process) as bool

extern "x_uia_desktop" func x_uia_desktop() as point
extern "x_uia_focused" func x_uia_focused() as point
extern "x_uia_from_hwnd" func x_uia_from_hwnd(point hwnd) as point
extern "x_uia_from_point" func x_uia_from_point(int x, int y) as point

extern "x_uia_element_release" func x_uia_element_release(point element) as bool
extern "x_uia_element_valid" func x_uia_element_valid(point element) as bool
extern "x_uia_element_name" func x_uia_element_name(point element) as string
extern "x_uia_element_automation_id" func x_uia_element_automation_id(point element) as string
extern "x_uia_element_class_name" func x_uia_element_class_name(point element) as string
extern "x_uia_element_control_type" func x_uia_element_control_type(point element) as int
extern "x_uia_element_hwnd" func x_uia_element_hwnd(point element) as point
extern "x_uia_element_rect_json" func x_uia_element_rect_json(point element) as string

extern "x_uia_element_find_first" func x_uia_element_find_first(point element, point condition, int scope, int timeout_ms) as point
extern "x_uia_element_find_all" func x_uia_element_find_all(point element, point condition, int scope, int timeout_ms) as array<var>
extern "x_uia_element_parent" func x_uia_element_parent(point element) as point
extern "x_uia_element_children" func x_uia_element_children(point element) as array<var>

extern "x_uia_element_set_focus" func x_uia_element_set_focus(point element) as bool
extern "x_uia_element_click" func x_uia_element_click(point element, int timeout_ms) as bool
extern "x_uia_element_invoke" func x_uia_element_invoke(point element) as bool
extern "x_uia_element_value" func x_uia_element_value(point element) as string
extern "x_uia_element_set_value" func x_uia_element_set_value(point element, string value) as bool
extern "x_uia_element_text" func x_uia_element_text(point element) as string

extern "x_uia_condition_release" func x_uia_condition_release(point condition) as bool
extern "x_uia_condition_valid" func x_uia_condition_valid(point condition) as bool
extern "x_uia_by_name" func x_uia_by_name(string name) as point
extern "x_uia_by_process_id" func x_uia_by_process_id(int process_id) as point
extern "x_uia_by_automation_id" func x_uia_by_automation_id(string automation_id) as point
extern "x_uia_by_class_name" func x_uia_by_class_name(string class_name) as point
extern "x_uia_by_control_type" func x_uia_by_control_type(int control_type) as point
extern "x_uia_condition_and" func x_uia_condition_and(point left, point right) as point
extern "x_uia_condition_or" func x_uia_condition_or(point left, point right) as point
extern "x_uia_condition_not" func x_uia_condition_not(point condition) as point
```

## 6. XLang Facade

`uia.xl` should expose ergonomic wrappers and hide raw `point` handles:

```xl
class Element extends object
  nocopy
  var handle as point

  create(point h)
    self.handle = h
  end

  destroy()
    if self.valid() then
      x_uia_element_release(self.handle)
    end
  end

  func valid() as bool
    return x_uia_element_valid(self.handle)
  end

  func name() as string
    return x_uia_element_name(self.handle)
  end

  func automation_id() as string
    return x_uia_element_automation_id(self.handle)
  end

  func class_name() as string
    return x_uia_element_class_name(self.handle)
  end

  func control_type() as int
    return x_uia_element_control_type(self.handle)
  end

  func rect_json() as string
    return x_uia_element_rect_json(self.handle)
  end

  func find_first(Condition condition, int timeout_ms) as Element
    return Element(x_uia_element_find_first(self.handle, condition.handle, TREE_SCOPE_DESCENDANTS, timeout_ms))
  end

  func click() as bool
    return x_uia_element_click(self.handle, 5000)
  end
end
```

Public module functions:

```xl
func version() as string
func last_error() as string
func launch(string command) as Process
func test_window() as TestWindow
func desktop() as Element
func focused() as Element
func from_hwnd(point hwnd) as Element
func from_point(int x, int y) as Element
func element(point handle) as Element

func by_name(string name) as Condition
func by_process_id(int process_id) as Condition
func by_automation_id(string automation_id) as Condition
func by_class_name(string class_name) as Condition
func by_control_type(int control_type) as Condition
func all_of(Condition left, Condition right) as Condition
func any_of(Condition left, Condition right) as Condition
func not_condition(Condition condition) as Condition
```

## 7. Constants

Expose stable constants in `uia.xl`:

```xl
const TREE_SCOPE_ELEMENT = 1
const TREE_SCOPE_CHILDREN = 2
const TREE_SCOPE_DESCENDANTS = 4
const TREE_SCOPE_SUBTREE = 7

const CONTROL_BUTTON = 50000
const CONTROL_CALENDAR = 50001
const CONTROL_CHECK_BOX = 50002
const CONTROL_COMBO_BOX = 50003
const CONTROL_EDIT = 50004
const CONTROL_HYPERLINK = 50005
const CONTROL_IMAGE = 50006
const CONTROL_LIST_ITEM = 50007
const CONTROL_LIST = 50008
const CONTROL_MENU = 50009
const CONTROL_MENU_BAR = 50010
const CONTROL_MENU_ITEM = 50011
const CONTROL_PROGRESS_BAR = 50012
const CONTROL_RADIO_BUTTON = 50013
const CONTROL_SCROLL_BAR = 50014
const CONTROL_SLIDER = 50015
const CONTROL_SPINNER = 50016
const CONTROL_STATUS_BAR = 50017
const CONTROL_TAB = 50018
const CONTROL_TAB_ITEM = 50019
const CONTROL_TEXT = 50020
const CONTROL_TOOL_BAR = 50021
const CONTROL_TOOL_TIP = 50022
const CONTROL_TREE = 50023
const CONTROL_TREE_ITEM = 50024
const CONTROL_CUSTOM = 50025
const CONTROL_GROUP = 50026
const CONTROL_THUMB = 50027
const CONTROL_DATA_GRID = 50028
const CONTROL_DATA_ITEM = 50029
const CONTROL_DOCUMENT = 50030
const CONTROL_SPLIT_BUTTON = 50031
const CONTROL_WINDOW = 50032
const CONTROL_PANE = 50033
const CONTROL_HEADER = 50034
const CONTROL_HEADER_ITEM = 50035
const CONTROL_TABLE = 50036
const CONTROL_TITLE_BAR = 50037
const CONTROL_SEPARATOR = 50038
```

## 8. Search and Wait Semantics

`find_first(condition, timeout_ms)` should:

1. Try immediately.
2. Poll until timeout.
3. Sleep between attempts using a small interval, initially 50 ms.
4. Return an invalid `Element` when no match is found.
5. Set `last_error()` to a clear timeout message.

V1 should use these default timeouts:

- `find_first`: caller supplied, example default 5000 ms at facade level.
- `click`: 5000 ms.
- `set_value`: 5000 ms.
- `text`: no wait unless the facade adds a `wait_text` helper later.

## 9. Click Semantics

`Element.click()` should use the safest available method:

1. Try `InvokePattern.Invoke` when available.
2. If not available, try `SelectionItemPattern.Select` for selectable items.
3. If not available, focus element and synthesize a center-point mouse click.

The fallback mouse click must use the bounding rectangle center and should fail
if the rectangle is empty or offscreen.

## 10. Value and Text Semantics

`Element.value()` and `Element.set_value()` use `ValuePattern`.

`Element.text()` tries:

1. `TextPattern` document range text.
2. `ValuePattern` value.
3. `NameProperty`.

The fallback order must be documented because real Windows controls expose text
through different patterns.

## 10.1 Process Launch Helper

`x.uia.launch(command)` is a small Windows process helper for automation
scripts. It starts a target process without blocking and returns a `Process`
wrapper.

`Process` supports:

```xl
func valid() as bool
func id() as int
func close() as bool
func release() as bool
```

This helper exists so examples can launch their own target and then locate the
window with `by_process_id(pid)`, avoiding accidental interaction with a user's
existing windows.

## 10.2 Controlled Test Window

`x.uia.test_window()` creates a small Win32 window with a child `EDIT` control
and a child `BUTTON` control. It is part of the package test harness and gives
UIA stable controls for `set_value`, `value`, `text`, `click`, and `invoke`
validation without depending on Notepad, Calculator, localization, or modern app
process redirection.

## 11. Error Model

V1 uses a package-level last error:

```xl
string message = x.uia.last_error()
```

Error message format:

```text
<operation>: <kind>: <details>
<operation>: HRESULT 0x800xxxxx
<operation>: timeout after <n> ms
```

Examples:

```text
find_first: timeout after 3000 ms
desktop: HRESULT 0x80040154
click: no usable pattern and empty bounding rectangle
```

Future versions may add structured error objects, but V1 must keep
`last_error()` stable.

## 12. Encoding

Windows BSTR strings are UTF-16. The native layer must convert UTF-16 to UTF-8
before returning xlang strings.

Rules:

- Empty BSTR returns `""`.
- Null BSTR returns `""` and does not set an error.
- Conversion failure returns `""` and sets `last_error()`.
- Input strings from xlang are UTF-8 and must be converted to UTF-16 before
  property condition creation or `ValuePattern.SetValue`.

## 13. Memory and Lifetime

Element ownership:

- Every `Element` wraps one `XUiaElement*`.
- `Element.destroy()` releases the native handle.
- Native element handles hold one `IUIAutomationElement*`.
- `find_all` and `children` return `array<var>` containing raw element handles.
  User code should cast each item to `point` and call `x.uia.element(handle)`
  when it needs an `Element` wrapper.

Example:

```xl
func as_point(var value) as point
  return value
end

auto children = root.children()
auto first = x.uia.element(as_point(children[0]))
```

Condition ownership:

- Every `Condition` wraps one `XUiaCondition*`.
- Conditions are immutable after construction.
- Combination helpers create a new native condition.
- Releasing a combined condition must release only the combined native object,
  not double-release its source conditions.

Package-global UIA ownership:

- `x.uia` lazily creates one process-local `IUIAutomation` object.
- `x.uia.shutdown()` releases that global object and calls `CoUninitialize()`
  only when this package successfully initialized COM.
- `x.uia.shutdown()` is idempotent and returns `true` even when UIA was never
  initialized.
- A later `desktop()`, `focused()`, `from_hwnd()`, `from_point()`, or condition
  factory call can lazily initialize UIA again.
- Deterministic shutdown is covered by `use_x_uia_shutdown.xl`.

## 14. Minimal Windows UIA Interface Strategy

There are two viable implementation strategies:

### Strategy A: Use Windows SDK headers

Include:

```c
#include <windows.h>
#include <ole2.h>
#include <uiautomation.h>
```

Pros:

- Less manual interface definition.
- Easier to keep GUIDs and method signatures correct.

Cons:

- May be fragile with the bundled TCC/MinGW include set.
- May require `uiautomationcore` import library.

### Strategy B: Local minimal header

Create `src/x_uia_min.h` with only the required GUIDs, enums, interface vtables,
and method signatures.

Pros:

- Better control over compatibility with demo5 toolchains.
- Smaller V1 surface.

Cons:

- Higher risk of signature mistakes.
- Needs careful validation against Microsoft UIA interface definitions.

Implementation decision: use Strategy B for the package. A probe showed that the
bundled TCC include path does not provide `uiautomation.h`; GCC can see the
w64devkit header, but the release package must remain compatible with the
demo5/TCC package path. `src/x_uia_min.h` therefore owns the V1 UIA COM surface.

Keep the SDK probe under `dev/extlibs/uia/probes/uia_sdk_probe.c` as evidence
and as a future check if the toolchain changes.

## 15. V1 Example Scripts

Add these scripts under `demo5/release/examples/xle_official_native/`:

```text
use_x_uia_version.xl
use_x_uia_smoke.xl
use_x_uia_focused.xl
use_x_uia_timeout.xl
use_x_uia_tree.xl
use_x_uia_lifetime.xl
use_x_uia_shutdown.xl
use_x_uia_launch_notepad.xl
use_x_uia_test_window_value.xl
use_x_uia_test_window_click.xl
```

### use_x_uia_smoke.xl

```xl
import x.uia

func main() as int
  if x.uia.version() == "" then
    return 1
  end
  auto root = x.uia.desktop()
  if root.valid() == false then
    return 2
  end
  if root.name() == "" then
    return 3
  end
  return 0
end
```

### use_x_uia_timeout.xl

```xl
import x.uia
import std.str

func main() as int
  auto root = x.uia.desktop()
  auto missing = root.find_first(x.uia.by_name("__xlang_missing_uia_element__"), 100)
  if missing.valid() then
    return 1
  end
  if str.contains(x.uia.last_error(), "timeout") == false then
    return 2
  end
  return 0
end
```

## 16. Validation Matrix

Required before V1 can be called done:

- Build `demo5` successfully after adding `x.uia`.
- Compile a script that imports `x.uia`.
- Run `use_x_uia_version.xl` or `run_x_uia_static_smoke.bat` in
  non-interactive environments.
- Run `use_x_uia_smoke.xl`.
- Run timeout smoke with deterministic missing element.
- Run the controlled test-window value and click smokes.
- Run at least one real app smoke such as Notepad process launch and window
  discovery.
- Confirm `last_error()` is populated after failure and cleared or replaced on
  next operation.
- Confirm invalid handles do not crash.
- Confirm repeated create/destroy loops do not leak obvious handles or crash.

## 17. Risks

### Toolchain UIA Header Support

The bundled compiler may not have complete `uiautomation.h` support. Mitigation:
keep `x_uia_min.h` as an accepted fallback.

### UAC and Integrity Levels

Low-integrity or normal processes cannot automate elevated windows reliably.
V1 should document this and return a clear error instead of pretending support.

### Desktop Session State

UI Automation generally requires an interactive desktop. Locked sessions,
service sessions, and headless CI may fail. V1 tests should separate interactive
smokes from compile-only smokes.

### Localization

Name-based locators are fragile across OS languages. Prefer `automation_id` and
`control_type` when possible. Examples should avoid depending only on English UI
text unless the app is controlled by the test.

### Eventing

UIA events require callbacks and more lifecycle handling. V1 should use polling
and wait helpers; event registration is V2+.

## 18. V1 Completion Definition

V1 is complete when:

- `demo5/release/libs/x.uia` exists as a source-mixed XLE package.
- `import x.uia` works from release examples.
- The facade exposes `Element` and `Condition` classes.
- Desktop, focused, hwnd, point, property lookup, simple tree traversal, click,
  focus, invoke, value, and text are implemented.
- At least five smoke scripts exist and the interactive subset passes on a
  normal Windows desktop.
- The package has clear failure messages and no known crash on invalid handles.
- The package exposes deterministic `shutdown()` for the global UIA COM object
  and can lazily reinitialize after shutdown.
