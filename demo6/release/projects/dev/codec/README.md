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

## 实现说明（重要）

本工具**没有**使用 `codec.hexEncode/hexDecode`、`codec.percentEncode/percentDecode` 和 `Buffer`，原因是踩到 3 个 xlang BUG（详见上层 `../BUGS.md`）：

1. **BUG-15**：`codec.hex*` 与 `crypto.*` 在同一源文件内会冲突——两者的 C wrapper 都声明 `xrtHexEncode` 且签名不兼容，TCC 报 `incompatible types for redefinition`。run/build 都编译失败。
2. **BUG-16**：`codec.percentEncode/Decode` 在 `xl build` 下链接失败（`xrtPercentEncode/Decode` 未解析）；`xl run` 正常。
3. **BUG-17**：`Buffer` native-backed class 在 `xl build` 下链接失败（`xrtNetDgramPacketBytesCopy` 未解析）；只要源文件出现 `Buffer` 就触发，`xl run` 正常。

### 绕过方案（run/build 双路径通用）

- **字节入口**：用 `string.bytes(text)` 取 UTF-8 字节数组（`array<int>`），代替 `Buffer`。
- **hexenc**：`string.bytes` 逐字节格式化为大写 HEX。
- **hexdec / pctdec**：把解码出的字节收进 `list<int>`，**手工 base64 编码**后交给 `codec.base64Decode` 还原字符串——`codec.base64*` 是唯一在 run/build 都能与 `crypto.*` 共存的 codec 对。
- **pctenc**：`for c in text` + `string.contains(UNRESERVED, c)` 判定保留字，非保留字用 `string.bytes(c)` 取字节后 `%XX`。
- **hash**：直接用 `crypto.sha1/sha256/sha384/sha512`（输出大写 HEX）。

手工实现与官方 `codec.hexEncode/percentEncode` **逐字节一致**（ASCII + UTF-8 多字节均校验过）。

> `list<int>` 作为 `var` 形参传递可用（函数内 `.length`/`[i]` 正确）；但 `var` 元素读出需 `int(...)` 显式转换。见 `../BUGS.md` NOTE-15。
