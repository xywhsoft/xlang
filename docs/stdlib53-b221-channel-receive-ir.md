# Channel 拥有接收的 typed 与 lowered IR 接线

Channel 的 `recv`、`tryRecv`、`recvTimeout` 现在具有独立的资源操作和
运行时操作身份。签名、泛型逻辑合同、lowering、整模块验证、拥有值
消费与错误观察共用封闭的接收计划，不再为这一组操作重复猜测结果
类型或把输出变量写回作为返回值。

本批完成 IR 接线，尚未完成公开方法的语义注册或原生 Cgen 切换。
已验证的 owning receive IR 在 Cgen 边界明确拒绝，不落回旧 Into
实现。现有公开 Channel 与 WaitQueue 仍须继续迁移，不能因此标记
三组标准库总验收完成。

## 同一声明表贯穿接收合同

`xl6_channel_receive_ops.inc` 同时定义接收种类、资源操作、运行时操作、
公开名称、原生等待模式和超时输入。原有枚举身份保持，新增拥有接收
身份在末尾追加；旧 Into 没有被当作新操作的兼容别名。

| 操作 | 输入 | 结果 | 原生等待模式 |
| --- | --- | --- | --- |
| recv | Channel | `(int, T?)` | WAIT |
| tryRecv | Channel | `(int, T?)` | TRY |
| recvTimeout | Channel 和 int 毫秒 | `(int, T?)` | FOR |

状态必须是 xlang 有符号 64 位整数。T 已可空或为 var 时复用同一
值类型；只有新增的 tagged optional 才记录额外标签。正常成功、空、
关闭和超时独立于真正的复制或资源错误，错误合同固定为 may-raise
与独立 STATUS_OUTPUT 观察，不依赖是否成功分配了错误对象。

`xl6IrChannelReceiveInstructionPlan` 从实际 SSA 接收者重新封闭拥有者、
规范消息 schema、负载类型、传输 ABI、槽 ABI、准入身份集合、结果
和标签形状。错误字段、越界操作数、旧 Into、写回存储引用和不相关
键或容器元数据被拒绝；拒绝不改写输出计划。

Lowering 借用接收者，返回值独立拥有，显式建立紧随生产者的错误
分支。整模块验证要求精确结果和 checked outcome；拥有值消费分析
将新操作的输入视为观察，不制造形参消费或写回所有权。

## 泛型声明与特化

开放的 Channel 泛型只能保留逻辑负载与精确 `(int, T?)` 形状，不能
获得猜测出来的槽或传输 ABI。未被调用的自由泛型函数可能还没有
callable 类型，因此其显式泛型声明才是授权来源；泛型类成员使用
声明的类身份。普通函数、模块根和无效结果不能借此获得授权。

特化后的操作必须重新通过封闭计划和 intrinsic 检查，开放的逻辑
合同不能直接进入可执行指令。Future、闭包和拥有型原生记录的
Channel 传输范围没有在本批扩大。

## 双端验证

Windows 303 和 Linux 302 个翻译单元均从隔离的选择性源码构建。
`check_stdlib_channel_recipes.py` 在两端各完成 20 个阶段，O0 与 O2
严格编译和运行均通过。统一测试的既有 unused 和缺省字段初始化
警告豁免保持，类型、转换和其他警告继续按错误处理。

新增合同测试覆盖 39 个封闭计划、6 个泛型逻辑合同、7 条真实 typed
到 lowered IR 路径和 1695 次拒绝检查。IR 路径使用实际源码类型池、
返回产品工厂、出口清理、SSA 和整模块验证；测试内部将已验证的
send 夹具转换为接收 IR，不把这一转换冒充公开接收语法验收。
破坏载荷和 checked outcome 时整模块验证拒绝，导出函数保证这些
操作不会被死代码裁剪隐藏；Cgen 对新操作给出未实现诊断。

消息工厂回归包含 13 个封闭消息类型、39 个计划、3 个开放泛型和
3 个不支持的具体类型，353 次拒绝检查通过。实际 Cgen 类型与
Sequence 工厂仍执行原生 Channel 的独立拥有槽测试。并发队列
回归保持 60 个配方与 960 次损坏拒绝，三种 family 均覆盖。
旧 Channel 整数、精确 string、var 与捕获、class 回归保持通过，
不计入新公开接口验收。

本批证据保存在 `E:/xlang-validation/stdlib53-b221-tests-win-final`
和 `/var/tmp/xlang-stdlib53-b221-tests-linux-final`。最终选择性源码
另与每个实际编译输入、头文件、产物和日志逐一校验；XRT 消费者
快照保持，未修改外部 XRT 供应者工作区。

## 下一步

公开语义注册应直接产生拥有接收结果，然后将 Channel 构造、发送、
接收、关闭、查询、类型准入、拥有图与按需组件全部切换到常驻原生
底座，清退旧 Into 和生成器中的 Channel 算法。WaitQueue 按已批准
的独立 backend 设计继续，不因 Channel IR 接线而视作完成。

语言层故障前缀、真实接收源码、JIT、AOT、裁剪、拥有图压力、shipping
DLL 和 53 模块发布总门仍是后续验收。CRT 0、XRT 19 组未决策及
对象池最后的范围保持不变。
