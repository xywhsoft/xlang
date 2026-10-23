# aho_corasick - 多模式匹配

`aho_corasick` 用一次扫描同时查找多组字节模式，并为常用文本场景提供查找、计数、替换、遮盖、高亮和跨分块扫描能力。

本库分为两层：

- `include/xrt_aho_corasick.h`：完整 C 契约，使用 `xstrview`，支持嵌入零字节。
- `src/aho_corasick.xl`：XLang 易用层，提供 `Automaton`、`Scanner` 和 `Match`。



## 快速开始

```xlang
import aho_corasick
from aho_corasick import Match, Automaton

Automaton words = aho_corasick.create(["he", "she", "his", "hers"])

for item in words.search("ushers")
    println(item.pattern, " @", item.start, "..", item.endPos)
end

string safe = words.replace("ushers", "*")
assert safe == "u*"
```

C 调用方可以直接使用明确长度视图：

```c
#include "xrt_aho_corasick.h"

xrtaho* automaton = xrtAhoCreate(NULL);
xrtAhoAdd(automaton, XRT_STR_LITERAL("error"));
xrtAhoAdd(automaton, XRT_STR_LITERAL("fatal"));
xrtAhoBuild(automaton);

bool contains = false;
xrtAhoContains(automaton, XRT_STR_LITERAL("fatal: disk"), &contains);
xrtAhoDestroy(automaton);
```



## 实现特征

旧实现为每个 Trie 节点固定保存 `children[256]`，64 位目标上仅子节点表就占 2048 字节。当前实现改为：

- 连续节点数组；
- 构建期稀疏边；
- 构建后按节点压实、排序的转移区间；
- failure link 和 output link；
- 只保存直接输出，不向子节点复制 failure 输出。

普通节点不再承担 256 个指针的固定开销。查找转移使用二分搜索，单次最多比较 8 次；failure 回退仍保持 Aho-Corasick 的线性摊销特征。

经典 `he/she/his/hers` 自动机包含 10 个节点和 9 条压实转移。当前 64 位测试门禁要求模块有效分配量小于 4 KiB；旧固定表仅 10 个节点的子指针就需要约 20 KiB。

`build()` 在分配转移表和 BFS 队列成功后才修改 failure 状态。分配失败时自动机仍处于未构建状态，可直接重试，不会重复合并输出或留下半构建结构。

XLang 的文本结果优先从原生 `Buffer` 直接移交已有 XRT 分配，避免再由
`Buffer.text()` 复制整串。仅当结果无嵌入零字节且缓冲已有终止字节空间时
使用此路径；含零字节或缺少可移交内存/终止空间时仍由长度感知的 `Buffer.text()`
生成结果，原缓冲保持不变。这个优化不改变匹配器的字节语义。



## 匹配语义

### 字节模式

核心匹配器处理任意字节序列，不要求 UTF-8，也不以 `\0` 作为结束条件。`xstrview` 中的嵌入零字节属于模式和文本的一部分。

`bIgnoreAsciiCase` 只折叠 ASCII `A-Z`，不会尝试进行 Unicode 大小写映射。Unicode 全量大小写折叠会改变字节长度和一对多映射关系，不属于此自动机的隐式行为。

### 偏移

`Match.start`、`Match.endPos`、`xrtahomatch.iStart` 和 `iEnd` 都是 UTF-8 原始字节偏移，区间为 `[start, end)`。

### 普通结果顺序

`search` / `xrtAhoSearch` 的结果按以下规则稳定排序：

1. 起点升序；
2. 同起点结束位置升序；
3. 同区间模式注册索引升序。

`scan` 和增量扫描为了即时发送结果，按扫描结束位置顺序产生匹配，不执行结果排序。

### 非重叠选择

`replaceWith` / `xrtAhoSearchNonOverlapping` 使用独立的左优先最长规则：

1. 选择当前最小起点；
2. 同起点选择最长模式；
3. 跳过与已选择结果重叠的后续匹配。

`replace`、`mask` 和 `highlight` 会合并严格重叠区间。相邻区间不合并，因此 `ab` 和 `cd` 在 `abcd` 中仍是两个替换位置。



## XLang API

### 创建

```xlang
aho_corasick.create(
    patterns = [],
    ignoreCase = false,
    maxPatterns = 0,
    maxPatternBytes = 0,
    maxTotalPatternBytes = 0,
    maxNodes = 0,
    maxMatches = 0,
    maxInputBytes = 0,
    maxOutputBytes = 0
) as Automaton
```

限制字段为零时不限制。非零限制在添加、扫描或创建输出前执行，不会在超限后返回截断结果。

`patterns` 的类型为 `array<string>`，不将数字或其他对象隐式转换成模式。单个字符串
也不是模式数组。负数限制被拒绝；预算以字节数或对应对象计数为单位，不是 CPU 时间
或整个进程的内存上限。

传入非空 `patterns` 时自动调用 `build()`。不传模式时可以逐项添加后手动构建：

```xlang
Automaton words = aho_corasick.create()
words.addPattern("warn")
words.addPatterns(["error", "fatal"])
words.build()
```

空模式无定义且会被拒绝。`ignoreCase = true` 时，ASCII 大小写等价的重复模式也会被拒绝。
`addPatterns` / `xrtAhoAddMany` 顺序添加，不是事务：后续元素失败时，之前成功添加的
模式保留。空数组是无操作，但 C 入口仍要求有效的自动机指针。

### Automaton

| 成员 | 返回值 | 说明 |
|---|---:|---|
| `valid` | `bool` | 原生句柄是否存在 |
| `built` | `bool` | 是否已构建 |
| `patternCount` | `int` | 模式数量 |
| `pattern(index)` | `string` | 返回指定模式的独立字符串 |
| `addPattern(pattern)` | - | 添加一个非空模式 |
| `addPatterns(patterns)` | - | 顺序添加一组模式 |
| `build()` | - | 构建不可变自动机 |
| `search(text)` | `array<Match>` | 返回排序后的全部重叠匹配 |
| `searchFirst(text)` | `Match?` | 无结果数组地返回普通排序下首项 |
| `contains(text)` | `bool` | 首次命中立即停止 |
| `count(text)` | `int` | 不创建结果数组地计数 |
| `scan(text, fn)` | `bool` | 即时回调；`false` 表示被回调停止 |
| `scanner()` | `Scanner` | 创建跨分块扫描器 |
| `replace(text, replacement)` | `string` | 合并重叠区间后固定替换 |
| `replaceWith(text, fn)` | `string` | 左优先最长回调替换 |
| `mask(text, mask = "*")` | `string` | 按 UTF-8 标量数量遮盖 |
| `highlight(text, open, close)` | `string` | 包裹合并后的重叠区间 |

### Match

| 字段 | 类型 | 说明 |
|---|---:|---|
| `pattern` | `string` | 注册时的原始模式 |
| `start` | `int` | 起始字节偏移 |
| `endPos` | `int` | 结束字节偏移 |
| `length` | `int` | 匹配字节长度 |

### Scanner

`Scanner` 持有自动机引用，并复制当前输入分块。回调暂停或抛出异常时，剩余分块不会引用已经销毁的临时字符串。

XLang 调用方使用 `Automaton`、`Scanner` 的公开包装方法；`*Native` 方法仅供类内部调用，不能自行传入字节长度或操作原生扫描状态。C 调用方仍可按公共头文件使用原生 API。

原生 `Scanner(point)` 构造器目前仍可被包外代码引用；请只使用 `Automaton.scanner()`。最小复现和需要主线解决的权限边界见 `dev/extlib_blockers/aho_scanner_constructor.md`，因此本包尚不具备完整发布验收条件。

```xlang
Scanner scanner = words.scanner()

scanner.feed("fa", func(Match item) as bool
    return true
end)

scanner.feed("tal", func(Match item) as bool
    println(item.pattern, " @", item.start)
    return false
end)

if scanner.pending then
    scanner.resume(func(Match item) as bool
        return true
    end)
end
```

| 成员 | 说明 |
|---|---|
| `feed(chunk, fn)` | 复制并扫描新分块；未恢复旧分块时拒绝新输入 |
| `resume(fn)` | 继续被回调暂停的当前分块 |
| `reset()` | 丢弃当前分块并把偏移、计数和 failure 状态归零 |
| `pending` | 是否仍有未消费分块 |
| `offset` | 累计消费字节数 |
| `matchCount` | 累计发送匹配数 |

回调抛出的异常保留原类别和消息；已经交付的匹配不会在恢复时重复交付。
同一个 Scanner 在回调内再次 `feed`、`resume` 或 `reset` 会被拒绝，其他独立
搜索对象不受此限制。这是重入保护，不是允许多线程共享 Scanner 的锁。

输入分块超过累计 `maxInputBytes` 时，在复制和消费前拒绝，原状态保持不变。
扫描中途超过 `maxMatches` 等限制时，`offset` 保留真实已消费量，状态进入失败态，
必须 `reset()` 后才能继续使用。回调暂停或回调异常不等同于扫描器失败态。



## C API

### 配置与生命周期

| API | 说明 |
|---|---|
| `xrtAhoOptionsInit` | 初始化 `xrtahooptions` |
| `xrtAhoInfoInit` | 初始化 `xrtahoinfo` |
| `xrtAhoCreate` | 创建空自动机 |
| `xrtAhoRef` | 增加引用 |
| `xrtAhoDestroy` | 释放引用 |
| `xrtAhoAdd` | 添加一个明确长度模式 |
| `xrtAhoAddMany` | 顺序添加多个模式 |
| `xrtAhoBuild` | 构建 failure link 和压实转移表 |
| `xrtAhoBuilt` | 查询构建状态 |
| `xrtAhoPatternCount` | 查询模式数量 |
| `xrtAhoPattern` | 返回借用模式视图 |
| `xrtAhoGetInfo` | 获取模式、节点、转移和有效分配量快照 |

`xrtahooptions` 必须由 `xrtAhoOptionsInit` 初始化。`iSize` 和 `iVersion` 用于结构 ABI 校验；当前版本为 `XRT_AHO_OPTIONS_VERSION`。

`xrtahoinfo` 必须由 `xrtAhoInfoInit` 初始化，当前版本为 `XRT_AHO_INFO_VERSION`。`iAllocatedBytes` 统计自动机向 XRT 分配器请求的有效字节，不包含底层 allocator 元数据，也不包含结果集和扫描状态。

### 搜索与结果

| API | 说明 |
|---|---|
| `xrtAhoSearch` | 创建排序后的完整结果集 |
| `xrtAhoSearchNonOverlapping` | 创建左优先最长非重叠结果集 |
| `xrtAhoFind` | 无结果数组地返回首项 |
| `xrtAhoContains` | 无结果数组地判断命中 |
| `xrtAhoCount` | 无结果数组地计数 |
| `xrtAhoResultCount` | 返回结果数量 |
| `xrtAhoResultAt` | 读取一个 `xrtahomatch` |
| `xrtAhoResultDestroy` | 释放结果集 |

结果集持有自动机引用。调用方可以先销毁自己的自动机引用，再读取或销毁结果集；`xrtahomatch.Pattern` 仍然有效。

### 增量扫描

| API | 说明 |
|---|---|
| `xrtAhoStateCreate` | 创建持有自动机引用的增量状态 |
| `xrtAhoStateFeed` | 扫描一个借用分块 |
| `xrtAhoStateCheckInput` | 不消费输入，预先检查状态和累计输入限制 |
| `xrtAhoStateOffset` | 返回累计字节偏移 |
| `xrtAhoStateMatches` | 返回累计发送数量 |
| `xrtAhoStateReset` | 重置状态 |
| `xrtAhoStateDestroy` | 释放状态 |
| `xrtAhoScan` | 扫描单个完整视图的便利函数 |

`xrtAhoStateFeed` 返回：

- `XRT_AHO_SCAN_COMPLETE`：当前分块完整消费；
- `XRT_AHO_SCAN_STOPPED`：回调返回 false；
- `XRT_AHO_SCAN_ERROR`：参数、状态、资源或内存错误。

回调停止时，`pConsumed` 包含已经消费的当前分块字节数。同一结束位置可能还有待发送重叠匹配，因此下一次调用可能返回 `pConsumed == 0`。调用方必须再次传入原分块的剩余切片，直到返回 `COMPLETE`。

中途发生匹配限制错误时，`pConsumed` 同样包含真实已消费的字节数，不会错误归零。

C 增量状态不复制分块，只在 `xrtAhoStateFeed` 调用期间借用 `Chunk`。XLang `Scanner` 为了异常和暂停安全，会额外复制当前分块。
原生 C 回调不得销毁、重置或再次进入正在执行的同一个状态；嵌套扫描使用独立状态。
XLang `Scanner` 对同一对象的回调重入显式报错。

### 文本变换

| API | 说明 |
|---|---|
| `xrtAhoReplace` | 固定替换合并后的重叠区间 |
| `xrtAhoMaskBytes` | 每个匹配字节重复一次 Mask，支持二进制 |
| `xrtAhoMask` | 每个匹配 UTF-8 标量重复一次 Mask |
| `xrtAhoHighlight` | 包裹合并后的重叠区间 |
| `xrtAhoCheckOutputSize` | 检查输出字节预算，不分配缓冲区 |

四个函数始终返回独立、补零的 XRT 字符串，调用方使用 `xrtFree` 释放。`pOutputSize` 返回真实字节长度，因此输出可以包含嵌入零字节。

`xrtAhoMask` 严格校验文本和 Mask 的 UTF-8。匹配区间不是 UTF-8 标量边界时返回 `XRT_AHO_ERROR_UTF8`。任意二进制遮盖应使用 `xrtAhoMaskBytes`。

XLang 的 `replaceWith` 也执行 `maxOutputBytes` 检查，包含未匹配文本、回调结果和
尾部文本；没有匹配不意味着可以绕过输出限制。预算不限制用户回调自身的分配量。



## 错误模型

所有 C 失败都写入 XRT 当前执行上下文的结构化错误，域为 `xlang.extlib.aho_corasick`。

| 错误代码 | 含义 |
|---|---|
| `XRT_AHO_ERROR_ARGUMENT` | 空指针、无效视图或空模式 |
| `XRT_AHO_ERROR_MEMORY` | 分配失败 |
| `XRT_AHO_ERROR_STATE` | 未构建搜索、构建后添加等状态错误 |
| `XRT_AHO_ERROR_LIMIT` | 配置的资源限制被超过 |
| `XRT_AHO_ERROR_RANGE` | 索引或长度溢出 |
| `XRT_AHO_ERROR_UTF8` | Unicode 遮盖输入无效 |
| `XRT_AHO_ERROR_EXISTS` | 重复模式 |

XLang 包装不会把失败伪装成 `false`、`0`、空字符串或原始文本。异常类别遵循
XRT 结构化错误：无效值、空模式、重复模式和无效 UTF-8 为 `ValueError`，
原生索引及资源上限为 `RangeError`，分配失败为 `MemoryError`，状态错误为 `RuntimeError`。
语言层 `Automaton.pattern(index)` 的下标预检查使用 `IndexError`。
回调自己的异常原样传播，不能统一捕获为 `RuntimeError`。



## 所有权与线程规则

- 添加和构建阶段由调用方独占，不允许并发修改。
- `build()` 成功后自动机不可变，可并发执行搜索、计数和变换。
- `xrtahoresult`、`xrtahostate` 和 XLang `Scanner` 都持有独立自动机引用。
- 单个 `xrtahostate` / `Scanner` 不允许被多个线程同时操作。
- `xrtAhoPattern` 返回借用视图；结果集和匹配中的 `Pattern` 由对应自动机引用保证生命周期。



## 测试门禁

`test/test_aho_corasick.c` 覆盖：

- 搜索排序、左优先最长和严格重叠合并；
- 嵌入零字节模式与文本；
- 结果集、增量状态独立持有自动机引用；
- 跨分块匹配、回调暂停、零消费待发送结果和恢复；
- UTF-8 标量遮盖与二进制字节遮盖；
- 模式、节点、匹配、输入和输出限制；
- 构建期分配失败后无污染重试；
- 200 轮随机二进制模式与朴素匹配器差分，额外对照计数、首项、包含判断和随机分块；
- 分块覆盖完整消费与每次命中即暂停两种模式，并验证零消费待发结果；
- XRT memory-debug 下零活动分配、零越界、零重复释放和零 UAF 事件。

2026-09-27 的完整专项门禁通过 44 项 Windows 检查，包含严格 GCC 编译、全部 41 个
生成原型与公开 C 头对照、C/xlang JIT/AOT、三个工厂参数负例和成对范例固定输出。
另外执行 `test_aho_native.c`、`test_aho_alloc.c`、`test_aho_threads.c` 与
`test/boundaries.xl`：覆盖句柄寿命、状态与索引、回调异常/重入、输出预算、错误恢复、
38 个分配失败点，以及四线程共享不可变自动机的 1000 次独立扫描。
Linux 独立 C 的 ASan/UBSan/泄漏门禁也通过，包含两个 C 范例。

从 demo6 目录执行：

```powershell
./tools/check_extlib_aho.ps1
```

```sh
bash tools/check_extlib_aho.sh
```

证据：[Windows 专项](../../../docs/extlib_aho_2026-09-27.windows.results.json)、
[Linux 独立 C 专项](../../../docs/extlib_aho_2026-09-27.linux.results.json)。
此前的 [18 项增量记录](../../../docs/extlib_aho_2026-09-27.incremental.results.json)
作为历史证据保留。检查项包含编译与执行步骤，不是 44 个独立 API。

这不是全平台或性能认证：未覆盖 Linux xlang、32 位/ARM、同一 Scanner 的并发操作、
Unicode 全量大小写折叠。C 层故障注入也不代表已经验证全部语言运行库分配失败路径。



## 文件结构

```text
aho_corasick/
|- include/xrt_aho_corasick.h   C 公开契约
|- include/aho_corasick_native.h XLang 桥接精确原型
|- xrt_aho_corasick.c           稀疏自动机核心
|- aho_corasick_native.c        XLang 薄适配层
|- src/aho_corasick.xl          XLang 易用 API
|- test/test_aho_corasick.c     C 边界与故障注入测试
|- test/test_aho_native.c       C 桥接状态与寿命测试
|- test/test_aho_alloc.c        分配失败恢复测试
|- test/test_aho_threads.c      共享自动机并发测试
|- test/boundaries.xl           XLang 边界测试
|- examples/basic.c            与 XLang 对照的完整范例
|- docs/site.html              本地官网详情页内容源
|- xlang.package.json           包清单
`- readme.md                    本文档
```

完整示例：

- `release/examples/extlibs/aho_corasick.xl`
- `release/examples/extlibs/aho_corasick_c.c`
- `release/examples/extlibs/aho_corasick_basic.xl`
- `release/extlibs/aho_corasick/examples/basic.c`

最后两个范例的输出均与 `release/examples/extlibs/aho_corasick_basic.expected.txt`
严格对照，展示相同的查询、变换、暂停恢复和 Unicode 遮盖能力。
