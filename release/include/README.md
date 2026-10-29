# release/include 对外头文件

本目录保存 DEMO6 对 C 宿主暴露的公共头文件。

## 文件

- `xl.h`：`xl.dll` 嵌入式运行库的公开 C API。宿主程序可以通过它创建上下文、设置工作目录、运行脚本字符串/脚本文件、编译脚本、注册 C 函数和加载依赖库。

## 维护规则

- 只放稳定公开 API，不放编译器内部结构。
- 修改 `xl.h` / `xlc.h` 时必须同步检查 `DLL嵌入式运行库设计.md`、`release/examples/xl_dll_*.c`、`build_dll.bat` 和 `test_dll.bat`。
- DLL 生命周期命名使用 `xlRuntimeInit` / `xlRuntimeUnit`；生成代码生命周期使用 `xlInit` / `xlUnit`。
- 公开 API 应保持 C 友好，避免把 xlang 内部 AST、语义状态或调试 dump 接口暴露到 DLL 精简接口中。
