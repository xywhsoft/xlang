# 拥有型原生值的容器接入与键 ABI 校正

SlotMapKey 的同步语言 ABI 现在可用于 Array、List、Set、Dict 的精确值域，
包含嵌套 Array 与 var 导入。它仍是不可变基础值：Set 按内容判等；可变
对象和容器按引用身份，插入后修改内容不会改变其键身份。本批不新增
SlotMap 公开 API，不修改供应方 XRT，不关闭三组标准库总任务。

## 同一声明提供容器能力

`src/type/xl6_native_value_profiles.inc` 同时描述直接 C 记录、原生槽位、
描述符、Value 转换器、永久值 profile、语言身份和驻留 Sequence 工厂。
IR 据此派生 transfer-5、direct codec 和空记录默认值；Cgen 仅消费经过
验证的计划与真实符号，不生成转换器、哈希或库实现，也不靠诊断 AST
的形状判断集合读取是否可以投影。原生值的复制、判等和投影仍遵守
独立的 checked 结果与拥有权合同。

集合的标量／记录装箱结果统一使用真正的动态 Value 载体，不再把一个
装箱元素标成其 Array／List／Set／Dict 容器类型。仍保留原生 xvalue
元素的精确身份，不把类型擦除当成放宽值域准入。

## 修正两个运行时合同错位

原生键的旧 Value ID 来自 XRT 原生 ABI 哈希，误以为它就是语言的
`type:SlotMapKey`。两者的哈希种子不同。本批将语言 ID 统一为
`src/type/xl6_native_value_ids.h` 的常量；IR 独立重算的类型身份必须与
声明相等，工厂读取相同的永久 profile。原生描述符的 ABI ID 保持不变，
不改动语言的全局 TypeId 算法，也不在调用点临时重新标记已有对象。

XRT `xrttype` 的 Copy／Move 是向已初始化槽位赋值，不是只向空输出
取得副本。键的描述符因此改为先取得新拥有权、发布新值，再释放旧值；
复制失败保留旧值，自复制与自移动不改变值。语言 checked Copy／Unbox
则仍要求私有、对齐、不重叠的空输出，拒绝已占用目标。这是两个不同
边界，不能用放宽语言 acquire 的方式满足容器替换。

## 提取是 status 加拥有型输出

Array 的 pop／take 调用共享驻留 `xl6ArrayExtractNativeValueV1`。原生槽
直接移动，没有临时 Value 装箱或堆分配；wire Array 先完成受认证投影，
再删除元素。成功的空键和失败必须靠独立 status 区分，不能检查实例
指针代替成功信号。失败清理已取得的结果，保留原错误优先级；未对齐、
已占用或与数组任何 backing 槽位重叠的输出不会被释放或写入。

## 验证与边界

Windows／Linux 均使用冻结源码的完整编译器（301／300 个对象），实际
接收来自原生 SlotMap 的真键，先销毁两张原表与原键，再执行语言夹具。
两端各 479 个阶段、O0／O2 各 1000 轮、446 次完整失败前缀及恢复重试
通过；覆盖复制、投影、装箱、无诊断提取失败与 OOM。1069 个损坏 IR
变异在生成 C／头文件前被拒绝，五个实际编译的坏实现被真实执行检出：
不取得拥有权、漏释放、忽略实例域、错误语言 ID、描述符只接受空槽位。

原生提取专项另外覆盖赋值／移动、native／wire、空值、范围、对齐、
占用、backing 重叠和无分配移动；Linux 对该独立闭包使用 ASan／UBSan。
相邻同步 IR、原生键／容器／拥有图、Set、Dict 直接槽位与驻留目录回归
独立记录。语言源码消费者本身不是全运行时 sanitizer 产品；主夹具链接
完整驻留对象，依赖计划检查也不能代替裁剪后的最终发布产物执行。

证据根为 `E:/xlang-validation/stdlib53-b211-*` 与 Linux
`/var/tmp/xlang-stdlib53-b211-*`。失败的探索报告保留，不作为通过证据。
旧文档目录和文本守卫的历史失败分别比较，不标记成全绿。

通用索引读取仍可能先经过 Value 投影；尚未宣称所有访问都成为 direct
slot。nullable、capture cell、挂起 frame、Stack 等其他消费边界、公开
SlotMap API、裁剪发布产物与三组总验收继续推进。对象池仍在最后；
CRT 未决策 0、XRT 未决策 19 组，不引入未批准的新功能。
