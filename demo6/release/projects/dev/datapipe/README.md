# datapipe - composable data pipeline framework

A data-pipeline framework written in pure xlang. A pipeline is a JSON document
listing ordered steps; a global operation registry dispatches each step by name
to its handler (xlang function values cannot be passed around, so the registry
is a name table plus a single dispatch site).

```
Source -> Filter -> Map -> Reduce(aggregate) -> Sink   (+ Sort, Join anywhere)
```

## Usage

```bat
cd /d D:\GIT\x-lang\demo6\release
xl run  projects\dev\datapipe\datapipe.xl            // runs self-test
xl build projects\dev\datapipe\datapipe.xl -o projects\dev\datapipe\datapipe.exe

datapipe run <pipeline.json> [input.csv|input.json] [csv|json|table|tsv]
datapipe test
datapipe ops
```

## Operations

| op     | fields                                                |
|--------|-------------------------------------------------------|
| source | `file` or `data:[ {..}, {..} ]` (csv/json auto by ext)|
| filter | `expr`: `col OP v` with `= != > >= < <=`, `AND`/`OR`, `(...)` |
| map    | `fields`(const) / `copy` / `compute`(`+ - * /`) / `rename` / `drop` |
| group  | `by`(col or [cols]), `agg`: `{name:"COUNT"|"SUM(c)"|"AVG(c)"|"MIN(c)"|"MAX(c)"}` |
| sort   | `by`(col or [cols]), `desc`(bool or [bools])         |
| join   | `file`,`left`,`right`,`prefix`, `type`=`inner`/`left` |
| sink   | `format`: csv/json/table/tsv                          |

## Example pipeline.json

```json
{"steps": [
  {"op": "filter", "expr": "age > 25"},
  {"op": "map", "fields": {"category": "senior"}},
  {"op": "group", "by": "dept", "agg": {"count": "COUNT", "avg_salary": "AVG(salary)"}},
  {"op": "sort", "by": "avg_salary", "desc": true},
  {"op": "sink", "format": "csv"}
]}
```

## Design notes

- Rows are var dicts (`colname -> value`); column order is tracked in a global
  `gHeaders` so output is stable regardless of dict hash order.
- CSV parser is char-by-char (quoted fields, `""` escape, cross-line fields).
- Sort is an **iterative** bottom-up merge sort (no recursion -> no GC pressure).
- Filter / compute expressions are parsed **once** into an AST; per-row eval is
  read-only recursion (no per-frame dict allocation).
- Workarounds for documented xlang limits (see dev/BUGS.md): no `+=`, no `var`
  on the left of string `+` (cells are funneled through `string(...)`), `is`
  checks lowered to a `bool` local before `not`.

Self-test: **73 assertions, all passing**.
