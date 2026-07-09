# xlint - xlang lint 工具

用 xlang 编写、用 `xl parse --format json` 取 AST、递归（迭代）遍历 AST 树做代码分析的 lint 工具。
**用 xlang 处理 xlang 自身代码**。

## 用法

```bat
cd /d D:\GIT\x-lang\demo6\release
xl run  projects\dev\xlint\xlint.xl <file.xl|dir> [options]
xl build projects\dev\xlint\xlint.xl -o projects\dev\xlint\xlint.exe
xlint.exe <file.xl|dir> [options]
```

> `xl run <script.xl>` 传参需用 `--`：`xl run xlint.xl -- <target> [options]`。

### 选项

| 选项 | 说明 |
|---|---|
| `<file.xl>` | 检查单个文件 |
| `<dir> -r` | 递归检查目录 |
| `--format json` | JSON 格式报告（纯 JSON，可机器解析） |
| `--rule <name>` | 只跑单条规则 |
| `--threshold <n>` | 设置 no-magic-numbers 阈值 |
| `--config <path>` | 指定 `.xlintrc.json` 配置文件 |
| `--strict` | 发现任何 issue 返回 EXIT=1（默认 EXIT=0） |

默认退出码 0（lint 发现不影响工具退出，便于在 CI 中先收集再判断）；加 `--strict` 才按发现返回非零。

## 规则

| 规则 | 严重度 | 说明 |
|---|---|---|
| `no-while-without-do` | error | `while` 必须带 `do`（源码扫描，能捕获无法解析的坏代码） |
| `no-undefined-var` | error | 变量使用前必须声明（小写开头的裸标识符；大写视作类型/类名跳过） |
| `consistent-naming` | warning | 变量 snake_case（或常量 SCREAMING_SNAKE）/ 类名 PascalCase |
| `no-magic-numbers` | warning | 未命名的魔法数字（不在命名声明初始化里，绝对值 ≥ 阈值，默认 100） |
| `require-doc` | info | 模块级公开函数（除 `main`）应有 `#doc` |
| `max-function-length` | warning | 函数体不超过 N 行（默认 80） |
| `no-deep-nesting` | warning | if/while/for/do-while/try 嵌套不超过 N 层（默认 5） |
| `unused-variable` | warning | 声明但未使用的局部变量（`_` 开头豁免） |

## 配置 `.xlintrc.json`

按 cwd → 目标文件所在目录 → 目标目录 顺序自动发现；也可用 `--config` 指定。

```json
{
    "rules": {
        "consistent-naming": "off",
        "require-doc": "off",
        "max-function-length": "off"
    },
    "thresholds": {
        "no-magic-numbers": 100,
        "max-function-length": 80,
        "no-deep-nesting": 5
    }
}
```

## 实现要点

- `process.run("xl parse <abs> --format json")` 取 AST JSON → `json.parse` 成 `var` 树。
- **两次迭代遍历**（`passA` 收集声明 + 命名/魔法数/函数长度/嵌套；`passB` 未定义变量/未用变量）。
- **fan-out**：多文件时主进程对每个文件起一个短命子进程做单文件 lint（输出 JSON），主进程聚合。
  这是规避 xlang 运行时在单进程内累积大量 var AST 逐步变慢的必要手段（见 `../BUGS.md` BUG-XL2）：
  92 文件从 >15min 降到 ~21s。

## 验证

```bat
xl run  projects\dev\xlint\xlint.xl                       :: EXIT=0（用法）
xl build projects\dev\xlint\xlint.xl -o projects\dev\xlint\xlint.exe   :: EXIT=0
xlint.exe projects\dev -r                                  :: 92 文件 ~21s, EXIT=0
```
