# Channel 显式状态与拥有接收值的公开接入

Channel 的公开接收方法现已贯通语义检查、统一 IR、常驻原生 ABI 和
C 生成。接收不再写回调用者的变量，而是返回独立拥有的 `(status, T?)`。
Windows/Linux 源码运行、模块重定位、严格 C、独立 EXE 和按需裁剪专项
均通过。本批不是 WaitQueue 验收，也不代表 shipping SDK 或 53 模块发布
总门完成。

## 公开合同

| 方法 | 输入 | 结果 |
| --- | --- | --- |
| `recv()` | 无 | 等待后返回 `(status, T?)` |
| `tryRecv()` | 无 | 非阻塞返回 `(status, T?)` |
| `recvTimeout(timeoutMs)` | 一个位置整数参数 | 有界等待后返回 `(status, T?)` |

`channelStatus.ok/empty/full/closed/timeout` 对应 `0/1/2/3/4`。
接收结果区分成功、空、关闭和超时；真正的复制、类型准入或资源错误
仍走 checked 异常边界。对于 `Channel<T?>`，成功收到 null 与空／关闭
依靠状态区分，不重复套一层 nullable。

字符串、bytes 按基础值规则传递，保留完整字节长度；类对象保留引用
身份。Channel 自身是共享身份的资源对象，函数返回、别名及作为接收
者的函数调用结果会保留其生命周期，不复制一个新的原生队列。
默认容量为 2；显式容量 1 不再被扩大，0 表示 rendezvous。

旧 `recvInto/tryRecvInto/recvTimeoutInto` 在语义检查阶段明确拒绝，
不作为兼容别名。存量使用这些旧方法的其他夹具与文档仍需继续迁移；
本批没有宣称旧的全量测试清单已通过。

## 实现边界

公开名称、运行操作及等待模式共用接收声明表。泛型模板只具有逻辑
合同，具体实例必须重新验证精确消息类型、拥有结果及失败 ABI，不能
把模板的 pending 标记当作执行权，也不能从源代码名称推断 codec。

生成 C 仅包含机械调用、精确局部槽位和 checked 清理。Channel 的传输、
消息 box、准入、操作 pin 和拥有图算法由常驻 provider 负责。
`xl6_channel_abi.h` 是声明头，Channel 不再请求旧 `xlir_channel_*`
生成实现。旧实现仍服务于尚待迁移的 WaitQueue，未在本批删除。

原生 status 使用独立 enum 局部槽位，之后转换为语言 int，不把
int64 地址误当成原生 enum 输出地址。失败后已取得的拥有值先在保留
原错误的清理作用域内释放，再进入语言错误边界。按需生成也把这种
checked 结果的失败清理计为 product drop 依赖。

文件系统验证 SDK 按已登记 manifest 镜像原生源码；AOT 只从编译器
模块旁的固定 `include/__xl_compiler` 根或既有开发／内嵌 VFS 根读取。
不搜索用户项目和任意 include 目录来补实现。此镜像不包含嵌套编译器
的生成资源，不等同于已经验收的发布 SDK。

## 已验证的证据

`check_stdlib_channel_owned.py` 在 Windows/Linux 各完成 34 个阶段：

- 全量源码构建分别为 303／302 个翻译单元，消费者 XRT 快照不变。
- 两份真实源码覆盖 20 个接收操作，含 int、char、bytes、string、
  nullable、类对象、泛型 int/string/类接收、接收者表达式、未消费
  类消息清理、rendezvous、跨线程与关闭后 drain。
- 源码 JIT、模块重定位、O0/O2 严格 C 检查、独立 EXE 构建与运行。
- 四项语义错误拒绝；O0/O2 各验证 39 个计划、6 个泛型逻辑合同、
  7 条 private lowering 链及 1695 项损坏合同。
- 实际源码的原生组件计划选中一个常驻 Channel provider，不选旧
  Channel bridge 或 typed MPMC。未调用的 Channel 函数不进入组件计划，
  其生成 C、JIT 与独立 EXE 也通过裁剪专项。
- 两份已有拥有值／callable/Future 并发 Queue 源码回归通过。

证据目录为 `E:/xlang-validation/stdlib53-b222-tests-win-dev9` 与
`/var/tmp/xlang-stdlib53-b222-tests-linux-dev9`。实际编译源码、验证源码、
私有 SDK 文件、产物及日志在提交前与最终选择性快照逐一校验。

## 尚未完成

WaitQueue 独立 backend 与拥有接收 API、旧 fixture 清单迁移、语言层
故障前缀／无诊断错误压力测试、更广的拥有图与模块卸载压力，以及
shipping DLL/SDK 和 53 模块发布总门继续。CRT 0、XRT 19 组未决策与
对象池最后的范围不变，不扩大本轮功能范围。
