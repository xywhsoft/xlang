# 受管理 XDH 密钥

X25519／X448 复用 XRT 的 RFC 7748 实现。语言适配驻留于 runtime；
Cgen 只输出已有合同确定的声明、调用、私有字段接管和清理，不生成算法或 HEX 包装。

## 公开接口

| 接口 | 结果与合同 |
|---|---|
| `X25519Key(bytes privateKey)` | 完整 32 字节私钥；复制输入并缓存公钥 |
| `X448Key(bytes privateKey)` | 完整 56 字节私钥；复制输入并缓存公钥 |
| `crypto.x25519KeyPair()`／`crypto.x448KeyPair()` | OS 安全随机生成相应受管理密钥对象 |
| `key.publicKey` | 独立 bytes 公钥，属性只读 |
| `key.exportPrivate()` | 显式返回独立 bytes 私钥 |
| `key.sharedSecret(bytes peerPublicKey)` | 完整同曲线编码长度；返回拥有式 bytes，共享秘密校验由 XRT 执行 |
| `cryptoAdvanced.x25519(bytes scalar, bytes point)`／`cryptoAdvanced.x448(...)` | 显式高级原始标量乘法，保持原生解码／输出语义；不是 sharedSecret 校验接口 |

不存在默认构造、公开 native point、HEX 隐式解析、动态字典结果或失败空字符串。
旧 `crypto.x25519Keypair/x448Keypair` 和命名空间 `*SharedSecret` 入口退出默认标准库，
不保留兼容别名。普通调用应使用受管理对象的方法。

私钥和点编码不是消息，必须完整传入，因而不提供 size 截断。
导入保留输入标量编码，RFC 7748 所要求的标量处理由实际 XRT 运算完成。
构造后修改输入 bytes，或修改任何导出的 bytes，不改变对象。

## 效果、拥有权和清理

密钥对象不可变，无线程绑定，可在持有有效语言引用时并行读取。
resident leaf policy 的真实 payload 不拥有语言值、回调或生成代码地址；
在选中目录中按精确策略身份接纳，不能用 void* 形状推断拥有权。

导入、读取、导出、共享秘密和 raw 运算均可能因参数／分配失败而报 checked 错误；
读取／导出／计算为 pure + nonblocking，导入为 nonblocking。
随机生成不声明 pure 或 nonblocking，不回退到伪随机源。
析构为 no-raise + nonblocking，擦除私有 payload 并保留先前错误。

输出分配失败或原生运算失败时不交付部分 bytes，临时输出擦除后释放。
显式导出的私钥／共享秘密属于普通语言 bytes：本批不承诺其每一份副本都具有
锁页、禁止交换或安全擦除能力。不要将此适配等同于安全内存容器。

共享秘密是协议原料，不是已经带认证、上下文绑定的会话密钥。协议需要自行使用
合适的 KDF、认证和上下文；RFC 7748 第 6 节也明确讨论结合双方公钥派生密钥。
[原始规范与公开固定向量](https://www.rfc-editor.org/rfc/rfc7748.html#section-6)。

## 裁剪与验证范围

每条曲线分别拆分导入／方法、拥有目录、随机生成和高级 raw 的 resident 源。
导入固定密钥不拉入生成源或安全随机模块；raw-only 不拉入受管理密钥方法；
SHA256-only 不拉入两条曲线。共享 XRT 算法及底座不复制。

验收入口为 `tools/check_stdlib_xdh_source.py --source-build <build.json> --out <fresh-directory>`。
它使用当前源码驱动、实际 JIT／模块／独立 AOT、严格 C、完整原生选中闭包、
分配失败前缀、实际目录／leaf 拥有权、私有字段拒绝、参数-only 导出和 IR 合同拒绝。
仅离线公开固定数据和正常线程操作，不进行网络流量、恶意点／签名专项或安全审计。

正式结果和提交以
`F:/xlang-validation/five-groups-20261022/p4-xdh/verified-commit.json` 为准。
失败／准备轮保留但不计成功。此批不是完整 P4：Ed25519、ECDSA／ECDH、
RSA、其余密钥格式和高级接口、X.509、TLS 及 shipping 联合验收仍需继续。
