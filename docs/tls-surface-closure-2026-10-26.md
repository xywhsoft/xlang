# TLS 逐模块归属、恢复绑定与普通产品回归

本批不新增协议算法，不修改 Cgen／runtime／XRT，不升级两边时间 ABI。
目标是核对原 47 模块范围，补齐普通产品中的真实语言组合，而非把角色
分类、静态登记或旧源码专项当作完整 TLS／五组验收。

运行结果：两端各 295 个成功阶段，共 590。主批各 281 个成功，另各保留
1 个代理前置失败；自有代理定点批和受限配置批各 7 个成功阶段。不改写主批
失败状态，也不将宿主内部标记与控制器阶段相加。

## 模块归属与真实缺口

完整逐模块记录位于 `tools/five_groups_module_roles.json`。46 个 TLS 模块
角色已核对，`tls_verify` 因 CRL／附加策略回调仍未接通而继续 pending。
总账本 195 项已复核、3 项角色 pending，不是实现百分比或剩余工作数量。

| 模块组 | 语言呈现／边界 |
| --- | --- |
| tls；handshake／reader／hello／negotiate／PSK／messages／auth_messages 及 write 家族 | 精确结果／角色词汇和原生协议实现；通过 TlsSession／Stream 使用，不暴露临时借用或握手私钥 |
| tls_key_exchange 与 x25519／x448／p256／p384 | TlsPolicy 指定组，XRT 执行密钥交换，不增加第二套会话密钥 API |
| tls_schedule／sha256／sha384 | 原生转录与 traffic-key 派生，由明确的套件配置选择 |
| tls_record／aes／chacha | 会话／传输的记录处理及 KeyUpdate，拥有的 bytes 和状态 |
| tls_policy／tls_context | 不可变 TlsPolicy／TlsParameters／TlsLimits |
| tls_session | 带真实 gate／拥有图的 sans-I/O TlsSession |
| tls_verify | 默认链／用途／时间／名称已接通；CRL／附加验证回调与必要拥有接口未完成，不归类略过 |
| tls_client／tls_client_verify／tls_client_resume | 独立客户端上下文、默认验证、显式恢复与票据配置 |
| tls_identity 与 rsa／ec／p256／p384／ed25519 | 同一 TlsIdentity 的算法命名工厂及已有私钥类型，不按长度猜测算法 |
| tls_server／tls_server_resume | 服务端上下文、类型化身份选择器和拥有式恢复查找 |
| tls_resume | 不可变拥有凭据；明确导入／秘密导出及有效期查询 |
| tls_stream／tls_stream_future | 受管 TCP 组合及同一流的 typed Future／取消／物理清理 |
| tls_stream_listener／future／sync | 聚合 TlsServer／tls.listen 及同步／异步接受 |
| tls_stream_dial／proxy／future | 同一原生拨号组合，独立 SNI／验证名、期限和取消 |

没有因为默认枚举包含 TLS 1.2、P521、FFDHE 等词汇就声称它们均能执行；
本批受限配置只使用原生已支持的 TLS 1.3、四组和三个套件。

## 新的真实组合见证

`trust_rebind.xl` 在同一程序中完成两次真实恢复及三次完整验证握手：

- 参数／协议／票据容量复制保持原有凭据及 required 状态。
- 显式 `withVerifier()` 即使传回相同 verifier，也清除凭据与 required 状态。
- 换成新建 verifier 的快照、以及 `withoutResume()`，都实际走完整握手并
  取得对端证书，而不只检查 hasResume 属性。
- 原快照仍可恢复；已交付的恢复请求在会话销毁后仍独立拥有。

这不代表新 CRL／附加策略会在恢复时执行。当前尚无这些公开回调；将来
必须另行证明凭据与策略绑定和退役，不能借本批普通恢复结果关闭该缺口。

`restricted_negotiation.xl` 对 X25519／X448／P256／P384 × AES128-GCM／
AES256-GCM／ChaCha20-Poly1305 共 12 个受限配置完成正常验证握手和 NUL
应用数据传输。套件从两个角色的实际 getter 回读；没有公开 group getter，
因而组证据是“只有该组的配置能够完成握手”，不是伪造的协商组遥测。

## 测试前置修复与自有代理

旧 TLS 代理生成器依赖专用控制器替换模板端口。首次搬入通用产品批次时，
未 provision 的模板端口 1 造成两端拒绝连接；这是本批测试前置错误，不是
XRT／语言产品缺陷。原输入、失败日志和失败主报告保留，不重复执行该旧
程序，不将其记入成功阶段，也不称它符合最终自有端点验证范围。

最终 `public_tls_proxy.xl` 自己创建字面量回环上的 TcpListener 代理及
TlsServer，两者都使用系统分配的唯一端口；正常 CONNECT 请求逐字验证后，
仅连接该程序自己的上游地址，异步双向转发。20 个同步／异步／期限／取消、
默认／显式配置重载逐个真实调用，验证两方向 NUL 数据和 checked 退出。
没有 Python 服务、固定端口、额外 extern 声明或真实目标域名解析前置。
未变主批不重复运行，只定点复测该代理程序。

## 静态与运行证据的区别

实际预处理 stdlib 数据、IR 合同、ABI 头和导出登记后，402 个声明名与 402
个合同名一致，且都在 ABI／导出集合中。ABI 集合还有 2 个内部名，导出
还有 20 个策略／支撑名，不把它们机械算成新的标准库函数。
这是**名称闭合**，不是 402 个签名／效果合同逐项相等或入口运行计数。

主清单 47 正例／4 个语义拒绝例；另有 1 个受限配置程序。三个私有 native
字段负例核对明确的 private 诊断，错误恢复参数另核对有类型的调用拒绝。
持久化用例从原正常固定材料生成器保存，生成器本身不改；保留生成输入
摘要和代理转换来源，不把原本需要独立 peer 的模板冒充自包含程序。

运行层使用冻结的 `p7-crypto-migration/shipping-source-r3` 正常构建产品，
隔离部署真实 CLI／两库，不替换源对象或 provider。每个正例检查 typed／
lowered IR、重复 C 一致性、header、JIT、module、checked 退役、严格 GCC
O0／O2、可搬移独立 AOT／导入及 CLI。结果绑定此候选，不宣称本轮最新
完整工作区或新私有 verified-peer packet 已重新构建验收。

证据根 `F:/xlang-validation/five-groups-20261026/p7-module-audit/`；独立收据
为 `verified-tls-surface-windows.json`／`verified-tls-surface-linux.json`。
核对真实构建输入／产品、用例／阶段集合、日志／输出、402 名称集合和生成
输入摘要。Linux 主批、受限配置及代理批保留原件与完整成员逐文件验证归档。
角色库存两端各拒绝 11 份坏清单，这仍不是功能验收。

## 仍须完成

CRL／附加策略回调、Verifier 的真实拥有／活动调用／checked 退役，以及
恢复和新验证策略的绑定；原始 AES／独立 Poly1305 的范围确认；完整参数化
门、相关 IDE／嵌套编译、历史间歇诊断与最后跨组联合验收继续。
本批正常有效材料与最终自有回环不替代网络／密码学安全审计。
未决策 CRT 0／XRT 11 不变，内存池最后。
