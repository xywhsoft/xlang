# xlang IDE 工程组织

当前 IDE 位于 `D:\GIT\x-lang\ide`。

第一阶段只实现最小可运行骨架，用于验证 XGE + XUI 技术路线，并为后续迁移旧 IDE 功能预留稳定边界。

## 目录

- `src/app`：应用生命周期、XGE/XUI 初始化、输入转发、主循环。
- `src/ui`：主窗口、菜单栏、工具栏、状态栏、DockPanel 默认布局。
- `src/views`：各 Dock 面板的最小实现。
- `src/lang`：xlang CodeEdit 语言注册和最小词法高亮。
- `release`：本地构建产物，包含 `xlang_ide.exe` 和运行所需的 `xge.dll`。

## 依赖口径

- 运行 DLL 固定从 `D:\GIT\auto_test_ide\build\xge.dll` 复制。
- 字体文件复制到可执行文件同目录的 `res` 目录，例如 `release\res\Consolas-with-Yahei_Nerd_Font.ttf`。
- 字体不做资源同步；当前只是一次性复制，IDE 运行时优先从 `exe\res` 查找。
- 字体配置位于 `release\option\ide.json`，路径可以是绝对路径，也可以是相对于 exe 目录的相对路径。
- UI 字体和代码编辑器字体默认都使用 `Consolas-with-Yahei_Nerd_Font.ttf` 的 18px 版本。
- 编译头文件暂时使用 `D:\GIT\xge\xui.h` 和 `D:\GIT\xge\xge.h`。
- `D:\GIT\xge` 目录当前可能处于修改状态，因此不直接使用其中的运行 DLL。

如果后续稳定 DLL 与当前头文件 ABI 不一致，应以 `auto_test_ide` 对应的稳定 XGE/XUI 版本补齐头文件或生成独立 SDK，而不是切换到不稳定 DLL。
