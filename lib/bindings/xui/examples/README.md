# XUI 入门范例

不创建窗口的数据模型范例见 [document_exports.xl](document_exports.xl)：加载 Markdown，
导出 HTML／纯文本和 bytes，并将序列化结果恢复为独立的 Document。

先运行 [button.xl](button.xl)，然后按“标签 → 输入 → 事件 → 布局”的顺序学习。每个文件都是独立程序，打开源码就能按顺序看到界面是如何组成的。

在发布目录 `release/win_x64` 中运行：

```powershell
./xl.exe run examples/xui/button.xl
./xl.exe run examples/xui/slider.xl
./xl.exe run examples/xui/basic.xl
```

关闭窗口或按 Escape 退出。Linux 在 `release/linux_x64` 中使用 `./xl run ...`，图形范例需要桌面和可用的图形后端。

## 第一个按钮

```xlang
import xui
from "./_window.xl" import Window

Window.show("按钮", func(xui.Context ui, xui.Widget root)
 auto button = ui.buttonNative()
 button.setText("点我")
 Window.place(root, button, 24, 24, 220, 44)

 button.setClick(func(xui.Widget sender)
  button.setText("你点击了按钮")
 end)
end)
```

这里没有示例类，也不需要写 `main()`：xlang 可以直接执行顶层代码。

- `import xui` 导入当前 XUI 的类型和函数声明，不需要手动加载 DLL。
- `_window.xl` 是同目录的普通 xlang 源文件，集中处理窗口、字体、输入、每帧绘制和退出清理。可以直接打开它阅读。
- `Window.show` 创建窗口，准备好图形后端后执行一次初始化回调；控件的创建代码不会每帧重跑。
- `ui` 是本窗口的 Context，用来创建并管理控件；`root` 是窗口中的根容器。
- `Window.place(parent, child, left, top, width, height)` 设置子控件的位置并加入父容器。坐标相对父容器，单位为像素。
- `setClick` 中的代码在每次点击时执行。修改 `setText` 的文字或 `place` 的数字，再次运行即可验证变化。

控件方法继续使用直接的原生声明。`buttonNative()` 等现有名称中的 `Native` 表示使用原生默认描述创建控件；辅助文件不包办按钮、表格等控件逻辑。

## 建议学习顺序

1. **按钮与文字**：`button`、`label`、`basic`。理解创建、布局和点击事件。
2. **输入与选择**：`input`、`checkbox`、`radio`、`radiogroup`、`slider`、`combobox`。直接操作控件，观察值和选中状态。
3. **容器与页面**：`widget`、`panel`、`tabs`、`scrollview`、`scrollframe`、`splitlayout`。先认识父子关系，再学习滚动和分隔布局。
4. **数据与资源**：`tableview`、`tablegrid`、`image`、`gui_surface_draw`、`richtext`、`documenteditor`。这些范例较长，包含数据回调或有意义的资源清理。
5. **组合组件**：`menu`、`menubar`、`msgbox`、`msgtip`、`toast`、`filedialog`。

`all_controls.xl` 是用于浏览控件的综合画廊，可双击 `run_xui_examples.bat` 启动。它由独立范例生成，包含场景切换和清理逻辑，适合看效果；学习时阅读对应的独立文件。

## 阅读较复杂的范例

`Window.showWithProxy` 额外传入绘图后端 `proxy`，供图片和手工绘图使用。普通控件范例用 `Window.show` 即可。

`c_int`、`c_uint`、`c_float` 是对应 C ABI 的数值类型。普通数值参数通常直接写 `24`、`0.5`；批量数组和回调返回值需要保持声明要求的元素类型或位宽，因此表格、级联选择等范例会出现这些类型。

`as Widget` 用于取得原生内部容器的非空视图，如 Accordion 的分区或 ScrollFrame 的视口。Context 统一管理控件；创建图片、文档、绘图会话的范例中，`try/finally` 则释放创建者持有的资源引用。图片或编辑器需要继续使用资源时，会保留自己的原生引用。

表格的适配器分别提供行数、读取单元格以及写回编辑结果。`Callback...` 参数是回调期视图；需要保存回调中的 C 字符串时，先调用 `copy()`。文件选择范例只显示路径，不读取或修改文件。

WebView 范例只加载源码中的本地 HTML。它需要启用 WebView2 的 Windows 后端；后端不可用时，界面显示实际错误码。这不代表网页渲染已经通过验收。

## 范例索引

每项的说明同时给出了运行后可以观察和操作的内容。

| 文件 | 可以观察或操作的内容 |
| --- | --- |
| [accordion.xl](accordion.xl) | 点击标题，展开或折叠其中的按钮和文字。 |
| [basic.xl](basic.xl) | 按钮修改下方文字；复选框和开关可直接点击。 |
| [breadcrumb.xl](breadcrumb.xl) | 依次显示 Home、Projects 和 xlang 三层路径。 |
| [button.xl](button.xl) | 界面：点击按钮，按钮上的文字变为“你点击了按钮”。 |
| [canvas.xl](canvas.xl) | 橙色矩形、绿色圆形；按住鼠标在画布中画线。 |
| [carousel.xl](carousel.xl) | 用左右箭头切换三张页面。 |
| [chart.xl](chart.xl) | 折线的四个点来自下方两个数据数组。 |
| [checkbox.xl](checkbox.xl) | 点击复选框，在三个选中状态之间切换。 |
| [codeedit.xl](codeedit.xl) | 编辑 C 代码，拖动右侧 minimap 滚动。 |
| [colorpicker.xl](colorpicker.xl) | 点击色块，打开 RGBA 颜色面板。 |
| [combobox.xl](combobox.xl) | 下拉选择 Debug、Release 或 Profile。 |
| [datepicker.xl](datepicker.xl) | 点击日期，展开日历选择日期。 |
| [dockpanel.xl](dockpanel.xl) | 拖动 Inspector 标题移动或停靠内容。 |
| [hyperlink.xl](hyperlink.xl) | Hyperlink / local click callback |
| [image.xl](image.xl) | 把一张橙色原生 Surface 放大显示为图片。 |
| [input.xl](input.xl) | 点击输入框，编辑或选择 UTF-8 文字。 |
| [listview.xl](listview.xl) | 点击行，改变列表的选中项。 |
| [menu.xl](menu.xl) | Menu / command selection |
| [messagelist.xl](messagelist.xl) | 两条聊天消息对应两个 addNode 调用。 |
| [numericinput.xl](numericinput.xl) | 输入数字，或用增减按钮调整数值。 |
| [page.xl](page.xl) | 点击页码，在多页之间切换。 |
| [panel.xl](panel.xl) | 带标题的面板中放置一个按钮。 |
| [popup.xl](popup.xl) | 点击按钮打开弹层；点击外部关闭。 |
| [progress.xl](progress.xl) | 点击按钮增加进度；超过 100% 后从 0 开始。 |
| [propertygrid.xl](propertygrid.xl) | 展开 General，编辑 Name 和 Mode。 |
| [qrcode.xl](qrcode.xl) | 二维码内容就是 setValue 传入的字符串。 |
| [radio.xl](radio.xl) | RadioGroup 管理互斥关系；两个 Radio 负责显示各自的文字和圆点。 |
| [rangeslider.xl](rangeslider.xl) | 拖动两个滑块，选择 20 到 80 的初始范围。 |
| [richtext.xl](richtext.xl) | 显示并选择文档中的文字。 |
| [scrollbar.xl](scrollbar.xl) | 拖动滚动条，观察滑块位置变化。 |
| [scrollview.xl](scrollview.xl) | 滚动查看超出窗口的十二行文字。 |
| [separator.xl](separator.xl) | 分隔线上下各放一行文字。 |
| [slider.xl](slider.xl) | 拖动滑块，调整 0 到 100 之间的数值。 |
| [splitlayout.xl](splitlayout.xl) | 拖动中间的分隔条，改变左右区域的宽度。 |
| [statusbar.xl](statusbar.xl) | 左侧文字和右侧进度条分别对应两个添加调用。 |
| [stepbar.xl](stepbar.xl) | 四个步骤来自一个字符串数组。 |
| [tablegrid.xl](tablegrid.xl) | 双击单元格编辑；回调把新文字写回 rows。 |
| [tableview.xl](tableview.xl) | 每个单元格从 rows 的对应行列读取文字。 |
| [tabs.xl](tabs.xl) | 切换 Overview 和 Settings，查看各页文字。 |
| [taginput.xl](taginput.xl) | 输入文字并回车添加标签；点击标签上的关闭按钮删除。 |
| [terminal.xl](terminal.xl) | 显示两行 ANSI 文本，不启动命令行子进程。 |
| [textedit.xl](textedit.xl) | 多行文字编辑，支持选择、撤销和重做。 |
| [timeline.xl](timeline.xl) | Animation 图层中有一个从第 5 帧到第 25 帧的片段。 |
| [toggle.xl](toggle.xl) | 点击开关，改变启用状态。 |
| [toolbar.xl](toolbar.xl) | 工具栏中依次放置 New、Build、分隔线和 Run。 |
| [treeview.xl](treeview.xl) | 展开 Project 和 Sources，选择 main.xl。 |
| [window.xl](window.xl) | 拖动子窗口标题，或调整其边缘；窗口内有一个按钮。 |
| [checkcard.xl](checkcard.xl) | 点击卡片选择或取消；卡片中的文字是普通 Label 子控件。 |
| [radiogroup.xl](radiogroup.xl) | RadioGroup 保证两个选项互斥；点击任一选项即可验证。 |
| [virtualjoystick.xl](virtualjoystick.xl) | 拖动摇杆，右侧实时显示 X、Y；回调视图只在回调期间读取。 |
| [inventorygrid.xl](inventorygrid.xl) | 创建十二个空槽；点击或用方向键切换，下面显示当前槽号。 |
| [scrollframe.xl](scrollframe.xl) | ScrollFrame 提供视口和滚动条；文字放在视口中，内容高度为 900。 |
| [menubar.xl](menubar.xl) | 点击“文件”展开菜单；选择项目后，下方显示命令编号。 |
| [cascader.xl](cascader.xl) | parent 是父项的数组下标，-1 表示根；点击选择“开发 / 编辑器”。 |
| [widget.xl](widget.xl) | Widget 是透明的布局节点；移动 group 的位置会一起移动它的子控件。 |
| [label.xl](label.xl) | 两个 Label 对应两行文字；Label 用于显示文字，不接收文本输入。 |
| [documenteditor.xl](documenteditor.xl) | 点击文档编辑文字；事务先创建初始段落，编辑器随后持有文档。 |
| [msgbox.xl](msgbox.xl) | 打开确认框；点击确定或取消，结果显示在窗口中。 |
| [msgtip.xl](msgtip.xl) | 点击按钮，短提示显示两秒后自行消失。 |
| [toast.xl](toast.xl) | 每次点击增加一条通知；通知可以堆叠，也可点击关闭。 |
| [filedialog.xl](filedialog.xl) | 只选择并显示路径，不打开、修改或删除所选文件。 |
| [webview.xl](webview.xl) | 只加载内存中的 HTML，不访问网站。需启用 WebView2 的 Windows 后端。 |
| [all_controls.xl](all_controls.xl) | Select a control in the gallery. |
| [gui_surface_draw.xl](gui_surface_draw.xl) | Draw to a Surface once, then display it through a normal Image widget. |

## 无窗口进阶范例

这些程序在终端打印结果，用来学习资源和数据模型，不创建图形窗口。

| 文件 | 预期输出或用途 |
| --- | --- |
| [headless_path.xl](headless_path.xl) | 路径命令数依次为 2 和 0 |
| [headless_cell.xl](headless_cell.xl) | 显示文字 Answer、提示及值 42 |
| [context_graph.xl](context_graph.xl) | 子控件数 1，以及按钮文字 |
| [manager_lifetime.xl](manager_lifetime.xl) | 消息框标题和正文；不打开弹窗 |
| [document_model.xl](document_model.xl) | 从文档快照读出 Hello, xlang |
| [document_ui_headless.xl](document_ui_headless.xl) | 只读状态 1，以及文档身份比较 true |
| [data_object.xl](data_object.xl) | 二进制长度 3，以及十六进制 610062 |

## 当前覆盖范围

共 71 个独立程序，其中综合画廊可浏览 57 个场景。IconPicker、FlowGraphWidget 和 WorkflowWidget 仍缺少可安全使用的数据模型声明，暂不提供空白控件范例；这三项的绑定缺口保留在覆盖清单中。

## 开发和验证

原来的断言、故障和生命周期检查保留在仓库的 `test/fixtures/xui_learning/`；教学代码不混入模拟点击和自动退出逻辑。`tools/check_xui_lessons.py` 从当前教学源码生成单独的交互验收探针；`tools/check_xui_gallery.py` 批量编译并有界执行。

`_window.xl` 保留 `XUI_EXAMPLE_FRAMES` 和 `XUI_EXAMPLE_DUMP`，供外部验收控制运行时长和读取画面；正常学习无需设置。默认字体由辅助文件加载，发布包已附带。可用 `XUI_EXAMPLE_FONT` 指定替代字体。

可选静态 SDK 的使用方法见运行目录的 `static/xge3/README.md`。维护综合画廊时运行 `python tools/update_xui_gallery.py`；独立范例是源码依据，综合文件由工具生成。

`shared_find.xl` demonstrates the native multi-document search scope without a
graphics window. Interactive find/replace uses the same scope in an initialized
window loop. System fonts are demonstrated in the xge package's `examples/system_font.xl`.
