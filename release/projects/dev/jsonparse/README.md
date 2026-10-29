# jsonparse - 从零手写的 JSON 解析器

不依赖 `json` 标准库，纯 xlang 实现的完整 JSON 解析器（RFC 8259 子集）。

## 架构

| 组件 | 实现 |
|---|---|
| **Lexer**（词法分析器） | `class Lexer`（`derive value`）。`next()` 跳空白后产出 Token：`{ } [ ] : ,` / `string` / `number` / `lit`(true/false/null) / `eof`。字符串转义 `\" \\ \/ \b \f \n \r \t \uXXXX`（含 UTF-16 代理对合并）。数字按 RFC 文法校验（禁前导零、`.` 后必跟数字、指数必跟数字）。 |
| **Parser**（递归下降解析器） | `class Parser`（持有 `Lexer lex` + `Token cur`）。`parseValue → parseObject / parseArray / string / number / lit`，object 校验 string key，array/object 拒绝尾逗号。 |
| **数据模型** | 用 `var` 动态类型：object → `var dict {}`，array → `var []`，number（无 `./e/E`）→ `int`，否则 → `float`，true/false/null → 对应类型。 |
| **Stringifier**（序列化器） | 手工递归 `stringifyValue`，支持 compact 与 pretty（2 空格缩进）。dict 保留插入序（避开标准库 pretty 模式的 TAB 缩进与键序 hash 重排问题）。 |
| **Query** | 点路径 `.key.sub[0]`，`.` 或空表示整个文档。 |
| **Unicode** | `\uXXXX` 经 `codepointToUtf8`（手写 UTF-8 编码 + `encoding.fromBytes`）转为字符；高代理自动合并紧随的低代理。 |

## 命令

```
jsonparse parse <file.json> [-p]      解析并重新输出（compact；-p = pretty）
jsonparse query <file.json> <path>    路径查询（.key.sub[0]）
jsonparse valid <file.json>           验证合法性
jsonparse roundtrip <file.json>       parse -> stringify -> parse 一致性
```

## 运行

```bat
cd /d D:\GIT\x-lang\release
xl run projects\dev\jsonparse\jsonparse.xl -- parse projects\dev\jsonparse\tests\nested.json -p
xl build projects\dev\jsonparse\jsonparse.xl -o projects\dev\jsonparse\jsonparse.exe
```

## 测试

`tests/` 下覆盖：简单/嵌套(10 层)/Unicode(含代理对 emoji)/大数字精度/空白/标量顶层，
以及错误集（缺逗号/未闭合/尾逗号/非法转义/前导零）。

## 关键设计 / 绕过

- **Lexer / Token 必须 `derive value`**：作为另一个 class 的成员字段时，普通 class 触发
  `[checked] class persistence requires __toValue__ or derive value/json/xlon`。详见根 `BUGS.md` BUG-JP1。
- **不依赖 `json` 标准库**：解析、转义、序列化、查询全程手写。
- **手写 UTF-8 编码**：标准库无 codepoint→char，故用 `Buffer` 拼 UTF-8 字节再 `encoding.fromBytes`。
- **手写十六进制解析**：`int()` 不接受进制参数，自写 `parseHex4`。
- **位运算用 `& | <<`**：`and/or` 是逻辑运算符。
