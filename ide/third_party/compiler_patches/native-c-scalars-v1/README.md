# 私有编译器 C 标量目标查询

此补丁叠加在 `native-file-paths-v6` 的固定编译器源码上，修改 `lib/tcc/libtcc.h`、`lib/tcc/libtcc.c` 和 `src/api/xl_api.c`。`changes.json` 绑定原种子清单、三份前后字节摘要、精确替换范围和根构建脚本。`before` / `after` 保存实际修改的源码。

TCC 的私有 `tcc_xlang_scalar_target` 返回代码生成器使用的 `PTR_SIZE`、`LONG_SIZE`、`LDOUBLE_SIZE`、`LDOUBLE_ALIGN`，以及按 `tcc_predefs` 相同方式编码的版本。它只在 Windows x86-64 目标上提供数据，不使用宿主 C 编译器的 `sizeof(long double)`。

同次 `c-bridge --json --c-environment` 查询的 `nativeEnvironment.scalarTarget` 是可选扩展，不改变环境 schema 1。其范围是 `tcc-win64-scalar-target`，不是完整的预定义宏、预处理输入图、结构体规则或函数调用 ABI。原有 `cPreprocessorGraphComplete=false`、`abiEquivalenceGuaranteed=false` 保留。

源码准备工具为 `tools/prepare_native_scalar_compiler.py`。指定固定 v6 种子、其 `source-inputs-native-project-v6.json` 和新的输出目录；准备成功后在新目录运行 `build.bat`。工具只复制已核对的输入，生成 `built=false` 的准备记录，不把准备操作算作编译。根仓库 `demo6` 和旧候选不被修改。
