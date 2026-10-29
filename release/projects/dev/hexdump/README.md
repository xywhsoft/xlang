# hexdump - hex + ASCII file viewer

`hexdump -C` / `xxd` 风格的文件十六进制 + ASCII 查看器。用途：实战验证 xlang 的
`Buffer` 完整 API 与 `File` 流式对象（DEMO6 功能清单第 14 节）。

## 用法

```
hexdump <file>                 dump first 512 bytes
hexdump <file> <bytes>         dump first <bytes> bytes
hexdump <file> -s <offset>     start at <offset>
hexdump <file> -n <count>      dump <count> bytes (-1 = to EOF)
hexdump <file> -s <off> -n <n> combine skip + length
hexdump --help | -h            show this help
```

输出格式（与 `hexdump -C` 一致，8 位零填充偏移、每行 16 字节、中段分组空格、
右侧 `|ascii|`，末尾短行用空格补齐使 ASCII 列对齐）：

```
00000000  23 20 78 6c 61 6e 67 20  44 45 4d 4f 36 0a 0a 23  |# xlang DEMO6..#|
00000010  ...
```

## 运行 / 构建

```bat
cd /d D:\GIT\x-lang\demo6\release
xl run   projects\dev\hexdump\hexdump.xl -- <file> [-s <off>] [-n <n>]
xl build projects\dev\hexdump\hexdump.xl -o projects\dev\hexdump\hexdump.exe
hexdump.exe <file> [-s <off>] [-n <n>]
```

两条路径均须 EXIT=0（成功）；参数错误返回 1，API 自检失败返回 2。

## 验证的 API

读取路径：`file.readBytes(path)` → `Buffer`，逐字节 `buf.byteAt(i)` 格式化。
此外程序在每次运行末尾输出三项自检结果：

- **buffer self-test**：在独立 scratch `Buffer` 上跑 `len`/`capacity`/`byteAt`/`[]`/
  `setByte`/`[]=`/`append`/`appendByte`/`appendBytes(data,size)`/`text`/`data`/
  `reserve`/`clear` 全套。
- **file-stream read-only**：`file.open(path,true)`，交叉验证 `pos`/`size`/`eof`/
  `seek(seekSet|seekCur|seekEnd)`/`readText(size)`，并和 `readBytes` 的缓冲逐字节
  比对。二进制区段 `readText` 会按设计抛错，`try/catch` 优雅降级，校验仍通过。
- **file-stream writable**：在 `%TEMP%\hexdump_scratch.txt` 上验证 `writeText`（返回
  写入字节数）/ `setEOF`（截断）/ `seek` / `readText`，**不触碰用户输入文件**。

## 发现的限制（见上级 `../BUGS.md`）

- **BUG-HD1（中）**：native-backed class（`Buffer`/`File`）**不能从已有具名值 return**
  （`[sem] 不可复制类型不能从已有值返回`），但 (a) 作函数参数传入、(b) `return Buffer()`
  直接构造的临时量，两条路径都正常。dev/README「仍需注意的限制」表中「不可按值传参」
  措辞偏宽，实际被禁的只有「return 已有具名值」一档。本工具因此把 `dumpBytes` /
  `verifyStreamReadOnly` 设计成**接收 Buffer 参数**（而非返回 Buffer）。
- **BUG-HD2（低）**：`int.format(n, "c")` 不按 `%c` 字符格式化，原样返回十进制
  文本。byte→char 改用 `Buffer` 构造 0x20..0x7e 可打印 ASCII 表 + `string.charAt`
  索引（见 `buildPrintable()` / `asciiOf()`）。

## 文件

- `hexdump.xl` — 主源码（单文件，无 C 依赖）。
