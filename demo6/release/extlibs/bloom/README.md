# bloom

全局位数组 Bloom 与 4-bit Counting Bloom，复用 XRT 的内存、hash64、临时区和错误机制。
对象没有内部锁；同一对象的修改与读取由调用方串行化。

```xlang
import bloom

auto filter = bloom.create(1000)
filter.addAll(["alpha", "beta"])
assert filter.contains("alpha")
auto restored = bloom.deserialize(filter.serialize())
assert restored.contains("beta")
```

## API

| API | 合同 |
| --- | --- |
| `create(capacity, fpRate=0.01)` / `createCounting(...)` | capacity 为正的预计元素数，不是强制容量；概率必须严格位于 (0,1) |
| `add(text)` / `addAll(array<string>)` | 添加；标准版 count 累计本对象 add 调用次数，不是去重计数 |
| `contains(text)` / `containsAll(...)` / `containsAny(...)` | 可能存在，不可用于精确授权或当作数据库真实记录 |
| `clear()` | 清零筛选状态和计数 |
| `clone()` | 独立拥有的完整副本 |
| `merge(other)` | 相同维度和探针数才能合并，否则 false 且不修改；不转移对象、不累加 count |
| `serialize()` / `deserialize(text)` | hex 编码的筛选状态；无效头、尺寸、版本及截断抛 ValueError |
| `deserializeCounting(text)` | 只接受计数版格式，不与标准版互读 |
| `estimatedCount()` / `estimatedFpp()` | 基于填充率的估计，不是精确基数或实测误判保证 |
| `valid` / `capacity` / `count` / `hashes` | 状态；标准版另有 bits，计数版另有 slots |
| Counting `remove(text)` | 只应用于确定添加过且尚未删除的项目，不能仅凭 contains 判断能否删除 |

## 必须理解的限制

- 标准版的成员筛选没有删除路径，未清空时不漏掉已添加项；允许误报。
- 计数版单槽最大 15，超过后饱和；当前 remove 不能恢复溢出的次数。
  删除未添加项、饱和后的删除、合并后的独立删除都可能影响其他项目，不能承诺无条件“不漏判”。
- Counting merge 取逐槽最大值，不是两个多重集合计数相加；count 仍为本对象本地
  add/remove 统计，不能用它推导合并后的真实基数。
- 概率参数为 NaN、无穷、0、1 或超出范围时立即拒绝，不悄悄替换为 0.01。
  容量无法表示时抛 RangeError，实际分配失败为 MemoryError。反序列化先对比实际
  长度与声明尺寸，再分配；内存失败不能被改报为数据损坏。
- XLang `add`、`contains`、`remove` 及批量操作按 UTF-8 字节长度传递字符串，
  包括内嵌 NUL；C 端传 `-1` 才按 NUL 结尾字符串处理，显式长度入口可处理二进制键。
  序列化状态是十六进制文本，XLang 反序列化会拒绝内嵌 NUL，不能只接受合法前缀。
- 普通版序列化为版本 2，全局位探针；旧分块版版本 1 明确拒绝，不作兼容读取。计数版仍为版本 1。筛选状态不是永久跨版本数据库格式。
  本轮运行验证仅覆盖小端 Windows/Linux x86-64，不宣称所有平台间二进制兼容。

## C 与验证

公开头 `include/bloom.h`；创建、恢复、clone 的句柄由 C 调用方各释放一次，
`destroy(NULL)` 安全。不能重复释放同一非空句柄。`serialize` 字符串借用 XRT 临时区，
长期保存请复制；`serialize_owned` 返回独立的十六进制文本内存，调用方使用
`xrtFree` 释放，失败时返回 NULL 并设置 XRT 错误。XLang 的 `serialize()` 直接接管
拥有型结果，不再从临时区复制整串。单独编译 `bloom_native.c` 并链接一份 XRT
即可，不依赖编译器内部类型。

`examples/extlibs/bloom.xl` 覆盖语言 API；`bloom_basic.xl` 与 `bloom_c.c` 是四行
同结果范例。专项门禁 `tools/check_extlib_argon2_bloom.ps1` 校验所有生成原型、
C/xlang JIT/AOT、序列化破坏、尺寸、分配故障及内嵌 NUL 键。
2026-10-26 当前 Windows 门禁报告 `passed`、47 项检查；Linux
C ASan/UBSan、中央和范例 JIT/AOT 及 NUL 键 JIT/AOT 均通过。
旧 `xl6ValueDataSnapshotV1` 缺符号只描述早期发布物。
结果以每次保存的哈希与输出为准。
固定键集的普通版与计数版误判率、无漏判和旧格式拒绝已进入 C 回归；
普通版的 512 位分块探针在目标 1%/0.1%/0.01% 时分别测到约 1.3%/0.3%/0.09%，
故改用全局位探针。样本测试不构成对任意键分布的概率上界；计数器饱和仍按上述约束处理。

独立性能基准 `test/bench_bloom.c` 在固定 10 万个短键上各重复 100 轮；
插入轮次之间清空筛选器，清空与键生成不计入耗时，命中和未命中查询
对填充后的筛选器重复采样。它只输出本机吞吐率和误判数，不设机器相关
的通过阈值，也不进入日常回归门禁。可在 `demo6` 目录运行：

```sh
mkdir -p build/test
gcc -std=c11 -O2 -Ilib/xrt -Irelease/include \
  release/extlibs/bloom/test/bench_bloom.c \
  release/extlibs/bloom/bloom_native.c \
  test/support/extlib_scalar_runtime.c -lm -o build/test/bloom_bench
build/test/bloom_bench
```

Windows GCC 链接时另加 `-lWs2_32 -lAdvapi32 -lIphlpapi`，输出可改为
`build/test/bloom_bench.exe`。该基准不测并发共享、磁盘序列化或跨机器
性能，亦不能替代完整 AOT 验收。

当前包清单仅声明直接使用的 `XRT_MODULE_HASH64`；同一份
`test/aot_core.xl` 在 Windows 下由 3,635,200 缩至 660,992 字节，
构建和运行通过。可从 `demo6` 运行
`xl build release/extlibs/bloom/test/aot_core.xl -o build/test/bloom_aot_core.exe`
再执行产物。当前完整 `test/cases/extlib_bloom.xl` 的 AOT 已进入
双平台联合门禁，不再以这个最小核心用例代替完整验收。

2026-10-20：包内 `test/nul_keys.xl` 先复现 XLang 把 `"a\0b"` 键截成
`"a"`，再固定普通/计数版的单条与批量增查、计数版删除，以及状态文本
内嵌 NUL 的拒绝。该用例登记到包清单；Windows 当前 `xl.exe` 的包级
smoke 2/2、C 回归、C/XLang 基础范例，Linux 两组 XLang 用例均通过。
此项为候选包维护，不计入正式优先级清单的 100 包完成数；
2026-10-20 的记录仅描述当时的回归范围，不代表当前 AOT 仍被阻断。
