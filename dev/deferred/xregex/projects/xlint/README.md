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
- 多文件检查在单个 xlint 进程内顺序执行，不再通过短命子进程 fan-out 规避运行时问题。
- 每个文件完成后释放 AST、遍历栈和诊断临时值；XRT 的临时区与复合值生命周期已完成公共修复。
- 同一源码树连续运行会产生稳定排序的确定性结果，便于 CI 做报告哈希和差异比较。

## 验证

```bat
xl run  projects\dev\xlint\xlint.xl                       :: EXIT=0（用法）
xl build projects\dev\xlint\xlint.xl -o projects\dev\xlint\xlint.exe   :: EXIT=0
xlint.exe projects\dev -r --format json > xlint-report.json
```

当前发布验收快照：174 个 `.xl` 源文件，165 个文件产生诊断，共 9421 个 issue，
JSON 报告 1,547,715 字节，单进程约 27 秒。连续两次运行的 SHA-256 均为
`EFE19798D760BF56DED86E86110DDDE66BE7D228F029B42C433F6681D74A6176`。
