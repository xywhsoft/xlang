# WorkspaceEdit 的不可变编辑准备层

M7b4u，2026-11-11。`protocol/workspace_edit` 为 IDE 的语义重命名、格式化和代码操作准备完整候选正文。任一文件的版本、来源、范围、额度或分配失败，整个准备失败；不修改编辑器、撤销栈或磁盘，也不返回已完成前缀。这是后续原子应用的准备阶段，尚未启用 `renameProvider` 或 `workspace/applyEdit`。

输入直接持有现有 `xls_sync` 的不可变 `xls_text` 租约，来自发起请求时的文件组。全部编辑针对同一原始正文；不逐项调用会重新复制正文的顺序文本应用接口。复用既有严格 UTF-16 批量坐标转换，排序后一次拼接完整正文，每个改变文件只构造一个结果快照。结果快照内部仍会复制拼接缓冲并建立行索引，不能称为零复制；源码正文、坐标映射及规范化也有扫描成本。

支持 `documentChanges`、兼容的 `changes`、整数/未知版本和 `AnnotatedTextEdit`。两种表示同时存在时优先 `documentChanges`；保留相同起点的原有数组顺序，拒绝重叠。插入内容中的 CRLF/CR 转为 IDE 内部 LF；源快照必须为无物理 BOM 的 LF 正文。注释标签、说明和确认要求独立保存，不因原 JSON 释放而丢失。资源操作与片段操作明确拒绝，不能当作普通文本编辑执行。

版本化行须匹配捕获的线上版本；旧版本、未知文件、重复的词法路径身份、代理项内部坐标、越界、反向及重叠范围均拒绝。`xlang` 来源证明可要求存在，检查规范化正文摘要、长度、覆盖版本及来源类别；关闭文件的原始字节摘要只作为待核验声明保存，准备层不冒称验证了磁盘。实际原生 `rename_plan` 编码器的覆盖文件、关闭文件和 BOM/CRLF 输出均通过往返检查。

返回的文件、编辑及注释视图借用不可变准备对象；调用者持有对象期间有效。需要单独保存正文时使用 `xlsTextRetain`，不得直接释放借用视图。对象通过 XRT 原子引用计数共享，不保留原 JSON、AST、检查或 GUI 对象。四个读者在原输入/JSON及创建方引用释放后实际并发读取。

提交前的 `StillCurrent` 只比较相同不可变正文租约、文档身份、64 位修订、线上版本和捕获 URI，不分配或扫描正文。必须逐文件检查，并另行核对请求/连接代次、配置、项目成员、物理别名与可写性、关闭文件的实际字节及窗口生命周期。该函数不是写入授权，不能代替 XUI 的多文档提交/回滚和恢复记录。当前 XUI 的 BeginEdit/EndEdit 只合并撤销项，不能据此声称已有原子事务。

默认/最大已知存储额度为 128/256 MiB，最多 512 个输入/目标、65,536 个编辑、4,096 个注释；每文件正文 8 MiB，before/after 正文分别总计 64 MiB，捕获 URI 总计 1 MiB，替换字符串总计受传输的 8 MiB 上限约束。保守计入每文件持有的快照容量、数组、字符串和本模块暂存；分配器、路径转换、坐标帮助函数和结果快照构造的内部瞬时存储额外。完整 UTF-8 验证、库排序、路径帮助函数及快照构造仍有不可抢占部分，不将局部检查点当作端到端取消或 RSS 保证。

三个新程序通过 24,077 项主检查、97 项边界检查及分配/取消/生命周期检查。12,000 个逆序单行编辑实际只调用一次结果快照构造；38 个观测到的冷态分配拒绝点各在独立进程运行，14 个取消点逐点检查。早期同进程故障枚举受冷暖分配差异影响的失败保留，未缩减冷态验收。五个实际变异分别破坏版本检查、同起点顺序、修订围栏、来源摘要与整体失败返回，均被正常测试拒绝。

IDE 使用匹配 SDK 重新编译 117 个对象，28 个已完成 GUI 回归原件与后续核心/旧能力检查合并；再补充匹配 GUI/XRT 下的边界程序，共 31 项实际检查。历史 GUI 运行时没有内存统计 API，只有测试观察器按配置省略该 API；生产接口和行为没有裁剪，分配拒绝/平衡在编译器 XRT 配置验证。当前服务及其 189 个既有原生程序的源码/头文件/对象按实际依赖保留，未重跑冒领；服务程序摘要不变，也不重新声称旧协议或全目标性能通过。

材料索引为 `dev/release-materials/provenance/lsp-workspace-edit-staging-20261111.json`。保留全部编译失败、故障枚举失败、GUI 观察器适配失败、最终配方及实际对象。临时程序在根 `release/win_x64` 运行并清理，默认运行程序及交付索引保持。后续继续 XUI 原子提交/撤销、物理文件围栏、完整静态来源覆盖、协议/IDE 入口及整体性能和交付。

协议依据：[LSP 3.17 TextDocumentEdit](https://raw.githubusercontent.com/microsoft/language-server-protocol/gh-pages/_specifications/lsp/3.17/types/textDocumentEdit.md)、[WorkspaceEdit](https://raw.githubusercontent.com/microsoft/language-server-protocol/gh-pages/_specifications/lsp/3.17/types/workspaceEdit.md)；相同起点排序与 Microsoft 的 [TextDocument.applyEdits 参考实现](https://github.com/microsoft/vscode-languageserver-node/blob/main/textDocument/src/main.ts)交叉核对，不把准备成功当作标准事务能力已完成。
