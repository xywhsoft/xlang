# TLS 验证／撤销回调的实际剩余与拥有数据底座

## 初始功能缺口基线

初始逐模块核对发现 `tls_verify` 有以下实质接入缺口。后续已交付
[原生拥有权与消费者图](tls-verifier-consumer-ownership-2026-10-26.md)，并接入
[拥有式 peer／附加谓词](tls-peer-policy-2026-10-26.md)；下面保留原始基线，
不再把其中已修项目描述为当前缺陷。CRL 与恢复策略绑定仍未交付：

- `xl6_stdlib_tls_verifier.inc` 只公开默认信任库／策略构造及离线 verifyServer／
  verifyClient，没有 CRL 提供者或附加验证回调。
- `xl6_tls_verifier_create.c` 只配置 Store／Time／AllowSha1／Release，没有接入
  原生 `xtlsverifierconfig.Policy`。
- `XL6_TlsVerifierV1` 目前确实没有语言引用，按 native leaf 管理。它不能在
  保持该声明不变的情况下直接塞入闭包；原生消费者可能比语言包装活得更久。
- 计划及 `tls-managed.md` 中的真实 CRL／附加策略尚未由后续批次交付。会话、
  票据、关闭或普通握手成功不能替代这项需求。

`tools/five_groups_module_roles.json` 继续保留 TLS 角色 pending，并记录 CRL
接入缺口；没有通过重新分类将缺口略过。

## 已确认的实现决策：2026-10-26

用户已确认：默认不启用 CRL；一旦显式启用，缺少适用、有效且覆盖所需范围
的 CRL 时严格拒绝未知结果，不能降级为默认放行。复用已有目标／issuer／时间／
覆盖合同，不把独立的任意状态当成验证结论。

用户同时允许补充 XRT Verifier 必要的拥有图、活动调用和 checked 退役接口，
定点同步到 xlang 固定快照。独立 XRT 与固定快照各自的时间 ABI 保持不变。
闭包不能仅由语言包装持有：原生 Context／Session／Stream 等消费者仍持有
Verifier 时，完整语言拥有边必须继续可见；活动调用及失败退役不能释放回调。

以上两项从待确认移入决策记录，不改变 `tls_verify` 的待完成状态。本轮没有
新增宽松 CRL 策略，也没有把接口批准当作功能交付。原始 AES／独立 Poly1305
公开范围仍待用户确认，与上述 TLS 工作独立。

## 本批实现：私有拥有快照

2026-10-26 后续已完成原生 Verifier 拥有权前置与六节点定点同步，见
[交付和精确验证边界](tls-verifier-native-ownership-2026-10-26.md)。语言默认
工厂和实际消费者图已在后续批次迁移；附加谓词本批接线不代表 CRL 缺口关闭。

新增 `xl6_tls_verified_peer_packet.c/.h`，供后续原生验证成功后的策略／CRL
回调生产端使用。当前没有公开语言构造、借用 API、注册符号或 Cgen 特判，
也没有声称回调已经接通。两份正常构建响应模板只增加这一源条目。

快照只复制角色、验证参考名、原生验证时刻、完整 presented chain、selected
path 和独立锚证书 DER。role 是**对端角色**，不是本地 endpoint；name 是
**验证参考名**，不冒充 ClientHello SNI。Time 原样保存，在固定快照中为微秒，
不升级独立 XRT 的时间 ABI。它不重新解析或验证签名，也不是认证入口。

所有数据保存在一个分配中。原生 path 若直接指向 presented 数组元素，按
对象身份在常数时间定位已有私有 DER；不同地址的 native view 独立复制。
整个捕获是线性遍历／复制，没有二次扫描、内容哈希、重新解析或额外分配。
大小／数量溢出在复制前拒绝，失败不发布部分对象；0 时刻、空参考名及空
已选路径有明确数据语义。只读引用可跨线程共享，计数饱和 checked 拒绝。

内部 `*View/*Der` 返回的是 C resident 层持有 packet 时的只读视图；未来语言
facade 必须投影成独立拥有 string／Certificate，不能把它们直接登记为语言
借用结果。当前 packet 没有语言值、闭包、生成地址或被借用的 native owner。

## 本批验证及边界

复用真实普通 Windows DLL／编译 DLL、Linux SO／编译 SO 的分配器、错误及
原生 API；只编译被测新 packet、未改且与候选一致的 TLS 诊断帮助器及观察者，
没有重编前端、替换现存 provider 或加入 mock。诊断帮助器是非导出私有符号，
因而正常参加新主体的链接，不能把单独编译主体说成普通产品已重建。

Windows GCC O0／O2／TCC、Linux GCC O0／O2／ASan+UBSan，分别连接两个
实际库，12 个配置通过；共 384 个分配故障前缀、48,000 次并行持有／只读，
分配账本平衡。覆盖原输入销毁后的独立性、同地址与不同地址 view、512 个
完整有效 DER view 不截断、索引／算术边界、错误保留及最后引用释放。
Linux 将 DER 视图故意改成借用的真实坏实现被同一观察者拒绝；这不是密码
输入攻击，不进行 socket、握手、信任决策或畸形密码学测试。

证据根 `F:/xlang-validation/five-groups-20261026/p7-module-audit/`，最终为
`verified-peer-packet-windows.json`／`verified-peer-packet-linux.json`。前期混合
TCC 对象的链接前置、准备轮及路径拼写失败保留，不计成功阶段。最终为
Windows 12、Linux 14 个构建／执行阶段（Linux 含坏实现构建与拒绝）。输入、
输出、产物及真实基线库独立复核，Linux 原件／逐项校验归档保留。

## 接续顺序，不缩减目标

1. 为带语言闭包的 Verifier 补齐真实拥有边、活动调用及 checked 退役；原生
   保留、角色上下文、会话及传输必须保留同一个完整权利，不能仅保留包装。
2. 在默认路径／用途／名称成功后执行附加策略，不用普通 hook 绕开默认信任。
   交付拥有快照；异常保留首因，执行线程和并发合同明确，不隐式创建线程。
3. CRL 接入复用已有 X509ValidatedCrl／X509CrlSet／X509RevocationCheck，不
   重写选择、验签、delta 或覆盖算法。不能只接受脱离目标／时钟的任意 status；
   需保留目标、issuer／路径、时间、完整覆盖及未知结果规则。
4. 明确恢复边界：原生证书验证回调不等于每次恢复握手都会调用；不能在没有
   凭据／策略绑定证据时声称恢复也执行了新的撤销策略。
5. 完成公开声明、闭合 ABI／效果／拥有权、回调／取消／清理和双端普通产品
   回归；在此之前不将本批准备工作标作完整 TLS／CRL 或长期任务完成。

未决策 CRT 0／XRT 11 不变，内存池最后；另有已批准范围内的 API 确认项，
不将其混作新标准库类别或用来关闭其他可推进工作。
