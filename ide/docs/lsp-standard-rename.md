# 标准重命名协议

后续 M7b6h 已接入[prepareRename](lsp-prepare-rename.md)及客户端协商；下文保留 M7b6g 当时的范围与验收说明。

M7b6g，2026-11-14。原生后端提供 `textDocument/rename` 和布尔能力 `renameProvider: true`。CLI 后端不声明这项能力。`textDocument/prepareRename` 尚未实现，也没有声明 `prepareProvider`；后续将复用原生目标捕获建立轻量检查入口，不通过虚构新名称执行一次完整重命名来代替准备操作。

标准请求使用 `textDocument.uri`、UTF-16 `position` 和 `newName`，支持字符串、零及负整数请求 ID。可选 `workDoneToken` 接受整数或字符串；当前不发送进度通知。未知扩展字段不影响标准请求。原来的 `xlang/renameCandidate` 继续使用严格的私有意图、正整数 ID、输入回执和完整证明，不改变 IDE 的预览、确认与安全写入路径。

服务端保留当前 catalog、全部在线文档的不可变正文、身份和有符号版本。标准客户端无需构造私有输入回执或候选来源证明。工程配置仍由 `initializationOptions.analysisConfiguration` 或标准 `workspace/didChangeConfiguration` 的 `settings.xlang.analysisConfiguration` 提供，格式沿用[工程配置接入](lsp-development.md)。配置的所有项目都参与分析，包括未打开文件和没有目标引用的项目；未被工程拥有的在线 `.xl` 文件作为独立根加入。

没有 `rootUri`、非空 `rootPath` 或 workspace folders 的独立文件会话，可不提供 catalog，使用文件自身目录及 `XLANG_PACKAGE_PATH` 建立分析上下文。它不把目录或导入库自动认作可编辑的项目成员。对于提供了工作区根而没有完整配置的客户端，请求返回 `RequestFailed`（-32803），不会将在线文档清单当作完整工作区。工作区文件夹变化会取消当前请求并使范围失效，直到收到更新代次的完整配置。文件夹通知与正文/配置消息共用输入队列分类；分析租约期间触发失效并保留，释放后按原顺序重放。自动读取 `.xlp/.xlw`、闭合的工作区独立文件清单及文件夹成员发现仍需接入；普通文件夹客户端的跨文件重命名尚不具备开箱即用的完整范围。

两个协议入口借用同一套原生引用协调器、上下文、来源防护、声明族、精确标识符范围、冲突检查与假想正文静态复检。标准路径直接从已验证计划导出编辑，不导出再丢弃整份私有 inputGuards DOM。提交阶段只保留不可变正文引用，不复制或重哈希源码正文；原生检查和来源守卫所需的工作保持。全部范围准备、原生检查、编辑 JSON 构建和响应帧编码在已有后台任务池运行；没有新增常驻解析器或同任务池嵌套等待。

客户端声明 `workspace.workspaceEdit.documentChanges: true` 时返回版本化 `TextDocumentEdit[]`；在线文档使用实际 int32 版本，关闭文件使用 null。否则返回 URI 到 `TextEdit[]` 的 `changes` 映射，不附私有证明、资源操作或 change annotations。后者不能携带版本，客户端负责自身的过期检查和应用事务。服务端仅返回建议，不写磁盘、不调用 `workspace/applyEdit`。

两种响应都在完整帧编码之后再次检查所有原始输入守卫，包括初始光标提供者和零目标范围；磁盘被同字节替换也拒绝结果。owner 继续检查在线身份、版本、inputSerial、配置 epoch 和入站输入队列。编辑、关闭重开、配置变化、分析租约、取消和关闭服务会丢弃旧结果。并发上限仍为四个请求，输入为最多 64 个在线文档及 2 MiB 复制标签，范围最多 64 个上下文，编辑序列化最多 7 MiB，完整帧最多 8 MiB；这些数值不是总 RSS 上限。

本阶段的标准协议矩阵覆盖完整工程清单中的跨项目关闭文件、两种响应格式、int32 边界版本、Unicode/BOM/CRLF、类型精确的 ID 和取消、队列容量、冲突、无关范围错误、配置失效及独立文件。补测未实例化泛型体中的已绑定全局引用会一起修改。另一个依赖成员样例在原生类型检查中已被拒绝，重命名亦不返回编辑；这项反例不能证明有效的未绑定/依赖引用已经覆盖。这不代表完整泛型、未绑定或生成事实和所有外部来源已获资格。

配对验收重建原生服务及 IDE，测试程序仍通过根 `release/win_x64` 中匹配 SDK 的临时别名运行，保护现用运行库与用户状态。前一阶段记录的三个旧弹窗失败没有由此修复，也不计为本阶段通过项。标准 prepare、工作区范围自动加载、通用编辑应用、当前引擎资产对齐、全目标冷启动/增量延迟、内存/耐久及 Linux/sanitizer 验收继续在长期任务范围内。现用发行程序、开发 SDK、active delivery 和线上更新不替换；实际构建、源码和验证材料由 `dev/release-materials/provenance/lsp-standard-rename-20261114.json` 索引。

协议依据：[LSP 3.17 Rename](https://raw.githubusercontent.com/microsoft/language-server-protocol/gh-pages/_specifications/lsp/3.17/language/rename.md)。
