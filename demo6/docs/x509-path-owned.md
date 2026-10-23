# X.509 有序路径：拥有底座与语言接口

状态：拥有底座之后，已接入 31 个驻留入口、四类公开拥有对象、一个内部
critical 上下文、三个枚举和五个真实叶目录；最终双平台联合源码验收已通过。
它仍不代表 path/build/store/identity 或完整 X.509 已接入。

上述正式验收绑定提交 `8e2f2a6e6` 的冻结工作区。之后另一项 C-owned 编译器
合并 `2d1471ceb` 改变了 AST／IR／语义及 Cgen，但未改本批路径实现。已使用
双端 source-r3／native-public-r5／api-r4／regression-r2 重新验收，1571 阶段
再次通过。合并前后证据分开保留；后者见 `verified-postmerge-commit.json`。

最新原生进度：独立 XRT `c980bc84b` 已完成路径／建链显式结果及吞错修复，
双平台 138 阶段通过。xlang 固定快照和驻留分类器尚未切换；本页 31 个
语言入口仍是原接线，下一批定点同步新结果合同并清退原因链分类器。

## 实现边界

- `xl6_x509_anchor.c` 从显式 Certificate 复制 DER，并由 XRT 提取独立信任锚。
  这不是认证该证书，也不把它隐式加入待验证路径。
- `xl6_x509_path_validate.c` 复制目标在首的有序证书链、独立 anchor 和用途 OID，
  建立自有解析索引，再调用固定的 `xrtX509PathValidate`。不复制路径验证算法。
- `xl6_x509_path_result.c` 保留类型化接受／拒绝结论、精确原生原因码与原诊断。
  只有验证成功才保留并交付路径；拒绝结果不交付半验证路径。

路径索引、DER 和用途内容在一个分配内；结果外壳另有一个分配。子证书及锚
投影独立拥有，不因输入或父路径释放而失效。没有隐藏语言对象／闭包边，三类
原生 leaf policy 都使用现有 handle 发布／清理合同，未另建拥有图。

时间由调用方显式给出，固定快照仍使用微秒。证书数量和总输入字节分别受预算
控制；总字节包含路径 DER、anchor DER 和用途 OID。零显式关闭预算，负值拒绝；
索引／字节求和先检查溢出。用途 OID 交给现有 DER 库检查，不能绕过总字节预算。

## 错误及回调

接受、拒绝和已明确报告的后端不支持分别表示。拒绝不等于“签名损坏”：固定
XRT 的 `X509_ERROR_PATH` 同时包含时间、路径形状等拒绝，signature 类也可能是
算法策略拒绝。底座保留原生原因，不用消息文本猜出更精细的结论；后续公开
typed 结果不能宣称原生未提供的粒度。

只识别已知 `xrt.x509` 高层验证 envelope 和已知签名 cause。错误域使用精确
字节长度比较；资源、参数、内部错误、未知域／未知 cause 都保持 checked
失败。即使外层为路径／签名拒绝，原因链中的 OOM 也不能吞成正常拒绝。
`require` 重发保留的原始诊断，不构造替代错误；清理保留已存在的错误槽。

内部同步 C critical callback 由栈上代理跟踪：返回 ERROR、设置错误或返回非法
状态都保持 checked 失败。失败但没有诊断对象也通过既有错误作用域保留失败位，
不被 native 的 critical-extension 包装变成普通拒绝。接受／未处理仍是两种
正常返回。发布路径将 Critical／UserData 清空，绝不保留回调、栈上下文或代码
地址。语言闭包、代码租约、投影片段及 context 生命周期仍是后续公开接线工作。

## 验证边界

`tools/check_x509_path_owned.py` 使用 RSA、P256、P384、Ed25519 四组正常离线
证书链，每组 root／intermediate／target／自定义 critical target 四张证书。
固定公开测试密钥复用现有夹具；cryptography 独立核对全部 16 张证书的签发。
不执行 XRT 原有安全测试，不使用网络、畸形密码数据、扫描或攻击模拟。

覆盖接受、显式时间／预算／配置拒绝、原诊断保留、未知域和 wrapped OOM
不误分类、critical 接受／拒绝／资源失败／无诊断失败、输入和父对象销毁、
独立投影、三个 handle 的发布失败／成功及四线程各 32 次只读。

底座批次的历史结果记录在 `F:/xlang-validation/five-groups-20261023/p5-x509-path`。
正式边界为双端 `native-*-r5`：Windows 9／Linux 12，共 21 个阶段通过，
包括 GCC O0／O2、TCC 和 Linux ASan／UBSan 及每个二进制的独立重放。
每配置 19 个分配预算、14 次实际失败、三个 handle 的六个发布预算通过，
内存账本精确平衡。每组四线程各 32 次只读，总计四组正常链；所有 16 张
证书签名独立核对。输入、产物与日志哈希重查，Linux 原件及逐文件镜像
同时保留。r1 编译诊断、r2—r4 的中间结果保留，不算作最终成功阶段；
正式提交身份见该目录 `verified-commit.json`。

## 公开 API 与驻留接线

| 类型 | 公开入口 |
|---|---|
| `X509TrustAnchor` | `X509TrustAnchor(certificate)`；独立拥有的 `certificate` 投影 |
| `X509PathPolicy` | `X509PathPolicy(at, flags=none, keyUsage=none, purpose="", maxCertificates=64, maxBytes=16777216)`；六个只读属性 |
| `X509Path` | `check(array<Certificate> certificates, anchor, policy)` 与 critical handler 重载，返回 `X509PathOutcome` |
| `X509PathOutcome` | `status`、`reason`、`path: X509Path?`、`requireValid()` |
| `X509Path` | 成功结果的 `count`、`at(index)`、`anchor`、`checkedAt`、`flags`、`keyUsage`、`purpose` |

数组中目标证书在首，随后为有序中间证书，独立锚不放进数组。创建锚是调用方
显式赋予信任，不验证该锚证书的时间或自签名，也不是系统自动授信。

策略不可变，时间显式。purpose 为点分十进制用途 OID，空字符串表示未请求。
keyUsage／purpose 指定允许用途；相应 require 标志额外要求扩展必须存在。
none 不关闭签名、时间或 CA 检查，也不允许 SHA-1；后者必须显式 allowSha1。
构造策略时，maxBytes 分别限制用途文本和编码，验证时限制 DER＋编码 OID 的
总字节。预算零为显式无限，负值 checked 拒绝。

`X509ValidationStatus` 为 valid／rejected／unsupported。`X509PathReason` 使用
精确原生高层原因码：none、signature、issuer、path、constraints、criticalExtension、
purpose、nameConstraints；不从诊断文本细猜“过期”或“签名损坏”。原生未细分
的算法策略拒绝仍按其原始类别保留，不能宣称所有不支持情况均被细分。
requireValid 在负结论上抛出原诊断；path 为 null，资源或回调失败根本不返回
正常 outcome。成功 path 投影共享不可变原生存储的原子引用，不复制整条链；
子证书和锚仍独立复制，避免仅保存一张证书却长期钉住大型路径。

critical 为同步 `func(Certificate, X509Extension, int depth) as bool`；参数均独立
拥有，可保存到持有对象。true 接受未知扩展，false 表示未处理，异常保留
原错误。语言 helper 捕获显式上下文并复用现有 nullary checked bool 闭包及
真实代码租约；没有新增回调 ABI、TLS／FLS 或生成端包装算法。

输入集合采用已有密封原生类数组合同，不遍历动态字段猜类型。收集并保留
原生证书 handle 后立即结束数组 pin，之后才进入验证／用户回调；回调可以
正常读取甚至清空原数组，验证仍使用完整拥有快照。上下文只有活跃所属线程
可访问，拒绝同上下文重入及跨线程读取；结束后清空所有借用字段并退役 owner。
不保存闭包或模块数据。失败而无诊断对象也保留独立 checked 失败位。

ABI、枚举、owner、nullable 结果、集合物理合同及效果均在现有 XLIR 注册体系
登记；Cgen 只使用这些声明。provider 按策略／数据／验证／回调分离：纯枚举
不引入 X.509 实现，创建纯策略不引入路径／密码验证，普通验证不拉入回调。

## 后续仍须完成

发行者候选匹配、候选回溯建链、
TrustStore／文件／系统来源、身份匹配和与 CRL/TLS 的组合。固定原生返回的
结论粒度与建链时资源错误处理
仍需逐项核对，不能用通用 false、消息匹配或 Cgen 分支掩盖。

已确认一项建链依赖缺陷：固定原生 `xrtX509PathBuild` 会把候选分支的 OOM
当作可继续搜索的验证拒绝，后续正常候选成功时释放该错误并返回成功。
证据根的 `build-oom-probe-r1` 使用四张正常、独立核对签名的证书、两个合法
CA 用途配置和 FailAfter(0)，实测 `accepted=1 fault=1 kind=0 swallowed=1`。
没有网络或畸形密码输入。当前 ordered 接口并不选择 PATH_BUILD，故不受这条
未接入路径影响；候选建链接入前必须修复独立 XRT 并定点同步固定快照，
不能在 Cgen 或语言适配层用重试／吞错掩盖。

`build-callback-resource-probe-r1` 又以正常 CA 的自定义 critical handler 返回
X509_ERROR／Memory 复现：已有另一候选的用途拒绝时，最终结果变成 NotFound，
原因链中 Memory 完全丢失（`accepted=0 kind=8 memory_in_chain=0 swallowed=1`）。
这不仅是可选诊断分配的恢复问题；原生建链必须明确区分可回溯的验证拒绝与
必须终止的操作／回调失败。优先补足原生 typed decision 与结构化原因，共用
现有验证算法；不要把更复杂的域名／消息猜测继续堆到驻留适配层。

独立 XRT、固定快照、时间 ABI、Cgen 和发布资产均未修改。完整目标继续 active。

## 公开接线正式验收（2026-10-23）

Windows 784／Linux 787，共 1571 阶段通过，包含原生独立重放；完整源码
构建为 1087／1086 TU，不混入阶段数。每端新路径 API／IR／裁剪 71，七组
回归合计 701：CRL policy 93、CRL profile 115、typed profile 93、结构化记录
92、基础数据 59、签名 104、完整密钥格式 145。回归每端最多三组并行。

31 个入口、11 个拥有型结果、450 处坏合同、36 个完整模块／C／头输出拒绝门
通过；包括既有密封类数组与 checked callable 的物理合同。JIT、module、
独立 AOT 和无发布运行库依赖检查通过；纯枚举、纯策略、普通验证及回调的
裁剪门通过，没有拉入 path_build／store／TLS。

原生专用每端 9／11 阶段，加独立重放 3／4。每配置覆盖四组正常链共 16 张
独立核对签名的证书、36 个分配预算、28 次实际失败、五类 handle 的十个
发布预算、五个真实拥有目录，以及每组四线程各 32 次只读；内存账本平衡。
回调原异常、失败无诊断、同上下文重入拒绝、片段留存及清空原数组后仍可
完成验证均通过。GCC O0／O2、TCC 和 Linux ASan／UBSan 覆盖。旧有底座
独立检查工具也在当前拆分与引用计数布局下复测通过，未另混计主阶段。

正式边界：native-public-*-r4、source-*-r2、双端 api-r3、regression-r1。
r1—r3 的原生中间诊断与 r1—r2 的 API 结果保留，不冒充最终通过。早期测试
harness 曾误将读取常量的夹具依赖源加入编译，导致多 main 链接失败；修复
了编译入口，没有执行这些原测试。语言回调新增用例的数组属性由误写 count
修正为 length，未修改语言语义规则。原生实际修复是提前结束源数组 pin，
并以真实语言／C 回调清空输入的回归压实。

输入／驱动／对象／日志／产物独立哈希重查，Linux 原件及逐文件 F 盘镜像
同时保留；本批提交身份见证据根 `verified-api-commit.json`。底座历史的
`verified-commit.json` 不覆盖。无关工作区、暂存区及 RSP 既有重构均保留。

这是冻结完整工作区的源码验收，不是纯净 checkout／发布 DLL／SO／VFS 或
完整 P5 验收。长资源作用域的通用 IR 性能／清理 C 体积风险仍开放。没有
进行网络流量、原生网络安全测试、畸形密码数据或攻击模拟。

## C-owned 编译器合并后的独立复测

在 `2d1471ceb` 合并之后重新构建 1087／1086 TU，重跑同一完整矩阵：Windows
784／Linux 787，共 1571 阶段再次通过，未复用先前的验证结果。31 个入口、
11 个拥有结果、450 处坏合同与 36 个输出门，以及 36 个分配预算、28 次失败、
十个发布预算／五个目录的原生专项均通过。本批路径实现和声明没有改变。

正式边界为 source-*-r3、native-public-*-r5、双端 api-r4、regression-r2。
驱动／输入／日志／产物再次独立哈希复核，Linux 原件与新的
`postmerge-linux-evidence` 镜像均保留；合并前的全部证据和两个原提交回执
不覆盖。当前证据与提交身份见 `verified-postmerge-commit.json`。

这是路径与七组既有标准库对该编译器合并的集成验收，不代替 C-owned 特性
自身测试，更不是发布验收。上述两个原生建链错误仍未修复，且当前接口不
选择 PATH_BUILD。下一批先补原生显式验证／操作失败分流，再接入候选建链。

## 原生验证结果底座（2026-10-23）

独立 XRT 已提交 `65e2ee2b9`：RSA PKCS#1／PSS、raw／DER ECDSA、Ed25519
内部先返回 `xrt_crypto_check`，旧公开 bool 入口在需要时再构造诊断。数学
算法及临时数据清零仍只有一份；结果不根据线程当前错误、消息或原因链猜测。
这是后续路径／建链 typed decision 的必要前置，不是 PathBuild 吞错修复本身。

顺带压实两项内部边界：Ed25519 上下文参数检查与 hash init 共用纯谓词，
坏参数不分配错误对象；ECDSA DER 入口先检查暂存容量再解码。没有新增公开
密码 API，也没有改变公开声明、两边时间 ABI、Cgen 或 xlang 固定快照。

独立干净原生源码加本批范围的分文件／单头验收：Windows 18／Linux 24，
共 42 阶段（28 个构建／运行阶段及 14 次独立重放）。GCC O0／O2、TCC 与
Linux ASan／UBSan 通过；五组正常固定签名由独立实现核对，13 项参数决定、
零分配、既有错误保留、旧错误类别和资源账本通过。没有执行原密码安全测试，
没有签名变异、畸形输入活动或网络流量。中间 r1 保留，正式边界为 r2。

证据根 `F:/xlang-validation/five-groups-20261023/p5-x509-decisions`，
`verified-native-commit.json` 记录原生提交和独立哈希证据；Linux 原件及
316 文件镜像均保留。17 个提交路径中配置／单头均只纳入本批内容，保留原生
CRC32 已暂存重命名及其他工作区修改。12 个单头源码段与重新生成结果一致，
其他生成段原先已有的 HEAD 漂移不顺带纳入，因此不宣称全单头 freshness。

接下来把原生路径验证与候选搜索改用显式结果，再定点同步到 xlang。当前
PathBuild 的两个已确认错误仍未修复／未接入；本轮 42 阶段不冒充 xlang 的
新源码或发布验收，整个 P5／TLS／联合发布目标继续 active。

## 原生路径／建链错误边界已修复（2026-10-23）

独立 XRT 提交 `c980bc84b`，新增 `xrtX509PathCheck` 与
`xrtX509PathBuildCheck`，显式区分 VALID／REJECTED／UNSUPPORTED／ERROR。
普通拒绝只产生值，带稳定原因、证书深度和仅用于展示的静态文本；错误文本、
线程当前错误和原因链不参与搜索决策。旧 bool 入口复用同一算法后按需报错。
既有公开结构布局／签名与两边时间合同未变；只增加两个 API 和新的结果类型。

移除了建链“缓存第一项错误并继续”的机制。只有完成的拒绝／不支持才能
回溯；名称／发行者匹配、解析、帧分配及 critical handler ERROR 立即终止，
原错误甚至“失败但无诊断”均保留。ERROR 不发布决定，result 只在 VALID
发布；路径数组明确只是搜索暂存区。名称约束和签名验证共用显式结果底座，
旧密钥原因链的 Value 类别亦保留，不误改为 Protocol。

冻结的三组常规检查及原复现双平台通过 Windows 60／Linux 78，共 138 阶段，
含 46 次独立重放：分文件／单头、GCC O0／O2、TCC、Linux ASan／UBSan。
22 张正常证书全部独立核对签名；每主路径运行实际覆盖 12 个归一化分配失败
和一个帧分配失败，以及普通用途／名称约束备选、原错、无诊断错误、准确
原因与深度、输出发布和账本。没有原生安全套件、签名变异、畸形密码输入或网络。

原四张正常证书的两份复现程序也直接重跑，只绑定本批原生头和按已解析证书
确定测试时间：普通备选搜索 accepted=1／fault=0／swallowed=0；回调资源失败
accepted=0／原 Memory 保留／swallowed=0。前者是取消了拒绝诊断分配，不以
“未触发故障”替代真正的分配失败覆盖。最终矩阵 r2／原复现 r3，全部中间
证据保留；原复现早期检查曾误写 Memory 数字，修正为按原生枚举核对，未改库。

证据根 `F:/xlang-validation/five-groups-20261023/p5-x509-path-check`，
回执 `verified-native-commit.json`；823 文件再次独立哈希，Linux 原件及
427 文件镜像保留，Windows r1 的冻结源码另存。20 路径范围提交保留全部
无关工作区／暂存区。只同步 10 段实现差分和一段声明差分，已有 SPKI 声明
顺序及其他生成漂移保留；不宣称全量单头 freshness。

这是原生前置验收，不是 xlang 新快照、候选建链公开面、完整 P5 或发布验收。
下一步定点回补，驻留适配直接消费原生结果并删除 xl6X509PathClassifyV1，
再继续候选发行者、build／store／identity、TLS 和联合验收。Cgen 本轮未改。
