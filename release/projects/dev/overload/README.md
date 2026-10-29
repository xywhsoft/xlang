# overload - 方法重载验证工具

验证 xlang 的四类重载能力：函数重载、方法重载、构造重载、协议方法重载（含 `__call__`）。

## 运行

```bat
cd /d D:\GIT\x-lang\release
xl run projects\dev\overload\overload.xl
xl build projects\dev\overload\overload.xl -o projects\dev\overload\overload.exe
overload.exe
```

三条路径都必须 EXIT=0。

## 覆盖范围

| 类别 | 被测写法 | 说明 |
|---|---|---|
| 函数重载 | `area(int)` / `area(int,int)` / `area(float)` | 同名不同参数，按静态类型分派 |
| 方法重载 | `Grid.at(int)` / `at(int,int)` / `at(int,int,int)` | 1/2/3 维索引 |
| 构造重载 A | `Vec()` / `Vec(int)` / `Vec(int,int)` | 0/1/2 参数三个 `create` |
| 构造重载 B | `Box()` / `Box(int, int=0)` | 无参 ctor 与带默认参数 ctor 共存 |
| 协议重载 | `Matrix2D.__item__(int,int)` / `__item__(int)` / `__setItem__(int,int,int)` | 二维读 / 一维读 / 二维写 |
| `__call__` 重载 | `Dispatch(int)` / `Dispatch(string)` / `Dispatch(int,int)` | 三签名调用 |
| 解析精度 | `kind(int)` / `kind(int64)` / `kind(float)` | int / int64 / float 字面量与变量 |
| 综合场景 | `Point`: 2 ctor + 重载 `distance` + 重载 `__call__` + `__eq__` | 多重重载并存 |
| 管理值返回 | `RowLookup.__item__(int|string) as array<int>` | 协议重载返回静态泛型容器 |
| 属性重载 | `Meter.value` / `Meter.value[int]` | 零维与一维属性同名重载 |

## 重载解析边界

重载在编译期按静态类型解析：

- `var` 不参与运行时重载搜索；先写 `value as int` 等 checked downcast，再调用目标重载。
- `f(T)` 与 `f(T, U=默认)` 在单参数调用时都是同等候选，编译器应报告歧义；调用方需要补齐参数或调整 API。

## 设计要点

- 测试框架与 `vops` 一致：`g_pass` / `g_fail` 计数 + `ok` / `bad` / `expectI` / `expectF` / `expectS`。
- 每条断言包在 `try/catch` 内，单条失败不中断整体。
- `main(...)` 返回失败数，便于脚本和 CI 识别回归。
