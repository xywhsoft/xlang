# TLS 驻留接入

本文件跟踪五组长期任务的 P6，不以单个身份对象的通过声明 TLS 已完成。
协议、证书、签名和加密算法复用 XRT；语言适配负责准确的声明、拥有关系、
预算、错误边界、取消和清理。Cgen 只消费经过 XLIR 验证的调用及清理信息。

## 实施顺序及当前边界

1. 不可变的受管理身份：已有证书与私钥形成自包含快照，精确类型、资源目录
   及按算法裁剪；本批已接入，验收范围见下节。
2. 客户端／服务端选项分离、不可变上下文快照、信任及验证策略；复用 P5 的
   数据和完整链／名称／撤销能力，检查所有原生预算及回调拥有合同。
3. 安全 sans-I/O 会话、明确的读写／握手状态、bytes 输出及硬预算。
4. 受管理 TCP 上的 TLS 流、拨号／监听、同步／Future、期限／取消和真实退役。
5. 角色分离的恢复票据及 KeyUpdate，移除被替代的旧声明和 Cgen 实现，进行
   双平台常规联合验收及发布接线。

以上保持原批准的 47 模块范围；不承诺原生没有完整实现的 mTLS、0RTT、OCSP
或新的密钥生成／签发能力。只使用离线固定向量及受控本机正常功能测试，
不运行网络安全性质专项、扫描、攻击模拟或畸形密码学样本。

## TlsIdentity

八个精确驻留入口提供四种工厂、析构、kind／count 及证书投影：

```xlang
auto key = P256Key(privateScalar)
array<Certificate> chain = [Certificate.fromDer(leafDer)]
auto identity = TlsIdentity.fromP256(chain, key)
auto certificate = identity.at(0)
```

工厂按签名密钥类型命名，不根据字节长度或动态值猜测算法：

| 工厂 | 私钥类型 |
|---|---|
| `fromRsa` | `RsaPrivateKey`，须有完整 CRT 参数 |
| `fromP256` | `P256Key` |
| `fromP384` | `P384Key` |
| `fromEd25519` | `Ed25519Key` |

均接收 `array<Certificate>`，叶证书在前。`maxCertificates=64` 与
`maxCertificateBytes=16777216` 分别限制完整证书数量及 DER 累计字节；负值
checked 拒绝，显式 0 关闭该项预算。预算不截断输入，不是可选 size 前缀。
创建时由 XRT 核对叶证书与私钥，失败不交付半成品。没有完整 CRT 的 RSA
对象仍可用于其已有能力，但这里不会替它推导或伪造缺失参数。

创建后身份不借用原数组、证书或语言私钥。P256／P384 标量和 Ed25519 种子
仅同步传递给 XRT；RSA 使用已有 PKCS#1 格式支撑，临时 DER 按真实 capacity
擦除释放。身份只保存 XRT 的不可变拥有快照，可跨线程读取和供后续上下文
保留原生引用。`at(index)` 返回独立拥有的 Certificate，不钉住整个身份。

`TlsIdentityKind` 保留原生完整观察词汇；枚举中的 p521／ed448 等值不表示本轮
提供了对应的私钥类型或工厂。身份创建和证书读取不是信任验证，不能据此
接受 TLS 对端。当前没有公开 nativeHandle、自定义裸签名回调或协议密钥。

工厂具有 nonblocking、may-raise 合同；只读 getter 及 at 另有 pure；析构为
no-raise、nonblocking。一个真实叶策略和拥有目录管理身份；四个工厂使用共享
的精确 native collection 合同，只同步借用已验证的 Certificate 元素，不增加
Cgen 中的名称分支或算法实现。

旧 TlsConfig／TlsSession／TlsStream 及 Cgen 实现尚未替换，不能把新身份
接线当作这些旧入口已经迁移。完整 TLS、最新发布产品和 P7 验收继续开放。

## 身份批次验收范围

每端 100 个新身份／IR／裁剪阶段及 143 个共享证书数组路径回归阶段通过。
驻留与独立重放为 Windows 12／Linux 15，合计 513；完整源码为
1123／1122 TU，8863 份稳定输入两端一致。8 个目标、5 个拥有结果、4 个
精确数组合同、一个句柄策略、133 个错误合同及 9 个输出拒绝门通过。
实际 JIT／module／严格 C O0/O2／独立 AOT、线程只读及 typed 容器通过；
八份真实裁剪计划确认四个身份算法独立、纯枚举不拉入 TLS 身份实现。

原生适配的四种正常证书及合法九证书链验证小栈／堆视图两条路径；每个
配置覆盖 40 构造预算／35 实际失败、8 发布预算及 8 证书复制预算，真实
目录 Hold／Claim／Restore 与精确内存账本通过。原数组／私钥释放后输出的
28 份普通签名由独立后端核验；从身份复制的证书在身份释放后仍可读取。

第一轮的 AOT 静态叶策略登记遗漏已通过一条元数据补齐，并新增直接检查。
同一输入的旧／新登记 AOT 对照单独保存，不增加上述正式阶段计数。
版本 3 与 ASN.1 标签 2 的夹具错误另以原生产编译器证实；独立核签增强
只替换两份 native 测试控制，所有生产源与编译对象保持第二轮冻结版本。
早期原生夹具缺失构造依赖的链接失败记录也保留，不作为成功阶段。

证据根 `F:/xlang-validation/five-groups-20261024/p6-tls-identity`，最终回执
`verified-commit.json`；输入、日志、产物和重放独立哈希复核，Linux 原件与
逐项校验的归档保留。没有网络流量、畸形密码学样本或原生安全专项，没有
更新发布 DLL／SO／VFS；原有两个 Windows 间歇诊断不因本批通过而关闭。

## 显式验证预算底座（P6 前置）

固定 XRT 的默认建链入口没有工作预算，因此先新增三个原生入口：
`xrtTlsVerifyLimitsInit`、`xrtTlsVerifierCreateLimited`、
`xrtTlsPeerVerifyLimited`。`xtlsverifylimits` 默认限制对端 64 张／16 MiB、
路径 64 张／100000 个确定性工作单位、信任库 4096 张／64 MiB。每项 0
显式关闭；限制使调用失败，不截断证书／DER 或交付部分路径。

信任配额在复制 store 前检查，对端配额在自定义信任决策前检查；接受回调
不能绕过输入配额，但其内部工作不受默认建链预算约束。深度／工作耗尽是
RangeError；完成的默认路径策略拒绝与资源错误分开转换，不能借用旧错误槽
中无关的 cause。创建失败不接管回调 Context；成功后复制配额与信任数据，
最后一个引用释放时恰好清理一次。复制使用已有 XRT StoreClone，避免重复
Add 引入的二次方查重。握手接收／解析前的输入分配仍需上层预算，不能以此
替代后续会话和传输限制。

旧的无限额入口、既有结构体布局和签名保持；新增的只是限额结构体及函数。
独立 XRT 保持毫秒／For，xlang 固定快照保持微秒／Until；仅同步指定公共头
与验证器源区段，未导入其他工作区改动。回溯记录见
`lib/xrt/backports/tls_verifier_limits.json`。

四份快照、3844 项原生符号登记和真实特性根链接目录一致。重建目录还补齐
六个已有 X.509 typed path／store API 的旧静态归属缺项。目录的 426 个特性
根由实际预处理得到；原编译器二分查表函数对全部 3844 个符号、九个新增／
修复归属和 unknown／null 边界通过 O0／O2 双端检查，不增加 Cgen 逻辑。

该批常规验收合计 120 阶段（Windows 52／Linux 68）：原生模块化／单头 42，
四份实现／声明交叉组合 70，实际编译器查表 8。每个快照二进制覆盖四种
正常固定证书、48 个精确边界、24 个实际构造分配失败及内存平衡；原生还
覆盖验证分配回滚和旧错误隔离。GCC O0／O2、TCC，以及 Linux ASan／UBSan
通过，微秒日期合同在实际路径验证中单独断言。只读元数据检查另列，不混入
运行阶段；早期深度错误预期与 Windows COFF 独立探针链接失败保留。

复查入口为 `python tools/check_xrt_tls_verify_limits.py --out <新目录>`。
证据根 `F:/xlang-validation/five-groups-20261024/p6-tls-verify-limits`。
没有网络、安全专项、畸形密码学样本，也未更新发布产品。这三个原生 API
尚不是新的语言级验证策略／上下文公开面；P6 其余步骤与 P7 继续开放。

## 共用协议配置与参数快照

`TlsLimits`、`TlsPolicy`、`TlsParameters` 已通过驻留接入，作为后续客户端／
服务端选项和角色上下文的共用值。三个对象均不可变，可跨线程只读，没有
可见 nativeHandle 或对调用者数组／对象的持久借用。

```xlang
array<TlsVersion> versions = [TlsVersion.v13]
array<TlsCipherSuite> ciphers = [TlsCipherSuite.aes128GcmSha256]
array<TlsGroup> groups = [TlsGroup.x25519, TlsGroup.p256]
array<TlsSignature> signatures = [TlsSignature.ed25519, TlsSignature.ecdsaP256Sha256]
auto policy = TlsPolicy.fromPreferences(versions, ciphers, groups, signatures,
    TlsKeySharePolicy.preferReady)
auto parameters = TlsParameters(policy, TlsLimits())
```

`TlsPolicy()` 保留 XRT 的完整默认排序和 preferReady 策略。
`fromPreferences` 原子校验四个有序列表，不以连串 setter 产生暂时不一致的
版本／套件组合；其 `keyShare` 参数默认 preferGroup，供自定义策略严格保持
组偏好，调用者也可显式选择 preferReady 以优先已有 key share。五个精确
数值枚举共 37 个成员来自一个声明／XLIR 共享清单；观察词汇不等于当前
角色／裁剪后端都能执行，执行能力仍须在角色创建时验证。

四个列表在读取和分配 detached 元素前检查固定数量上限，再由 XRT 校验
完整策略；不截断、不保留数组槽位或语言回调。版本／套件／组／签名的
count 和 at 读取原始顺序，结果保持各自枚举身份。共有 C 整数表示不能
让 `array<TlsGroup>` 冒充 `array<TlsVersion>`。这一点通过通用私有桥接合同
表达，不在 Cgen 按函数名推断。

`TlsLimits` 的 feed／send／plain 默认各 262144 字节，handshake 默认
1048576，record／handshake 驱动预算默认各 64。所有值检查 native 宽度、
合法记录最小尺寸及握手线路上限；这里的 0 不是“无限”，非法值 checked
拒绝。`TlsParameters()` 使用完整默认值，双参数构造把已有策略和限额
复制进 XRT 的一次紧凑分配；构造本身不建立连接，也不预分配这些队列。
`policy` 保留不可变原生引用，`limits` 返回独立副本，均能晚于来源对象释放。

三个真实叶策略／拥有目录及对应静态登记齐全。25 个驻留目标、7 个拥有
结果、4 个精确枚举数组合同、3 个句柄策略、210 项错误合同和 28 个零输出
拒绝门通过。每个原生配置覆盖 16 个分配预算／10 个实际失败、6 个发布
预算及三个目录的 Hold／Claim／Restore，临时值和内存账本平衡。

本批共 148 阶段：Windows 73、Linux 75；包含实际 JIT／module／独立 AOT、
生成 C 的严格 O0／O2、跨线程只读和 typed 容器、旧 string 数组／内嵌 NUL
回归、四份实际裁剪计划及错误枚举数组源拒绝。驻留实现另用 GCC O0／O2、
TCC 和 Linux ASan／UBSan；纯枚举不拉入 TLS，配置不拉入密码学／会话／
传输。共享清单的全部 37 个枚举数值另与真实 XRT 常量逐项编译核对。
源码驱动分别构建 1130／1129 TU，8893 稳定输入在两端一致，
另列于阶段之外，不表示发布 DLL／SO／VFS 已验收。

首轮公共 ABI 头引用未发布的内部 `.inc`，已改为自包含声明；原失败与修复
源码保留。夹具的默认 key-share 预期更正为原生 preferReady，同一生产编译器
已跑通更正的夹具；最终控制不改变生产源码／对象。早期未完成快照拷贝时的
启动失败也保留，不计作产品或成功阶段。

复查工具为 `tools/check_tls_parameters_owned.py` 与
`tools/check_stdlib_tls_parameters.py --source-build <build.json> --out <新目录>`。
证据根 `F:/xlang-validation/five-groups-20261024/p6-tls-parameters`。本批无网络
或安全专项，未增加 Cgen 语义特判，未修改 XRT 或两边时间 ABI。分离角色
选项／上下文、信任及 CRL／回调策略、会话、传输、恢复和 P7 仍待完成。

## 默认信任验证器与验证策略

`TlsVerificationPolicy` 管理前述六项验证预算，默认有限、负值拒绝、显式 0
关闭对应配额；`allowSha1=false` 是默认值，兼容旧链须显式启用。策略不可变，
`withTime(time)` 返回固定 UTC 微秒时钟的新策略，`withCurrentTime()` 返回
使用每次验证当前时刻的新策略，不改变来源对象。`usesCurrentTime` 可检查
时钟模式；没有固定时刻时读取 `fixedTime` checked 报 RuntimeError（原生
XERR_STATE），而不伪造 0。
固定 epoch 0 是合法时间，不表示“当前”或“缺省”。

```xlang
auto policy = TlsVerificationPolicy().withTime(time(1893456000000000))
auto verifier = TlsVerifier(store.snapshot(), policy)
verifier.verifyServer(certificates, "localhost")
```

`TlsVerifier(TrustStoreSnapshot)` 使用默认策略，双参数构造使用显式策略。
创建时复制完整信任数据和策略；空信任库 checked 拒绝，失败不交付半成品，
不回退到未验证模式。`fromSystem()`／`fromSystem(policy)` 显式装载系统快照，
沿用已有 P5 系统信任库入口及其失败合同，不标注 nonblocking。
来源 snapshot／store／policy 可以随后释放；`verifier.policy` 返回独立策略副本。

固定时钟的数据由原生验证器自己的 Context 拥有，最后一个原生引用释放时
清理，不指向语言包装或调用栈。已有原生消费者保留验证器后，即使语言包装
已释放，时钟和信任快照仍有效；这为后续角色上下文／会话提供真实生命周期，
不靠保留无关的语言对象掩盖原生借用。

`verifyServer(array<Certificate>, string name, size=-1)` 与
`verifyClient(array<Certificate>)` 是同步的离线 checked 校验，不建立连接。
XRT 执行完整建链、时间、digitalSignature 用途及 serverAuth／clientAuth，
服务端对端另匹配精确 DNS／IP 名称；不做 DNS 查询。前者的 size 按实际
UTF-8 字节长度取前缀，超长取实际长度，其他非法负数拒绝，选中的名称不能
为空或包含 NUL。证书数量／DER 字节预算不截断输入。

这两个方法无 bool 返回：失败保持结构化原因，配额和内存错误不能被混同
成普通“不可信”。捕获后得到稳定的证书拥有引用，原生连续视图只同步借用；
小链使用栈视图，较大合法链使用检查溢出后的堆视图，错误和 OOM 都完整回滚。
手动 verifyClient 不表示服务端握手已经支持 mTLS。

纯策略组件不选择 TLS／X.509 算法；创建验证器与实际验证分属不同组件，
后者才按合同引入 RSA／ECDSA／Ed25519 验证后端。两个新的真实资源策略与
目录使用通用 NativeHandle 体系；证书数组使用已有密封的精确类数组合同，
没有添加 Cgen 名称分支。完整 CRL／附加回调政策及角色／会话／传输接入
仍在 P6 后续范围，本节不是它们已经完成的声明。

### 验证器批次验收

本批冻结 8916 份稳定输入；Windows／Linux 完整源码驱动分别构建
1136／1135 TU，另列于验收阶段之外。共 276 阶段：Windows 137、Linux 139；
含四种正常证书族的服务端／客户端离线校验、合法九证书链、独立拥有和线程
只读、JIT／module／独立 AOT、严格生成 C O0／O2、旧参数及字符串数组回归。
19 个驻留目标、6 个拥有结果、2 个精确证书数组合同、2 个真实资源策略、
174 个错误合同及 21 个零输出门通过；十份实际裁剪计划覆盖纯策略、仅创建
及实际验证，
不因声明验证器而引入 TLS 角色、会话或网络传输。

驻留实现另经 GCC O0／O2、TCC，Linux 再加 ASan／UBSan。每个配置覆盖
60 个构造／复制预算、16 个验证预算、56 个合计实际分配失败、4 个发布预算；
两个真实目录的 Hold／Claim／Restore 及精确内存账本平衡。释放语言包装后
仅保留原生引用仍能使用固定时钟完成校验；固定 epoch 0 和当前时钟模式均
显式检查。正常证书的签名由独立后端核对，不运行网络／安全专项。

首轮测试使用不存在的 StateError，现改为既有 RuntimeError；TCC 不支持的
测试视图数组推断改为定长数组与赋值。这些仅是测试控制修正，不改变冻结的
生产代码或完整源码对象；前后轮次和原始失败全部保留。
证据根 `F:/xlang-validation/five-groups-20261024/p6-tls-verifier`，复查工具为
`tools/check_tls_verifier_owned.py` 及
`tools/check_stdlib_tls_verifier.py --source-build <build.json> --out <新目录>`。
本批不改 XRT 核心、两边时间 ABI 或 Cgen，不表示最新完整工作区／发布产品
或整个 TLS 已验收，也不关闭历史 Windows 间歇诊断。

## 客户端／服务端不可变角色上下文

共用 `TlsParameters` 保存协议偏好和队列限额；角色自己的配置分别放在
`TlsClientContext`、`TlsServerContext`，不以一个通用配置袋混合两种职责，
也不再增加仅重复相同不可变内容的 Options 包装。

```xlang
auto client = TlsClientContext(TlsVerifier.fromSystem())
client = client.withProtocols([b"h2", b"http/1.1"])

auto server = TlsServerContext(identity)
server = server.withProtocols([b"h2", b"http/1.1"], true)
```

客户端构造必须提供 `TlsVerifier`，没有不验证的缺省或回退；服务端必须
提供 `TlsIdentity`。双参数构造先接收显式的 `TlsParameters`，再接收对应
角色材料。单参数构造采用默认参数。上下文可跨连接复用，不发起网络、
不创建会话、不产生握手随机数；它不是握手已完成或指定算法组合已可执行
的证明。实际角色创建还必须通过 XRT 对完整线路与执行能力的 checked 校验。

两者都有 `parameters`、`protocolCount`、`protocolAt(index)` 和
`withParameters(parameters)`；客户端另有 `verifier`／`withVerifier`，
服务端另有 `identity`／`withIdentity` 和 `requiresProtocol`。所有 with 方法
返回新上下文，不修改原对象。参数／验证器／身份 getter 都返回独立拥有的
原生引用或包装，能够晚于上下文及其来源对象释放。

ALPN 是不透明协议标识，不是 Unicode 文本或逗号分隔字符串。`withProtocols`
接收精确的 `array<bytes>`，每项 1—255 字节；包含两字节列表长度的完整
ALPN extension_data 不超过 65535 字节。名称可含 NUL／非 UTF-8 字节，不做
转换、截断、去重或隐式大小写折叠。完全相同的名称 checked 拒绝；临时排序
与相邻比较只用于重复检查，公开的偏好顺序不改变。数量在 detached 元素
存储分配之前检查，字节总额在协议数据分配之前检查。

`protocolAt` 返回独立 bytes 值；修改它不影响上下文。空数组关闭 ALPN。
服务端 `withProtocols(protocols, requireProtocol=false)` 将偏好与必需标志
一起原子创建；要求协商却给空列表时拒绝，原对象不变。客户端列表只是
offer，不额外暗含“必须选中一个”的未声明行为。完整 ClientHello 的其他
扩展仍会占用线路预算，创建真实会话时不能跳过原生的整体长度校验。

SNI／验证名称属于每次连接，不绑定在可复用上下文中。动态身份选择、恢复
回调和 CRL 回调的拥有合同仍须后续接入，不能把含语言闭包的对象伪装成叶。
本批两种目录是真实叶：只持有不可变原生对象与私有协议字节，没有语言引用、
调用方视图或模块回调。默认构造不引入协议数组读取组件，只有 withProtocols
选择该组件；客户端不因声明上下文引入服务端身份算法，反向亦然。

### 角色上下文批次验收

冻结 8933 份稳定输入，Windows／Linux 完整源码驱动分别构建 1144／1143 TU。
正式阶段共 290：Windows 144、Linux 146；每端 129 个语言／IR／裁剪阶段，
6 个独立私钥保留证明，驻留分别 9／11 阶段。源码构建与额外核对不混计。
实际 JIT／module／独立 AOT、严格 C O0／O2、四类正常固定密钥证书、跨线程
只读和 typed 容器、旧参数／验证策略／字符串数组回归均覆盖。

21 个驻留目标、14 个原生拥有结果、2 个 bytes 结果、2 个精确 bytes 数组
合同和 2 个真实资源策略通过；228 错误合同、17 个相同物理表示的角色交换、
37 个模块及 C／头零输出拒绝门通过，十份实际裁剪计划保持角色与算法分离。
每个驻留配置覆盖 152 构造／复制预算、100 实际失败、16 发布预算及真实
目录 Hold／Claim／Restore；GCC O0／O2、TCC 和 Linux ASan／UBSan 内存平衡。

原身份和上下文都释放后，保留的身份仍能执行正常签名；两端 O0／O2 的
16 份签名由独立后端验证，不能仅以 count／非空签名代替私钥拥有证明。
同一证明同时覆盖“空 ALPN 却要求协商”的 checked 拒绝及原上下文不变。

测试夹具的数组函数名和与 bytes typedef 冲突的计数器已修正，生产代码及
完整源码对象不变。每端复用 123 个未受影响阶段，只补跑合同矩阵；原失败、
300 秒诊断轮次与后续逐目标进度全部保留，不删负例或输出门。默认复查工具
仅对 contracts 阶段采用 900 秒上限，其他阶段保持 300 秒。工具为
`check_tls_role_context_owned.py`、`check_stdlib_tls_role_context.py` 和
`check_tls_role_context_key_retention.py`，证据根
`F:/xlang-validation/five-groups-20261024/p6-tls-role-context`。

本批没有修改 Cgen、XRT 核心或时间 ABI，没有网络／安全专项。角色上下文
和默认验证器已经接线，不代表动态选择／CRL 回调、sans-I/O 会话、TLS 传输、
恢复／票据／KeyUpdate、旧后端清退或发布产品已经验收。P6／P7 继续，历史
Windows 间歇诊断不因本批通过而关闭。

### 全源码验收驱动的显式优化

后续批量验收可以显式选择全源码 O2：Windows 构建工具使用
`-Optimization O2`，Linux 使用 `--optimization O2`。默认仍是 O0，非法档位在
创建输出前拒绝；不是替换若干对象，也没有复用旧测试结论。构建清单记录总
编译选项和每个 TU 的实际选项／命令，缓存继续按实际参数、预处理输入与
工具链区分对象。Windows 的 Complete／InputsUnchanged 只在构建、链接和
最终输入核对成功后写入。

本次冻结 8941 份输入，其中 2168 份生产源码／头输入与上一批逐字节一致。
两端全部 1144／1143 TU 均采用 O2，重跑各 129 个语言／IR／裁剪阶段和
各 6 个私钥保留证明，共 270 阶段、16 份独立验证的正常签名；不复用之前
阶段结果，生成 C 仍分别接受 O0／O2 检查。Windows 相同完整合同矩阵从
约 353—360 秒缩短到 97—100 秒，约为原来的 28%，没有删减 228 个错误
合同或 37 个输出拒绝门；这是本次环境实测，不保证其他机器的相同比例。
证据根为 `F:/xlang-validation/five-groups-20261024/p6-source-optimization`。

这一项只改善源码验收工具，不改变生产语义、Cgen、XRT 或时间 ABI，也不
代表最新工作区、发布 DLL／SO、VFS 内嵌源或完整 TLS 已经验收。

## 会话队列拥有边前置

XRT 新增 `xrtTlsSessionBufferOwnershipTrace`，只读访问 Feed、Send、Plain、
Scratch 的真实池块租约与不可变字节引用，包含独立预留块，保留边的重数。
未知释放回调／裸借用拒绝；访问器可以停止，失败前可能已交付部分边。
不会分配、增减引用、执行释放回调或覆盖线程错误。调用方仍须排除变更，
另行拥有 Context、角色状态、验证器／身份回调和默认分配池；成功不意味着
会话是叶，更不是完整的会话清理／退役证书。

独立 XRT 及 xlang 四份实现／声明快照只定点加入该接口；精确反向核对确认
所有未选区域不变，独立 XRT 的毫秒／For 与快照的微秒／Until 合同各自保留。
源布局和结构体 ABI 未改变，符号登记为 3380 个核心／3845 个产品公共函数。

验收 245 阶段：原生模块化／单头 Windows 87、Linux 116；两种嵌入快照的
实现与声明探针 Windows 18、Linux 24。28 次运行覆盖 GCC O0／O2、TCC 和
Linux ASan／UBSan；四个不同的字节拥有者确认四条队列均被访问，8 个真实
池租约、4 条字节边、预留块、停止、复制／接管内存及未知回调边界通过。
零分配与原错误保留、引用不变、完整释放和内存调试账本通过。增强夹具仅
改测试，前轮证据保留。证据根为
`F:/xlang-validation/five-groups-20261024/p6-tls-session-buffer-ownership`。

这是 sans-I/O 会话集成的原生前置，不新增语言层 TlsSession 接口，不认证
动态回调、传输／Future、恢复／票据／KeyUpdate 或发布产品；没有网络、
畸形密码材料或安全专项测试。P6／P7 和历史诊断仍开放。

## 受管理 sans-I/O 会话

`TlsSession(TlsClientContext, serverName, size=-1, verifyName="", verifySize=-1)`
和 `TlsSession(TlsServerContext)` 分别创建客户端／服务端会话。原生会话独立
拥有配置、身份和验证器；来源语言对象可以先释放。SNI 与验证名称分离：空的
verifyName 按原生合同继承 serverName；IP 身份应显式使用空 SNI 和 IP 验证名。
名称采用双字段 string 和可选字节长度，不通过 strlen 恢复长度。

公开状态为 TlsRole／TlsState／TlsWait／TlsResult；原生 ERROR 转为 checked
错误，不伪装成 wouldBlock。`ready` 才表示握手完成，version／cipher 的选中
不表示对端已认证；尚未选中时访问它们报状态错误。protocol 返回独立 bytes。

`drive()` 推进协议；`feed(string|bytes, size=-1)` 接收精确字节，`feed(NetBuffer)`
移动队列；wouldBlock／失败时后者保持来源不变。`write(string|bytes, size=-1)`
返回 `(TlsResult, uint64)`，必须处理部分接收。`read(maxBytes=65536)` 返回
`(TlsResult, bytes)`；`takeCipher(maxBytes=65536)` 返回待传输的密文字节。
两种读取最多复制一批 16 个原生 span，maxBytes 是上界，不保证一次填满。
分配和 bytes 装箱成功后才消费队列，OOM 不丢数据；不暴露借用 span，也不
通过 const bytes 视图回填。Plain 仅在 ready／closing／closed 时允许交付。

`close()` 发起协议关闭，`eof()` 告知底层输入终止；`dispose()` 是幂等的突然
释放，不等于经过认证的关闭。析构只释放资源，不隐式完成握手、补尾或关闭
通知。这里不拥有 socket，不自动等待输入，不引入 TCP／NetEngine／TlsStream。

会话是可变原生节点，不是叶。真实引用计数包括语言句柄、原生别名、活动
调用与收集器 pin；每次调用先取得互斥准入和真实引用，再离开 Mutation 执行
原生代码。重入／并发调用 checked 拒绝，Busy 时拥有图快照延后；四条原生
队列的真实拥有边参与目录，Claim／Restore／Clear／Finish 支持 checked 退役，
最终原生释放在 Freeze 外执行。本批工厂仅接入静态原生身份和验证器；这不
构成对任意 C 回调或未来语言闭包的拥有证明。

`(int64或数值枚举, bytes)` 使用闭合的 `XL6_ProductI64BytesV1` 物理合同，
尾项带一个拥有的 xvalue 引用。语义、类型化 IR、lowered IR 和模块／C／头
输出门共同验证其 shape 与 ownership，失败边使用现有 PRODUCT_STORE_MOVE
后再执行异常清理，不放宽聚合拥有权校验。Cgen 不增加 TLS 算法或名称分支。

动态身份／CRL／恢复回调、票据／恢复／KeyUpdate、传输／Future、旧 TLS 后端
清退及发布产品验收仍待后续批次；不能以 sans-I/O 正常握手代表完整 TLS 完成。
旧 `stdlib_tls_session.xl`、`stdlib_tls_session_pair.xl`、`stdlib_tls_session_trim.xl`
仍按旧 `TlsConfig.session()`／回调和恢复接口编写，保留其覆盖并待后续迁移，
不计入本批通过项；本批不是默认全套测试或最新发布 DLL／SO 的验收。

### 会话基础批次验收

冻结 8977 份稳定输入，Windows／Linux 完整源码驱动分别构建 1152／1151 TU，
全部 O2，不替换生产对象。合计 417 阶段：Windows 210、Linux 207，包含两端
各 72 个既有产品 ABI／托管多返回值／CRT 数学／TLS 参数回归。四类固定密钥
正常内存握手覆盖完整二进制 ALPN、分块 feed、NUL／非 UTF-8 数据、部分 read、
COW、协议关闭、dispose 与 typed 容器，实际 JIT／module／独立 AOT 和严格
C O0／O2 通过；20 份实际裁剪计划保持无传输依赖、单侧角色与空声明分离。

25 目标、2 原生拥有结果、3 产品结果、2 bytes 结果、1 资源策略通过；两端
O0／O2 均拒绝 249 错误合同、25 同物理表示类型交换及 32 个模块／C／头
零输出门。驻留 GCC O0／O2、TCC、Linux ASan／UBSan 覆盖活动最后引用、
Busy 延后、真实目录 Hold／Claim／Restore／Clear／重复 Finish、16-span 短取、
字节内容哈希、8 输出分配预算（5 实际失败）、2 发布与 2 构造预算，内存平衡。

原测试误把“未调用的工厂声明”当作原生可达性证明；修正断言并增加两份真实
单侧构造用例，生产源码和全部编译对象不变，Windows 保留已通过的 98 阶段，
补跑 30 阶段；Linux 以修正后的工具完整运行。本批源码修复还包括声明岛的
完整依赖和 foreign managed-product 失败边的显式移动；没有放宽 verifier。

首次 Windows 驻留编译 OOM 及各前轮诊断保留。Linux 第一份 R3 解包副本的
8977 文件均为空，命令空成功未计通过；原归档正常，新目录恢复后逐文件哈希
一致才构建／验收。独立原生库哈希保持不变，不将坏副本归咎于语言编译器。
两端接受报告与产物重新哈希，Linux 构建及测试产物另存 F 盘归档并核对，
全部原目录仍保留；证据根为
`F:/xlang-validation/five-groups-20261024/p6-tls-session`。

没有网络流量、畸形密码材料或安全专项；没有更改系统时区、防护、页文件或
WSL 服务。两边时间 ABI 均保留，Cgen 未改；完整工作区／发布产品、动态回调、
恢复与传输、P6／P7 及历史 Windows 间歇诊断仍开放。

## 会话观察与角色操作

驻留会话使用永久的角色操作表，创建者分别提供客户端或服务端的 drive、
KeyUpdate 和原生观察函数。共享适配只通过这张私有表分派，不按语言类型名
回查，也不为一个客户端观察入口机械链接服务端握手实现。表没有语言闭包、
动态模块地址或额外拥有边；活动准入、真实引用及 Busy 规则与会话基础相同。

`peerCertificateCount` 在认证完成前为 0；失败会话 checked 拒绝，不能把已解析
或只做过链检查的证书当成已完成认证的对端。`peerCertificateAt(index=0)` 必须
在 ready／closing／closed 状态读取，负数及越界索引拒绝。它返回独立拥有的
Certificate／DER 快照，来源会话销毁后仍可使用；分配失败不改变原会话。
不支持的服务端客户端证书链为 0，不因此宣称原生已经支持 mTLS。

`serverName` 返回接收到的 SNI 原始 bytes，尚无名称时为空，不强行转成 UTF-8；
`serverCookie` 返回原生选择器的 uint64 元数据，静态身份默认为 0。两个入口
只适用于服务端，在客户端调用 checked 拒绝。所有返回字节都是独立值。
SNI／Alert 是收到的对端元数据，不是已认证身份或握手成功的凭据。

`peerAlert()` 返回 `(hasAlert, level, description)`，后两项为 int，保留完整原生
数值而不猜测未知 Alert。缺失时为 `(false, 0, 0)`，close_notify 的 description
也为 0，因此不能仅靠 description 判断有无 Alert。该纯标量物理合同为
`XL6_ProductBoolI64I64V1`：bool 加两个语言 signed64，别名独立解析；不接受
窄整数、C 整数别名、任意枚举或托管字段，不把 bool 擦除为 int。模块／IR／
C／头输出门继续校验闭合的 profile，Cgen 无 TLS 分支。

`keyUpdate(requestPeer=false)` 通过本次角色的原生入口排队 TLS 1.3 KeyUpdate，
返回 TlsResult；请求对端更新由原生协议驱动处理。调用方继续传递密文并 drive，
不能把成功排队误认为对端已经处理。TLS 1.2／不合法状态按原生 checked 报错，
wouldBlock 仍由调用方处理背压；该入口不承担重新握手、票据或恢复缓存职责。

### 会话观察批次验收

冻结 8981 份稳定输入，Windows／Linux 全源码 O2 分别构建 1154／1153 TU，
无生产对象替换。两端各 137 个语言／IR／裁剪阶段、72 个既有产品 ABI／CRT
数学／TLS 参数回归，驻留 Windows 10、Linux 12，合计 440 阶段通过。
四类正常固定密钥握手覆盖认证前的证书拒绝、独立证书存活、SNI 值复制、
Alert 存在位及 close_notify、客户端请求对端更新、服务端单向更新和更新后的
双向应用数据；真实 JIT／module／独立 AOT 及严格 C O0／O2 均通过。

31 个驻留目标、3 原生拥有结果、4 产品结果、3 bytes 结果、1 资源策略通过；
两端 O0／O2 均拒绝 311 错误合同、31 同物理表示类型交换及 42 个模块／C／头
零输出门。22 份实际裁剪计划保持角色分离且无 TLS 传输依赖，错误参数和
窄整数三元 profile 拒绝；普通 bool／signed64 三元组的别名、间接调用、赋值
与丢弃独立检查。驻留 GCC O0／O2、TCC、Linux ASan／UBSan 及内存账本通过，
新增观察错误路径确认 pin 与准入门全部释放。

能力矩阵工具补识别操作表中的已知 API 地址绑定，不把字符串、注释、类型
声明或未知数据名当作证据，3 项针对性单元检查通过。这个报告工具变更发生
在冻结后，不改变生产源或编译对象，另存工具控制哈希；新增的 15 条直接
引用统计不等于本批新增了 15 个公开函数。矩阵现为 2239 直接引用／2431
语义抽象，低层 API 仍为 3845。全部正式阶段在一次冻结上通过，未靠放宽
verifier、删除负例或 Cgen 名称特判；证据根
`F:/xlang-validation/five-groups-20261024/p6-tls-session-observe`。

本批没有改 XRT 核心、两边时间 ABI 或 Cgen，没有网络流量或安全专项测试。
动态选择／CRL／恢复回调、TLS 传输／Future、票据／恢复、旧后端和旧测试
迁移、完整发布产品及 P7 继续；不把此处 KeyUpdate 的正常功能通过当作完整
TLS 或安全审计完成，历史 Windows 间歇诊断保持开放。

## TLS 传输配置

`TlsStreamOptions` 是后续受管理 TLS Stream／Listener／Dial 共用的不可变配置，
不是旧 `TlsConfig` 的别名。可以单独构造，不会打开网络引擎或创建会话。

```xl
auto options = TlsStreamOptions()
auto bounded = options.withTimeoutsMillis(10000, 5000).withAsyncLimits(1048576, 1024, 64)
```

五个只读属性为 `handshakeTimeoutMillis`、`closeTimeoutMillis`、`maxAsyncBytes`、
`maxAsyncOperations`、`asyncBatch`。默认分别为 10000、5000、1048576、1024、64，
与固定 XRT 快照的公开常量一致；验证另与真实 `xrtTlsStreamConfigInit` 比较，
不复制底层初始化实现。两个 `with…` 方法返回独立配置，不修改来源对象。

超时使用显式毫秒，-1 表示禁用，有限超时必须大于 0，换算为固定快照的微秒
不得溢出。独立 XRT 的毫秒／For ABI 不因本批而升级或回退。字节上限必须为
正且能装入原生 size_t，操作数和批次必须为正且能装入 uint32；不自行添加
原生没有的“批次不得大于操作数”限制。这里是排队硬上限，不是预先分配
相应大小的缓冲，也不能把超大合法配置理解为资源充足的保证。

配置仅按值保存 `xtlsstreamconfig`，无引擎、会话、回调、语言值或借用存储，
因此具有真正的叶节点拥有策略，可跨线程只读共享。创建、复制、装箱失败
按 checked 错误返回并回滚。getter 为 pure／nonblocking，但保留错误合同；
分配入口没有误标 no-raise。适配和校验驻留 runtime，Cgen 只发声明和调用。
配置单独使用时必须裁掉 TLS Stream／Client／Server／Verify、TCP 和网络引擎。
私有 ABI 头保持声明 SDK 的 `.h` 闭包，不引入实现用字段表。

这不是 TLS 传输的完成证明。原生 TLS Stream／Listener／Dial 的真实持有边、
静止检查、checked 退役、Future 和迁移仍待补齐；不能将它们当作配置叶节点。
用户已授权必要原生接口及定点同步，不包含整体时间 ABI 更新。P6／P7、
历史 Windows 间歇诊断与完整发布产品验收继续开放。

### 传输配置批次验收

首轮真实 Linux JIT 发现 ABI 头引用实现 `.inc`，违背声明 SDK 的头文件闭包。
修复为独立声明头，并新增只复制真实 `.h` SDK 的 GCC／TCC 隔离编译门，
没有放宽 SDK 规则、增加实现文件分发或绕过真实 JIT。失败目录完整保留。

修复后重新冻结 8999 份输入，全源码 O2 Windows 1156／Linux 1155 TU。
最终冻结上 Windows 51 个新增阶段加 72 回归、Linux 53 加 72，共 248 阶段
通过。覆盖真实 JIT／module／独立 AOT、严格 C O0／O2、驻留 GCC O0／O2、
TCC、Linux ASan／UBSan、6 个分配预算中的 3 个失败、2 个发布预算及真实
目录 Hold／Claim／Restore，内存账本平衡。原生默认值对照调用真实后端 Init，
而非测试复制体；独立驻留构建和 4 份语言裁剪计划都不引入 TLS／网络后端。

9 个目标、3 个原生拥有结果、1 个资源策略通过；每端 O0／O2 均拒绝 70 个
错误合同并检查 10 个模块／C／头零输出拒绝门。现有产品 ABI、CRT 数学及
TLS 参数的 72 阶段回归分别通过。所有源／对象／产物哈希复核；Linux 5504
份构建与测试产物另存归档并独立复核，原目录未删除。证据根为
`F:/xlang-validation/five-groups-20261024/p6-tls-stream-options`。

本批未改 XRT 核心、两边时间 ABI 或 Cgen，没有网络流量、安全专项或
畸形密码材料测试。它只完成配置前置，不替代原生传输拥有权或发布验收。

## 拥有型 TCP／TLS 交接底座

原生新增 `xrtNetStreamConnectOwnedV1`：投递启动工作之前登记永久事件策略
和一个真实 Data 引用，成功消费一个、失败消费零个；旧借用式 Connect
保留。为后续受管理 TLS 提供创建时拥有边，而不是让 Open／Close 先执行
再补登记。此处仅新增低层 C ABI，不冒充已经完成 TLS 语言公开面。

原生 TLS Attach／Accept 不再直接改写 TCP Events／Data，而是使用既有的
事件替换、准入和 Worker 延迟退役。旧拥有型上下文在当前调用栈结束后
才释放；真实待读 Future 会 checked 拒绝交接。Attach 失败还原 Session
四个队列的分配策略，保留原上下文及 TCP／Session 两个调用方引用，成功
后才发布 Transport 和输出。两边时间 ABI 不动，Cgen 无修改或符号名特例。

冻结原生 2331、xlang 8999 输入；原生模块化／单头，GCC O0／O2、TCC、
Linux ASan／UBSan，select／IOCP／epoll 的正常回环生命周期通过。共 945
个原生编译／运行阶段，其中 42 次 fixture 执行、84 轮交接；不将编译步骤
冒充独立测试。两份既有正常固定密钥 TLS 握手／Attach 用例同时回归。

xlang 全源码 O2 Windows 1156／Linux 1155 TU；配置／IR／裁剪 51／53、
产品 ABI／CRT 数学／TLS 参数回归各 72，真实 SDK 原型的新符号 TCC C
执行检查各 6，合计 260 阶段通过。C 执行检查是冻结后追加并单独记录
哈希的测试控制，不修改生产对象；使用 volatile 的精确函数指针，避免
优化掉待验证符号。它不以 point 粗略重声明原生结构指针，也不放宽 Cgen
或依赖不兼容函数指针转换。JIT／module／独立 AOT、严格 C、拥有图和
裁剪检查继续通过。两端时间 ABI 保留，快照逐段反向校验，XRT／xruntime
低层 API 为 3381／3846，不把新增低层入口统计成新增标准库语言函数。

Linux 7092 份已接受构建与测试产物独立归档、逐项复核，原件保留；失败
和探索日志不删除，也不纳入正式通过。证据根为
`F:/xlang-validation/five-groups-20261024/p6-tls-transport-handoff`。
原生完整单头仍有非本批生成差异，命名段校验通过不等于完整原生产品验收。

本批仅做普通受控回环及离线 ABI／生命周期检查，无公网、DNS、探测、
畸形流量、模糊或网络安全专项。Stream 运行时根、会话／配置／异步节点
的完整拥有边，Listener／Dial、静止检查和 checked 退役，动态回调、恢复、
旧后端清理及 P7 仍开放；长期任务没有缩小到这一交接前置。

## 显式拥有 TLS Stream 核心底座（V1）

新增 Connect／Attach／Client／Accept 四个 OwnedV1 工厂，以及引用、运行时、
传输事件、静止策略与 Adapter 五个低层入口。永久上下文策略真实持有事件
和所有 Session 借用回调，成功消费一个 Data 引用、失败消费零个，Session
先于 Data 销毁。旧借用工厂不会自动获得认证，Events／Data 不可被替换。

图中分别列出实际 Engine 操作引用和 storage 引用、原有 RuntimeHeld 根、
TCP 事件拥有引用、Session 四个缓冲队列及错误根因，不伪造引用数。Claim
隔离新提交，Prepare 通过原生生命周期推进真正关闭；未读缓冲在停止后于
Freeze 外释放并重新验证全图，避免池租约与 Engine 退役的循环依赖。
Clear 不执行回调，Finish 保留首次清理错误，优先恢复进入时的线程错误，
重复检查不重复析构。底层算法驻留 XRT，Cgen 无改动，两边时间 ABI 保留。

此阶段仍明确拒绝旧式 pending Future 节点和未知 Listener 管理上下文。
它不是完整 Stream／Future、Listener／Dial 接入，也没有把传输错误地标成
叶节点。还须补全原生异步节点、共享上下文、Listener／Dial 拥有图，再迁移
语言公开面、动态回调、恢复及旧后端；P6／P7 与发布产品验收继续开放。

验收证据为 `F:/xlang-validation/five-groups-20261024/p6-tls-stream-ownership`。
原生冻结 2334 输入，最终 r3 仅将测试数组改为等价逐项初始化，以避开
Windows TCC 的 initializer 内部错误；r1／r2／探测失败证据原样保留。xlang
冻结 9010 输入，两端原始产物保留，不将源码冻结验收冒充当前全部工作区
或发布 DLL 的验收；未运行公网、DNS、畸形流量或安全专项。

最终原生 Windows 411／Linux 548 编译运行步骤、21／28 次实际 fixture
执行通过，包含模块化／单头、GCC O0／O2、TCC、Linux ASan／UBSan 及
select／IOCP／epoll。新图用例累计验证 56 个实际循环图、28 次持久清理
失败与完整内存台账；未把编译步骤计作独立测试。两端全源码 O2 分别
1156／1155 TU，xlang 配置／IR／裁剪、产品 ABI／数学／TLS 回归及九个
精确 SDK 新符号的真实 TCC 绑定执行共 129／131 阶段通过。所有接受的
输入、对象和产物由独立读取器逐项复核哈希；Linux 原件仍在 WSL，接受
报告另复制到证据根，未删除或以旧产物替换当前验证。

## 原生 TLS Future 激活与拥有结果

以真实引用节点替换旧 pending Future 的保守拒绝。构造／FIFO、Promise
producer 和 Owned CancelWatch 各持有一个引用，节点只 Trace 实际的
Stream、Promise、Watch 和缓存 Engine 租约。接收结果使用永久 bytes
拥有策略；没有裸析构函数遮蔽结果图，也没有按每个引用重复展开子图。

Publishing／Active 排除半发布与调用中的图，失败先退役 Watch、归还预算
和全部引用。完成后即脱离 Engine 缓存租约，即使收集 pin 仍保留外壳也
不能阻塞退役；普通无 pin 完成仍复用小节点缓存，元数据不超 128 字节。
FIFO walker 校验环、尾、节点类型、偏移、读写数和字节预算。节点 Prepare
等待真实源完成，不能把已产生 Future 提前关闭来假装收敛。

Claim 隔离新公开提交，不中断原生已受理的读写；内部 accepted 路径仍要求
正确 Worker、未 Clear 对象及真实写入／关闭／取消门。没有临时解除 Claim
或公开不安全绕过入口。同步仅更新六个命名段及对应声明／符号／矩阵，
新增五个低层策略／Adapter 入口，API 3395／3860，时间 ABI 与 Cgen 不改。

普通固定密钥新夹具覆盖三个客户端拥有工厂及 Accept、待读导致 Attach
失败的引用与池回滚、构造 OOM、分片发送和 NUL 拥有结果、真实 pending
节点的三个引用、策略副本拒绝、Claim／Restore、实际停止、惰性 pin、
持久清理错误及内存台账；旧 Stream 与交接用例一并回归。证据根为
`F:/xlang-validation/five-groups-20261024/p6-tls-stream-future`。仍不表示
Listener／Dial、语言传输迁移、动态回调／恢复、旧后端清退或 P7 完成。

本批最终冻结原生 2337、xlang 9011 输入。原生 Windows 417／Linux 556
编译运行步骤、24／32 次实际 fixture 执行通过；新图夹具累计 168 个
循环图、336 个 pending 激活、336 个拥有结果、840 个预算中的 504 次
失败、56 次 Attach 回滚、84 次持久清理错误，均平衡收尾。覆盖 Select、
IOCP、epoll、GCC O0／O2、TCC、模块化／单头及 Linux ASan／UBSan。
两端全源码 O2 1156／1155 TU，配置／IR／裁剪 51／53、产品 ABI／数学／
TLS 回归各 72、十四个精确 SDK 符号的真实 TCC 绑定各 6，共 260 阶段
通过。输入、对象和产物哈希独立复核；Linux 原件留于 WSL，接受报告
复制到证据根。只做普通本机回环与离线检查，无公网、DNS 或安全专项，
不冒充当前完整工作区、发布 DLL 或整个 TLS 验收。

## 原生 TLS Listener 组合图与 accept Future

`StartOwnedV1` 成功消费一个 Data 引用，失败消费零个。Listener 保留
Engine 操作及 storage 信用，运行时目录与底层 TCP 分别枚举真实引用。
每条子连接记录拥有 Listener；未交付时另持 Stream 创建信用，握手链／
完成队列及 Stream 上下文分别持有记录信用。交付只转移已有创建信用，
不把链表指针或已发布 Stream 借用误算成拥有边。

子连接复用永久 Stream Data／事件策略，pull 和 push Read 分开，不引入
Observer 或在交接时改写事件。回调收到原始 Data，低层 StreamData 返回
私有组合记录。普通 Listener Close 不关闭已交付连接；全图准备时各子
Stream 执行自己的停止职责。待完成 accept 有 FIFO／producer／Owned
CancelWatch 三份真实信用，结果使用拥有 Stream payload。Prepare 等待
真实源完成，Clear 无用户回调，Finish 保留首次清理错误且可重复检查。

定点同步五个命名段、新增十五个低层入口；API 3410／3875。两端各自
时间 ABI 与 Cgen 不变。尚未完成 Dial、语言层传输迁移、动态回调／恢复、
旧后端清退及完整 P7，因此本批不是完整 TLS 或发布产品验收。

证据 `F:/xlang-validation/five-groups-20261024/p6-tls-listener`。最终冻结
原生 2340、xlang 9012 输入；Windows／Linux 原生 429／572 编译运行步骤、
27／36 次实际 fixture 执行通过。新夹具累计 224 个闭合图／证明、280 条
记录、56 个待完成 accept、896 次拒绝、112 次持久清理错误，以及 1204
个分配预算中的 812 次失败，均平衡收尾。覆盖 select／IOCP／epoll、模块化／
单头、GCC O0／O2、TCC、Linux ASan／UBSan。两端全源码 O2 1156／1155 TU，
配置／IR／裁剪、产品回归及二十九个精确 SDK 符号的真实 TCC 绑定共 260
阶段通过。输入、对象和产物哈希独立复核；Linux 原件保留，接受报告复制。
仅普通显式本机回环和离线检查，没有公网／DNS／安全专项。
