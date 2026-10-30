# 显式 ChaCha20 原语

本批修复实际复现的存量断链：旧 `crypto.chacha20Encrypt` 等 HEX／C-string
包装指向不存在的 `xl6_xrt_chacha20_*` 符号。未发布接口不保留兼容负担，改为
驻留声明；算法只使用固定 XRT 的 `xrtChaCha20`，不增加 Cgen 实现。

## 公开合同

```text
cryptoAdvanced.chacha20(bytes key, bytes nonce, string|bytes data,
                       int counter = 1, int size = -1) -> bytes
cryptoAdvanced.chacha20Buffer(bytes key, bytes nonce, Buffer data,
                             int counter = 1, int size = -1) -> bytes
```

这是原始 IETF ChaCha20 异或操作，同一函数用于两个方向；不是认证加密。
一般应用使用已提供的 `ChaCha20Poly1305`。本接口不会管理 nonce／计数器：
同一 key 下不能重复使用相同的密钥流位置，也不能将结果当作完整性校验。

- key 必须恰为 32 字节，nonce 必须恰为 12 字节；均使用完整 bytes，不受
  消息 size 控制，不允许文本／HEX 密钥或猜测编码。
- 消息使用实际字节长度，保留 UTF-8 和内嵌 NUL；size=-1 使用全部，非负
  size 截断到实际长度，小于 -1 报范围错误。结果是独立拥有的 bytes。
- counter 为 0..UINT32_MAX，允许最后一个合法块；所需块数超过剩余计数器
  空间时，在分配前拒绝，不先缩窄为 uint32，也不允许回绕。
- 同步只读输入不逃逸，Buffer 不修改、不创建临时 Buffer；没有新增语言
  引用／借用机制。编译器已有的内部 cbytes 路径避免不必要的 bytes 装箱。
- string／bytes 路径为 pure、nonblocking；Buffer 路径只声明 nonblocking。
  所有入口可能分配／报错，均不声明 no-raise；Buffer 的物理 owner 合同
  精确指定既有 `xl6BufferDestroyV1`。
- 原生失败或结果发布失败不交付半成品，临时输出擦除并释放，保留原诊断。

六个 ABI 入口（文本、bytes、Buffer 及各自 span 版本）共同使用同一材料、
长度和计数器实现。独立组件只选择 CRYPTO_CHACHA20 和 VALUE，不能因为使用
原语就拉入 AEAD、其他密码算法、DER、X.509 或 TLS。

旧 chacha20Encrypt／chacha20Decrypt／chacha20Bytes 及其三个内部桥接注册、
语言包装体和依赖条目全部移除。公开替代仍为三个重载登记，内部总登记数
由 1028 减为 1025，生成索引由既有生成器更新。

## 本批验收状态

原生 Windows GCC O0／O2／TCC 和 Linux GCC O0／O2／ASan+UBSan 已通过。
每配置六个入口共 48 分配前缀，12 次真实失败／36 次完成，内存账本平衡；
另覆盖最后计数器的合法块与越界拒绝。原生阶段使用新驻留单元与已有正常
运行库，不能据此宣称新普通产品完成。

最终 1,274 TU 完整源码 cohort 的 23 阶段通过：声明／IR、固定正常向量、
NUL／长度／参数、JIT／module／无 xl DLL 依赖的独立 AOT、严格生成 C O0／O2
与组件裁剪。实际 IR 拒绝 76 处合同变异；SHA256 和正常 AEAD 共存另有
4 阶段回归。原始固定向量逐字节与本地 OpenSSL 独立实现核对。
入口为
`tools/check_stdlib_crypto_chacha20_source.py`；证据根为
`F:/xlang-validation/five-groups-20261026/p7-tls-close/`。
独立回执为 `verified-chacha.json`，绑定全部输入、对象、日志与产物哈希；
Linux 原生证据有独立 reader 和归档核对。首轮因遗漏生成索引前置而主动
停止，向量抄写修正及负例诊断断言修正的失败记录均保留，未计为通过。
最终源门显式绑定修正后的测试输入，不篡改早期冻结快照中的测试文件。

本批不是密码学安全审计，不进行公网通信、协议攻击或畸形密码学专项；
五组逐项封板和最终普通产品联合验收仍独立，未决策范围不扩张。
