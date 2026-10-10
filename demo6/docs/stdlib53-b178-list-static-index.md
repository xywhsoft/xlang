# List 静态索引的原生合同

静态 List 索引读写现在使用明确的 XLIR 驻留操作，能够处理真实原生
List，不再生成旧 IntMap 读取辅助函数。源 List 工厂仍是显式保留的
IntMap 迁移生产者；静态索引完成不等于整个 List 载体迁移完成。

## 读取与写入语义

int64、uint64、bool、float64 和非 nullable string 使用直接读取入口。
XLIR 提供精确 owner、schema 和 element 身份，原生实现校验完整物理
布局。标量直接复制槽值，不装箱、不复制整表，也不分配元数据；
分配器禁止分配时的 int64 读取用例通过。

string 返回独立拥有的双字段值，保留 UTF-8 和内嵌 NUL。char、bytes、
nullable 和嵌套受管理元素通过独立拥有的 Value 投影与精确重建读取。
普通 nullable/var 缺键得到正常 null，非 nullable 缺键得到 RangeError，
安全索引允许 null receiver 或缺键。nullable null 不等于空字符串。

索引赋值、数值和文本复合赋值以及 `?=` 统一使用消费输入的写入入口。
所有结果下输入恰好消费一次；false 或失败 scope 即使没有诊断对象
也会失败。IntMap 迁移分支先保留 receiver 和旧子对象，提交后离开
BUSY 区域再退役旧值；清理错误不能覆盖首次失败。这里不将 wire
退役的可重入性质扩展为原生 List 的任意重入承诺。

## XLIR 与驻留依赖

新增九项闭合操作，分别描述三种拥有 Value 读取、五种直接读取及
消费写入。结果所有权、异常边界、元素域和参数槽位由 lowering 与
verifier 固定。文本读取使用 NULL_RESULT 错误 ABI，以便失败路径
正确关闭待提交的拥有值；标量读取和写入使用 FOREIGN_SCOPE。

Cgen 的同步与 continuation 路径共用入口选择，只渲染已验证操作，
不追溯源 AST、不内联 List 算法。标量读取、文本读取和写入各有
独立驻留组件。实际静态 List artifact 不依赖通用开放索引组件，
原生 scalar/string 入口也不需要运行时 schema 拷贝。

## 双端验收

冻结生产源码分别位于
`E:/xlang-validation/stdlib53-b178-source-dev4/frozen/demo6` 和
`/var/tmp/xlang-stdlib53-b178-source-dev4`。Windows 从源码重编译 266
个翻译单元，Linux 重编译 265 个；仅保留记录过的供应商 TCC 引擎。

双端各通过以下阶段：

| 专项 | 通过阶段 |
| --- | ---: |
| 静态 List 索引、真实原生输入及裁剪产物运行 | 141 |
| 开放 List 索引与 Array/Dict/Set 查询裁剪 | 166 |
| 存量拥有值索引及 continuation | 27 |
| 细化键合同 | 33 |
| List 快照迭代 | 64 |
| 存量容器循环 | 64 |
| List 方法及 continuation | 75 加 9 |
| Closed/Open 恢复及完整分配失败前缀 | 96 |

静态专项包含 26 条闭合故障前缀、96 次尝试，逐点检查所有权账本
及适用的 finally。50 项损坏 IR 在输出 C 和声明头之前被拒绝。
三个真实驻留负对照分别编译，再由相同原生合同拒绝。生成 C 两次
一致；真实 JIT、O0/O2 语言宿主、按 artifact 声明闭包独立链接的
产物均运行通过。MemoryDebug 是测试扩展，不是生产组件根。

完整驻留目录双端通过 227 个组件、1,214 个精确导出及 13 个负对照。
Linux ASan/UBSan 仅覆盖选定原生闭包，不认证整个编译器或全部生成
程序。当前正式报告为
`E:/xlang-validation/stdlib53-b178-static-index-win-final6/results.json` 和
`/var/tmp/xlang-stdlib53-b178-static-index-linux-final6/results.json`。
test-root 的宿主、合同及 runner 修正均记录实际哈希；冻结生产源码
未被替换。早期失败记录不计入通过阶段。

文档门仍有 14 项历史拒绝，本批新增为零，不是全仓文档门通过。

## 剩余载体与发布工作

静态索引批次之后，已知 List 的浅复制与代数已由
[List 浅复制与代数操作](stdlib53-b179-list-copy-algebra.md) 关闭。
List 源工厂、类型准入、cast、开放复制、递归 clone 和恢复还需联动切换；
Set/Dict 迁移、发布 DLL/SO、VFS、打包 AOT 和三组总验收仍未完成。
本批不关闭长期任务，也不宣称发布级验收通过。

JSON/XLON 共享默认子对象仅按声明 clone 能力隔离，否则 checked
拒绝，不修改外部原对象。CRT 未决策为零，19 个 XRT 未决策组及
对象池最后的安排保持不变，见 [标准库决策清单](标准库决策清单.md)。
