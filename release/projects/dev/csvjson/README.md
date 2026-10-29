# csvjson - CSV <-> JSON 转换器

用 xlang 编写的命令行 CSV/JSON 双向转换工具。

## 用法

```
csvjson tojson <input.csv> [output.json]   CSV -> JSON（首行作 header）
csvjson tocsv  <input.json> [output.csv]   JSON -> CSV（对象数组转 CSV）
```

- 不指定 output 时输出到 stdout。
- CSV → JSON 输出为 2 空格缩进 pretty 格式，保留 CSV 列顺序。
- JSON → CSV 列顺序由 `json.parse` 的 hash 字典决定（见下文已知限制）。

## 运行与构建

```bat
cd /d D:\GIT\x-lang\release

REM 直接运行
xl run projects\dev\csvjson\csvjson.xl -- tojson projects\dev\csvjson\test.csv
xl run projects\dev\csvjson\csvjson.xl -- tocsv  projects\dev\csvjson\test.json

REM 写到文件
xl run projects\dev\csvjson\csvjson.xl -- tojson projects\dev\csvjson\test.csv projects\dev\csvjson\out.json

REM 构建独立 exe
xl build projects\dev\csvjson\csvjson.xl -o projects\dev\csvjson\csvjson.exe
projects\dev\csvjson\csvjson.exe tojson projects\dev\csvjson\test.csv
projects\dev\csvjson\csvjson.exe tocsv  projects\dev\csvjson\test.json
```

## CSV 引号处理（RFC 4180 子集）

- 含逗号、双引号、CR 或 LF 的字段用 `"..."` 包裹。
- 引号内的字面 `"` 转义为 `""`。
- 引号字段可跨行（换行保留在值中）。
- 引号外的 `\r` 丢弃，`\n` 结束记录。

示例（`test.csv`）：

```
name,age,city,note
Alice,30,Beijing,"Hello, World"
Bob,25,Shanghai,"She said ""hi"""
Carol,28,"New York","Multi
line field"
Dave,,Tokyo,
```

CSV → JSON 输出：

```json
[
  {
    "name": "Alice",
    "age": "30",
    "city": "Beijing",
    "note": "Hello, World"
  },
  {
    "name": "Bob",
    "age": "25",
    "city": "Shanghai",
    "note": "She said \"hi\""
  },
  ...
]
```

## 设计

### CSV → JSON

- 用 `string.charAt` / `string.len` 逐字符状态机解析 CSV：`inQuotes` 标记引号内/外，
  引号内遇 `""` 还原为单个 `"`，逗号/换号在引号外分别结束字段/记录。
- 补一个尾部 `\n`（若原文件无），使"结束记录"逻辑只出现在 `\n` 分支，避免末行重复代码。
- 首行解析为 header（拼成 JSON 数组文本 `["h1","h2",...]` 再 `json.parse` 成 var 数组，
  按列号索引取键名）。后续每行拼成 `{"k":"v",...}` 对象文本。
- 整体输出先拼成紧凑 JSON 数组文本 `[...]`，再用自写的**文本级** pretty-printer
  （`prettyJson`）格式化为 2 空格缩进——**不经过** `json.parse` 二次 round-trip，
  从而保留 CSV 列顺序（绕过 BUG-14）。

### JSON → CSV

- `json.valid` 校验 → `json.parse` 成 var 数组。
- 取第一个对象的键作为表头（`for key, val in <var dict>` 迭代收集成 JSON 数组文本，
  再 `json.parse` 回来按列号索引）。
- 每行每列用 `obj[key]` 取值，`string(v)` 强转（缺失键得到 null，`string(null)` 兜底为 `""`）。
- `csvQuote(s)` 对含逗号/引号/换行的字段加引号并转义内部 `"`。

## 已知限制

见 `../BUGS.md`：

- **BUG-14**：`json.parse` 不保留 JSON 对象键的插入顺序（hash dict）。
  - CSV → JSON 方向已绕过（文本级 pretty-printer 保留列序）。
  - **JSON → CSV 方向无法绕过**：表头顺序由 `json.parse` 的 hash 字典决定，
    输出 CSV 列序可能与输入 JSON 键序不同。数据值本身正确，仅列序变化。
- **BUG-006 / B5**：`xl build` 出的 exe 接收非 ASCII argv 会乱码。
  命令行参数（文件路径）请用 ASCII；CSV/JSON 数据内容支持完整 UTF-8（走文件读写）。

## 测试验证

`xl run` 与 `xl build`+exe 两条路径输出完全一致。验证覆盖：

- 基本字段拆分
- 引号包裹的逗号字段（`"Hello, World"`）
- 引号转义（`""` → `"`）
- 引号内跨行字段
- 空字段（行尾连续逗号 / 行中间空值）
- 仅 header 无数据行 → 输出 `[]`
- 无效 JSON → 报错返回退出码 1
- 文件不存在 → 报错返回退出码 1
- CSV → JSON → CSV → JSON 多跳 round-trip 数据值无损
