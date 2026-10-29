# cmix — C 混编验证工具

全面压测 xlang ↔ C 的互操作能力。这是 `dev` 项目群里专门覆盖 C 混编 / ABI / 导出三大方向的最小可用验证集。

## 目标

覆盖 `DEMO6功能清单.md` 第 15 节「C 混编、ABI 与导出」和第 11 节 native-backed class 的全部公开能力：

| # | 能力 | 本工具覆盖位置 |
|---|---|---|
| 1 | `extern "c_fn" func name(...) as T` 调用 C 函数 | `cAdd/cMul/cMax/cAverage/cSquareLL`（`as int`/`float`/`int64`） |
| 1b | C ABI 类型精确匹配（`int32`/`c_int` → native int） | `cNativeAdd(int32,...) as int32`、`cNativeMul(c_int,...) as c_int` |
| 2 | `extern "c_var" Type name` 读 C 全局变量 | `cMagic`（含在表达式中使用） |
| 3 | `export "c_sym" func name(...) as int` 导出 xlang 函数到 C ABI | `xDouble/xTriple/xSum` |
| 4 | `export "c_sym" int name = value` 导出 xlang 变量 | `xHits`（读写 + 复合赋值） |
| 5 | `export extern "c_var" Type name` 公开 C 全局变量 | `hostCounter`（读 + 写穿透到 C 存储） |
| 6 | native-backed class 包装 C opaque handle | `Counter`（open/close/add/value/ops/reset） |
| 7 | `.xl + .c` 混合构建 | `xl build cmix.xl cmix_native.c` |
| 8 | C → xlang 反向回调 | `cmix_callback_demo/chain` 在 C 中调用 xlang 导出 |

## 文件

- `cmix.xl` — 主工具，按 7 个 section 逐项自检并打印 `[ok]/[FAIL]`，失败数即返回码。
- `cmix_native.c` — C 侧：数学函数、全局变量、opaque counter 句柄、反向回调。
- `probe.xl` / `probe_c.c` — 边界探针：plain `extern` 变量写穿透、C 读 xlang 导出变量/函数、native-backed class 混合普通 xlang 字段与方法、多实例隔离。
- `probe2.xl` / `probe2_c.c` — 类型映射探针：`int`↔`int64_t` 与 `int32`↔`int` 的 ABI 行为、`export const` 读取。

## 运行与验证

```bat
cd /d D:\GIT\x-lang\release

:: 主工具（两条路径都必须 EXIT=0）
xl.exe run   projects\dev\cmix\cmix.xl projects\dev\cmix\cmix_native.c
xl.exe build projects\dev\cmix\cmix.xl projects\dev\cmix\cmix_native.c -o projects\dev\cmix\cmix.exe
projects\dev\cmix\cmix.exe

:: 边界探针（可选）
xl.exe run   projects\dev\cmix\probe.xl  projects\dev\cmix\probe_c.c
xl.exe run   projects\dev\cmix\probe2.xl projects\dev\cmix\probe2_c.c
```

## 关键类型映射（重要）

经 `xl cgen --emit-header` 实测，xlang 类型与 C 类型的对应关系如下，**C 侧函数签名必须匹配**，否则会出现静默的 ABI 错配（见 BUGS.md `手感-CMIX1`）：

| xlang 类型 | C 类型 | 说明 |
|---|---|---|
| `int` / `int64` | `int64_t` / `long long` | 64 位，**默认整数类型** |
| `int32` / `c_int` | `int` | 原生 32 位 C int |
| `float` / `float64` | `double` | xlang `float` 即 f64 |
| `point` | `void*` | 指针 / opaque handle |

> 规则：声明 `extern "c_fn" func f(int a) as int` 时，C 函数必须是 `int64_t f(int64_t a)`。
> 要调用真正的 native-int C 函数（如 libc `int`），xlang 侧必须用 `int32` 或 `c_int`。

## 设计要点

- **存储归属**：`export "sym" Type name = value` 由 xlang 拥有存储；C 侧**不得重复定义**同名符号（否则 TCC 报 `defined twice`），需要读取时在 C 侧声明 `extern`。`export extern "sym" Type name` 反过来，存储归 C，xlang 只是把 C 全局变量以公开 package 符号重新导出（可读可写）。
- **反向回调**：C 函数调用 xlang 导出时，xl 的 csrc wrapper 会先把生成的 `module.h` include 进来，其中已声明了所有 export 符号（`int64_t cmix_xlang_double(int64_t x)`）。C 侧可再写一条 `extern long long cmix_xlang_double(long long x)` 的兼容重声明（`long long == int64_t`），或直接依赖头文件。
- **native-backed class**：`extern "open" create(seed)` 的 C 函数返回 `void*`（成为 `handle`）；`destroy` 接收 handle；成员 `func` 的第一个 C 参数自动是 handle。可混合普通 xlang 字段与方法（见 probe.xl 的 `MixedCounter`）。
