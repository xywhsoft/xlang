# xlang 标准库 CRT 功能决策清单

更新日期：2026-09-22

本文只处理 CRT，用于当前 xlang 标准库设计决策。不展开 XRT/xruntime 函数，
也不对 XRT 分类作取舍。文中的“当前已有”以 `src/stdlib/xl6_stdlib_data.c`
公开注册表中的 CRT 直接符号和 CRT-derived 包装为准。

本文保留 2026-09-22 的决策输入快照；以下“当前已有”和 `round/llabs`
实现描述均指该日基线，不是 2026-09-23 之后的实时注册表。新增入口、
整数绝对值边界和声明式效果合同以《标准库设计.md》及
《标准库决策清单.md》为准。

## 一、当前 xlang 已有的 CRT 标准库功能

### 1.1 公开函数

当前只有 `math` 命名空间直接使用 CRT 数学函数：**16 个公开函数名，
17 个重载注册项**。

| xlang 函数 | CRT 符号/实现 | 现状 | 建议 |
|---|---|---|---|
| `math.sin` | `sin` | 已有 | 保留 |
| `math.cos` | `cos` | 已有 | 保留 |
| `math.tan` | `tan` | 已有 | 保留 |
| `math.asin` | `asin` | 已有 | 保留 |
| `math.acos` | `acos` | 已有 | 保留 |
| `math.atan` | `atan` | 已有 | 保留 |
| `math.atan2` | `atan2` | 已有 | 保留 |
| `math.sqrt` | `sqrt` | 已有 | 保留 |
| `math.pow` | `pow` | 已有 | 保留 |
| `math.exp` | `exp` | 已有 | 保留 |
| `math.log` | `log` | 已有 | 保留 |
| `math.log10` | `log10` | 已有 | 保留 |
| `math.floor` | `floor` | 已有 | 保留 |
| `math.ceil` | `ceil` | 已有 | 保留 |
| `math.round` | xlang 包装：正数 `floor(x+0.5)`，负数 `ceil(x-0.5)` | 已有 | 保留公开语义；后续决定是否改用 CRT `round` |
| `math.abs(float)` | `fabs` | 已有 | 保留 |
| `math.abs(int)` | `llabs` | 已有 | 保留；需明确最小负数边界 |

结论：**当前公开的 CRT 功能没有明确应删除的项，16 个函数名均建议保留。**

`math.pi/tau/e/nan/inf` 是 xlang 字面量常量，不是 CRT 函数，不计入本清单。

### 1.2 内部 CRT 依赖

| 内部入口 | CRT 符号 | 公开性 | 建议 |
|---|---|---|---|
| `__crt_string.strlen` | `strlen` | 私有桥接，不是用户 API | 最终移除直接 CRT 依赖；本轮不删除 |

当前有 41 个包装源码行共使用该入口 85 次。这是内部实现清理项，
不是 xlang 标准库公开功能变更。

### 1.3 当前没有直接 CRT 公开入口的已保留分类

下列分类已被允许进入或继续评估，但当前注册表没有相应的直接 CRT 公开函数：

- 原始内存与 C 字符串；
- 字符分类；
- 数字转换与通用算法；
- 浮点环境；
- 控制台；
- 环境变量；
- 复数与泛型数学；
- 编译器运行时。

## 二、之前已标记可进入 xlang 的 CRT 分类

| 决策 ID | CRT 来源 | 已确定方向 | 时序 |
|---|---|---|---|
| C04 | `<string.h>` | 只选有价值的原始内存/C 字符串能力 | 前置 |
| C05 | `<ctype.h>` | 选择一部分字符分类能力 | 前置 |
| C06 | `<stdlib.h>` | 数字转换与有价值的通用算法 | 前置 |
| C07 | `<math.h>` | 实数数学进入 xlang | 前置 |
| C08-FP | `<math.h>`、`<fenv.h>` | 浮点分类、舍入和环境能力 | 前置 |
| C09-CONSOLE | `<stdio.h>` | 只要控制台，不要 `FILE*` 文件流 | 前置 |
| C13-ENV | `<stdlib.h>` 及平台 CRT | 环境变量 API 进入 xlang | 前置 |
| C08-COMPLEX | `<complex.h>`、`<tgmath.h>` | 待 xlang 建立复数和数值泛型根基后进入 | 后置 |
| C02 | 编译器运行时 | 与 xlang 解释执行能力一并开发 | 最后 |

`C13-PROCESS` 进程生命周期尚未决定是否进入，因此不纳入下面的“建议新增”清单。

## 三、CRT 候选功能决策表

本节只列 CRT 功能。“建议入口”是 xlang 外观，不代表必须直接暴露 C 签名。

### 3.1 C04：原始内存与 C 字符串

| 候选 ID | CRT 函数 | 建议 xlang 功能 | 建议 |
|---|---|---|---|
| C04-01 | `memcpy`、`memmove` | 有范围检查的字节块 `copy`、`move` | 进入，但不公开裸指针签名 |
| C04-02 | `memset` | 字节范围 `fill` | 进入，但不公开裸指针签名 |
| C04-03 | `memcmp` | 字节序列 `compare` | 进入 |
| C04-04 | `memchr` | 字节查找 `indexOf` | 进入 |
| C04-05 | `strlen` | C FFI 的受限 `cString.length` | 只放显式 C 互操层，不进默认 string API |
| C04-06 | `strcmp`、`strncmp`、`strchr`、`strrchr`、`strstr` | C FFI 受限查询 | 默认不新增；仅在 C 指针互操时使用 |
| C04-07 | `strcpy`、`strncpy`、`strcat`、`strncat`、`strtok` | 无 | 不进入 xlang 标准库 |
| C04-08 | `strcoll`、`strxfrm` | 无 | 已决定弃用 locale 模型，不进入 |

### 3.2 C05：字符分类

这一组只建议定义为确定性 ASCII 能力，不把 C locale 语义冒充 Unicode 语义。

| 候选 ID | CRT 函数 | 建议 xlang 功能 | 建议 |
|---|---|---|---|
| C05-01 | `isalpha`、`isdigit`、`isalnum`、`isxdigit` | `ascii.isAlpha/isDigit/isAlphaNumeric/isHexDigit` | 进入 |
| C05-02 | `isspace`、`isblank` | `ascii.isWhitespace/isBlank` | 进入 |
| C05-03 | `iscntrl`、`isprint`、`isgraph`、`ispunct` | `ascii.isControl/isPrintable/isGraphical/isPunctuation` | 进入 |
| C05-04 | `islower`、`isupper` | `ascii.isLower/isUpper` | 进入 |
| C05-05 | `tolower`、`toupper` | `ascii.toLower/toUpper` | 进入 |
| C05-06 | locale-sensitive `ctype` 行为 | 无 | 不进入；ASCII 语义必须固定 |

### 3.3 C06：数字转换与通用算法

| 候选 ID | CRT 函数 | 建议 xlang 功能 | 建议 |
|---|---|---|---|
| C06-01 | `strtol`、`strtoll` | 有基数、溢出和完整消费语义的有符号整数 `parse/tryParse` | 进入 |
| C06-02 | `strtoul`、`strtoull` | 无符号整数 `parse/tryParse` | 进入 |
| C06-03 | `strtof`、`strtod` | 浮点 `parse/tryParse` | 进入 |
| C06-04 | `atoi`、`atol`、`atoll`、`atof` | 无 | 不进入；无法完整报告错误 |
| C06-05 | `abs`、`labs`、`llabs` | `math.abs` 整数重载 | 已有 `llabs` 路径；补齐其他整数宽度由类型设计决定 |
| C06-06 | `div`、`ldiv`、`lldiv` | `math.divRem` 或语言多返回值 | 可选；如 `/` 和 `%` 已足够则不增加 |
| C06-07 | `qsort` | 类型安全的 `sort/sortBy` | 功能进入，不公开 `void*` comparator ABI |
| C06-08 | `bsearch` | 类型安全的 `binarySearch` | 功能进入，不公开 `void*` comparator ABI |
| C06-09 | `rand`、`srand` | 无 | 不进入；全局状态、质量和并发语义不合格 |

### 3.4 C07：实数数学

已有函数不再新增：`sin/cos/tan/asin/acos/atan/atan2/sqrt/pow/exp/log/log10/floor/
ceil/round/abs`。

| 候选 ID | CRT 函数 | 建议 xlang 名 | 建议 |
|---|---|---|---|
| C07-01 | `sinh`、`cosh`、`tanh` | 同名 | 进入 |
| C07-02 | `asinh`、`acosh`、`atanh` | 同名 | 进入 |
| C07-03 | `erf`、`erfc` | 同名 | 进入 |
| C07-04 | `tgamma`、`lgamma` | `gamma`、`logGamma` | 进入 |
| C07-05 | `fma` | `fma` | 进入 |
| C07-06 | `copysign`、`nextafter` | `copySign`、`nextAfter` | 进入 |
| C07-07 | `frexp`、`modf` | 多返回值 `frexp`、`modf` | 进入，不公开输出指针 |
| C07-08 | `ldexp`、`scalbn` | `ldexp`、`scaleByPowerOfTwo` | 进入 |
| C07-09 | `logb`、`ilogb` | 同名 | 进入 |
| C07-10 | `remainder`、`remquo` | `remainder`、多返回值 `remQuo` | 进入 |
| C07-11 | `fdim` | `positiveDiff` | 进入 |
| C07-12 | `fmin`、`fmax` | `minNumber`、`maxNumber` | 可选；必须在名称上体现 NaN 语义 |
| C07-13 | `nan` | 无 | 不公开 payload-string 接口 |
| C07-14 | `nexttoward`、`scalbln`、所有 `*f/*l` 后缀变体 | 无独立名 | 不进入；交给 xlang 类型重载 |

### 3.5 C08-FP：浮点分类、舍入和环境

| 候选 ID | CRT 函数/宏 | 建议 xlang 功能 | 建议 |
|---|---|---|---|
| C08-01 | `fpclassify`、`isnormal`、`signbit` | `classify/isNormal/signBit` | 进入 |
| C08-02 | `isunordered` | `isUnordered` | 进入 |
| C08-03 | `isgreater/isgreaterequal/isless/islessequal/islessgreater` | 无 | 默认不增加；普通比较配合 `isUnordered` 足够 |
| C08-04 | `nearbyint`、`rint` | 按当前舍入模式取整 | 二选一；不同时暴露重复语义 |
| C08-05 | `lrint/llrint/lround/llround` | 无 | 不新增；浮点到整数交给 xlang 显式转换 |
| C08-06 | `fegetround`、`fesetround` | 获取/设置舍入模式 | 进入 |
| C08-07 | `feclearexcept`、`fetestexcept`、`feraiseexcept` | 清除/查询/触发浮点异常标志 | 进入 |
| C08-08 | `fegetexceptflag`、`fesetexceptflag` | 异常标志快照/恢复 | 可选；可由更高层环境快照取代 |
| C08-09 | `fegetenv`、`feholdexcept`、`fesetenv`、`feupdateenv` | 浮点环境保存、挂起、恢复和合并 | 进入，但需明确线程和嵌套语义 |

### 3.6 C09-CONSOLE：控制台

| 候选 ID | CRT 函数/流 | 建议 xlang 功能 | 建议 |
|---|---|---|---|
| C09-01 | `fputs/puts/fputc` + `stdout` | `console.write/writeLine` | 进入；公开签名不暴露 `FILE*` |
| C09-02 | `fputs/fputc` + `stderr` | `console.error/errorLine` | 进入 |
| C09-03 | `fflush(stdout/stderr)` | `console.flush` | 进入 |
| C09-04 | `fgets(stdin)` | `console.readLine` | 进入；需定义 EOF、换行和 UTF-8 语义 |
| C09-05 | `getchar` | `console.readChar` | 可选；必须区分字节与 Unicode code point |
| C09-06 | `printf/vprintf` | 无独立 CRT 外观 | 不直接公开；使用 xlang 格式化系统 |
| C09-07 | `scanf/vscanf` | 无 | 不进入；可组合行读取和类型解析 |
| C09-08 | 所有文件 `FILE*` API | 无 | 已决定不进入 |

### 3.7 C13-ENV：环境变量

| 候选 ID | CRT/平台函数 | 建议 xlang 功能 | 建议 |
|---|---|---|---|
| C13E-01 | `getenv` | `env.get`、`env.has` | 进入 |
| C13E-02 | `setenv` / `_putenv_s` | `env.set` | 进入；平台适配不泄漏到语言层 |
| C13E-03 | `unsetenv` / `_putenv_s(name, "")` | `env.remove` | 进入；需单独定义 Windows 空值语义 |
| C13E-04 | 无可移植 ISO C API | `env.entries`/`snapshot` | 不属于当前 CRT 决策；留待平台库阶段 |
| C13E-05 | `putenv` | 无 | 不直接公开；字符串寿命语义不适合 xlang |

### 3.8 C08-COMPLEX：后置的复数与泛型数学

本组已确定长期进入，但不进入当前开发队列：

- 基础：`cabs`、`carg`、`conj`、`cproj`、`creal`、`cimag`；
- 幂与对数：`cexp`、`clog`、`cpow`、`csqrt`；
- 三角：`csin`、`ccos`、`ctan`、`casin`、`cacos`、`catan`；
- 双曲：`csinh`、`ccosh`、`ctanh`、`casinh`、`cacosh`、`catanh`；
- `<tgmath.h>` 不形成第二套函数名，未来由 xlang 重载/数值泛型统一选择实数或复数实现。

### 3.9 C02：编译器运行时

已确定放在队列最后，并与 xlang 解释执行能力一并设计。当前不建立用户可见
标准库函数清单，仅保留为内部运行时任务。

## 四、仅限 CRT 的增删差分

### 4.1 建议保留

- 当前 16 个公开 `math` 函数名全部保留。
- `round` 保留当前“中点远离零”语义，是否替换底层实现单独决策。

### 4.2 建议新增

1. C04 安全字节操作：4 组；
2. C05 确定性 ASCII 分类/大小写：5 组；
3. C06 严格数字解析、排序和二分查找；
4. C07 实数数学：表 C07-01 至 C07-11；C07-12 可选；
5. C08 浮点分类和环境：C08-01、02、06、07、09；C08-04、08 可选；
6. C09 控制台：标准输出、标准错误、flush、行输入；字符输入可选；
7. C13 环境变量：get/has/set/remove。

### 4.3 建议不新增

- 危险 C 字符串写入和 `strtok`；
- locale-sensitive 字符分类与字符串比较；
- `atoi/atof` 家族、`rand/srand`；
- `nan(tag)`、long double 专用变体、数学函数的 `f/l` 后缀变体；
- `lrint/llrint/lround/llround` 等浮点到 C 整数的组合入口；
- `printf/scanf` 的 C 可变参数外观；
- 任何文件 `FILE*` API；
- `putenv` 的字符串寿命模型。

### 4.4 建议最终移除，但本轮不执行

- 私有 `__crt_string.strlen` 桥接；它不是公开 API，移除不应改变用户代码。

### 4.5 本轮不作决定

- `exit/_Exit/quick_exit/atexit/at_quick_exit/abort/system` 等进程生命周期函数；
- 复数/泛型数学的具体 xlang 类型和 API 签名；
- 编译器运行时的用户可见面。

## 五、建议的逐项决策顺序

1. C04 安全字节操作和 C 字符串边界；
2. C05 ASCII 字符分类；
3. C06 数字解析与通用算法；
4. C07 实数数学；
5. C08 浮点环境；
6. C09 控制台；
7. C13 环境变量；
8. 最后单独询问 C13-PROCESS 是否进入。

用户可直接使用候选 ID 作决策，例如：“C04-01 至 C04-04 进入，C04-05 只留 C FFI，
C04-06 至 C04-08 不进入”。
