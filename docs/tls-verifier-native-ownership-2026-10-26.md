# TLS Verifier 原生拥有权前置与定点同步

## 本批交付，不等于语言回调完成

按用户批准，XRT 新增显式拥有式 Verifier：
`xrtTlsVerifierCreateOwnedV1`、物理拥有图、活动调用观察和 checked 退役接口。
原生提交为 `2b6a95876aea8e2c13cdd28c0f72628f011012b1`。

- 成功只接管一个真实 Context 引用；失败不接管，不执行回调／Drop。
- policy 地址身份与四个回调槽明确匹配，字段相同的副本不能冒充注册权限。
- Context 是一条真实物理拥有边，Trace 不扁平展开、不伪造语言根。
- 每次拥有式 Verify 持有实际引用，整次时钟／验证／附加策略调用均为 busy。
  回调释放原始引用不会销毁活动数据；最后 owner 的清理尾段也保持 busy。
- Claim 隔离新调用，Restore 可撤销；Clear 不执行析构，Finish 在域外清理。
  失败原因保留、重试不重复已执行的 Drop。旧 opaque context 不被自动接纳。

没有新增算法或改变默认信任检查。原 `xtlsverifierconfig` 公共布局、既有函数
签名及独立 XRT 时间 ABI 均不变。测试的自定义 Verify 返回 DEFAULT，正常
固定证书链仍经过实际默认路径、名称和签名检查，不用 ACCEPT 绕过信任。

## 固定快照同步

四份头的六个原 Verifier 节点与原生父版本逐项完全一致；只替换这些生成节点：

- `lib/xrt/xrt.h`、`lib/xruntime/xruntime.h`：公开声明和实现。
- `release/include/xrt_decl.h`、`release/include/xruntime_decl.h`：公开声明。

逐字节回放旧节点可以还原原文件，所有未选区域完全不变，包括两边各自的
时间体系。不是全量升级 XRT；独立库仍是毫秒／For，固定快照仍是微秒／Until。
来源和哈希记录于 `lib/xrt/backports/tls_verifier_ownership.json`。
没有修改 Cgen、标准库语言声明、普通产品或 release 二进制。

## 实际验证

原生当前源码冻结 cohort：Windows GCC O0／O2／TCC，Linux GCC O0／O2／
ASan+UBSan，各执行模块化与单头，12 配置／24 编译执行阶段通过；两端实际
注册的单头 wrapper 再各完成编译和执行，共 4 阶段。注册源集合与正式构建器
读取的依赖闭包一致；不将手工场景说成整库 suite 或全部 HEAD 已验收。

验证真实 context 环及其两条边、实际 collector pin 这一唯一根、活动原生
线程的时钟、回调内释放原始引用、五个创建分配失败前缀、claim／restore、
checked 清理失败重试，以及最后 owner 的清理尾段。堆账本平衡。

三个私有真实坏实现分别漏掉 busy、Context 边、最后清理的 busy；全部先
成功编译，再由对应运行见证拒绝。含正确对照及共享真实闭包编译，共 9 阶段；
不是因源哈希不同或编译失败而拒绝，不替换生产 provider。

固定快照另直接编译 core／xruntime 两种实现，分别链接两份真实声明头。
Windows 三种编译模式、Linux O0／O2／sanitizer，共 60 阶段：12 个实现对象、
24 次声明链接、24 次实际拥有权夹具执行，均通过。复用原生完整夹具，只改变
include 路由、测试线程等待的毫秒→微秒适配，并断言固定快照的 2030-01-01
确为 `1893456000000000`。生产时间 ABI 没有转换或升级。

准备轮缺少 LICENSE、模块 feature 宏／搜索路径和 RSA provider 的失败均保留；
没有删源或放宽警告。注册 wrapper 的附加准备轮曾错误地把源放在 `-lm` 后面，
保留该链接失败；最终按原参数位置替换，两个实际 wrapper 均通过。

证据根 `F:/xlang-validation/five-groups-20261026/p7-tls-verifier-owned/`：
`verified-verifier-ownership-final-r1.json`、`verified-pinned-verifier-final-r1.json`。
独立读取器复核源码、输入、日志、产物、实际阶段和生成节点；Linux 原件及
逐项校验归档保留。没有 socket、外部 DNS、畸形密码材料或安全性质专项。

## 仍必须完成

后续批次已迁移语言默认工厂、Verifier／ClientContext／Session 的实际引用，
并组合原生 Stream／pending Dial 图，见
[消费者图迁移和验收边界](tls-verifier-consumer-ownership-2026-10-26.md)。
这不等于公开附加策略／CRL 闭包已完成；仍须接通拥有式 verified peer、
语言异常、CRL 目标／issuer／时间／完整覆盖，并证明恢复凭据与策略绑定及
实际普通产品路径。

CRL 默认关闭；显式开启后未知结果严格拒绝的用户决策保持有效，但本批尚未
交付语言 CRL 开关或回调。`tls_verify` 仍 pending，不把前置接口和这组测试
当作完整消费者图、语言 TLS、shipping 或长期任务完成。原始 AES／独立
Poly1305 公开范围仍待确认。未决策 CRT 0／XRT 11，内存池最后。
