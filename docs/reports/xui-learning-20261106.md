# XUI 教学范例整理与残影修复

独立范例改为顶层窗口初始化回调，按创建、文字／数据、布局、事件的顺序展示。移除 Example 类、重复标题和测试断言，使用具体控件名称，补充操作结果说明。按钮、菜单、超链接和滑块在界面中反馈操作结果。公共的 `_window.xl` 管理字体、输入、绘制和退出清理；控件方法仍直接使用原生声明。

当前提供 71 个公共程序，比之前新增 15 个。综合画廊从独立范例生成，包含 57 个场景；README 给出学习顺序、逐行解释、运行方式和逐项预期结果。原来的八项资源／生命周期验证保留到 `test/fixtures/xui_learning/`，交互断言由 `tools/check_xui_lessons.py` 生成到外部验收目录。

## 补充和修正

- 增加 CheckCard、RadioGroup、VirtualJoystick、InventoryGrid、ScrollFrame、MenuBar、Cascader、Widget、Label、可见 DocumentEditor、MsgBox、MsgTip、Toast、FileDialog 和 WebView 范例。
- 增补五项直接声明合同：RadioGroup／ScrollFrame 的空描述默认构造、WebView 的尺寸化安全描述构造、Cascader 的列式数据输入和 DockPanel 的停靠输出。不增加逐方法 C 转发函数。
- Radio 明确通过 RadioGroup 建立互斥关系，并使用该控件专属的方向常量。
- 修正 C 浮点回调值转文字时的显式语言 `float` 转换，实际验证鼠标滑动和摇杆状态回调。
- 补足 Chart／PropertyGrid 的字体、DockPanel 窗口的停靠显示、Image 的缩放模式、Scrollbar 的方向和完整显示模式，并分开分隔线与文字。
- 窗口标题在原生窗口就绪后设置。

## 残影原因和修复

`xge.clear` 原本已逐帧清理主窗口，但 XUI 先绘制到一个持续复用的离屏 Surface。该 Surface 原本只在创建／缩放时清理。XUI 的合成函数不会擦除背景，删除旧场景后，透明区域中仍保留旧像素。

公共循环现在每帧清理离屏 Surface，并标记全视口合成区域后调用 XUI render。原生 `xuiInvalidateAll` 只重置 damage 列表并加入视口，不使控件位图缓存失效，因此仍复用控件缓存。代价是一次目标面清屏和全视口合成；未引入逐控件重新栅格化。

回归先绘制红色画布，再删除它并创建较小按钮。旧循环中采样点仍为 `(204,68,51,255)`；新循环恢复为背景 `(244,246,250,255)`。永久检查还要求新按钮区域确有非背景像素，防止“全擦掉但没重画”的假通过。

## 验收范围

| 范围 | 结果 |
| --- | --- |
| Windows 公共程序 | 首轮 71 项构建／执行；三个生成问题统一修复后，13 个受影响程序重新构建／执行通过，合并覆盖全部 71 项 |
| 综合画廊 | 动态、静态均实际切换全部 57 个场景；最终发布路径 BAT／JIT 再次通过 |
| 交互 | 按钮点击、单选互斥、滑块实际鼠标输入、摇杆浮点回调、级联数据边界、消息框／提示／通知及文件选择取消均有独立探针 |
| 画面 | 原始帧读回，确认文字、停靠、缩放、滚动方向及残影修复 |
| Linux | 71 项语义检查、九项代表性 AOT 构建和七项无窗口执行；未运行 Linux 图形窗口 |
| 发布副本 | Windows／Linux 普通范例、两个动态包及 Windows 静态 SDK 五处同步，保留未枚举的用户文件 |

全量初查、修复复测、失败探针及其修正、清屏负例和最终通过记录分别保留，不覆盖旧报告。最终仅清除综合生成文件空行中的缩进，先前验收用的精确源文件也保存在证据包内。共同窗口循环最终修正另有安装路径执行和像素回归验证。

## 明确的边界

IconPicker、FlowGraphWidget、WorkflowWidget 的安全数据模型绑定仍不足，本轮没有以空界面冒充完整教学范例。InventoryGrid 当前展示空槽和选择；其完整物品数据模型不在此范例中。WebView 的实际初始化结果为 `XUI_ERROR_UNSUPPORTED (-7)`，已验证界面能说明该限制，未声称 HTML 渲染可用。文件选择探针只打开并取消，不读取或修改所选文件；未开展网络或安全性质的测试。

材料索引：`dev/release-materials/sources/xui-learning-20261106/manifest.json`、`dev/release-materials/provenance/xui-learning-20261106/receipt.json`。此前画廊的报告及原生宿主／编译器验收材料保持原字节。
