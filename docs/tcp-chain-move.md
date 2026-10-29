# NetBuffer 引用块与跨线程链式接管

```xlang
auto chain = NetBuffer()
chain.append("header")
chain.appendRef(payload, size=-1, offset=0) // payload 是不可变 NetBytes
NetStatus status = stream.sendMove(chain)
```

`sendMove(NetBuffer)` 和 `sendMove(Buffer)` 都只接管全部有效内容，不提供部分
消费参数。OK 后源和所有别名看见空容器，可以立即复用；AGAIN、CLOSED 和受理前
checked 失败保留原链及内容。OK 表示队列受理，不等于对端已收到。

## 原生合同

原有 `xrtNetStreamSendBuffer` 从 Worker-only 扩展为任意线程受理。先预留整个
发送预算并建立全部视图元数据，成功后才移动实际块链并提交完整批次。提交失败
恢复原 Head／Tail／Blocks／Size；队列不持有调用者容器的地址。数据地址不变，
没有展平或复制载荷；仍有发送／视图元数据分配，不声称内核端到端零复制。

受理后 Abort 释放真实载荷；后续写入／IOCP 分配失败由流错误报告，不伪装成源
回滚，不能撤销网络前缀。C 调用者须串行访问源；异线程接管只接受堆块或所属
Worker 的池块，拒绝外来线程亲和池和活动写预留。语言层只使用堆块／不可变引用。

复制、单块引用、批量引用、文件及链式命令在水位回调期间保持活动生命周期，
避免回调 Abort、失去最后外部引用后命令尾部访问已退役 Stream。只使用真实拥有者。

## 拥有图与语言分层

`appendRef(NetBytes,size=-1,offset=0)` 保留实际不可变块。size 缺省／-1 取 offset
之后的全部字节，超长截断；小于 -1 或 offset 不在 [0,length] 报错。单位是字节，
允许切分 UTF-8／内嵌 NUL。可变 Buffer 须显式快照。失败不留下半条引用或锁死容器。

NetBuffer、行／长度分帧器和代理握手缓存经 appendMove 接收引用块后都不能作为
拥有图叶节点。驻留 policy 用 XRT 只读接口枚举真实块／池边；握手还枚举原本持有
的 Proxy。重复引用保留重复边；未知借用／外部回调拒绝认证，不执行释放回调，
不伪造引用计数节点。修改在 mutation 内，Freeze 期间只读追踪，不反向获取 mutation。

精确类字段／状态枚举／checked 和 nonblocking ABI 共用现有 XLIR；Cgen 未修改。
单纯 NetBuffer 不拉入 NetBytes 类；同步接管不因本入口引入 Future 回调／deadline；
纯缓冲引用图不拉入 Engine／TCP。

## 验收与范围

证据根 `F:/xlang-validation/five-groups-20261022/p3-tcp-chain`，最终输入／日志／
产物和提交封条见 `verified-commit.json`。覆盖双端 GCC／TCC O0／O2、Linux
ASan／UBSan、select／IOCP 或 epoll、48 个原生接管预算、65 个引用块、预留／
背压／中止／重入、驻留 OOM／精确拥有图，以及 JIT／module／独立 AOT／裁剪／
IR 门禁。假叶节点对照须成功编译后由拥有图断言拒绝。

| 正式分阶段检查 | Windows | Linux |
| --- | ---: | ---: |
| 独立 XRT 与固定快照原生专项 | 16 | 20 |
| 驻留完整图、OOM 与拒绝对照 | 53 | 58 |
| 源码 JIT／module／AOT／裁剪／IR | 62 | 62 |

合计 271 个阶段；源码驱动分别为 791／790 个编译单元。另有同步工具 15 个
单元测试及 427 份真实预处理链接归属证据，后者不混算成跨平台运行检查。

固定头只同步本批网络段，保留微秒 ABI，不导入其他内存／文件改动。API 为 xrt
3321／xruntime 3786，扩展自有仍 465；新增追踪入口归属 NET_BUFFER 和
NET_PROXY_HANDSHAKE。

仅离线及自有字面量回环正常通信，无公网、DNS、扫描、畸形协议或网络安全专项。
不重建发布 DLL／SO／VFS，不是全部网络完成。UDP、文件／任务组合、剩余并发、
非对称密钥、X.509、TLS 和发布联合验收仍开放。未决策 CRT 0／XRT 11，对象池最后。
