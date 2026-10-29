# xlang 光标语义补全服务

2026-10-07，`xlang-ls.exe` 0.4 已提供标准 `textDocument/completion`。它读取当前打开文档的不可变全文快照，独立调用真实编译器光标查询，支持半输入标识符、整个标识符替换和 class 成员点号后的零宽度插入。作用域遮蔽、重载身份、规范类型和声明位置均来自本次编译器查询。候选不能当作整份编译成功的证明。

本页记录后台与协议合同。后续已接入 [原生候选](native-completion.md)，使用独立连接同步当前输入、组合语义和目录候选，并验证稳定身份、版本、光标、焦点和来源，支持 Ctrl+Space/点号与 Enter/Tab 整段提交、一次撤销。C 语义、完整工程输入及其余首版事项继续实施。定版 Dock、中央底栏和单栏帮助保持。

## 请求与能力

初始化返回 `completionProvider: { resolveProvider: false, triggerCharacters: ["."] }`，实验能力 `cursorCompletion: true`、`completionInputGuard: "background-poll"`；`cSemantics` 仍为 false。服务实现接口，实际编译器必须在真实 check 帮助行声明 `--json`、`--source-overlay`、`--source-report` 和 `--completion-at`。旧编译器会在查询前返回明确失败，原诊断功能继续可用。

```json
{
  "textDocument": { "uri": "file:///D:/Example/main.xl" },
  "position": { "line": 3, "character": 14 },
  "context": { "triggerKind": 1 }
}
```

位置为零基 UTF-16，必须是打开 `.xl` 文档中的标量边界。标准请求无需版本扩展；可选 `version` 必须等于当前已同步全文版本。BOM 前、代理项中间、越界和未打开文档位置均拒绝。每份最多 8 MiB、64 份合计 32 MiB，仍受 8 MiB JSON 帧限制。请求 ID 支持整数与 UTF-8 字符串，补全字符串 ID 最多 65,535 字节。

默认以查询文档作为编译入口；可选 `entryDocument: { uri: "..." }` 指定另一个已打开入口，用于按工程入口的实际导入上下文查询依赖。结果版本取查询文档，不能误用入口版本。所有已打开命名 `.xl` 全文均被覆盖，原文件不保存、不改写。

## 响应与替换范围

结果是 `CompletionList`，`isIncomplete: false`，包含候选的 `label/kind/detail/filterText/sortText/insertTextFormat: 1/textEdit`。`textEdit` 为 UTF-16 的半开范围与普通 `newText`，覆盖光标所在整个标识符，包括光标后的已有后缀；点号后使用零宽度范围。

候选 `data.symbolId` 是本次查询中的真实符号 ID；存在原源码声明时附带 `data.declaration` Location。身份只在该次查询中使用，不能跨检查或跨查询比较。类型可能没有规范字符串，此时省略 detail；没有真实声明范围的隐式实体不伪造声明位置。

结果附带 `queryId/version/scope: "cursor-query"/checkedCompilation: false/guardScope: "background-poll"`、查询 `prefix/replacementRange` 及 `available/error/phase/durationMs/ok`。`ok/available` 表示取得查询候选事实，与正常 check 成功分开。无法建立前置语义条件或未支持接收对象时，合法查询返回 `available: false`、空 items 和原因；格式、来源或容量无法验证则整批返回协议错误。

解码器核对 schema、编码、范围约定、查询路径和原字节偏移、前缀、整个 token、候选类别/身份、规范类型容量、所有声明源及范围。排序后逐源码扫描，同时核对 UTF-8 偏移、一基 Unicode 标量行列和 UTF-16 位置；正确处理 BOM、CRLF、中文及非 BMP。错误字段不会降级成部分候选。

## 并发、取消与持续输入守护

补全拥有独立于诊断/定义/大纲/悬停的工作线程和结果守护。它不发布诊断、不清除成功检查索引；可以和 `xlang/check` 同时运行。服务最多保留一个正在运行或守护的补全对象，以及一个最新等待请求。

新查询取消旧请求并替换等待请求。被替换、显式取消、关闭和关机返回 `-32800`；打开集合或正文变化返回 `-32801`；坏参数为 `-32602`；工具链能力、进程、来源或解码失败为 `-32603`。owner 派发不等待编译器执行或后台磁盘扫描，只在线程结束后回收旧对象。命名和路径别名检查沿用现有绑定流程。`$/cancelRequest` 同时处理正在运行和等待的补全。

成功响应保留期间，后台按一秒加扫描耗时复核实际磁盘来源/包描述、普通文件候选状态、进程包环境和 `xl.exe/xlc.dll/xl.dll` 身份。变化或已发布请求被取消/替换时发送 `xlang/completionInvalidated`，参数为 `queryId/uri/version/reason`。守护失效永久，客户端应先关闭候选窗格、释放借用字符串，再建立当前请求。当前 GUI 已处理此通知，提交前还排空已到达的通知，详细生命周期见 [原生补全](native-completion.md)。

## 容量与验收

编译器查询整体输出最多 12 MiB；已加载图仍为 512 文件/64 MiB。解码最多 4096 候选、4 MiB 名称与类型文本、名称 512 字节、类型 8192 字节。逐项序列化测量名称、类型与声明 URI，在追加到列表前计算聚合预算，避免长路径在大量候选中重复占用内存；整个列表序列化后另限制为 7 MiB，为 8 MiB JSON-RPC 帧留出元数据和 ID 空间。超限整批拒绝，服务仍能继续处理请求。

`tools/check_language_completions.py` 使用真实冻结编译器及实际 stdio 服务，也编译受控 CLI 夹具验证坏响应、取消和来源竞态。复跑：

```powershell
python .\tools\check_language_completions.py --compiler .\build\toolchains\demo6-completion-a96b23d8-16c9-4eae-9da7-6d082d45f5a7\xl.exe
.\tools\test.ps1 -Configuration Release -CompilerPath <xl.exe> -CompletionCompilerPath <completion-xl.exe>
```

本轮结果与修复过程见 [进度](progress.md)、[最终核验](../build/completion-lsp-final-verification.json) 和 [可复跑测试](../tools/check_language_completions.py)。编译器支持/未支持语法范围见 [编译器查询合同](../../docs/IDE语义补全查询接口.md)。