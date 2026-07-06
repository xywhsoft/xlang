# demo6/release 构建产物目录

本目录保存 DEMO6 构建和验证生成的产物，不作为源码或设计真相来源。

## 常见产物

- `xl.exe`：DEMO6 CLI。
- `xl.dll`：嵌入式运行库 DLL。
- `xlc.dll`：带编译能力的嵌入式运行库 DLL。
- `xl.def`：由 `build_dll.bat` 生成的 DLL import definition。
- `include`：随 `xl.exe` 分发的公开 C API 头文件目录。
- `examples`：随 `xl.exe` 分发的范例程序。
- `extlibs`：随 `xl.exe` 分发的 import/package 扩展库资产。
- `xl_dll_embed_string.exe`、`xl_dll_null_context.exe`、`xl_dll_threaded_host.exe`：DLL API 验证程序。

## 维护规则

- 不手工修改构建产物来修复问题；应修改源码、构建脚本或工具链配置后重新构建。
- 如果产物和源码行为不一致，以源码、测试和构建脚本为准。
- 发布目录不保存临时产物；构建中间文件进入 `build/tmp`，测试与范例回归产物进入 `build/test`。
- `release/libtcc.dll` 不属于发布物；`xl.dll` / `xlc.dll` 均静态链接 demo6 自带 libtcc。
