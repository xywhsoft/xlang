# XRT 标准库完整接入任务

## 1. 目标

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
- `TcpStream` 补齐 pause/resume、vectored/zero-copy send 和 readable/writable/drain/close 等待。
- HTTP 客户端补齐可复用 `HttpClient`、连接池、异步请求、流式响应、取消和 Header 枚举。
- HTTP 服务端补齐原始 connection/response 能力，但高层业务继续优先使用 `http.server`。

### 2.4 原始 TLS

- `TlsConfig` 表示证书、私钥、验证策略和协议配置。
- `TlsSession` 表示独立 TLS 状态机，提供 handshake、feed、read、write、pending、close 和 session resume。
- `TlsStream` 将 `TlsSession` 与 `TcpStream` 组合为易用同步/异步安全流。
- P-256 调试 API 不进入标准库。

### 2.5 Regex、Crypto、Encoding、JSON/XSON

- Regex 补齐捕获组、命名捕获、编译错误位置/消息、builder、clone 和 RegexSet captures。
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
- `WaitQueue<T>`：可阻塞和超时等待的 MPSC 队列。
- 内存池、BSMM、MemUnit 和裸 pointer array 只进入 `xrt` 低层模块，不作为默认安全标准库类型。

专用结构必须保存完整类型描述并正确执行值的 retain/release；不允许将 `xvalue*` 当作无所有权裸指针塞入队列。

## 3. 验收标准

- 每个公开函数和方法都有成功、失败、空值、边界值和重复释放测试。
- 异步对象覆盖完成、取消、超时、对象先释放和进程退出清理。
- 多线程结构覆盖生产者/消费者竞争、close/drain、满队列和空队列。
- TLS 覆盖内存握手、TCP 回环、错误证书、截断数据、重复关闭和 session resume。
- C 与 xlang 范例实现相同场景并校验相同结果。
- 标准库依赖裁剪、DLL 符号、XAPI、`xl run`、`xl build` 和完整 RC0 守门全部通过。
