# TLS Verifier 消费者图迁移

本批迁移运行时的实际 Verifier 引用，不是公开语言回调、CRL 或整套 TLS
发布验收。用户已确认：CRL 默认关闭，显式开启后未知／缺少适用、有效且
完整覆盖的结果必须拒绝；允许为实际语言闭包补齐必要原生拥有接口。

## 实现

所有语言 Verifier 工厂改用 `xrtTlsVerifierCreateOwnedV1`，接管实际计数的
驻留 Context。Context 拥有不可变策略及可选的内部 peer-policy capability；
真实 Ref／Drop／Ownership／Verify 槽必须完整。默认信任路径、名称、时钟和
预算仍由原生验证器执行，附加策略不能替代或绕过它们。公开语言闭包生产者
尚未接线，内部 C capability 不应被描述成已经交付的语言函数。

原生 Verifier → Context → 实际 capture，以及语言 Verifier、ClientContext
各自 → 原生 Verifier 的独立引用均进入图。Session 报告自己真正持有的
Verifier 槽，Stream／pending Dial 组合这条边。不是复制 facade 或在图中
扁平展开虚构的 capture 引用。拥有目录使用精确类型的策略表，未扩展 Cgen。

原生完整 Session 遍历核对内建角色身份；保守拒绝含不透明外部 signer 的
Session／Listener 图接纳，不影响普通 C API。内建身份、参数及字节存储
没有语言回调边，但不错误宣称它们独占。

Context 的 Drop 在 mutation 域外执行。Finish 保存自己的失败结果，包括
没有诊断对象的失败；重试不重复 Drop。调用者原有错误不污染本次清理结果。
受控清理错误使用既有 checked 传播，不把业务失败改成 abort。

新增源进入实际 artifact 闭包及 compiler/runtime 两份正式响应模板。
Linux 共享源清单读取同一模板；嵌套编译 VFS 的源码目录规则覆盖新增 C／H。
没有重建或替换 release 二进制，也没有宣称全产品验收通过。

## 定点同步和证据

只同步选中的原生 session／client／server／stream／dial／listener 及私有
声明节点和公共 session 声明；所有未选区域逐字节保留。独立 XRT 继续使用
毫秒／For，xlang 固定快照继续使用微秒／Until，不全量升级时间 ABI。
来源记录见 `lib/xrt/backports/tls_verifier_consumers.json`。

原生夹具使用正式模块闭包和实际注册的 single wrapper：Windows GCC O0／O2／
TCC、Linux GCC O0／O2／ASan+UBSan，模块化与单头共 12 配置、24 编译执行阶段。
检查实际 Session 槽、三个构造分配失败前缀、创建者与消费者独立寿命、无尾部
base Session、内建身份和不透明外部 signer 边界，堆账本平衡。

驻留夹具在 Windows 三模式和 Linux 四模式通过三个真实环的完整
Inspect → catalog → Plan → Prepare／Finalize → Commit → Finish／Destroy：
capture 分别回指 Verifier、ClientContext、Session 的实际 handle。检查零
外部根、真实 native/context/capture 边、默认验证后附加策略和 checked 清理
失败重试。无真实语言闭包／代码卸载／JIT 验收的含义。

同一最新快照另重跑原有四族正常证书链和 Verifier 资源回滚夹具，两平台
上述七模式通过。原先准备轮失败、旧快照通过结果均保留，不冒充最终 cohort。

证据根：`F:/xlang-validation/five-groups-20261026/p7-tls-verifier-consumers/`。
包括冻结原生源码、生成增量链、两平台最终报告、Linux 原件及校验归档。
无 socket、外部 DNS、畸形密码材料、信任绕过或网络安全性质专项。

另将五种真实坏实现仅放在私有目录：漏 client Verifier 边、放过不透明 signer、
漏 ClientContext 边、Session 退回 buffer-only、清理继承无关错误。原夹具及
provider 均不变；五者均成功编译后被运行见证拒绝，正确对照通过。包含共用
闭包编译、对照及坏实现共 16 成功构建／运行阶段；初次清理反例少传一个
参数的编译准备失败独立保留，不计作坏实现运行拒绝。

## 尚未完成

公开的拥有式 verified-peer、语言闭包／异常／代码寿命，以及 CRL 提供者的
目标、issuer、时间与完整覆盖判定仍待接线和验收；恢复凭据策略绑定及实际
普通产品路径仍须最终验证。`tls_verify` 继续 pending，长期任务未完成。

原始 AES／独立 Poly1305 的公开范围仍待确认。未决策为 CRT 0／XRT 11：
核心／错误／拥有权、内存、动态值／图、剩余系统信息、HTTP、WebSocket、
运行时类型／转换、对象／字段／图、调用／值桥接、typed 容器、内部支撑。
内存池按决定最后处理。
