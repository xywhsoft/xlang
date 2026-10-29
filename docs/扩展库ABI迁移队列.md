# 扩展库 ABI 迁移队列

本文只记录仍在使用旧 XRT ABI 的已有实现。它们不属于占位库，也不受首期占位库
`10000` 行新增实现预算限制；但在完成迁移和真实验证前，不得作为可发布能力宣传。

## 统一约束

1. 不保留 `xvo`、旧 `xrtSetError(message, take)`、旧 Future/Promise 的兼容宏或假实现。
2. `xvalue` 是当前 XRT 的不透明、引用计数对象，只通过 `xvalue*` 和所有权 API 传递。
3. XRT 容器使用当前 API 的零基索引、显式初始化和明确销毁规则；不得把旧容器布局强转为新布局。
4. native 包直接包含当前 `xrt.h`，由 xlang/TCC 上下文提供 XRT 符号；包不得复制 XRT 实现。
5. 每个迁移阶段必须有独立 C 测试、xlang 测试和资源释放测试，再开放上层包。

## 图形基座：_xge_native、xge、xge_shape_ex、xui

现状：`_xge_native` 已有实质图形、UI 和工作流实现，但仍混用旧 `xvo` 动态值、旧容器布局和
旧回调帧。`xge`、`xge_shape_ex`、`xui` 因此必须保持依赖阻塞状态。

已完成的无歧义修正：

- Windows TCC 运行 VFS 已补入 XGE 所需的 `windowsx.h`、`strings.h`、`imm.h`；外部文件仍优先。
- 路径读取改为当前 XRT `xrtPathCwd`、`xrtPathJoin`、`xrtFileReadAll`。
- 图形错误改为 `xrtSetErrorInfo` 的结构化错误模型。
- 渲染和异步加载线程改用当前 `xthread*`、`xrtThreadWaitFor` 与 `XWAIT_TIMEOUT`。

后续必须按模块拆分，不做全库机械替换：

1. 回调桥：先将帧和 UI 回调迁移到 `xrtcallable`/当前 xlang callable bridge，明确保留、释放和线程亲和性。
2. 值容器：依次迁移 dock、inventory、workflow、flow graph。每个模块将旧 table/array 操作改为
   当前 `xvalue*`、`xrtValue*` API，并补全成功、失败和析构测试。
3. 普通容器：将旧数组/字典按当前 `xarray`/`xmap` 的零基、显式元素大小和析构规则重写；
   不允许布局强转或沿用一基索引 helper。
4. 图形与平台：在无窗口 smoke、实际窗口、输入法、线程渲染、资源载入五类门禁均通过后，
   才解除 `xge`、`xge_shape_ex`、`xui` 的阻塞状态。

## 当前验收口径

- 已通过 `tools/run_phase1_extlibs.ps1` 的 59 个包可作为首期已验证基线。
- ABI 迁移包必须先完成真实构建；仅能解析 import、仅能 dry-run 或仅含外部声明均不计通过。
- 状态由 `tools/audit_extlib_ecosystem.ps1` 生成的 JSON/报告统一给出。
