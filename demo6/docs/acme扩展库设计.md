# acme 扩展库设计（对标 acme.sh）

> 状态：设计稿，待评审后进入实现。
> 日期：2026-08-17
> 前置：本设计基于对 `lib/xrt/xrt.h` 的实际核查（见 §3），非推测。

## 1. 目标与对标范围

对标 acme.sh（ACME v2 / RFC 8555 客户端事实标准），取其核心签发链路，
剥离 shell 安装器属性。多 CA、多 DNS 平台、通配符、续期簿记全部保留。

| acme.sh 能力 | 本库 | 说明 |
| --- | --- | --- |
| 多 CA（LE / ZeroSSL / BuyPass / Google / 自定义） | ✅ | 目录 URL 驱动，预设常量 + 任意 URL |
| dns-01（通配符唯一途径） | ✅ 首批 | 内置多平台 DNS API（§6） |
| http-01（standalone / webroot） | ✅ 三期 | standalone 走 http.server，webroot 写文件 |
| tls-alpn-01 | ❌ 排除 | 需 TLS 服务端定制，收益低 |
| ECC（P-256）账户/证书密钥 | ✅ 默认 | RSA-2048 可选 |
| SAN 多域名 / 通配符 | ✅ | order 的 identifiers 数组 |
| EAB（外部账户绑定，ZeroSSL 必需） | ✅ 三期 | newAccount 带 externalAccountBinding |
| 续期（cron + 到期检查） | ✅ | `shouldRenew` + 范例守护进程（OS 调度归用户） |
| deploy hooks（安装/重载回调） | ✅ 简化 | 签发成功回调 `onIssued`，不做 shell 模板 |
| revoke / deactivate account | ✅ 三期 | |
| OCSP Must-Staple / CSR 复用 | ⏸ 暂缓 | CSR 导入三期后评估 |
| 150+ DNS 提供商 | 首批 5 个 | 接口开放，社区可按协议自扩（§6） |
| dnssleep / DNS 传播检查 | ✅ | 复用已压实的 `dns` 扩展库查 TXT |

## 2. 架构分层

延续六轮验证的"编解码在 C、编排走标准库"模式，但本库重心反转：
**编排层（xlang）是主体，C 层只做密码学重活**。

```
┌────────────────────────────────────────────────┐
│ 编排层（xlang，src/acme.xl 等）                  │
│  目录发现→账户→订单→授权→挑战→轮询→finalize→证书  │
│  nonce 池 / Retry-After / 续期簿记 / 存储布局     │
│  依赖标准库：http（request/HttpHeaders）、json、  │
│  file/dir、crypto（sha256/hmac）、dns 扩展库     │
├────────────────────────────────────────────────┤
│ DNS 提供商适配层（xlang，src/dns_*.xl，纯标准库） │
│  Cloudflare / Aliyun / DNSPod / 华为 / Route53  │
│  各自的请求签名在 xlang 内完成（HMAC 走标准库）    │
├────────────────────────────────────────────────┤
│ 密码核心（C，acme_native.c）                     │
│  JWS ES256/RS256 签名、JOSE b64url、RFC 7638     │
│  thumbprint、EC/RSA 密钥生成与 PEM 编解码、       │
│  CSR（PKCS#10 + SAN）构造、证书到期/SAN 解析      │
└────────────────────────────────────────────────┘
```

## 3. 依赖核查结论（已对 xrt.h 实测）

| 能力 | 现状 | 结论 |
| --- | --- | --- |
| EC P-256 密钥生成 | `xrtP256KeyPair`（公开 XRT_API，OS CSPRNG） | ✅ 直接可用 |
| ECDSA 签名（RFC 6979 确定性、low-S、raw） | 仅 `__xrtEcdsaSign`（内部前缀） | ⚠️ **缺口 D1**：需在 XRT 暴露公共 API（raw R‖S 正是 JWS ES256 格式） |
| RSA RS256 签名 | `xrtRsaPkcs1Sign`（公开） | ✅ |
| SHA-256 / HMAC | 公开（六轮已用） | ✅ |
| PEM 编解码 | `xrtPem*` 全家（公开） | ✅ |
| X.509 解析 | `xrtX509*` 1606 处引用（Parse/Verify/GeneralNames…） | ✅ 到期/SAN 解析走 XRT |
| CSR（PKCS#10）构造 | 未见直接 API；pkcs 扩展库有 DER 组装基建 | ⚠️ **缺口 D2**：C 层自建（~300 行 DER 组装）或 XRT 补 CSR API，spike 定夺 |
| HTTP（任意方法/自定义头/大小写不敏感响应头） | 标准库 `http.request` + `HttpHeaders` | ✅（POST-as-GET 用空 body POST 表达） |
| JSON / base64 | 标准库 json / codec | ✅（b64url 去补 `-_` 与去 padding，C 或 xlang 均可） |
| DNS TXT 查询（传播检查） | `dns` 扩展库（phase1-verified） | ✅ 复用 |
| RSA 密钥生成 | 未见公开 API | ⚠️ 缺口 D3（三期）：ECC 默认下非阻塞 |

> 按集成原则：D1 属"XRT 缺口先修 XRT"——一行公开封装的工作量；
> D2 倾向 C 层自建（CSR 结构小且稳定，不必等 XRT 排期）。

## 4. 包结构

```
release/extlibs/acme/
├── xlang.package.json
├── acme_native.c            # §2 密码核心
└── src/
    ├── acme.xl              # 客户端状态机 + 存储布局 + 续期
    ├── jose.xl              # JWS 组装薄包装（b64url/JWK/thumbprint 转换）
    ├── order.xl             # 订单/授权/挑战的数据结构与轮询
    └── dns_*.xl             # 每提供商一个文件，注册进 dns 注册表
```

现有 `acme` 占位包（deferred-platform）按 redis/mqtt 先例整体替换。
依赖声明：`dependencies: ["dns"]`（传播检查用，可选加载）。

## 5. 核心 API（xlang 草案）

```xlang
// ---- 目录与账户 ----
acme.LE_STAGING / acme.LE_PROD / acme.ZEROSSL / acme.BUYPASS   // 目录 URL 常量
acme.Client(directoryUrl, accountKeyPath, userAgent)            // 账户密钥自动加载/生成
client.register(email, eabKid = "", eabHmac = "")               // -> AccountURL（EAB 可选）
client.account()                                                // 查询/更新账户

// ---- 订单与签发（dns-01 主流程） ----
order = client.newOrder(["example.com", "*.example.com"])       // 通配符用 dns-01
for authz in order.authorizations()                             // [{domain, status, challenges}]
    ch = acme.pick(authz, "dns-01")                             // {token, url}
    keyauth  = acme.keyAuthorization(ch.token, client.thumbprint)
    txtValue = acme.dnsTxtValue(keyauth)                        // b64url(sha256(keyauth))
    recordId = provider.setTxt("_acme-challenge." + authz.domain, txtValue)
    acme.waitDnsPropagated(authz.domain, txtValue, timeoutMin = 15)   // 复用 dns 扩展库
    client.notifyChallengeReady(ch.url)
    provider.rmTxt(recordId)                                    // 失败也要清理（finally 语义由调用方保证）
client.pollOrder(order)                                         // 等待 ready（处理 Retry-After）
csr = acme.buildCsr(domains, certKeyPath)                       // EC P-256 默认
client.finalize(order, csr)
client.pollOrder(order)                                         // 等待 valid
chainPem = client.downloadCertificate(order)                    // 完整链
acme.saveIssued(workDir, order, chainPem)                       // 落盘 + 续期元数据

// ---- 生命周期 ----
acme.certificateInfo(pemPath)      // {notAfter, notBefore, sans, issuer}（XRT x509 解析）
acme.shouldRenew(pemPath, days = 30)  // 到期判断
client.revoke(pemPath)
```

设计要点：
- **nonce 池**：每次响应收集 `Replay-Nonce` 头，请求前取新 nonce（HEAD 目录 URL），坏 nonce 自动重试一次（RFC 8555 §6.7.1 节流遵循 `Retry-After`）。
- **POST-as-GET**：空 payload 的 POST，标准库 `http.request("POST", url, "")` 表达。
- **错误模型**：6.x 问题文档映射为 `AcmeError`（含 type/detail/subproblems），不复用 ValueError。
- **并发**：单订单串行即可；多订单用 task.spawn 由调用方组合。

## 6. DNS 提供商矩阵（首批 5 平台）

统一适配器协议（全部纯 xlang，签名走标准库 crypto HMAC）：

```xlang
acme.dnsCloudflare(apiToken).setTxt(domain, value) -> recordId
acme.dnsAliyun(accessKeyId, secret).setTxt(...)     // RPC 签名 HMAC-SHA1 + 特殊 percent-encode
acme.dnsDnspod(secretId, secretKey).setTxt(...)     // TC3-HMAC-SHA256 日期签名
acme.dnsHuawei(ak, sk).setTxt(...)                  // 同类签名（接入点不同）
acme.dnsRoute53(ak, sk, region).setTxt(...)         // SigV4 + ChangeResourceRecordSets XML
// 每个 .rmTxt(recordId)；后续提供商按同协议追加
```

| 提供商 | 认证 | 签名方案 | 离线可测性 |
| --- | --- | --- | --- |
| Cloudflare | Bearer API Token | 无（纯 REST/JSON） | 请求构造断言 |
| Aliyun DNS | AK/SK | HMAC-SHA1 RPC 签名（排序参数+特殊编码） | 官方签名向量 |
| 腾讯云 DNSPod | SecretId/Key | TC3-HMAC-SHA256 | 官方签名向量 |
| 华为云 DNS | AK/SK | HMAC 签名变体 | 官方向量 |
| AWS Route53 | AK/SK | SigV4 | AWS 官方测试向量 |

提供商层是纯编排（http+json+crypto），符合 phase1 无副作用门禁——
真实 API 调用只在用户提供凭据时发生，测试用请求构造向量与响应解析夹具。

## 7. 存储布局（对标 acme.sh 的 ~/.acme）

```
<workDir>/                          # 默认 ~/.xlang/acme，可传参
├── ca/<host>/account.key           # 按 CA 域分目录的账户密钥（EC P-256 PEM）
├── <主域名>/
│   ├── <主域名>.key                 # 证书私钥（EC P-256，独立于账户密钥）
│   ├── <主域名>.cer                 # 终端实体证书
│   ├── fullchain.cer               # 完整链
│   ├── order.json                  # 续期元数据：CA、SAN、下次检查时间、DNS 提供商参数引用
│   └── conf.json                   # 签发参数快照（续期原样复用）
```

私钥文件权限收紧到当前用户（XRT 文件 API）；密钥永不进入日志与错误文本。

## 8. 测试策略

| 层 | 方式 | 门禁归属 |
| --- | --- | --- |
| JOSE/JWS | RFC 7515 附录 A 向量、RFC 7638 thumbprint 官方向量 | phase1 |
| keyauthz→TXT | RFC 8555 §8.4 例程 + certbot 已知向量 | phase1 |
| CSR | DER 结构断言 + XRT x509 回读 SAN/算法 | phase1 |
| 证书解析 | 仓库预置夹具证书（test/fixtures/acme_*.pem，openssl 预生成入库） | phase1 |
| 提供商签名 | 各云官方签名测试向量（Aliyun TC3、AWS SigV4 均有公开规范例） | phase1 |
| **状态机集成** | **mock CA**：用标准库 http.server 实现最小 ACME 服务端（目录/nonce/订单/finalize），完整跑通 dns-01 流程 | phase1 |
| 真实 CA | LE staging 全流程（范例形式，离线自动降级跳过，同 redis 先例） | 平台/手动门禁 |

mock CA 是本库离线可测性的关键：nonce 轮换、Retry-After 轮询、挑战状态
迁移全部在测试内闭环，且顺带是 http.server 标准库的活体测试。

## 9. 分期计划

| 批次 | 内容 | 前置 |
| --- | --- | --- |
| 1 | D1 spike（XRT ES256 公共 API）+ D2 spike（CSR 构造路线定夺）+ jose.xl + JWS/JOSE 全向量测试 + 账户注册/订单/挑战状态机 + mock CA 集成测试 | XRT 一行封装（D1） |
| 2 | dns-01 完整闭环 + Cloudflare/Aliyun/DNSPod 三提供商 + CSR + finalize + 证书下载/解析/存储 + phase1 登记 | 批次 1 |
| 3 | http-01（standalone+webroot）+ EAB（ZeroSSL）+ Route53/华为 + revoke + 续期簿记与守护范例 | 批次 2 |

## 10. 风险与开放问题

1. **D1 阻塞度**：若 XRT 不便立即加公共 API，可临时以 RS256（已公开）先行
   完成批次 1-2 的全部链路，ECC 账户密钥作为批次 2.5 增强——LE 同时支持两者。
2. **PEBKAC 风险（域名验证安全）**：keyauthz 拼接与 TXT 值计算必须只用官方
   向量交叉验证（oauth2 批次的教训：独立实现对照），禁止自造测试值。
3. **提供商 API 漂移**：响应解析保持宽松（只取必要字段），夹具锁定响应样本。
4. **短周期证书（LE 6 天 profile）**：设计上 shouldRenew 按 notAfter 百分比
   而非常量天数可扩展，三期评估。
5. **多账户/多视图**：一期一 CA 一账户密钥；多账户为 order.json 的 ca 字段
   天然支持，暂不做 UI 级管理。

## 11. 验收门禁（并入批次 2 完成时）

```powershell
tools\run_phase1_extlibs.ps1    # acme 进入清单
# 手动（可选）：
#   范例对 LE staging 签发一张测试证书（需真实域名+DNS 凭据）
```
