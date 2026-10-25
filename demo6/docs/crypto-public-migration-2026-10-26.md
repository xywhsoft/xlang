# 密码学旧综合用例迁移与普通产品验收

## 本批修改

迁移 `test/cases/stdlib_crypto.xl`、xlang／C 两份 `stdlib_crypto` 示例及两份
BAT 的密码学片段，不改运行时、Cgen、XRT 快照或时间 ABI。旧程序仅作编译
对照，确实在退休的标准库名字处拒绝，没有执行其中的旧安全负例。

- 默认摘要／HMAC／KDF 使用 bytes；HEX 显式命名并使用小写，SHA1 归入
  cryptoLegacy。Buffer 使用明确入口，流式结果使用 final／hex。
- AEAD 使用受管理 AesGcm／ChaCha20Poly1305，密钥、nonce、AAD 都是完整
  bytes；原始 ChaCha20 使用 cryptoAdvanced，不经 HEX 或裸指针桥接。
- 密钥对使用拥有型对象与明确字节导入／导出，不再按字典读取 HEX 私钥。
  正常 Ed25519 和 P256／P384 签名验证替换示例中的畸形输入 guard。
- 消息和二进制使用不同 nonce，raw 原语另用独立 nonce。C 对照中二进制
  解密改为实际输入长度，不读取未初始化的密钥流缓冲尾部。
- 保留已有的 `crypto.randomBytes(17).length` 修正。BAT 只迁移密码学片段；
  其他 CRT／字符串、网络及工作区修改保留，不借此宣称整套 BAT 通过。

## 普通产品，而不是源码替代

冻结 10,414 个输入后，从正常 build.bat／build_linux.sh 分别重建 TCC、XRT、
CLI、运行库、编译器库和两套 VFS。没有借用旧产品、替换编译对象、测试用
运行时或源码搜索回退，也没有覆盖仓库 release 二进制。

原构建曾被新合并的 XACME 原生引用分类阻断；必要前置已独立提交，见
[扩展 API 归属](build-external-api-ownership.md)。核心 API 数量和严格未知
引用检查保持不变；本批没有运行 XACME 功能。

Windows／Linux 各 33 个产品阶段通过，使用 5 正例／2 编译期负例：

- 真实公共宿主执行加载、typed／lowered IR、重复 C／头输出、JIT、模块、
  正常 AOT 和 checked 退役；隔离 CLI 没有源码部署。
- 生成 C 经严格 GCC O0／O2 检查；AOT 移至独立目录运行，实际依赖中没有
  xl DLL／SO。驻留密码学函数只声明／调用，不由 Cgen 生成实现体。
- SHA256-only 与 ChaCha20-only 实际 AOT 消费的保存版 XRT 实现经预处理
  核对，只含对应算法及 crypto_core，不引入其他密码算法、X.509 或 TLS。
- 两端 C 示例各三配置通过并与 xlang 的 12 项正常结果逐行一致：Windows
  GCC O0／O2／TCC，Linux GCC O0／O2／ASan+UBSan。
- 新 ChaCha20 六个 ABI 入口另直接连接两端实际运行库／编译器库，不另编
  实现：每库 48 个分配前缀，四库共 192 个，48 次真实失败／144 次完成均
  平衡，并核对计数器末块边界。
- Windows 实际提取的两个 BAT 片段各有正常通过及必要失败对照：旧包装
  哨兵、缺失 signature 结果分别被原断言拒绝。不等于完整 08／09 suite。

公共门沿用 `check_network_public_shipping.py` 的真实公共宿主，显式指定
`tools/crypto_public_fixtures.json`；默认网络清单及其断言不变。静态扩展
归属检查两端各 14 项，没有执行扩展 API 或发起网络连接。

## 证据与边界

证据根 `F:/xlang-validation/five-groups-20261026/p7-crypto-migration/`，最终为
`verified-products-windows.json`、`verified-products-linux.json`。输入、产品、
日志、真实计数及生成行尾变化独立复核；Linux 原件保留，并归档镜像全部
11,135 个证据文件。首轮输入变化、前置拒绝和成功轮次分开保存。

这是正常库接入、ABI／生命周期与裁剪验收，不是密码学或网络安全专项。
普通产品中的新原语及综合用例已验收，但五组逐模块矩阵、其余旧调用、
嵌套编译／IDE 和最终联合封板仍须接续。只读搜索另发现五个
`release/projects/dev` 草案仍引用旧 crypto API，不能把这些应用视为已经迁移。
不扩张 CRT 0／XRT 11 的未决策范围，内存池仍最后。
