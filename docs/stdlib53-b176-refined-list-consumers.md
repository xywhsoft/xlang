# 类型细化后的容器键与 List 动态调用

`var` 经容器类型判断后的循环，现在保留物理存储与逻辑容器两份信息。
已知 List 的键是完整 int64，不再要求动态键；Array 使用顺序索引，
Dict 使用精确长度的字符串键。原生 List 的动态 remove、take、clear、
contains 也进入驻留 checked 分发。这关闭了这组消费者缺口，但语言
List 工厂、索引、转换和恢复仍未整体切换。

## 循环键合同

| 逻辑来源 | 物理存储 | 键 | 键生命周期 |
| --- | --- | --- | --- |
| 未细化 var | var | 动态 Value | 每次迭代独立释放 |
| 已细化 Array | var | int64 顺序索引 | 普通标量替换 |
| 已细化 List | var | 完整有符号 int64 键 | 普通标量替换 |
| 已细化 Dict | var | 精确长度 string | 每次迭代独立释放 |

Dict 单目标循环按语言语义遍历键，双目标循环遍历键和值。丢弃目标
不创建投影。Set 的静态细化只接受值遍历，开放 var 仍保持原有动态
键值遍历合同；本次没有给 Set 增加新语言能力。

XLIR 为细化键携带逻辑 payloadType，并证明它与实际 checked 游标
创建相同：游标来自只有一次初始化的编译器私有局部存储，创建与
投影必须采用同一逻辑家族。只伪造一个同 C ABI 的家族标记不能通过
验证。游标的物理参数仍是 var，不改写共享容器的身份。Cgen 使用
原有统一投影操作，不追加源 AST 搜索、临时装箱或容器类型猜测。

## List 动态方法

动态分发先识别真实驻留 List，再以 Sequence 的私有协议识别 Array。
HANDLE 只是物理载体，不代表 Array。外来 handle 的 opaque 指针不会
被解引用，Stack、游标和未绑定投影不能冒充相应的语言容器。

List 支持 remove(key)、take(key)、clear、contains。缺键分别返回
false 或正常 null；操作失败则使 enclosing checked scope 失败，
即使错误对象分配失败也不能把它当成缺键。Array 专属方法及双参数
范围删除在 List 上被拒绝，不改变 List 内容。旧 wire IntMap 路径
作为显式迁移边界保留，不能据此宣称源工厂已经迁移。

## 验收入口

`check_stdlib_list_refined.py --root <frozen-demo6> --public-directory
<fresh-build> --out <fresh-output>` 检查确定性 C、严格 O0/O2、真实
TCC JIT、实际生成函数的原生 List 输入和独立内存账本。原生输入
运行完整分配前缀，以及快照、Advance 和四类方法的无诊断状态失败，
每批运行到首个未命中点。每次调用都要求 finally 恰好执行一次。

IR 负对照包括同物理 ABI 的家族伪造；C 与头文件输出前均须拒绝。
实际驻留实现的负对照先编译成功，再由行为合同拒绝。纯 Array
细化键值循环的实际 artifact plan 必须排除全部 List 专属组件。

这些是源码驱动验收，不认证发布 DLL/SO、VFS、打包 AOT 或三组全部
模块。本专项也不代表整个编译器或全部生成程序已经 sanitizer 化。

## 已验证结果

Windows 重新编译 259 个翻译单元，Linux 重新编译 258 个。候选源码
分别冻结于 `E:/xlang-validation/stdlib53-b176-source-dev5/frozen/demo6`
与 `/var/tmp/xlang-stdlib53-b176-source-dev5/demo6`，不使用旧编译器。
真实 TCC 引擎作为已记录的供应商依赖保留，编译器和 XRT 源码重新构建。

双端各通过 33 阶段细化专项、64 阶段迭代、64 阶段存量循环和
84 阶段方法回归，以及包含闭合分配前缀的 96 阶段恢复回归。细化
专项拒绝 34 项损坏 IR 和一项已编译的错误
实现；O0/O2 每个优化级分别运行 25 次原生分配尝试和 14 次无诊断
失败尝试，每次独立关闭内存账本，直到首个未命中点。

完整驻留审计双端均通过 220 组件、1,202 精确导出和 13 项负对照。
纯 Array 程序的实际 plan 不包含 List 专属组件。Linux 迭代专项的
ASan/UBSan 仅覆盖选定原生闭包，不扩大为编译器或发布产物认证。

专项证据为 `E:/xlang-validation/stdlib53-b176-refined-win-final3/results.json`
和 `/var/tmp/xlang-stdlib53-b176-refined-linux-final3/results.json`。
两项测试专用修正使用显式 native-host 入口和当前 runner，均记录
实际输入哈希；它们没有替换冻结候选的编译器或运行时源码。失败
的前两次专项记录保留，不计入通过阶段。

文档索引与 HEAD 基线比较仍有 14 项历史拒绝，本批新增为零；这不是
整个文档门已通过的声明。

## 后续迁移边界

List 的源工厂、索引、类型准入、转换、合并及恢复继续按统一载体
合同联动推进。开放 var 的通用 `in` 操作与此次动态成员 contains
是不同入口；后续 [动态索引与查询](stdlib53-b177-list-index-consumers.md)
已接通其原生 List 路径，并独立验证纯 Array/Dict/Set 的实际裁剪产物。
Set/Dict 和三组发布总验收仍未完成。

共享默认子对象保持已批准的恢复规则：按声明 clone 能力隔离，否则
checked 拒绝，不修改外部共享原对象。CRT 无未决策，19 个 XRT
未决策组和对象池最后不变，见 [标准库决策清单](标准库决策清单.md)。
