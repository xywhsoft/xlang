# 当前 IR 产品重建与三组标准库批量验收

2026-10-14。当前统一 IR 的 Windows CLI、两份 DLL 和 Linux CLI、
两份共享库已重新构建，TCC、XRT 及两份内嵌 VFS 均来自本轮源码。
三组标准库的 302 项清单完成真实产品扫描，双端各 294 项通过
run/module、1 项按预期拒绝；六项专用宿主由产品子门完整验证。
平台专属覆盖、完整函数覆盖、worker/graph OOM 与发布总门继续。
本轮候选位于隔离验证目录，不覆盖仓库旧发布文件。

## 原生 Array 与路径构建修复

实际产品扫描揭露 `path.join` 的旧桥接仍要求 wire Array，而语言
Array 已采用原生载体。路径片段函数迁到独立驻留提供方，复用
ArrayInput 的完整取得／回滚／首错清理事务，接受认证的原生 Array
和显式 wire Array，保留严格字符串、精确长度、NUL 拒绝及 style 范围。
取得的元素独立拥有，原生操作 pin 不跨出输入事务。

Cgen 只输出提供方头和调用，不再生成这段遍历／分配算法。组件目录
登记实际源文件、ArrayInput 与错误边界依赖；共享库、原生符号目录、
独立产品和内嵌 VFS 都提供同一实现。没有恢复旧载体、放宽名义身份
或在调用方添加临时转换来掩盖不匹配。

新增语言用例覆盖 native 与 typed 字符串片段、变参 join、Unicode、
非字符串、内嵌 NUL、非法 style 和源内容不变。实际两份库的 O0/O2
宿主均通过完整 4 个 OOM 预算，其中 3 次真实失败；诊断分配失败仍
是 checked 失败，输出为空、输入和内存保持正确。路径用例还通过
真实 CLI 的独立 AOT。引号用例接收实际 CLI 路径，两端验证带空格
路径的 shell 调用，不再把硬编码的 Windows 文件名当作产品覆盖要求。

## 产品矩阵

父门记录 Windows 607 行、Linux 608 行，含夹具执行、准备和子门调度。
这些行不与子门内部步骤混加。子门独立结果如下。

| 子门 | Windows | Linux | 范围 |
| --- | --- | --- | --- |
| 私有 VFS 头 | 202 个 | 202 个 | 两个实际资源映像及 sibling 引用闭合 |
| 嵌套编译与 AOT | 32 阶段 | 32 阶段 | 真实 CLI/VFS、compiler import 与独立消费 |
| 图及 continuation | 279 阶段 | 279 阶段 | 204 模式，实际 Stack/Tree/Queue/Promise |
| clear 错误 | 30 阶段 | 30 阶段 | 原有 RangeError/finally/async/generator 断言 |
| Thread 产品 | 67 阶段 | 70 阶段 | 两库 VFS、CLI 与独立产品，限定裁剪检查 |
| Sync checked 恢复 | 26 阶段 | 26 阶段 | 持锁最后引用、宿主恢复与真实产品 |
| 独立 Sync ELF | 不适用 | 28 阶段 | 四个独立消费程序及非可执行栈 |

夹具宿主只使用公开 compiler API，生成程序在无源码／头／TCC 分发
目录的隔离位置使用产品内嵌资源；原生 GCC 宿主编译使用明确的声明头。
没有原生供应方或 lookup 替换。独立失败集中收集后统一修复、统一复测。
首次语言失败与错误的子报告路径记录保留，最终报告不改写失败证据。

## 来源与剩余验收

正式候选构建为 `stdlib53-b264-build-win3` 与
`/var/tmp/xlang-stdlib53-b264-build-linux4`，产品执行为
`E:/xlang-validation/stdlib53-b264-products-win-final1` 与
`/var/tmp/xlang-stdlib53-b264-products-linux-final1`。认证将编译输入、
实际资源和所有执行产物逐项与最终选择性 index 快照比较。

Windows 的 IANA DST 夹具仍需独立原生 transition 验证；Linux 的
旧 cmd/findstr/more 夹具不适用，已有 portable process 用例实际执行。
整个公开函数面的覆盖证明、worker/graph OOM、生成模块组合与发布
提升仍待完成，不由本轮批量功能通过推断完成。

已有工作区构建脚本重写保持原样。提交只为 HEAD 构建路径添加新
提供方，工作区未提交的响应模板同步添加该项，不纳入其余模板内容。
CRT 无未决策，XRT 19 组不变；WaitQueue MPMC 和对象池最后不变。
