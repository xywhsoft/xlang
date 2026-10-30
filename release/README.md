# xlang 统一运行与发布目录

xlang 工具链版本为 **0.6.0**，版本代号 **`0x00000600`**，C 嵌入 ABI 为 1。当前仍有发布阻塞，见 [0.6.0 发布前检查](../docs/prerelease-0.6.0.md)；本轮工具链检查不签收并行开发中的 IDE 产品。

IDE、语言服务、工具和 xlang 引擎统一放在仓库根目录的 `release/`，不再使用 `ide/release/`。编译器源码已从 `demo6/` 移到仓库根目录。

直接运行 `xlang_ide.exe`。IDE 默认使用同目录的 `xl.exe`，共享 `xl.dll`、`xlc.dll`、`include/`、`extlibs/` 和范例；C 语言分析工具位于 `toolchains/clangd/`，不再复制一套 `toolchains/xlang/`。`data/` 保存命令帮助和字体，`option/` 保存可迁移的 IDE 设置和会话。

`ide/build_current.bat` 的隔离输出位于 `ide/build/cb-*/release/`。`ide/build.bat` 普通构建更新本目录；带 `-Tests` 的构建和 `ide/test.bat` 使用 `ide/build/<配置>/release/`，避免混入测试程序。显式 `-ReleaseDirectory` 可选择其他构建输出位置。详见 [统一发布目录](../ide/docs/unified-release.md)。

## 常见产物

- `xlang_ide.exe`、`xlang_ide_console.exe`：IDE 图形入口和控制台入口。
- `xlang-ls.exe`：xlang 语言服务。
- `xge.dll`：IDE 使用的 XGE / XUI 运行库。
- `xl.exe`：xlang CLI。
- `xl.dll`：嵌入式运行库 DLL。
- `xlc.dll`：带编译能力的嵌入式运行库 DLL。
- `xl.def`：由 `build_dll.bat` 生成的 DLL import definition。
- `include`：随 `xl.exe` 分发的公开 C API 头文件目录。
- `examples`：随 `xl.exe` 分发的范例程序。
- `extlibs`：随 `xl.exe` 分发的 import/package 扩展库资产。
- `xl_dll_embed_string.exe`、`xl_dll_null_context.exe`、`xl_dll_threaded_host.exe`：DLL API 验证程序。
- `xl`：Linux CLI。
- `libxl.so`：Linux 嵌入式运行库。
- `libxlc.so`：Linux 编译版嵌入式运行库。

## 维护规则

- 不手工修改构建产物来修复问题；应修改源码、构建脚本或工具链配置后重新构建。
- 如果产物和源码行为不一致，以源码、测试和构建脚本为准。
- 发布目录不保存临时产物；构建中间文件进入 `build/tmp`，测试与范例回归产物进入 `build/test`。
- `release/libtcc.dll` 不属于发布物；`xl.dll` / `xlc.dll` 均静态链接仓库自带 libtcc。
