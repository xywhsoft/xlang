# tpl - 模板渲染工具

用 `std.template` 标准库把模板文件 / 模板字符串 + JSON 数据渲染成文本。
用于验证 `template.compile` / `template.render` / `template.renderFile` / `Template` 类。

## 用法

```
tpl render      <template_file> <data.json> [output] [--bracket=STR]
tpl render-text <template_string> <data.json> [--bracket=STR]
tpl demo
```

- `render`：从模板文件渲染（走 `template.renderFile`，XRT UTF-8 文件层）。
- `render-text`：从命令行模板字符串渲染（走 `Template` 类：`template.compile` + `render`）。
- `demo`：内置模板与数据，展示各类插值与统计信息。
- 不指定 `output` 时渲染结果写 stdout（`print`，不加尾换行，保持模板原文）。

## 模板语法（XRT xte 引擎）

| 写法 | 含义 |
|---|---|
| `{$name}` | 变量插值 |
| `{$items[0]}` | 数组下标插值 |
| `{#if:flag}A{#else}B{#end}` | 条件分支 |
| `{#foreach:items}...{#end}` | 遍历数组 / 列表 / 字典 |

`foreach` 循环体内可用的变量：

- `loop.index`：当前序号（从 0 开始）
- `loop.number`：当前序号（从 1 开始）
- `loop.value`：当前元素
- `loop.key`：当前键
- `loop.first` / `loop.last`：是否为首项 / 末项
- `loop.depth`：当前循环嵌套深度
- 若元素本身是对象，其字段也会直接进入作用域（例如遍历 `users` 时可直接写 `{$name}`）

## 定界符（bracket）

默认 `{}`，即 `{$var}`。用 `--bracket=STR` 改写（偶数长度，前半开、后半闭）：

| `--bracket` | 模板写法 |
|---|---|
| `{{}}` | `{{$var}}` |
| `<>` | `<$var>` |
| `(())` | `(($var))` |

## 示例

```
> tpl demo
=== tpl demo: std.template showcase ===
[1] scalar var       => Title: xlang (v6)
[2] Template class   => <h1>xlang</h1>
[3] if/else          => Visible: yes
[4] foreach objects  => Users: Ada; Lin;
[5] foreach scalars  => Items: [0:alpha][1:beta][2:gamma]
[6] array index      => First item: alpha
[7] bracket {{}}     => Double brace: xlang
[8] bracket <>       => Angle: xlang

> tpl render test.tpl test.json
> tpl render test.tpl test.json out.txt
> tpl render-text 'Hello {$title}' test.json
Hello xlang
> tpl render-text '<$title>' test.json --bracket=<>
xlang
```

## 运行 / 构建

```
cd /d D:\GIT\x-lang\demo6\release
xl run  projects\dev\tpl\tpl.xl -- <subcommand> <args>
xl build projects\dev\tpl\tpl.xl -o projects\dev\tpl\tpl.exe
```

`xl run` 与 `xl build` + exe 两条路径均验证通过（EXIT=0）。

## 验证结论

`std.template` 全部能力正常工作：`template.compile(text)` / `compile(text, open, close)`、
`template.render(text, data)` / `render(text, data, open, close)`、`template.renderFile(path, data, open, close)`、
`Template` 类的 `render(data)` 与统计属性 `nodeCount` / `exprCount` / `argCount` / `stringPoolSize`，
以及 `{$var}` 插值、`{#if}` 分支、`{#foreach}` 遍历（含 `loop.index` / `loop.value` / `loop.key`）、
数组下标 `{$items[0]}`、自定义定界符 `{{}}` / `<>` 等。

> 注意：PowerShell 调试时双引号字符串会把 `$title` 当 PS 变量展开成空串，
> 传模板参数要用单引号（`'Hello {$title}'`）。这是 shell 行为，与 xlang / 模板引擎无关。
