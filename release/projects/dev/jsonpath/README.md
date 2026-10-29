# jsonpath - JSONPath 查询引擎

用 xlang 实现的 [JSONPath](https://goessner.net/articles/JsonPath/) 查询引擎：从 JSON 文件读取 → `json.parse` → var 树 → 路径遍历 → 结果输出。

## 用法

```bat
cd /d D:\GIT\x-lang\release

:: 查询
xl run projects\dev\jsonpath\jsonpath.xl -- query projects\dev\jsonpath\sample.json "$.store.book[0].title"
xl run projects\dev\jsonpath\jsonpath.xl -- query projects\dev\jsonpath\sample.json "$..author"
xl run projects\dev\jsonpath\jsonpath.xl -- query projects\dev\jsonpath\sample.json "$.store.book[?(@.price<10)]"

:: 自检（29 项断言）
xl run projects\dev\jsonpath\jsonpath.xl -- test

:: 构建可执行
xl build projects\dev\jsonpath\jsonpath.xl -o projects\dev\jsonpath\jsonpath.exe
jsonpath query sample.json "$..price"
jsonpath test
```

## 支持的路径语法

| 语法 | 含义 |
|---|---|
| `$` | 根节点 |
| `$.key` | 点访问子节点 |
| `$['key']` / `$["key"]` | 括号访问子节点 |
| `$[idx]` | 数组下标（支持负数，`$[-1]` 倒数第一） |
| `$[start:end]` | 切片（可省略：`$[1:]`、`$[:3]`、`$[-2:]`） |
| `$[*]` / `$.*` | 通配符（数组全部元素 / 对象全部值） |
| `$..key` | 递归下降：任意深度的 `key` 子节点 |
| `$..[idx]` / `$..*` | 递归下降 + 下标 / 通配 |
| `$[?(expr)]` | 过滤表达式 |
| `$[0,1,2]` | 多下标 |
| `$['a','b']` | 多键 |

## 过滤表达式

```
@                  当前候选节点
@.field            当前节点字段
@.a.b[0]           嵌套路径
$.x.y              从根开始的路径（用于跨节点比较）
< > <= >= == !=    比较（数值或字符串）
&&  ||  !          逻辑与 / 或 / 非
( ... )            分组
@.field            存在性测试（字段存在即为真）
```

示例：
- `$.store.book[?(@.price<10 && @.category=="fiction")]`
- `$.store.book[?(!@.isbn)]`
- `$.store.book[?(@.price < $.store.bicycle.price)]`

## 输出规则

- 0 个匹配 → `[]`
- 1 个匹配 → 直接打印该值（容器美化 2 空格缩进，标量紧凑）
- 多个匹配 → 打印为 JSON 数组（美化）

## 实现要点

- **路径解析**：`PathParser` 把路径解析为 segment 列表（var dict），逐段应用产生候选集合。
- **递归下降**：`..` 展开为 segment，把当前候选集扩展为「自身 + 所有后代」（`collectAll` 用 var 数组 + 局部 `push`，不在递归热路径分配 dict，规避 BUG-LP1）。
- **过滤表达式**：`FParser` 递归下降解析为 AST（var dict 树：`or/and/not/cmp/exists`），对每个候选元素求值。`Missing` 哨兵 class 区分「路径不存在」与「合法 JSON null」。
- **序列化**：手工实现（`stringifyValue`），避免 json 库 pretty 模式 TAB 缩进 / 键序重排问题。
- **var 容器按引用共享**：函数参数为 `var` 容器时 `push` 会反映到调用方（实测确认；与 typed 容器 BUG-XL1 的值拷贝语义不同）。

## 触及的已知限制（绕过方式见实现）

- BUG-LP1：递归热路径不分配 var dict（`collectAll` 只用 var 数组 push）。
- BUG-XT4 #4：var 比较只直接放 `if` 条件（`evalCmp` 用早返回写法）。
- BUG-JP2：var 数组负数下标返回 null（`applyIndex`/`resolveValue` 手工归一化 `len+idx`）。
- BUG-S1：未使用 `string.splitLines`，不受尾部空串影响。
