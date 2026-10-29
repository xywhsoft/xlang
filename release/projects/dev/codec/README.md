# codec - Base64 / Hex / URL percent codec and crypto hash tool

一个命令行编解码器：Base64、HEX、URL percent 编解码，以及 SHA1/256/384/512 哈希。支持从文件读取输入。

## 用法

```
codec b64enc <text>            Base64 编码
codec b64dec <base64>          Base64 解码
codec hexenc <text>            HEX 编码（大写）
codec hexdec <hex>             HEX 解码（大小写均可）
codec pctenc <text>            URL percent 编码
codec pctdec <text>            URL percent 解码
codec hash <algo> <text>       哈希：sha1 / sha256 / sha384 / sha512（大写 HEX）

  -f <file>                    从 UTF-8 文件读取输入，可用于任何文本命令：
  codec b64enc -f <file>
  codec hash sha256 -f <file>

  codec --help | -h            帮助
```

## 运行与构建

```bat
cd /d D:\GIT\x-lang\demo6\release
xl run projects\dev\codec\codec.xl -- b64enc "Hello, World!"
xl build projects\dev\codec\codec.xl -o projects\dev\codec\codec.exe
projects\dev\codec\codec.exe hash sha256 "test"
```

`xl run` 与 `xl build`+exe 两条路径均全功能通过，输出一致。

## 验证输出（示例）

| 命令 | 输出 |
|---|---|
| `b64enc "Hello, World!"` | `SGVsbG8sIFdvcmxkIQ==` |
| `b64dec SGVsbG8sIFdvcmxkIQ==` | `Hello, World!` |
| `hexenc "ABC"` | `414243` |
| `hexdec 414243` | `ABC` |
| `hexenc "xlang"` | `786C616E67` |
| `pctenc "alice bob/ok"` | `alice%20bob%2Fok` |
| `pctdec "alice%20bob%2Fok"` | `alice bob/ok` |
| `hash sha256 "test"` | `9F86D081884C7D659A2FEAA0C55AD015A3BF4F1B2B0B822CD15D6C15B0F00A08` |
| `hash sha1 "test"` | `A94A8FE5CCB19BA61C4C0873D391E987982FBBD3` |

## 实现说明

- Base64、HEX、URL percent 编解码直接调用 `codec` 标准库。
- SHA1/256/384/512 直接调用 `crypto` 标准库。
- 文件输入使用 `file.exists` 与 `file.readText`，命令分派和错误码保持统一。

该范例刻意不重复实现编解码算法，用于展示标准库的推荐调用方式。
