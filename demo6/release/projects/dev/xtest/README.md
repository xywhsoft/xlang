# xtest - xlang 测试框架

类 minitest / JUnit 的轻量测试框架：断言库 + 测试发现 + 测试运行 + setup/teardown + 彩色 / JSON 报告 + 可选多线程并发。用 xlang 编写，用来给 xlang 代码写单测。

## 运行 / 构建

```bat
cd /d D:\GIT\x-lang\demo6\release
xl run  projects\dev\xtest\xtest.xl -- selftest
xl build projects\dev\xtest\xtest.xl -o projects\dev\xtest\xtest.exe
xtest.exe selftest
```

> 注意：`xl run` 把脚本名之后的位置参数当作 C helper 文件，所以给脚本的参数必须用 `--` 分隔：
> `xl run xtest.xl -- run <target>`。构建出的 `xtest.exe` 不需要 `--`。

## 命令

```
xtest run <test_file.xl>                运行单个测试文件
xtest run <dir> [-r]                   运行目录（-r 递归）下所有 test_*.xl
xtest run <target> --filter <pat>      只运行名字包含 pat 的测试（大小写不敏感）
xtest run <target> --format json       JSON 报告（CI 集成）
xtest run <target> --jobs <N>          最多 N 个文件并发运行（上限 8）
xtest run <target> --timeout <ms>      单文件超时（默认 30000）
xtest run <target> --no-color          关闭 ANSI 彩色
xtest run <target> --save-temps        保留生成的 harness 文件（调试用）
xtest list <target> [-r]               只列出发现的测试，不运行
xtest selftest                          用自带的 tests/ 验证 xtest 自身
xtest -h | --help                       帮助
```

退出码：全部通过 `EXIT=0`；存在失败 / 错误 `EXIT=1`。

## 编写测试

测试文件名以 `test_` 开头，里面定义以 `test_` 开头的无参函数。可选的 `setup()` / `teardown()` 会在**每个**测试前后执行。**不要定义 `main`**——它由 xtest 生成的 harness 提供。

```xlang
// test_math.xl
func test_addition()
    assertEqual(1 + 1, 2, "1+1 should be 2")
end

func test_string()
    assertContains("hello world", "world")
end
```

断言**不需要 import**：xtest 在运行每个文件时把断言库（`xtest_assert.xl`）拼到生成的 harness 前面，断言以全局函数形式可用。

### 可用断言

| 断言 | 说明 |
|---|---|
| `assertEqual(actual, expected, message="")` | 相等 |
| `assertNotEqual(actual, expected, message="")` | 不等 |
| `assertTrue(condition, message="")` | 真值（condition 为 var，按真值判断） |
| `assertFalse(condition, message="")` | 假值 |
| `assertNull(value, message="")` | 为 null |
| `assertNotNull(value, message="")` | 非 null |
| `assertContains(haystack, needle, message="")` | 字符串子串 / 数组成员 |
| `assertThrows(func block, message="")` | 期望 block 抛异常；block 用 `var b = func() as int => ...` 构造 |
| `assertInRange(value, low, high, message="")` | 闭区间 `[low, high]` |
| `approxEqual(actual, expected, tolerance, message="")` | 浮点近似 |
| `skip(reason)` | 标记当前测试为跳过 |

### setup / teardown / skip 示例

```xlang
int g_count = 0

func setup()
    g_count = 0
end

func teardown()
    // 每个测试后清理
end

func test_thing()
    g_count = g_count + 1
    assertEqual(g_count, 1)
end

func test_skipped()
    skip("暂不在本场景运行")
end
```

## 工作原理

1. **发现**：按 `test_*.xl` 文件名约定（可 `-r` 递归）。
2. **提取**：对每个文件 `xl parse --format json` 取 AST，收集 `test_*` 函数名，并探测 `setup` / `teardown` / `main`；AST 失败则回退到源码行扫描。
3. **生成 harness**：把断言库源码 + 用户测试文件源码 + 生成的「按名分派 + main 驱动循环」拼成一个临时 `.xl`。main 对每个测试做 `setup()` → `test_xxx()`（try/catch）→ `teardown()`（finally，仅当存在），用 `time.timer()` 计时，捕获 `throw` 字符串作为失败信息，输出一行 `__XTEST_RESULT__<json>`。
4. **运行**：`xl run <harness.xl>`，主进程解析结果行聚合。
5. **报告**：文本（彩色）或 JSON。

失败用 `throw "信息"` 表达；跳过用 `throw "[XTEST_SKIP]原因"` 表达（harness 据前缀区分）。

## 限制

- 测试文件**不要定义 `main`**，也**不要用 `import`**（断言以全局注入；import 需走标准 package 机制，与「拼接注入」不兼容）。
- 测试函数必须无参、以 `test_` 开头。
- 并发 `--jobs` 上限 8。

## 自带测试

`tests/` 下三个文件用于 selftest：
- `test_basic.xl` — 算术 / 字符串 / 比较
- `test_assertions.xl` — 覆盖每个断言（含 `assertThrows` / `approxEqual` / `skip`）
- `test_lifecycle.xl` — setup / teardown 生命周期

`selftest` 全部通过即 `EXIT=0`。
