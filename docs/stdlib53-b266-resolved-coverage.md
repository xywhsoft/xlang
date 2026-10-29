# 标准库重载绑定与数组算法产品验收

2026-10-14。三组标准库所选 22 个命名空间的实际登记共 224 条，解析其
声明体得到 228 个参数签名。Windows 和 Linux 各扫描 303 份夹具，
全部签名均找到真实语义分析和统一 IR 解析后的源调用绑定。没有新增
公开功能或改动生产实现。静态绑定覆盖不代表所有运行时分支已执行；
完整类方法清单、动态派发及运行时函数覆盖继续验收。

## 注册签名与源调用

新门 `tools/check_stdlib_resolved_coverage.py` 使用已有完整核心构建，
重新核对所选源码和构建输入。测试宿主直接读取实际编译的注册表，
解析每份声明体，而不是从 C 字符串或文档中的函数名估计 API 数量。
一个 `pattern.compile` 登记包含多种声明，因此签名数量大于登记数量。

调用侧读取 Lowered IR 的 CallSite、目标符号、声明及完整参数类型。
函数值参数保留参数和返回签名，不将两种 signal 回调都归为 `function`。
只有原始夹具节点中的调用绑定参与登记覆盖，注入的标准库助手调用
不会替代夹具直接使用。声明存在、注释和字符串中出现名字也不计数。

范围为 time、env、os.env、signal、process、number、text、string、
unicode、html、encoding、pattern、regex、queue、array、list、buffer、
map、dict、set、orderedMap、slotMap。class、基础类型内建方法和动态
派发不在这 228 个登记签名的分母中，不能据此宣布 53 模块全函数覆盖。

## 泛型与反例

原有三组文件筛选未包含 `stdlib_crt_algorithms.xl`。新门明确纳入这份
夹具，检查 array.sort、sortBy、binarySearch 的真实具体实例。泛型
语义分析使用独立的实例符号；源模板绑定必须与具体 IR 参数向量一起
匹配，T/K 绑定必须一致，不能只按函数名推断覆盖。此次保留整数和
字符串实例，不把代表性实例称为所有合法类型组合的测试。

八项匹配控制检查重复类型参数、回调返回值、参数个数和两种 signal
回调的区别。独立语言反例只调用 `encoding.isUtf8(string)`，断言 bytes
重载未被统计；未调用函数声明、注释及字符串中的调用文字也未被统计。
既有不允许复制的 typed 元素负例保持语义拒绝。

## 实际产品执行

数组算法夹具另使用上一批新建的真实 compiler 产品完成 Windows 和
Linux 的 run、module 及独立 CLI AOT 构建和执行，共八阶段。夹具涵盖
整数、字符串、重复值、空数组、排序键调用次数及比较器／键函数抛错。
执行宿主仅使用公开 compiler API，编译环境没有原生供应方替换。

本批生产源码未变，产品来源保持
[当前 IR 产品重建与三组标准库批量验收](stdlib53-b264-current-products.md)
的同一构建。静态扫描使用
[工作线程完成与分配失败清理验收](stdlib53-b265-worker-completion.md)
的 Windows 327 TU／Linux 326 TU 完整核心，不以单个覆盖宿主或补编
提供方代替完整编译器。

证据目录如下，初期工具调试的失败记录另外保留，不改写为通过。

- Windows：`E:/xlang-validation/stdlib53-b266-bindings-win-final2/results.json`。
- Linux：`/var/tmp/xlang-stdlib53-b266-bindings-linux-final2/results.json`。
- 实际数组算法产品：`E:/xlang-validation/stdlib53-b266-dev/algorithms-runtime.json`。
- 最终提交快照：`E:/xlang-validation/stdlib53-b266-source-final1/frozen/demo6`。

复现静态门使用 `--root`、`--public-directory`、全新 `--out`；Windows
另以 `--runtime-library` 指定真实当前 runtime 中的 TCC 核心。门记录
所有编译、控制和逐夹具扫描日志，以及输入／产物哈希和未覆盖签名。
即使静态签名全部出现，FunctionCoverageAcceptance 仍为 false。

后续验收继续补 class 方法、getter/setter、内建与动态调用的完整清单
及真实执行证明，同时继续跨线程／对象图 OOM、生成模块组合和发布
总门。CRT 无未决策，XRT 19 组、WaitQueue MPMC 和对象池最后不变。
