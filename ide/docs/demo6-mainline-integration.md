# DEMO6 主源码集成与 IDE 桥接验收

2026-10-11，本轮将 IDE 私有编译器已有的原生 C 查询和声明来源修正同步到当前 DEMO6 主源码，并补齐 DLL 构建清单。独立 DLL／CLI 冷构建和 14 项实际编译器／IDE 检查通过。当前 IDE 开发候选继续提供 C／xlang 双语法及定版布局，入口见 [当前候选](../CURRENT-CANDIDATE.md)。

## 主源码改动

- `src/api/xl_api.c` 与 `lib/tcc/libtcc.{h,c}`：环境查询附带实际 TCC 标量目标和配置状态的位域模式，保持现有运行时、集合和所有权逻辑。
- `src/cgen/xl6_ir_cgen_identity_closure.c` 与 `xl6_ir_cgen_header_emit.c`：真实 extern 原型的标识符发射点提供实际 lowered symbol，供来源映射连接 checked 声明；普通 Cgen 沿用相同输出路径。
- `res/tcc/win64/x86_64/include/stdio.h`：`vsnprintf` 的外部声明与静态内联定义按原有模式互斥，真实原生扩展可使用函数指针和格式化函数。
- `tools/{runtime,compiler}_dll.rsp.in`：补齐 7 个已存在的 SlotMap、原生叶值和数组原生值实现源文件，修复接口已注册而 DLL 缺少实现的链接失败。
- [声明映射检查](../../tools/check_c_bridge_map.py)、[TCC 目标检查](../../tools/check_native_c_target.py)与[编译器接口文档](../../docs/IDE原生C声明桥接接口.md)：新增实际 extern／未保存覆盖身份、头文件字节对照和 TCC 内存编译执行检查。

## 已完成的实际检查

独立源码快照包含 1,755 份源码／资源／构建输入，3,953 份兼容测试素材另列。2085 份扩展运行文件沿用已校验资产，本轮没有重建它们。无预置编译器及生成对象的 `build.bat` 生成 `xl.exe`、`xl.dll`、`xlc.dll`，实际退出 0，用时 338.985 秒；冻结构建输入未变。

| 检查 | 结果 |
| --- | --- |
| 9 项编译器兼容检查 | 语义索引、补全、范围、来源、解析、覆盖、桥接、声明映射、内嵌环境均实际退出 0 |
| 声明映射 | 17 次实际命令、12 份验证响应；extern 磁盘／未保存身份与普通头一致；32,769 个导出触发可选映射容量回退，ABI 头继续可用 |
| 内嵌环境 | 17 次实际命令、12 份响应、2,655 次原始头字节／长度／摘要对照 |
| TCC 实际状态 | 版本、指针、long、long double 大小／对齐与执行结果一致；默认、MS 位域及后续覆盖三种模式均通过 |
| IDE 原生扩展 | CRC32／MD5 共 10 次实际命令，混编执行、stdio、诊断、悬停、补全、定义及间接头变化撤回通过 |
| IDE ABI 对照 | 23 类聚合的 74 项对照、原有 37 项标量／宏／身份对照通过 |
| 实际原生界面 | 17 条断言，C／xlang、CRC32／MD5 F12、未保存 ABI 和原文保留；三张明暗主题截图已查看 |

前 7 项编译器检查来自同一冻结编译器的首轮成功记录；修正声明映射检查的输出文件名后，继续其余 7 项。没有把首次失败的整体验收计作通过。IDE 应用的 774 份严格构建输入重新核对一致，因此本轮没有重复应用完整回归或重组便携包。

## 证据与适用范围

- [DLL／CLI 实际构建](../build/demo6-native-sync-v2-878e23e182/compiler-build.json)
- [14 项组合验收](../build/demo6-native-sync-gates-v5-35cd169ee8/evidence.json)
- [实际 TCC 查询与执行](../build/demo6-native-sync-gates-v5-35cd169ee8/native-c-target/evidence.json)
- [首次 DLL 链接失败](../build/demo6-native-sync-v1-7b779c4dba/compiler-build.json)
- [首次声明映射整体失败](../build/demo6-native-sync-gates-v4-0ac15855f7/evidence.json)
- [测试修正前后字节](../build/demo6-query-test-fix-v1-dd59fdbdb2/evidence.json)
- [本轮最终核验](../build/demo6-mainline-integration-final-v1.json)

以上构建验证的是明确冻结的 DEMO6 源码版本。构建及测试期间，主源码另有 SlotMap／IR 相关并行改动，差异单独记录并保留；本轮没有回退这些改动，也没有据此宣告整个最新 DEMO6 已验收。本轮同步文件的当前字节另行核对。

DEMO6 正式发布二进制、IDE 默认私有编译器和当前便携包保持原状态。新冻结编译器仅用于本轮源码集成验证。`cPreprocessorGraphComplete` 和 `abiEquivalenceGuaranteed` 继续为 false，指定实际对照不构成任意 C 预处理或 ABI 等价证明。

定版计划的相关编译器改动进入 DEMO6 源码与文档这一要求已补齐。正式首版仍有干净 Windows、实体输入法／混合 DPI 和发行材料核对等验收事项；完整调试器按既定后续独立阶段处理，长期目标继续。
