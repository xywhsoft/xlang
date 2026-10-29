# 参数化 worker 的绝对输出路径：2026-10-26

后续终态：原冻结发现批次已收集 73 项、11 通过／62 失败；尚有 string_join
另一类路径写法，不能把本批十份修正说成所有路径问题已关闭。见
[完整首轮结论](parameterized-first-sweep-2026-10-26.md)。

完整冻结场景暴露多个工具前置问题。本批集中修复其中一种：10 个 worker
无条件执行 `Join-Path root Directory`，把已经是绝对路径的 `Directory`
再拼接一次。在 PS5 下产生非法路径，部分 UNC 形式则静默指向错误位置。
总门合法的跨盘独立输出因此尚未进入语言／拥有权合同就失败。

## 修复范围

仅改变下列脚本第一次 `$outputPath` 的计算：绝对路径直接规范化，相对路径
继续以原 root／工作目录解释。没有改变输出根、断言、源／对象清单、编译器
选项、并行度、生命周期合同或故障注入范围。

| worker 后缀 | 原路径基准 |
| --- | --- |
| aggregate_strict_source、callable_contextual、future_aggregate_ownership、future_ownership、generator_public_source、terminal_source_runtime | 当前工作目录 |
| capture_ownership、frame_ownership、generator_ownership | 原 rootPath，即当前工作目录 |
| code_finalizer | 原 rootPath，即脚本所在仓库根 |

原拒绝已存在输出目录的保护保留。独立非参数化脚本 `check_xlir_c_owned.ps1`
不在本次改动内，不借此扩张到全部工具重构。

新增聚焦回归守卫，只从这 10 份实际脚本的 AST 提取并计算第一次路径赋值；
不执行后续 worker，不访问 UNC，也不创建目录。生产总门在源码构建前运行
它，结果单独保存在 `output-paths.json`，不增加第 75 个必需参数 worker。

## 实际验证

PS5／PS7 各覆盖 70 项：10 个脚本 ×（三个相对路径、同盘／跨盘／UNC
绝对路径和无效 NUL 拒绝），包含空格和 Unicode。当前路径计算全部通过；
相同守卫读取原冻结脚本时，两端各准确发现 30 项绝对路径错误。

另在不含任何编译器／运行时源或 provider 的隔离副本，调用真实生产总门：
当前 driver 角色先通过，随后旧路径计算被拒绝；`PhaseReports` 为空、构建
目录不存在，实际退出 1。两端各三种场景，共六个真实进程的退出码、日志、
报告及输入哈希均已核对，没有靠替代 provider 获得通过。

证据：`F:/xlang-validation/five-groups-20261026/p7-parameterized/`，
`verified-worker-paths-r1.json` 与 `path-accept-r1/`。

## 不代表什么

这是路径赋值和总门拒绝次序的实际验证，不是十个完整 worker 的通过证明。
运行中的完整冻结批次保持旧输入，其他长命令行、驻留头／源清单、旧 IR
断言等失败继续保留并分类修复；不通过放宽断言消除失败。
完整 74-worker、package 依赖分支、最终联合／嵌套门尚未通过，长期目标
继续 active。未决策 CRT 0／XRT 11，内存池最后，无网络／安全性质专项。
