# lru

2026-10-24：更新后的 Windows 发布版执行默认 `test/check.ps1`，严格 C、
原生边界与线程、中央/包内 XLang JIT、私有入口拒绝、C/XLang 同输出及
边界 AOT 构建均通过。边界 EXE 先打印 `lru boundaries: PASS`，随后
模块退出时抛 `RuntimeError: operation is not valid in the current state`
并返回 1；测试在顶层持有多个 native-backed `LruCache`。这与已登记的
顶层原生类退役风险同形，但不能只凭症状断定同根。发布级 AOT 未签收，
详见 `dev/extlib_blockers/global_native_class_aot_fini.md`。

字符串键/值的有界 LRU 与 LFU 内存缓存。XRT 映射负责键查找，双向链表记录访问次序；没有持久化、TTL、异步加载或跨进程共享。

## 行为合同

| XLang API | 约定 |
| --- | --- |
| `lru.create(capacity)` | 建立 LRU 缓存，容量必须大于零 |
| `lru.createLFU(capacity)` | 建立 LFU 缓存，容量必须大于零 |
| `cache.put(key, value)` | 插入或覆盖；覆盖计一次使用。新条目在旧条目安全插入后才触发淘汰，分配失败不丢旧值 |
| `cache.get(key, default = "")` | 命中返回独立文本并更新频次/次序；缺失返回独立的 default 并累计 miss |
| `cache.contains(key)` | 不更新频次/次序/统计 |
| `cache.remove(key)` | 删除返回 true，缺失返回 false |
| `cache.clear()` | 清空条目及 hit/miss 计数，保留容量 |
| `count` / `capacity` / `hits` / `misses` | 当前快照 |

LRU 淘汰最久未访问条目；LFU 淘汰访问频次最低条目，同频时淘汰最久未访问者。LRU 查找/更新/淘汰平均 O(1)；LFU 查找与更新平均 O(1)，**淘汰时扫描最多 capacity 个条目**。不用 O(1) LFU 的复杂索引换取这部分边缘成本，适用于中小缓存。频次和统计计数在 int64 上限饱和。

空键 `""` 是合法键；空值 `""` 与缺失键不同：`contains` 可判断是否存在；`get(key, default)` 的缺失判断在原生缓存锁内完成，不依赖第二次 `contains`，避免并发读写误判。C 字符串入口不支持嵌入 NUL 的键或值；XLang 的键、值和缺失默认值若含 NUL，进入 C 前即抛 `ValueError`，不会静默截成前缀。

C 原生 `get` 在结果副本分配失败时不更新命中统计或访问顺序。XLang 调用中，若原生读取已成功、随后周边语言表达式分配失败，调用者仍可能收到 `MemoryError`，但该次命中和最近使用顺序已经更新；缓存内容保持有效。这两种故障阶段不能合并为“失败一律不更新统计”。

## C 接口和所有权

声明见 `include/lru_native.h`。`xlru_create` 返回需 `xlru_destroy` 的句柄。`xlru_get` 返回独立分配的文本指针，C 调用方转成 `char*` 使用后以 `xrtFree` 释放；失败返回 `NULL`，以 `xrtGetError` 查因。`xlru_put` / `xlru_clear` 返回 1 成功、0 失败；`xlru_remove` 返回 1 删除、0 缺失、-1 错误。XLang 对象管理句柄生命周期并自动接管 `get` 文本。

公开 C 操作在调用开始时清除本线程上次留下的 XRT 错误，成功调用不会保留旧错误。空句柄统计查询返回 -1，并设置 `XERR_ARGUMENT`。

每个缓存使用 XRT 互斥锁同步操作，结果副本在后续淘汰或清理后仍有效。销毁句柄前必须结束其它线程对它的使用；`destroy` 不能与正在进行的操作并发。

## 范例和验证

- XLang：`release/examples/extlibs/lru.xl`
- 同输出 C 对照：`release/extlibs/lru/examples/lru_c.c`
- 固定输出：`release/extlibs/lru/examples/basic.expected.txt`
- 边界：`release/extlibs/lru/test/test_lru.c`、`test_threads.c`、`boundaries.xl`、`allocations.xl` 与 `test/cases/extlib_lru.xl`

`putNative` 等原生入口仅供类内部使用，包外不能绕过公开方法的参数与生命周期检查；`test/private_native.xl` 是预期编译失败的可见性负例。

在 `release` 运行 `xl run extlibs/lru/test/boundaries.xl`。在 `demo6` 运行 `powershell -NoProfile -ExecutionPolicy Bypass -File tools/run_extlib_c.ps1 -Package lru -Source release/extlibs/lru/test/test_lru.c`。2026-10-12 已通过 Windows 严格 GCC、Linux ASan/UBSan、同实例并发读写、XL JIT 与双语输出对照；2026-10-13 增加六种 NUL 红绿用例并复跑 C/XL 主路径及范例。同日补充分配故障注入，逐点验证满缓存插入失败不淘汰旧值、覆盖失败不改原值，以及读取失败不改变命中统计；C 定向测试与严格 GCC 语法检查通过。
Windows 可在 `demo6` 运行 `powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/lru/test/check.ps1 -SkipAot`，同次检查严格 GCC、C 边界/线程、中央及包内 XLang、原生入口私有性、C 直跑和双语输出。去掉 `-SkipAot` 会继续构建完整边界 AOT。C 教学范例内含包的原生实现，可直接 `xl run release/extlibs/lru/examples/lru_c.c`；正式包仍按清单分别编译原生源。

2026-10-22：C/XLang 教学范例除互比外，还与固定输出逐字核对；
Windows 默认门禁在边界 AOT 后继续构建、运行公开范例 AOT，并核对同一预期。
Windows `-SkipAot` 与 Linux C ASan/UBSan、并发及语言联合门禁通过。
默认 Windows 脚本仍先在边界 AOT 缺 `xl6ValueDataSnapshotV1` 处失败；
公开范例单独构建也缺同一符号。新增的范例步骤须待主线修复后重跑，
不据聚焦门禁通过签收整包。

`xl build extlibs/lru/test/boundaries.xl -o ../build/test/lru_xl_test.exe` 当前被共享链接缺符号 `xl6ValueDataSnapshotV1` 阻断，AOT 尚未验收。主线修复后需重跑，见 `dev/extlib_blockers/text_snapshot_link.md`。

2026-10-14 补充淘汰顺序边界：分配失败的覆盖不得刷新 LRU/LFU 顺序；`contains` 不刷新顺序，而成功覆盖应刷新。C 故障注入与 XLang 定向回归通过。

2026-10-14 补公开 C 接口连续调用的错误状态回归：非法参数后有效创建、读写、删除、清空及统计应清除旧错误；空句柄统计应报参数错误。Windows 严格 GCC、C/XL 边界、双语同输出通过。在 `demo6` 运行 `bash release/extlibs/lru/test/check.sh` 可复跑 Linux ASan/UBSan 门禁；本轮已通过。AOT 阻断仍见上文。

2026-10-16 增加 LRU/LFU 双模式 XLang 逐点故障测试，覆盖创建、满容量插入、覆盖及读取的 `MemoryError`、旧值保持和恢复；C 故障测试补验 `XERR_MEMORY`。读取测试还固定区分原生失败与原生成功后语言表达式失败的命中统计。Windows 严格 GCC/C、目录 smoke 3/3、无输出故障注入 AOT 与 Linux C/线程 ASan/UBSan 通过；完整边界 AOT 的共享缺符号阻断未变。

同日增加独立 C 小模型的 800 步确定性交错对照，分别检查 LRU/LFU 的淘汰、覆盖、命中/未命中、删除、清空及每步计数；测试还要求序列实际触发关键分支。Windows C 与 Linux ASan/UBSan 门禁通过。该测试验证包内原生算法，不替代完整 XLang AOT 验收。

同日补 XLang LFU 覆盖边界：覆盖已有键会提高频次，随后加入第三键时淘汰较低频的旧键。Linux `test/check.sh` 现合并 C ASan/UBSan 边界与并发测试、中央和包内 XLang 用例、C 范例 sanitizer 运行及 C/XLang 逐字节同输出；完整门禁通过。完整 AOT 的共享阻断未变。

2026-10-18：C/XLang 边界补空字符串键的写入、命中、读取和删除；
Windows TCC C、XLang JIT 及 Linux C ASan/UBSan/线程/语言联合门禁通过。
实现本体无需改变，完整 AOT 的共享缺符号阻断仍在。

2026-10-19：C/XLang 边界新增两条只在最后一个字节不同的 8 KiB 键，
验证完整键比较、容量淘汰和已返回文本独立持有。Windows C/XLang 定向
用例与 Linux 严格 C、ASan/UBSan、并发、语言及双语范例联合门禁通过；
这不改变输入合同，也不解除完整 AOT 的共享阻断。

同日：C 教学范例原先只靠 GCC 额外链接原生源才能运行，单独 `xl run` 报 `xlru_destroy` 缺符号；现范例内含原生实现，Linux 门禁不再重复链接，并增加 C 直跑与 GCC 逐字对照。新增 Windows 联合门禁的严格 GCC、C 边界/线程、XLang、私有入口及双语输出通过；Linux ASan/UBSan 联合门禁通过。默认 Windows AOT 构建实测仍缺共享 `xl6ValueDataSnapshotV1`，不签收发布级。

2026-10-20：包级 C 测试宿主只编译 `map`、`mutex`、`string`、`thread` 与分配故障注入所需的 XRT 模块，Windows 门禁不再链接无关的系统库。`check.ps1 -SkipAot` 的严格 C、线程、C 直跑、XLang 边界/故障注入、私有入口与双语同输出均通过；Linux `check.sh` 的 C ASan/UBSan、线程及 XLang 联合门禁通过。此项仅缩短测试构建时间，不改变缓存算法或解除完整 AOT 阻断。

2026-10-23：四处故障注入捕获由单名 `catch MemoryError` 改成明确的
`catch MemoryError error`。前者在当前语法中是无类型捕获变量，不能证明
异常类别。严格分类后双模式创建、插入、覆盖与读取测试，以及 Windows
聚焦门禁、Linux C ASan/UBSan/XLang 联合门禁仍通过。库实现和公开 API
未改；完整 AOT 的共享缺符号阻断仍未解除。
