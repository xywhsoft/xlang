# 标准请求的按需语义分析

开发版服务 0.20 的默认原生诊断后端允许外部客户端直接发送 `textDocument/definition`、`textDocument/hover` 和 `textDocument/documentSymbol`。文档须先通过 `didOpen` 同步；无需先调用 `xlang/check`，也无需提供自定义 `version` 或 `checkId`。初始化的 `experimental.automaticSemanticQueries` 表示这项能力，显式 `diagnosticsBackend: "cli"` 不启用它。

已有当前索引时直接查询。没有索引时，服务保留请求 ID、文档身份／版本、位置、方法和输入集合代次，异步提交现有的原生完整检查会话；同入口的请求共享检查。最多保留 64 个请求，字符串 ID 最多 65,535 个 UTF-8 字节，不为每个等待者复制正文或整个参数对象。不同入口按先后处理；已经进行的显式检查保持其生命周期，前台光标补全仍使用独立 actor。

检查结束后先验证输入、发布诊断，再回复等待者。成功返回真实定义／悬停／符号列表；合法位置没有目标时返回空结果。源码错误可发布真实诊断，但没有完整语义事实，因此定义与悬停返回 `null`、文档符号返回空数组。基础设施、分配和索引容量失败返回错误，不能降级为空成功。单次失败会结束本次等待，不自动反复提交同一个请求。

`$/cancelRequest`、关闭、正文变化、输入集合变化和 shutdown 都会清理等待者；文档槽位重新使用还须匹配独立身份。没有请求继续需要的按需检查会协作取消，不在协议 owner 等待后台线程。源码或依赖改变返回 `ContentModified`，取消／关闭返回 `RequestCancelled`；过长 ID 和非法位置返回参数错误，队列满返回服务繁忙。取消目前仍受共享前端检查点限制，尚未证明细粒度 CPU 中断或最终取消时延。

显式提供 `checkId` 的调用继续绑定那一次检查；过期批次不能被自动新分析替代。提供 `version` 时也必须与当前打开版本一致。IDE 已有的批次诊断、F12、悬停和大纲使用这种严格关联，后续的标准自动诊断接入另行实施。默认 `didOpen`／`didChange` 还没有因此变为自动诊断触发器。

本阶段复用完整语义／IR 检查和已实现的精确缓存，没有引入语义专用轻量路径，也没有新增语言规则。磁盘／包解析候选／环境继续采用已声明的后台守卫契约；工作空间与配置隔离、更多协议及整体性能目标继续实施。

协议结果形状参考 Microsoft 的 [definition](https://github.com/microsoft/language-server-protocol/blob/gh-pages/_specifications/lsp/3.17/language/definition.md)、[hover](https://github.com/microsoft/language-server-protocol/blob/gh-pages/_specifications/lsp/3.17/language/hover.md) 和 [documentSymbol](https://github.com/microsoft/language-server-protocol/blob/gh-pages/_specifications/lsp/3.17/language/documentSymbol.md)。客户端能力下的结果格式协商是后续独立事项。

实际回归入口为 `tools/check_lsp_automatic_semantics.py`：使用不存在的编译器路径验证冷启动请求、重载、未保存依赖、独立入口、来源错误／容量错误、64 个等待者、重复 ID、取消／关闭／变化／shutdown 及 CLI 兼容。它验证协议和正确性，不代表 IDE 显示总时延或更大工程性能验收。
