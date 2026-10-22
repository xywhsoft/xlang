# X.509 接入：完整模块归属与拥有数据边界

状态：本批两个拥有数据类型及其 35 个源码 API 已完成双平台验收；完整 P5
仍在进行。四个静态工厂、两个真实叶策略、语言调用和无关旧接口裁剪均有独立
证据，不把原始数据接入等同于完整证书认证。

这是原五组长期任务 P5 的执行清单，不缩减已批准的 21 个模块范围。
语言公开面按证书、撤销、验证和信任来源组织，不机械复制原生游标／裸指针接口。
实现复用固定 XRT；驻留适配负责拥有数据、精确错误和安全值投影，Cgen 仅发出
已经由 XLIR 验证的声明、调用、结果接管与清理。

## 21 个模块逐项归属

| XRT 模块 | xlang 归属及完整能力 | 当前批次边界 |
|---|---|---|
| x509_parse | Certificate 完整 DER、算法、时间、Name／Extension、SPKI；原始字段独立拥有，结构化读取保留 ASN.1 标签及 RDN 序号 | 本批接拥有数据和原始字段；结构化记录继续 |
| x509_crl | CertificateRevocationList、完整 DER／时间窗口、撤销条目及按序列号查询 | 本批接拥有数据和原始字段；条目记录继续 |
| x509_signature | 解析后的签名方案、摘要、MGF、盐长及 trailer；未知 OID 保留而不猜算法 | 后续 |
| x509_digest | 证书／CRL 验证使用的原生摘要支撑；摘要算法复用已有 crypto，不另造同义算法库 | 后续验证依赖 |
| x509_verify | 明确区分仅验证签名与完整路径／信任判断；支持能力按实际构建报告 | 后续 |
| x509_crl_verify | CRL 的签发者和签名验证，与撤销覆盖策略分开 | 后续 |
| x509_verify_rsa | RSA PKCS#1／PSS 证书及 CRL 验证后端 | 后续验证依赖 |
| x509_verify_ecdsa | P256／P384 证书及 CRL 验证后端 | 后续验证依赖 |
| x509_verify_ed25519 | Ed25519 证书及 CRL 验证后端 | 后续验证依赖 |
| x509_profile | SAN／IAN、KU、BasicConstraints、EKU、SKI／AKI、OID 记录 | 后续 |
| x509_name | RDN／属性、语义 Name 比较与包含关系，复用原生 Unicode 规范化 | 后续 |
| x509_distribution | CRLDistributionPoints／FreshestCRL、完整／相对名称、理由及 CRL issuer | 后续 |
| x509_crl_profile | CRL 编号、delta base、AKI、IssuingPoint、条目理由／失效时间／issuer | 后续 |
| x509_crl_policy | 严格配置、已验证 CRL、complete／delta 组合、分段覆盖及状态归并 | 后续 |
| x509_name_constraints | permitted／excluded 子树、名称类型及证书检查；未知能力不能静默忽略 | 后续 |
| x509_path | 有序路径、独立 trust anchor、用途／时间／critical 扩展策略 | 后续 |
| x509_path_build | 候选 issuer、独立 anchor、回溯建链、成功时拥有的路径与 typed 结果 | 后续 |
| x509_store | TrustStore 导入／去重、证书／锚读取和不可变验证快照 | 后续 |
| x509_store_file | 文件 DER／PEM 的事务导入，与现有 file／VFS 边界协调 | 后续 |
| x509_store_system | 显式装载当前 OS 信任库快照，不隐式信任、不联网补链 | 后续 |
| x509_identity | DNS pattern／引用主机和 SAN 中 DNS-ID／IP-ID 的严格匹配 | 后续 |

验证后端没有重复的语言数学实现。只读取证书／CRL 数据不应拉入签名算法、
路径、信任库、TLS 或网络传输。正常验证 API 所需的已支持签名后端必须完整，
未知／未编译能力明确报告，不回退弱算法或把“不支持”误判成“验证成功”。

后续 typed 验证结果需区分不可信、用途／时间／名称／撤销等验证结论与真正的
复制／预算／资源错误；不得将所有原生错误统一吞成 false。成功路径和 anchor
只能在全部检查通过后发布。原生 critical 策略／其他回调若进入语言公开面，必须
具备真实拥有边、代码租约和 checked 调用，不得交付短生命周期的指针视图。
CRL 状态未确定或覆盖不完整不能伪称 good；TLS 最终使用同一信任和身份体系。

## 本批拥有数据及源码 API

Certificate／CertificateRevocationList 都是不透明、不可变、可供多个线程读取的
语言对象。不是 xrtX509Parse／xrtX509CrlParse 返回的借用视图外壳。

- DER 导入只分配一份头部＋DER 存储，解析结果只引用自己的数据。
- PEM 导入使用 XRT 的 PEM 解码，接管解码缓冲，不再复制一份 DER；严格要求
  单个 CERTIFICATE／X509 CRL 块，块外仅允许空白。批量 PEM 是后续 TrustStore
  的事务接口，不把“自动挑第一块”混入单对象构造。
- 原始字段、toDer 和 toPem 返回独立的 bytes／string 值，不向语言暴露借用。
- 原始 Name／Extension／SPKI／TBS 字段显式以 Der 命名。serialContent 是 DER
  INTEGER 的完整内容，保留必要的正数前导零，不窄化到 int64。
- signatureOid 是 ASN.1 OID 内容；signatureParametersDer 是完整参数 TLV，
  无参数时为空 bytes，不猜 ASN.1 参数或任意转换成 UTF-8。
- 35 个原生入口的声明、登记、效果及拥有者闭合；四个静态工厂沿用既有物理
  native-field 接管，两个类型进入真实叶策略和拥有目录，没有平行生命周期。

```xlang
auto cert = Certificate.fromDer(encoded)
bytes spki = cert.publicSpki
auto crl = CertificateRevocationList.fromPem(pem)
if crl.hasNextUpdate then println(crl.nextUpdate) end
```

`validAt(instant)` **只检查时间窗口，不验证签名、名称、用途、路径、信任或
撤销状态**，不能据此接受 TLS 对端。证书窗口闭合；CRL 若缺 nextUpdate，
原生窗口检查没有上界。缺少该字段时读取 nextUpdate checked 报 StateError，
不能返回一个伪造的时间值；后续 CRL 验证策略仍会独立要求合适的更新界限。
全部时间使用语言 `time` 的精确微秒表示，不能传 int 冒充时间。

预算不是 size 截断：必须消费完整编码。fromDer 默认 16 MiB，fromPem 分别有
默认 32 MiB 文本、16 MiB 解码后 DER 上限；toPem 默认 32 MiB。每项预算独立
检查，负数非法，显式 0 表示不施加该项预算，以支持大型 CRL。即使预算关闭，
仍检查表示范围和分配尺寸溢出；不会返回截断的证书或部分 PEM。

## 验收范围

`tools/check_x509_owned.py` 使用四种固定公钥的正常有效证书、两种正常签名的
CRL（包含合法缺省 nextUpdate），独立库先核对签名与字段，再检查原生层数据
拥有、输出、预算、构造／字段复制失败、句柄发布及真实叶目录 Hold／Claim／
Restore。只复用 XRT 的解析、PEM 和分配实现。

`tools/check_stdlib_x509_data.py` 使用当前完整 source-driver 验收实际静态工厂、
getter、time ABI、typed 容器和跨线程读取，覆盖 JIT／module／独立 AOT、严格
GCC O0／O2、声明／合同集合、坏合同输出前拒绝和解析单独使用时的按需裁剪。

本批不是上述 21 模块的全部完成，不是路径／信任／名称／撤销策略验收，不是
TLS 或发布 DLL／SO／VFS 验收。不运行畸形证书／签名攻击样本、网络安全测试、
网络流量或公网服务。本任务原完整范围保持不变。

正式验收 Windows 77／Linux 79，共 156 阶段：各有 59 个语言／IR／裁剪、
9 个原有密钥／DER／数学接口回归；原生专项分别 9／11 阶段。全源码构建为
964／963 TU，不混计阶段。每配置 278 个分配预算、142 次实际失败、12 个
句柄发布预算及两个真实叶目录通过；399 处坏合同和 37 个完整模块／C／头
输出门验证拒绝。输入／日志／产物哈希独立复核，Linux 原件及 F 盘镜像保留。
证据根 `F:/xlang-validation/five-groups-20261023/p5-x509`，最终身份见
`verified-commit.json`。没有修改 Cgen、独立 XRT、固定快照或时间 ABI。
