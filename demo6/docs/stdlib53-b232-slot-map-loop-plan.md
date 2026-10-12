# SlotMap 键遍历和键值遍历的类型合同

`for key in slots` 固定遍历完整 `SlotMapKey`，与 Dict 的单变量键遍历
一致。`for key, value in slots` 固定绑定同次快照中的完整键和值。
Sem 和 Typed XLIR 已记录这些规则、拥有关系与失败合同；公开 foreach
尚未接通配对游标，lowering 会明确拒绝，不退回普通字典或整数键。

## 类型和拥有关系

单变量不是槽位索引，也不是元素 T。双变量的键保留 generation 和实例
身份，值类型精确等于 `SlotMap<T>` 的 T。`_` 保留目标形态与推断类型，
但不要求键或值的物化、写入和每轮清理。

SlotMap 使用独立的键遍历与键值遍历 Loop ABI。键投影使用 sealed
SlotMapKey 原生值合同，复制可能失败，绑定拥有值只存活于当前迭代。
键构造的 may-raise 效果在 typed 阶段进入构造与退出计划，而不是由 Cgen
追溯语义。bytes 和 nullable bytes 也有独立的拥有投影 ABI，不伪装成 var。

循环配方要求精确的原表布局、直接值传输 schema 和
`array<SlotMapKey>` schema。游标 TypeId 绑定实际源 `SlotMap<T>`，
不复用普通 Dict 的键布局。任何配方输出只在完整准入后发布；校验器
重新计算循环 ABI、键和值表示、资源类型、生命周期及错误效果。

稳定命名源、函数返回的拥有临时值、已检查字段投影分别保留其获取
合同。直接调用的临时结果以精确 CallSite 和 result TypeId 准入，
不把任意 rvalue 或借用表达式当作拥有对象。

## 双端专项结果

Windows 和 Linux 当前源码驱动分别构建 308 和 307 个编译单元。
O0 和 O2 各验证 18 个循环形态，包括单变量、双变量、全部丢弃绑定、
命名源、函数返回值、字段源，以及整数、char、精确 NUL 和 UTF8
字符串、bytes、nullable bytes、可空整数、原生键、对象和容器值。

每端校验器专项拒绝 216 项 IR 篡改，拒绝时 C 与头文件输出为空。
把键改为整数、把原生键生命周期改为平凡替换的两种实际坏实现，也均
触发对应合同断言。非法键参数、把值作为键、三变量循环和未准入闭包域
在源码检查阶段被拒绝。

尚未实现的配对后端有明确诊断。Windows 验证 lowered IR、默认 Cgen、
XLIR 兼容选择器和 emit 四个已有驱动入口；Linux 验证其驱动提供的
lowered IR 和默认 Cgen 两个入口。没有把驱动不支持的模式当作后端拒绝证据。

既有公开 SlotMap 专项每端通过 780 个阶段、610 个完整失败前缀、
49 个调用合同和 499 项 IR 篡改；JIT、模块执行、独立裁剪程序和公开
AOT 均通过。两端各有 O0 和 O2 的 1000 次生命周期循环。这些回归
覆盖原有方法与独立快照，不证明新的 SlotMap foreach 已可执行。

## 剩余接线

下一步为配对快照增加拥有游标，保留键和值的独立拥有边及 checked
推进结果，再接入 lowering、精确投影和全部退出路径的清理。双变量
必须复用同次原生配对捕获，不能拼接两次独立 keys 和 values 调用。
原有 ValueIterator V2 布局不能被静默扩展或复用为不兼容载体。

新循环的语言执行、break、continue、return、finally、失败回滚、
分配故障、游标压力和按需组件裁剪仍需验收。WaitQueue 与三组标准库
总验收、shipping DLL 和内嵌 VFS 验收仍未完成。此次没有修改 XRT、
xruntime 原生实现或新增未批准的标准库功能；对象池仍放在最后。

专项入口为 `tools/check_stdlib_slot_map_loop_plan.py`，既有回归入口为
`tools/check_stdlib_slot_map.py --snapshots --pruned`。两者均要求新的证据
目录、当前源码驱动和固定输入哈希，不以旧二进制代替当前源码验证。
