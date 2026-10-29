# docgen

`docgen` 是 `#doc ... #end` 文档块的可执行验证范例，覆盖文档附加、字段校验、泛型、class 方法和错误诊断。

## 运行

在 `demo6` 目录运行：

```bat
release\xl run release\projects\dev\docgen\docgen.xl
release\xl build release\projects\dev\docgen\docgen.xl -o build\docgen.exe
build\docgen.exe
```

也可以在 `release` 目录使用：

```bat
xl run projects\dev\docgen\docgen.xl
```

程序会定位当前构建的 `xl.exe`，并通过 `process.runFile()` 执行 10 个独立负例。任何断言、负例、文件定位或编译器调用失败都会返回非零退出码，不会静默跳过。

## 覆盖范围

- 全局变量与常量文档。
- 普通函数、无返回值函数和泛型函数文档。
- `desc`、`param`、`return`、`note`、`example` 字段。
- 多行字段正文和仅作为文本保存的代码围栏。
- class、成员函数、构造函数及 `derive value`。
- 文档块不影响 JSON 序列化和反序列化。
- 未知字段、裸文本、重复字段、参数名不匹配等语义错误。
- 未闭合和孤立文档块等语法错误。

## 目录

- `docgen.xl`：合法功能和测试驱动程序。
- `cases/`：必须被 `xl check` 拒绝的独立负例。

预期结果为 28 项检查全部通过，进程退出码为 0。
