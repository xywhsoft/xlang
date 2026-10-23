# X.509 CRL 条目与 typed profile

此层只读取 CRL 数据，不作签名、信任、撤销策略或证书路径结论。复用固定
XRT 的公开 CRL／profile／DER 接口；适配在驻留 runtime，Cgen 只生成声明、
调用和拥有清理。不更换 XRT 快照，不修改两边各自的时间 ABI。

## CRL 上的入口

| 入口 | 结果与含义 |
|---|---|
| `entries(maxEntries = 65536)` | 独立拥有的 `X509CrlEntries`，无条目时为空列表 |
| `findBySerial(serialContent)` | `X509CrlEntry?`；输入是 DER INTEGER 正文，不是整个 TLV |
| `findCertificate(certificate)` | 按证书序列号查找的 `X509CrlEntry?`，不表示证书已验证或已撤销 |
| `numberMagnitude`／`deltaBaseMagnitude` | `bytes?`，任意精度、非负、大端幅值；没有扩展时是 null |
| `authorityKeyId()` | `X509AuthorityKeyIdentifier?`，复用已有 AKI 记录 |
| `issuingPoint()` | `X509IssuingDistributionPoint?` |
| `freshestCrl(maxPoints = 65536)` | `X509DistributionPoints?`；不自动请求 URI |

序列号正文保留 DER 正数所需的符号零，例如 128 是 `b"\x00\x80"`；编号幅值
去除多余符号零，128 是 `b"\x80"`，零是 `b"\x00"`。缺省与数字零不同。
`findCertificate` 不检查 issuer、间接 CRL 的有效签发者或适用范围；未命中也
不能解释为“未撤销”。这些结论只能由后续 checked CRL policy 给出。

## 拥有记录

`X509CrlEntries.fromDer(encoded, version, maxBytes = 16777216,
maxEntries = 65536)` 读取完整 SEQUENCE；提供 `count`、`version`、`at(index)`
和 `toDer()`。索引在创建时完成，`at` 不重复扫描前缀。

`X509CrlEntry.fromDer(encoded, version, maxBytes = 16777216)` 读取完整条目；
提供 `version`、`serialContent`、`revokedAt`、`extensionsDer`、`toDer()` 和
`extensions(maxExtensions = 65536)`。两种 standalone DER 都不编码 CRL
版本，必须显式给 `X509CrlVersion.v1/v2`，不可猜测；从 CRL 提取时自动保留
真实版本。`revokedAt`／`invalidityDate` 都是现有语言 time 的微秒表示。

条目的 typed 扩展为 `hasReason`／`reason`、`hasInvalidityDate`／
`invalidityDate` 与 `issuerNames(maxNames = 65536)`。reason/date 缺省时读取
值抛 StateError；不得冒充 unspecified 或时间零。issuerNames 只报告该条目
显式携带的 CertificateIssuer，不计算间接 CRL 的继承状态。

`X509CrlReason` 包含 0—6、8—10 共十个标准值，7 不是成员。它不同于可组合的
`X509ReasonFlags` 位掩码；二者不能混用。

`X509IssuingDistributionPoint.fromDer(encoded, maxBytes = 16777216)` 提供
`hasDistributionPoint`、`nameKind`、`nameDer`、`fullNames(maxNames = 65536)`、
`relativeNameDer()`、`onlyUserCertificates`、`onlyCaCertificates`、
`onlyAttributeCertificates`、`hasReasons`、`reasons`、`indirect`、`toDer()`。
完整与相对名称沿用已有枚举；相对名称返回完整 SET DER。缺省名称的 nameKind
或不适用的 relativeNameDer 抛 StateError；fullNames 不适用时返回 null。
理由缺省时 hasReasons 为 false、reasons 为 none，不由此推导策略允许范围。

每个对象拥有自身 DER 与必要索引，不持有原生短期借用。子对象复制自身片段，
父证书／CRL／列表释放后仍可使用。三类全部登记真实叶策略与拥有目录，允许
现有共享拥有图中的跨线程只读。

## 独立值解析

`x509.parseSubjectKeyId`、`parseCrlNumber`、`parseCrlReason`、
`parseInvalidityDate` 都接收 `bytes encoded, int maxBytes = 16777216`。
分别返回 bytes、bytes 幅值、X509CrlReason、time；必须是一份完整 DER，不按
size 截断，也不把任意二进制转成 string。它们不要求构造 Certificate／CRL。

所有预算 0 表示显式不限，负值报 ArgumentError，超过预算报 RangeError。
解析及分配错误经过 checked 边界，不误标 no-raise。缺省 bytes? 用拥有的 null
Value，C NULL 只表示失败；可空类的物理合同与此不同，不能相互套用。

## 验收与剩余边界

工具为 `check_x509_crl_profiles.py` 与 `check_stdlib_x509_crl_profiles.py`。
固定 Ed25519 密钥生成四份正常离线 CRL，独立库核对签名与字段；包括 v1/v2、
空列表、完整／delta、显式／缺省条目扩展、两种分发名称与不同作用范围。
仅进行正常功能、预算、OOM、生命周期、ABI、IR、JIT／AOT 和裁剪验证，不进行
网络流量、畸形样本、攻击模拟或安全专项。

本批不关闭 CRL policy、真实签名验证、typed 验证结果、路径／信任库／系统
来源／身份检查、TLS 或最终 shipping 联合验收；长资源作用域的编译性能及
清理 C 体积风险也保持打开。

2026-10-23 正式验收为 Windows 380／Linux 383，共 763 阶段：各有新语言／IR／
裁剪 115、上一批 profile 93、记录 92、原始数据接口 59、旧 DER／密钥／数学
回归 9；原生专项 9／11，独立原生重放 3／4。完整源码构建 1048／1047 TU，
不混计阶段。每配置四份有效 CRL、92 个分配预算、53 次实际失败、12 个发布
预算通过；45 个新驻留入口、13 个拥有结果、三类真实叶策略、613 个坏合同与
48 个完整模块／C／头输出门通过，旧三组合同／输出门继续通过。

正式边界为 Windows native-r4／Linux native-r1、source-*-r1、api-*-r2；完整
证据根 `F:/xlang-validation/five-groups-20261023/p5-x509-crl`。所有输入、日志、
产物及驱动身份独立复核；Linux 原件与 F 盘镜像同时保留，最终提交身份见
`verified-commit.json`。本批没有 Cgen、独立 XRT、固定快照、时间 ABI 或发布
资产改动；无关工作区及暂存区保留。它是冻结工作区源码验收，不是纯净
checkout／发布 DLL／SO／VFS 或完整 P5 验收。
