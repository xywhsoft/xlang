# MQTT 边界压实计划

本批承接扩展库生态压实任务。立项时包已有 MQTT 3.1.1 QoS 0/1 构帧、解析与 TCP 握手，
不是占位包，但当时的离线用例不能证明真实传输可靠。只以 demo6 当前实现与本计划为开发口径。

## 立项时确认的问题（已处理）

- 握手单次 recv 后假定 CONNACK 已收齐；也会吞掉同一次读取附带的后续 PUBLISH。
- 解析未要求 Remaining Length 与输入长度相等，尾随数据被当成 payload。
- SUBACK 少于 2 字节时执行无符号减法；无效输入可能越界、长循环或崩溃。
- 固定头保留位、包编号零值、返回码、主题和 UTF-8 缺少验证。
- 成功与失败分支均有忽略对象插入失败、泄漏解析结果的问题。
- 静态全局 hex 缓冲不具备重入性；1024 字节正文与 512 字节 topic 限制无公开合同。
- 多个 C int 参数与 xlang int64 声明不一致；frameToHex 的 size 与 c_size 声明不一致。
- keepalive=0 被替换成 60，改变了调用者要求关闭心跳的语义。
- 编解码自行实现 hex 和字符串复制，未充分复用 XRT。

## 实施步骤

| 步骤 | 内容 | 状态 |
| --- | --- | --- |
| 1 | 二进制 C 构帧、公开头、精确语言 ABI、XRT buffer/hex/UTF-8 复用 | 已完成 |
| 2 | 严格完整帧解析、增量头检查、错误与 OOM 拥有权 | 已完成 |
| 3 | XRT TCP 逐帧收发、统一截止时间、尾随帧保留、截断/取消后的状态 | 已完成 |
| 4 | C/xlang 双语测试与范例、默认后端与独立 C 验证、官网与范围说明 | 已完成本页限定范围；2026-09-26 验收见下 |

## 边界

1. 保持已有 hex 便捷 API，增加 Buffer 形式供实际传输使用；hex 不再作为 C ABI 的必要中间格式。
2. C 编解码返回调用方拥有的 xbuffer / xvalue；语言层显式接管，不返回可覆盖的全局临时字符串。
3. 分离编解码、解析、网络和语言声明层，使用一套核心规则，不设置旧后端或旧 ABI 回退。
4. 构帧支持现有 CONNECT、PUBLISH QoS 0/1、PUBACK、SUBSCRIBE、PINGREQ、DISCONNECT。
   解析覆盖对应基础帧及 CONNACK、SUBACK、PINGRESP；不宣称完整 Broker、MQTT 5、QoS 2 会话或重连引擎。
5. 解析单帧必须恰好消费输入。网络接收只取本帧所需的字节，不丢弃后续帧。
6. 网络接口以毫秒接收语言参数，转换为 XRT 单调微秒截止时间；握手中的发送和接收共用一个截止时间。
7. 收到半帧后发生超时、断连、取消或分配失败，连接不能继续把余下字节当新包读取，需明确终止。
   未消费任何字节的超时则不能破坏现有连接。
8. 接收默认限制为 1MiB，允许显式设置；超过限制在大块分配前拒绝。完整帧构造仍遵守协议字段长度上限。
9. 不移植或同步开发中的 XRT；复用仓库当前固定快照。协议层可独立供 C 使用。

协议字段依据 [OASIS MQTT 3.1.1 Errata 01](https://docs.oasis-open.org/mqtt/mqtt/v3.1.1/errata01/os/mqtt-v3.1.1-errata01-os-complete.html)。
所有长度与边界检查以协议定义为准，不把正常分段输入误报成坏包。

自动验收入口为 `tools/check_extlib_mqtt.ps1`。它使用固定 XRT 快照执行独立 GCC
编解码、OOM 和本地 TCP 测试，并通过 xlang JIT/AOT 重复语言编解码与 TCP 边界；
不连接公共 broker，也不扩大到重连、持久会话、QoS 2 或 MQTT 5。

## 验收证据

- 正常、空、零字节、UTF-8、大正文与多字节长度；逐字节分包及连续多帧。
- 每种固定头、包编号、长度、返回码的负例，未知帧、截断和多余尾随数据。
- 结构化解析的每个分配位置，失败无泄漏、无双重释放，不返回缺字段的假成功结果。
- 本地真实 TCP 连接：分段 ACK、ACK 与后续消息粘连、断连、超时与取消；不访问公共 broker。
- 生成的 extern 与公开桥接头严格核对；默认 xl run / build、独立 GCC、可用时 Linux sanitizer。
- 两种语言的范例可直接运行，覆盖相同功能并逐行比较结果。
- 记录源文件与构建哈希、命令及输出；当前 scope 以外的会话功能不能用离线通过率替代。

## 2026-09-26 批次收口

本次复用了当前仓库 XRT 快照，没有同步 `D:/GIT/xrt`，没有修改编译器或运行库实现。
本页已确认问题对应的二进制核心、严格解析和逐帧传输已进入 package manifest；
本轮补齐了下列验收与发布材料：

- 修正 includeDirs 的 manifest 层级，登记包内边界测试和 C smoke。
- 核对全部 16 个生成的 xmq_* extern 与公开 C 头，不以手写声明互相印证。
- 半帧取消先观察实际消费，再触发取消；不依赖固定 sleep 猜线程时序。
- 握手覆盖全部拒绝码、非法会话位、错误报文、非最短长度头与后续帧保留。
- 发送覆盖预检查失败后可重用连接，以及背压发送中途取消/超时后的终止状态。
- 接收覆盖 48 个分配注入位置：18 次拒绝、30 次正常完成，分别验证未消费可重试和半帧失败必终止。
  结束后存活分配数量/字节数回到基线，无新增双重释放、非法释放、越界或 use-after-free 计数。
- 原有编解码 OOM 扫描共 896 次：94 次注入拒绝、802 次完成，逐次检查内存计数。
- 补 65535 字节文本字段及越界、最大 Remaining Length、64KiB 二进制正文；随机输入扫测 8192 组。
- 四份可直接运行的范例：`mqtt.xl` / `mqtt_c.c`（15 行黄金输出）和
  `mqtt_tcp.xl` / `mqtt_tcp_c.c`（4 行本地真实 TCP 输出）。不需要公共 broker。
- 完整 API、错误、拥有权、时限、并发约定见
  [发行包 README](../release/extlibs/mqtt/README.md)。
- 官网详情页源位于 `release/extlibs/mqtt/docs/site.html`；
  `tools/generate_extlib_site.ps1 -Package mqtt` 生成官网详情页和旧目录入口，
  不批量覆盖其他包页面。官网目录卡片已链接详情页。

### 固定构建验证

| 平台与入口 | 结果 | 证据 |
| --- | --- | --- |
| Windows x64，`tools/check_extlib_mqtt.ps1` | 36 项门禁检查全部通过；含严格 GCC、原型检查、C/xlang JIT/AOT、两组范例；输入和产品哈希前后一致 | [完整结果](extlib_mqtt_2026-09-26.windows.results.json) |
| WSL Ubuntu x86_64，`tools/check_extlib_mqtt.sh` | 四份 C 测试与两份 C 范例全部通过 ASan/UBSan（启用 leak detection）；源码哈希前后一致 | [完整结果](extlib_mqtt_2026-09-26.linux.results.json) |
| package C smoke | `tools/run_extlib_c.ps1 -Package mqtt -Source release/extlibs/mqtt/test/test_mqtt.c` 通过 | 与上述相同源码；验证 manifest 源与 includeDirs 接入 |
| 文档与网站 | 文档索引门禁通过，详情页与包内源字节相同，本地资源/链接存在 | 仅静态验证，不声称已完成浏览器视觉验收 |

首次扩大的 Windows 验证虽然各检查通过，但并行任务改写了 xl.dll，哈希门禁将该轮判为失败；
没有把它纳入通过结论。待该编译进程结束后重新执行，以上 Windows JSON 才是固定构建的结果。
构建随后若变化，应重新运行门禁，不能沿用旧结果宣传当前产物。

### 仍然明确排除

真实 broker 产品矩阵、公网故障环境、长时会话与 Linux xlang JIT/AOT 尚未验证。
自动心跳、重连、重发、去重、持久会话、Will、UNSUBSCRIBE、QoS 2、MQTT 5、TLS
不属于当前包的实现声明；这些能力不能由本轮编解码与 TCP 测试推导为完成。
扩展库全量压实任务继续进行；本批收口不等于全部扩展库已达到发布质量。
