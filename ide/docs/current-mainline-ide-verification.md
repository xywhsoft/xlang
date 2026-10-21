# 当前 DEMO6 源码副本的 IDE 集成验收

2026-10-12 完成新一轮独立构建与全部 14 项既有 IDE 集成检查。编译器来源是本轮捕获的 DEMO6 主目录，包含此前冻结副本之后的 15 处构建输入变化；原有 IDE 桥接集成的 11 个主线文件保持一致。首次核对观察到 14 处变化，实际冻结前又增加一处工具文件差异，以冻结清单的逐项比较为准。

结果证明本次冻结副本的实际编译器能够完成既有 IDE 查询、C/xlang 桥接与界面工作流。验收期间主目录继续发生变化，因此当前整个主目录验收状态保持 false，结论不扩展到尚未执行的新版本或所有编译器特性。

## 实际构建

本轮复制 1,755 份当前源码、资源和构建输入，另复制 3,959 份测试素材。随包扩展的 2,085 份运行资产保持原字节，单独记录为未重建。副本起初没有 `xl.exe`、`xl.dll`、`xlc.dll` 或已有编译对象。

在私有 Win32 桌面执行实际 `cmd /d /c build.bat`，完整构建退出 0，耗时 349.672 秒，生成 CLI、运行时 DLL 和编译器 DLL。构建阶段的原始主线输入与冻结输入均未发生变化。构建记录和三份新程序的摘要见 [compiler-build.json](../build/demo6-current-mainline-v1-20261011/compiler-build.json)，所有来源逐项记录于 [source-inputs.json](../build/demo6-current-mainline-v1-20261011/source-inputs.json)。

本轮没有修改主线源码或替换主目录生产二进制；IDE 默认编译器和当前便携候选也保持原样。

## 实际集成检查

14 项检查全部使用本轮构建出的 `source/release/xl.exe`，各自实际进程退出 0，没有将此前版本的成功计作本轮执行。

| 检查 | 本轮证据范围 |
| --- | --- |
| semantic_index | 实际语义索引 |
| completion_query | 实际补全查询 |
| source_ranges | 源码范围 |
| source_report | 来源报告 |
| resolution_report | 定义解析报告 |
| source_overlay | 未保存依赖覆盖层 |
| c_bridge | C 桥接查询 |
| c_bridge_map | 17 条实际命令、12 个响应、32,769 个容量探针导出，以及声明来源映射 |
| c_bridge_environment | 17 条实际命令、12 个响应和 2,655 项头文件对照 |
| actual-tcc-target | 实际 TCC 目标查询、执行及三个配置模式 |
| ide-extensions | 两个真实扩展、十条实际构建／执行／IDE 环境命令 |
| ide-aggregates | 23 类聚合、74 项实际尺寸／对齐／偏移测量 |
| ide-scalars | 既有实际原生标量与 IDE 桥接对照 |
| actual-native-ui | 17 项原生断言；CRC32/MD5 的 C 到 xlang 来源跳转、未保存文档与 ABI 保留，以及三张真实渲染图 |

各项命令、PID、原始日志、退出码、来源摘要和输入不变检查见 [ide-gates/evidence.json](../build/demo6-current-mainline-v1-20261011/ide-gates/evidence.json)。执行过程中使用独立桌面及自有进程句柄，未切换用户桌面。三张原生图已实际查看，C/xlang 高亮与既定停靠布局正常。

## 并发变化与结论边界

冻结后的两份集合测试在集成检查开始前已变化；检查期间，主目录 `src/sem/xl6_type_registry.c` 又将 `Queue` 的模块映射改为 `xlang.MpmcQueue`。全部测试仍执行原冻结字节。没有以当前文件覆盖旧副本，也没有将这三处新字节计作已验证。

本轮保存了冻结前后观察到的实际字节和摘要，见 [observed-root-changes-v1.json](../build/demo6-current-mainline-v1-20261011/observed-root-changes-v1.json)。这是并发变化的观察记录；不推定它们存在问题，也不据此宣称新 Queue 行为已验证。

本次补充 [DEMO6 主线集成](demo6-mainline-integration.md) 的版本证据，没有重新执行整个 IDE 回归或重新打包。当前开发候选仍由 [CURRENT-CANDIDATE.md](../CURRENT-CANDIDATE.md) 指向；实体输入法、混合显示器 DPI、干净 Windows 环境和正式发行材料要求保持原有状态。首版交付层级的澄清问题仍待用户答复，长期目标保持 active。

最终证据闭合脚本初次误将首次观察的 14 处变化作为冻结后的固定数量，实际退出 1；该脚本、失败记录和修正前文档保留在本轮 `close-first-failure/`。之后按两份冻结清单的实际差异修正为 15，所有编译与集成检查均使用原先实际执行的成功结果，未覆盖或重跑它们。
