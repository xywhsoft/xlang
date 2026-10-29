# Socket 错误队列与 PMTU

这部分复用 XRT Socket 的既有选项、异步数据报错误结构和读取接口。
没有新算法、Cgen 实现、原生指针构造或第二套错误存储机制。

## 类型化控制

`NetPathMtuMode` 提供 `system`、`discover`、`fragment`、`probe`。
Socket 提供 `setPathMtuMode(mode)`、`pathMtuMode()`、`pathMtu()`，后者返回
`uint64` 字节数；错误或不支持不伪装成成功的零值。原通用选项入口保持可用。
`enableDatagramErrors(enabled=true)`／`datagramErrorsEnabled()` 控制错误队列。
设置使用独占控制借用，查询使用共享借用；能力依旧按当前 Socket 查询，
不以编译平台猜测，也不回退到别的机制。

## 错误是数据

`NetDatagramErrorInfo()` 是可复用的调用方独占输出，和语言异常 `Error` 不同：

| 公开字段 | 合同 |
|---|---|
| `hasError`、`clear()` | 判断／清除已接收快照 |
| `flags` | 独立 `NetDatagramErrorFlags`：remote、offender、pathMtu、payloadTruncated、metadataTruncated |
| `origin` | 独立 `NetDatagramErrorOrigin`：unknown、local、icmp、icmp6 |
| `kindCode` | 原始 XRT `xerrkind` 数值，不是语言 `Error.kind`，也不新增全局错误分类决策 |
| `systemCode` | 有符号平台错误码，保持原值 |
| `protocolType`、`protocolCode` | 原生协议 Type／Code |
| `detailInfo`、`detailData` | 原生无符号 32 位字段，无损提升到语言 `int` |
| `pathMtu` | 对应标志有效时返回 `uint64`；不进行有符号缩窄 |
| `remote`、`offender` | 对应标志有效时返回独立拥有的 `NetAddress` 副本 |

尚无快照或缺少对应字段时 checked 拒绝读取，不用零、空地址冒充数据。
快照不保留 Socket、Buffer、语言值或地址对象；读取地址副本失败可以重试，
不破坏已有快照。它是可变对象，调用者负责内容的排他使用。

`socket.receiveErrorInto(output, info, maxBytes=65536)` 返回 `(NetStatus, uint64)`：

- 原始错误负载按字节追加到 Buffer；`maxBytes=0` 合法，其余负数拒绝。
- 在读队列前完成输出容量预留。分配失败不消费队列项。
- `ok`／`truncated` 提交逻辑长度和新快照；`again`／读取错误保持原逻辑内容。
- 已读到的错误记录（包括非零 `kindCode`）只是数据，不安装为本线程的 checked 错误。
  只有读取、资源、格式或参数失败走 checked 错误边界。
- 当前固定 XRT 的错误队列在 Linux 可用；其他平台按能力与 checked Unsupported
  表达不支持，不模拟、不静默丢弃，也不把不支持当空队列。

## 裁剪与验收边界

错误值、错误队列 I/O、拥有域分别登记精确组件；错误值单独使用不根入 Socket
或 Buffer，错误队列不强制根入普通数据报值／I/O 实现。PMTU 薄适配复用控制组件。

`check_stdlib_net_datagram_runtime.py --error-queue` 验证离线有效字段、完整宽度、
缺失字段、地址复制 OOM、叶句柄拥有图、设置／共享查询借用、空队列／Unsupported、
读取前 OOM 和快照保留。PMTU 路由查询使用自建 IPv4／IPv6 回环 UDP 对端，
只 connect 并查询系统路由，不发送探测报文。

**不将空队列测试称为真实错误出队验收。** 本轮不制造 ICMP 或错误报文，
`SuccessfulNativeErrorDequeue` 在报告中明确为 false。真实错误负载、来源及截断的
原生平台行为仍需要用户在 XRT 层的既定验证；语言层成功出队端到端也未在本批证明。
没有外部网络、DNS、扫描、攻击模拟、网络安全专项或 shipping 认证。

数字枚举位运算仍待决策，本批只使用已有显式整数检查和精确枚举传参。

## 冻结证据

证据根：`F:/xlang-validation/five-groups-20261020/p2-datagram-errors`。

- `runtime-win-r2`／`runtime-linux-r2`：48／56 阶段，四类精确闭包；
  GCC／TCC O0／O2、Linux 实际闭包 ASan／UBSan，全部通过且输入稳定。
- `source-win-r1`／`source-linux-r1`：完整工作区源码 660／659 单元。
- `language-win-r1`／`language-linux-r1`：两端各 117 阶段，10 个用例、
  9 个负例及每级三组合同；严格 C、JIT、module、独立 AOT、动态依赖和裁剪通过。
  独立错误值 AOT 不根入 Socket／Buffer；全部 AOT 不依赖 xlang DLL／SO。
- 每个优化级别的错误专项检查 21 目标／1 产品／15 个精确拥有位置／4 个枚举位置，
  拒绝 116 项损坏合同且不生成 C／头文件；扩大后的数据报合同拒绝 306 项，
  Socket 合同拒绝 360 项。
- `verified-win.json`／`verified-linux.json` 复核报告、源文件、实际对象、
  SDK、驱动、日志和产物哈希。驱动使用实际源码对象；仅 TCC 核心按既有工具链
  复用，不等于新 shipping 验收。

R1 原生检查通过后，R2 增加双协议族的实际路由 MTU 查询范围，并非重试失败。
上述证据不扩张前述成功错误出队／shipping 的未验证边界。
