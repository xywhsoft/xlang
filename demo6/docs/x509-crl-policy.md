# CRL 策略接入：拥有底座与公共面边界

状态：本批实现并验证共享驻留底座，尚未登记语言公开 ABI、标准库声明、
XLIR 合同或正式 provider。不能把本文件的公共面方案当作已可调用的 API。
完整 P5、CRL policy 接入及长期任务仍在进行。

## 目标合同

语言层将提供显式时间／容差／标志的 CRL 策略、不可变的已验证 CRL、
complete／delta 组合、独立拥有的撤销结果，以及绑定目标证书的归并检查。
默认要求 nextUpdate、CRLNumber、AKI、keyIdentifier 和签发者 cRLSign；
SHA-1 只能由显式 legacy 标志允许，不自动下载 URI 或引入第二套密码实现。
策略验证检查 CRL 及其指定签发者，不把该签发者本身变成信任锚；完整路径、
用途与信任仍属后续 P5。

公开撤销状态必须区分 unknown、good、revoked、removed。部分原因覆盖时即使
原生单项返回 GOOD，语言也只能报告 unknown 并保留 coveredReasons；无适用
结果或 delta 无更新也不是 good。removed 是单份 delta 的观察，必须与完整
CRL 组合，不能直接喂给归并器。只有完整原因覆盖或确定撤销才算 determined。

每个归并检查在构造时固定目标证书，只接受已验证 CRL 或 complete／delta
组合，不接受可随意混用的裸查询结果。这样不同证书的部分覆盖不能拼成
一个 good 结论。新来源必须与已有来源使用相同的时间、容差、策略标志；
complete／delta 也遵守相同限制，不能混合不同时间的验证快照。

归并检查采用不可变快照：withCrl／withSet 返回新检查，失败不修改旧状态，
可从旧快照分支。已 determined 的检查拒绝继续添加来源，避免原生终态
短路被误解为“后续来源已检查”。需要完整遍历的其他策略不能借此接口
暗中假定所有 CRL 都参与了结论。

## 本批已实现的拥有底座

`xl6_x509_crl_policy.c` 复制一次 CRL DER 和一次 issuer DER 到同一分配中，
解析后由 XRT 执行实际策略验证，只在全部成功后发布。全部原生 profile
视图均指向自有 DER；保存时间／容差／标志，但不保存 Critical 或 UserData。
输入 CRL／issuer 对象释放后仍可查询。

`xl6_x509_crl_set.c` 复用 XRT complete／delta 检查，持有两个不可变原生拥有者
的原子引用，不再复制大型 CRL。set 释放时精确释放引用，不隐藏语言对象
或闭包边。原子引用检查溢出；它只是原生内存生命期，不替代语言拥有目录。

`xl6_x509_revocation.c` 将原生查询转成独立拥有结果，只复制命中条目的 DER，
重定位 serial／extensions 视图。结果与子条目都可以比来源活得更久；未知
原生状态或不一致输出 checked 拒绝，不默认解释成 good。组合得到的
removeFromCRL 可映射为 good，同时仍保留原始移除条目以供检查。

`xl6_x509_revocation_check.c` 的共享目标原生记录拥有证书副本，检查快照只
持有其原子引用。状态更新和最终结果复制全部成功后才发布新快照；OOM 不
污染旧检查。最终命中条目独立拥有，部分状态不保存借用结果。所有新增
记录都是不可变的纯原生拥有数据，未保存语言值或生成代码地址。

以上均调用固定 XRT 的 Validate／Check／SetInit／SetCheck／RevocationInit／
Update／Result，不在适配层复制 CRL 策略、覆盖、间接 issuer 或 delta 算法。
只做拥有、输入范围、结果投影和组合上下文检查。没有修改 Cgen、独立 XRT、
固定快照、时间 ABI 或发布资产；固定快照的时间及 skew 仍是微秒。

## 关键扩展回调与后续接线

原生 Critical 回调只在同步验证期间使用。底座当前接受内部 C 回调，测试
覆盖合法 CRL 级和 entry 级自定义扩展，以及 callback 资源错误 cause 保留。
尚未将这个 C 回调、UserData、借用视图或函数指针暴露给语言开发者。

公开语言 handler 应得到独立拥有的 Extension 和可空 Entry；CRL 级调用的
Entry 为 null。它可以在回调后保存这些数据，不应每次复制整个大型 CRL。
回调必须通过现有代码租约及 checked 闭包调用，抛错后不交付验证结果，
不吞成“扩展未处理”、false 或 unknown。不要用不透明原生指针长期保存闭包，
也不要用 Cgen 特例生成回调包装。标准库 helper 与原生桥的确切合同、IR
拒绝门和 JIT／AOT 实测需在后续公共接线中补齐，不能因 C 单测通过而跳过。

后续还须登记四类真实叶策略／拥有目录、类型和状态枚举、精确参数／结果
owner、效果与回调合同、按需模块、两端构建清单、源码 API／负例／裁剪／
JIT／module／AOT 回归。公共读取须保留预算、不泄漏视图；无关证书数据读取
不能引入 policy 或密码验证。完成后再继续 path／build／store／identity、
TLS 和最终 shipping 联合验收。

## 本批验证范围

`tools/check_x509_crl_policy.py` 使用七份正常离线固定密钥 CRL 和五份证书，
独立 cryptography 工具先验证每个签名。覆盖 complete／delta、两份原因
分段、空完整列表、正常自定义关键扩展，以及间接 issuer 继承。只有普通
参数、拥有、OOM、发布回滚和跨线程只读检查；没有网络流量、畸形编码、
攻击模拟或网络安全专项。

证据根 `F:/xlang-validation/five-groups-20261023/p5-x509-crl-policy`；初始 r1
保留缺失 ATOMIC 模块的链接诊断，修复后使用新的证据目录，不覆盖失败记录。
正式验收与提交身份记在该目录 `verified-commit.json`。这不是完整语言接入、
整个 X.509、纯净 checkout 或发布 DLL／SO／VFS 验收。长资源作用域的前端
成本与清理 C 体积风险也保持开放，不能用 X.509 Cgen 补丁规避。

正式边界为两端 native-r3。Windows 12／Linux 15，共 27 个阶段（含独立
重放）通过；GCC O0／O2、TCC 和 Linux ASan／UBSan 均覆盖。每配置 40 个
分配预算、22 次实际失败、四类原生 handle 的八个发布预算，以及四线程
各 32 次只读查询通过，内存账本精确平衡。初始 r1 链接诊断与中间 r2 结果
保留，不计作正式成功阶段。输入／日志／产物哈希独立复核，Linux 原件及
F 盘逐文件哈希镜像同时保留。尚未执行此批语言／IR／JIT／AOT 或真实拥有
目录验收，四类 native leaf policy 的发布回滚通过不等于目录接线已完成。
