# XUI 入门范例

[flow_canvas.xl](flow_canvas.xl) 和 [workflow_canvas.xl](workflow_canvas.xl)
使用直接声明的默认构造器创建画布，右键后在标签中显示坐标。原生模型由画布拥有，
不需要裸指针或手工销毁。运行命令分别为 `xl run extlibs/xui/examples/flow_canvas.xl`
和 `xl run extlibs/xui/examples/workflow_canvas.xl`；其余准备同本页的窗口范例。
这两个入门例仅展示画布创建与事件；独立模型、节点编辑和模型长期别名仍在继续接入。

不创建窗口的数据模型范例见 [document_exports.xl](document_exports.xl)：加载 Markdown，
导出 HTML／纯文本和 bytes，并将序列化结果恢复为独立的 Document。

[document_streaming.xl](document_streaming.xl) 演示分块预备、续接、解析和发布；
需要包含 `continueStreamSource` 的匹配源包。它不创建窗口，不隐式启动工作线程。

[document_queries.xl](document_queries.xl) 演示从文档快照查找文字，
并检查当前内容转成 CommonMark 时会丢失的标记。

[document_selection_geometry.xl](document_selection_geometry.xl) 在不打开窗口的情况下
计算并输出一段跨行选区的矩形。

[dock_queries.xl](dock_queries.xl) 不创建图形窗口，演示两个窗口的窗格拆分，
以及使用局部坐标查询拖放目标。

[terminal_search.xl](terminal_search.xl) 向终端写入两行文本，然后输出两个搜索命中的行号和显示列。

[table_columns.xl](table_columns.xl) 设置数值列的类型、对齐和宽度范围，然后输出原生表格限制后的宽度。

[inventory_snapshot.xl](inventory_snapshot.xl) 在不打开窗口的情况下导出物品栏数据，关闭控件后仍能读取快照。

[combobox_items.xl](combobox_items.xl) 使用业务值、禁用状态和分隔线配置下拉选项，并读取当前选项。

[tree_nodes.xl](tree_nodes.xl) 批量设置父节点和子节点，展开父节点后读取子节点的名称与深度。

[code_assist.xl](code_assist.xl) 读取编辑器的补全位置并打开签名帮助。

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
| [document_node_attributes.xl](document_node_attributes.xl) | 插入带字体、字号和语言的文字节点，再从快照读出这些属性 |
| [document_ui_headless.xl](document_ui_headless.xl) | 只读状态 1，以及文档身份比较 true |
| [data_object.xl](data_object.xl) | 二进制长度 3，以及十六进制 610062 |
| [code_document.xl](code_document.xl) | 创建文本模型、合并两次编辑为一个撤销步骤，并观察变更回调、撤销和重做。 |
| [code_models.xl](code_models.xl) | 选中 xlang 一词，输出字节范围 6—11；设置关键字样式，输出颜色已保存 true。 |
| [code_editing.xl](code_editing.xl) | 替换选中的 world、缩进、撤销并切换行注释；使用原生选区和撤销组。 |
| [workflow_callbacks.xl](workflow_callbacks.xl) | 根据配置添加预览端口，并打印端口名称。 |

## 当前覆盖范围

不创建窗口的 [edit_search.xl](edit_search.xl) 演示 CodeEdit 查找、替换、读取结果和
“没有匹配”的状态检查；直接使用 XGE 文档模型，不需要外部文件或服务。

共 88 个独立入口文件，其中现有综合画廊可浏览 57 个场景。图标专题入口包括
`icon_picker.xl`（分类与格式化）、`icon_memory.xl`（内存 SVG）、
`icon_custom.xl`（回调期 Painter 自绘）和 `icon_file.xl`（SVG／PNG 文件与批量更新）。
文件范例附带两份小型 `assets/icon_file.*` 资源；保留 assets 目录，公共窗口助手会
从范例目录、发布根目录或仓库根目录定位它们。新增接口需要本轮匹配 SDK；最终
正式发布树仍待统一交付。FlowGraphWidget 和 WorkflowWidget 仍缺少可安全使用的
完整数据模型声明，相关缺口保留在覆盖清单中。

`asset_names.xl` 不创建窗口或 Context，直接列出内建图集资产和矢量图标名称；
读取结果为独立文本快照，索引越界返回 null。

## 开发和验证

原来的断言、故障和生命周期检查保留在仓库的 `test/fixtures/xui_learning/`；教学代码不混入模拟点击和自动退出逻辑。`tools/check_xui_lessons.py` 从当前教学源码生成单独的交互验收探针；`tools/check_xui_gallery.py` 批量编译并有界执行。

`_window.xl` 保留 `XUI_EXAMPLE_FRAMES` 和 `XUI_EXAMPLE_DUMP`，供外部验收控制运行时长和读取画面；正常学习无需设置。默认字体由辅助文件加载，发布包已附带。可用 `XUI_EXAMPLE_FONT` 指定替代字体。

可选静态 SDK 的使用方法见运行目录的 `static/xge3/README.md`。维护综合画廊时运行 `python tools/update_xui_gallery.py`；独立范例是源码依据，综合文件由工具生成。

`shared_find.xl` demonstrates the native multi-document search scope without a
graphics window. Interactive find/replace uses the same scope in an initialized
window loop. System fonts are demonstrated in the xge package's `examples/system_font.xl`.

`localization.xl` 在独立窗口创建 Context 持有的语言包，把“复制”译文改为
“自定义复制”，并显示实际查询结果。关闭窗口时 Context 回收语言包；没有
额外的库加载、裸指针、手工引用计数或语言包析构。它不加入综合画廊，避免
修改其他场景共享的 Context 语言。CPU 数据示例可以校验返回值；GUI 教学代码
继续把模拟输入与验收断言放在独立探针中。

`geometry_queries.xl` 读取滑块的轨道、滑块尺寸和圆角，并把图表数据坐标转换为
像素后再还原。图表显式使用数值横轴，坐标只在界面中保留一位小数；底层仍以
原生 float／double 精度计算。更多控件的尺寸、滚动状态读取见
`docs/xui-geometry-queries.md`，教学源码不混入故障注入和生命周期测试。

`text_layout.xl` 创建一份独立文本布局，读取行数、尺寸、每行字节范围，并把
同一份布局绘制到 Surface 后交给 Image 显示。布局与 Surface 的局部拥有引用
在初始化结束时关闭；Image 保留其 Surface，后续帧无需重新排版。字体和图形
初始化仍由共享 Window 处理。这里的字节范围不是 Unicode 字符索引。

`code_document.xl` 不创建窗口，在终端展示合并编辑“你好, xlang! 欢迎！”、
撤销回“Hello, xlang!”和重新应用。变更回调只在调用期间读取文档，输出的是
独立文本快照；`finally` 关闭模型。无需外部资源、文件写入或网络服务。

`code_folding.xl` 在终端演示一个数值折叠范围：标题行保留可见，内部行折叠，
然后将可见行号映射回原始行号。预期输出 `Visible lines: 3` 和
`Row 2 maps to line: 5`。所有行号从 0 开始；不创建窗口、Context、文档或
字体，也不需要文件／服务。`finally` 关闭独立折叠模型。

`code_replace.xl` 不创建窗口，先把所有 `red` 替换为 `green` 并撤销，再用正则
把 `item 12, item 34` 转成 `#12, #34`。预期输出 `Literal replacements: 2`
和 `#12, #34`；每一步都能从源码和输出核对，`finally` 关闭独立文档。

`code_tokens.xl` 用一个关键词的 UTF-8 字节范围创建带版本的词法缓存，打印
`Cached tokens: 1` 和 `Version and byte range: 1 0 4`。不创建窗口或文档；
缓存复制输入记录，`finally` 释放独立模型。它不演示尚未公开的批量 token 读取。

`code_annotations.xl` 不创建窗口，先标记两处 `todo`，再用平行列发布一条诊断。
预期输出 `Marked matches: 2` 和 `W001 : Complete this task ; source: lesson`。
随后用 `getDiagnostic(0)` 读回范围、级别、编号、说明、来源和用户 ID，断言数值、
打印文本；它们直接对应输入的六列。返回文本是独立快照，最后分别关闭模型。

`input_events.xl` demonstrates a shared command for a button and Ctrl+F2. It
inserts a Unicode heart through the input-result API and updates a status label.

`document_source_patches.xl` 不创建窗口，以 UTF-8 字节范围替换 `world` 和 `Hello`，
一次发布后输出 `你好, xlang!`。首次预备采用具名的起点、终点、文本三列，续接
继承首次配置；范例展示源码编辑，不是对可视节点或字符编号的编辑。

`document_source_index.xl` 演示取消一次 Markdown 源码索引构建，然后用同一快照
重新构建并读取索引信息。

`control_items.xl` 在一个窗口中批量创建面包屑、菜单栏和状态栏条目。同一下标的
列组成同一条目；状态栏用具名参数标明宽度、分区和进度范围。菜单由同一 Context
管理；显式关闭被引用菜单时，原生菜单栏会清除弱引用并禁用对应项。

`document_metadata.xl` 不创建窗口，从 Markdown 快照读取链接和节点文本信息。
关闭模型后仍可使用返回的字符串；预期输出 `Document metadata: independent
text snapshot passed`。示例中的网址仅是文档数据，不会发起网络请求。

`dock_state.xl` 创建编辑区和属性两个窗口，用 `var` 保存布局、隐藏属性窗口、
再载入保存的布局。最终窗口数为 2，浮动窗口数为 0。布局值可以独立于 Dock
存活；`loadState` 遵循原生载入语义，不承诺原生操作失败后的布局整体回滚。

`page.xl` 显示 20 页中的第 3 页，并把四个导航文字设为首页、末页、上一页和
下一页。初始化时用一次 `getText()` 核对四个结果；查询不放在逐帧绘制路径中。
# Search result value lesson

`search_value.xl` shows independent capture-array storage in a FindResult value.
Run `xl run search_value.xl`; assertions verify the original range and the
output displays the original/copied end offsets. It needs no window, backend,
or actual search operation. CodeSearchResult has the same ten-slot native
capture capacity. Batch layout/source records are descriptors; this lesson
does not imply that the batch transaction API is integrated.

`webview.xl` 在调用 `initializeAsync()` 之前安装拥有事件回调。Ready 事件中
载入内存 HTML 并查询缩放比例；初始化失败或未启用 Windows WebView2 后端时，
显示错误码。它不再逐帧轮询浏览器状态。关闭控件会解除回调；回调捕获的对象
遵循受管拥有图。通用 CPU 回调验收不等于 WebView2 浏览器后端已通过验收。
