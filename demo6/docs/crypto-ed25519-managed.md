# 受管理 Ed25519 密钥

2026-10-22：已批准 P4 的 Ed25519 增量。只复用固定 XRT 的算法；适配驻留
runtime，XLIR 持有闭合 ABI／效果／拥有权合同，Cgen 不生成算法或包装实现。
本批不代表其余非对称密钥、X.509、TLS 或五组发布验收已经完成。

## 公开入口

`Ed25519Key(bytes seed)` 导入完整 32 字节原始 seed，复制并缓存原生展开密钥。
它不是 64 字节的其他库私钥序列化格式；DER／PEM 也不能传给此构造函数。
`crypto.ed25519KeyPair()` 使用 OS 安全随机源创建同一受管理类型。

| 入口 | 结果与合同 |
|---|---|
| `key.publicKey` | 独立的 32 字节 bytes 公钥，只读属性 |
| `key.exportSeed()` | 显式导出独立的 32 字节原始 seed |
| `key.sign(message, size = -1)` | string／bytes 消息，返回 64 字节原始签名 |
| `key.signBuffer(message, size = -1)` | 直接读取 Buffer，不创建临时 Buffer／文本 |
| `key.signContext(message, context, size = -1)` | Ed25519ctx，消息 string／bytes，context 为 bytes |
| `key.signContextBuffer(message, context, size = -1)` | Ed25519ctx 的 Buffer 消息入口 |
| `key.signPrehash(digest, context = b"")` | Ed25519ph，digest 必须是完整 64 字节 SHA512 摘要 |
| `crypto.ed25519PublicKey(seed)` | 从完整 seed 派生 bytes 公钥，不创建受管理密钥对象 |
| `crypto.ed25519Sign(seed, message, size = -1)` | 一次性普通 Ed25519 签名，消息 string／bytes |
| `crypto.ed25519SignBuffer(seed, message, size = -1)` | 一次性普通签名的 Buffer 入口 |
| `crypto.ed25519Verify(publicKey, message, signature, size = -1)` | 普通 Ed25519 验证，消息 string／bytes，返回 bool |
| `crypto.ed25519VerifyBuffer(publicKey, message, signature, size = -1)` | 普通验证的 Buffer 入口 |
| `crypto.ed25519VerifyContext(publicKey, message, signature, context, size = -1)` | Ed25519ctx 验证，消息 string／bytes |
| `crypto.ed25519VerifyContextBuffer(publicKey, message, signature, context, size = -1)` | Ed25519ctx 的 Buffer 验证入口 |
| `crypto.ed25519VerifyPrehash(publicKey, digest, signature, context = b"")` | Ed25519ph 验证，完整 64 字节摘要 |

普通、context、prehash 采用不同名称，不公开可混淆的模式整数。
Ed25519ph 有自己的域分离规则，不等于将摘要传给普通 `sign`；摘要由调用者
显式计算，运行时不会再对传入摘要隐式做一次 SHA512。[RFC 8032 §5.1](https://www.rfc-editor.org/rfc/rfc8032.html#section-5.1)

```xlang
auto key = crypto.ed25519KeyPair()
bytes publicKey = key.publicKey
bytes signature = key.sign("a\0你")
assert crypto.ed25519Verify(publicKey, "a\0你", signature)

bytes digest = crypto.sha512("large message")
bytes phSignature = key.signPrehash(digest)
assert crypto.ed25519VerifyPrehash(publicKey, digest, phSignature)
```

## 长度、错误与效果

- 消息 `size=-1` 取完整实际字节长度；非负值取实际长度与 size 的较小值。
  小于 -1 报范围错误。UTF-8 也按字节截断，内嵌 NUL 不截断消息。
- seed／公钥／签名／摘要必须分别为 32／32／64／64 字节；不接受字符串 HEX，
  不进行 size 截断。context 为完整 0—255 字节 bytes，超过上限报范围错误。
- 参数、分配、状态或其他原生错误仍 checked 报错，失败不交付部分签名。
  只有原生验证报告的 `xrt.crypto` 域与精确 kind／code 组合
  `XERR_VALUE/XCRYPTO_ERROR_KEY`、`XERR_PROTOCOL/XCRYPTO_ERROR_SIGNATURE` 转为 false。
  不匹配诊断文字或 Operation，不把 OOM、未知错误等吞成 false。
- 调用原生验证前暂存已有错误，结束后保留第一项错误；清理不覆盖已有错误。
  成功结果若异常伴随原生错误，也不将该错误静默清除。
- 普通只读消息入口声明 pure＋nonblocking，但可能报错，不声明 no-raise。
  Buffer 入口不声明 pure；生成入口可能等待 OS 随机源，不声明 nonblocking。
  析构 no-raise＋nonblocking，只释放并擦除私有密钥数据。

## 拥有权与裁剪

密钥是不可变 resident leaf，不持有语言值、回调、生成代码地址或外部可变指针。
生命周期由语言 native handle／拥有目录统一管理；对象存活期间可跨线程只读。
Buffer 消息本身仍可变，调用者须避免并发写入。

导入时只展开一次；生成时先读 OS 随机 seed，再展开一次，不先 KeyPair 再
KeyInit 重复计算公钥。析构清除原生展开状态并擦除整个私有 payload。
`exportSeed()` 返回普通 bytes，不承诺锁页、安全内存或消除语言副本；需要导出
私钥的调用者仍负责保管这些副本。

核心／拥有目录／随机生成／受管理签名／一次性签名／公钥派生／验证按源单元
拆分，Buffer 薄适配也独立。只导入密钥不拉入随机源、签名、验证或 Buffer；
只求公钥、只验证、一次性签名不拉入受管理对象底座；仅有参数类型与 sign 方法
的导出也不要求本地构造函数。普通 SHA256、XDH 不引入本批适配。

旧 `ed25519Keypair()` 的动态字典与 HEX 形态删除，无兼容别名；入口使用
`KeyPair` 的规范大小写。综合旧测试仅迁移 Ed25519 小节，不据此声称整套
旧密码学测试已经验收。

## 验收与证据边界

专项为 `tools/check_stdlib_ed25519_source.py`。11 个语言用例包括 7 个 Ed25519
用例及 4 个 XDH／SHA256 回归，覆盖 JIT、module、独立 AOT、严格 GCC O0／O2、
实际选中闭包与裁剪；另有 8 个静态错误拒绝和索引计数过期门禁。
固定公开向量来自 [RFC 8032 §7.1—7.3](https://www.rfc-editor.org/rfc/rfc8032.html#section-7)，
另验 NUL／UTF-8／bytes／Buffer 等价、字节前缀、生成与复制隔离、普通不同消息
验证失败。没有畸形点／签名攻击样本或网络安全专项。

23 个实际调用目标的 251 项错误合同在完整 module／C／声明头输出之前拒绝。
选中原生闭包以 GCC O0／O2、TCC 直接编译链接；Linux 另加 ASan／UBSan。
每种模式覆盖 51 个分配前缀、27 次实际触发失败、4 个真实 leaf／目录拥有图、
11 类 typed 错误边界，核查失败无部分输出和分配账本平衡。优化级别重复运行
不计成不同 API 或不同错误合同。

正式结果与源输入、日志、产物 SHA 以本批完成回执为准：
`F:/xlang-validation/five-groups-20261022/p4-ed25519/verified-commit.json`。
最终 Windows 147／Linux 149 阶段通过，共 296 阶段；完整源码重建分别为
849／848 个编译单元，不混计验收阶段。正式轮次为 `source-win-r1`／
`accept-win-r3` 与 `source-linux-r1`／`accept-linux-r1`。
该目录保留失败／准备轮，不混入成功统计；Linux 原始证据与 F 盘镜像同时保留。
这是当前源码产品验收，不更新发布 DLL／SO／VFS，不等于 shipping 或五组联合验收。
