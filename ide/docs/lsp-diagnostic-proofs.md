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

扩展使用固定字段投影，对已存在的 XRT 元数据建立短期引用，消息发送后释放；不深拷贝整份分组或诊断、不启动分析／观察线程、不在协议 owner 访问文件系统。原发布帧、诊断及报告预算继续生效；没有新增峰值内存或最终延迟结论。

专项入口为 `tools/check_lsp_diagnostic_proofs.py --server <本次服务> --output <新目录>`。真实 stdio 覆盖协商关闭、Unicode overlay、与原生检查元数据完整相等、干净结果／撤回区别、编辑版本、URI 拼写及槽位重开、版本能力、BOM／CRLF 磁盘证明、同字节身份替换、磁盘转 overlay、手动兼容、非法能力与初始化恢复、源文件／工具指纹。证据与正式构建记录见 [长期实施进度](lsp-development.md)。
