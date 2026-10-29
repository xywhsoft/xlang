# rxgrep — 正则表达式提取器

用 `regex.compile` 预编译模式，从文件中按正则提取匹配。支持同一行内多个非重叠匹配的完整枚举。

## 用法

```bat
cd /d D:\GIT\x-lang\release
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

## 匹配位置语义

- `RegexMatch.byteStart/byteEnd` 是字节偏移，适合无额外转换地推进下一次匹配。
- `RegexMatch.start/end` 是按 UTF-8 codepoint 计算的字符索引，首次属性访问时懒计算。
- `RegexMatch.text` 首次访问时创建选区字符串并缓存。
- 空匹配用 `byteStart == byteEnd` 识别，迭代器会主动推进，避免停在同一位置。
- `-i` 支持普通模式和交替组；`ERROR|error` 这类折叠后重复的分支也可正常匹配。

## 测试数据

- `data.log` — 10 行服务器日志（含 IP、邮箱、时间戳、级别）
- `utf8.txt` — 6 行含中文的 UTF-8 文本（验证多字节迭代）
