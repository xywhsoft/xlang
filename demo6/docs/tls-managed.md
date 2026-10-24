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
