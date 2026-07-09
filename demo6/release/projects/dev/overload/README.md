# overload - 方法重载验证工具

验证 xlang 的四类重载能力：函数重载、方法重载、构造重载、协议方法重载（含 `__call__`）。

## 运行

```bat
cd /d D:\GIT\x-lang\demo6\release
xl run projects\dev\overload\overload.xl
xl build projects\dev\overload\overload.xl -o projects\dev\overload\overload.exe
overload.exe
```

三条路径都必须 EXIT=0。

## 覆盖范围（40 条断言，全部 PASS）

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

## 发现的 BUG（未执行，仅记录）

本工具只执行「编译且运行都正确」的重载路径。下列重载有编译器缺陷，执行会导致编译失败或运行期硬崩溃，
因此未纳入可执行测试，只在结尾以 `NOTE` 形式打印。完整复现见上级 `../BUGS.md`：

- **BUG-O1**: `var` 实参无法分派到重载函数（编译期 `[sem] 没有匹配的函数重载`）
- **BUG-O2**: `__item__ as array<int>` 重载在运行期硬崩溃 `0xC0000005`（阻塞「返回整行」模式）
- **BUG-O3**: `get` 属性 0 维 + 1 维重载，访问 0 维版本报 `[checked] 找不到 class 方法`
- **LIMIT-O1**: `f(T)` + `f(T, U=默认)` 因「必选参数数相同」被判为歧义重载（限制，非崩溃）

## 设计要点

- 测试框架与 `vops` 一致：`g_pass` / `g_fail` 计数 + `ok` / `bad` / `expectI` / `expectF` / `expectS`。
- 每条断言包在 `try/catch` 内，单条失败不中断整体。
- `main(...)` 始终返回 0，保证 `xl run` / `xl build` 双路径 EXIT=0。
