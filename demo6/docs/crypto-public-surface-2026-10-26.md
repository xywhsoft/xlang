# 密码学公开面与密钥格式的普通产品验收

## 结果

本批不改 runtime、Cgen、XRT 快照或时间 ABI。使用上一批正常全构建的真实
CLI／运行库／编译器库，Windows、Linux 各通过 166 个产品阶段，共 332 个。
不使用源码替代实现，不覆盖仓库 release 产品，也不是密码学安全审计。

`tools/crypto_surface_public_fixtures.json` 选择 27 正例、3 编译拒绝例。覆盖：

- 摘要／HMAC 的完整二进制输入与流快照、PBKDF2／HKDF；
- 正常 AEAD、XDH／ECDH、Ed25519 pure/context/prehash、ECDSA raw／DER，
  RSA PKCS1／PSS 的摘要／MGF／盐选择、受管理拥有值与显式高级原语；
- 七种密钥拥有对象的 PKCS8／SPKI、RSA PKCS1、NIST SEC1 和对应 PEM；
- 各算法单独使用，以及 crypto／DER／PEM 私有句柄访问的编译期拒绝。

新 `stdlib_crypto_normal_public.xl` 复用既有公开固定向量中的正常操作，
不执行其中的错误编码、短密钥／签名、篡改等检查。原综合测试保持不变。
普通完整签名对另一个正常消息返回 false 属于既有 bool 语义，不构造畸形
签名。新 NUL AEAD 往返使用独立 nonce。固定 RSA 测试密钥不是生产强度建议。

`stdlib_crypto_key_format_values.xl` 通过实际语言公开方法，调用全部 74 个
密钥格式驻留入口。核对的是生成 C 中实际赋值调用，不是仅有声明／符号名；
结果经实际 JIT、module、独立 AOT 和隔离 CLI 的往返断言验证。包含 PKCS8
version 0／1、NIST SEC1 含／不含公钥、五种曲线的明确公钥格式函数。

每个正例还有严格 GCC O0／O2 检查、重复 C 输出一致性、可搬移独立 AOT 的
真实依赖检查与 checked 退役；独立程序不依赖 xl DLL／SO。负例 checked
拒绝且不输出 C／头文件，不运行被拒绝的程序。

## 57 模块的角色复核

范围账本本轮新增 55 项密码学／ASN.1 角色复核。内部大整数、通用曲线及
参数化签名／验证模块继续由具体接口使用，不另造裸状态 API。需要区别：

| 原生便利函数 | 语言侧等价接入 |
|---|---|
| PEM EncodeNew／DecodeNew | 驻留层有界输出与拥有数据，不泄露原生分配指针 |
| AEAD Encrypt／Decrypt 的分离输出指针 | 受管理 seal／open，拥有式 cipher‖tag |
| GMAC／Verify | 在已批准的 nonce／tag 合同内，空消息 GCM 与 AAD 组合；不是新增独立算法 |
| Ed25519 SignKey／Verify 的 pure 形式 | 统一 Ex 模式，普通／context／prehash 分名 |
| Ed25519 KeyPair 的 seed/public 便利输出 | OS 安全随机加一次 KeyInit，避免公钥重复派生 |

`crypto_aes` 原始块操作、`crypto_poly1305` 独立 MAC 的公开 API 范围仍待
用户确认；不能因为已经被 AEAD 使用就认定独立公开面完成。原生算法不重写。
账本共 89 项角色已复核、109 项本次复核 pending，后者不是未实现模块数量。
静态库存检查两端各拒绝 11 项坏数据，不等于功能／竞态或安全验收。

## 证据与剩余边界

证据根 `F:/xlang-validation/five-groups-20261026/p7-module-audit/`，最终独立
回执为 `verified-crypto-surface-windows.json`／`verified-crypto-surface-linux.json`。
原构建输入、真实产品、日志／输出、阶段集合、用例部署及 74 个调用均复核；
Linux 891 个文件归档后逐项校验且原件保留。当前 2,490 个已记录核心构建
输入与复用源码 cohort 仍相同；该内容比较本身不是验收阶段。

继续 X.509／TLS／网络逐模块复核、剩余旧调用及最终联合／嵌套编译门，
不以本批替代五组总封板。无网络流量、扫描、攻击模拟或安全性质专项。
未决策 CRT 0／XRT 11 不变，内存池最后；完整类别见
[并发回归的范围记录](concurrency-public-closure-2026-10-26.md)。
