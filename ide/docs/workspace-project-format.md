# 工作空间与项目格式

本文件定义新 IDE 的三层模型及持久化契约，状态为 2026-10-05 定版。下列新字段为实现约定，不表示当前 IDE 或编译器已经实现。

## 三层模型

| 对象 | 持有内容 | 不持有内容 |
| --- | --- | --- |
| Workspace | 项目引用、顺序、启动项目、工作空间级文件 | XUI 节点、控件、编译器上下文 |
| Project | 清单路径、入口、输出类型、源码和库资源清单、构建运行配置 | Dock 编号、光标、展开状态 |
| FileEntry | 稳定 ID、路径、文件种类、所属项目引用 | 编辑控件和撤销历史 |
| Document | 规范化路径、内存文本、版本、编码、脏状态、撤销历史 | 项目构建规则 |
| EditorView | 文档引用、视图位置、选择和折叠 | 独立文本副本 |

一个工作空间可引用多个项目。一个物理文件可被多个项目引用，但打开后共享一份 Document。FileEntry 与 Document 分离，以同时表达工程归属和唯一文本。

稳定 ID 不使用数组下标或树节点编号。项目引用和文件条目生成不随重排、重命名而变化的 ID；文档以规范化路径去重，未命名文档使用独立临时 ID。集合可增长，不能固定为旧 IDE 的项目、文件或库数量上限。

## 工作空间清单

新 `.xlw` 使用 `xlang-workspace-1`，路径相对于工作空间清单所在目录。

```json
{
  "format": "xlang-workspace-1",
  "name": "Demo",
  "projects": [
    {"id": "hello-app", "path": "HelloApp/HelloApp.xlp"},
    {"id": "core-tests", "path": "CoreTests/CoreTests.xlp"}
  ],
  "startupProject": "hello-app",
  "files": [
    {"id": "workspace-readme", "path": "README.md"}
  ]
}
```

`projects[].id` 在工作空间内唯一，`startupProject` 引用该 ID，不能引用易变化的显示名称。无启动项目时构建和运行入口要求明确选择；删除启动项目后清除引用，不默默运行另一个项目。

项目文件缺失时保留引用，并在树中显示不可用状态，不能加载后保存就丢失该项目。添加已有项目按规范化清单路径去重。空工作空间有效。启动项目、项目顺序与工程引用可共享，当前选中节点留在本机会话。

## 项目清单兼容性

`.xlp` 保持 DEMO6 的 `xlang-project-1` 根格式，编译器字段和 IDE 扩展分区明确区分。

| 字段 | 当前工具链含义 |
| --- | --- |
| `format` | `xlang-project-1` |
| `kind` | `exe` 或 `dll`，当前解析器也接受 `shared` |
| `entry` | `.xl` 入口文件 |
| `cSources` | C 源文件路径数组 |
| `resources` | `.json` 或 `.xlon` 资源配置数组 |
| `ide` | 本设计新增的 IDE 元数据；当前解析器会跳过未知根字段 |

不要将旧项目的 `Project`、`Build` 或 `BootCode` 直接作为当前编译器输入。资源分组可展示任意资源文件，但 `resources` 编译字段只接收工具链支持的资源配置，不直接填图像、图标或旧 `.xson` 文件。

```json
{
  "format": "xlang-project-1",
  "kind": "exe",
  "entry": "src/main.xl",
  "cSources": ["src/native.c"],
  "resources": ["app.res.json"],
  "ide": {
    "format": "xlang-ide-project-1",
    "name": "HelloApp",
    "description": "xlang 示例应用",
    "files": [
      {"id": "main-source", "path": "src/main.xl", "kind": "xlang"},
      {"id": "utils-source", "path": "src/utils.xl", "kind": "xlang"},
      {"id": "native-source", "path": "src/native.c", "kind": "c"},
      {"id": "resource-config", "path": "app.res.json", "kind": "resource-config"}
    ],
    "libraries": [
      {"kind": "package", "name": "sqlite"}
    ],
    "configurations": [
      {
        "id": "development",
        "name": "开发配置",
        "output": "build/development/HelloApp.exe",
        "includePaths": [],
        "libraryPaths": [],
        "linkLibraries": [],
        "run": {"workingDirectory": ".", "arguments": [], "environment": {}}
      },
      {
        "id": "release",
        "name": "发布配置",
        "output": "build/release/HelloApp.exe",
        "includePaths": [],
        "libraryPaths": [],
        "linkLibraries": [],
        "run": {"workingDirectory": ".", "arguments": [], "environment": {}}
      }
    ]
  }
}
```

所有项目相对路径以 `.xlp` 所在目录为基准。这里的开发配置和发布配置是不同的配置记录，不隐含当前编译器尚未支持的优化级别或调试信息开关。

没有 `ide` 扩展的现有项目必须可打开；IDE 从入口、C 源和资源配置建立基础视图，按需补充元数据。额外 `.xl` 文件清单用于管理和显示，不能把所有文件都作为独立入口重复编译；模块关系遵循源码 import 语义。

`libraries` 中的 package 引用用于库视图和文档发现，源码仍通过 import 表达语言依赖，不因出现在树中就自动导入或链接。真实链接选项通过当前 CLI 的 `--inc-path`、`--lib-path`、`--link` 等已支持入口映射。

入口、C 源和资源变更更新根字段；IDE 文件列表不能产生与根字段矛盾的第二份构建真相。属性编辑和清单文本编辑必须协调。保存保留未知根字段和扩展字段，已知字段做类型与引用验证。

构建配置首版不定义项目间链接依赖图。工作空间批量构建按项目顺序执行，import/package 依赖交给编译器；以后新增项目产物依赖时另行定义顺序、循环检测和失败传播。

## 文件种类与操作

文件种类包括 xlang 源码、C 源码、头文件、资源配置、普通资源和文本。分组属于视图表达，不改变物理路径。从项目移除文件默认仅移除引用；删除磁盘文件是独立动作。

路径保存采用 UTF-8 和可迁移的相对路径，允许明确的外部绝对引用。运行时按平台处理大小写、分隔符、Unicode 和真实路径，去重不能只做小写字符串比较。另存为项目或工作空间时重新计算相对路径，并保持实际引用对象不变。

新建、重命名、另存为后同步 FileEntry、Document、语言服务 URI 和会话引用。原子保存使用同目录临时文件，失败保留原文件和内存脏状态。

## 本机会话

工作空间旁的 `.xlang-ide/workspaces/<scope>/session.json` 和 `layout.json` 保存本机状态，建议从版本控制排除。scope 按清单种类与文件名隔离，避免同目录多个工作空间互相覆盖；独立项目同样有独立作用域。实现细节见 [会话与布局恢复](session-and-layout.md)。未保存工作空间的会话进入用户配置目录。不同工作空间互不覆盖，用户全局偏好有独立应用命名空间。

```json
{
  "format": "xlang-ide-session-1",
  "activeProject": "hello-app",
  "coordinateSystem": "unicode-codepoint-1",
  "configurations": [
    {"id": "hello-app", "manifest": "HelloApp/HelloApp.xlp", "configuration": "development"}
  ],
  "openDocuments": [
    {"id": "doc-example", "path": "HelloApp/src/main.xl", "language": "xlang", "line": 4, "column": 1}
  ],
  "activeDocument": "HelloApp/src/main.xl",
  "activeDocumentId": "doc-example",
  "selectedCommand": "language:func"
}
```

会话文档路径相对于工作空间目录，内部位置信息明确采用一基行号和一基 Unicode 码点列，读取写入由坐标适配器转换。选区使用 anchorLine/anchorColumn；滚动和匹配语法范围的折叠状态已有可选字段。可选 reading/version=1 保存稳定树身份、展开/选择/顶部行、帮助章节和正文锚点、属性滚动；SDK 节点编号不进入会话协议。未命名文件和未保存恢复文本需要单独恢复存储，不能将正文塞进普通会话 JSON。

Dock 文件保存版本号、逻辑面板 ID、分割轴、比例、活动页与隐藏状态。轴和序列化格式由 Dock 适配层统一转换，不能直接假定 XUI 的内部编号及格式是稳定公共协议。默认活动页固定为 `command_help`，默认空间关系以 [架构中的 Dock 树](architecture.md) 为准。

## 命令目录契约

目录使用 `xlang-command-catalog-1`。语言结构、标准库和扩展库条目可分文件维护，加载后统一索引。一个条目包含以下信息。

| 字段 | 用途 |
| --- | --- |
| `id`、`name`、`category`、`module` | 稳定定位、列表、分类与所属模块 |
| `signature`、`parameters`、`returns` | 文档和静态提示，未知内容不猜测 |
| `documentation` | 有序正文段落、列表和章节，可关联 Markdown 文件 |
| `examples`、`insertText` | 展示范例与显式插入模板，二者可不同 |
| `source`、`since`、`availability` | 权威来源、适用版本与能力条件 |

语言结构没有参数或返回值时省略相应字段。范例有语言标记和来源，以真实编译器验证后进入正式目录。目录缺失时给出明确空状态，不能保留界面样例假装完整标准库文档。

## 旧格式迁移

`D:\GIT\xlang_ide` 与 `ide_old` 的 `.xlw/.xlp` 是旧格式，需要专门识别和导入，不能与 DEMO6 格式混用。

旧工作空间的 Name、Projects 和 Files 可映射为新对象；Open、Status、展开状态转入本机会话。旧项目的源码、库与构建资源先分类，再生成当前入口、C 源和资源配置。旧 BootCode、版本信息和旧运行命令等不能保证等价转换的字段进入迁移报告，不静默丢弃。

迁移生成新清单并保留原文件。导入失败不修改原资产。迁移报告记录缺失文件、无法映射的资源、旧 DLL 依赖和需手工确认的语义差异。

当前 `services/legacy_import` 已实现独立准备、预览、入口/类型/启动选择和新目录发布后端。只有单一 `.xl` 候选时自动选择入口；旧 C 源映射到 `cSources`，普通资源不自动进入编译资源数组。完整旧 JSON、未知字段和缺失引用保留，旧状态另存会话提示，项目菜单的原生导入窗口已支持生成后打开和显式状态应用；已有会话受到保护，缺失引用与 C/xlang 恢复已验收。详细规则与已执行验收见 [旧格式导入](legacy-import.md)。

## 格式验收

必须覆盖最小编译器项目、完整 IDE 元数据、多项目、共享物理文件、中文空格路径、外部引用、未知字段往返、损坏 JSON、缺失文件、另存为和旧格式导入。项目超过 32 个、文件超过 256 个时仍应正常管理，避免重引入旧固定容量。

本次编译器字段依据为 `D:\GIT\x-lang\src\driver\xl6_xlc_shell.c` 的根字段解析和验证函数。清单设计改变时同步更新此文档与导入测试。

## 当前配置与事务编辑

`ide.activeConfiguration` 是该项目的当前构建/运行配置 ID，必须指向其 `configurations` 中的条目。未提供时兼容使用第一配置；没有配置的兼容项目保留空值。该选择独立于工作空间启动项目与树的所选项目。

配置页导出相对路径草稿，保留配置及 `run` 下未知字段；复制时使用独立容器，应用时完整验证 ID 唯一性、路径列表、环境变量及活动配置，再整体替换内存模型。点击应用后项目变脏，保存继续执行磁盘基线比较与原子写入。

## 已提交清单与外部变化协调

2026-10-06 实现：已绑定的 `.xlw/.xlp` 文本保存前验证 JSON 及当前格式结构，成功后立即从实际提交正文事务刷新模型。模型/配置保存继续同步干净的已打开清单文档；未保存 JSON 草稿、脏模型和未应用属性输入互相保护，不静默覆盖。模型导出保留未知字段且不修改原始 JSON 基线。

外部工作空间按稳定引用 ID 与物理路径匹配项目；匹配的当前配置/未保存模型保留，新引用由后台读取，删除或改绑脏项目需要“磁盘变化”窗内明确确认。清单刷新保留独立源文档、树阅读内容与当前作用域，不将项目选择混同启动项目。坏 JSON/格式保留当前有效模型，保存继续比较磁盘基线。具体协议与原生验收见 [外部文件变化与清单协调](file-coordination.md)。

## 工程引用事务实现

2026-10-06：xideProjectReferencesJson 导出独立 files/libraries 草稿；完整验证成功后一次替换模型，并按文件种类同步根 cSources/resources。入口引用必须存在且为 xlang，C/xlang/头文件和资源配置扩展名受到核对，重复物理路径及库种类/名称被拒绝；没有改变则不新增脏标记。普通资源不进入编译字段，额外 xlang 文件不生成入口。

ide.libraries 可带非空字符串 id，新建管理记录生成稳定 ID，旧记录可以没有 ID。事务应用核对显式 ID 唯一性与 NUL 字符，未知种类和字段保留。编辑文件或库不更换原稳定 ID；重排与更名后的树阅读键使用库 ID，旧记录继续按完整定义兼容。另存为和已打开清单正文的冲突保护继续有效。原生入口、字段未加入列表的保护及真实 PE 资源验收见 [工程文件、资源与库引用](project-references.md)。

## 模板生成项目

新建向导生成 DEMO6 xlang-project-1 清单，使用上述项目/文件身份、FileEntry、cSources 和 development 配置协议，输出位于 build/development。DLL 模板输出 kind=dll，只有 C ABI 导出函数，不生成应用 main。生成清单与预览字节一致，创建先提交新目录，成功后以已预分配内存添加工作空间引用；工作空间仍需单独保存。名称、目录竞争、深拷贝草稿基线与失败回滚见 [项目模板与新建向导](project-templates.md)。

源文件另存为现以预分配路径事务同步当前加载的 FileEntry、entry、cSources、resources 和工作空间 files，稳定身份和未知字段保留，受影响清单标记已修改。文档原子写入失败时不提交模型，完整保存结果回调释放计划；会话和监视随后使用新路径。物理文件重命名现复用该路径事务，保留未保存正文与撤销；当前文档与工程树所选文件入口及保护见 [重命名协议](file-rename.md)。目录与项目清单迁移现通过专用原生事务实现；语言服务 URI 和源码 import/include 重构继续单独实施。见 [另存为路径协议](file-save-as.md)。

项目清单与目录迁移后端已验证绝对模型路径的完整目录边界更新、嵌套/缺失项目保留及跨父目录保存重算。参数、环境和未知字段不推断为路径，库元数据保留；可用项目标记已修改，原 JSON 仍用于磁盘基线检查。现已接入原生入口、全部打开文档、任务、监视与会话绑定，已命名清单正文未保存时拒绝操作。成功仍需保存清单，详见 [迁移协议](project-migration.md)。

资源内容编辑不改变已有 resources 或 FileEntry 引用。版本、ICO 和清单表单保留未知 JSON/兼容 XLON 数据，素材路径相对于配置文件本身；应用只产生文档编辑，保存继续保留 BOM、换行和磁盘冲突保护。协议见 [Windows 资源属性](windows-resources.md)。

库候选检查和文档阅读不新增 ide.libraries 字段，不把探测路径或当前版本写为持久依赖。库名/种类和未知元数据仍按引用事务保存，真实 import 和链接配置独立。文档往返保存的是当前进程内的原引用草稿；命令目录或外部工程变化不替换其提交基线。使用与检查边界见 [工程引用与包文档](project-references.md)。

## 显式测试程序扩展（2026-10-08）

`ide.configurations[].test` 已实现 `program`、`workingDirectory`、有序 `arguments` 和 `environment`，其未知字段保留。程序路径与工作目录相对于项目清单；缺失或空程序关闭测试入口。另存为、引用改名和目录迁移协调这些已声明路径。测试仅在主动操作时运行，语义及完整示例见 [项目测试](project-tests.md)。
