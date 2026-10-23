# 自动诊断的文件输入证明

服务 0.22 为 [自动原生诊断](lsp-automatic-diagnostics.md) 增加可协商的文件级元数据。它用于核对诊断位置对应的正文，以及跳转到磁盘文件前核对原始字节和文件身份。数据来自该次已验证发布分组，复用原快照格式，不重新读取文件或生成另一份分析报告。

客户端初始化时声明：

```json
{ "capabilities": { "experimental": { "diagnosticInputProof": true } } }
```

服务在 `capabilities.experimental.diagnosticInputProof` 声明支持。请求能力必须是布尔值；未声明或为 false 时，诊断通知没有下述扩展。当前仅自动原生诊断使用此扩展，显式关闭自动诊断后仍保留既有手动批次格式。IDE 客户端尚未启用，需继续接入同步与过期保护。

每个 `textDocument/publishDiagnostics.params` 可以带一份 `xlang`：

```json
{
  "uri": "file:///D:/example/main.xl",
  "version": 7,
  "diagnostics": [],
  "xlang": {
    "schema": 1,
    "generation": "12",
    "cleared": false,
    "document": {
      "path": "D:/example/main.xl",
      "origin": "overlay",
      "version": 7,
      "normalizedSize": 4,
      "normalizedSha256": "<64 lowercase hex characters>"
    }
  }
}
```

例子中的大小和摘要仅示意字段，不是一份可验证的真实正文。元数据在文件级携带一次，不重复放入各条诊断。`document` 取原分组的 `path/origin/version/normalizedSize/normalizedSha256/input` 字段，排除诊断数组；可选字段继续遵循原分组格式，不携带正文、AST、IR、整批其他文档或解析报告。

`origin:overlay` 有当前线上版本及规范化正文的 UTF-8 字节数／SHA-256；`origin:disk` 没有版本，另带原报告的 `input`，包括原始字节数／SHA-256、来源及文件系统元数据。磁盘 BOM／CRLF 的原字节摘要与编辑器 LF 正文摘要分别保留，同字节文件替换仍会改变身份。客户端应复用 [快照检查](language-service.md) 的严格路径、格式、容量、哈希及文件身份验证。

`cleared:false` 表示这份已分析的文件贡献，诊断可以为空；空列表此时表示该分析没有此文件错误。`cleared:true` 表示撤回先前发布，`document` 缺省且诊断为空，不能据此认定完成了一次无错误检查。关闭文件也使用撤回语义。标准 `params.version` 仍按 `publishDiagnostics.versionSupport` 协商，关闭清除可以省略版本；显式选择此扩展的客户端仍能在 overlay 证明中读取原线上版本。

`generation` 是当前服务进程的输入排序上下文，以十进制字符串传输，避免客户端数字精度丢失。它不是跨进程持久身份、客户端同步确认或整份语义有效性的证明。磁盘观察和不同入口可以产生同代次的不同结果，不能只比较该字段便接受诊断。

客户端仍须验证会话、文档身份和版本，并正确处理尚未发送／确认的依赖修改、输入集合变化、配置、外部冲突及文件观察。仅凭当前文件的相同哈希，不能证明诊断所依赖的其他输入没有变化；本切片不独立完成 IDE 的全部过期保护。后续 IDE 接入必须补齐这条输入同步边界。

服务 0.23 增加共享的 [在线文档同步确认](lsp-input-state.md)，可以核对已经处理的 URI／版本集合及输入代次。它仍不代替正文证明、本地模型身份或语义依赖有效性检查。

扩展使用固定字段投影，对已存在的 XRT 元数据建立短期引用，消息发送后释放；不深拷贝整份分组或诊断、不启动分析／观察线程、不在协议 owner 访问文件系统。原发布帧、诊断及报告预算继续生效；没有新增峰值内存或最终延迟结论。

专项入口为 `tools/check_lsp_diagnostic_proofs.py --server <本次服务> --output <新目录>`。真实 stdio 覆盖协商关闭、Unicode overlay、与原生检查元数据完整相等、干净结果／撤回区别、编辑版本、URI 拼写及槽位重开、版本能力、BOM／CRLF 磁盘证明、同字节身份替换、磁盘转 overlay、手动兼容、非法能力与初始化恢复、源文件／工具指纹。证据与正式构建记录见 [长期实施进度](lsp-development.md)。

## 0.26：共享的精确依赖范围

IDE 等待新输入分析时，需要保留未受影响的诊断，并立刻撤销受影响的旧结果。仅有文件正文证明无法完成这项判断。客户端可同时声明两个布尔能力：

```json
{ "capabilities": { "experimental": {
  "diagnosticInputProof": true,
  "diagnosticDependencyProof": true
} } }
```

服务在同名能力声明支持；后者要求前者为 true。未协商依赖能力时，原来的文件证明和标准通知格式保持。扩展仅作用于自动原生诊断，不改变显式手动批次。当前 IDE 尚未启用这条自动诊断路径。

新增通知 `xlang/diagnosticDependencies.params` 描述一份会话内共享字典：

```json
{
  "schema": 1,
  "id": "<64 lowercase hex characters>",
  "removed": false,
  "part": 0,
  "parts": 1,
  "paths": ["D:/example/main.xl", "D:/example/lib.xl"]
}
```

`part` 从零开始，同一 id 的分片按顺序传输；客户端收到全部分片、完成校验后才可引用。服务在对应的 `publishDiagnostics.params.xlang.dependencies` 写入该 id，原来的 `document` 证明继续必需。字典没有源码、AST、IR 或文件内容摘要。

范围是**实际为该文件贡献诊断的所有入口**的输入路径并集，包括各入口实际加载的文件、包清单和所有观察过的导入解析候选，候选即使不存在也保留。它与服务输入守卫的 Touches 采用相同保守规则，不声称已经实现符号或函数级依赖图。无关的打开文件不会仅因在捕获集合中而加入。干净文件也有完整范围，不能只为非空诊断建立失效规则。

各集合先按原生词法路径键排序、去重；Windows 使用规范化绝对路径及原来的序数大小写比较，同键使用原始 UTF-8 字节比较选择固定代表。不同入口掩码产生相同集合时复用字典；相同摘要仍用完整数组相等校验，冲突时拒绝发布。摘要描述传输中的**有序路径数组**，编码为：

1. ASCII `xlang-diagnostic-paths-v1` 及一个 NUL 字节。
2. 四字节大端无符号路径数。
3. 按数组顺序写入每项四字节大端 UTF-8 字节长度，再写入路径字节。
4. 使用 XRT SHA-256 输出 64 位小写十六进制。

共享 `protocol/diagnostic_paths` 验证最多 163840 项、每项 1..65535 字节、有效 UTF-8 且不含 NUL；它不替调用方排序、解析路径或验证绝对性。服务生成器只投影原本已验证的输入路径。空数组独立固定向量为 `e3d55c9066bce93a2936028873706e9f77d091b1c2bc26898cd070bfb0a55408`。

每个字典的所有声明均先于引用它的文件通知；已知 id 不在连续发布中重复声明。即使诊断文字没有变化，依赖集合变化也重新发布该文件绑定。所有引用都被更新或撤回后，服务才发送释放通知：

```json
{ "schema": 1, "id": "<64 lowercase hex characters>", "removed": true }
```

释放后同一 id 可以再次声明。服务重连必须清空整个字典与引用，不可把 id 当作跨会话有效性证明。`cleared:true` 不带依赖引用；撤回、干净结果和关闭的语义与上文相同。shutdown 响应后不再发布，也不发送额外释放通知。

构造、合并、排序、去重、摘要和分片计量均在原诊断 actor 完成，复用 XRT 数组／映射／摘要和不透明输入守卫，不增加每入口线程。每个分片的 JSON params 不超过 256 KiB；唯一字典分片数组的编码总量另受 `retainedReportBytes` 预算限制，默认 32 MiB。原有记录报告 32 MiB、文件发布报告 4 MiB 预算分别保留，不能把这些编码预算相加后宣称为进程堆或 RSS 上限。超限、取消或编码失败时整个事务失败，存储不提交部分字典。协议 owner 仍需编码和发送通知；本阶段没有证明 UI 1 ms、最终诊断 P95 或大工程性能目标。

新结果仍需通过会话／本地模型身份、线上版本、文件正文证明和 [在线输入确认](lsp-input-state.md)。字典只为本地变化提供精确路径失效范围，不携带同步确认；不能靠 id 或相同文件哈希独立认定语义仍有效。默认自动诊断的 IDE 控制器、后台接收校验和问题面板合并继续实施。

专项 `check_lsp_diagnostic_dependencies.py` 使用真实 stdio 和独立 Python 摘要编码，覆盖协商、共享入口并集、依赖变化但错误文字不变、无关结果保留、关闭／最后引用释放、干净结果、负候选创建、实际包清单、Unicode、多分片及非法能力恢复。长路径场景验证错误入口的完整负候选范围；现有共享编译器源码 import 使用 512 字节路径缓冲，本阶段没有扩大这一上限或宣称长路径源码导入成功。原生守卫／存储和两种 XRT 所有者的摘要夹具同时执行。
