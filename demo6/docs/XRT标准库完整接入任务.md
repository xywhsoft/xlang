# XRT 标准库完整接入任务

## 1. 目标

动态方法与绑定方法已接通完整多返回值，动态 Stack 的 `tryPop`、
`tryPeek`、`tryPeekAt` 按精确原生操作返回拥有的 `(found, item)`。
2026-10-14 的双平台源码、语言与故障专项见
[动态方法多返回值与 Stack 安全接收](stdlib53-b277-dynamic-method-products.md)。
三分类接入长期任务仍包含 typed nullable callable 编解码、完整容器
转换、工作线程／对象图故障、模块组合和发布总验收；对象池最后。

本批次补齐当前只公开部分能力的 XRT 模块，并新增专用数据结构与原始 TLS 会话。

实现必须遵守以下边界：

- 真实功能首先由 XRT 提供，xlang 只保留类型适配、所有权适配和易用接口。
- XRT API 必须能被 C 独立使用，不引入 xlang 专属语义。
- xlang 不直接暴露需要用户手工释放的裸资源；资源统一通过 native-backed class 管理。
- 同一能力只保留一条正式路径，不保留旧标准库兼容别名。
- 二进制数据统一使用 `Buffer`，文本统一使用 UTF-8 `string`。
- 异步 API 返回 `future<T>`，同步和异步接口使用相同的结果语义。

## 2. 本批公开面

### 2.1 文件与目录

- `Buffer.insert/insertBytes`。
- `file.readBytesAsync/writeBytesAsync`。
- `AsyncFile`：随机位置读写、flush、size、resize 和显式关闭。
- `dir.createAsync/createAllAsync/copyAsync/moveAsync/deleteAsync`。
- 删除生成代码自行运行同步文件操作的重复异步实现，改为适配 XRT future。

### 2.2 进程

- `ProcessCommand` 支持工作目录、环境变量、继承环境、终端模式、窗口模式、stderr 合并、进程组、捕获上限和事件上限。
- `Process` 支持 state、结构化退出信息、interrupt、terminal resize、增量 stdout/stderr/event 读取和异步等待。
- pipeline 使用同一份 `ProcessCommand` 配置，不另建第二套参数模型。

### 2.3 网络与 HTTP

- `NetAddress`、DNS resolve、`NetEngine`、`NetProxy`。
- TCP/UDP 支持完整配置、异步 accept/recv、代理、TLS、流量水位和关闭等待。
- `TcpStream` 已补齐 pause/resume、vectored/move/file zero-copy send 和 readable/writable/drain/close 等待；`sendFileResult` 保留非阻塞背压结果，`sendFile` 对大文件按 `WriteLimit` 分段并共享一个截止时间。
- HTTP/WebSocket 在本轮只接入 XRT 核心快速路径、TLS、升级和压缩链路；客户端池、路由、表单、Cookie 等高级抽象继续由 xhttp/xws 扩展包承担，不复制回编译器标准库。
- TCP 文件发送仅在实际使用 `sendFile*` 时保留 `XRT_MODULE_NET_TCP_FILE` 和文件模块；普通 TCP 程序的生成 C 与裁剪实现不得包含该能力。

### 2.4 原始 TLS

- `TlsConfig` 表示证书、私钥、验证策略和协议配置。
- `TlsSession` 表示独立 TLS 状态机，提供 handshake、feed、read、write、pending、close 和 session resume。
- `TlsStream` 将 `TlsSession` 与 `TcpStream` 组合为易用同步/异步安全流。
- P-256 调试 API 不进入标准库。

### 2.5 Regex、Crypto、Encoding、JSON/XSON

- Regex 的新 ABI 适配器、捕获组、命名捕获、错误位置、builder、clone 与 RegexSet 已作为正式 `xregex` 源码包接回，并进入 Windows/WSL JIT、AOT 与 C smoke 门禁。
- Crypto 补齐 P-256/P-384 ECDH、ECDSA verify、SHA-1 streaming 和 Buffer 二进制接口。
- Encoding 补齐 UTF-8/16/32 显式转换和大小端转换。
- JSON 补齐 SAX/流式输出；XSON 补齐稳定选项对象，不公开裸 flags。

### 2.6 Template、Log、URL、Query、ID

- Template 支持独立 Engine、自定义函数/语句、load/save 和结构查看。
- Log 支持 custom appender、设置默认 Logger 和 appender 配置。
- URL 支持 build、authority/target、default port 和 Host Header。
- Query 支持 count/find/iterate 和 dict 双向转换。
- XID 支持 encode/decode/compare 和结构化对象。

### 2.7 Task/Future

- `TaskGroup` 支持父子组、加入 future、取消、join、超时和回收已完成任务。
- `Future` 补齐 then/catch/finally、调试名、时间信息和统一 wait source。
- 原始 coroutine scheduler 继续作为语言运行时内部设施，不另建第二套公开协程模型。

### 2.8 专用数据结构

- `Stack<T>`：LIFO、peek、push、pop、clear、count。
- `AvlTree<K, V>`：有序映射、set/get/remove/contains、按序迭代。
- `SpscQueue<T>`、`MpscQueue<T>`、`MpmcQueue<T>`：有界无锁队列。
- `WaitQueue<T>`：可阻塞和超时等待的 MPMC 队列，允许多个消费者并行等待和消费；接收返回 `(status, T?)` 拥有值（2026-10-13 用户确认）。
- 内存池、BSMM、MemUnit 和裸 pointer array 只进入 `xrt` 低层模块，不作为默认安全标准库类型。

专用结构必须保存完整类型描述并正确执行值的 retain/release；不允许将 `xvalue*` 当作无所有权裸指针塞入队列。

## 3. 验收标准

- 每个公开函数和方法都有成功、失败、空值、边界值和重复释放测试。
- 异步对象覆盖完成、取消、超时、对象先释放和进程退出清理。
- 多线程结构覆盖生产者/消费者竞争、close/drain、满队列和空队列。
- TLS 覆盖内存握手、TCP 回环、错误证书、截断数据、重复关闭和 session resume。
- C 与 xlang 范例实现相同场景并校验相同结果。
- 标准库依赖裁剪、DLL 符号、XAPI、`xl run`、`xl build` 和完整 RC0 守门全部通过。

## 4. 当前状态（2026-08-22）

在 HTTP/WebSocket 只接入核心快速路径、TLS、升级与压缩链路的既定边界内，XRT、xruntime 与 xlang 的实现对接已经闭环。当前没有旧 API 延期清单，也没有为兼容历史接口保留第二套语言路径。

### 4.1 能力与公开面

- XRT 核心快照包含 `2827` 个公共函数；xruntime 产品快照包含 `3269` 个公共函数，其中 `442` 个由 xruntime 自有模块提供。
- `3269` 个函数全部进入 TCC 底层符号面；当前有 `1093` 个直接实现调用、`1295` 个语言内核或标准库语义抽象，其余能力仍可通过显式 extern 使用。
- 标准库公开 `427` 个函数，显式延期为 `0`；自动化接线覆盖 `74` 个标准库测试文件。
- xhttp、xws、xregex 的旧核心引用均为 `0`。HTTP/WebSocket 非快速路径继续属于扩展包，不回流编译器标准库。
- xruntime 的类型、调用、动态字段、对象所有权和 typed container 已进入语言内核或语言类型；对象图遍历等能力保留为明确的底层 extern。

### 4.2 本轮补强

- 同步最新 XRT/xruntime 单头文件、声明快照、TCC 符号表和能力矩阵；新增的 `xrtNetStreamSendRefs`、`xrtNetStreamSendBuffer` 保留为显式底层能力，xlang 继续用 `sendVec` 表达复制语义、用 `sendMove` 表达所有权转移。
- 补齐 `TcpStream.waitAvailable` 与 `waitAvailableAsync` 生成桥接，并增加同步、异步可读字节边界测试。
- 公开网卡快照、名称/索引转换和接口标志的语言抽象，补充 xlang/C 对照范例及生命周期、裁剪和往返测试。
- 网络统计生成器改为按 engine、resolver、TCP stream、listener、UDP 的实际依赖分别发射，最小 `NetEngine` 程序不再隐式带出 DNS 或传输层。
- 网卡顶层 extern 使用固定宽度 `int64_t` ABI，消除 Windows LLP64 与 Linux LP64 数据模型差异导致的声明冲突。
- `_xge_native` 对 xregex 的真实 C ABI 依赖改为正式包依赖对象，并同步到清单生成器，扩展库依赖闭包重新通过审计。

### 4.3 当前验证

- WSL2 Ubuntu 已完成原生 TCC、XRT、`libxl.so`、`libxlc.so` 和 `release/xl` 重建；`test_linux.sh` 全部通过。
- Linux 门禁覆盖 CLI/VFS、ELF、C/XRT、UTF-8 路径、import/package、xregex、xssh、xmail SMTP/POP3/IMAP 本地回环、async/task、TCP 文件快速路径、网卡快照、HTTP/1.1、WS/WSS 升级与压缩，以及共享库宿主 API。
- Windows 已按最新源码重建 `xl.dll/xlc.dll` 并通过 DLL 专项：NULL context、cwd、线程宿主、模块依赖生命周期、字符串/文件运行、编译器 API 和 XRT 导出均通过。
- 快照、能力矩阵、三种头文件包含顺序、TCC 契约、所有权、xruntime 语言桥接、标准库覆盖、源码编码、文档索引和发布扩展库审计均通过；Windows TCC VFS 资源也已按最新源码重新生成并验证。
- 最新 Windows `xl.exe` 在完整回归中被 Microsoft Defender 机器学习规则隔离。当前源码尚待在可信本地环境补跑 Windows 产品裁剪和最终完整动态回归；不通过恢复隔离文件、关闭防护或添加排除项绕过该门禁。
