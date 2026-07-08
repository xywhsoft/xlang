# rxgrep — 正则表达式提取器

用 `regex.compile` 预编译模式，从文件中按正则提取匹配。支持同一行内多个非重叠匹配的完整枚举。

## 用法

```bat
cd /d D:\GIT\x-lang\demo6\release
xl run projects\dev\rxgrep\rxgrep.xl -- <pattern> <file> [options]
:: 或 build 后:
projects\dev\rxgrep\rxgrep.exe <pattern> <file> [options]
```

| 选项 | 含义 |
|---|---|
| `<pattern>` | 正则模式（位置参数，必填） |
| `<file>` | 待搜索文件（位置参数，必填） |
| `-o` | 只输出匹配部分（默认输出整行 + 每个匹配） |
| `-c` | 只统计匹配数（不打印行/匹配） |
| `-n` | 带行号 |
| `-i` | 大小写不敏感（**注意 BUG-28 限制**） |
| `-h` | 帮助 |

## 示例

```bat
rxgrep "\d+\.\d+\.\d+\.\d+" data.log              :: 提取所有 IP
rxgrep "\d+\.\d+\.\d+\.\d+" data.log -o            :: 只打印 IP
rxgrep "\d+\.\d+\.\d+\.\d+" data.log -c            :: 统计个数
rxgrep "[0-9]+" utf8.txt -o -n                     :: 多字节文本 + 行号
rxgrep "[a-z]+@[a-z]+\.[a-z]+" utf8.txt -o -n      :: 提取邮箱
```

## 退出码

- `0`：至少一处匹配
- `1`：无匹配 / 用法错误 / 文件不存在 / 读文件失败

## 验证

`xl run` 与 `xl build`+exe 两条路径输出**逐字节一致**，匹配数与 PowerShell `[regex]::Matches` 逐项对齐（data.log IP=6、utf8.txt `[0-9]+`=15、邮箱=1）。

## 触发的 BUG（见 dev/BUGS.md）

- **BUG-25**：`RegexMatch.start` 在行首匹配（byteStart==0）时返回串长而非 0。→ 用 `.byteStart`/`.byteEnd` 代替。
- **BUG-26**：空匹配的 `.text` 返回 `s[pos:]` 尾串、`.start`/`.end` 错误。→ 用 `byteStart==byteEnd` 判定并跳过空匹配。
- **BUG-27**：`Regex.match(s, startPos)` 的 startPos 是**字节偏移**而非字符偏移。→ 迭代全程用 `mm.byteEnd` 推进。
- **BUG-28**：交替组（`|`）+ `-i`，当分支大小写折叠后重复（如 `ERROR|error`、`Cat|cat`）时 XRT 断言崩溃（exit 3，不可 catch）。

### `-i` 使用限制（BUG-28）

`-i` 对**简单模式**和**折叠后不重复的交替组**（`Cat|Dog`、`foo|bar`）正常。但对**折叠后重复的交替组**会直接崩溃：

```bat
rxgrep "ERROR|error" data.log -i     :: 崩溃 (exit 3) —— 折叠后都是 "error"
```

需要此类搜索时改用字符类写法：

```bat
rxgrep "[Ee][Rr][Rr][Oo][Rr]" data.log     :: 等价于 "ERROR|error" + -i，且不崩
```

## 测试数据

- `data.log` — 10 行服务器日志（含 IP、邮箱、时间戳、级别）
- `utf8.txt` — 6 行含中文的 UTF-8 文本（验证多字节迭代）
