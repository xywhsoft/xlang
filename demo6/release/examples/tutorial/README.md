# DEMO6 教学范例

这个目录用于教学，不是从 DEMO5 机械复制的回归集。

每个范例只讲一个小主题，成功时 `main` 返回 `0`，失败时返回非零错误码，方便直接运行和批量回归。

## 学习顺序

- `01_entry_and_layout.xl`：程序入口、语句分隔、注释、中文标识符。
- `02_variables_and_literals.xl`：类型前置声明、`const`、`auto`、`var`、数字/字符串/time 字面量。
- `03_operators.xl`：算术、复合赋值、位运算、逻辑、三元、空合并、安全访问。
- `04_control_flow.xl`：`if`、`while`、`do while`、`for in range`、多层 `break/continue`。
- `05_functions.xl`：普通函数、多返回值、可选参数、命名参数、`...`、`***`、重载。
- `06_function_values_and_closures.xl`：`function` 类型、匿名函数、箭头函数、闭包、函数值容器。
- `07_containers.xl`：array/list/set/dict、索引、遍历、typed container。
- `08_generics.xl`：泛型函数、`::<T>`、推断、多类型参数、泛型 class。
- `09_class_lifecycle.xl`：class 字段、构造、继承、base 调用、析构顺序。
- `10_c_interop_and_custom.xl`：extern C 函数、extern C 变量、native-backed class C 句柄类。
- `11_export_to_c.xl`：xlang 导出 C ABI，由 C 宿主调用。
- `12_errors_and_docs.xl`：字符串异常、`try/catch/finally`、`assert` 和 `#doc ... #end`。

## 运行

在 `demo6` 目录下：

```bat
test_tutorial.bat
```

单个范例也可以直接运行：

```bat
release\xl.exe run release\examples\tutorial\05_functions.xl
```

涉及 C 宿主的范例需要链接 `host.c`：

```bat
release\xl.exe run release\examples\tutorial\09_class_lifecycle.xl --link release\examples\tutorial\host.c
release\xl.exe run release\examples\tutorial\10_c_interop_and_custom.xl --link release\examples\tutorial\host.c
```

`11_export_to_c.xl` 是导出库示例，不直接 `run`，由 `test_tutorial.bat` 生成 C/H 后和 `host_export.c` 一起编译验证。
