# XRT 标准库完整接入任务

## 1. 目标

并发队列的真实双产品角色、拥有传输与完整线程局部分配前缀见
[并发队列真实产品验收](stdlib53-b335-queue-product-stress.md)。
生产实现不变，SPSC／MPSC／MPMC／WaitQueue 的专项已实际执行；
调度器全线程同时故障和整个 53 模块完成审计仍独立保留。

OrderedMap 的 C ABI 标量、bytes 键与泛型逻辑合同见
[OrderedMap 原生键域与泛型合同](stdlib53-b334-ordered-key-domains.md)。
复用 XRT typed tree，bytes 使用独立按需 codec；统一直接／嵌套类型准入，
不在 Cgen 生成比较算法。其他键域和完整 53 模块验收继续。

不可空队列的声明初始化与 OrderedMap 字符键合同见
[队列初始化与字符键排序](stdlib53-b333-queue-initialization-char-keys.md)。
按用户决定拒绝无初始化的不可空队列声明，nullable 队列保留延迟创建；
字符键复用原生 char 比较，不在 Cgen 添加比较器。

原生容器公开面与默认构造见
[原生容器公开面与默认构造验收](stdlib53-b332-native-public-surface.md)。
按用户决定要求显式正容量，修复 SlotMap 默认 producer／错误边与 Array
区间删除合同；新增专项及公开面核对进入产品总门，完整目标继续。

内部容器的实际依赖与未使用实现裁剪见
[内部容器依赖与原生裁剪验收](stdlib53-b331-internal-container-closure.md)。
六项纯内部待验收已有双平台实际产物和对象证明；typed Stack 不依赖旧 XRT
stack 系列。没有新增公开类型或改动生产实现，完整目标继续。

任务线程的阻塞终结及真实产品迁移见
[任务线程阻塞终结与真实产品验收](stdlib53-b330-task-worker-quiescence.md)。
已修复 wait 被忽略的问题；完整执行人口与五条真实故障前缀在双端实际产品通过，
非阻塞退役合同不变，53 模块覆盖表与最终发布条件继续收敛。

拥有返回、字符串槽、Tree 查询及 SlotMap 回滚的后续修复见
[拥有值与槽复制的工作线程错误隔离](stdlib53-b329-owned-return-worker.md)。
双平台新产品的十组完整 worker 故障前缀已通过；不外推为调度器全局 OOM、
OS join 或完整发布通过，其他目标继续。

通用 NULL／显式状态返回的首批隔离修复及真实产品故障复测见
[普通失败返回与外层回调的隔离](stdlib53-b328-checked-return-isolation.md)。
每端共享产品失败由 40 降至 20，另五组仍须修复；真正逃逸的回调与析构首错
继续保留，工作线程及完整发布门禁不结项。

工作线程故障门已发现已捕获错误污染线程退出的核心缺陷，见
[工作线程已捕获错误的传播缺陷与验收门](stdlib53-b327-worker-error-boundary.md)。
双平台新门禁保持失败，普通返回／void 回调／清理的边界待修复；历史产品总门
通过不代表该路径已验收，本批不新增公开能力。

终端原生账本已迁到真实共享产品，见
[终端共享产品的完整资源账本](stdlib53-b326-terminal-product-ledger.md)。
观察器只读取匹配快照的私有布局，不复制运行时实现或增加公开 API。

终端支持策略已按用户决定收敛为系统后端与应用就绪协议；见
[启动期 resize 的应用就绪合同](stdlib53-b325-terminal-readiness.md)。
这是明确平台支持边界，不是修复系统 ConPTY，其他发布目标继续。

最新普通 CLI／DLL／SO、私有 SDK 和独立 AOT 的验收口径见
[真实产品与独立程序的标准库验收](stdlib53-b324-current-products.md)。
现有夹具完整执行不等于全部 53 模块的函数、分支与故障路径验收完成；
剩余发布目标和对象池最后的顺序不变。

同步 IO 和 File 流回调已迁入驻留生产单元，见
[IO 的驻留实现和文件流回调](stdlib53-b309-resident-io.md)。
内建流增加精确拥有策略；进程来源仍依赖共享 Future 的真实
传递边，不以叶子兜底，任务组及原发布目标继续。

标准 VFS 和派生资源的驻留迁移见
[VFS 的驻留适配与资源清理](stdlib53-b308-resident-vfs.md)。
原先入口清单剩任务组和 IO reader、writer 三项未准入，跨模块
Future、完整 exit、终端与发布总门继续。

ProcessRunOptions、Random 和 File 的驻留清理进展见
[取消选项和基础资源的驻留清理](stdlib53-b307-cancel-rng-file-owners.md)。
可达原生待审入口由九项降至六项，完整发布目标继续。

22 项原生析构入口的布局审计、精确拥有边与双端 AOT 验收见
[原生资源的精确拥有边与销毁](stdlib53-b306-native-resource-graphs.md)。
31 项入口中的另九项、终端竞态、完整 exit 及发布总门继续。

clear 续体独立 AOT 的原生池空闲检查修复及精确移植范围见
[任务池空闲线程的拥有图准入](stdlib53-b305-task-pool-idle.md)。
确定性原生回归与真实产品验收分别记录，完整三组目标不缩减。

Context／Buffer 的驻留拥有合同、嵌套 SDK 修复及 31 个可达原生
入口审计清单见
[原生 Context 和 Buffer 的驻留拥有合同](stdlib53-b304-xlang-context-policy.md)。
完整三组目标不缩减，未知原生拥有关系仍拒绝准入。

对象图策略已拆为独立能力并接入 JIT，按需部署元数据见
[对象图退役的精确策略目录](stdlib53-b302-ownership-policy-catalog.md)。
独立 main 已接入共同原生驱动，源码 AOT 及读取合同见
[独立程序入口与对象图退役](stdlib53-b303-static-entry.md)。
完整产品及三组总验收继续，Windows 终端失败仍未关闭。

独立模块共同原生拥有记录与 checked 退役驱动见
[独立模块拥有记录与可重试退役底座](stdlib53-b301-static-module-owner.md)。
该底座现由声明化部署入口使用，发布总验收继续。

动态方法运行时默认表达式降为独立 checked IR 程序，不在 Cgen
解析源码。接收者、前序参数、拥有结果及失败合同见
[动态方法默认值的 IR 执行合同](stdlib53-b288-runtime-method-defaults.md)。
普通 xcall／匿名闭包默认值已按
[动态函数默认参数合同](stdlib53-b289-runtime-callable-defaults.md) 接入；
固定签名 typed／nullable 函数参数见
[动态调用的函数参数合同](stdlib53-b290-runtime-callable-func.md)；
变长函数反向展开与 nullable 动态方法回调已按
[可变参数展开与可空方法回调的 IR 合同](stdlib53-b291-variadic-nullable-functions.md) 接入。
异步动态返回的精确 Future 合同见
[动态调用的 Future 合同](stdlib53-b292-runtime-futures.md)，本批冻结源码三通道通过；
异步多返回值按用户决定继续禁止，留作后续语言设计，不属于本轮
标准库阻塞。其余动态／Native 组合及完整目标继续。

动态方法共享标量 codec、可空载体、原生纯匹配及单声明绑定身份见
[动态方法的精确标量与绑定签名合同](stdlib53-b293-method-scalars.md)。

`c_int(var)` 等显式平台数值构造和精确 nullable 投影已接入统一
checked IR；静态／动态使用同一安全转换，范围与专项证据见
[C 标量显式转换与精确投影](stdlib53-b294-native-scalar-conversion.md)。
反射令牌的 UInt 载体与数值语义已分离，规范身份及 V2 checked
入口见 [动态数值构造的反射身份边界](stdlib53-b295-numeric-reflection-boundaries.md)。
泛型 C 构造在具体实例化后重新验证，字段默认值与声明共用零值域；
整批 JIT／模块执行及剩余范围见
[泛型 C 数值构造与原生字段合同](stdlib53-b296-generic-scalar-contracts.md)。
已证明安全构造的 IR 优化及完整发布总验收继续，不扩大隐式 C ABI
转换或异步多返回值的设计范围。

整数转换常量默认值的阶段合同与 File 动态 map／lock 见
[整数转换默认值的统一 IR 合同](stdlib53-b287-integer-defaults.md)。
运行时默认表达式、异步动态返回与发布总验收仍在完整目标内。

动态 bytes 默认值的声明合同、临时 owner 和实际进程接入见
[动态调用的二进制默认值](stdlib53-b284-bytes-defaults.md)。综合
Native／容器场景已纳入回归，其他动态编码与完整目录仍须验收。

队列／Channel／SlotMap／同步与线程的资源引用投影及 `as float32`
无损合同见 [资源引用投影与 float32 无损转换](stdlib53-b283-resource-domains.md)。
资源见证只创建元数据，成功仍保留原对象身份；工作线程故障、其他
动态／Native 编解码与发布总验收继续。

Array／Stack／List／Set／Dict／OrderedMap 已共用完整原生动态投影
IR 合同。List、Set、Dict 与 Tree 复用共享 checked 事务，提供方独立
选择；见 [原生容器的统一动态投影](stdlib53-b282-container-projection.md)。
其他资源转换、完整 Native 编码和发布总验收继续。

Array／Stack 的动态投影已接通精确 owner、角色、布局和完整元素策略，
不复制或重标记源对象；nullable 错配与真实失败明确区分。见
[Array 与 Stack 的动态类型投影](stdlib53-b281-sequence-projection.md)。
List／Set／Dict／Tree 等其余容器转换和发布总验收仍在完整目标内。

既有 Stack／Sequence 驻留接口的精确归属及源码构建绑定见
[驻留导出归属与源码构建校验](stdlib53-b280-resident-registration.md)。

typed nullable callable 的基础标量、字符串和 bytes 编解码合同见
[Nullable callable 的存在性与载荷合同](stdlib53-b278-nullable-callable.md)。
存在性独立于 false、零值和空容器。动态 float32 转换与直接参数
装箱的修复合同见 [动态标量装箱与 float32 显式转换](stdlib53-b279-scalar-conversion.md)。
完整容器转换、其他动态／Native 编码组合和发布总验收继续。

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
- `WaitQueue<T>`：可阻塞和超时等待的 MPMC 队列，允许多个消费者并行等待和消费；接收返回 `(status, T?)` 拥有值（2026-10-14 用户确认）。
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
