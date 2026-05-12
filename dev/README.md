# xlang dev 文档口径索引

更新时间：2026-05-12

本目录同时保存了当前第五版文档、阶段性 spec、历史方案和资料归档。阅读顺序必须先看本索引，避免把旧 draft 当成当前实现状态。

## 当前口径

- 当前实现主线是 `demo5`：xlang 源码经 lexer / parser / AST / semantic / lowering / C codegen，再由 libtcc 构建或运行。
- `release\xl.exe` 是发布 CLI，只保留 `build` / `run` / `build-c` / `run-c`。
- `release\xldbg.exe` 是调试 CLI，额外提供 `lex` / `parse` / `sem` / `lower` / `cgen` / `dbg` / `dap`。
- `std.task`、DAP 调试器、XLE/native binding、SDK 都已经有可运行主链路，但仍按首版边界收口，不应写成完整生产级能力。
- 文档中的“标准表”定义目标和命名，“覆盖率”才描述实现状态；二者不能混写。

## 推荐阅读顺序

1. `demo5/README.md`：demo5 可运行入口、命令和当前边界。
2. `demo5/docs/README.md`：demo5 设计文档索引和状态说明。
3. `xlang第五版语言标准支持覆盖率.txt`：当前语言实现覆盖率，优先级高于旧设计稿。
4. `xlang第五版语言标准总表.txt`：第五版语言目标表，里面的 demo5 注记用于解释当前差异。
5. `xlang第五版增强标准库支持覆盖率.txt`：当前标准库和官方扩展库覆盖率。
6. `xlang第五版增强标准库标准表.txt`：第五版增强标准库命名和职责目标。
7. `xlang第五版发布候选版收口开发Spec.txt` 与 `xlang第五版发布候选版补充跟踪Spec.txt`：发布候选阶段任务跟踪。
8. `extlibs/browser/README.md`：官方 `x.browser` 扩展库设计区，属于扩展库口径，不是 demo5 核心内置能力。

## 历史与参考文档

这些文件保留设计背景，但不是当前实现状态的权威来源：

- `语言标准第三版方案.txt`
- `xlang第四版设计方案.txt`
- `xlang第四版demo一阶段方案.txt`
- `xlang第四版demo一阶段方案-语法列表.txt`
- `xlang类型注册系统设计.txt`
- `xlang标准库完整设计方案.txt`
- `类型系统.txt`
- `内置库.txt`
- `资料/`

历史文档可以引用思想和背景，但若内容与第五版覆盖率、demo5 README 或发布候选 spec 冲突，以当前文档为准。

## 文档维护规则

- 新增实现状态时，优先更新覆盖率文档，不要只追加设计稿段落。
- 描述 CLI 时必须区分 `xl.exe` 与 `xldbg.exe`。
- 描述标准库时必须区分“模块名已稳定”“最小 API 已验证”“语义已完整压实”。
- 描述 native / XLE / binding 时必须写清 ABI、包加载、C 绑定覆盖面的边界。
- 非 spec 设计文档应在开头标注“当前状态”或“历史草案”，避免和当前实现混淆。
