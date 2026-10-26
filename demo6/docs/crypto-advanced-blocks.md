# AES 单块与独立 Poly1305

本批按用户确认，将两项原语放入显式 `cryptoAdvanced` 标准库命名空间。
普通应用仍优先使用 `AesGcm`／`ChaCha20Poly1305`；不增加默认 `crypto.aes`
或 `crypto.poly1305` 别名。沿用现有标准库的按使用注入声明机制，不增加
新的语言 import 语法，也不让默认哈希或 AEAD 依赖这些新适配单元。

## 公开面

```text
cryptoAdvanced.aes(bytes key) -> AesBlockCipher
AesBlockCipher(bytes key)
  encryptBlock(bytes block) -> bytes
  decryptBlock(bytes block) -> bytes
  encryptBlockBuffer(Buffer block) -> bytes
  decryptBlockBuffer(Buffer block) -> bytes

cryptoAdvanced.poly1305(bytes key, string|bytes data, int size = -1) -> bytes
cryptoAdvanced.poly1305Buffer(bytes key, Buffer data, int size = -1) -> bytes
cryptoAdvanced.poly1305Stream(bytes key) -> Poly1305
Poly1305(bytes key)
  reset(bytes key)
  update(string|bytes data, int size = -1)
  updateBuffer(Buffer data, int size = -1)
  final() -> bytes
```

同步只读 bytes 参数另有现行内部 C 互操 `cbytes` 路径；不引入语言层借用、
byref 或裸输出指针。命名空间工厂直接声明为驻留创建符号，不生成语言
转发函数体。所有结果为独立拥有的 bytes，输入不被修改，也不逃逸。

AES 密钥恰为 16／24／32 字节，对应 AES-128／192／256；输入块恰为 16
字节。这是固定大小的单块原语，不做填充、不选择模式、不生成 IV、不提供
认证，不能把它当作通用消息加密接口。固定密钥和固定块不采用 size 截断。
原生展开密钥由对象持有，创建后不可变，可只读跨线程使用；析构擦除。

Poly1305 密钥恰为 32 字节，标签为 16 字节。消息按真实字节长度处理，
内嵌 NUL 保留；size=-1 表示全部，非负值钳制到实际长度，小于 -1 checked
报范围错误。该限制只用于消息，绝不截断密钥。Buffer 同步消费，不经过
临时 Buffer 或文本编码转换。
共享可变 Buffer 的并发访问仍由调用方同步；这些同步入口不声称能安全地与
另一线程的 Buffer 扩容／修改并行，也不为此暗中增加整份输入快照。

**Poly1305 是一次性密钥认证原语。不同消息必须使用新的完整一次性密钥。**
`reset(key)` 要求显式提供新密钥，不保存初始密钥，不提供无参 reset 或 clone。
运行时无法证明调用方送入的密钥确实全局唯一，不伪造此保证。
`final()` 严格沿用 XRT 的非破坏性状态快照；重复读取同一结果不改变状态。
这并不授权同一密钥认证不同前缀／消息；不得将多个前缀快照用作独立标签。
分块对象沿用现行可变摘要的创建线程约束，跨线程方法调用 checked 拒绝；
自动资源释放不要求用户再返回创建线程，不会保存语言闭包或代码地址。

## 实现与成本边界

只调用固定 XRT 的 AES Init/Clear/Encrypt/Decrypt 和 Poly1305
Init/Update/Final/一次性接口；没有算法重写，也没有 Cgen 操作码分派或函数体。
IR 显式登记结果 owner、参数 owner、native handle policy 与效果合同。
同步一次性文本／bytes 和不可变 AES bytes 操作为 pure、nonblocking；读取
可变 Buffer 不声明 pure，Poly1305 状态操作不声明 pure。能分配或拒绝参数
的入口不声明 no-raise；只有保留既有错误的析构声明 no-raise。

AES 创建分配并展开一次密钥；Poly1305 对象保存一个状态和创建线程身份，
不保存第二份初始密钥。输出暂存于 16 字节栈空间，再复制到拥有式 bytes；
该发布过程有实际的分配／拷贝。语言资源还使用现行 handle／拥有图／错误
边界记账，可变状态有线程身份查询和管理线程引用；这里不声称零桥接开销，
没有以不同工作负载的计时替代手写 C 同负载性能比较。

Buffer 入口独立为三个编译单元；共享的小输入适配是静态内联，不增加
跨适配层调用。只用 AES bytes 时不应选择 Buffer／Poly1305／AEAD／X.509／
TLS 单元；只用一次性 Poly1305 时不应选择 AES／流状态／线程对象适配。
是否最终成立由实际产物裁剪门检查，不能仅凭声明就作完成结论。

## 验收

可重复入口：`tools/check_crypto_advanced_runtime.py` 与
`tools/check_crypto_advanced_source.py`。仅使用现有 XRT 测试中的正常公开
FIPS 197／RFC 8439 固定材料，以及普通 ABI、长度、线程归属、拥有与分配
失败清理；无网络通信、攻击或密码学安全专项测试。

证据根为 `F:/xlang-validation/five-groups-20261026/p7-crypto-advanced/`。
驻留适配 Windows GCC O0/O2/TCC 与 Linux GCC O0/O2/TCC/ASan+UBSan 已通过；
每配置 192 个分配失败前缀，内存账本平衡。完整源码接入门和普通发布产品
门是不同层次，不能据此关闭五组长期任务。

最终两端完整源码构建分别为 1,295／1,294 TU，无对象覆盖或替代原生 provider；
只动态复用真实 TCC core，不是重新构建普通发布 DLL／SO／VFS。
两端各 40 个源码阶段通过，包含三个正例的 typed／lowered IR、严格 C O0/O2、
JIT／module、无 xl／xlc 动态依赖的独立 AOT、最小裁剪，以及默认命名空间
和无参 reset 的两个语义负例。既有通用 IR 合同门两端各拒绝 1,243 个变异，
每次 C 和头文件均保持空输出；其中包含共存 AEAD 的既有合同，不全是新增
AES／Poly1305 合同，不把它们冒充新增功能数量。

最小 AES 只选两个新增 AES 单元（状态与拥有域），一次性 Poly1305 只选其
单个计算单元；均没有 Buffer、其他 crypto、ASN.1、X.509 或 TLS 适配。
连同驻留门为 100 阶段；独立 reader 最终分别复核 Windows 7,561／Linux
7,573 份输入、对象、日志和产物。最终语言收据为 `public-win-r2` 和
Linux `public-linux-r2`，reader 为 `proof-win-r1`／`proof-linux-r1`。
Linux 原始文件保留在 `/home/ubuntu/xlang-validation/20261026-p7-crypto-advanced/`，
普通驻留门原始目录为同层 `20261026-p7-crypto-advanced-runtime-r1`。

Linux 第一轮两个负例确实在语义阶段拒绝，但脚本误要求 Windows 的诊断
前缀，保留失败记录和旧脚本；最终按各平台实际驱动前缀及精确 stdlib／sem
错误断言，双端整轮复验，没有降低负例标准或改写旧报告。三份 source 冻结
分别保留早期声明、Buffer 依赖登记和最终按单元裁剪状态；只使用 r3 构建。
源码文件在编译前冻结；Linux 本地暂存逐项核对 9,265 个文件及 13 个真实
TCC provider 文件，修正暂存脚本的 Windows 路径分隔符后才开始构建。

本批将角色登记由 195/198 更新为 197/198，唯一尚 pending 的角色为
`tls_verify`。角色覆盖不是总任务完成比例；最新普通发布产品整合、TLS 总验收、
参数化门存量阻塞和五组联合验收仍继续，不能把源码驱动器当作发布产品。
