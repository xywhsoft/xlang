# demo5

`demo5` 是 `xlang` 第五版 demo 的首轮可运行实现。

当前已经打通的主链路：

- `flex` 词法
- `lemon` 语法
- AST / semantic / lowering / C codegen
- `libtcc` 运行链
- 模块 / native 模块
- `throw`
- `async/await`
- `yield` lowering
- generator 最小物化运行时
- `for in` 的 `range(...)` 主路径
- 多层 `break n / continue n`
- `task.spawn / all / race / cancel` 主路径
- 统一测试入口
- 自动化回归基线

## Quick Start

构建：

```bat
build.bat
```

运行统一测试：

```bat
release\bin\xl5.exe test
release\bin\xl5.exe test runtime
release\bin\xl5_test.exe lower
```

运行自动回归：

```bat
regress.bat
```

运行示例程序：

```bat
release\bin\xl5.exe run examples\full\run_basic.xl
release\bin\xl5.exe run examples\full\for_in_range.xl
release\bin\xl5.exe run examples\async\run_main.xl
release\bin\xl5.exe run examples\task\task_basic.xl
```

查看中间产物：

```bat
release\bin\xl5.exe lower examples\async\basic.xl --dump-hir
release\bin\xl5.exe cgen examples\task\task_basic.xl --dump-c
```

产物目录：

- `release/dump/`
- `release/trace/`
- `release/trace/tests/`

## Stable Commands

CLI 主入口：

```bat
release\bin\xl5.exe lex <file>
release\bin\xl5.exe parse <file>
release\bin\xl5.exe sem <file>
release\bin\xl5.exe lower <file>
release\bin\xl5.exe cgen <file>
release\bin\xl5.exe run <file>
release\bin\xl5.exe test [selector]
```

## Design Docs

- `docs/design/xldbg-dap-debugger-design.md`
- `docs/specs/xldbg-dap-debugger-v1-spec.md`

## Current Boundaries

当前首轮主链路已经可用，但仍保留这些边界：

- generator 当前已切到显式恢复执行器主路径：生成状态结构体 + executor 统一管理 suspend/resume/done；async/generator 复杂控制流的 embedded runtime 直跑回归也已补齐，并通过禁用 `[fallback.*]` 痕迹断言固定下来
- xlp 元信息当前已显式携带 `moduleAbiId` / `languageAbiId` / `nativeAbiId`，以及导出项级别的 `abiId` / `versionedSymbol`
- `for in` 的运行期主路径当前聚焦 `range(...)`
- `std.task` 当前是最小可运行编排语义，不是完整调度系统
- 更多样例和更细粒度断言会继续通过统一 runner 逐步补齐

## Regression Baselines

稳定期自动回归基线位于：

- `test/cases/all/baseline.txt`
- `test/cases/lexer/baseline.txt`
- `test/cases/parser/baseline.txt`
- `test/cases/sem/baseline.txt`
- `test/cases/lower/baseline.txt`
- `test/cases/codegen/baseline.txt`
- `test/cases/runtime/baseline.txt`

`regress.bat` 会重新构建、执行回归，并把日志写入：

- `release/trace/tests/logs/`
- `release/trace/tests/diff/`
