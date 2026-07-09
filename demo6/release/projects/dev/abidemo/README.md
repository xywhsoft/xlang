# abidemo — ABI 元数据验证工具

测试 `xl abi` 命令与 C ABI 导出能力（DEMO6功能清单.md §15、§16）。

工具自身既是**被测对象**（源码顶部声明了各种 export/extern 形式），又是**验证驱动**
（运行时调用 `xl abi` / `xl cgen --emit-header` 子进程，解析 JSON / TXT / C 头文件产物，
逐项校验符号完整性、kind、cExport、类型映射、class owner、多返回值 out 指针等）。

## 覆盖的 ABI 能力

| # | 能力 | 本工具覆盖位置 |
|---|---|---|
| 1 | `export "c_sym" func name(...) as T` 导出函数 | `xlAdd` / `xlMul` |
| 2 | `export "c_sym" int name = value` 导出变量 | `xlCounter`（运行时读/写/复合赋值） |
| 3 | `export "c_sym" const int name = value` 导出常量 | `XL_VERSION` |
| 4 | `export "c_sym" string name = value` 导出字符串变量 | `xlGreeting` |
| 5 | class 成员 `export "c_sym" func method` 导出 | `Gauge.add` |
| 6 | class 多返回值成员导出（→ C out 指针） | `Gauge.pair() as (int,int)` |
| 7 | `extern "c_sym" Type name` extern 变量声明 | `xlExternVar`（仅元数据，运行时不调用） |
| 8 | `extern "c_sym" func name(...) as T` extern 函数声明 | `xlExternFn`（仅元数据） |
| 9 | `xl abi --json --emit file.abi.json` 生成 JSON ABI | `testAbiJson()` |
| 10 | `xl abi --emit file.abi.txt` 生成 TXT ABI | `testAbiTxt()` |
| 11 | `xl cgen --emit-header file.h` 生成 C 头文件 | `testHeader()` |
| 12 | 运行时自检：导出符号在 xlang 侧可正确调用 | `testRuntimeExports()` |

## 文件

- `abidemo.xl` — 主工具（被测声明 + 验证驱动合一）。
- `abidemo.exe` — `xl build` 产物（可选，用于验证 build 路径）。

## 运行与验证

```bat
cd /d D:\GIT\x-lang\demo6\release

:: 主工具（两条路径都必须 EXIT=0）
xl.exe run   projects\dev\abidemo\abidemo.xl
xl.exe build projects\dev\abidemo\abidemo.xl -o projects\dev\abidemo\abidemo.exe
projects\dev\abidemo\abidemo.exe
```

run 与 build 两条路径均通过，61 项检查全 PASS。

## ABI 元数据格式实测要点

### `xl abi --json` 符号 kind 枚举

| kind | 出现条件 | 关键字段 |
|---|---|---|
| `export_func` | `export "c" func ...` | `name`、`signature`、`symbol`、`cExport` |
| `export_var` | `export "c" Type name = value` | `name`、`type`、`cExport` |
| `export_const` | `export "c" const Type name = value` | `name`、`type`、`cExport` |
| `export_class_method` | class 成员 `export "c" func method` | `owner`、`name`、`signature`、`symbol`、`cExport` |
| `extern_var` | `extern "c" Type name` | `name`、`type`、`cSymbol` |
| `extern_func` | `extern "c" func name(...)` | `name`、`signature`、`cSymbol` |
| `func` | 普通 `func`（含 `main`、注入的 std 函数） | `name`、`signature`、`symbol` |

注意 `extern_*` 用 `cSymbol` 字段，`export_*` 用 `cExport` 字段——解析时必须按 kind 分流。

### `xl cgen --emit-header` 类型映射

| xlang 类型 | C 头文件类型 | 备注 |
|---|---|---|
| `int` / `int64` | `int64_t` | 默认整数类型，64 位 |
| `string` (export var) | `const char*` | 字符串导出为只读 C 字符串 |
| `int` export var | `extern int64_t` | 带 `extern` |
| `const int` export | `extern const int64_t` | 带 `extern const` |
| class 成员方法首参 | `XL_Class_<Name>* self` | 自动注入 self 指针 |
| 多返回值 class 方法 | `void` 返回 + `int64_t* out0, int64_t* out1` | 多返回值映射为 out 指针 |
| class 字段 | `typedef struct XL_Class_<Name> { ... }` | 结构体定义含字段布局 |

## 关键设计点

- **自洽验证**：工具自身的 export/extern 声明就是被测对象。运行时通过 `arg[0]`（或相对
  路径回退）定位到自己的 `.xl` 源码，再调用 `xl abi` / `xl cgen` 重新解析自身。
- **extern 声明不调用**：`xlExternVar` / `xlExternFn` 只出现在 ABI 元数据里，运行时绝不
  使用——否则 TCC 会报 `unresolved reference`。这是测试「extern 声明进 ABI」的唯一安全方式。
- **子进程调用**：用 `process.run(cmd, 30000)` 调 `xl.exe`，从 `ProcessResult.exitCode` /
  `r.stdout` 取输出；带 `--emit file` 时直接 `file.readText` 读产物，避免 stdout 编码问题。
- **build 路径兼容**：`xl build` 出来的 exe，`arg[0]` 是 exe 路径而非 `.xl`。`findSelfSource`
  依次尝试 `arg[0]`（.xl 后缀）→ `cwd\projects\dev\abidemo\abidemo.xl` → `appDir\abidemo.xl`。

## 发现的 BUG / 手感问题

见 `projects\dev\BUGS.md` 末尾「abidemo 工具发现」一节：
- **手感-ABI1**：`arg` 在非变参函数中访问报误导性错误「class field receiver is not a class value」。
- **手感-ABI2**：`array<T> = string.split(...)` 不支持（typed 容器不能从 stdlib 动态数组直接赋值）。
