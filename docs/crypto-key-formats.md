# 显式密钥格式接入

状态：P4 的共享格式底座及下表语言方法、声明／XLIR／拥有权／产物依赖接线
已在源码中实现并通过固定输入的双平台联合复验。2026-10-26 的
[普通产品验收](crypto-public-surface-2026-10-26.md) 又在真实双端 CLI／两库／
VFS 中执行全部 74 个密钥格式驻留入口，覆盖语言 JIT／module／独立 AOT。
不覆盖仓库 release 资产，也不代表整个长期任务或安全审计完成。

## 接入目标与依赖

复用现有 RSA、P256／P384、Ed25519、X25519／X448 密钥对象及叶拥有权策略。
新代码仅处理明确的 ASN.1 schema、PEM 外壳和拥有数据，不实现密码算法。
先有共享 wire 合同，再接 typed 工厂／方法及实际模块／AOT／裁剪验收，最后
将同一格式底座交给 X.509／TLS 使用，避免三处分别猜测或解析私钥。

源码公开面：

| 现有类型 | 静态导入／输出方法 |
|---|---|
| RsaPublicKey | fromSpki/toSpki、fromPkcs1/toPkcs1 |
| RsaPrivateKey | fromPkcs8/toPkcs8、fromPkcs1/toPkcs1 |
| P256Key／P384Key | fromPkcs8/toPkcs8、fromSec1/toSec1、publicSpki |
| Ed25519Key／X25519Key／X448Key | fromPkcs8/toPkcs8、publicSpki |
| 无独立公钥对象的五种曲线 | 显式曲线命名的 publicFromSpki/publicToSpki，返回完整 bytes |

对应 PEM 入口显式带 Pem 后缀，固定 PUBLIC KEY／PRIVATE KEY／RSA PUBLIC KEY／
RSA PRIVATE KEY／EC PRIVATE KEY 标签，不提供一个靠长度或内容猜算法的入口。
私钥输出默认 PKCS#8 version 0；显式 includePublic 才输出 RFC5958 version 1。
格式函数不接受消息式 size 截断：密钥和编码输入必须完整。

```xlang
auto key = X25519Key.fromPkcs8(encodedPrivateKey)
bytes publicDer = key.publicSpki()
bytes rawPublic = crypto.x25519PublicFromSpki(publicDer)
string privatePem = key.toPkcs8Pem()
```

五种曲线均有 `crypto.<算法>PublicFromSpki`／`PublicToSpki` 和相应 Pem
入口。默认 PKCS#8 不附带外层公钥，`toPkcs8(true)`／`toPkcs8Pem(true)`
明确要求 RFC5958 version 1。`toSec1()` 默认保留内部公钥，可显式传 false；
这与 PKCS#8 的外层公钥开关不是同一项配置。

## 已实现的共享合同

- SPKI 明确校验算法 OID 和参数。RSA 接受 NULL 或缺省参数，写出 NULL；
  NIST EC 要求匹配 named-curve OID；RFC8410 参数必须缺省。
  受限 RSA-PSS／EC-DH OID 不会被降格为不受限密钥。
- PKCS#8／RFC5958 DER 支持 version 0／1、合法的可选 attributes 及隐式
  publicKey。版本必须与公钥是否出现一致，整份输入必须恰好消费完。
  属性验证后不保留，输出不是带属性文档的字节级往返承诺。
- RSA PKCS#1 使用最短、非负且非零整数，两素数私钥需要完整八组件。
  不伪造缺失的 CRT 字段、不分解只有 n/e/d 的私钥、不宣称多素数支持。
- ECPrivateKey 支持具名 P256/P384、固定宽度私钥和可选公开点。
  独立 SEC1 输入必须带曲线参数；嵌入 PKCS#8 时可使用已验证的外层曲线。
  写出时总是包含曲线参数。只支持已有 XRT 使用的非压缩点表示。
- RFC8410 私钥保留规范的双层 OCTET STRING，不将裸标量误当完整 PKCS#8。
- 辅助公钥的视图与私钥分别返回。受管理工厂与实际派生公钥逐一比对后
  才能发布对象，嵌套 SEC1 和外层 RFC5958 公钥分别验证；wire shape 校验
  不能冒称数学有效性验证。
- PEM 要求恰好一块目标标签，块外只允许空白；多块选择仍交给已有 PemReader。
  只用显式字节长度，不将用户输入交给 strlen。DER／文本预算分别 16／32 MiB。

普通借用视图只在同步原生调用内存在。导入复制进现有不可变密钥 payload；
格式写入先准备临时字段，成功后原子追加一份 TLV。分配失败不交付半成品，
清理擦除实际 buffer capacity、保留第一项诊断，且不吞掉原本没有先前错误时的
新清理错误。明确导出的私钥 DER／PEM 仍是普通语言 bytes/string，不承诺锁页
或消除调用方所有副本。

本项只接 DER，不宣称完整 BER 接收能力、加密 PKCS#8／PEM 或 PKCS#12。
读取／导出可标 pure＋nonblocking，但不能标 no-raise；RSA 私钥导入沿用
已有原生自检／随机盲化，不能误标 pure／nonblocking。

## 验收边界

`tools/check_key_format_wire.py` 从既有测试中只提取公开固定常量，不执行旧
原生测试。独立 cryptography／OpenSSL 后端生成六种有效密钥及正常标准格式；
XRT 写出的普通 SPKI、PKCS#8、PKCS#1、SEC1、PEM 再由该后端读取并核对真实公钥。
RFC8410 的独立 version 0／1＋attribute 示例另行检查。version 1 不冒称所有
OpenSSL 导入器都支持；未进行畸形密钥／点／签名攻击样本或网络安全专项。

每配置检查 398 个分配预算、362 次实际失败及精确内存账本，包含失败原子性、
同宽不同算法的显式身份拒绝、输出别名拒绝、正确 PEM 标签与先前错误拥有权。
实际 Windows GCC O0／O2／TCC，Linux 另加 ASan／UBSan；正式回执及哈希见
`F:/xlang-validation/five-groups-20261023/p4-key-formats`。

`tools/check_stdlib_key_formats.py` 验收完整语言接线：74 个精确原生入口、
22 个拥有型工厂，沿用原来的七种密钥叶策略，没有新增生命周期模型。
五种原始构造只增加私有同步 span 入口，避免导入时先构造临时语言 bytes。
原生输出直接转移 Buffer payload，不经过临时语言 Buffer 或 Cgen 格式包装。

每配置包含 1008 个分配预算、934 次实际失败，七种真实叶的 Hold／Claim／
Restore，14 个发布预算及 10 处有效但不匹配的辅助公钥拒绝；所有路径核对
精确内存账本。n/e/d-only 的合法 RSA 私钥仍可使用既有能力，但完整格式输出
以 checked 错误拒绝，不补造 CRT 参数。受管理对象输出另由独立库读取。

完整 source-driver 使用当前全部编译单元，而非替换个别旧对象：实际语言
静态工厂、方法、原有构造和 typed 容器拥有值通过 JIT／module／独立 AOT，
生成 C 通过 GCC O0／O2。812 处签名／效果／拥有者坏合同及 74 个完整模块／
C／头文件输出门验证拒绝行为。12 个单独产物验证按算法、DER／PEM 和
SEC1／PKCS#8 裁剪：裸公钥格式不包含私钥核心，DER 不包含 PEM，独立 SEC1
不包含外层 PKCS#8，且不启用 MODULE_ALL。

标准库函数表增加 20 个命名空间函数，索引使用既有生成器更新到 1023 项，
静态方法不混入该表。74 个声明、登记和 IR 合同做集合相等校验。Cgen 未增加
任何密钥解析／格式写入实现，适配位于按需选取的驻留组件。

本批正式验收 Windows 154／Linux 156，共 310 阶段，其中各有 145 个语言／
IR／裁剪阶段，原生专项分别 9／11 阶段。全源码构建分别为 956／955 个 TU，
不混计为验收阶段。输入／日志／产物哈希另行独立复核；原始与镜像证据、
独立提交身份见上述证据根的 `verified-api-commit.json`。

本轮不修改独立 XRT、固定快照或两边时间 ABI，不重建发布资产。X.509／TLS
及最终 shipping 联合验收仍是长期任务的后续工作。

依据：[RFC8410](https://www.rfc-editor.org/rfc/rfc8410.html)、
[RFC5958](https://www.rfc-editor.org/rfc/rfc5958.html)、
[RFC5915](https://www.rfc-editor.org/rfc/rfc5915.html)、
[RFC5480](https://www.rfc-editor.org/rfc/rfc5480.html)、
[RFC8017](https://www.rfc-editor.org/rfc/rfc8017.html)。
