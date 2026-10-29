# siphash (legacy combined package; retained for migration reference only)

包版本 `1.0.0`，算法为 SipHash-2-4 的 64 位和 128 位变体。用于带密钥的短消息哈希和哈希表抗碰撞；不是密码存储、加密、签名或无密钥通用摘要接口。范例的公开固定密钥仅用于复现，真实用途需要保密的随机 16 字节密钥。128 位变体使用独立域分离，不能由两个 64 位摘要拼接得到。

## xlang API

`import siphash`。所有 `data` 和 `key` 各自支持 `string` / `Buffer`，四种组合均可使用。字符串长度按 UTF-8 字节计数，二进制和 NUL 建议用 Buffer。密钥必须恰好 **16 字节**，无填零、截断、默认密钥或回退。空数据合法。

| API | 返回值 / 合同 |
| --- | --- |
| `version()` | 包版本字符串 `1.0.0`，不是算法轮数 |
| `SIPHASH_KEY_LEN` | `16` |
| `hash64(data, key)` | `uint64`，不能用有符号 `int` 接收完整值域 |
| `hash64Hex(data, key)` | 16 个小写十六进制字符 |
| `hash64Bytes(data, key)` | 8 字节 Buffer |
| `hash128Hex(data, key)` | 32 个小写十六进制字符 |
| `hash128Bytes(data, key)` | 16 字节 Buffer |
| `hashFile64(path, key, maxInput=268435456, onProgress=null)` | `uint64` |
| `hashFile64Hex(...)` / `hashFile64Bytes(...)` | 文件摘要，string / Buffer |
| `hashFile128Hex(...)` / `hashFile128Bytes(...)` | 128 位文件摘要，string / Buffer |
| `hasher(key, maxInput=268435456)` | `SipHasher` |

文件函数的 `key` 同样支持 string / Buffer。`maxInput` 是输入字节预算，必须大于零，默认 256 MiB；不是超时控制。路径通过 XRT UTF-8 文件接口打开。hex/Buffer 均是**小端摘要字节序列**，不是直接把数值格式化为十六进制。

```xlang
import siphash
string key = "0123456789abcdef" // 仅用于范例
auto h = siphash.hasher(key)
h.update("你")
h.update("好")
assert h.inputBytes == uint64(6)
string tag = h.digest128Hex()
auto branch = h.clone()
branch.update("!")
h.reset()
```

`SipHasher` 提供 `update(string|Buffer)`、`digest64()`、`digest64Hex()`、`digest64Bytes()`、`digest128Hex()`、`digest128Bytes()`、`clone()`、`reset()`、`inputBytes` 和 `valid`。

- digest 是可重复的非终结快照，之后仍可 update；一个对象同时维护 64/128 两种状态。
- create 复制密钥；clone 创建独立状态，不共享可变数据。
- update 失败不部分接收数据，后续 update/digest/clone 失败直到 reset。reset 清空输入和失败状态，保留密钥及预算。
- digest 输出参数错误不使正常流失效。`valid` 仅表示句柄非空，不表示流未失败。
- 各对象可独立并发；同一可变对象由调用方串行化。对象销毁擦除其拥有的堆状态，但不承诺擦除所有调用方密钥、寄存器和临时副本。

## 进度与错误

回调签名为 `func(int current, int total, int output) as bool`，同步执行。每读取一块（最多 64 KiB）通知一次，EOF 额外通知，空文件也通知。current/total 是输入字节数，未知 total 为 0，output 恒为 0；最终 current 可能重复。底层 uint64 计数转为 xlang int 前检查范围。

返回 false 抛 `CancelledError`，最终通知也能取消；回调抛错保留原始类型和消息。回调可重入独立哈希操作。失败不提交部分摘要。文件不是并发修改快照，预算与取消也不能打断尚未返回的阻塞读取。

非法参数为 `ValueError`，预算/计数溢出为 `RangeError`，分配失败为 `MemoryError`，句柄错误为 `StateError`；文件和回调错误保留 XRT 类型。native adopt 必须接管有效真实句柄，不能伪造指针或重复接管。

## C API 与所有权

`include/xrt_siphash.h` 独立于 xlang/xruntime，提供：

```c
xrtSipHashOptionsInit / xrtSipHashErrorInit
xrtSipHash64 / xrtSipHash128
xrtSipHasherCreate / Update / Digest64 / Digest128 / Reset
xrtSipHasherInputBytes / Clone / Error / Destroy
xrtSipHashFile64 / xrtSipHashFile128
```

上表 Hasher 后缀都使用 `xrtSipHasher` 前缀。一次性/对象的 key 指针由调用方保证至少有 16 字节可读。输出空间分别要求 8/16 字节；核心 64 位输出为数值。**0 也可以是有效摘要**，C 用 `xrtGetError()` 区分失败。options 和 limits 严格验证 `iSize/iVersion`；NULL 使用 XRT 默认预算，C 显式零预算表示不设置业务上限，但仍检查累计溢出。

File 借用 Reader，从当前游标读，不回退也不销毁。可确定剩余长度时预检预算，否则逐块检查。返回时读取游标可能已推进；Reader 和回调原始错误优先于清理错误。

64 位计算直接复用 XRT 的 `xrtSipHash*`，不另造第二套。128 位核心在 `third_party/siphash.c`；来源与独立参考测试见 [PROVENANCE](third_party/PROVENANCE.md)。独立 C 程序可在一个翻译单元定义 `XRT_IMPLEMENTATION` 后包含本包头；已有宿主在首次包含 XRT 前启用所需模块，只编译一份 XRT。最小示例见 `test/test_siphash_standalone.c`。

`include/siphash_native.h` 是 xlang 的 43 个薄绑定入口：所有 `point` 为 `void*`、string 为 `const char*`、无符号整数为 `unsigned long long`，与生成声明严格匹配。hex 借用 XRT 临时区，后续正常调用不覆盖已有结果，清理/回退后失效；Buffer 为独立所有权，C 用 `xrtBufferDestroy` 释放，xlang 自动接管。错误描述借用 XRT 错误对象，其生命周期随错误对象。

## 范例与门禁

| xlang | C | 内容 |
| --- | --- | --- |
| `examples/extlibs/siphash_basic.xl` | `extlibs/siphash/examples/basic.c` | 一次性、UTF-8、二进制、流式快照、clone/reset |
| `examples/extlibs/siphash.xl` | `extlibs/siphash/examples/files.c` | 中文文件路径、进度、取消、异常、预算 |

在 demo6 目录运行：

```powershell
release/xl.exe run release/examples/extlibs/siphash_basic.xl
powershell -NoProfile -File tools/run_extlib_c.ps1 -Package siphash -Source release/extlibs/siphash/examples/basic.c
powershell -NoProfile -File tools/check_extlib_siphash.ps1
```

Linux C 门禁：`bash tools/check_extlib_siphash.sh`。Windows 门禁覆盖 GCC 严格编译、C/xlang JIT/AOT、全部 41 个生成声明与 43 个 native API；Linux 门禁覆盖 C 独立构建、ASan/UBSan/泄漏检查。二者冻结输入集合与哈希。

算法验证包括上游 128 个已知答案、4 种密钥 x 8 种地址偏移 x 81 种输入长度、短输入全切点、百万字节输入、空输入、快照、失败恢复。其他测试覆盖 Reader 当前游标/未知长度/原始错误、回调重入与最终取消、4 线程独立对象、192 个普通/临时区分配失败位置。范例摘要由固定版本上游参考实现产生，门禁独立复核，不由被测核心自产自验。

这些门禁不等于独立密码学审计、性能认证、Linux xlang、32 位/ARM、大端或同一对象并发验收。实际带日期执行结果以 demo6/docs 中的 SipHash 交付记录为准，未运行平台不视为通过。
