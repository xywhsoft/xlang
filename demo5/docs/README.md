# demo5 文档索引

更新时间：2026-05-12

本目录只放 demo5 实现相关的设计说明。实现状态以 `demo5/README.md`、`dev/xlang第五版语言标准支持覆盖率.txt`、`dev/xlang第五版增强标准库支持覆盖率.txt` 和回归测试为准。

## 当前实现入口

- `release\xl.exe`：发布入口，支持 `build` / `run` / `build-c` / `run-c`。
- `release\xldbg.exe`：调试入口，支持 `lex` / `parse` / `sem` / `lower` / `cgen` / `dbg` / `dap`，并保留 `build` / `run`。
- `release\xl5_test.exe`：统一测试入口。

## 设计文档状态

- `design/implicit-conversion-rules.md`：隐式转换规则。当前已有 `src/sem/xl_convert.*` 统一 API，但类型描述符中心化和所有上下文收敛仍未完成。
- `design/xldbg-dap-debugger-design.md`：xldbg DAP 设计。当前 stdio DAP V1 已接通并有 smoke 测试；TCP listen、条件断点、完整表达式求值、变量修改等仍是边界外能力。
- `design/xldbg-vscode-dap-adapter.md`：VS Code 最小 adapter 接入。当前扩展骨架位于 `tools/vscode-xlang-debug`，语义能力以 `xldbg dap` 为准。

## 已知边界

- `std.task` 是最小任务编排语义，不是完整调度系统。
- 标准库模块有一部分仍是源码声明 + codegen/runtime helper 的组合，不是独立稳定 ABI。
- XLE 元信息和加载链路已经可用，但包解析、版本策略和 binding 覆盖面仍按首版处理。
- DAP 调试器是源码 probe V1，不是 OS 级或完整语言调试器。

