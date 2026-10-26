# TLS CRL 策略接入

本批实现用户已经批准的规则：默认 Verifier 不启用 CRL；显式传入
`TlsRevocationPolicy` 后，默认严格拒绝未知、无适用来源或覆盖不完整的结果。
`allowUnknown = true` 是显式宽松选项，不会忽略回调异常、目标／时钟／来源
绑定错误或已经确定的 revoked 结果。没有网络获取、隐式线程或 Cgen TLS 特例。

## 公开模型

- `TlsRevocationPolicy(provider, allowUnknown = false, skew = 0,
  flags = X509CrlPolicyFlags.strict, maxSources = 64, maxSignerCertificates = 64)`。
  provider 的精确类型为 `func(TlsRevocationRequest) as X509RevocationCheck?`；
  返回 null 表示未知，不表示 good。配置创建后不可变。
- `TlsRevocationRequest` 只由驻留运行时创建，拥有不可变快照。公开 `peer`、
  `target`、`issuer`、`index`、`verifiedAt`、`skew`、`flags`。不公开原生指针、
  setter 或用户构造。`peer` 是已经完成默认验证的 `TlsVerifiedPeer`。
- `TlsVerifier(trust, policy, revocation)` 及
  `TlsVerifier(trust, policy, additionalPolicy, revocation)`。
  默认信任路径／用途／名称验证先成功，再执行 CRL，最后执行附加谓词。
- `X509RevocationCheck.withCrl(source, signerPath)` 和
  `withSet(source, signerPath)` 补充实际签名者路径证据；原有单参数方法保留。

组合方式示例：

```xlang
func checkedVerifier(TrustStoreSnapshot trust, TlsVerificationPolicy policy,
    func(TlsRevocationRequest) as X509RevocationCheck? provider) as TlsVerifier
    return TlsVerifier(trust, policy, TlsRevocationPolicy(provider))
end
```

provider 使用已有 `X509ValidatedCrl.fromCrl`／`X509CrlSet` 完成 XRT 原生验证，
再从 `X509RevocationCheck(request.target)` 聚合来源；必须使用请求给出的
验证时刻、skew 和 flags。没有重新实现 CRL 选择、签名、delta 或原因覆盖算法。

## 绑定与范围

请求按 **selected path** 的实际次序逐个覆盖非锚证书，不按 presented chain
次序猜测。issuer 是下一张已选证书或独立信任锚。结果必须属于同一 DER 目标和
同一次时钟／策略；持久证据链保留每个实际适用的 validated base／delta 引用，
不能以一个脱离来源的 status 充当证明。

直接 CRL 签名者必须是该 selected issuer。间接 CRL 必须有原生 indirect
标志，并附真实 `X509Path`：首证书等于 CRL 签名者、锚等于本次选中锚、验证
时刻相同，且强制 `crlSign` key usage；不隐式放宽 SHA-1。签名者路径长度与
CRL 来源数量均有显式上限。`X509Path` 是已有成功验证产生的不可变对象，不
接受用户构造的枚举／伪造 token。本层没有自动递归获取或撤销检查 CRL 签名者
的附加证书；签名者路径仍遵守已有 X.509 path 合同。

普通 good 只有在 native aggregation 确定、完整原因覆盖时接受；分区来源
通过原生 aggregation 合并。delta 经已有 `X509CrlSet` 解释，不把独立 removed
状态当作 good。unknown 只受显式 allowUnknown 控制，其他绑定错误始终 checked。

## 拥有权、效果与成本

策略真正拥有普通 V2 closure：resident entry、一个实际 class-callback driver
捕获及不可变配置。driver 保留目标语言闭包、真实 CodeLease 和拷贝的字段名。
策略 handle Trace 到这一真实 closure；原生 Verifier Context 分别保留并追踪
附加谓词与撤销策略，两边清理都走已有 checked 退役，不将它们误标为 leaf。

请求只持有无语言引用的 packet，getter 返回独立拥有 peer／Certificate。
请求、证书和名称可在回调及 Verifier 释放后继续使用。证据是只含原生拥有者
的持久前缀，释放用迭代而不是递归，避免长聚合链消耗 C 栈。

构造 pure／nonblocking 但可失败；验证可能调用任意用户代码，不承诺
nonblocking。策略与 Verifier 的最后释放可能清理语言捕获，不能标 no-raise／
nonblocking。回调同步在实际验证线程执行，捕获可变状态须遵守普通并发规则。

本批不声称零桥接成本：撤销回调分支复制一个拥有 peer packet，路径中的请求
共用它；每个请求分配一个小拥有者，通用 typed callback 构建／释放真实参数
对象。证据节点保留已有 CRL／path 引用，不再次拷贝 DER 或验签。证书 getter
按语言拥有值合同复制／解析；若同时使用附加谓词，该分支仍有自己的 packet。
Cgen 只输出通用 sealed context、声明、调用与清理，不输出策略实现函数体。

ValidatedCrl 的 Ref／Destroy／handle policy 从验签创建单元拆到独立 owner
单元，避免空 Check 只因可保存来源证据就引入验签算法。附加谓词的实现也不
直接引用撤销策略，未使用的策略声明不应选择任何新 CRL runtime。

## 验收与未完成边界

证据根：`F:/xlang-validation/five-groups-20261026/p7-tls-revocation/`。
本批报告针对冻结源码 cohort；普通 release DLL／SO、CLI、VFS／嵌套编译／
IDE 与五组联合验收仍是后续门槛，不能以定向 source-driver 通过代替。

TLS 恢复握手不保证再调用证书验证。本批的撤销回调不构成恢复凭据与策略的
新鲜度绑定，不能据此宣称恢复已经重新执行 CRL；该项继续保留，`tls_verify`
仍未整体关闭。独立 XRT 的 ms／For 和 xlang 固定快照的微秒／Until 未改变。

Windows／Linux 各完成 150 个公开接入阶段：四族正常证书、严格 unknown、
显式宽松、回调抛错、真实语言引用环、分区覆盖、delta、间接 CRL、空程序裁剪，
以及客户端证书的空参考名与拥有请求。包含 typed／lowered IR、严格 C O0／O2、
JIT／module／无运行库依赖的独立 AOT；每端两个优化配置均实际拒绝 14 种
IR 合同变异并保证 28 次 C／头输出为空。

双端 runtime 七配置共 20 阶段通过，每配置包含 384 个创建／调用／双策略
构造回滚的分配前缀、实际独立 CodeLease、nil／首因错误及堆账本平衡；Linux
包含 ASan＋UBSan。空 Check／仅附加谓词的裁剪两端共 10 阶段通过。合计
330 个成功阶段，不把 1,286／1,285 TU 的完整 source-driver 构建或角色清单
lint 再算入这些阶段。独立核对输入、日志与产物，最终收据为
`proof-windows-r2/proof.json`（164）与 `proof-linux-r1/proof.json`（166）。

首次 Windows 冻结准备漏了 manifest 的六个外部源码输入；只按 hash 追加
缺项，保留原文件／原收据和失败轮后完整重建。Linux 末尾两个 AOT 进程曾
进入 9P 共享目录磁盘等待，随后自行恢复并由同一进程完成原轮，没有迁移、
重启、改系统设置、替换 provider 或因观察超时而重新开始验证。

实际阶段数、结果及失败准备轮以最终收据为准。仅使用离线正常证书／CRL、
常规 ABI／IR／拥有图／分配故障检查，没有 socket、公网、DNS 探测、畸形密码
输入或网络安全性质专项测试。

未决策 CRT 0／XRT 11 不变，内存池最后；本批不增加新的标准库分类决策。
