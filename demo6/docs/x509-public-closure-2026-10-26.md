# X.509 21 模块角色与普通产品验收

## 本批交付

不修改 runtime、Cgen、XRT 快照、时间 ABI 或仓库 release 二进制。复用已经
完整重建的实际 CLI／运行库／编译器库，在 Windows、Linux 各通过 252 阶段，
共 504 阶段。不是源码替代、测试运行时或密码学／网络安全专项。

`x509_public_fixtures.json` 选择 40 正例、4 个私有字段编译拒绝例，每端
245 阶段；另用 `x509_policy_public_fixtures.json` 补做不可变策略预算属性，
每端 7 阶段。后一批只重验新增用例，未重复执行没有变化的主批。

覆盖完整证书／CRL 数据、结构化 Name／Algorithm／SPKI、profile／分发点／
名称约束、CRL 条目／IDP／complete-delta 策略、签名、路径／有界建链和回调、
可变 TrustStore／不可变版本、文件／VFS／只读 OS 来源以及本地 DNS-ID／IP-ID
匹配。身份名称是本地字符串匹配，不发起 DNS 请求或联网补链。

新用例复用既有独立生成并验证的有效固定密钥材料，不运行旧安全测试或畸形
密码学输入。原生成器及原测试不改。文件来源用例改为自身创建的唯一临时文件，
关闭后读取并在 finally 清理，不依赖验证机器的 F 盘路径，不覆盖既存文件。
系统来源只读本机根库快照，不修改 OS 信任策略。

## 证据的准确含义

每个正例通过真实公共宿主的 typed／lowered、重复 C／头输出、JIT、module、
AOT 和 checked 退役；严格 GCC O0／O2、独立目录 AOT 执行、实际动态依赖及
隔离 CLI 通过。独立 AOT 不依赖 xl DLL／SO，部署目录没有源码回退。
四个负例均因 `[sem] private class member is not accessible` 拒绝，不是其他
语法／类型错误碰巧失败，且没有输出 C／头文件。

重新核对 342 个语言声明、C ABI、导出与闭合 XLIR 合同，包含四个精确的
native collection 合同，集合一致且没有重复。合并生成 C 后，307 个普通
入口与 35 个 native 清理入口都有调用位置见证。调用位置不等于逐入口执行
计数，不能把这个静态数字说成 342 项运行覆盖率或析构故障前缀验收。

当前固定头文件仍有 96 个 X.509 C API，驻留层直接引用 84 个，其余 12 个
按 [等价组合表](x509-surface-audit.md) 复用。Cgen 对这 96 个 API 的调用形式
引用为零；旧 AddFile 生成实现已随 TLS／网络后端清退，不再沿用旧文档状态。

## 逐模块账本与后续

完整 21 模块归属见 [接入总表](x509-managed.md)；本轮全部补入
`tools/five_groups_module_roles.json`，区分公开类型、命名空间与内部验证后端。
其中 x509_digest／RSA／ECDSA／Ed25519 验证后端不另造语言数学对象。
角色累计 110 已复核／88 本次复核 pending，后者不是未实现数量或开发百分比。
库存检查两端各拒绝 11 项坏数据，不冒称静态文件检查就是运行验收。

证据根 `F:/xlang-validation/five-groups-20261026/p7-module-audit/`，最终回执
`verified-x509-combined-windows.json`／`verified-x509-combined-linux.json`。
输入、产品、日志／输出、阶段集合和当前用例独立复核；Linux 主批 1,101
文件及策略补批 469 文件均完整归档、逐项校验，原件保留。2,490 个已记录
核心构建输入仍与复用的源码 cohort 相同，不重复重建未变的运行时。

这不关闭五组整体、历史间歇诊断、完整参数化总门或最终联合／嵌套编译要求；
继续 TLS／网络逐模块复核。原始 AES／独立 Poly1305 的公开范围仍待确认。
未决策 CRT 0／XRT 11 不变，内存池最后；完整类别见
[先前范围记录](concurrency-public-closure-2026-10-26.md)。
