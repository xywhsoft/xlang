# X.509 接入：完整模块归属与拥有数据边界

状态：已有两个拥有数据类型及其 35 个源码 API、七类结构化记录及 68 个
精确驻留入口、11 类 typed profile／分发点／约束记录和 85 个入口；本批继续
接入三类 CRL 条目／IDP 记录和 45 个入口，及签名检查／独立 SPKI 的 15 个
入口。CRL policy 再补入 30 个驻留入口、四类公开拥有对象和一个内部回调
上下文。有序路径再接入 32 个入口、四类公开拥有对象和一个内部上下文。
累计 310 个驻留入口、31 类公开拥有数据类型（另有两个内部上下文）；
完整 P5 仍在进行，不把数据读取、签名通过或 CRL 结论等同于完整证书认证。

这是原五组长期任务 P5 的执行清单，不缩减已批准的 21 个模块范围。
语言公开面按证书、撤销、验证和信任来源组织，不机械复制原生游标／裸指针接口。
实现复用固定 XRT；驻留适配负责拥有数据、精确错误和安全值投影，Cgen 仅发出
已经由 XLIR 验证的声明、调用、结果接管与清理。

## 21 个模块逐项归属

| XRT 模块 | xlang 归属及完整能力 | 当前批次边界 |
|---|---|---|
| x509_parse | Certificate 完整 DER、算法、时间、Name／Extension、SPKI；原始字段独立拥有，结构化读取保留 ASN.1 标签及 RDN 序号 | 拥有数据、Name／Extension／算法／公钥信息及 UID 已接入 |
| x509_crl | CertificateRevocationList、完整 DER／时间窗口、撤销条目及按序列号查询 | 拥有数据、Name／Extension／算法、独立拥有条目／索引列表及序列号查询已接入；查询不是撤销结论 |
| x509_signature | 解析后的签名方案、摘要、MGF、盐长及 trailer；未知 OID 保留而不猜算法 | 结构化元信息已接入；不代表验证后端可用 |
| x509_digest | 证书／CRL 验证使用的原生摘要支撑；摘要算法复用已有 crypto，不另造同义算法库 | 签名检查依赖已接入 |
| x509_verify | 明确区分仅验证签名与完整路径／信任判断；支持能力按实际构建报告 | 签名 check／严格 verify 已接入；不是路径／信任验证 |
| x509_crl_verify | CRL 的签发者和签名验证，与撤销覆盖策略分开 | 签名检查已接入；覆盖和撤销策略继续 |
| x509_verify_rsa | RSA PKCS#1／PSS 证书及 CRL 验证后端 | 已接入签名检查 |
| x509_verify_ecdsa | P256／P384 证书及 CRL 验证后端 | 已接入签名检查 |
| x509_verify_ed25519 | Ed25519 证书及 CRL 验证后端 | 已接入签名检查 |
| x509_profile | SAN／IAN、KU、BasicConstraints、EKU、SKI／AKI、OID 记录 | typed 读取及拥有记录、独立 SKI DER 便利入口已接入 |
| x509_name | RDN／属性、语义 Name 比较与包含关系，复用原生 Unicode 规范化 | 已接入，规范化比较单独裁剪 |
| x509_distribution | CRLDistributionPoints／FreshestCRL、完整／相对名称、理由及 CRL issuer | 已接入拥有列表／记录及完整字段 |
| x509_crl_profile | CRL 编号、delta base、AKI、IssuingPoint、条目理由／失效时间／issuer | 已接入拥有投影及独立 DER 值解析；不计算间接 issuer 继承或撤销策略 |
| x509_crl_policy | 严格配置、已验证 CRL、complete／delta 组合、分段覆盖及状态归并 | 已接入拥有快照、完整／delta、绑定目标的不可变归并、类型化撤销状态和 checked 关键扩展回调；不是完整路径／信任验证 |
| x509_name_constraints | permitted／excluded 子树、名称类型及证书检查；未知能力不能静默忽略 | 已接入拥有记录、基点比较和严格检查；非路径／信任判断 |
| x509_path | 有序路径、独立 trust anchor、用途／时间／critical 扩展策略 | ordered 验证、显式锚、不可变策略／结果、拥有路径与 checked 回调已接入，双平台源码通过；发行者候选匹配后续 |
| x509_path_build | 候选 issuer、独立 anchor、回溯建链、成功时拥有的路径与 typed 结果 | 后续 |
| x509_store | TrustStore 导入／去重、证书／锚读取和不可变验证快照 | 语言接线后续；独立 XRT 深克隆／事务合并前置已完成 |
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

有序路径的底座历史及 32 个语言入口见 [路径接入](x509-path-owned.md)。它不是
候选建链、信任库、身份或撤销验证。显式原生结果提供 reason／depth，驻留层
不再从错误域／原因链分类；展示诊断独立拥有，不影响算法决定。

有序路径在 `8e2f2a6e6` 提交后，又针对 C-owned 编译器合并 `2d1471ceb` 完整
复测 1571 阶段通过，详情及独立回执见专项。其后已修复候选建链原生资源／
回调失败被吞问题，并从 `c980bc84b` 定点回补显式 decision 底座。公开建链、
信任库与身份仍待接入；xlang 微秒／Until 与独立 XRT 毫秒／For 合同分别保持。

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
原生窗口检查没有上界。缺少该字段时读取 nextUpdate checked 报 RuntimeError（原生 XERR_STATE），
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

## 结构化记录层

公开 `X509Name`、`X509NameAttribute`、`X509Extensions`、`X509Extension`、
`X509Algorithm`、`X509SignatureInfo`、`X509PublicKeyInfo`。每个记录只拥有自己的
完整 DER 片段；子记录不引用父对象，不让小字段保留整张证书或大型 CRL。Name
及 Extensions 在一次分配中保存片段和解析索引，创建时线性解析，at 为 O(1)，
find 按原始 DER 顺序返回首个匹配。查找未命中返回 nullable 空，不建立或丢弃
NotFound 错误；真正的分配／资源错误仍 checked 报告。

```xlang
auto cert = Certificate.fromDer(encoded)
auto subject = cert.subjectName()
auto commonName = subject.find("2.5.4.3")
if commonName != null then println(commonName.value.raw) end
auto extensions = cert.extensions()
auto algorithm = cert.signatureAlgorithm
auto scheme = algorithm.signatureInfo()
```

Name 属性保留零起始 `rdnIndex`、完整属性 DER、OID 内容及点分文本、值的完整
TLV 和拥有的 DerValue；不把任意 ASN.1 标签猜成 UTF-8。多个属性可属于同一个
RDN。equals／isWithin 复用 XRT Unicode 规范化，只在实际使用比较时拉入相应
模块。Extension 保留 OID、critical 和 extnValue 内部 bytes，不猜扩展语义；
SAN、KU、约束、分发点等 typed profile 是下一层，不能因此视为已完成。

Algorithm 保留完整参数 TLV 及 hasParameters，缺省参数与编码 NULL 不混同。
signatureInfo 对未知算法 OID 返回空；已识别算法返回封闭 X509SignatureKind、
X509Digest、MGF 摘要、盐长及 trailer。字段是否适用由 kind 决定；digest.none
并不表示 EdDSA 内部没有摘要运算。解析能力与验证能力分离，不根据后端缺失
更换算法，也不因识别了 Ed448 就声称能够验证 Ed448。

PublicKeyInfo 保留 SPKI、算法、BIT STRING 内部公钥字节、RSA 无符号 n／e，
并返回封闭 X509KeyKind／X509Curve；非 RSA 的 n／e 为空 bytes。不自动转换为
忽略 RSA-PSS 限制的普通 RSA 对象。证书另公开 issuer／subjectUniqueId、
各自 unusedBits 和 has 标志，避免把“字段缺省”和“已编码空位串”混同。

Name／Extensions 从 DER 构造默认限制 16 MiB 和 65536 项；从证书投影时可设置
项目上限。0 显式关闭该项预算，负数报 ValueError，超过限制报 RangeError。
预算不截断输入或返回半成品。Extensions 接受空 bytes 表示缺省扩展列表。
DerValue.asTime 复用 XRT UTC／GeneralizedTime 解析，返回精确语言 time。

68 个声明、登记和 XLIR 合同逐项闭合。七类记录全部进入已有真实叶策略与
拥有目录；nullable 工厂使用独立错误作用域，空结果不是错误判别哨兵。原有
DER OID 文本转换改为接管解码缓冲，移除隐藏 STRING 依赖及一次额外复制。
适配驻留 runtime，Cgen 不增加算法、包装函数或按符号追溯语义。

`tools/check_x509_records.py` 的八组正常固定密钥样例涵盖 RSA PKCS#1／PSS、
P256／P384、Ed25519、X25519／X448 公钥及 CRL；独立工具验证每份样例签名。
检查 Unicode／多值 RDN、内嵌 NUL、OID、参数、UID、父对象先释放、逐分配
失败与七个真实叶目录。`tools/check_stdlib_x509_records.py` 检查源码语言、
四枚举合同、JIT／module／独立 AOT、跨线程只读及按需裁剪。

本批正式边界为 native `*-r3`、source-driver `*-r4`、语言 `api-*-r2`。
Windows 181／Linux 186，共 367 阶段，包括 92／92 个新语言／IR／裁剪阶段、
59／59 个上一批数据接口回归、9／9 个旧 DER／密钥／数学回归、9／11 个
新记录原生阶段、9／11 个原有原生数据回归，以及 3／4 个独立原生重放。
全源码构建为 991／990 TU，不混计阶段。每原生配置 707 个分配预算、379 次
实际失败、110 个句柄发布预算和七类真实叶目录通过；850 处新坏合同及
75 个模块／C／头输出门通过，上一批 399／37 项合同与输出门也继续通过。
记录读取不拉入验证／密码学／TLS，Name 比较和签名元信息分别按需裁剪，
仅枚举程序不拉入 X.509，旧接口无 X.509 残留依赖。

证据根 `F:/xlang-validation/five-groups-20261023/p5-x509-records`；全部输入、
日志、产物、源码驱动身份独立复核，Linux 原件及 F 盘镜像同时保留，最终
提交身份见 `verified-commit.json`。验收针对冻结的完整工作区，不声称纯净
checkout／发布 DLL／SO／VFS 已通过。没有修改独立 XRT、快照或时间 ABI。
此层不关闭 profile、约束、撤销策略、路径、信任库、身份匹配或 TLS。

后续 typed profile／distribution／name_constraints 的实际进展与公共合同见
[X.509 typed profile](x509-profiles.md)。该批增加 85 个精确驻留入口、27 个拥有
结果、11 个真实叶策略及四个枚举；复用 XRT，无 Cgen 算法或符号特例。完整
profile／CRL 数据公共面的当前进展见 [CRL 条目与 typed profile](x509-crl-profiles.md)。
签名检查、类型化签名状态及独立 SPKI 已接入，详见
[签名检查与独立 SPKI](x509-signatures.md)。CRL policy、完整路径／信任／身份
验证与 TLS 继续。

CRL policy 的公共 API、拥有边界和验收见 [CRL 策略接入](x509-crl-policy.md)。
本层不关闭路径／建链、完整 typed 验证结果、信任来源、身份或 TLS。

另保留 P7 联合验收风险：把八组大量受管理临时值集中在一个顶层作用域的
初始用例，Windows／Linux 均触发 240 秒前端输出／规划超时。独立函数版本
能输出但 C 仍较大；正式字段核对复用小型校验函数，数据、断言和调用范围
不减少。原始诊断证据保留在本批 `api-*-r1`，不能将正式功能通过解释为长
资源作用域的编译性能／清理代码体积已经封板；这需要后续按通用 IR 清理
生命周期方案评估，不为 X.509 增加符号特例或 Cgen 补丁。

## 信任库与建链的原生拥有前置（2026-10-23）

独立 XRT `50649120e` 新增 `xrtX509StoreClone` 和 `xrtX509StoreMerge`：
前者重新绑定全部 DER／证书／锚视图，不重复执行源条目去重；后者复用已有
导入算法，失败回滚本次追加并保持输出计数不变。重复／自身合并不分配，
空库可克隆。内容事务不承诺回滚已增长的容量或原借用结构地址。

这是后续语言层“验证保留稳定版本、只有存在活跃快照的更新才深克隆”的
必要底座，不把每次验证变成整库复制。文件／系统来源可先独立装载，再合并，
不要求在共享信任库的保护区内执行外部 I/O。共享可变库仍由调用方同步；原生
函数没有宣称自动线程安全，语言版本管理／拥有目录也尚未实现。

三张已有正常固定证书的签名独立核对；原生分文件／单头、GCC O0／O2、TCC、
Linux ASan／UBSan 及独立重放为 Windows 18／Linux 24，共 42 阶段。每配置
14 克隆预算／九合并预算／18 次实际分配失败、部分追加回滚、独立生命周期及
四线程各 32 次克隆读取通过，账本平衡。没有网络、签名变异或原安全套件。
原生 X.509 文档的 94 函数／53 类型也已通过 G1/G2/G4/G5，补齐旧三个 API／
两个类型的缺节，并纠正旧配置可空、结果发布及搜索诊断描述。

证据 `F:/xlang-validation/five-groups-20261023/p5-x509-store`，原生正式回执
`verified-native-commit.json`；Linux 原件及 33 文件镜像、306 项独立重查保留。
八路径限定提交，其他 73 项跟踪修改和 CRC32 暂存重命名保留；原结构布局和
时间合同不变，已有单头无关漂移不升级。

当前 xlang 固定快照尚未同步这两个函数，语言公开面仍为累计 310 个驻留入口。
下一批定点回补并完成 Store 稳定版本、真实拥有目录、发行者匹配与候选建链，
继而文件／系统来源、身份和 TLS；本批原生前置不冒充这些能力已接入或发布。
