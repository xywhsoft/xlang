# 私有编译器 C 标量目标查询 v2

此补丁叠加在 `native-file-paths-v6` 的固定编译器源码上，修改 `lib/tcc/libtcc.h`、`lib/tcc/libtcc.c` 和 `src/api/xl_api.c`。`changes.json` 绑定原种子清单、三份前后字节摘要、精确替换范围和十一份根构建脚本。`before` / `after` 保存实际修改的源码。

私有 `tcc_xlang_scalar_target` 使用 TCC 的目标常量查询指针和 long 大小，按 `tcc_predefs` 相同方式编码版本。long double 遵循 `parse_btype` 在 `TCC_USING_DOUBLE_FOR_LDOUBLE` 下的有效类型映射，再调用实际 `type_size` 获取大小与对齐，不使用宿主 C 的 sizeof 或原始 x87 后端常量。接口仅在 Windows x86-64 目标上提供数据。

同次 `c-bridge --json --c-environment` 的 `nativeEnvironment.scalarTarget` 是可选扩展，不改变环境 schema 1。范围 `tcc-win64-scalar-target` 不是完整预定义宏、预处理输入图、结构体规则或调用 ABI。`cPreprocessorGraphComplete=false`、`abiEquivalenceGuaranteed=false` 保留。

准备工具 `tools/prepare_native_scalar_compiler.py` 核对固定 v6 种子及其 `source-inputs-native-project-v6.json`，只复制核验过的输入并写入 `built=false` 的准备记录；在新的输出目录运行 `build.bat` 才是编译。根仓库 demo6、v1 失败副本和旧候选保留。v1 的原始 LDOUBLE_SIZE 元数据与真实执行不符，已由实际对照拒绝；不把 v1 算作完成的功能。
