# winui — Win32 GUI 程序（C 混编 + native-backed class 封装窗口）

用 xlang + C 混编编写一个真实的 Win32 GUI 程序，全面验证 native-backed class 封装窗口、控件、消息循环、C↔xlang 双向回调的完整能力。

## 目标

覆盖 `DEMO6功能清单.md` 第 15 节「C 混编、ABI 与导出」和第 11 节 native-backed class 在 **GUI 场景**下的全部公开能力：

| # | 能力 | 本工具覆盖位置 |
|---|---|---|
| 1 | `extern "c_fn" func f(point, string, int) as point` 调用 Win32 包装函数 | `winui_window_create` / `winui_label_create` 等 |
| 1b | C 函数返回 `const char*` 映射 xlang `string` | `winui_get_text` |
| 2 | native-backed class 封装 HWND（`native point handle`） | `Window` / `Label` / `Button` / `Edit` |
| 3 | `create(point parent, string, int...)` 带 point 形参的 native 构造 | `Label/Button/Edit.create` |
| 4 | 多个 class 共用同一 C 符号作为成员 extern | `winui_set_text` / `winui_get_text` 三类共用 |
| 5 | native-backed class 自动析构（destroy） | Window/控件作用域结束自动 destroy |
| 6 | `.xl + .c` 混合构建 + `--link User32 --link Gdi32` | `xl build winui.xl winui_native.c` |
| 7 | C → xlang 反向回调（消息循环中触发） | WndProc → `winui_on_button_click` |
| 8 | headless 自检模式（不阻塞、可自动化） | `--headless` |

## 文件

- `winui.xl` — 主工具。xlang 侧用 4 个 native-backed class 封装 Window/Label/Button/Edit，并提供 `--headless` 自检和 GUI 两种模式。
- `winui_native.c` — C 侧。封装 Win32 API（CreateWindow/MessageBox/GetMessage 等），安装自定义 WndProc，在 WM_COMMAND 时回调 xlang 导出 `winui_on_button_click`。
- `probe_string_copy.xl` / `probe_string_copy2.xl` — 边界探针：验证 C 返回 `const char*` 的拷贝语义（发现 BUG-WINUI1）。

## 运行与验证

```bat
cd /d D:\GIT\x-lang\demo6\release

:: headless 自检（两条路径都必须 EXIT=0）
xl.exe run   projects\dev\winui\winui.xl projects\dev\winui\winui_native.c --link User32 --link Gdi32 -- --headless
xl.exe build projects\dev\winui\winui.xl projects\dev\winui\winui_native.c -o projects\dev\winui\winui.exe --link User32 --link Gdi32
projects\dev\winui\winui.exe --headless

:: GUI 模式（弹出一个真实窗口）
projects\dev\winui\winui.exe
::   - 在文本框输入名字
::   - 点击 "Say Hi" → WndProc → xlang 回调 → MessageBox 显示 "You typed: ..."
::   - 关闭窗口结束
```

## 关键设计

### 类型映射（经 `xl cgen --emit-header` 实测确认）

| xlang 类型 | C 类型 | 用途 |
|---|---|---|
| `point` | `void*` | HWND / 控件句柄；create 的 parent 参数 |
| `string`（形参） | `const char*` | UTF-8 文本输入 |
| `string`（返回） | `const char*` | **见 BUG-WINUI1：返回值是借用，不拷贝** |
| `int` | `int64_t` / `long long` | 坐标、尺寸、flags、id、返回码 |

### native-backed class 封装

```xlang
class Window
    native point handle
    extern "winui_window_create"  create(string title, int w, int h)
    extern "winui_window_destroy" destroy()
    extern "winui_window_show"    func show()
    ...
end
```

- `create(string,int,int)` → C `void* winui_window_create(const char*, int64_t, int64_t)`，返回值成为 `self.handle`。
- `func show()` → C `void winui_window_show(void* handle)`，第一个参数自动是 handle。
- `Label/Button/Edit` 的 `create` 第一个参数是 `point parent`，传入 `window.getHandle()` 即可。

### C → xlang 回调

```c
// winui_native.c 的 WndProc：
case WM_COMMAND:
    if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) >= 1000)
        winui_on_button_click(LOWORD(wParam));   // 调用 xlang 导出
```
```xlang
export "winui_on_button_click" func winui_on_button_click(int id) as int
    string text = winui_get_text(gEditHandle)
    winui_message_box(gMainHwnd, "You typed: " + text, "Button Clicked", 0)
    return 0
end
```

回调在 `winui_message_loop()`（C 的 GetMessage 循环）内被触发，验证了 xlang 运行时在 C 调用栈深处仍能正确派发到 xlang 导出函数。

### 链接耦合注意

`winui_native.c` 通过 `extern long long winui_on_button_click(...)` 引用了 xlang 导出符号。**任何与 `winui_native.c` 链接的 `.xl` 文件都必须导出 `winui_on_button_click`**，否则 TCC 报 `unresolved reference to 'winui_on_button_click'`（见 `probe_string_copy.xl` 初版）。这是 C 混编 `extern` 引用 xlang 导出符号的固有权衡，不是 BUG。

## 发现的 BUG

见 `../BUGS.md` 的 **BUG-WINUI1**：`extern` 函数返回 `string` 时 xlang 不拷贝 C 的 `const char*`，导致对静态缓冲区的返回值发生别名（第二次调用覆盖第一次的值）。绕过：用 `s + ""` 或装入 typed container 强制拷贝。
