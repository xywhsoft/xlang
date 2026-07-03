# DEMO6 功能范例套件

这个目录用于展示 demo6 目前已经实现的语言功能。每个 `.xl` 文件按主题拆分，尽量做到一种写法一个清晰示例，并通过 `host.c` 中的断言函数验证行为。

## 文件索引

- `00_language_basics.xl`：UTF-8、大小写敏感、中文标识符、注释、语句分隔、变量声明、typedef、字面量、time、bool、null。
- `01_expressions_control_flow.xl`：运算符、空合并、空赋值、安全访问、`in`、`if`、`while`、`do while`、`for range`、`break/continue`。
- `02_functions.xl`：普通函数、多返回值、可选参数、命名参数、`...`、`***`、函数重载。
- `03_types_and_values.xl`：`type` 值、类型转换、隐性类方法、`var` 动态值。
- `04_containers.xl`：array/list/set/dict 字面量、索引、遍历、静态泛型容器、容器保存 class/custom/function。
- `05_generics.xl`：泛型函数、显式 `::<T>`、类型推断、泛型 class、嵌套泛型、泛型别名、泛型与重载。
- `06_function_values_closures.xl`：function 一等公民、匿名函数、闭包、多返回值函数签名、可变参数函数签名、var 动态调用。
- `07_class_custom.xl`：class、继承、构造/析构、方法重载、custom C 句柄绑定、custom 进入 var。
- `08_c_interop_exports.xl`：extern 函数/变量、export 函数/变量、多返回值 ABI。
- `09_c_export_abi.xl`：纯 export ABI 示例，用于生成头文件并由 C 宿主调用。
- `10_errors_docs.xl`：异常处理、断言和 `#doc ... #end` 文档注释。
- `11_concurrency.xl`：async/await、task.all/race/spawn、async generator/for await、generator/yield、channel、thread、mutex、sem、cond、rwlock。
- `12_union.xl`：顶层具名 union、union 传参与返回、class 内匿名 union 和嵌套 union 字段访问。
- `host.c`：供运行范例使用的 C 宿主函数。
- `host_exports.c`：C 调用 `09_c_export_abi.xl` 导出符号的宿主示例。
- `run_feature_suite.bat`：一键执行检查、运行和 C ABI 导出验证。

## 常用命令

在 `demo6` 目录下运行：

```bat
release\xl.exe check release\examples\feature_suite\00_language_basics.xl
release\xl.exe run release\examples\feature_suite\00_language_basics.xl --link release\examples\feature_suite\host.c
```

`release\xl.exe run` 会使用 DEMO6 的默认 XRT/TCC 链接环境；需要覆盖工具链路径时，先设置 `XL6_TCC`、`XL6_XRT_OBJ`、`XL6_TCC_LIBTCC1` 等环境变量，或通过 `tools\xl6_toolchain_env.bat` 统一调整。

批量检查：

```bat
for %f in (release\examples\feature_suite\*.xl) do release\xl.exe check "%f"
```

`09_c_export_abi.xl` 是纯导出库，没有 `main`，不直接使用 `run`。导出 ABI 示例需要先生成 C 与头文件，再用 `host_exports.c` 编译链接。生成头文件的示例命令：

```bat
release\xl.exe cgen release\examples\feature_suite\09_c_export_abi.xl --emit release\tmp\feature_exports\09_c_export_abi.c --emit-header release\tmp\feature_exports\09_c_export_abi.h
```

一键回归：

```bat
release\examples\feature_suite\run_feature_suite.bat
```
