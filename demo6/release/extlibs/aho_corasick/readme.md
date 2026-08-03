# aho_corasick — 多模式字符串匹配

在一段文本中**同时**搜索多个关键词，一次扫描即找出所有命中位置。

基于 Aho-Corasick 自动机算法，时间复杂度 O(文本长度 + 匹配数)，与模式数量无关。

---

## 典型应用场景

| 场景 | 说明 |
|------|------|
| 敏感词过滤 | 用户输入中检测违禁词，替换或拦截 |
| 日志多规则匹配 | 一条日志同时匹配数百条告警规则 |
| 关键词高亮 | 搜索结果页面标注命中词 |
| 入侵检测 | 网络载荷中匹配攻击特征库 |
| 词典标注 | NLP 中对文本进行实体识别 |
| 代码审计 | 源码中批量搜索危险函数调用 |

---

## 快速开始

```xlang
import aho_corasick
from aho_corasick import Match, Automaton

Automaton ac = aho_corasick.create(["he", "she", "his", "hers"])

// 搜索所有匹配
for m in ac.search("ushers")
    println(m.pattern, " @", m.start, "..", m.endPos)
end
// she @1..4
// he  @2..4
// hers @2..6

// 敏感词一行搞定
Automaton filter = aho_corasick.create(["赌博", "色情"], ignoreCase = true)
string safe = filter.replace("涉及赌博内容", "***")  // "涉及***内容"
```

---

## 核心概念

### 自动机（Automaton）

自动机是本库的核心对象。它持有一组模式（patterns），构建后形成 Trie + failure link 结构，可对任意文本执行线性扫描。

**生命周期**：`创建 → 添加模式 → 构建 → 搜索（可重复）`

### 匹配结果（Match）

每次命中产出一个 Match 对象：

| 字段 | 类型 | 说明 |
|------|------|------|
| `pattern` | string | 命中的模式文本 |
| `start` | int | 起始字节偏移（含） |
| `endPos` | int | 结束字节偏移（不含） |
| `length` | int | 匹配长度（= endPos - start） |

### 区间合并

`replace`、`mask`、`highlight` 会自动合并重叠区间。例如 "ushers" 中 she[1,4)、he[2,4)、hers[2,6) 合并为 [1,6)，替换结果为 `"u*"`。

---

## API 参考

### 工厂函数

```xlang
aho_corasick.create(patterns = [], ignoreCase = false) as Automaton
```

- `patterns`：模式字符串数组。非空时自动调用 `build()`。
- `ignoreCase`：匹配时忽略大小写（构建和搜索双侧统一 tolower）。

### Automaton 方法

| 方法 | 返回值 | 说明 |
|------|--------|------|
| `addPattern(string)` | — | 添加单个模式（build 前） |
| `addPatterns(array<string>)` | — | 批量添加 |
| `build()` | — | 构建自动机（之后不可再添加） |
| `search(text)` | `array<Match>` | 返回全部匹配 |
| `searchFirst(text)` | `Match` | 返回首个匹配，无则 null |
| `contains(text)` | `bool` | 是否存在任意匹配（最快） |
| `count(text)` | `int` | 匹配总数 |
| `scan(text, fn)` | `bool` | 流式扫描，fn 返回 false 中断 |
| `replace(text, repl)` | `string` | 合并区间 → 固定文本替换 |
| `replaceWith(text, fn)` | `string` | 非重叠匹配 → 回调决定替换 |
| `mask(text, ch)` | `string` | 合并区间 → 逐字符遮盖 |
| `highlight(text, open, close)` | `string` | 合并区间 → 标签包裹 |

### Automaton 属性

| 属性 | 类型 | 说明 |
|------|------|------|
| `valid` | bool | handle 非空（未销毁） |
| `built` | bool | 是否已构建 |
| `patternCount` | int | 已添加的模式数 |

---

## 由浅入深

### 第 1 步：创建与搜索

```xlang
Automaton ac = aho_corasick.create(["error", "warn", "fatal"])
array<Match> hits = ac.search("error: disk\nwarn: mem\nfatal: crash")
```

### 第 2 步：快速判定

```xlang
if ac.contains(userInput)
    // 有敏感内容，需要处理
end
int n = ac.count(logText)  // 统计命中数
```

### 第 3 步：替换与遮盖

```xlang
string clean = ac.replace(text, "[已过滤]")   // 固定替换
string masked = ac.mask(text, "*")            // 逐字符遮盖
string html = ac.highlight(text, "<em>", "</em>")  // 高亮
```

### 第 4 步：回调替换

```xlang
string result = ac.replaceWith(text, func(Match m) as string
    return "<a href='/search?q=" + m.pattern + "'>" + m.pattern + "</a>"
end)
```

### 第 5 步：流式扫描（大文本 + 早停）

```xlang
ac.scan(hugeText, func(Match m) as bool
    if m.pattern == "fatal" then
        alert(m.start, m.pattern)
        return false  // 找到即停，不扫描剩余文本
    end
    return true
end)
```

### 第 6 步：手动构建（模式来源动态）

```xlang
Automaton ac = aho_corasick.create()
for line in file.readLines("rules.txt")
    ac.addPattern(line)
end
ac.build()
```

---

## 性能特征

| 方法 | 时间复杂度 | 内存分配 | 适用场景 |
|------|-----------|----------|----------|
| `contains` | O(到首命中) | 零 | 只需知道有没有 |
| `count` | O(文本长) | 零 | 只需知道多少个 |
| `scan` | O(到停止位) | 每步 1 个 Match | 流式处理、条件中断 |
| `search` | O(文本长 + 匹配数) | N 个 Match + 数组 | 需要全部结果 |
| `replace` / `mask` / `highlight` | O(文本长) | 1 次精确分配 | 固定替换 |
| `replaceWith` | O(输出长) | Buffer 自增 | 回调替换 |

### 选型建议

```
只需判定？        → contains / count（零分配，最快）
需要逐个处理？    → scan（不构建数组，可中断）
需要全部结果？    → search（结构化数组，可排序/遍历）
需要文本变换？    → replace / mask / highlight / replaceWith
```

### 实现细节

- **replace / mask / highlight**：C 层预计算输出长度 → `xrtTempMemory` 单次分配 → 顺序 memcpy。无中间字符串对象。
- **replaceWith**：xlang 标准库 `Buffer`（底层 xbuffer，64KB 步长自增）。回调返回值长度不可预知，故不使用预分配。
- **contains**：命中第一个模式即返回，不消费剩余文本。
- **scan**：C 层增量步进器，每次 `xac_scan_next` 推进到下一个匹配，回调返回 false 后扫描立即终止。

---

## 注意事项

### 偏移是字节偏移

`Match.start` 和 `Match.endPos` 是**字节**偏移，不是字符偏移。对纯 ASCII 文本两者相同；对 UTF-8 多字节文本（如中文），一个字符占 3 字节。

### build 后不可修改

调用 `build()` 后再调用 `addPattern` 会抛出 `RuntimeError`。如需更新模式集，创建新的 Automaton。

### 重叠匹配

`search` 返回所有匹配（含重叠）。`replace`/`mask`/`highlight` 会合并重叠区间。`replaceWith` 跳过与前一个替换重叠的匹配（左优先）。

### ignoreCase 的工作方式

构建时将模式统一 tolower 插入 Trie，搜索时将文本逐字节 tolower 后匹配。仅对 ASCII 字母有效（A-Z → a-z），不影响 UTF-8 多字节字符。

### 空模式集

`create()` 不带参数时返回未构建的空自动机。对空自动机调用 `search`/`contains`/`count` 前必须先 `build()`，否则抛出异常。空自动机匹配任何文本都返回 0 个结果。

---

## 文件结构

```
aho_corasick/
├── aho_corasick_native.c    # C 引擎：Trie、failure link、搜索、替换
├── src/
│   └── aho_corasick.xl      # xlang API 层：export class + 工厂函数
├── xlang.package.json       # 包清单
└── readme.md                # 本文件
```

---

## 完整范例

参见 `release/examples/extlibs/aho_corasick.xl`，包含 12 节逐步教学，覆盖全部 API。
