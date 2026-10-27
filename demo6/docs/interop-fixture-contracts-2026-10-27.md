# C 互操作夹具合同校正

本批只修正验收夹具，不修改编译器、运行时或标准库 API。

- `xlir_pkg_valid` 的 `increment(byref count)` 仍使用已经删除的参数修饰语法。改为 C 函数按值接收并返回结果，xlang 显式执行 `count = increment(count)`；原有多 C 文件、导出函数与计数断言均保留。
- `xlir_artifact_native_linkage` 将 `xrtEncodingUnitSize(xencoding)` 的参数声明为 `c_int`。当前 GCC/TCC 目标的该枚举使用无符号整数 ABI，改为 `c_uint`；保留原有四个数值断言、未调用的 Math 声明和相似名称字符串。

验证使用独立源码快照，完整重新编译 1,295 个编译单元，全部禁用对象缓存。在这套源编译器上，两份夹具各通过 emit、JIT、AOT 构建及执行，共 8 项。TinyCC 的独立 C 声明对照也确认：有符号重声明拒绝，无符号重声明通过。

证据位于 `E:/xlang-validation/five-groups-20261027/p7-shipping/fixture-contracts-r34/results.json`；编译器构建证据为同级 `source-win-r27-closure.exit.json`。本批结果不替代三个完整包验收工作器，也不表示 74 项总门禁或五组长期任务已经完成。
