# md5

16 字节 MD5 摘要，支持 UTF-8 文本、Buffer、文件和增量输入。算法直接复用 XRT
的 `xrtMd5/Init/Update/Final`，包只负责预算、Reader、回调及语言绑定，不重复实现
摘要核心，不需要外部 DLL。MD5 用于既有格式兼容，不用于密码、认证或防恶意篡改。

## 快速使用

```xlang
import md5

assert md5.hashHex("abc") == "900150983cd24fb0d6963f7d28e17f72"
auto h = md5.hasher()
h.update("a")
h.update("bc")
assert h.digestHex() == md5.hashHex("abc")
assert h.inputBytes == uint64(3)
```

## 公开函数

| 函数 | 参数与结果 |
| --- | --- |
| `version()` | 包引擎版本 `1.0.0` |
| `hashHex(Buffer|string data) as string` | 32 个小写十六进制字符 |
| `hash(Buffer|string data) as Buffer` | 16 字节原始摘要 |
| `hashFileHex(string path, int maxInput=268435456, (func(int,int,int) as bool)? onProgress=null) as string` | 文件摘要，返回 hex |
| `hashFile(...) as Buffer` | 相同参数，返回 16 字节 Buffer |
| `hasher(int maxInput=268435456) as Md5Hasher` | 增量摘要对象 |
| `MD5_DIGEST_SIZE` | 常量 16 |

`Buffer|string` 表示两个重载。空输入合法；文本按 UTF-8 字节数计算，不另建 Buffer。
语言 string 是 C 风格文本，包含 NUL 的二进制输入应使用 Buffer。一次性函数不另设
业务预算；错误通过异常报告，不能把空字符串或空 Buffer 当作成功摘要。

## 增量对象

- `update(Buffer)`、`update(string)`：追加输入，不返回摘要。
- `digest() as Buffer`、`digestHex() as string`：非终结快照，可重复读取，之后仍可追加。
- `reset() as bool`：清空输入和失败状态，保留预算。
- `inputBytes as uint64`：成功接收的字节数。
- `valid as bool`：句柄非空，不代表对象没有发生过 update 错误。

超限或非法 C 输入使对象进入失败状态，后续 update/digest 保留该错误，直到 reset。
失败 update 不部分接收、不增加计数；即便不设业务上限，也检查 MD5 位计数范围。
建议通过 `hasher()` 创建。底层构造及 `*Native` 用于受控的 C 互操作，不接受任意
指针。原生对象自动析构，不隐式复制资源；独立对象可以并发，同一可变对象必须串行。

## 文件与回调

```xlang
string digest = md5.hashFileHex("data.bin",
    onProgress = func(int current, int total, int output) as bool
        return true
    end)
```

三个参数是已读字节、总长度、输出字节。未知总长为零，输出恒为零。每块最多读取
64 KiB，EOF 另通知一次，空文件也通知；最后两次的字节计数可能相同。XRT 的 uint64
进度转为语言 int 前检查范围，不静默截断。

- 回调返回 false 抛 `CancelledError`，最终通知也可以取消，不交付部分摘要。
- 回调抛错保留原类别、消息；允许重入另一独立文件摘要操作。
- maxInput 必须为正，非法参数为 `ValueError`，超限为 `RangeError`。
- 分配失败为 `MemoryError`；文件打开/读取错误保留 XRT 的原分类。
- 使用 XRT UTF-8 文件接口，关闭自己打开的 Reader，不修改输入文件。
- 预算不是 CPU 时限、峰值内存限制或并发写入下的文件快照保证。

## C API

[include/xrt_md5.h](include/xrt_md5.h) 公开 OptionsInit/ErrorInit、Hash、
HasherCreate/Update/Digest/Reset/InputBytes/Error/Destroy 及 File，前缀均为 `xrtMd5`。
失败返回 false/NULL 并设置 XRT 错误；可选 error 输出保存偏移和说明。
诊断查询与 Destroy 不用于清除错误；调用方可显式调用 xrtClearError。

纯 C 程序在唯一运行库实现单元定义 `XRT_IMPLEMENTATION` 后包含公开头，另编译
`xrt_md5.c`；头文件选择 CRYPTO_MD5 和 IO_BUFFER。已有宿主 XRT 时直接链接它，
不要再生成一份运行库。参见 [独立 C 集成测试](test/test_md5_standalone.c)。

`xrtMd5File` 借用 Reader，从当前游标到 EOF，不关闭、不回退。可寻址流用剩余长度
作为 total，已知超限在读取前拒绝；未知流可能消费失败块，不保证回滚游标。只有
最终回调也成功后才写摘要，C 进度帧保留 FINAL/TOTAL_KNOWN 标志及 uint64 计数。

NULL 配置采用 XRT 默认限制；初始化后的核心配置中 `iMaxInputBytes=0` 表示不设
业务上限。Options/Limits 的 size/version 必须匹配。薄绑定 maximum=0 则表示
默认预算。C 输入只在调用期间借用，显式长度允许包含零字节。

[include/md5_native.h](include/md5_native.h) 提供与实际生成 C 一致的薄绑定原型：
point 对应 void*，string 对应 const char*，长度为 size_t，预算为 unsigned long long。
hex 借用 XRT 临时区，在该区清理/回退后失效，后续摘要调用不会覆盖此前文本。
返回的二进制 xbuffer* 归调用方所有，C 端用 xrtBufferDestroy 释放，语言端由 Buffer
接管。不使用私有全局错误槽、循环文本槽或独立文件状态码。

## 范例与验证

- [教学范例](../../examples/extlibs/md5.xl)。
- [xlang 对照范例](../../examples/extlibs/md5_basic.xl)、[C 对照范例](examples/basic.c)、
  [相同固定输出](../../examples/extlibs/md5_basic.expected.txt)。
- [核心测试](test/test_md5.c)：149 组独立生成答案，每组 8 种地址偏移；覆盖分块、
  填充边界、RFC 常用答案、百万 a、位计数溢出、参数/版本、Reader 与取消/异常。
- [原生测试](test/test_md5_native.c)：四线程独立对象、临时结果寿命、错误隔离、
  Buffer 所有权、普通分配和临时区故障、失败后恢复与资源计数。
- `tools/check_extlib_md5.ps1`：严格 GCC、全部 15 个语言 extern 生成原型、C/xlang
  JIT/AOT 及范例固定输出。`tools/check_extlib_md5.sh`：Linux 独立 C ASan/UBSan/泄漏检查。

门禁检查执行前后的输入集合与哈希，结果只属于对应快照。未声称 Linux xlang、
真实大端目标、32 位/ARM、同一可变对象并发或性能基准已验收。
