# gstack - 泛型验证工具

用 xlang 编写的泛型能力综合验证器。通过一组真实可用的泛型容器（`Stack<T>` / `SortedStack<T>` / `Range<T>`）和泛型函数，全面压测 xlang 的泛型声明、约束、实例化与迭代协议。

## 用法

```bat
cd /d D:\GIT\x-lang\demo6\release
xl run projects\dev\gstack\gstack.xl
xl build projects\dev\gstack\gstack.xl -o projects\dev\gstack\gstack.exe
gstack.exe selftest
gstack.exe demo
gstack.exe help
```

无参数运行等同 `selftest`。`selftest` 全部通过返回 EXIT=0，任一断言失败返回非 0。

## 验证覆盖（35 条断言）

| # | 能力 | 实现位置 |
|---|---|---|
| 1 | 泛型函数 `max<T>` / `min<T>` / `identity<T>`，显式 `::<int>` 与推断实例化 | `gstack.xl` 顶部 |
| 2 | 泛型约束 `<T is comparable>`、`<T is cloneable>` | `maxValue` / `cloneValue` |
| 2 | 组合约束 `<T is cloneable and json>` | `combined` |
| 2 | class 继承约束 `<T is Entity>`（`Player extends Entity`） | `keepEntity` |
| 3 | 泛型 class `Stack<T>`，`array<T>` 字段 + `push/pop/top/size/isEmpty/clear` | `Stack<T>` |
| 3 | 多类型实例化 `Stack<int>` / `Stack<string>` / `Stack<float>` | `testStack{Int,String,Float}` |
| 3 | 带约束的泛型 class `SortedStack<T is comparable>`，内部 `array<T>` 冒泡排序 | `SortedStack<T>` |
| 3 | 泛型函数 `array<T>` 形参 `sortArray<T>` / `arrayMax<T>`（GS1 已修复） | `sortArray` / `arrayMax` / `testGenericArrayFunc` |
| 4 | 泛型 class 迭代协议 `Range<T>.__iter__() as RangeIter<T>` + `RangeIter<T>.__next__() as (bool, T)` | `Range<T>` / `RangeIter<T>` |
| 4 | `for value in Range` 与 `for index, value in Range` | `testRangeIter` |

## 已知限制 / 发现的 BUG

详见 `../BUGS.md`「gstack 工具发现」一节。摘要：

- **BUG-GS2（低）**：`hashable` 不是合法泛型约束名。合法能力约束仅
  `comparable` / `json` / `xson` / `cloneable`（外加 class 继承约束）。
  组合约束示例改用 `<T is cloneable and json>`。

## 验证结果

- `xl run projects\dev\gstack\gstack.xl` → EXIT=0，35/35 PASS
- `xl build ... -o gstack.exe` → EXIT=0；生成的 exe 在 `selftest` / `demo` / `help` 三种模式下均正常
