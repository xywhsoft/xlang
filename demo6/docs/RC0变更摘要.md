# DEMO6 RC0 变更摘要

本文档记录 DEMO6 当前 RC0 基线的主要变化，供提交前审查和后续开发接续使用。

## 核心目标

RC0 的目标不是继续扩张语言特性，而是把已经实现的基础能力压实：

- 编译器可以稳定构建。
- 主要命令面可以重复验证。
- 代码生成和 dump 输出不再依赖 CRT 文本输出兼容宏。
- 文件、目录、时间、输出等基础能力优先建立在 XRT 上。
- 诊断输出具备文本和 JSON 两条稳定路径。
- 测试与范例覆盖当前已实现的语言基础能力。

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

### 测试与范例

当前覆盖：

- `test/cases`：446 个 `.xl` 测试用例。
- C/H fixture：41 个入口 fixture，9 个 package native fixture。
- `examples`：57 个 `.xl` 范例文件，其中标准库范例 17 个。
- 负例诊断守门：245 条预期失败命令、244 个诊断产物、245 条内容断言，baseline-only 为 0。
- `feature_suite` 覆盖基础语言、控制流、函数、类型、容器、泛型、闭包、class/custom、C 混编、C export ABI。

## 不属于 RC0 的范围

以下内容不在当前 RC0 继续扩张：

- 新增高级语言特性。
- 完整 package 管理器。
- 用户手写 XAPI 格式承诺。
- 继续把 `test.bat` 拆成更细的独立子套件。
- object 动态对象系统。
- f-string 插值实现。
- 完整跨平台 DLL 入口抽象。

这些能力可以在 RC0 基线稳定后继续设计和实现。

## 当前风险与后续建议

仍需关注：

- `sem` 仍是最大单体模块，后续可按类型检查、调用检查、class/custom、泛型等边界拆分。
- `test.bat` 体量很大，后续可拆成 smoke/sem/cgen/import/export 多个脚本。
- 中文诊断文本需要持续保持 UTF-8 源文件口径。

建议下一步：

1. 审查当前基线并决定是否提交。
2. 提交后以 `rc0_guard.bat` 作为进入下一轮功能开发的固定门槛。
3. 下一轮再推进 import/package 的正式扩展或 driver 后端抽象优化。
