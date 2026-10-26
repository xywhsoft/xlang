# X.509 接入：完整模块归属与拥有数据边界

2026-10-26 更新：21 个模块已完成本轮角色复核，342 个驻留声明／ABI／导出／
XLIR 合同仍闭合；两端实际普通产品各通过 252 阶段。见
[普通产品验收及精确边界](x509-public-closure-2026-10-26.md)。下文阶段性的
“本批／继续”保留历史实现顺序，不应逐句当成今天的功能缺口清单。

状态：已有两个拥有数据类型及其 35 个源码 API、七类结构化记录及 68 个
精确驻留入口、11 类 typed profile／分发点／约束记录和 85 个入口；本批继续
接入三类 CRL 条目／IDP 记录和 45 个入口，及签名检查／独立 SPKI 的 15 个
入口。CRL policy 再补入 30 个驻留入口、四类公开拥有对象和一个内部回调
上下文。有序路径再接入 32 个入口、四类公开拥有对象和一个内部上下文。
信任库核心再接入 17 个入口及两个公开拥有类型，发行者／建链再补入三个入口，
搜索预算再补一个只读属性及策略构造参数，信任来源再补八个入口，身份匹配再补三个，累计 342 个驻留入口、
33 类公开拥有数据类型（另有两个内部上下文）；
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
| x509_path | 有序路径、独立 trust anchor、用途／时间／critical 扩展策略 | ordered 验证、显式锚、不可变策略／结果、拥有路径、checked 回调及发行者候选匹配已接入 |
| x509_path_build | 候选 issuer、独立 anchor、回溯建链、成功时拥有的路径与 typed 结果 | 受管理快照上的公开建链、不可变策略搜索预算及双端路径专项已通过；完整 P5 仍须身份及联合封板 |
| x509_store | TrustStore 导入／去重、证书／锚读取和不可变验证快照 | 空库、DER／PEM、合并、独立可变克隆、稳定快照及拥有投影已接入；建链源组合继续 |
| x509_store_file | 文件 DER／PEM 的事务导入，与现有 file／VFS 边界协调 | 原生通用数据导入、硬限额文件／VFS 读取及工厂已接入；锁外 I/O、稳定快照、失败不发布 |
| x509_store_system | 显式装载当前 OS 信任库快照，不隐式信任、不联网补链 | 只读系统来源、直接采用的不可变／可变工厂与事务合并已接入；不写 OS 根库 |
| x509_identity | DNS pattern／引用主机和 SAN 中 DNS-ID／IP-ID 的严格匹配 | 三个精确驻留入口、布尔零分配及独立拥有的可空命中记录已接入；完整联合验收继续 |

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

在上述原生前置提交时，xlang 固定快照尚未同步两个函数，语言入口仍为 310。
其后的信任库核心接线见下一节；发行者匹配、候选建链、文件／系统来源、
身份和 TLS 继续，不能以原生前置或信任库数据操作代替这些能力已接入或发布。

## 受管理信任库核心与稳定版本（2026-10-23）

本批公开 `TrustStore` 与 `TrustStoreSnapshot`，17 个驻留入口、七个拥有结果、
两个真实叶目录。四份实现／声明头只回补原生 `50649120e` 的 Store 和公开声明
差分；固定微秒／Until 与独立 XRT 毫秒／For 各自保持。回补身份与源哈希在
`lib/xrt/backports/x509_store_ownership.json`，没有整体快照／时间 ABI 升级。

| 类型 | 公开面 |
|---|---|
| `TrustStore` | 空构造、`count`／`byteCount`、`at`／`anchorAt`、`add(Certificate)`、`addPem(text,size=-1)`、`merge(snapshot/store)`、`clear`、`clone`、`snapshot` |
| `TrustStoreSnapshot` | 只读 `count`／`byteCount`、独立拥有的 `at`／`anchorAt`；不可直接构造或修改 |

创建空库不加载系统根，也不自动授信。`add` 是调用方显式将证书作为信任锚，
不验证其信任、有效期或自签名；精确重复返回 false，合并／PEM 返回新增数量。
PEM 的 size 按原始字节选择前缀，-1 取全长，超过实际长度截断，其他负值拒绝；
不用 strlen。byteCount 是唯一 DER 总字节，不是容量或所有堆对象体积。

可变包装内部串行化更新。快照只保留不可变原生版本，底层版本取得不分配，
但语言结果的 handle／class 包装仍可能分配；不宣称整次语言调用零分配。
clone 创建独立可变包装、初始共享版本；存在活跃快照／其他包装时才在更新前
深克隆，否则复用唯一版本。普通导入／分配失败保持原逻辑内容和 byteCount；
可能已增长的原生容量不承诺回滚。新可变 clone 使用保守效果，不标 pure。

快照的多次查询属于同一版本；对可变库分开的两次查询不保证看见同一版本。
所有证书／锚投影独立拥有，既不会因库清空或销毁失效，也不长期钉住整个库。
merge(store) 先取得来源快照，再进入目标写入，不同时持有两个库锁。私有锁
不跨语言回调或外部 I/O；公开查询／更新不误标 nonblocking，快照标量读取
不进入可变包装。版本只有原子引用、原生 Store 和 DER，无闭包或语言拥有边。

双端完整源码为 1097／1096 TU。本批语言每端 69 阶段已通过，17 个精确目标、
七个拥有结果、233 处坏合同、19 个 module／C／header 输出拒绝门均通过。
驻留层每配置 132 个分配预算／99 次实际失败／四个发布预算／两个目录、
四线程各 32 次读及 32 轮清空／追加通过；GCC O0／O2、TCC、Linux ASan／
UBSan 与账本覆盖。Cgen 没有新增命名包装、算法或查找特例。

最终 native／api-r3、八组 regression-r2 及独立原生重放通过 Windows 853／
Linux 856，共 1709 阶段；旧轮结果不混计。全部输入、对象、驱动、头文件、
日志及产物再次独立哈希，Linux 原件和 F 盘镜像同时保留。这是冻结完整
工作区的源码验收，不等于纯净 checkout、发布 DLL／SO 或 VFS 验收。

正式联合回执以 `F:/xlang-validation/five-groups-20261023/p5-x509-store-language`
中的 `verified-commit.json` 为准。本轮保留 r1 清单漏登记及 r2 裁剪检查的
诊断：前者已补入中央资源策略表；后者改为可达的 nullable 快照函数，并检查
实际 Store 来源，不把公共启动层的 Mutex 归因给信任库。原始证据不覆盖。

候选建链／发行者匹配、文件与系统来源、身份、TLS 和联合发布仍未完成。
另有 P7 映射检查：旧 `xl6_artifact_native_symbols.inc` 尚未包含新 PathCheck／
BuildCheck／StoreClone／StoreMerge 四个底层名字；本批驻留 provider 显式选择
模块，不依赖该反查。显式 FFI 的制品映射仍须统一刷新验收，不能以 JIT 低层
符号已登记代替 AOT 映射完整。长期任务继续 active，不声称 shipping 已通过。

## 候选建链的空信任源合同（2026-10-23）

为接通稳定信任快照上的候选建链，先压实原生空库边界。独立 XRT
`0dd411185` 使 `xrtX509StoreSource` 接受空库／零候选；合法空锚源经
`xrtX509PathBuildCheck` 给出 `REJECTED`／`X509_ERROR_PATH_BUILD`，depth 为
`SIZE_MAX`，不分配搜索帧或诊断，不发布成功路径／结果，也不覆盖已有线程
错误。非法的非零计数／空数组仍是操作错误，不被空库快捷路径掩盖。旧 bool
入口继续为完成拒绝给出 NOT_FOUND 诊断，没有增加隐式系统授信。

原生分文件／单头、GCC O0／O2、TCC、Linux ASan／UBSan 和独立重放共
42 阶段。三份已有正常固定证书的签名由已有独立 oracle 核对，没有执行原
密码安全套件；旧 Store 测试中的空库断言同步更新，但未运行其原安全套件。

四份 xlang 实现／声明头只应用上述两个源和公开注释的精确差分，记录在
`lib/xrt/backports/x509_empty_trust_source.json`。原微秒／Until、独立原生
毫秒／For、所有未选中段、API 数量／符号表／能力矩阵／deferred 清单不变。
四种实现／声明组合各用四类正常证书验收：Windows 36／Linux 48，共
84 阶段，含独立重放、已有错误及失败输出保持、零搜索分配和内存平衡。
首轮只有新夹具遗漏 const 的严格编译失败，r2 修正并完整重跑；原报告保留。

证据根 `F:/xlang-validation/five-groups-20261023/p5-x509-empty-source`，原生
回执 `verified-native-commit.json`，xlang 回执 `verified-xlang-commit.json`。
这是原生和快照支撑验收，未增加语言公开入口，仍为 327 个；不是候选建链
语言接口／JIT／AOT、完整源码重建、TLS 或发布验收。后续继续发行者匹配、
候选建链及其 typed reason／回调接线、文件／系统来源与身份验证。

## 发行者匹配与快照建链（2026-10-24）

`Certificate.matchesIssuer`、`X509Path.build` 的快照／可变库重载及 checked
critical 回调已经接线。新增三个精确驻留入口，路径组由 32 增至 35，复用
现有结果／路径／策略／回调上下文和五个叶目录；未增加同义拥有类型或回调
ABI。功能、预算及边界见 [候选建链](x509-path-build.md)。

数组先独立保留句柄，再解除占用；可变库先快照，不持锁执行用户代码。只有
成功选中的路径、锚和用途复制为独立拥有块，不复制整个信任库。原生搜索、
完成决定、路径复制、数组捕获与 checked 分派分别共用，ordered／build 不维护
两套状态分类或从原因链推测结论。Cgen 没有新增算法、命名包装或符号特例。

正式冻结副本基于 `8d156f5c5` 的完整编译器／runtime 工作区，8990 个输入文件
在两端逐字节一致；包含验收期间合入的 C-owned 编译器前置，并保留工作区
和暂存区。完整源码为 Windows 1106／Linux 1105 TU。每端九组语言／回归共
900 阶段，加驻留及独立重放后为 912／915，共 1827；路径专项各 130 阶段、
35 目标／13 拥有结果／534 坏合同／40 输出门通过，10 份真实裁剪计划证明
普通数据／ordered 不拉建链，issuer 查询不拉信任库，build 不拉 ordered 适配。

合并驻留专项每配置 55 分配预算／43 实际失败／10 发布预算／五目录，含
四种正常链、原始／缺省诊断回调失败、重入及用户清空输入／信任库；GCC
O0／O2、TCC、Linux ASan／UBSan 通过。另有每端六阶段的参数拥有重入专项：
清空目标、策略、快照的最后外部数组持有者后，JIT／独立 AOT 调用与结果仍有效。

中间失败不覆盖：r1 修正零候选与暂存区重叠及严格编译诊断；语言修正声明
闭包、分组声明位置与夹具的 rejected／unsupported 误判。r2 期间外部 Core
合入使输入哈希变化，不能混计为最终结果，故切换一致冻结副本。r2 另有一次
Windows AOT 的 storage-release ABI 诊断；同一输入六次独立构建／执行及 r3
完整签名专项均通过，但原始原因尚未确认，不能宣称该问题已被修复。

证据根 `F:/xlang-validation/five-groups-20261023/p5-x509-build`，正式回执
`verified-commit.json`；全部输入／对象／驱动／头文件／日志／产物独立复核，
Linux 原件及镜像保留。能力矩阵为 3839 低层、2202 直接接入、2394 抽象使用、
零延期；只新增三个 X.509 原生调用引用。不是纯净 checkout、最新活动工作区
或发布 DLL／SO／VFS 验收；没有网络／原密码安全套件。

P5 仍需原生搜索工作预算、文件／系统信任来源和身份；TLS、低层新函数静态
映射、长拥有作用域成本、上述一次性 AOT 诊断及联合 shipping 继续，不关闭
整个长期目标。未决策 CRT 0／XRT 11，对象池最后。

## 原生搜索预算与定点回补（2026-10-24）

独立 XRT `a31852464` 的 `xrtX509PathBuildCheckLimit` 已接入四份实现／声明
快照及低层符号面。候选／锚访问和完整路径验证预先扣减本次调用的预算，
耗尽是 checked Range／ERROR，绝不交付完成拒绝或半成品；零值明确不限额。
共用原生算法，计数器属于栈上调用，不改变结构布局、所有权或两边时间合同。

原生专项／正常回归与四消费者专项各 84 阶段，共 168；GCC O0／O2、TCC、
Linux ASan／UBSan 及独立重放通过，包含正常链、分配失败、精确边界、嵌套
计数独立与无诊断回调失败；中间 TCC 测试初始化诊断保留，没有生产代码补丁。
证据及边界见 [建链](x509-path-build.md)。矩阵为 3840 低层／2202 直接／2394
抽象使用／零延期；公开拥有类型及驻留语言入口仍为 33／330。

在该原生回补批次结束时，语言级可选预算参数尚未更新；后续语言接线见下节。

## 语言级搜索工作量预算（2026-10-24）

X509PathPolicy 新增 maxSearchSteps 构造参数及只读属性，默认 100000、零显式
不限额、负值 checked 拒绝。它与已有深度／字节预算共用不可变策略及拥有图；
普通和回调建链直接传给原生限额入口，不增加每次配置对象分配或 Cgen 特例。
策略工厂为七参数，回调入口形状及通用合同表保持。路径组累计 36 个驻留入口，
P5 累计 331，仍为 33 类公开拥有类型及两个内部上下文。

双端路径专项各 143 阶段通过，覆盖 36 目标／13 拥有结果、547 错误合同变体及
41 输出拒绝门、十一份真实裁剪计划。原生每配置的 55 分配预算／43 实际失败／
10 发布预算／五目录通过，新增四类正常链的工作边界与回调前停止；精确限额
下清空目标／快照／策略外部最后拥有者的 JIT／AOT 检查各六阶段通过。联合
验收轮次、Windows 资源诊断及未关闭的间歇问题以专项最终回执为准。

最终证据根 `F:/xlang-validation/five-groups-20261024/p5-x509-managed-work`，
回执 `verified-commit.json`：每端九组语言／回归 913 阶段，加驻留／独立重放
为 Windows 925／Linux 928，共 1853；参数拥有十二阶段另计。8799 份稳定
输入（另 194 份生成缓存不作源码证明），完整源码 1106／1105 TU。Windows
初轮的 OOM／load 断言原件保留，只在同一生产代码、测试诊断叠加和单组并发
下重验两个失败组；不冒称初轮通过或原始断言原因已经查明。详见专项轮次。

## 文件／VFS／系统信任来源（2026-10-24）

接口及边界见 [信任来源](x509-trust-sources.md)。原生 `e52d91421` 将已有
文件格式识别抽为 `xrtX509StoreAddData`，内存与文件共用事务；原生 84 阶段
通过，定点同步四份消费者，保留两边时间 ABI 与全部未选中段。

语言信任库组由 17 增至 25 个精确入口，P5 累计 339。两端本批各 116 个
公开语言／ABI／裁剪阶段通过：25 直接目标、9 拥有结果、342 错误合同及
27 输出拒绝门、九份真实计划。数据输入只借用实际字节片段；文件／VFS 的
64 MiB 默认硬限额在锁外读取；系统源显式、只读，不联网或修改 OS 根库。

驻留及独立重放为 Windows 21／Linux 27 阶段，本批来源合计 280，加原生
84 为 364。新导入的 Windows 42 分配预算／37 失败、Linux 41／36 均验证
回滚、旧版本稳定、实际 DER 字节计数及内存平衡；系统快照完成只读实测。
全量源码编译为 1111／1110 TU，8818 份稳定输入逐字节冻结，两端一致。

证据根 `F:/xlang-validation/five-groups-20261024/p5-x509-store-input`，本批
提交回执为 `verified-focus-commit.json`。这是冻结工作区的来源专项，不是
clean-checkout／shipping 验收；已有九组总回归仍独立运行，不计成通过。
之前未查明的 Windows load／lower 断言和 AOT storage-release 诊断不关闭。
后续继续总回归、身份匹配、TLS 及 P7；完整 P5／目标仍未完成。

来源批次后续九组回归已完成：每端 960 个语言／回归阶段，加驻留与独立
重放为 Windows 981／Linux 987，共 1968；加该批原生 84 为 2052。单组调度
首轮九组均通过，Windows 最低系统可用提交内存为 772669440 字节。独立
复核 26343 份证据文件，Linux 原件及 18195 份镜像保留，最终回执为
`verified-full-regression.json`。Windows 驻留补验使用同一冻结源码，避免后续
工作区头文件变化污染证据。身份新批次、最新工作区和 shipping 均不在此
回执范围；历史两个 Windows 诊断仍不以这次通过作为根因已修复的依据。

## 严格名称匹配（2026-10-24）

`x509.matchesDns`、`Certificate.matchesHost` 和 `Certificate.matchHost` 补齐
三个驻留入口，P5 累计 342。布尔路径不分配名称；详细结果为既有 GeneralName
的独立拥有副本，不钉住原证书，普通不匹配为 false／null，非法输入 checked
报错。完整逻辑长度及显式截断前缀沿用标准规则，不使用 strlen 或暴露原生借用。
名称匹配不代替路径、用途、时间、撤销和信任验证。

XRT `af2f24f12` 修复数字地址视图中的 NUL 静默终止问题，定点回补保留签名、
布局、全部未选中段及两边时间合同。两端本批身份／受影响数据／profile 各
208 阶段，加驻留与独立重放为 220／223，共 443；原生 42，合计 485。
完整源码构建 1114／1113 TU，8837 份稳定输入两端一致，Cgen 未新增算法。

首轮函数计数静态检查失败及 private 负例语法错误原件保留，分别通过既有
索引生成器和单一测试夹具修正。后者仅使用测试叠加、生产字节及编译器对象
保持不变；不虚报原轮通过。证据、裁剪及错误／拥有合同详见
[名称匹配](x509-identity.md)。本批不含网络／安全专项，不声明最新完整工作区、
P5 全需求或 shipping 已完成；继续需求审计、TLS 和 P7，不关闭历史间歇诊断。

## 公开面归属核对

[逐 API 归属审计](x509-surface-audit.md)确认 342 份语言声明／ABI／导出／
XLIR 合同闭合：338 份直接合同及四份共享 collection 合同。96 个原生 API
中 84 个由驻留适配直接使用，其余十二个逐项说明拥有式或有界的等价公开
能力，没有为了覆盖调用计数再添加重复包装。此核对不新增运行通过阶段，
也不代替 P6／P7；旧 TLS Cgen 引用及完整发布联合验收继续保留。
