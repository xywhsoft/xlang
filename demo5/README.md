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
- generator 显式恢复执行器主路径
- `for in` 的 range / 容器 / generator / object 主路径
- 多层 `break n / continue n`
- `task.spawn / all / race / cancel` 主路径
- 统一测试入口
- 自动化回归基线

## Path Policy

`import` 包搜索路径统一收紧为：

- `release\libs`，也就是 `xl.exe` / `xldbg.exe` 所在程序目录下的 `libs`
- 脚本/项目目录下的 `libs`

TCC 编译链路使用 `release\tcc` 下的固定 include/lib/support 路径；额外 include/lib 路径只通过 `#incpath`、`#libpath` 或 `xlang.json` 的 `incpath` / `libpath` 显式注册。

## Quick Start

构建：

```bat
build.bat
```

运行统一测试：

```bat
release\xl5_test.exe all
release\xl5_test.exe runtime
release\xl5_test.exe lower
```

运行自动回归：

```bat
regress.bat
```

运行 xldbg DAP smoke：

```bat
node test\dap\xldbg_dap_smoke.js
```

运行示例程序：

```bat
release\xl.exe run release\examples\full\run_basic.xl
release\xl.exe run release\examples\full\for_in_range.xl
release\xl.exe run release\examples\async\run_main.xl
release\xl.exe run release\examples\task\task_basic.xl
```

查看中间产物和调试 dump：

```bat
release\xldbg.exe lower release\examples\async\basic.xl --dump-hir
release\xldbg.exe cgen release\examples\task\task_basic.xl --dump-c
```

产物目录：

- `release/dump/`
- `release/trace/`
- `release/trace/tests/`

## Stable Commands

CLI 主入口：

```bat
release\xl.exe build <file> --emit <path>
release\xl.exe run <file>
release\xl.exe build-c <file> --emit <path>
release\xl.exe run-c <file>
```

调试入口：

```bat
release\xldbg.exe lex <file>
release\xldbg.exe parse <file>
release\xldbg.exe sem <file>
release\xldbg.exe lower <file>
release\xldbg.exe cgen <file>
release\xldbg.exe dbg <file>
release\xldbg.exe dap
```

测试入口：

```bat
release\xl5_test.exe [selector]
```

## Design Docs

- `docs/README.md`
- `docs/design/implicit-conversion-rules.md`
- `docs/design/xldbg-dap-debugger-design.md`
- `docs/design/xldbg-vscode-dap-adapter.md`
- `tools/vscode-xlang-debug/README.md`

## Current Boundaries

当前首轮主链路已经可用，但仍保留这些边界：

- generator 当前已切到显式恢复执行器主路径：生成状态结构体 + executor 统一管理 suspend/resume/done；async/generator 复杂控制流的 embedded runtime 直跑回归也已补齐，并通过禁用 `[fallback.*]` 痕迹断言固定下来
- xle 元信息当前已显式携带 `moduleAbiId` / `languageAbiId` / `nativeAbiId`，以及导出项级别的 `abiId` / `versionedSymbol`
- `for in` 已覆盖 `range(...)`、基础容器、generator 和 object 场景；更复杂的用户自定义 Iterable 协议仍按首版边界推进
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
