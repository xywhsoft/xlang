# 小键盘 Enter 修复验收

确认是 XGE 的输入转换遗漏。Windows Sokol 将扩展 Enter 扫描码 `0x11c` 映射为键码 335；XGE 保留了该键码，但 XGE→XUI 只识别普通 Enter 257，所以按下和释放被丢弃。

修复在 XGE 共享转换层：新增 `XGE_KEY_KP_ENTER=335`，两种 Enter 均映射为 `XUI_KEY_ENTER`。游戏可继续区分物理按键，UI 控件统一执行 Enter 行为。没有添加包装层或额外分配。XGE 提交 `faf1a089c0496c44b6f6a4a7c09e63180edb97c4`。

旧 DLL 的同一专项中，257 能收到 down/up，335 完全收不到；新 DLL 通过 62 项检查，覆盖按下／释放、修饰键、输入框提交、编辑器换行、重复、只读与原键消费。既有输入、事件队列、查找生命周期也通过。IDE 新增 C／xlang 双键换行与小键盘补全确认回归，连同查找、消息框、设置和弹窗性能共 5 项均通过。

已用 -O2 完整重编 108 个 GUI 编译单元，安装到 `release/win_x64` 并在真实发布 DLL 上再次运行小键盘专项。Windows／Linux 各 7 项 Cgen、JIT、范例、静态构建、导入与移位执行检查均通过；两平台动态／静态包、配套 SDK、命令目录一起更新。新增公开条目仅 `xge.XGE_KEY_KP_ENTER`，其他条目仅源码行号变化。

本轮是事件链注入和真实控件验证，没有冒称物理键盘实测，也未跑全仓测试。编译器、语言服务器和用户配置保持原样。原始日志及输入哈希见 `receipt.json` 和 `evidence.zip`。源码采用上轮不可变完整闭包加本轮增量，见 `dev/release-materials/sources/xge-keypad-enter-20261108/manifest.json`。
