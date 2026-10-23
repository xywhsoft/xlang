# X.509 发行者匹配与候选建链

状态：公开声明、精确 XLIR 合同与驻留适配、双端冻结专项及联合回归已通过。
完整 P5、TLS 和 shipping 仍未完成；原生搜索工作预算已定点回补，但下列语言
接口尚未接入该预算，仍走不限额原生入口。不代替身份或撤销策略，也不是
生产封板声明。

## 公开接口

```xlang
target.matchesIssuer(issuer) as bool
X509Path.build(target, issuers, trust, policy,
               maxCandidates = 4096, maxSourceBytes = 67108864) as X509PathOutcome
X509Path.build(target, issuers, trust, policy, critical,
               maxCandidates = 4096, maxSourceBytes = 67108864) as X509PathOutcome
```

`target` 是 Certificate，`issuers` 是 `array<Certificate>`，允许空数组。
`trust` 可为 TrustStoreSnapshot 或 TrustStore；后者在进入建链前先取得稳定
快照，整个验证期间不再进入可变库的锁。`policy` 是已有的不可变 X509PathPolicy。
没有隐式系统根、联网补链、下载发行者或 CRL。

`matchesIssuer` 仅复用 XRT 的发行者名称及可用 AKI／SKI 候选匹配，不验证
签名，不作授信、用途或有效期判断。false 是普通不匹配，真实操作错误仍抛出。
`build` 复用唯一 XRT 候选搜索／验证算法，不在语言层重写搜索或猜测错误。

```xlang
auto trust = TrustStore()
trust.add(rootCertificate) // 调用方明确授信；不是自动认证根证书。
array<Certificate> issuers = [intermediateCertificate]
auto outcome = X509Path.build(targetCertificate, issuers, trust,
                             X509PathPolicy(validationTime))
outcome.requireValid()
auto path = outcome.path
```

## 结果和错误

沿用 X509PathOutcome、X509Path、X509ValidationStatus、X509PathReason；不增加
同义结果类型或平行拥有图。成功路径目标在前、独立锚不计入路径数组。
结果在全部验证和拥有复制完成后才发布；分配、预算或回调失败不交付半成品。

- valid：reason 为 none，depth 为 -1，path 有值。
- rejected：验证或策略拒绝，没有可交付路径。无发行者链／空信任源使用新增
  `X509PathReason.build`（原生 PATH_BUILD／28）；无特定证书深度时为 -1。
- unsupported：原生明确报告算法／能力不支持，没有可交付路径。
- 操作错误：资源、参数、预算和语言回调错误仍为 checked 异常；不会吞成
  rejected／unsupported 后继续尝试其他路径。

关键扩展不被处理是原生策略拒绝（rejected／criticalExtension），不是算法
不支持。诊断文本独立拥有，仅供展示；状态和原因直接来自原生决定，不解析
消息或遍历原因链来控制语义。requireValid 对负结论抛出已保存的诊断。

## 预算和生命周期

| 预算 | 约束 |
|---|---|
| policy.maxCertificates（默认 64） | 最大搜索路径深度，不含独立锚 |
| policy.maxBytes（默认 16 MiB） | 成功路径 DER、选中锚 DER 和用途 OID 的拥有副本总量 |
| maxCandidates（默认 4096） | 候选数组条目＋信任快照锚总数；重复候选也计数，不含单独的 target |
| maxSourceBytes（默认 64 MiB） | target、全部候选、快照唯一锚的 DER 和用途 OID 总字节 |

预算不是 size 截断：超额 checked 报错，不改变证书或返回截断路径。零显式
关闭相应预算，负值拒绝；求和和索引分配仍检查溢出。maxBytes 约束最终发布
的拥有结果，输入遍历由另外的 maxSourceBytes 约束。

候选数组先由既有 nominal／native-class admission 校验并独立保留每个句柄，
随后解除集合占用；快照原子保留不可变版本。用户回调可以清空候选数组或
来源 TrustStore，不会使本次原生视图悬空。只有最终选中的路径、锚和用途
被复制进独立拥有块；不为每次验证复制整个信任库。结果不保留候选数组、
快照、闭包、UserData 或生成代码地址；证书／锚投影也独立拥有。

普通建链、路径复制、完成决定和原始回调失败处理分别是共用驻留组件；
有序检查与候选建链不维护两套规则。Cgen 仅生成已验证声明、直接调用、
拥有结果接管和清理，不增加按符号生成函数体的特例。

## 关键扩展回调

`critical` 是同步 `func(Certificate, X509Extension, int) as bool`，在调用线程
执行；参数是可保留的拥有投影，int 是目标在前的深度。true 表示应用已处理
并认可该扩展，false 表示未处理，仍需满足全部其他路径检查。

搜索可能对不同候选路径重复调用处理器，不承诺一次调用，也不回滚其副作用。
回调可阻塞，因此回调入口不标 nonblocking；抛出的原始 checked 错误立即
终止搜索。沿用已有 nullary checked 分派、代码租约和线程／重入门禁，未增加
回调 ABI、线程局部隐藏 context 或可逃逸的原生借用视图。

## 验收与剩余边界

只用正常固定证书、常规 OOM／预算／拥有生命周期、语言 JIT／module／独立
AOT 和裁剪检查。不运行网络流量、原密码安全套件或攻击／畸形密码样本。
证据根 `F:/xlang-validation/five-groups-20261023/p5-x509-build`。

冻结基线 `8d156f5c5`，8990 输入文件在两端一致，源码 1106／1105 TU；每端
九组语言／回归 900 阶段，加驻留及重放为 Windows 912／Linux 915，共 1827。
路径专项 130 阶段、35 目标／13 拥有结果／534 坏合同／40 输出拒绝门及十份
真实裁剪计划通过。驻留每配置 55 分配预算／43 实际失败／10 发布预算／五
目录通过；另有参数持有者重入的双端 JIT／AOT 共 12 阶段。正式回执为
`verified-commit.json`，不是纯净 checkout 或活动工作区后续修改的验收。

r2 曾有一次 Windows AOT storage-release ABI 诊断，六次相同输入的独立
构建／执行和 r3 完整签名回归没有重现。原日志保留，原因仍未确认；后续
联合验收继续调查，不能把最终冻结通过写成该旧故障已被修复。

输入数、字节及路径深度上限不能替代搜索工作量上限。独立 XRT `a31852464`
新增 `xrtX509PathBuildCheckLimit`，同一搜索在候选／锚访问前扣一步，完整路径
验证再预扣证书数；零值显式不限额。耗尽是 checked Range／ERROR，不是信任
拒绝，结果／决定不发布；不承诺中断单次比较、验签或应用回调。

该公开段和搜索源已定点同步四份快照，其他段、结构布局及两边时间合同不变。
正常原生专项／回归 Windows 36／Linux 48，加四消费者独立专项 36／48，共
168 阶段通过，证据根 `F:/xlang-validation/five-groups-20261024/p5-x509-search-budget`；
分别见 `verified-native-commit.json`／`verified-xlang-commit.json`。快照专项含
四种正常链、20 个精确预算边界、八次独立嵌套调用及无诊断回调失败。

当前语言 build 参数及驻留 ABI 没有借此变动：仍须补公开可选参数、精确合同、
驻留调用和 JIT／AOT 后再封板，不以回调计数或 Cgen 补丁模拟。文件／系统
信任来源、身份匹配、TLS、静态低层符号映射及联合发布也仍需继续。
