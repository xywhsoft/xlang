# 受管理 RSA 密钥与签名

2026-10-22：已批准 P4 的 RSA 拥有型组件与运算接入。全部数学、随机盲化、PSS
和 PKCS#1 签名算法只复用固定 XRT，适配驻留 runtime；Cgen 不生成实现函数体。
RSA keygen／OAEP 仍不在本轮范围，显式 DER／PEM 密钥格式留在后续批次；不能
把本批 raw 组件导入或已有通用 ASN.1 读取当成全部密钥格式已经完成。

## 拥有型组件导入与导出

| API | 合同 |
|---|---|
| `RsaPublicKey(bytes modulus, bytes exponent)` | 复制完整大端无符号组件，按原生公钥运算验证其结构与边界 |
| `key.modulus`／`key.exponent` | 独立拥有的 bytes，属性只读，不泄露内部指针 |
| `key.modulusSize`／`key.modulusBits` | 模数字节数／实际位数，不把字节数乘八冒充实际位数 |
| `RsaPrivateKey(modulus, exponent, privateExponent)` | 导入完整私有指数路径；三个参数都为 bytes |
| `RsaPrivateKey(modulus, exponent, privateExponent, prime1, prime2, exponent1, exponent2, coefficient)` | 导入完整 CRT 五参数；此形态允许 privateExponent 为 b""，但不得仅提供一部分 CRT 参数 |
| `privateKey.publicKey` | 返回独立的受管理 RsaPublicKey；不借用私钥对象的存储 |
| `privateKey.hasCrt` | 是否有完整 CRT 五参数；原生优先使用 CRT，否则使用完整私有指数 |
| `privateKey.exportPart(RsaPrivatePart part)` | 显式导出独立私有组件 bytes；未保存的可选组件返回 b"" |

RsaPrivatePart 的完整成员为 `privateExponent/prime1/prime2/exponent1/exponent2/coefficient`。
CRT 的两个指数对应 d mod(p−1)、d mod(q−1)，系数对应 q 的模 p 逆元；组件定义见
[RFC 8017 §3.2](https://www.rfc-editor.org/rfc/rfc8017.html#section-3.2)。
本层不推导缺失组件、不修复密钥、不归一化提供的 raw 字节，也不接收 HEX 字符串。
模数遵循固定 XRT 的 128—1024 字节边界，具体实际位数通过属性查询；这是
底层兼容能力范围，不是新协议的密钥强度政策或生产推荐。

私钥对象一次分配并复制全部组件，内部视图重新绑定到自有存储。导入时沿用
XRT TLS 身份所采用的原生运算自检：固定输入的私钥运算加原生公钥复核。
这是操作一致性检查，不宣称完成数学／素性认证。后续每次私钥运算仍保留
原生随机盲化和结果公钥复核，不因导入已自检而跳过这些措施。

两类密钥都是不可变 resident leaf，不持有语言值、回调或生成代码地址，
对象存活期间允许跨线程只读。私钥销毁擦除整个拥有块并保留已有错误；
导出的私有 bytes 是普通值，不承诺锁页、安全堆或消除调用方副本。

## 摘要域与 PKCS#1 v1.5 签名

`RsaHash` 独立于 ECDSA 的 SignatureHash，完整成员为
`sha1Legacy/sha224/sha256/sha384/sha512`，只公开固定原生后端实际支持的选择。
SHA1 必须显式写 Legacy 名称，默认全部为 SHA256。不能用整数、SignatureHash
或其他枚举代替 RsaHash；驻留入口也校验实际值和摘要的完整字节长度。

`privateKey.signPkcs1Hash(bytes digest, RsaHash algorithm=RsaHash.sha256)` 返回
模数等宽的原始签名 bytes；`publicKey.verifyPkcs1Hash(digest, signature, algorithm=...)`
验证相同完整摘要与签名。接口接收的是摘要，不隐式哈希或二次哈希；签名
不是 HEX，也不是 DER 包装，更不是 PKCS#1 v1.5 加密。

## PSS：默认严格，例外显式

| 私钥方法 | 行为 |
|---|---|
| `signPssHash(digest, algorithm=sha256)` | OS 安全随机盐，长度等于消息摘要长度；MGF1 使用相同算法 |
| `signPssHashWithMask(digest, algorithm, maskAlgorithm)` | 显式不同 MGF1 摘要，仍使用原生随机盐 |
| `signPssHashSalt(digest, bytes salt, algorithm=sha256)` | 显式完整盐，MGF1 与消息摘要相同 |
| `signPssHashSaltWithMask(digest, salt, algorithm, maskAlgorithm)` | 显式完整盐与 MGF1 算法 |

普通应用优先使用随机盐入口；显式盐入口供已经明确盐值的协议互操作等场合。
空盐是显式允许的原生能力。盐按完整 bytes 传入，NUL 不截断；不提供 size 截断。

| 公钥方法 | 行为 |
|---|---|
| `verifyPssHash(digest, signature, algorithm=sha256, saltLength=-1)` | MGF1 与消息摘要相同，严格匹配指定盐长度 |
| `verifyPssHashWithMask(digest, signature, algorithm, maskAlgorithm, saltLength=-1)` | 显式 MGF1 算法，仍严格匹配盐长度 |
| `verifyPssHashAnySalt(digest, signature, algorithm=sha256)` | 显式允许原生编码携带的任意合法盐长度 |
| `verifyPssHashAnySaltWithMask(digest, signature, algorithm, maskAlgorithm)` | 显式任意合法盐长度及 MGF1 算法 |

上表的 sha256 参数均为 RsaHash 成员。saltLength=-1 只表示消息摘要长度，不表示
任意长度；非负值表示精确字节数，其他负值、不能表示或不能容纳的长度 checked
报范围错误。默认 MGF 跟随消息算法，不会在调用者改为 SHA384 时悄悄仍用 SHA256。
模数无法容纳所选算法／盐时 checked 失败，不截断、不回退。

```xlang
func signed(RsaPrivateKey key, bytes message) as bytes
    return key.signPssHash(crypto.sha256(message))
end

func verified(RsaPublicKey key, bytes message, bytes signature) as bool
    return key.verifyPssHash(crypto.sha256(message), signature)
end
```

## 错误、效果和高级运算

只有精确的 `xrt.crypto` 域与 `XERR_VALUE/XCRYPTO_ERROR_KEY` 或
`XERR_PROTOCOL/XCRYPTO_ERROR_SIGNATURE` 验证拒绝转为 false。长度／枚举参数、
OOM、未知域／code 等仍 checked 报错；不用诊断文本或 Operation 判断，保留
调用前第一项错误，失败不交付部分签名或半个密钥对象。

公钥读取／验证为 pure＋nonblocking，但可能报错，不标 no-raise。公钥构造
nonblocking；两种析构 no-raise＋nonblocking。私钥导入自检、所有签名及 raw
私钥运算都会执行原生随机盲化，可能等待 OS 随机源，因此不标 pure/nonblocking；
即使 PKCS#1 的最终签名可重复，或 PSS 盐由调用方给出，也不能省略这条合同。

`cryptoAdvanced.rsaPublic(RsaPublicKey key, bytes input)` 与
`cryptoAdvanced.rsaPrivate(RsaPrivateKey key, bytes input)` 是显式无填充原始运算，
输入／输出严格模数等宽。它们不是安全加解密 API，不用来模拟缺失的 OAEP。
算法继续由 XRT 处理，驻留层不实现模幂、盲化、MGF1 或签名编码。

## 裁剪、迁移与验收

公钥、私钥、各自目录、两个 raw 出口、PSS 验证、PKCS#1 验证、随机盐签名、
显式盐签名和 PKCS#1 签名分为 11 个适配单元。公钥导入／验证不依赖私钥运算、
也不调用安全随机源；只读组件不引入签名编码；PKCS#1-only 不引入 PSS／摘要计算模块；
enum-only 和现有 NIST／Ed25519／XDH／SHA256 不引入本批适配。私钥导入为
执行原生自检而依赖私钥运算及底层 xrtSecureRandom，是已声明能力所需的调用；
后者不能仅凭可选 random_secure 模块是否出现来判断。模块裁剪证据不冒充
全部底层平台函数已逐函数剔除的机器码证明。

旧 rsaPssVerify／rsaPkcs1Verify 的 HEX／摘要猜测入口、内部桥接与依赖登记
清退，不保留兼容别名；同时清理五组已经无引用的旧 crypto 参数描述。
综合旧测试只移除两个过期 RSA 负例，本批固定用例在独立专项内验收，
不运行其余旧密码学／网络安全专项。

专项 `tools/check_stdlib_rsa_source.py` 提供 13 个本批用例及 6 个 NIST／Ed25519／
XDH／SHA256 回归，覆盖 JIT、module、独立 AOT、严格 GCC O0／O2、实际闭包和
裁剪；另有 12 个静态拒绝、索引过期门禁、24 个目标的 302 项坏合同拒绝。
原生闭包以 GCC O0／O2、TCC 直接编译链接，Linux 另加 ASan／UBSan；每种模式
核查 49 个分配前缀、23 次实际失败、4 个真实 leaf／目录拥有图和 11 类 typed
错误边界，回滚无部分输出、分配账本平衡。固定数据来自 XRT 已提交的公开
RSA 测试夹具，只复用常量，不运行其负例／故障 CRT／时序等专项。

实际验收 Windows 239／Linux 241 阶段，共 480 阶段全部通过；当前源码分别
重建 881／880 个编译单元，TU 不混计阶段。输入、日志及产物 SHA 已复核。
证据与提交回执：`F:/xlang-validation/five-groups-20261022/p4-rsa/verified-commit.json`。
只做离线常规语言／ABI／生命周期检查和正常不同消息验证失败；没有网络流量、
畸形密钥／签名攻击样本或安全审计，不更新发布 DLL／SO／VFS。
