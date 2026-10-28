# HTTP 标准库实施与验收

2026-10-28。范围为已批准的 18 个 XRT HTTP 模块，完整公开其语言适用能力。
采用 OOP；协议算法仅使用固定的 XRT，Cgen 不生成 HTTP 实现。

## 实施次序

1. 字段、token/list、参数/指令/quoted-string、Host/authority、request-target。
2. Expect、Upgrade、TE、Connection、Trailer、内容编码协商与编码链。
3. HTTP/1 头部、正文计划与增量正文、trailer、完整消息、线路写出。
4. 受管 Content Decoder、TCP/TLS 组合、取消/期限与协议切换余留数据。
5. 默认 HTTP 迁移为驻留调用，移除旧 Decoder/裸正文入口；X22 旧岛单独隔离。
6. 批量原生/语言/独立产物验收、OOM/寿命和与同版本 C 的成本比较。

## 必须保持的合同

- string/bytes 精确字节长度；size=-1 使用实际长度，超长截断，其他负值拒绝。
- 字段保序、保留重复；名称为 ASCII，值为 bytes，可显式取经过验证的文本。
- 解析的 More、协议错误、资源限制、EOF、取消和超时有明确区别。
- 原生短期视图不逃出调用；语言返回值拥有数据。流式可写入现有 Buffer。
- HTTP/1 正文长度和特殊响应遵循 XRT，不另写一套规则。
- Content Decoder 默认 16 MiB；未知编码默认拒绝，原样输出必须显式选择。
- 网络层解析前不消费输入，保存结果后才前进；保留升级/隧道的后续数据。
- 每个 provider 记录原生模块依赖，HTTP 基础能力不能拉入 TLS/解压/网络。
- 不运行公网探测或网络安全专项测试；只用普通离线向量和自有本地对照。

## 当前状态

已完成批准的 18 个模块与最终隔离候选的双端验收。生产代码固定在
`32d5bcc170b12c0fe92bda9e4464a1ecb50db9a4`，基于
`fe80cbb57d238509dc6475714899030a10a01230`；并行工作区与当前 release
目录不在本轮发布验收内。具体收据、阶段及成本见下文。

首批运行已发现并修正：嵌入/栈 Buffer 回收使用 Unit；可空 owner 的独立
checked 错误通道；Expect/Param 对引号边界及参数片段的不同物理表示；
字段块写出含终止空行而字段迭代不含终止空行的适配。

原生 `xrtHttpContentEncodingWrite(NULL, 0, &size)` 的长度查询缺陷已独立
复现并修复，XRT 提交 `6447bb29b0c822a52ff355f8a9bc9ab067ce3305`。
只同步两个既有 scanner 和这一处函数体，不升级时间 ABI。语言层直接
使用已经计算的 Plan.JoinedSize，避免再次扫描。

## 公开结构

| 能力 | 主要对象 |
|---|---|
| 字段与参数 | HttpHeaders、HttpParameters、HttpTokens、HttpWeightedTokens、HttpList |
| 目标与语义 | HttpAuthority、HttpTarget、HttpExpectations、HttpUpgrades、HttpTe、HttpConnection、HttpTrailers |
| 内容编码 | HttpAcceptEncoding、HttpContentEncodings、HttpContentDecoder、HttpTransferCodings |
| HTTP/1 报文 | Http1、Http1Head、HttpBodyPlan、Http1BodyReader、Http1Message |
| 受管传输 | Http1Limits、Http1TcpTransport、Http1TlsTransport 及各自的 Handoff |

当前 27 个类，319 个精确驻留入口；包含重载、属性、构造/析构和 namespace
别名，不等同于 319 个底层算法。[完整声明](http-language-api.json)记录每项
签名、效果、原生符号与组件；[151 个原生定义角色](http-module-coverage.json)
逐项说明直接调用、原生内部依赖或语言等价形式。这是 API 角色覆盖，不能
当作 151 个函数逐个运行过的执行覆盖率。

[runnable examples](../release/examples/http/README.md)提供离线报文与本机
TCP 示例。传输对象按请求头／正文／升级／交接／关闭／失败管理状态；同一
接收端拒绝并发读取，外部使用者也须让它独占流的接收端。写出仍使用原有
受管流，可与读取并行。HTTP/2/3、高级客户端、路由、连接池、Cookie 等
不属于这 18 个 XRT 模块的能力。

## 生成代码与成本边界

标准声明来自 `tools/gen_stdlib_http.py`，OOP 传输编排来自普通 xlang 源码
`src/stdlib/http1_transport.xl.in`；编译器不生成协议函数体，不使用操作码
分派或动态装箱来隐藏原生签名。驻留调用仍需要语言既定的拥有权、native
字段读取和 checked 错误边界；可空结果有独立错误通道。

- Head 的普通输入路径只解析一次；典型不超过 100 个字段时，原生快照为
  一次分配与一次头部字节复制，重新定位字段视图。超过内联描述符容量才重试。
- string 传入为指针＋精确长度；bytes/Buffer 在调用期间使用实际长度。
  文本 getter 会验证 UTF-8 并复制为语言值，bytes getter 也返回独立值。
- 不可变语义集合需要计数／物化扫描和拥有存储，不能称为零分配或零扫描。
- TCP 收到的拥有片段通过 appendRef 入队；只对跨片段头部／控制行做有界
  合并。正文写入既有 Buffer 一次，丢弃正文直接前进而不复制数据。
- BodyReader 和 Decoder 支持 reset；fixed/none 正文不分配 trailer scratch。
  复用 reader 与预留容量的 Buffer 后，固定正文热路径不再分配原生内存。
- TLS 原生队列只在所属 worker 访问。所属 worker 直接解析；其他线程通过
  已有 TaskNet 机制提交任务，受期限约束等待。它有任务/Future/捕获记录成本，
  不创建辅助线程。普通 Async 传输使用已有 Future continuation。

性能结果必须分别对照同版本、同编译器的借用 C、拥有 C、驻留适配和完整
语言调用，并列出额外复制／分配／扫描／调用／同步／生命周期成本。单个
native 微基准不能代替完整 xlang 的成本，更不能推导网络吞吐或安全认证。

## X22 边界

旧 WebSocket 升级仍需要其旧 Http1 物理布局；该声明岛保持隔离，不能与
V2 对象混用。已删除无调用方的旧 HTTP Decoder 生成代码及默认公开面；
默认 Http1Body、HttpDecoder 与 nativeHandle 入口拒绝编译。旧 WebSocket
岛在 X22 拥有图重构时一起移除，不能据此声称全仓 HTTP Cgen 遗产已清空。

## 协议依据

字段顺序与重复值依据 [RFC 9110](https://www.rfc-editor.org/rfc/rfc9110.html#name-field-order)，
HTTP/1 framing、chunked、EOF 和连接交接依据
[RFC 9112](https://www.rfc-editor.org/rfc/rfc9112.html#name-message-body-length)。
算法仍由固定 XRT 承担；本轮测试是正常功能与绑定/生命周期验收。

## 最终验收与命令

Windows 146 阶段、Linux 145 阶段全部通过：16 个普通用例、5 个合同／旧
入口负例、JIT、真实模块、独立重定位 AOT、导入检查、纯 HTTP 裁剪，以及
严格生成 C 的 O0/O2 执行。38 个驻留 provider 的严格 C 检查也通过。

每个实际 runtime/compiler 产品的 19 类原生资源，共 44 个完整构造分配
前缀通过；头字段替换的事务性通过。完整语言入口 O0/O2 各 315 个失败
前缀以及首个无失败前缀均通过，错误状态、释放数量和无效释放计数平衡。
fixed 正文 reader 与预留 Buffer 的热路径在 fail-after-zero 下未分配。
原生长度查询修复另在 Windows 双 DLL、Linux 双 SO 中确认。两端 parse／
TCP 示例和既有 Windows WebSocket core 用例通过。

复现方式（先导出全新源码目录，避免覆盖正在使用的产品）：

```text
python tools/gen_stdlib_http.py --check
python tools/http_module_coverage.py --out roles.json
python tools/build_stdlib_windows_shipping.py --root <fresh-source> --out <fresh-report>
python tools/build_stdlib_linux_shipping.py --root <fresh-source> --out <fresh-report>
python tools/check_stdlib_http.py --root <built-source> --out <fresh-report> --jobs 2
```

[机器可读收据](http-acceptance-2026-10-28.json)固定报告路径与 SHA-256。
原始首轮、开发诊断和失败配方保留；最后测试使用完整普通产品和嵌入 VFS，
未用替代 provider 或宿主源码路径给隔离编译补洞。首轮双端各 146 阶段
通过后，统一加入无正文消息免建 BodyReader 与不可变标量 getter 的
`pure + no-raise + nonblocking` 合同，再进行上述最终验收。

## 性能实测与下一层成本

Windows 本机顺序采样，20,000 次 58 字节头部；同一普通产品、同一 XRT，
原生比较使用与驻留 provider 一致的 TCC。样本报告 memory-debug=1。
数字为五次中位数；它们是本机微基准，不是网络吞吐或启动时间。

| 路径 | 20,000 次头部 | 20,000 次 64 KiB 正文写入（共 1.31 GB，缓冲复用） |
|---|---:|---:|
| C 借用解析 | 10.164 ms | 不与拥有输出混比 |
| C 拥有结果 | 18.525 ms | 30.041 ms |
| 驻留适配 | 17.779 ms | 30.249 ms |
| 完整 xlang JIT | 135.637 ms | 50.204 ms |

原生适配与等价拥有 C 的成本接近；完整 xlang 的头部微调用仍有明显对象
包装、native 字段查找、checked 算术和生命周期管理成本，不能称作零桥接
开销。此次只消除不必要的库层分配与错误合同；五次顺序前后采样没有形成
稳定的整体语言 CPU 提速结论。后续若继续降低这部分开销，应在统一 IR／
通用 native class 表示中解决，不能在 HTTP Cgen 内另加专用补丁。

源码层面已确认：普通头部原生快照一次分配、一次头部复制；输入不经过
strlen；正文可直接写既有 Buffer、无复制丢弃、固定正文复用零原生分配；
无正文或升级不创建 BodyReader。集合物化扫描、文本验证／复制、语言
包装、mutation gate、错误边界、TLS 跨 worker 任务等成本仍如上文披露。

## 下一轮未决策

CRT 0 项；XRT 10 组：X01 核心／错误／所有权、X02 内存与池、X07 动态值
与值图、X10 剩余平台信息、X22 WebSocket、X25 类型与转换、X26 对象／
字段／对象图、X27 调用和值桥接、X28 typed 容器、X29 内部支撑。内存池
保持最后安排。X21 已转为已开发的批准角色；高层扩展与 X22 边界仍单列。
