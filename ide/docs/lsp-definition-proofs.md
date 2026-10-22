# 定义位置的目标文件证明

服务 0.24 提供可选扩展，供 IDE 安全接受无需手动检查的跨文件定义位置。客户端初始化提交 `capabilities.experimental.definitionInputProof: true`；服务通过同名 experimental 能力声明支持。该客户端的非空 `textDocument/definition` 结果为单个 Location，并增加如下字段：

```json
{
  "xlang": {
    "schema": 1,
    "document": {
      "path": "D:/Example/helper.xl",
      "origin": "overlay",
      "version": 7,
      "normalizedSize": 42,
      "normalizedSha256": "64 lowercase hexadecimal characters"
    }
  }
}
```

上例省略标准 `uri/range`，示意摘要需替换为真实 SHA-256。路径、来源、规范化 UTF-8 正文大小及摘要复用检查结果的既有字段。覆盖文件带其同步 `version`，不带 `input`；磁盘文件不带版本，改带原有 `input`：路径、disk 来源、原字节大小与 SHA-256，以及 `available/attributes/device/identity/modifiedUs/changedUs` 身份元数据。原字节可含 BOM／CRLF，规范化正文与编辑器一致，两种摘要不能混用。

不协商或显式 false 保持仅含 `uri/range` 的原格式。非布尔能力返回无效参数，不提交初始化状态；客户端可修正后重新初始化。无可验证定义返回 JSON null，不合成目标证明。

核心接口 `xideLanguageCheckDefinitionWithProof` 与原定义接口共用锁、完成状态、取消和输入守护检查；从本次实际文件报告和同次结果目标组投影至多六个元数据字段。无实际来源或目标报告时失败，不退回无证明跳转。返回值独立拥有，允许检查销毁后读取；不保留 AST／IR，不复制整批文件或诊断，不访问磁盘。普通定义接口保持原格式。

IDE 还需确认在线输入集合及所有本地模型身份、版本、源光标和活动标签。目标覆盖文件逐项匹配当前模型和正文；磁盘文件在打开前后核验身份，打开后核验原字节和正文摘要，最后严格转换 UTF-16 范围。本扩展不证明瞬时的完整依赖有效性；原有输入守护仍按复核耗时加一秒观察变化，跨文件／工程增量仍属于后续工作。

验证入口：[核心所有权与并发](../tests/language_definition_proof_test.c)、[真实协议](../tools/check_lsp_definition_proofs.py)、[IDE F12](../tests/definition_view_test.c)。128 个真实导入输入的请求仍只携带一个目标的固定元数据，不返回全批次报告；这不是最终时延或 RSS 性能验收。
