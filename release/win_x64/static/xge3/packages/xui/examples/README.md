# XUI 3 完整范例

当前提供 48 个控件／组合入口，以及原有 8 个资源与生命周期示例。
所有控件示例使用当前声明式绑定，不依赖旧的 XuiApp 或裸句柄包装。

双击 `run_xui_examples.bat` 可启动综合画廊。

## 开始使用

从发布目录 `release/win_x64` 运行：

```powershell
./xl.exe run examples/xui/all_controls.xl
./xl.exe run examples/xui/button.xl
./xl.exe run examples/xui/codeedit.xl
./xl.exe build examples/xui/button.xl -o button.exe
```

`all_controls.xl` 的下拉列表可以切换全部 47 个独立场景；切换时先关闭独立的菜单／浮动窗口，再销毁旧页面的控件与回调。
单独运行任一控件示例可直接查看其配置和事件代码。关闭窗口或按 Escape 退出。
Linux 使用同一目录下的 `./xl run ...`；需要桌面及可用的 XGE 图形后端。

默认使用发布目录 `data/fonts/Consolas-with-Yahei_Nerd_Font.ttf`。
可用 `XUI_EXAMPLE_FONT` 显式指定其他 TTF／OTF 字体路径。
`XLANG_PACKAGE_PATH` 如有设置，必须指向配套的 `_xge_native`、`xge`、`xui` 三个包的父目录。
可选静态 SDK 的选择方法见发布目录 `static/xge3/README.md`。

## 48 个入口

| 文件 | 展示内容 |
| --- | --- |
| [accordion.xl](accordion.xl) | Accordion / expand sections |
| [basic.xl](basic.xl) | Basic controls / interactive state |
| [breadcrumb.xl](breadcrumb.xl) | Breadcrumb / navigation callbacks |
| [button.xl](button.xl) | Button / click and disabled states |
| [canvas.xl](canvas.xl) | Canvas / persistent drawing and pen |
| [carousel.xl](carousel.xl) | Carousel / page navigation |
| [chart.xl](chart.xl) | Chart / typed data columns |
| [checkbox.xl](checkbox.xl) | Checkbox / three-state selection |
| [codeedit.xl](codeedit.xl) | Code editor / minimap and syntax |
| [colorpicker.xl](colorpicker.xl) | Color picker / RGBA popup |
| [combobox.xl](combobox.xl) | Combo box / popup selection |
| [datepicker.xl](datepicker.xl) | Date picker / calendar popup |
| [dockpanel.xl](dockpanel.xl) | Dock panel / owned client windows |
| [hyperlink.xl](hyperlink.xl) | Hyperlink / local click callback |
| [image.xl](image.xl) | Image / generated RGBA resource |
| [input.xl](input.xl) | Input / edit UTF-8 text |
| [listview.xl](listview.xl) | List / selection and scrolling |
| [menu.xl](menu.xl) | Menu / command selection |
| [messagelist.xl](messagelist.xl) | Message list / structured nodes |
| [numericinput.xl](numericinput.xl) | Numeric input / limits and step |
| [page.xl](page.xl) | Pagination / page count |
| [panel.xl](panel.xl) | Panel / child ownership |
| [popup.xl](popup.xl) | Popup / explicit open and close |
| [progress.xl](progress.xl) | Progress / change the value |
| [propertygrid.xl](propertygrid.xl) | Property grid / editable values |
| [qrcode.xl](qrcode.xl) | QR code / UTF-8 content |
| [radio.xl](radio.xl) | Radio / exclusive choice |
| [rangeslider.xl](rangeslider.xl) | Range slider / drag both endpoints |
| [richtext.xl](richtext.xl) | Rich text / current Document View model |
| [scrollbar.xl](scrollbar.xl) | Scrollbar / page and position |
| [scrollview.xl](scrollview.xl) | Scroll view / content larger than viewport |
| [separator.xl](separator.xl) | Separator / horizontal rule |
| [slider.xl](slider.xl) | Slider / drag the knob |
| [splitlayout.xl](splitlayout.xl) | Split layout / draggable divider |
| [statusbar.xl](statusbar.xl) | Status bar / text and progress |
| [stepbar.xl](stepbar.xl) | Step bar / workflow stages |
| [tablegrid.xl](tablegrid.xl) | Tablegrid / columns and cells |
| [tableview.xl](tableview.xl) | Tableview / columns and cells |
| [tabs.xl](tabs.xl) | Tabs / Context-owned pages |
| [taginput.xl](taginput.xl) | Tag input / add and remove tags |
| [terminal.xl](terminal.xl) | Terminal / ANSI text without a child process |
| [textedit.xl](textedit.xl) | Text editor / multiline selection |
| [timeline.xl](timeline.xl) | Timeline / layers and spans |
| [toggle.xl](toggle.xl) | Toggle / enabled state |
| [toolbar.xl](toolbar.xl) | Toolbar / commands and tooltips |
| [treeview.xl](treeview.xl) | Tree / expand and select |
| [window.xl](window.xl) | Window / move and resize |
| [all_controls.xl](all_controls.xl) | Complete control gallery |

`richtext.xl` 使用当前 Document／DocumentView 模型；不会恢复已经废弃的 RichText 包装类。
`terminal.xl` 演示内存终端和 ANSI 颜色，不启动 shell。`hyperlink.xl` 仅交付本地点击回调，不启动浏览器。
`image.xl` 在内存中生成图片，无额外图片文件依赖。表格示例使用拥有的单元格回调，TableGrid 支持编辑写回。

## 运行与维护

`_support.xl` 只共享示例窗口循环、字体、布局辅助与资源清理。每个控件方法仍直接调用原生声明。
窗口循环泵入输入、交付事件、执行布局和更新，再渲染到复用的 Surface；仅窗口尺寸变化时重建目标。
退出时先撤销帧回调，关闭 Context 控件图及其回调，再释放字体、Surface、Proxy 和引擎。
数组、字符串列和含指针记录使用现有编译器投影；这些批量输入有 O(n) 临时存储成本。
表格的 `setColumns` 直接调用原生 `SetColumnsCopy`，列 ID／标题由表格持有独立副本；临时数组不逃逸。

需要有界运行时可设置：

```powershell
$env:XUI_EXAMPLE_FRAMES = '3'
./xl.exe run examples/xui/button.xl
Remove-Item Env:XUI_EXAMPLE_FRAMES
```

开发工具 `tools/check_xui_gallery.py` 可批量执行 `check,cgen,build`；
显式选择 `smoke` 或 `execute` 才会打开有界窗口；后者运行已构建的程序，并让综合画廊依次覆盖 47 个场景。`tools/run_xui_examples.ps1 -Example button -Run -Frames 3`
可运行单个示例。`all_controls.xl` 由 `tools/update_xui_gallery.py` 从独立示例生成，修改控件示例后重新生成。

原有八个示例说明如下，旧验收记录保留各自的范围和日期。

---

# XUI 3 examples

Use the matching compiler/runtime, engine and staged `xui`, `xge` and
`_xge_native` packages. `XLANG_PACKAGE_PATH` names their parent directory.
The ordinary Context constructor requires an owned Proxy: `xui.create(proxy)`.
There is no backend-less safe Context constructor or raw-handle reconstruction.
Run from the installed runtime directory named by `$runtime`:

```powershell
$env:XLANG_PACKAGE_PATH = "$runtime\extlibs"
& "$runtime\xl.exe" "$runtime\examples\xui\headless_path.xl"
& "$runtime\xl.exe" "$runtime\examples\xui\headless_cell.xl"
& "$runtime\xl.exe" "$runtime\examples\xui\context_graph.xl"
& "$runtime\xl.exe" "$runtime\examples\xui\manager_lifetime.xl"
& "$runtime\xl.exe" "$runtime\examples\xui\document_model.xl"
& "$runtime\xl.exe" "$runtime\examples\xui\document_ui_headless.xl"
& "$runtime\xl.exe" "$runtime\examples\xui\data_object.xl"
```

| Example | Prerequisites and assertions | Cleanup |
| --- | --- | --- |
| `headless_path.xl` | Native Path; no Proxy, Context or graphics device. Two commands are stored; clear resets the count. | Path closes in `finally`. |
| `headless_cell.xl` | Owned native Cell support; no window/device. Text and the full unsigned 64-bit value round-trip. | Cell closes. Native UI text rejects embedded NUL. |
| `context_graph.xl` | Shared default XGE Proxy and safe Context construction. Builds a Panel/Button graph without rendering; verifies text and registers a typed owning callback. It does **not** dispatch a click or assert event delivery. | Clears the callback, closes Button, then Context graph and Proxy. |
| `manager_lifetime.xl` | Context-owned Manager support. Creates an unopened MsgBox and a logical MsgTip; dismiss invokes a typed callback exactly once. No OS dialog, frame, browser or clipboard operation is requested. | Clears the callback and closes the children, Context and Proxy in order. |
| `document_model.xl` | Matched default-provenance Model engine, leased-domain runtime and sized-record compiler. Builds a Rich document with no Context/Proxy/window, commits a unique transaction, reads exact UTF-8 text into a scoped Buffer and keeps the immutable snapshot readable after Document closes. | Transaction shell, Document, Snapshot and ChangeSet each release their real native owner. |
| `document_ui_headless.xl` | Matched Document UI declarations and nested-pack compiler. Builds a View/Editor/MessageList graph; a real Renderer measurement callback updates the native Vec2/float outputs and saves an independently shared Snapshot. No frame, draw, browser, file or clipboard operation. | Clears owning callbacks, closes Context-owned widgets and Context, verifies the independent Renderer rejects expired-Context methods, then releases Renderer/Model/Proxy owners. |
| `data_object.xl` | Owned DataObject and nullable writable-span support. Preserves embedded NUL, queries a lazy provider, reads into a pre-sized Buffer and keeps an independent shared owner readable. No Context, frame, clipboard or OS drag operation. | Both owned DataObject references close. |
| `gui_surface_draw.xl` | Interactive desktop, matching XGE 2D backend, target-Surface and Draw support. Allocates only after a frame begins. Draw ends before release, then the image is presented for one frame and the demo exits. | Releases Draw, Surface, callback, Proxy and engine, including exceptional exits. |

The GUI example is separate and must be requested explicitly:

```powershell
& "$runtime\xl.exe" "$runtime\examples\xui\gui_surface_draw.xl"
```

`panelNative()` and `buttonNative()` here use only the native-validated NULL
descriptor default. No caller descriptor containing untracked resources or
callbacks is created. Control setters call the original C symbols; Proxy
drawing calls its typed native function-pointer fields. Font/Surface ownership,
callback epochs and checked error propagation still perform their documented
runtime bookkeeping. This is not a claim of zero total cost.

For a long-running renderer, retain and reuse size-matched Surfaces between
frames. This deliberately single-frame lifetime example does not recommend
reallocating render targets every frame. It does not cover input pumping,
window resize, font selection, layout policy or a full application framework.

The first four headless examples pass Cgen/IR and actual generated-C execution
at O0 and O2 on Windows and Linux against the matched Model engine. The
Document model example has its separate real Model acceptance. The GUI
example retains compilation/ABI acceptance only. Allocator fault injection
and lifetime/module-unload tests remain separate fixtures. See
`docs/xge3-xui3-example-status.md` for exact qualification and legacy gaps;
these checks do not qualify final runtime delivery or graphics execution.
DataObject has a separate CPU/ABI matrix documented in
`docs/xge3-xui3-data-object.md`.
