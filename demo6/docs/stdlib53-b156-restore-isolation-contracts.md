# 共享默认对象的隔离合同

日期：2026-10-10。用户已确认 JSON/XLON 恢复不修改外部共享的默认
子对象：使用声明的 clone 能力隔离，没有能力则 checked 拒绝。
本批将这一选择固定为 lowered XLIR 的静态执行合同；运行时拥有图认证
与 Cgen 的共享原生事务切换仍在开发，不能据此宣称共享默认对象已经安全恢复。

## 声明能力与隔离操作

类恢复节点新增 `isolationAbi`、`cloneFunction`、`cloneSymbol` 和
`cloneErrorAbi`，与声明默认工厂分开。字段实例、nullable 替换和集合
元素使用各自的恢复域，但同一类的隔离能力来自同一个 ClassLayout。

| ClassLayout 的 clone 合同 | 恢复隔离合同 |
| --- | --- |
| 未声明或非复制资源 | checked 拒绝，不返回成功的 null |
| 显式 derive copy 的既有 clone 回退 | 声明的浅复制，子对象仍须独立认证 |
| 显式 derive clone | 声明的深复制，不从字段形状猜测能力 |
| 用户 clone 或既有用户 copy 回退 | 精确的内部函数调用 |

内部调用必须有唯一的已 lowering 用户函数。接收者按地址借用，但
这是编译器 ABI，不恢复语言参数修饰语法；参数和拥有结果必须具有
相同的精确 TypeId。同步调用的物理参数与结果为 DIRECT，错误合同
与实际函数一致，不能使用异步调用、裸 native 字段或字符串复制适配。

可达 JSON/XLON 读取保留需要的 clone 函数及其普通依赖。跳过的字段
和不参与读取的类不因声明协议而获得可达性。生成器无需扫描 AST、
寻找方法名，或用另一次默认初始化代替复制现有模板。

验证器重新推导并比较隔离合同。伪造隔离枚举、函数、符号、错误 ABI，
以及改变被引用函数的参数、返回、物理表示或角色，均拒绝 C/header
输出。六表重建仍使用独立临时 arena；失败保留原表及其拥有关系。

## 已验证的执行边界

Windows 251 和 Linux 250 个源码翻译单元及各 142 个运行时头成功构建。
仅 TCC 核心复用既有真实候选，编译器和 XRT/xruntime 来自隔离源码。

宽字段、声明构造和新增隔离合同夹具在双端生成确定性 C，O0/O2 执行
的分配账本平衡，真实 TCC JIT 成功，共 66 阶段。新增夹具没有普通
clone 调用，覆盖浅复制、深复制、用户 clone、用户 copy 回退、无能力
和无用协议裁剪。它也包含来自全局变量的默认子对象及返回 self 的
clone 声明，但使用空 wire 保留默认值，只验证旧路径的生成与清理，
不证明这两个危险情况已经执行隔离或拒绝。

List/Set/Dict 的迁移边界补测在双端通过，载体仍标为 pending。
泛型 class 的持久化派生仍被现有语义层拒绝，不属于已通过的隔离
执行范围，后续须补全或明确语言合同，不能以普通泛型 clone 测试代替。

最终六份双端报告共覆盖 194 个执行阶段，包含 17 个 IR 夹具的
O0/O2 合同门、pending 载体补测与上述三个语言回归。63,516 次
损坏 IR 测试均被拒绝，其中 16,860 次是恢复图合同变异；六表重建
另覆盖 156 个分配失败前缀。次数包含双端和两种优化配置，不是
独立缺陷数。生成 C 回归沿用 unused 与 missing-field-initializer
告警抑制，不能等同无告警的完整发布构建。

原 IR 原生执行全部成功，报告汇总曾因枚举表示和变量遮蔽出错。
修正检查器后，核对所有原输入哈希、相同源码/夹具及相同命令，
重新处理保留的日志与 IR；没有重复编译执行，也没有覆盖旧失败
报告。新报告标明 `NewHostExecution=false`，保留原处理错误。
失败命令、输入变化和命令变化的三种复用拒绝检查均通过，未启动
原生程序。dev2 与 dev4 冻结输入仅文档与此检查器不同。

最终 IR 与载体报告位于 Windows 的
`E:/xlang-validation/stdlib53-b156-{graphs,pending}-win-final4/results.json`
及 Linux 的 `/var/tmp/xlang-stdlib53-b156-{graphs,pending}-linux-final4/results.json`；
既有路径语言回归位于对应 `language-{win,linux}-dev2/results.json`。
这些是专项证据，不是发布目录中的产物验收。

## 运行时切换条件

拥有图认证必须按真实 shell、backing、内部边的 multiplicity，以及
事务实际持有的根、临时值和 memo 引用核对外部强引用。单个引用
计数或一个新根对象都不能证明其子对象私有；未知 native 策略在调用
Count/Trace 前拒绝，不能以未知或检查失败为由直接尝试 clone。

认证与实际写入之间必须保持有效的隔离边界，包括弱引用提升。
用户 clone 和析构不能在 ownership freeze 下执行。clone 返回 self、
另一个全局共享对象、错误类型或仍共享的待修改子对象时，必须重新
认证并 checked 拒绝或按声明能力继续隔离，不发布部分恢复结果。

隔离后的 memo 还须保留原默认实例与克隆结果的区别，使用真实引用
防止地址复用，不能继续假设实例 witness 总等于结果对象。普通用户
初始化和 clone 的外部副作用不属于数据恢复事务的回滚范围。

Cgen 的共享原生恢复接入、开放 DynamicFields、模块 owner 和 List
语言载体切换继续；Set/Dict typed 迁移、SlotMap、WaitQueue/Channel、
OrderedMap 公开合同及完整 53 模块产品验收仍未完成。本批没有新增
公开标准库 API，也不包含 AOT、发布 DLL/SO/SDK/VFS 或全产品 sanitizer
验收结论。CRT 无未决策，19 个 XRT 未决策组不变，对象池最后。
