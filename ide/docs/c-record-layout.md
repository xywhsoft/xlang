# C 聚合与位域的原生布局默认值

2026-10-11 当前扩展库交付：SDK67，84 模块／106 产物重新严格构建，含渲染的完整回归和十项实际便携门通过。默认编译器修正 stdio 外部／内联声明冲突，并为真实扩展函数 C 原型提供可验证的 xlang 来源映射。CRC32、MD5 的实际混编、stdio 格式化、诊断、悬停、补全、F12 和间接头变化撤回已通过开发与包内检查；最终构建的 17 项原生界面断言和三张截图、九项编译器兼容、41 项源码材料及原有 74 项聚合／37 项标量对照通过。 详见[扩展库分析与导航](c-native-extensions.md)与[当前候选](../CURRENT-CANDIDATE.md)。下文保留此前阶段的数量和验收记录。

当前私有编译器在 C/xlang 自动声明桥接中提供可选的 `nativeEnvironment.recordLayout`。IDE 用同次查询的布局默认值配置 C 分析，避免 Windows GNU 目标默认采用 MS 位域规则，而实际 TCC 使用 GCC/PCC 位域规则时产生错误诊断。独立 C 文件继续使用其自身分析配置；缺少字段的旧编译器保留原有行为。

## 实际差异与修正

原始对照实际构建并运行 C/xlang 混合程序，再将得到的尺寸、对齐和成员偏移写成静态断言，通过真实 IDE 桥接交给 clangd 解析。`BitsInt` 在 TCC 中大小为 4、尾成员偏移为 2，原默认 Clang 配置得到 8 和 4；混合整数位域得到 8/4 与 20/16。失败诊断按文档和通知序号分别保留，避免另一份文档的空诊断覆盖原始差异。

修正来自实际配置的 TCC 状态，而非硬编码 IDE 宿主 GCC 的布局。私有 `tcc_xlang_record_layout` 查询读取 `TCCState.ms_bitfields`，仅在已支持的 Windows x86-64 目标返回结果。编译器建立与原生 C 环境对应的状态，应用原有配置与源访问，再读取并销毁状态；该查询不编译用户 C 文件。公开编译器头文件及既有 DLL 导出项保持一致；两份 DLL 各新增该私有 TCC 查询导出。

字段约定为：

```json
{"schema":1,"scope":"tcc-win64-bitfield-default","msBitfields":false}
```

IDE 严格检查 schema、scope、布尔类型及已验证的标量目标。`false` 添加 `-mno-ms-bitfields`，`true` 添加 `-mms-bitfields`。默认值排在原有用户分析参数之前，保留后者的覆盖顺序；无效元数据阻止整批桥接替换原命令。选项定义见 [Clang 参数文档](https://clang.llvm.org/docs/ClangCommandLineReference.html#cmdoption-clang-mms-bitfields)。

## 验证范围

`tools/check_c_native_aggregates.py` 使用实际编译器构建和执行混合工程，再调用同一构建的 `c_bridge_test`、真实 C 服务及未保存的 xlang 声明。覆盖 23 个结构体/联合体中的普通、嵌套、匿名、数组、柔性数组、long double、packed/aligned、嵌套 pack 及恢复、混合位域、零宽位域、宽位域和有符号位域；共 74 项真实测量与语义断言。标准回归入口通过 `-NativeAggregateCompilerPath` 注册此门，并要求实际 C 服务和旧编译器回退路径。

另在独立测试进程中调用新 DLL 的 TCC 接口，真实编译、重定位并执行位域程序：默认状态返回 false、实际大小/尾偏移 4/2；`-mms-bitfields` 返回 true、实际值 8/4；随后追加 `-mno-ms-bitfields` 恢复 false 和 4/2。空状态及空结果指针被拒绝。该测试证明状态查询和三个选项序列，不代表 IDE 工程模型已经支持全部原生编译选项组合。

`c_record_layout_test` 验证可选字段、无效元数据及实际编译命令的用户参数优先级。标量的 37 项真实测量、九项编译器兼容性、整套 IDE 回归与随包默认工具链检查另行记录。所有检查仅在本次拥有的进程/私有桌面执行，没有切换用户桌面。

## 来源与边界

私有补丁位于 [native-c-record-layout-v1](../third_party/compiler_patches/native-c-record-layout-v1/README.md)。完整编译器构建使用 1,532 份冻结输入，包括 1,521 份源码/资源及 11 个根构建脚本；DLL/CLI 实际构建退出 0，输入在构建期间保持一致。随包编译器源码材料绑定这次实际构建的输入、日志和三个运行产物。扩展库沿用上一候选的 2,085 个固定文件，本轮没有重新编译这些扩展。

本修正只对齐有限目标、布局默认值及上述真实场景。其他属性、打包组合、调用约定、跨 DLL 聚合传参、完整预处理图和扩展库不能据此宣告全部等价；`cPreprocessorGraphComplete=false`、`abiEquivalenceGuaranteed=false` 保持明确。首版完整验收与当前可启动版本以 [当前候选](../CURRENT-CANDIDATE.md) 为准，长期任务继续。

## 本次完整交付记录

2026-10-11：SDK67，84 模块／105 产物重新严格构建，含渲染完整回归及八项实际便携门通过。默认 C/xlang 桥接新增实际 TCC 位域布局查询；开发与随包提供者各通过 23 类聚合、74 项尺寸／对齐／偏移对照，22 项元数据和用户参数优先级检查、三个实际 TCC 状态执行、九项编译器兼容检查及 41 项源码材料检查通过。原有双语法、定版布局、三层管理、编辑与构建运行保持可用，C 输入观察继续启用。 含渲染完整回归实际退出 0，耗时 994.7 秒；便携包与回归绑定同一精确构建。原生截图核对范围包含 C 悬停／定义及新鲜、搬迁后的 C/xlang 界面。

- [最终核验](../build/c-record-delivery-final-v1.json)
- [完整回归](../build/c-record-full-v2-b0c6f819/evidence.json)
- [便携验收](../build/c-record-package-native-v1.json)
- [三个 TCC 状态的实际执行](../build/c-record-state-api-v2-de92117a/cases/evidence.json)
- [源码材料及九项编译器兼容检查](../build/c-record-materials-adoption-v1.json)
- [保留的失败与修正背景](../build/c_record_failure_notes_v1.json)

之前的全量前置门因新验证工具读取 JSON 层级错误而退出，随后修正工具、重新严格构建并重跑完整回归。首次元数据测试和缺少测试输入的兼容检查原样保留，未替换它们的失败状态。上一 C 输入观察候选保持不变。
