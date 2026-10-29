# jq - 简化版 JSON 查询器

用点路径表达式查询 JSON 数据（类似 jq 的极简子集）。

## 用法

```
jq <query> <file.json>     从文件读取 JSON 并按路径查询
jq <query> -               stdin 模式 (当前 xlang stdlib 无 stdin API, 见下文限制)
```

## 查询语法

| 表达式 | 含义 |
|---|---|
| `.` | 整个文档（美化输出） |
| `*` | 整个文档（美化输出） |
| `.key` | 对象字段 |
| `.key.subkey` | 深层访问 |
| `[0]` | 数组元素 |
| `.key[0].name` | 混合路径 |

## 输出规则

- 标量（字符串/数字/布尔/null）→ JSON 文本，字符串带引号（与 jq 一致）。
- 容器（对象/数组）→ 美化输出，2 空格缩进。

## 示例

```
> jq .name test.json
"xlang"
> jq .version test.json
6
> jq .tags[0] test.json
"a"
> jq .author.name test.json
"dev"
> jq .modules[1].size test.json
512
> jq . test.json
{
  "name":  "xlang",
  ...
}
```

## 运行 / 构建

```
cd /d D:\GIT\x-lang\release
xl run  projects\dev\jq\jq.xl -- <query> <file.json>
xl build projects\dev\jq\jq.xl -o projects\dev\jq\jq.exe
```

`xl run` 与 `xl build`+exe 两条路径均验证通过。

## 已知限制

- **不支持 stdin**：DEMO6 功能清单第 14 节标准库只暴露 `print`/`println`，没有
  `stdin`/`readLine`/`input` 等控制台输入 API（`std.io.read_line`/`read_all` 仅存在于
  设计草案 `xlang标准库完整设计方案.txt`，未实现）。传 `-` 会提示改用文件。详见
  `../BUGS.md` BUG-007。
- **美化输出 dict 字段顺序与源文件不同**：`json.stringify(pretty=true)` 按内部哈希顺序
  输出对象字段，不保留源 JSON 的书写顺序（真 jq 会保留）。属 xlang stdlib 行为。
- **美化输出冒号后双空格 / 数组尾括号换行**：源自 `json.stringify` pretty 用 TAB 缩进
  并被本工具替换为 2 空格后的副作用（见 `../BUGS.md` BUG-002）。纯外观，不影响数据。
