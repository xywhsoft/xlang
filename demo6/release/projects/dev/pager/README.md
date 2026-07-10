# pager - 内存页式管理模拟器

虚拟内存页式管理 + 页面置换算法 + TLB 模拟器（xlang 实现）。

## 用法

```bat
cd /d D:\GIT\x-lang\demo6\release
xl run projects\dev\pager\pager.xl test
xl run projects\dev\pager\pager.xl test frames=3
xl run projects\dev\pager\pager.xl run lru   projects\dev\pager\sample.trace frames=4
xl run projects\dev\pager\pager.xl run clock projects\dev\pager\sample.trace
xl run projects\dev\pager\pager.xl compare   projects\dev\pager\sample.trace frames=3
xl run projects\dev\pager\pager.xl help
```

## 命令

| 命令 | 说明 |
|---|---|
| `pager run <algo> <trace> [frames=4]` | 单算法回放访问序列，输出逐步回放表 + 缺页/TLB 统计 |
| `pager compare <trace> [frames=4]` | 4 种算法同序列同帧数对比表 |
| `pager test [frames=N]` | 内置经典序列 `1 2 3 4 1 2 5 1 2 3 4 5` 演示全部算法 |
| `pager help` | 帮助 |

`algo`: `lru` / `fifo` / `clock` / `opt`。

## trace 格式

每行一个页号；同一行也可空格分隔多个：

```
1 2 3 4 1 2 5 1 2 3 4 5
```

非数字 token 自动跳过。

## 功能

1. **虚拟地址空间模拟**：`class PageTable` 二级页表（页目录 + 页表），`VPN = dirIdx(高) + tblIdx(低)`，`dirOccupancy` 维护各目录槽驻留页数。
2. **页面置换算法**：
   - **LRU** 最近最少使用（每次访问刷新 `lastUsed`，淘汰最小者）
   - **FIFO** 先进先出（按装入顺序淘汰队首）
   - **Clock** 时钟/二次机会（帧环形扫描，ref=1 清零跳过，ref=0 淘汰）
   - **OPT** 最佳置换（前瞻 trace，淘汰未来最远才使用者；作基准）
3. **TLB 模拟**：`class TLB` 固定槽位 + 内部 LRU 替换，命中/未命中独立计数。
4. **缺页统计**：缺页数、缺页率、TLB 命中数、TLB 命中率。
5. **访问序列回放**：`run` 命令逐访问输出 `Step/Page/TLB/Fault/Frames` 表。

物理帧用 `array<int>` 存储（`frames[i] = 帧 i 中的页号`，`-1` = 空闲）。

## 结构

- `class PageTable` — 二级页表（页目录 + 页表），PTE 含 frame/dirty/referenced/lastUsed。
- `class TLB` — 翻译后备缓冲器（固定容量 + LRU）。
- `class Pager` — 模拟器引擎（帧/页表/TLB + 算法状态 + 统计 + 回放日志）。
- `class TraceStep` — 单次访问回放记录。
