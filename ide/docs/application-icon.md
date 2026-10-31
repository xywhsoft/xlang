# 应用图标

IDE 使用用户指定的 `D:\GIT\xserver\res\xx_exe.ico`。原文件未修改，复制为 `data/xlang_ide.ico`；SHA-256 为 `ac6e594e8f009733760afb658b1ee5536dc0df6828d40b17e9098189486743d9`，来源见 `data/icon-source.json`。

`data/xlang_ide.rc` 将图标嵌入 GUI 和控制台 EXE，资源 ID 与原生窗口代码共享 `src/platform/app_resources.h`。启动时从当前 EXE 资源加载大小图标，并应用到 XGE 窗口，供标题栏和任务栏使用。选择尺寸时使用窗口 DPI，旧系统退回系统图标尺寸。共享图标句柄随进程存续，无需磁盘 ICO 文件。

构建需要 GCC 和 `windres`，图标、RC 及共享声明都在本仓库中，不依赖 xserver 的路径。正常本机构建仍输出根 `release/`，资源对象位于 `ide/build/Release/xlang_ide.res.o`，相关输入进入构建摘要守卫。

Windows 资源管理器可能缓存旧 EXE 图标；文件本身和新启动的 IDE 使用新嵌入图标。
