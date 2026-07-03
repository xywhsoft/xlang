# DEMO6 RC0 变更摘要

本文档记录 DEMO6 当前 RC0 基线的主要变化，供提交前审查和后续开发接续使用。

## 核心目标

RC0 的目标不是继续扩张语言特性，而是把已经实现的基础能力压实：

- 编译器可以稳定构建。
- 主要命令面可以重复验证。
- 代码生成和 dump 输出不再依赖 CRT 文本输出兼容宏。
- 文件、目录、时间、输出等基础能力优先建立在 XRT 上。
- 诊断输出具备文本和 JSON 两条稳定路径。
- 测试与范例覆盖当前已实现的语言、运行时、标准库、import/package、DLL 和并发能力。
- async/task/thread/channel/sync 等并发模块具备专项边界测试、重复 soak 和完整主回归证据。

## 已完成能力

### 工程守门

新增：

- `rc0_guard.bat`。
- `tools/rc0_guard.ps1`。
- `docs/RC0工程基线.md`。
- `docs/RC0工作区清单.md`。
- `docs/RC0变更摘要.md`。

守门内容：

- 必需文件检查。
- 禁止回退静态扫描。
- XRT/CRT 边界检查。
- 构建检查。
- CLI 命令面检查。
- 文本/JSON 诊断检查。
- 全量测试。
- diff 空白检查。

### 输出系统

完成：

- cgen 输出统一为 `XL6_Output`。
- dump 输出统一为 `XL6_Output`。
- 文件输出走 XRT `xbuffer`、`xrtOpen`、`xrtPut`。
- stdout 直出也可先进入缓冲输出层。
- 删除 `xl6_output_compat.h`。

守门限制：

- `src/cgen`、`src/common`、`src/abi`、`src/ast`、`src/ir` 中禁止 `fputc`、`fputs`、`fprintf`。
- `src/cgen` 禁止直接依赖 `FILE*`、`stdout`、`stderr`。

### XRT 边界

完成：

- 删除 `xl6_file.c`。
- 删除 `xl6_time_literal.h`。
- 文件读取改用 XRT。
- 文件存在检查直接调用 XRT。
- 临时文件和目录删除改用 `xrtFileDelete`、`xrtDirDelete`。
- 输出文件写入改用 XRT 文件对象。

当前允许：

- CLI 和诊断层对 `stderr` 的流程级输出。
- `snprintf` 用于固定缓冲区安全格式化。
- flex/lemon 生成代码内部 CRT 使用。
- CLI 调用 TCC、生成 loadlib import definition 和运行临时 exe 已迁移到 XRT process API，不再保留 `system` 例外。

### XRT future 与单头同步

完成：

- `xFutureForwardCancelTo(...)` 在 source 已完成的分支中释放临时 target 引用和转发上下文，避免取消转发边界上的引用泄漏。
- `future_core` 增加聚焦测试，覆盖已完成 source 转发到未完成 target 时的引用计数回落和后续 resolve 能力。
- 重新生成 `D:\GIT\xrt\singlehead\xrt.h`。
- 同步更新 `demo6/lib/xrt/xrt.h`。

当前证据：

- `D:\GIT\xrt\release\x64\test.exe future_core` 通过。
- `D:\GIT\xrt\singlehead\xrt.h` 与 `demo6/lib/xrt/xrt.h` 的 SHA256 一致：
  - `F9DEF8CEBA4FB5DA7B9925F383BF541BF720840B9DE15AF71C5C4D8C045A543B`

### cgen 结构

完成：

- 原大型 `xl6_cgen.c` 已拆分为多个 cgen 分片。
- 拆分后保留统一入口。
- `header`、`runtime`、`lifecycle`、`needs` 等低耦合分片已经独立。
- 代码生成输出路径统一收口。

代表性模块：

- `xl6_cgen_entry.*`。
- `xl6_cgen_header.c`。
- `xl6_cgen_runtime.*`。
- `xl6_cgen_lifecycle.*`。
- `xl6_cgen_abi.c`。
- `xl6_cgen_args.c`。
- `xl6_cgen_callable_*.c`。
- `xl6_cgen_stmt_*.c`。
- `xl6_cgen_type_*.c`。
- `xl6_cgen_value_*.c`。

### ABI、XAPI 与 import/package

完成：

- ABI dump 文本和 JSON 输出。
- XAPI JSON 生成。
- XAPI JSON 加载。
- import 从 XAPI/package 读取声明信息。
- package resolver 基础实现。
- C export ABI 和头文件生成测试。
- 混编 build/run 链路测试。

当前定位：

- XAPI 是 xl 的内部接口声明信息，可持久化用于 package/import/C 互通。
- 用户不需要手写 XAPI。
- C 自动获得 xlang 导出声明，xlang 仍通过显式 import 获得外部能力。

### 诊断系统

完成：

- 文本诊断保留阶段标记。
- JSON 诊断增加 `phase` 字段。
- RC0 守门检查语义错误和 import 错误的文本/JSON 输出。
- `channel.recvInto` 与 `channel.tryRecvInto` 的写目标诊断使用各自 API 名称，避免负例定位混淆。

JSON 诊断格式基线：

```json
{
  "ok": false,
  "phase": "sem",
  "diagnostics": [
    {
      "file": "test\\cases\\sem_unknown_type.xl",
      "line": 1,
      "column": 1,
      "message": "..."
    }
  ]
}
```

### async/task/thread/channel 压实

完成：

- 新增 `tools/check_async_task_soak.ps1`，用于 async/task 专项重复运行。
- soak 脚本捕获 stdout/stderr 时立即异步复制到文件，避免子进程输出较多时阻塞管道。
- `task.spawn` 的动态 `var` 参数 lowering 覆盖 range 边界、range contains、f-string、索引读写、安全索引/null coalesce、typed 容器 iterable、generator iterable 等路径。
- `task.all` 的取消转发生命周期收口：`groupFuture` 在成功、启动失败和转发失败路径均释放。
- `xl6_task_spawn_start(...)` 生成代码不再输出无用 `groupFuture` 局部变量。
- channel、sync、thread 的 captured receiver 用例补齐正向运行与 C 形态守门。
- captured `thread.stop()` 用例改为慢线程，确保测试语义覆盖运行中线程的停止请求路径。

当前证据：

- async/task 默认 soak 已接入 `test.bat` 和 `rc0_guard.ps1 -SkipFullTests`。
- `tools/check_async_task_soak.ps1 -Repeat 20 -IncludeHeavy` 通过：
  - 20 轮重复。
  - 每轮 7 个 `xl run` 入口。
  - 每轮 2 个 build/run case。
  - heavy 路径包含 `release/examples/task_api.xl` 与 `test/cases/cgen_task_api.xl`。
- 最新完整 `test.bat` 中默认 async/task soak 通过：1 轮、6 个 run 命令、1 个 build/run case。

### 测试与范例

当前覆盖：

- `test/cases`：668 个 `.xl` 测试用例。
- C/H fixture：45 个入口 fixture，55 个 package/extlib native fixture。
- `release/examples`：104 个 `.xl` 范例文件，其中标准库范例 17 个。
- package/extlib fixture：48 个。
- 测试数据 fixture：5 个 JSON/XAPI fixture。
- 标准库覆盖：180 个公开函数、21 个标准库测试、17 个标准库范例。
- 负例诊断守门：429 条预期失败命令、428 个诊断产物、429 条内容断言，baseline-only 为 0。
- `feature_suite` 覆盖基础语言、控制流、函数、类型、容器、泛型、闭包、class/object/native-backed class、union、C 混编、C export ABI、错误处理、文档注释、并发和任务。

### 当前完整验证证据

本轮最终审查修正后，已经完成以下主链验证：

- `cmd /c test.bat` 完整通过。
- `powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\rc0_guard.ps1 -SkipFullTests` 通过。
- `powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\check_async_task_soak.ps1 -Repeat 20 -IncludeHeavy` 通过。
- `powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\check_markdown_links.ps1` 通过。
- `powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\check_docs_index.ps1` 通过。
- xlang 与 XRT 两个仓库的 `git diff --check` 均通过。

最新完整主回归尾部确认：

```text
Negative diagnostics artifacts ok: 428 files, 429 expected-failure commands, 429 asserted, 0 baseline-only.
Release tmp artifacts ok: 882 text files.
DEMO6 smoke tests ok.
```

## 当前非目标

以下内容不作为本轮压实任务继续扩张：

- 未进入当前设计文档口径的新语言特性。
- 用户手写 XAPI 格式承诺。
- 继续把 `test.bat` 拆成更细的独立子套件。
- 新的 package 管理器命令和远程依赖获取能力。
- 非 TCC 后端的正式支持。

这些能力可以在当前发布候选基线稳定后，作为下一轮设计和开发任务继续推进。

## 当前风险与后续建议

仍需关注：

- `sem` 仍是最大单体模块，后续可按类型检查、调用检查、class/native-backed class、泛型等边界拆分。
- `test.bat` 体量很大，后续可拆成 smoke/sem/cgen/import/export 多个脚本。
- 中文诊断文本需要持续保持 UTF-8 源文件口径。
- `thread.stop()` 当前表示停止请求已发出，不等同于强制中断线程；未来如果设计 cooperative stop 查询 API，应另行补行为级测试。
- async/task/future/channel/thread/sync 仍是发布前最值得继续做长轮 soak 的模块。

建议下一步：

1. 审查当前基线并决定是否提交。
2. 提交后以 `rc0_guard.bat` 作为进入下一轮功能开发的固定门槛。
3. 封版前如有时间，可执行 50 轮 async/task heavy soak，进一步提高并发路径的偶发竞态发现概率。
4. 下一轮再推进 package 管理器、非 TCC 后端或 driver 后端抽象优化。
