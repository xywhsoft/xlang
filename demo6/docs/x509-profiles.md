# X.509 typed profile、分发点与名称约束

这是已批准 P5 的拥有数据层，复用固定 XRT 的 profile、distribution 和
name_constraints。它不验证证书签名、路径、信任、撤销状态或 TLS 对端。
算法及解析均在 XRT，拥有适配在驻留 runtime，Cgen 不增加实现或按符号补丁。

## 公开类型与入口

| 类型 | 主要能力 |
|---|---|
| X509GeneralNames | 完整 DER 导入／导出，count、O(1) at |
| X509GeneralName | kind、原始 DER／value、isConstraintBase；IA5 text、registeredOid、directoryName；实际名称对约束基点的 isWithin |
| X509Oids | 完整 OID 序列，内容 bytes、点分 oid、contains／containsOid |
| X509KeyUsage | 不可变 flags；扩展缺省与零值不混同 |
| X509BasicConstraints | ca、hasPathLimit、pathLimit；缺省路径上限不伪装为零 |
| X509AuthorityKeyIdentifier | 完整 DER、hasKeyId／keyId、hasIssuer／issuerNames、hasSerial／serialContent |
| X509DistributionPoints | 完整 DER、count、O(1) at |
| X509DistributionPoint | 完整 DER、hasName／nameKind／nameDer、fullNames／relativeNameDer、hasReasons／reasons、hasIssuer／issuerNames |
| X509NameConstraints | 完整 DER、permitted／excluded 拥有列表、check(Certificate) |
| X509GeneralSubtrees | 完整序列 DER、count、O(1) at |
| X509GeneralSubtree | 完整 DER、约束基点 base、hasMinimum／minimumMagnitude、hasMaximum／maximumMagnitude |

封闭枚举为 X509GeneralNameKind、X509KeyUsageFlags、X509ReasonFlags、
X509DistributionNameKind。两个 flags 类型支持同枚举位运算，拒绝跨枚举或整数
混用。ReasonFlags 是 BIT STRING 的位编号，不是后续 CRLReason 的协议数值；
all 不含保留 unused 位。未知选择不猜成文本，保留原始 DER／value；无法处理
的约束仍由 XRT checked 拒绝，不能静默忽略。

Certificate 增加 subjectAltNames、issuerAltNames、keyUsage、basicConstraints、
extendedKeyUsage、subjectKeyId、authorityKeyId、crlDistributionPoints、
freshestCrl、nameConstraints。扩展缺省返回可空值；存在但解析／资源失败仍
抛 checked 错误，不把错误或不支持降格为“扩展不存在”。

```xlang
auto cert = Certificate.fromDer(encoded)
auto usage = cert.keyUsage()
if usage != null
    assert (usage.flags & X509KeyUsageFlags.digitalSignature) != X509KeyUsageFlags.none
end
auto constraints = cert.nameConstraints()
if constraints != null then constraints.check(candidate) end
bytes? keyId = cert.subjectKeyId()
```

## 必须区分的表示

`bytes?` 驻留结果使用有 null 标签的 XRT Value。缺省 SKI 返回拥有的 canonical
null 值，C NULL 保留为失败；不能把类工厂的裸指针缺省约定套到它上面。语义
检查及精确桥接合同通用支持这个已有语言载体，没有为 SKI 添加 Cgen 特例。
可空字节可与局部 null、typed 容器及既有 string? 通道正常往返。

GeneralName 的普通 IP 值是 4／16 字节地址；GeneralSubtree.base() 的 IP 值
则是 8／32 字节“地址＋掩码”，isConstraintBase 明确标记后者。isWithin 仅
接受实际名称和约束基点，按 XRT 支持范围执行，不忽略不支持的类型。它只做
基点比较，不冒充完整 GeneralSubtree 距离策略；完整约束应使用 check。

DirectoryName 保留完整 Name DER，directoryName(maxAttributes=65536) 返回
独立拥有的 X509Name。text 只适用于原生已验证的 email／DNS／URI IA5 内容；
registeredOid 只适用于 registeredId，其他选择调用这些投影 checked 报 RuntimeError（原生 XERR_STATE）。

AKI serialContent 保留规范 INTEGER 内容及必要正数符号零；Subtree 的
minimumMagnitude／maximumMagnitude 则是 XRT 已去掉符号零的非负大端幅值，
不窄化为 int64。显式零的幅值可以为空 bytes，必须结合 has 标志区分缺省。

DistributionPoint.nameDer 是原始上下文标记，relativeNameDer() 才返回完整
SET 编码的相对 RDN；不把它伪装成完整 Distinguished Name。fullNames() 在
缺少名称或相对名称模式下为空，issuerNames() 在缺少 issuer 时为空；其余
字段由 has 标志和 nameKind 明确区分。读取缺省 nameKind／pathLimit 报
RuntimeError（原生 XERR_STATE），不伪造默认名称种类或无限路径上限。

## 拥有权、预算与裁剪

所有记录不可变、可跨线程读取。序列一次分配保存内容和解析索引，子对象只
拥有自身片段，不保留证书、大型父列表或借用游标。GeneralNames／Oids／
DistributionPoints／GeneralSubtrees 的 toDer 用 XRT DER 编码原语恢复完整序列，
接管输出缓冲后交付 bytes，不重复复制整个输出。11 类记录进入真实叶策略及
已有拥有目录；不是平行引用计数或把借用视图伪装成拥有对象。

完整 DER 导入默认 16 MiB，列表默认 65536 项；子名称／约束／分发点投影有
独立项目上限。0 显式关闭对应预算，负数非法，超过预算报 RangeError；预算
不截断 DER、不接受尾部数据、不交付半成品，分配尺寸和计数独立检查溢出。

35 个 provider 按调用闭包接入。SKI／AKI、KeyUsage／BasicConstraints 分开；
不因为读取 SKI 就引入 AKI 对象和 GeneralNames 适配。普通名称读取不拉入
名称约束／Unicode 名称比较；分发点不拉入路径或密码学验证；仅枚举不拉入
X.509。完整 profile 解析与严格约束检查仍尊重 XRT 原生模块依赖。

## 验收边界与剩余工作

`check_x509_profiles.py` 使用三份正常固定 Ed25519 密钥签名的离线证书，独立
工具先核对签名与字段；覆盖有／无扩展、零路径上限、IA5／目录／IP／注册
OID、AKI issuer／serial、完整与相对分发点、约束与任意精度距离、子对象
生命周期、逐分配失败、真实拥有目录。没有网络流量或安全专项。

`check_stdlib_x509_profiles.py` 覆盖 85 个原生声明／登记／XLIR 合同、27 个拥有
结果工厂、四个枚举、nullable bytes／string、typed 容器、跨线程读、JIT／
module／独立 AOT、严格 GCC O0／O2、坏合同输出前拒绝和按需裁剪。
正式验收结果单独记录，不把窄范围源码通过当作 shipping 验收。

2026-10-23 正式验收为 Windows 265／Linux 268，共 533 阶段：各有 93 个新
语言／IR／裁剪阶段、92 个上一批记录回归、59 个原始数据接口回归、9 个旧
DER／密钥／数学回归；原生专项 9／11 阶段，另有 3／4 个独立原生重放。
完整源码构建 1026／1025 TU，不混计阶段。每配置 146 个分配预算、82 次
实际失败、22 个句柄发布预算、11 个真实叶目录通过；1052 处新坏合同、96
个完整模块／C／头输出门通过，旧 850／75 和 399／37 两组也继续通过。

原生边界 `native-*-r2`、完整源码驱动 `source-*-r4`、语言 `api-*-r2`；证据根
`F:/xlang-validation/five-groups-20261023/p5-x509-profiles`。输入／日志／产物和
驱动身份独立复核，Linux 原件和 F 盘镜像同时保留，最终提交身份见
`verified-commit.json`。这是冻结的完整工作区源码验收，不是纯净 checkout
或发布 DLL／SO／VFS 验收。未修改 Cgen、独立 XRT、固定快照或时间 ABI。

此处记录的是 profile 批次边界。后续独立 SubjectKeyIdentifier／CRL DER 值
解析、撤销条目及 CRL profile 的实际进展见 [CRL typed profile](x509-crl-profiles.md)。
签名检查与独立 SPKI 的后续进展见 [签名检查](x509-signatures.md)。P5 仍缺
CRL policy、完整路径及 typed 验证结果、信任来源／身份检查；TLS 和联合
验收继续，不能把数据读取宣称为全部 X.509 模块已封板。
