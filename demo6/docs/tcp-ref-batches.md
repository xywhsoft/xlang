# 不可变 NetBytes 的原子批量引用发送

`TcpStream.sendRefs(array<NetBytes> data, int size=-1) as NetStatus` 是引用型入口，
不同于已有的复制型 `sendVector(array<string|bytes|Buffer>)`。它复用同一个
NetBytes 类型和实际 XRT 字节块，不引入裸指针、释放回调或第二套字节对象。

```xlang
array<NetBytes> chunks = [NetBytes(b"head\0"), NetBytes("内容")]
NetStatus status = stream.sendRefs(chunks)
```

## 合同

- 允许空数组、空块和同一块多次出现。64 项以内走栈上元数据，大数组经过大小与
  溢出检查后使用临时工作区，不为原子批次增加人为的项数上限。
- size 缺省／-1 取拼接后的全部字节，超长截断；可以截在块中间或 UTF-8 编码中间。
  小于 -1、工作区大小或总长度溢出时报 RangeError。不是逐项 size，也不是项数。
- 传输队列完整预留所选字节预算并原子受理整批，不以逐项 sendRef 循环伪装原子性。
  AGAIN／CLOSED 不新增原生载荷持有者，源数组和块始终不变。
- OK 只代表受理，不代表对端已经收到。受理后的写入／元数据分配失败由流错误或
  关闭报告，不能把网络发送视为事务或撤回已经发送的前缀。
- 成功后数组及所有语言包装都可释放，原生发送持有自己的引用直到真实消费／关闭。
  没有额外载荷复制，但仍有有界的引用／队列元数据分配，不声称内核端到端零复制。
- 可变 Buffer 须显式构造 NetBytes 快照，或走既有复制／接管入口；不隐式借用可变
  载荷。共享数组的结构性修改须由调用者同步，不新增语言借用或参数修饰语法。

## 分层与原生实现

XRT 提交 `0fec108e` 增加 `xrtNetStreamSendBytesVec` 和 `xnetbytesref`：每项指定
Bytes／Offset／Size。标准库当前公开全块数组加合计前缀；C 使用者仍可选逐项片段。

新的原生 typed 构造沿用已有批次分配、预算预留和 Worker 提交，只增加明确来源
标记及真实 NetBytes 持有关系。不猜测任意 callback 的意义，也不把旧 opaque
`SendRefs` 一律纳入拥有图。首个存活 FIFO 视图枚举全部存活载荷和唯一 Stream
引用，重复字节块对应重复真实边，退休前缀不再访问。

语言数组在现有统一集合合同中登记；语义层和 XLIR 共用精确 NetBytes 类字段、
policy、状态枚举和 checked／nonblocking ABI。驻留适配只捕获实际数组／字段句柄
到同步受理返回，随后清理；原生队列中没有生成的类、描述符、闭包或借来的字段名。
Cgen 源文件未修改。使用此同步入口不拉入 Future 回调队列或 deadline 服务。

固定头只同步 tcp 公共段、两个内部记录段和 buffer／tcp 源段；保留旧固定微秒 ABI，
不导入独立 XRT 工作区的其他内存／文件改动。xrt／xruntime 公共 API 分别为
3319／3784，扩展自有仍为 465。原生链接归属由真实预处理闭包重新生成：
3784 个符号、425 个候选根，本次目录只增加 SendBytesVec → NET_TCP 一行。

## 验收边界

证据根：`F:/xlang-validation/five-groups-20261022/p3-tcp-ref-batch`。

| 范围 | Windows | Linux |
| --- | ---: | ---: |
| 独立 XRT 与固定快照的原生批量专项 | 16 | 20 |
| 驻留 class-array 图、预算和拒绝对照 | 29 | 32 |
| 标准库联合 JIT／module／AOT／IR | 43 | 43 |
| 65 个非空原生项及空项的完整语言批次 | 9 | 9 |

合计 201 个正式检查阶段；最终源码驱动为 Windows 789／Linux 788 个编译单元。
真实命令、输入／日志／产物哈希以及提交前复核见 `verified-commit.json`。
原生链接目录的 427 份预处理证据单列保存，不冒称跨平台运行测试。

原生模块化两个受影响 TU 严格编译；独立原生单头和固定快照分别运行两端 GCC／TCC
O0／O2、select／IOCP 或 epoll、Linux ASan／UBSan。每进程 24 个分配预算及两个旧
opaque 回归；直接在真正的 Worker 高水位回调、写驱动之前观察原生块地址与拥有边，
不把 active 回调冒充整图 Freeze。

驻留专项另验证真实 class-array 完整拥有图、16 个小数组与 4 个大数组预算、源包装提前释放和 NUL
精确结果。漏登记 Stream／Server／NetBytes、漏亲缘边或假叶节点的独立对照必须
在成功编译后被生命周期断言拒绝。语言层运行新入口和既有复制向量、拥有型发送／
接收的实际 JIT／module／独立 AOT；IR 专项检查错误数组字段／效果／服务合同，
由完整 verifier 和 C／头文件输出门禁阻止。

仅使用离线合同和自建字面量回环正常通信，不执行 DNS、外网、扫描、畸形协议或
网络安全专项。本批不重建发布 DLL／SO／VFS，不是全部网络或长期任务完成声明。

## 后续及未决策

后续 [链式接管](tcp-chain-move.md) 已补齐任意线程合同及 `sendMove(NetBuffer)`；
这不是本页历史批量引用验收本身的覆盖范围。
完整 UDP、文件／任务组合、P4—P7 非对称密钥、X.509、TLS 和发布联合验收仍开放。

未决策仍为 CRT 0、XRT 11 组：core／error／ownership；内存；动态值与图；其余
平台信息；HTTP；WebSocket；运行时类型／转换；对象／字段图；调用／value 桥接；
typed 容器；内部支撑。内存池依旧最后处理。
