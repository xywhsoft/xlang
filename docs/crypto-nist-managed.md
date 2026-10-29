# 受管理 P-256／P-384 密钥

2026-10-22：已批准 P4 的 NIST 曲线增量。只复用固定 XRT 的 P-256／P-384、
ECDH、ECDSA 与 DER 算法；适配驻留 runtime，Cgen 不生成算法或包装函数体。
本批不等于完成 RSA、全部密钥格式、X.509、TLS 或五组发布验收。

## 密钥与交换

| API | 合同 |
|---|---|
| `P256Key(bytes privateKey)`／`P384Key(bytes privateKey)` | 导入完整 32／48 字节大端私钥标量；复制并验证，缓存公钥 |
| `crypto.p256KeyPair()`／`crypto.p384KeyPair()` | 使用 XRT 的 OS 安全随机源生成受管理密钥，不返回动态字典 |
| `key.publicKey` | 独立 bytes：65／97 字节未压缩 SEC1 公钥，首字节 0x04；只读属性 |
| `key.exportPrivate()` | 显式返回独立 32／48 字节私钥 bytes |
| `key.sharedSecret(bytes peerPublicKey)` | 返回完整 32／48 字节 ECDH 横坐标 bytes；保留原生私钥／对端点校验 |
| `crypto.p256PublicKey(privateKey)`／`crypto.p384PublicKey(privateKey)` | 一次性公钥派生，不构造受管理密钥对象 |

原始共享秘密不是应用会话密钥；应用按协议使用 HKDF 等既有 KDF。
不隐式接受 HEX、DER／PEM 私钥、压缩点或其他曲线的公钥，不截断密钥材料。
DER／PEM 密钥容器属于后续显式格式接入，不以 raw 导入假装已经完成。

## ECDSA：摘要与编码明确分离

`SignatureHash` 是名义数值枚举，仅包含 `sha256`、`sha384`、`sha512`。
签名算法不由摘要长度推测，传整数或其他枚举在语义检查时拒绝。
驻留入口仍校验实际枚举值及摘要长度，不能通过组合位值绕过校验。

| API | 合同 |
|---|---|
| `key.signHash(bytes digest, SignatureHash algorithm=...)` | 返回 64／96 字节定宽大端 raw r‖s 签名 |
| `key.signHashDer(bytes digest, SignatureHash algorithm=...)` | 返回规范 DER 签名，不混用 raw 编码 |
| `crypto.ecdsaP256VerifyHash(publicKey, digest, signature)`／`ecdsaP384VerifyHash` | 完整 bytes 公钥／摘要／raw 签名，返回 bool |
| `crypto.ecdsaP256VerifyHashDer(publicKey, digest, signature)`／`ecdsaP384VerifyHashDer` | 显式 DER 签名验证 |
| `crypto.ecdsaP256ToDer(signature)`／`ecdsaP384ToDer` | 定宽 raw 签名编码为 DER，不执行曲线运算 |
| `crypto.ecdsaP256FromDer(signature)`／`ecdsaP384FromDer` | 严格规范 DER 解码为定宽 raw；格式错误 checked 报错 |

P256Key 默认 SHA256，P384Key 默认 SHA384；可显式选择上述三个原生 RFC6979
后端。签名摘要必须完整匹配算法的 32／48／64 字节，不隐式哈希或二次哈希。
验证复用 XRT 的任意非空摘要合同，由原生实现执行 ECDSA bits2int；不根据摘要
宽度猜算法，也不宣称识别摘要来源。所有这些输入都是完整密码学材料，没有
消息前缀 size 参数。需要文本／Buffer 消息时先显式调用已有摘要接口。

签名使用 XRT 确定性 nonce 与 low-S 输出。固定向量来自
[RFC 6979 A.2.5／A.2.6](https://www.rfc-editor.org/rfc/rfc6979.html#appendix-A.2.5)，
验收常量的 s 按原生合同取 min(s,n-s)，不修改原生算法。
DER 签名只计算一次，在有界栈空间暂存后复制到拥有式 bytes，不为查询长度
重复执行曲线签名；临时空间始终擦除。

```xlang
auto key = crypto.p256KeyPair()
bytes digest = crypto.sha256("a\0你")
bytes signature = key.signHashDer(digest)
assert crypto.ecdsaP256VerifyHashDer(key.publicKey, digest, signature)
```

只有 `xrt.crypto` 域的精确原生验证拒绝
`XERR_VALUE/XCRYPTO_ERROR_KEY` 或 `XERR_PROTOCOL/XCRYPTO_ERROR_SIGNATURE` 转为 false。
参数长度、空摘要、分配、未知域／code、状态等错误仍 checked 传播；不匹配
诊断文本或 Operation，不把 OOM 吞成 false，并保留调用前第一项错误。
独立 DER 解码不采用 bool 验证的降格规则。

## 显式高级点运算

`cryptoAdvanced.p256Valid(point)`／`p384Valid` 检查完整未压缩公钥。
`p256Multiply(scalar, point)`／`p384Multiply` 与 `p256Add(left, right)`／`p384Add`
返回完整公钥 bytes，继续使用原生标量／点验证；无效参数不交付部分结果。
这些不是跨曲线通用整数运算，也不是绕过 sharedSecret 校验的默认 ECDH 入口。

## 生命周期、效果与裁剪

两种密钥都是不可变 resident leaf，不保留语言值、回调或生成代码地址。
统一 native handle／拥有目录管理生命周期，对象存活期间允许跨线程只读；
析构擦除整个私有 payload 并保留已有错误。显式私钥导出是普通 bytes，
不承诺安全内存、锁页或消除语言副本。

只读计算声明 pure＋nonblocking，但可能分配或报错，不能标 no-raise。
构造 nonblocking；析构 no-raise＋nonblocking；OS 随机生成不声明 nonblocking。
两条曲线分别拆分核心、目录、生成、交换、raw／DER 签名、公钥、raw／DER 验证
及高级点运算，纯 DER 转换独立于曲线计算。

导入不拉入随机源、签名或验证；只公钥／只验证／高级 raw／DER 转换不引入
受管理密钥底座。P256-only 不带入 P384，反向同样成立；仅用 SignatureHash
不带入曲线适配；已有 Ed25519、XDH、SHA256 不带入本批适配。
显式可选签名算法保证对应 HMAC 后端完整可用，不以当前碰巧启用的模块决定
同一个公开调用是否支持 SHA384／SHA512。

旧 p256Keypair／p384Keypair、HEX sharedSecret、自动猜曲线的 ecdsaVerify 及
内部桥接登记删除，不保留兼容别名。旧综合测试只迁移本批小节，不作为其余
旧密码学函数已经通过验收的证据。

## 验收与证据

专项 `tools/check_stdlib_nist_source.py` 包含 12 个本批语言用例与 5 个 Ed25519／
XDH／SHA256 回归，JIT、module、独立 AOT、严格 GCC O0／O2、实际闭包和裁剪，
另有 10 个静态拒绝、过期索引门禁，以及 32 个目标／316 项坏合同的全模块、
C 和声明头输出拒绝检查。原生闭包直接以 GCC O0／O2、TCC 编译链接，Linux
另加 ASan／UBSan；每种模式检查 72 个分配前缀、38 次实际失败、4 个真实
leaf／目录拥有图和 11 类 typed 错误边界，失败无部分输出且分配账本平衡。

只做离线固定公开向量、正常不同消息验证失败、语言／ABI／拥有权和 OOM 检查。
没有畸形点／签名攻击样本、网络流量或网络安全专项；不更新发布 DLL／SO／VFS。
正式双端结果及提交回执位于
`F:/xlang-validation/five-groups-20261022/p4-nist/verified-commit.json`；
Windows 215／Linux 217 阶段全部通过，共 432 阶段；完整当前源码为 870／869
个编译单元，不混计阶段。正式轮次 `source-win-r2`／`accept-win-r2` 与
`source-linux-r1`／`accept-linux-r1`，输入、日志和产物 SHA 均复核。
失败／准备轮保留但不混入成功，Linux 原始证据与 F 盘镜像同时保留。
