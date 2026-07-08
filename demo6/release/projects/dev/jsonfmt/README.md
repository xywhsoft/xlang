# jsonfmt - JSON 美化/压缩器

用 xlang 编写的命令行 JSON 格式化工具。

## 用法

```
jsonfmt pretty  <input.json> [output.json]   美化输出（2 空格缩进）
jsonfmt compact <input.json> [output.json]   压缩输出（无空白）
```

- 不指定 output 时输出到 stdout。
- 无效 JSON 报错并返回退出码 1。

## 运行与构建

```bat
cd /d D:\GIT\x-lang\demo6\release

REM 直接运行
xl run projects\dev\jsonfmt\jsonfmt.xl -- pretty  projects\dev\jsonfmt\test.json
xl run projects\dev\jsonfmt\jsonfmt.xl -- compact projects\dev\jsonfmt\test.json

REM 构建独立 exe
xl build projects\dev\jsonfmt\jsonfmt.xl -o projects\dev\jsonfmt\jsonfmt.exe
projects\dev\jsonfmt\jsonfmt.exe pretty  projects\dev\jsonfmt\test.json
```

## 实现

- 用 `json.valid` 校验，`json.parse` 解析成 `var`，`json.stringify(value, pretty)` 重新输出。
- 命令行参数通过 `func main(...)` 的 `arg` 数组读取（`arg[0]` 为程序名，用户参数从 `arg[1]` 开始）。

## 注意

- `json.stringify(value, pretty = true)` 默认用 TAB 缩进，本工具用 `string.replace(..., "\t", "  ")` 规范化为 2 空格。详见上层 `BUGS.md` BUG-002。
