# X.509 签名检查与独立 SPKI 公钥

此层只回答签名是否被指定的公钥／issuer 接受，不判断路径、信任、名称、
用途、有效期、撤销状态或 TLS 对端身份。不得据此接受网络对端。算法复用
XRT，拥有与 checked 适配驻留 runtime；Cgen 没有新增包装实现或符号特例。

## 公开接口

| 类型 | 入口 | 结果 |
|---|---|---|
| `Certificate` | `checkSignature(issuer)`／`checkSignatureWithKey(publicKey)` | `X509SignatureStatus` |
| `Certificate` | `verifySignature(issuer)`／`verifySignatureWithKey(publicKey)` | 无返回值；拒绝／不支持走 checked 错误 |
| `CertificateRevocationList` | 同上四个入口 | 同上；不计算撤销覆盖或证书状态 |
| `X509SignatureInfo` | `check(content, signature, publicKey, size = -1)` | `X509SignatureStatus` |
| `X509SignatureInfo` | `verify(content, signature, publicKey, size = -1)` | 无返回值；严格 checked |
| `X509PublicKeyInfo` | `fromSpkiDer(encoded, maxBytes = 16777216)` | 独立拥有的公钥信息 |

`issuer` 是 Certificate；`publicKey` 是 X509PublicKeyInfo。content 分别接受
string、bytes、Buffer 三种重载，signature 和 encoded 都必须是 bytes。
本批为 15 个精确驻留入口、一个拥有工厂、六个按需 provider 和一个封闭枚举，
复用现有公钥信息叶策略及拥有目录，不新建平行资源管理体系。

`X509SignatureStatus.valid = 1`、`rejected = 2`、`unsupported = 3`。
rejected 包含原生验证器认定的签名／密钥或参数不匹配，不应仅解读为数学
签名位串错误。unsupported 表示未知／当前构建未提供的签名能力，不是 valid。
严格 verify 不把这些错误变成状态值，保留原生诊断与 cause。

```xlang
auto issuer = Certificate.fromDer(issuerDer)
auto certificate = Certificate.fromDer(certificateDer)
if certificate.checkSignature(issuer) == X509SignatureStatus.valid then
    // 仅签名检查通过；这里不能宣称证书可信。
end
auto key = X509PublicKeyInfo.fromSpkiDer(issuer.publicSpki)
certificate.verifySignatureWithKey(key)
```

## 输入、拥有和错误合同

content 使用真实字节长度，保留内嵌 NUL；size 缺省 -1 取全长，非负值取
min(size, 实际字节长度)，小于 -1 抛 RangeError。size 可能截在 UTF-8 代码点
中间，因为签名操作的是字节而非字符。signature 和公钥绝不按 size 截断。
同步调用不复制只读内容；Buffer 可变，调用方必须避免并发写入。

SPKI 必须消费完整 DER，maxBytes 是预算而不是截断长度。0 显式关闭该预算，
负值抛 ValueError（原生 XERR_ARGUMENT），超限抛 RangeError。记录复制输入
并将内部视图重定位到自有数据，输入释放后仍有效；不伪造 Certificate 来
解析公钥，不重复实现 RSA／EC／OKP ASN.1 算法。可直接读取现有受管理 RSA、
P256／P384、Ed25519、X25519／X448 密钥导出的 SPKI。原生解析还保留 Ed448
类型信息，但这不意味着 Ed448 签名验证可用。

check 只将已识别的 XRT X.509 签名拒绝／不支持诊断及其已知签名 cause
转换成状态。Memory、Argument、Range、State、Internal 或未知诊断域／cause
仍走 checked 错误；保留调用前错误，失败不能清除其他错误或误报 valid。
原生 false 却无诊断时生成 checked 状态错误，不猜测失败原因。

语言异常名以实际映射为准：XERR_ARGUMENT → ValueError，XERR_STATE →
RuntimeError，XERR_UNSUPPORTED → NotImplementedError，xrt.x509 的
XERR_PROTOCOL → IOError，XERR_RANGE → RangeError。typed catch 必须写成
`catch RangeError error`；`catch RangeError` 是名为 RangeError 的 catch-all
变量，不是类型过滤。本批将旧四组夹具改为真正的 typed catch，并用错误
类型不匹配的内层 catch 验证它不会吞掉外层 RangeError。

## 原生依赖与裁剪

证书／消息验证启用 x509_verify 及 RSA PKCS#1／PSS、P256／P384 ECDSA、
Ed25519 三类原生后端；CRL 另启用 x509_crl_verify。没有第二套密码算法。
Ed448 返回 unsupported；实际未编入某后端的原生产物也必须报告 unsupported。
这与未知 OID 元信息可读取是两个独立合同。

SPKI 解析只依赖 x509_parse，不拉入密码验证；纯状态枚举不引入 X.509、
crypto 或 Buffer。Buffer 重载只在需要 Buffer 类时组合到标准库声明，
不为未使用方法强制引入整个 Buffer。消息验证不引入 CRL 验证；这些程序
均不应拉入 CRL policy、path、store 或 TLS。

独立 XRT 新增零分配 `xrtX509PublicKeyParse`，旧证书公钥读取复用同一实现。
仅定点同步实现／声明与必要元数据到固定 XRT／xruntime，公开入口数为
3370／3835（xruntime 自有 465）。同时修复原生裁剪目录漏记的 17 个既有
NetFile Future 符号归属；双平台 426 根探测闭合。没有整体升级快照或时间
ABI，独立毫秒／For 和固定微秒／Until 的合同保持原样。可逆片段证据记录
除目标 SPKI 增补外其余字节未变。

## 验收与未完成边界

正式证据根 `F:/xlang-validation/five-groups-20261023/p5-x509-signatures`。
工具为 `check_x509_verify.py`、`check_stdlib_x509_verify.py`；六组正常离线
固定密钥样例由独立库先核对 issuer／证书／CRL／消息签名。包含五种可用
签名方案及 Ed448 不支持、另一份有效 issuer 的正常拒绝、SPKI 拥有、内嵌
NUL、可选长度、跨线程只读和受管理密钥格式互操作。没有网络流量、畸形
密码学样本、模糊测试或网络安全专项。

Windows 522／Linux 526，共 1048 个阶段通过，包含独立重放；完整源码构建
1054／1053 TU，不混计阶段。两端各有新语言／IR／裁剪 104，CRL 数据回归
115，profile 93，记录 92，原始数据 59，旧 DER／密钥／数学 9 个阶段。
原生完整配置覆盖 62 个分配预算、11 次实际失败；真实缺少三个后端的产物
另行验证。274 处新坏合同与 15 个完整输出门、旧四组合同／输出门继续通过。
原生 standalone SPKI 的七类正常公钥在禁止分配的配置中通过模块化／单头、
GCC O0／O2、TCC 和 Linux ASan／UBSan；常规驻留测试也覆盖这些编译配置。

输入、日志、产物、源码驱动及目录身份独立复核，Linux 原件与 F 盘逐文件
哈希镜像同时保留；双仓提交身份见 `verified-commit.json`。独立 XRT 仅认证
本批相关源码段及产物，不声称包含其他工作区修改的整个生成头已同步。
这是冻结完整工作区的源码验收，不是纯净 checkout、发布 DLL／SO／VFS 或
shipping 验收。无关工作区、原暂存区和两份 RSP 的既有重构保持不变。

完整 P5 仍缺 CRL policy、路径／建链和 typed 完整验证结果、信任库／文件／
系统来源、身份匹配；之后继续 TLS 和联合验收。长资源作用域的前端成本
及清理 C 体积风险仍开放，必须用通用 IR 生命周期方案处理，不能增加
X.509 Cgen 补丁。长期任务保持 active；未决策 CRT 0／XRT 11，对象池最后。
