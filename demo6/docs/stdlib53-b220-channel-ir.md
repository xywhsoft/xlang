# Channel 拥有接收结果的统一 IR 合同

Channel 的消息类型与拥有接收结果已经具备闭合的 IR 配方。公开接收
采用 `(status, T?)`；成功、空、关闭、超时与真正失败彼此独立。
本次完成语言切换的前置合同和原生类型工厂验证，公开方法注册、
完整 lowering 与原生 Cgen 切换仍待完成，旧 Into 接口尚未清退。

## 消息类型与准入

Channel 和 SPSC、MPSC、MPMC 共用 `messageSchemaType`，指向规范的
`Array<T>` 准入类型，不共享 Array 存储。源码类型构建和泛型实例化
主动建立该类型，不依赖用户是否使用数组；类型池验证重新推导字段。
各队列保留自己的原生 family，WaitQueue 不因此被当作普通 Channel。

`xl6IrChannelConstructionRecipe` 核对拥有者、规范消息类型、完整
Sequence 布局和独立 nominal 准入集合。V1 只接受直接平凡值、精确
string 和 retained xvalue 引用；闭包、Future、拥有型原生记录和未
闭合泛型不推测物理槽。失败不写输出，正常 Channel 负载解析也复用
这一配方，不再只读取泛型参数并猜测其传输能力。

## 接收结果

三种接收计划共用声明表和 `xl6IrChannelReceivePlanBuild`：

| 接收计划 | 输入 | 拥有结果 |
| --- | --- | --- |
| `recv` | Channel | `(int, T?)` |
| `tryRecv` | Channel | `(int, T?)` |
| `recvTimeout` | Channel、毫秒超时 | `(int, T?)` |

这里的名称是新 IR 合同，不表示当前公开方法已切换。旧 `recvInto`、
`tryRecvInto`、`recvTimeoutInto` 不属于此合同。

状态必须是精确有符号 64 位整数，不能以 bool 或其他相同宽度类型
替代。结果的可空类型必须与实际负载身份对应，拥有类别也必须匹配。
T 本身已经可空或为 var 时复用 T，不再次嵌套 optional；正常成功
的 null 由成功状态区分。新加的 tagged optional 由计划明确记录。

原生 bool 失败和正常状态输出是两条边。计划固定为 may-raise 和
独立状态输出错误 ABI，不将资源或复制故障编码成普通缺失状态。
接收目标是编译器私有的空拥有槽，不是调用者变量或语言借用。

## 验证范围

真实源码类型图包含 13 个闭合消息类型和 39 个接收计划；一个未闭合
泛型及三个不支持的具体传输类型被拒绝。345 次检查覆盖损坏拥有者、
消息布局、错配类型域、错误状态类型、错误拥有类别和非法结果形状，
并核对拒绝时输出保持不变。

实际 Cgen 类型与 Sequence 工厂建立对应常驻 Channel，严格 O0、O2
运行空、零毫秒超时、成功、关闭和清理。12 个类型执行成功发送与
拥有接收，覆盖内嵌 NUL 的精确字符串、bytes、nullable 和 var；非空
class 验证类型工厂和伪造 wire Object 拒绝，不将其冒充原生语言实例。
全部工厂的分配和释放按内存快照核对。

旧整数、字符串、var、闭包捕获和 class 消息用例用于回归，不能作为
新公开接收接口的验收。此专项不替代 WaitQueue backend、语言层故障
前缀、完整拥有图压力、shipping DLL、VFS 或三组标准库总门。

复测入口为 `tools/check_stdlib_channel_recipes.py`；当前证据目录为
`E:/xlang-validation/stdlib53-b220-recipes-win-final3` 和
`/var/tmp/xlang-stdlib53-b220-recipes-linux-final2`。
