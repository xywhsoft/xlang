# 驻留组件导出与依赖验收

本批修复上一批完整驻留导出审计的 60 项差异。Windows 和 Linux
各自编译全部 215 个驻留组件，1,155 个实际导出与精确组件登记一致。
这是组件目录、依赖闭包和所列产物的验收，不是完整标准库或发布库封板。

## 符号归属与公开边界

56 个既有内部辅助函数补入 artifact 符号归属表，覆盖代码租约、Sequence
身份、递归准入、整图恢复和投影、私有 List、Tree、Queue 及同步资源。
每项归属来自实际定义所在的驻留翻译单元，不以名字相似推测实现。

这些登记不添加标准库声明，不添加 TCC callable，也不自动保留所属组件。
只有已经解析的 XLIR 链接目标实际引用该符号时，组件才成为产物根。
运行时函数体、组件依赖关系和 Cgen 模板没有修改。

四项 `xlSyncRetirement*CheckedV1` 宿主清理接口原本已有实现和准确登记。
旧检查器只收集 `xl6` 名字，错误报告其缺失。本批测试符号扫描同时覆盖
`xlSync`，以及 BSS 和弱定义；生产选择仍然使用精确符号表，扫描不参与
编译器语义或组件选择。

## 跨平台目录门

`tools/check_xlir_resident_catalog.py` 是 Windows 产物门和 Linux 源码门
共用的实际目标文件 oracle。它验证完整组件人口、唯一声明、唯一实际
定义、已知归属、声明与定义一致、依赖闭包和跨组件未解析符号。
独立原生合同还验证 46,225 对组件根组合和精确符号查找。

13 个负控基于实际通过的完整目录构造：移除声明、错误归属、重复声明、
未知组件、缺失定义、重复定义、构建失败、缺失组件、移除实际依赖，
以及分别移除四项真实宿主清理导出，全部拒绝。不增加豁免清单，也不
通过扩大组件闭包掩盖错误。

## 已完成验收

最终源码快照为 Windows
`E:/xlang-validation/stdlib53-b167-source-final2/frozen/demo6` 和 Linux
`/var/tmp/xlang-stdlib53-b167-source-final2/demo6`。完整编译器源构建分别
为 254、253 个翻译单元，位于 `stdlib53-b167-build-*-final2`。
仅复用真实 TCC 工具核心，不能外推为当前发布 DLL/SO 或嵌入 VFS 重建。

Windows `stdlib53-b167-artifact-win-final2/results.json` 和内嵌
`catalog-audit.json` 均通过；Linux 完整目录报告为
`/var/tmp/xlang-stdlib53-b167-catalog-linux-final2.json`。两端各有
215 个实际驻留对象、1,155 个导出、13 个拒绝负控和完整依赖闭包。

Windows 三个 JSON、XLON、Array JSON 恢复裁剪用例通过 Cgen、真实
JIT、AOT 构建和独立执行。选定原生目标文件没有漏符号，实际组件
程序集与计划一致，不带入另一格式、List 或 Future。三个生成 C 文件
分别为 25,924、25,961、45,490 字节，与上一批逐字节一致。

双端 `stdlib53-b167-language-*-final2/results.json` 各通过 59 阶段，
包含五个恢复用例的确定性 IR/C、严格生成 C O0/O2、真实 JIT、平衡
分配账本，以及目标消费/opaque 模块屏障的输出前拒绝。
本批没有重新执行完整语言 OOM 前缀或 sanitizer；既有证据保留在上一批，
不拼接为本批新的运行结果。

双端 `stdlib53-b167-plans-*-final2/results.json` 各通过 48 阶段，
完整 17 用例矩阵及字段拒绝检查、O0/O2 确定性 IR、变异输出前拒绝和
恢复六表失败原子重建通过。这是 IR 合同门，不是 17 用例全都重新执行 JIT。

同步组件邻接回归报告为 `stdlib53-b167-sync-*-final2/sync.artifact.json`。
Windows 64、Linux 38 阶段均通过，真实 TCC 供应者和选定 XRT 内核
逐族保留/裁剪正确，完整 runtime 作为负控。Windows 另执行独立 DLL
和退休后卸载；Linux 只证明实际原生选择，不宣称独立 ELF 执行。
所有本批通过报告均为 Complete=true、InputsUnchanged=true。

最终索引证明逐项复核实际目标文件及报告输入。哈希大小写仅按十六进制
表示归一化，不修改文件字节；本批索引检查器的测试侧修改单独记录，
编译源码没有后置替换。

## 后续边界

补齐本任务 34 份专项的文档身份及缺失入口。工作区文档索引门通过；
隔离暂存索引仍有八份既有扩展库/范例文档的 README/状态矩阵缺项，
相关修订属于未纳入的工作区内容。本批不宣称全仓文档总门已通过。

外部共享声明默认子对象仍只按声明 clone 隔离，否则 checked 拒绝，
隔离结果重新认证后才整图提交。封闭恢复已切换；带旧 opaque
DynamicFields 策略的模块仍明确保留旧路径，不能据此宣称它们已经安全迁移。
Open、List、Set、Dict、通用载体、未知资源策略、性能和发布总门继续。

上一批 Complete=false 的完整导出报告保留为历史证据，不能改写为通过。
canonical XRT 没有修改，无关 IDE、扩展库和源码覆盖层改动不纳入提交。
CRT 无未决策，19 个 XRT 未决策组保持不变，对象池最后，长期目标仍进行中。
