# B148 List 动态边界与稀疏数据导入

日期：2026-10-09。属于已批准的容器接入任务。本批完成 List 的私有
驻留投影、动态读写和显式数据导入合同；语言载体切换、递归图转换及
真实产品验收仍待完成，没有新增 xlang 公开 API。

## 原生边界

`xl6ListEraseV1` 保留同一个 shell 和实际 typed List 存储，不复制容器，
也不通过 wire 值模拟动态对象。投影绑定一次后不可替换，Clone 保留该
绑定并复制独立 typed 存储。只有驻留、无 Context 且接口完整的 codec
符合私有 ABI；引用槽固定使用已有的 retained-reference codec，不保留
生成模块的代码或上下文。Cursor、未绑定 owner、错误身份和未知 handle
均不能作为动态 List 使用。

动态 Read 将 checked 成功与 found 分开，缺项正常返回；输出是初始
NULL 的编译器私有拥有槽，不引入语言 byref。Take 先完成编码、精确
身份绑定和准入，再删除原键。Set/Append 在解码前检查 Value 域，解码
回调后检查实际槽域。原生输入准入、NUL 字节长度、char、nullable、bytes
和实际 Future 控制块身份不由 Cgen 重新猜测。

暂借的 native codec adapter 为每次回调安装 checked 边界。回调即使
返回 true 或已分配的 Value，只要报告失败就不会提交槽位或发布输出。
首错诊断为 NULL 时保留 failed 位，并只向 payload-only XRT 边界提供
次级诊断。重入拒绝后清除诊断也不能清除失败；回调释放最后语言引用时，
operation pin 保证当前槽和元数据直到调用结束仍存活。

消耗型 ProjectionTake/ImportTake 在入口错误边界未能安装时，仍先锁定
失败再释放输入。未发布结果的清理同样保留首错，包括没有诊断对象的
失败，不让后续 finalizer 的 ValueError 覆盖 MemoryError 或 NULL 首错。

## 显式稀疏数据转换

`xl6ListToWireV1` 创建独立的一层 IntMap 快照，原生 iterator 按键遍历
O(n)，不会逐项再做键查找。完整 int64 稀疏键、投影值的精确身份和文本
长度均保留。引用子项仍保留其真实类型；此函数不猜测嵌套 schema，也
不等同于递归序列化或语言 JSON/XLON 格式策略。

`xl6ListImportTakeV1` 消耗私有空的已投影目标，成功返回同一目标，失败
释放部分目标。完整同域且 resident codec 相同的原生源复用 XRT 事务
合并，直接复制 typed 存储，不装箱。其他原生源先投影出独立数据，再
显式转换；普通 IntMap 通过真实稳定快照 iterator 读取。每个输入值都
经过目标策略，不修改源的身份、内容或键。非空／未投影目标、错误 wire
类型及同 shell／同 owner 的源别名均拒绝。

这层 IntMap 是显式数据边界，不是 `var` 的替代载体。既有语言持久化
计划中的 List-to-Array 策略未在本批改写；完整格式策略仍由闭合 IR
计划决定。新增私有函数尚未登记普通产品、VFS、原生组件或语言分派。

## 最终源码验收

冻结源码位于 `E:/xlang-validation/stdlib53-b148-source-final-dev3/frozen/demo6`，
Linux 副本为 `/var/tmp/xlang-stdlib53-b148-final-dev3`。测试编译实际选定
XRT/xruntime 实现，不链接旧 DLL 或替换 storage/provider。两端均运行
严格 C11/Wall/Wextra/Werror 的 O0/O2；Linux 对本次完整选定原生闭包
启用 ASan/UBSan、leak 和 stack-use-after-return，不代表整个 runtime。

20 类 vector 保留 B146/B147 的拥有图、游标、身份、深度和失败清理测试，
新增同对象别名、精确投影、三条导入路径、同 owner 别名拒绝、非法解码
槽、首回调停止、重入、最后 shell 释放与入口边界失败后消耗输入。
24 类分配失败前缀逐点闭合，包括 read/take/set/append、wire、原生和
wire 导入。每次核对内容、计数、输出哨兵及 allocation/free/live bytes、
invalid/double-free/UAF。

| 最终报告 | 阶段 | 闭合前缀 | 前缀运行 | 实际失败命中 |
| --- | ---: | ---: | ---: | ---: |
| E:/xlang-validation/stdlib53-b148-list-win-final-dev3/results.json | 322 | 48 | 278 | 230 |
| /var/tmp/xlang-stdlib53-b148-list-linux-final-dev3/results.json | 322 | 48 | 278 | 230 |

两份报告 Complete=true、InputsUnchanged=true，Failures 为空。合计
644 阶段、96 个闭合前缀、556 次前缀运行，含 460 次实际失败命中和
96 次首次未触发。平台和优化等级重复计数；零分配绑定路径的未触发
不是 OOM 失败测试，也不以这些数字代替不同函数数量。

入口和 cleanup 的故障注入使用 GNU ld `--wrap`，只在显式激活时拒绝，
其他调用委托真实实现；包含 NULL 首诊断，**不是实际 OS/FLS 耗尽**。
测试中的 codec 代理仅注入回调行为，不替换容器算法或拥有图 provider。
测试 API／字段拼写、语法警告与 handle Clone 假设的失败报告保留，不
计入通过。最后的入口清理补强按新冻结源码统一复测，不沿用旧报告。

提交前两端 index audit 核对隔离源码、暂存生产／测试输入与报告哈希：
`E:/xlang-validation/stdlib53-b148-index-win-final-v2.json` 和
`E:/xlang-validation/stdlib53-b148-index-linux-final-v2.json`。文档不参与 native
编译；没有生产或测试差异豁免。

## 后续整体切换

下一步是 List 的嵌套 resident schema 与单一图 memo 的递归投影／恢复，
再整体切换字面量、默认值、索引、foreach、producer、var 和持久化入口，
完成真实产品 JIT/AOT/VFS/组件裁剪。Dict/Set、SlotMap、WaitQueue/Channel、
OrderedMap 和跨组总验收继续。CRT 无未决策，19 个 XRT 未决策组不变；
对象池最后，长期目标保持 active。
