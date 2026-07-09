# encconv - 编码转换验证工具

测试 `encoding` 标准库的全部能力，并把它们用一组实用 CLI 子命令串起来。

## 用法

```
encconv selftest                          运行 encoding API 全量自测
encconv names                             列出所有编码常量与名称
encconv detect <text>                     检测文本编码
encconv detect-file <path>                检测文件字节流的编码
encconv to <text> <charset>               文本 -> 字节 (HEX 展示)
encconv from <hex> <charset>              HEX 字节 -> 文本
encconv conv <hex> <from> <to>            字节编码互转 (HEX 输出)
encconv utf16 <text>                      UTF-8 文本 -> UTF-16 字节
encconv fromutf16 <hex>                   UTF-16 字节 -> UTF-8 文本
encconv file <path> <from> <to> [-o out]  文件级编码转换
encconv help | -h | --help                显示本帮助
```

支持的 charset（`parseCharset` 接受，大小写不敏感）：
`utf-8 / utf-16 / utf-16be / utf-32 / utf-32be / binary / oem / auto`

## selftest 覆盖的能力（13 条用例，全部 PASS）

| # | 用例 | 覆盖 API |
|---|---|---|
| 1 | encoding.name 全部常量 | `name` + 常量 `utf8/utf16/utf16BE/utf32/utf32BE/binary/autoDetect` |
| 2 | encoding.charSize | `charSize` (utf8=1/utf16=2/utf32=4) |
| 3 | name(utf8+bom) 仍为 utf-8 | `bom` 常量 + `name` 对 flag 位的处理 |
| 4 | isUtf8 / detect 文本路径 | `isUtf8`、`detect(text)` |
| 5 | isUtf8Bytes / detectBytes (0xFF→oem) | `isUtf8Bytes`、`detectBytes` |
| 6 | toBytes / fromBytes UTF-16 往返 | `toBytes`、`fromBytes` |
| 7 | convertBytes UTF-16 → UTF-8 | `convertBytes` |
| 8 | UTF-8 字节自洽 | `toBytes(utf8)` + `fromBytes(utf8)` |
| 9 | UTF-32 往返 | `toBytes(utf32)` + `fromBytes(utf32)` |
| 10 | UTF-8 ↔ UTF-16 中文长串往返 | `toBytes` + `fromBytes` + `convertBytes` |
| 11 | UTF-16 LE/BE 字节序差异 | `toBytes(utf16)` vs `toBytes(utf16BE)` + `byteAt` |
| 12 | 跨编码 convertBytes (utf32→utf8/utf16) | `convertBytes` 跨编码 |
| 13 | detectBytes/isUtf8Bytes 合法 UTF-8 → utf8 | `detectBytes` + `isUtf8Bytes` 正例 |

## 实用子命令示例

```
encconv names
encconv detect 你好
encconv to 你好世界Hello utf-16      # -> 604F7D59164E4C75480065006C006C006F00
encconv utf16 你好                    # -> 604F7D59
encconv conv 48006900 utf-16 utf-8    # -> 4869  text: Hi
encconv fromutf16 604F7D59            # -> 你好
encconv file in.txt utf-8 utf-16 -o out.utf16
encconv file out.utf16 utf-16 utf-8   # -> 打印原文
```

## 编码常量实测值（Windows，由 `encconv names` 输出）

| 常量 | codepage | 名称 |
|---|---|---|
| `encoding.utf8` | 65001 | utf-8 |
| `encoding.utf16` | 1200 | utf-16 |
| `encoding.utf16BE` | 1201 | utf-16be |
| `encoding.utf32` | 65005 | utf-32 |
| `encoding.utf32BE` | 65006 | utf-32be |
| `encoding.binary` | -1 | binary |
| `encoding.oem` | 0 | oem |
| `encoding.autoDetect` | -2 | auto |
| `encoding.bom`（flag 位） | 1073741824 (0x40000000) | — |

## 设计要点

- `Buffer` 是 native-backed 不可复制类，所有 `Buffer` 操作内联在使用它的函数体内，
  不通过函数参数传递 `Buffer`。每个 selftest 用例因此独立成一个返回 `bool` 的函数。
- 字节视图函数统一用 `(buf.data(), c_size(buf.len()))` 形式调用，
  与 `examples/stdlib_encoding.xl` 保持一致。
- 字节序列的可读展示走 `codec.hexEncodeBytes`；hex 解码走 `codec.hexDecodeBytes`。

## 验证

```
xl run  projects\dev\encconv\encconv.xl                 # EXIT=0
xl build projects\dev\encconv\encconv.xl -o projects\dev\encconv\encconv.exe   # EXIT=0
```

selftest 输出 `13/13 passed`。

## 发现的 BUG

见 `../BUGS.md` 的 **BUG-E1**：`encoding.toBytes(text, charset + encoding.bom)`
对任意 charset 都返回空 `Buffer`（`len()==0`）。`bom` 常量与 `name()` 配合正常，
但 `toBytes` 没有处理 `bom` flag 位，静默产出空结果。
