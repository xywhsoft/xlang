# murmur3

MurmurHash3 x86_32 与 x64_128，适合非对抗性哈希表、分桶和指纹。
它不是密码学摘要，不用于密码存储、签名、认证或抵抗恶意碰撞。
两种算法在所有平台都保持各自结果，不根据当前 CPU 位数切换变体。

## xlang API

```xlang
import murmur3

assert murmur3.hash32("abc") == 3017643002
assert murmur3.hash32Hex("abc") == "fa93ddb3"
assert murmur3.hash128Hex("abc", 42) == "d6f7cfb39f08850d303d35422b711075"

auto h = murmur3.hasher(seed = 42, maxInput = 1024)
h.update("a")
auto branch = h.clone()
h.update("bc")
assert h.digestHex() == murmur3.hash128Hex("abc", 42)
assert h.inputBytes == uint64(3)
```

| 函数 | 返回值与参数 |
| --- | --- |
| `version()` | 引擎标识 `3.0.0`；不是包清单版本 `1.0.0` |
| `hash32(data, seed=0)` | x86_32 的无符号值，放在语言 int 中，范围 0..4294967295 |
| `hash32Hex(data, seed=0)` | 4 字节摘要的小写 hex，共 8 字符 |
| `hash32Bytes(data, seed=0)` | 4 字节 Buffer |
| `hash128Hex(data, seed=0)` | x64_128 的小写 hex，共 32 字符 |
| `hash128Bytes(data, seed=0)` | 16 字节 Buffer |
| `hashFile32(path, seed=0, maxInput=268435456, onProgress=null)` | 文件的 x86_32 整数 |
| `hashFile128Hex(path, seed=0, maxInput=268435456, onProgress=null)` | 文件的 x64_128 hex |
| `hasher(seed=0, maxInput=268435456)` | 独立的 `Murmur3Hasher`，使用 x64_128 |

前五个摘要函数的 data 均支持 string 和 Buffer。文本按 UTF-8 **字节**计算；
任意二进制和 NUL 使用 Buffer。空输入合法，零也是合法的 32 位摘要。
seed 在窄化前检查为 0..4294967295；公开 maxInput 必须为正。
一次性函数处理已经提供的内存，没有额外 maxInput 参数。

摘要 bytes/hex 统一使用小端序：hash32("abc") 的数值为 0xb3dd93fa，
其四字节摘要依次为 fa 93 dd b3，因此 hash32Hex 返回 fa93ddb3，而不是整数格式化。
128 位输出是两个 64 位状态按小端序依次写出，不是 x86_128 算法。

## 增量对象

- `update(string|Buffer)` 追加输入，空输入合法；累计输入受预算和 uint64 溢出检查约束。
- `digestHex()` / `digestBytes()` 是非终结快照，可重复读取，之后仍可 update。
- `clone()` 显式创建独立状态，保留 seed、输入和预算；不共享可变算法状态。
- `reset()` 清空输入及失败状态，保留 seed 和预算，成功返回 true。
- `inputBytes` 为 uint64；`valid` 只表示句柄非空，不代表对象没有失败。

非法输入或超限 update 不部分接收，之后的 update/digest/clone 继续报告原失败，
直到 reset。错误输出指针不会破坏可用的哈希状态。析构自动释放对象。
独立对象可由独立线程使用；同一可变对象要求调用方串行，不内置锁。
底层 Native 方法及 point 构造属于 C 桥接表面，不接受任意地址；point 构造接管
由本库 Create/Clone 产生的唯一拥有句柄，不能重复接管或自行释放该句柄。

## 文件、回调与错误

```xlang
string digest = murmur3.hashFile128Hex("data.bin", seed = 42,
    onProgress = func(int current, int total, int output) as bool
        return true
    end)
```

回调同步调用，三个参数为已读字节、总字节、输出字节，output 恒为 0。
每块最多 64 KiB，EOF 另通知一次，空文件也通知；最后两次计数可以相同。
未知总长为 0。C 的 uint64 计数转为语言 int 前检查范围，不静默截断。

返回 false 抛 CancelledError，包括最终通知；回调抛错保留类别与消息。
回调可以执行独立嵌套哈希、文件调用，也可以捕获嵌套错误继续当前操作。
回调不得销毁当前正在使用的 Reader 或同一哈希对象。

输入参数错误为 ValueError，超限为 RangeError，分配失败为 MemoryError；
无效句柄的 XRT STATE 在语言侧映射 RuntimeError。文件错误保留 XRT 的类别，
例如缺失文件由 FileError 捕获。异常不能被空字符串、零摘要或局部 file_status 隐藏。
路径使用 XRT UTF-8 文件能力。路径包装自行关闭 Reader，保留读取或回调的原错误。

预算只限制接收的输入字节，不是 CPU 时限、并发写入时的文件快照或所有资源限制。
文件变动可能使最终进度与最初估计总长不同。未知长度 Reader 为检测超限可能已经
读走当前块，失败不回退游标；不向调用方交付部分摘要。

## 独立 C 核心

`include/xrt_murmur3.h` 提供 OptionsInit/ErrorInit、Hash32/Hash128、
HasherCreate/Update/Digest/Clone/Reset/InputBytes/Error/Destroy、File32/File128。
核心只依赖 XRT，不依赖 xlang 或 xruntime。

- 在一个翻译单元中先定义 XRT_IMPLEMENTATION，再包含该公共头，编译
  `third_party/MurmurHash3.c` 和 `xrt_murmur3.c` 即可；已有宿主 XRT 时复用唯一实现。
- `OptionsInit` 和 `xrtResourceLimitsInit` 建立当前版本结构，size/version 必须匹配。
  NULL options/limits 使用默认预算；显式 `iMaxInputBytes=0` 表示不设业务上限，
  仍检查累计字节溢出。本库只消费 limits 的输入字节预算。
- File32/File128 **借用 Reader**，从当前游标读取到 EOF，不关闭、不回退。
  支持可定位和未知长度 Reader；已知剩余长度超限时不开始读取。
- 最终回调接受以后才提交输出；Hash128/File128 失败不改写输出区。
  Hash32/File32 的零不是错误标志，必须检查 XRT error 或可选的详情对象。
- 当前错误是权威类别与消息，详情对象补充操作码和输入偏移；回调的自定义错误
  不被取消消息覆盖，Reader 的内存/IO 错误也保留。
- C 进度帧保留 uint64 与 TOTAL_KNOWN/FINAL 标志，可区分未知总长与已知空输入。

`include/murmur3_native.h` 是精确的语言薄绑定 ABI。hex 返回借用的 XRT 临时区，
后续摘要调用不覆盖它，但清理/回退/销毁临时区后失效。需要长期持有的 C 调用方
应复制文本。bytes 返回拥有的 xbuffer*，C 用 xrtBufferDestroy，xlang 由 Buffer 接管。
native Create/File 的 maximum=0 表示使用默认值，不是核心 limits 的无限预算写法。
last_error 只借用当前错误消息，不把 xerror* 当字符串，也不另设全局错误槽。

## 范例与验收

- `examples/extlibs/murmur3_basic.xl` 与本包 `examples/basic.c`：seed、整数/hex、
  UTF-8、二进制、分块、clone、reset；输出与固定 expected.txt 一致。
- `examples/extlibs/murmur3.xl` 与本包 `examples/files.c`：中文路径、文件、进度、
  取消、回调错误和输入预算；自行创建测试目录并清理文件，双语输出一致。
- `tools/check_extlib_murmur3.ps1`：严格 GCC、全部 23 个生成原型、JIT/AOT、
  C 独立使用、两个双语范例、状态/Reader、回调及内存边界。
- `tools/check_extlib_murmur3.sh`：Linux 独立 C 的 ASan/UBSan/泄漏检查及两个范例。

核心有 576 组上游独立答案，每组 8 个地址偏移，另测所有短消息切点、跨 64 KiB、
uint64 累计溢出、未知长度预算及 Reader 元数据失败。原生层覆盖全部 25 个入口、
四个独立线程、借用文本寿命以及 10 条路径各 12 个普通堆/临时区分配故障位置。
测试记录输入文件集合和哈希；脚本存在不等于已经通过，实际结果见带日期的报告。

这些检查不代替 Linux xlang、大端实际机器、32 位/ARM、同一可变对象并发或吞吐
基准。上游锁定、参考答案来源与重建方式见 `third_party/README.md`。
