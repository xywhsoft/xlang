# grep - 文件内容搜索工具

在文件/目录中按正则或纯文本搜索行级匹配，类似 Unix `grep`。

## 用法

```bat
xl run projects\dev\grep\grep.xl -- <pattern> <file>
xl run projects\dev\grep\grep.xl -- <pattern> <dir> -r
xl run projects\dev\grep\grep.xl -- -i <pattern> <file>
xl run projects\dev\grep\grep.xl -- -n <pattern> <file>
xl run projects\dev\grep\grep.xl -- -c <pattern> <file>
xl run projects\dev\grep\grep.xl -- -F <pattern> <file>
```

构建独立 exe：

```bat
xl build projects\dev\grep\grep.xl -o projects\dev\grep\grep.exe
grep.exe <pattern> <file|dir> [options]
```

## 选项

| 选项 | 长形式 | 说明 |
|---|---|---|
| `-i` | `--ignore-case` | 大小写不敏感 |
| `-n` | `--line-number` | 输出加行号 `file:line:content` |
| `-c` | `--count` | 只统计匹配行数，按文件输出 `file:count` |
| `-r` | `--recursive` | 递归搜索目录下所有文件 |
| `-F` | `--fixed-strings` | 纯文本包含搜索（不走正则，适合 `[`、`(` 等特殊字符） |

默认用 `regex.isMatch` 做正则匹配；`-F` 切到 `string.contains` 纯文本包含。
不带 `-n` 时输出格式为 `file:content`。

## 输出示例

```
examples\hello.xl:1:func add(int a, int b) as int
examples\containers.xl:5:auto values = [10, 20, 30]
Found 2 matches in 2 files
```

`-c` 模式只输出每个命中文件的计数，不输出明细行和汇总。

## 设计要点

- `file.readText` 读全文 → `string.splitLines` 按行拆分 → 逐行 `regex.isMatch` / `string.contains`。
- `-i` 正则模式传 `regex.ignoreCase`；纯文本模式给 `string.contains` 传 `ignoreCase=true`。
- 递归用 `dir.files(dir, true)` 取所有文件完整路径（返回值是字符串数组，元素直接是 OS 原生分隔符的完整路径）。
- 读文件包 `try/catch e`，二进制或无权限文件静默跳过。

## 重要用法提醒：run 模式必须用 `--` 分隔脚本参数

`xl.exe` 自身占用 `-i`（include/链接路径，与 build 模式共享），所以脚本要接收 `-i`/`-n`/`-c` 等开关时，**必须**用 `--` 把 xl.exe 参数和脚本参数隔开：

```bat
:: 错误：xl.exe 会把 -i 当成自己的 include 开关，报 "cannot add link library: RETURN"
xl run projects\dev\grep\grep.xl -i "RETURN" examples/hello.xl

:: 正确
xl run projects\dev\grep\grep.xl -- -i "RETURN" examples/hello.xl
```

built exe (`grep.exe`) 不存在此问题，可直接 `grep.exe -i RETURN file`。

## 验证

run 与 build 两条路径均通过：

- `grep.exe "add" examples/hello.xl` → 2 matches
- `grep.exe -n -i "return" examples/hello.xl` → `examples/hello.xl:2:    return a + b`
- `grep "func" examples -r` → 750 matches in 79 files（约 0.46s）
- `grep -F -n "[10, 20, 30]" examples/containers.xl` → 2 matches（正则特殊字符安全）
- `grep -c "func" examples -r` → 按文件输出计数

## BUG 清单

未发现新的编译器 BUG。开发中遇到两处语法/用法要点：

1. **`catch` 必须带变量名**：`catch e` 合法，裸 `catch` 触发 `parse 语法错误`。这是 xlang 语法要求（参见 `examples/concurrency.xl`），非 BUG。
2. **`xl.exe run` 占用 `-i` 开关**：见上方「重要用法提醒」。这是 CLI 设计（runner 与 builder 共享 flag 解析），非 BUG，但工具作者需在文档里提醒用户加 `--`。
