# CRL 策略接入：拥有底座与公共面边界

状态：共享驻留底座之后，本轮已接入 30 个精确驻留入口、四类公开拥有对象、
一个内部回调上下文、两个枚举及五个真实叶拥有目录；双平台源码验收已通过。
完整 P5、TLS 和最终 shipping 仍未完成。本文件末尾保留底座批次的历史边界。

## 目标合同

语言层提供显式时间／容差／标志的 CRL 策略、不可变的已验证 CRL、
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

实际策略调用固定 XRT 的 Validate／Check／SetInit／SetCheck／Update／Result，
不在适配层复制 CRL 策略、覆盖、间接 issuer 或 delta 算法。空检查使用固定
快照的原生零布局初始化，不因初始化而链接整个策略／密码后端。更新代码在
独立 `xl6_x509_revocation_update.c`；查询与回调也按依赖拆开，避免无关引入。
只做拥有、输入范围、结果投影和组合上下文检查。没有修改 Cgen、独立 XRT、
固定快照、时间 ABI 或发布资产；固定快照的时间及 skew 仍是微秒。

## 公开 API

| 类型 | 入口 |
|---|---|
| `X509ValidatedCrl` | `fromCrl(crl, issuer, at, skew = 0, flags = X509CrlPolicyFlags.strict)` |
| `X509ValidatedCrl` | handler 重载：`fromCrl(crl, issuer, at, critical, skew = 0, flags = strict)` |
| `X509ValidatedCrl` | `crl`、`issuer`、`at`、`skew`、`flags`、`isDelta`、`query(target)` |
| `X509CrlSet` | `X509CrlSet(base, delta)`；`base`、`delta`、`query(target)` |
| `X509RevocationCheck` | `X509RevocationCheck(target)`；`withCrl(crl)`、`withSet(source)`、`determined`、`result` |
| `X509RevocationResult` | `status`、`coveredReasons`、`hasObservation`、`entry` |

at 是显式语言 time，skew 是非负微秒；flags 控制字段存在性与显式 SHA-1
兼容，不关闭签名和 critical 检查。requireKeyIdentifier 必须同时要求 AKI。
负 skew 抛 RangeError，非法标志／上下文组合抛 ValueError，向已确定检查
继续添加抛 RuntimeError。验证失败走 checked 错误，不发布半成品快照。

`X509RevocationStatus` 为 unknown／good／revoked／removed。entry 返回拥有的
`X509CrlEntry?`，无条目为 null；不以空指针区分错误。coveredReasons 使用
已有 X509ReasonFlags。部分归并也有 hasObservation=true，但 status 仍为
unknown；空检查、无适用结果或 delta 无更新的无观察结果为 false。

这些结论只针对指定 CRL 的撤销覆盖，不同时验证目标证书或 issuer 的完整
路径／信任。crl、issuer、entry 是独立拥有的投影；set 的两个已验证记录
共享不可变原生存储，但不承诺投影对象与原语言变量的引用身份相等。

## 关键扩展回调

critical 的类型为 `func(X509CrlEntry?, X509Extension) as bool`；CRL 级调用的
Entry 为 null。参数都独立拥有，只复制相关片段，不复制整份大型 CRL。用户
可以保存这些对象。返回 false 拒绝未知 critical 扩展；抛错保留原始语言
错误，不能吞成“未处理”、false 或 unknown，失败不交付已验证对象。

普通入口与 handler 重载分开，普通调用不链接闭包或回调适配。handler
重载通过声明式标准库 helper 调用已存在的同步 nullary bool checked 闭包
合同；没有新回调 ABI、Cgen 特例、裸指针参数或新 TLS／FLS 状态。

内部 `__X509CrlCriticalContext` 只在同步调用期间提供快照读取。原生调用保留
独立引用，按原子 owner 门拒绝并发／同上下文重入；只有活跃的所属线程能
读取借用字段，getter 在返回前复制。结束后清空视图并退役 owner。上下文不
保存闭包，回调通过真实代码租约及错误作用域调用。即使 checked 回调失败
而没有分配到诊断对象，也保留独立失败标志，不能误报成功或换成包装错误。

保存局部回调数据遵循现有语言规则：可捕获持有对象并写入其字段，然后将
字段读到局部变量、完成 nullable 收窄后使用。直接改写被捕获的本地类变量
仍不属于现有共享 cell 支持面；这不是此次 API 新增的限制，也未伪装成已实现。

声明按需注入：普通 X.509 数据用例不加入 policy 类／handler；仅使用两个
策略枚举时只加入枚举声明。空归并检查不引入策略或密码验证；普通验证不
引入回调上下文。实际五类 leaf policy／拥有目录、精确 owner、枚举和效果
合同通过既有注册体系接线，不改变图算法。

## 底座批次验证范围（历史）

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

## 公开接线正式验收（2026-10-23）

Windows 713／Linux 716，共 1429 个阶段通过，包含独立原生重放；完整源码
构建分别为 1071／1070 TU，不混计阶段。两端各有新 API／IR／裁剪 93、CRL
数据回归 115、typed profile 93、记录 92、原始数据 59、签名 104 和完整密钥
格式 145 阶段。原生专项分别 9／11，独立重放 3／4。旧接口由六组独立进程
批量验证，每端最多三组并行，不缩减用例。没有用中间失败或旧源码报告
冒充最终通过。

每原生配置覆盖七份正常 CRL、五份证书、47 个分配预算、27 次实际失败、
五类 handle 的十个发布预算、五个真实拥有目录及四线程各 32 次只读查询；
内存账本精确平衡。回调的接受／拒绝、原始异常、失败但无诊断对象、同一
上下文重入拒绝、结果和回调片段独立拥有均通过。GCC O0／O2、TCC、Linux
ASan／UBSan 覆盖；语言包括 JIT、module、独立 AOT 和真实局部持有对象留存。

30 个目标、17 个拥有型结果、457 处坏合同及 35 个模块／C／头输出门通过；
回调参数数目、返回 ABI、checked 物理边界，nullable entry 的独立错误边界
均由合同拒绝门覆盖。仅枚举不注入 policy 类或运行实现；空检查不引入
策略／密码验证，普通入口不拉入回调；旧数据、签名和密钥的原有裁剪继续
通过。修复部分归并 hasObservation 投影，并为原生及语言路径加了断言。

正式边界为 native-*-r8、source-*-r3、Windows api-r5／Linux api-r2 和双端
regression-r2。较早目录保留诊断；旧源码回归因待修状态投影而主动停止，
没有覆盖或算入正式通过。最终输入、日志、产物、驱动身份独立哈希复核，
Linux 原件与 F 盘逐文件哈希镜像同时保留；本轮提交见证据根的
`verified-api-commit.json`，底座原 `verified-commit.json` 保持不变。

本轮是冻结完整工作区的源码验收，不是纯净 checkout／发布 DLL／SO／VFS
或完整 P5 验收。未修改 Cgen、独立 XRT、固定快照或两边时间 ABI，也没有
新增闭包 ABI、TLS／FLS 状态或发布资产。无关工作区、原暂存区及 RSP 既有
重构保留。后续继续完整 typed 验证结果、path／build／store／identity、TLS
和 P7；通用长资源作用域的前端成本及清理 C 体积风险仍开放。
