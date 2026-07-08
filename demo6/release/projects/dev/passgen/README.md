# passgen - 密码 / 随机串生成器

用 xlang 编写的命令行密码与随机串生成工具，覆盖 `random` 与 `crypto` 标准库。

## 功能

| 命令 | 说明 |
|------|------|
| `passgen` | 默认生成 16 位强密码（大小写字母 + 数字 + 符号） |
| `passgen <length>` | 指定长度 |
| `passgen <length> --no-symbols` | 不含符号 |
| `passgen <length> --hex` | 生成 HEX 串（`crypto.randomHex`，小写） |
| `passgen <count> <length>` | 批量生成 |
| `passgen --uuid` | 生成 UUID v4（`random.u32` 拼成 8-4-4-4-12） |
| `passgen --help` / `-h` | 帮助 |

强度评估：统计生成的密码实际覆盖的字符种类（小写 / 大写 / 数字 / 符号），输出 `[strong/medium/fair/weak] n/4 categories (...)`。

## 运行

```bat
cd /d D:\GIT\x-lang\demo6\release

:: run
xl.exe run projects\dev\passgen\passgen.xl
xl.exe run projects\dev\passgen\passgen.xl -- 32
xl.exe run projects\dev\passgen\passgen.xl -- 24 --hex
xl.exe run projects\dev\passgen\passgen.xl -- --uuid
xl.exe run projects\dev\passgen\passgen.xl -- 5 20

:: build + exe
xl.exe build projects\dev\passgen\passgen.xl -o projects\dev\passgen\passgen.exe
projects\dev\passgen\passgen.exe 32
projects\dev\passgen\passgen.exe --uuid
```

> 注意：`xl run` 需用 `--` 分隔脚本参数；构建出的 exe 直接接收参数（argv[0] 为程序名）。

## 实现要点

- 密码：`random.string(length, charset)`，字符集由 4 类拼接。
- HEX：`crypto.randomHex(nbytes)` 取 `(length+1)/2` 字节再 `string.left` 截断到指定长度。
- UUID：4 × `random.u32()` 各用 `string.formatInt(v, "x8")` 转 8 位小写 HEX，拼成 32 位后按 8-4-4-4-12 切片，并写入 version(`4`) / variant(`8`) 半字节。
- 强度：`for c in pw` 遍历，用 `string.contains` 判定字符所属类别，统计覆盖度。
