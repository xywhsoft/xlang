# csvengine - CSV/TSV 流式解析 + SQL-like 查询引擎

用 xlang 实现的 CSV/TSV 处理引擎：RFC4180 风格流式解析（引号/转义/跨行字段）+
SQL-like 查询（WHERE / ORDER BY / GROUP BY / JOIN / 聚合）+ 多种输出格式。

## 命令

```bat
cd /d D:\GIT\x-lang\release

xl run projects\dev\csvengine\csvengine.xl -- query  <file.csv> "<SQL>" [csv|json|table|tsv]
xl run projects\dev\csvengine\csvengine.xl -- convert <file.csv> [json|table|csv|tsv]
xl run projects\dev\csvengine\csvengine.xl -- stats   <file.csv>
xl run projects\dev\csvengine\csvengine.xl              :: 内置自测（55 项）
```

## SQL 语法

```sql
SELECT * FROM data WHERE col > 100
SELECT name, age FROM data WHERE age > 25 ORDER BY age DESC
SELECT dept, COUNT(*) FROM data GROUP BY dept
SELECT dept, SUM(salary), AVG(age), MIN(age), MAX(age) FROM data GROUP BY dept
SELECT a.name, b.salary FROM emp a JOIN dept b ON a.dept = b.id
SELECT a.name FROM emp a JOIN dept b ON a.dept = b.id WHERE b.salary > 90
```

- 聚合：`COUNT(*)` / `COUNT(col)` / `SUM` / `AVG` / `MIN` / `MAX`
- WHERE：`= != > >= < <=`，`AND` / `OR`，`(...)`，字符串字面量用 `"..."` 或 `'...'`
- ORDER BY：`col [ASC|DESC]`（可多列）
- JOIN：从主文件同目录解析 `<table>.csv`（如 `dept` → 同目录 `dept.csv`）
- TSV：文件名 `.tsv` 自动用 Tab 分隔（输入与 CSV 输出）

## 流式设计

CSV 解析逐字符进行（正确处理引号包裹、`""` 转义、跨行字段、CR 丢弃）。
单表 `WHERE` 查询走流式过滤：只有通过 WHERE 的行才进入结果集——选择性高的过滤
不会把整文件物化进内存。`ORDER BY` / `GROUP BY` / `JOIN` 自然需要持有匹配行。

## 输出格式

- `table`（默认）：对齐表格，带分隔线
- `csv` / `tsv`：RFC4180 引号转义
- `json`：对象数组，**保列序**（手工拼接 compact JSON 再文本级 pretty-print，
  不走 json.parse 以避免 hash 重排，见 BUG-K1）

## 验证

`xl run` 与 `xl build` 均通过，内置自测 55/55，EXIT=0。

## 类型选择

CSV 输出缓冲使用静态 `string`，记录字段使用 `var` 以保留输入的动态 JSON 类型；
字符串拼接会按当前类型转换规则把动态字符串值安全转换为文本。
