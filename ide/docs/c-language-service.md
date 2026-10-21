# C 语义服务与原生交互

2026-10-11 当前扩展库交付：SDK67，84 模块／106 产物重新严格构建，含渲染的完整回归和十项实际便携门通过。默认编译器修正 stdio 外部／内联声明冲突，并为真实扩展函数 C 原型提供可验证的 xlang 来源映射。CRC32、MD5 的实际混编、stdio 格式化、诊断、悬停、补全、F12 和间接头变化撤回已通过开发与包内检查；最终构建的 17 项原生界面断言和三张截图、九项编译器兼容、41 项源码材料及原有 74 项聚合／37 项标量对照通过。 详见[扩展库分析与导航](c-native-extensions.md)与[当前候选](../CURRENT-CANDIDATE.md)。下文保留此前阶段的数量和验收记录。

2026-10-11 当前 C 聚合与位域交付：SDK67，84 模块／105 产物重新严格构建，含渲染完整回归及八项实际便携门通过。默认 C/xlang 桥接新增实际 TCC 位域布局查询；开发与随包提供者各通过 23 类聚合、74 项尺寸／对齐／偏移对照，22 项元数据和用户参数优先级检查、三个实际 TCC 状态执行、九项编译器兼容检查及 41 项源码材料检查通过。原有双语法、定版布局、三层管理、编辑与构建运行保持可用，C 输入观察继续启用。 来源、适用范围和失败记录见[C 聚合与位域布局](c-record-layout.md)，启动入口及本轮精确构建见[当前候选](../CURRENT-CANDIDATE.md)。下文的既有数量和候选链接保留为相应历史阶段记录。

2026-10-07：新增真实 clangd stdio 后端，提供 C 诊断、补全、定义、悬停和文档符号。后端与真实服务器已验收；原生编辑器已接入 C 语义补全、自动诊断、F12 定义、文档大纲和鼠标/Ctrl+I 悬停。此项不改变已定版 Dock、中央底栏或 xlang 的独立语义连接。

## 工程参数

`src/services/c_commands.c` 从明确选择的项目活动配置生成标准 compilation database 的 `arguments` 数组，包含每个显式 C 源文件及 `header` 文件条目，不通过文件名猜测头文件归属。不写项目的 `compile_commands.json`，通过 clangd 初始化扩展发送副本。

原生“项目配置”在原有五类列表后增加四类 **C 分析**输入：系统头目录、强制包含头文件、宏定义及取消宏定义。各活动配置独立保存到 cAnalysis；路径按项目目录解析，保存/另存为使用相对路径，迁移和强制头重命名同步引用，未知字段保留。每类最多 128 项。系统头目录映射为 -isystem，强制头保持列表顺序映射为 -include；宏定义使用 ASCII 名称或“名称=单行 UTF-8 值”，取消定义只填名称，分别作为独立 -D/-U 参数，先定义再取消。暂不在字段中提供函数形宏，复杂宏可放在真实头文件中。所有列表均属于分析专用设置，普通 TCC 构建参数仍使用已有路径/库设置。

默认分析参数为 C11、`_CRT_SECURE_NO_WARNINGS`、Windows GNU x64 target，DLL/shared 增加 `XL6_BUILD_SHARED=1`。包含项目目录、`src`、`lib/xrt`、`lib/xruntime` 与配置的 includePaths。可显式提供分析 driver、target、systemIncludes 和 prelude；全部按独立参数传递，不经 shell，也不执行项目指定的编译器来探测头文件。

这是 Clang 分析参数，不等于 TCC 构建的全部预处理环境或 ABI。DEMO6 会在真实 C 源文件外注入 runtime 和 xlang 生成声明；自动提取、核验并同步这些声明与 TCC 系统头路径尚待实施。现在可以显式配置真实 runtime 声明目录与编译器 cgen --emit-header 生成的头文件；没有按名字推断 ABI。配置草稿应用会撤销旧批次并重新分析。缺少系统头或桥接声明时应显示实际错误，不补造声明或隐藏诊断。普通项目构建继续走原有 TaskService。

## 不可变会话

`src/services/c_language.c` 复制文档路径、全文、稳定 ID、IDE 版本和编译参数，每次启动分配独立 generation。参与文档、配置、工具链或磁盘依赖变化时，调用者必须撤销结果并替换会话；会话本身不守护磁盘；补全 job 另对显式编译文件、相关项目清单和服务器做持续守护，不宣称完整 C 预处理输入图。

初始化明确协商标准 UTF-16、普通文本补全、文档诊断版本和层次化符号；`xideCSessionSupports` 只返回服务器实际协商的能力。拒绝 UTF-8 位置协商或无法打开/同步文档的服务器。使用 clangd 的 `--use-dirty-headers=1` 内存覆盖层，禁用配置加载、后台索引、clang-tidy、补全自动加 include 和 label 装饰。固定 clangd 23.1.0 的强制包含头在预编译模式下会重新应用命令行宏，导致头里的取消/重定义失效；实际静态断言和悬停已复现。包含规范 -include/-imacros 配对参数的会话额外使用已核实的 --skip-preamble-build=1，完整解析强制头；普通会话保持原有预编译行为。此选项存在版本依赖，复杂项目的性能仍需测量。

2026-10-08：在启动进程前，为整批已打开 C/头文件建立独立临时 VFS 快照。每个文件使用已复制的正文；临时文件以 CREATE_NEW 创建，保持不允许其他写入的句柄，关闭时删除。所有编译条目追加 -ivfsoverlay（位于 -- 前），不修改调用者的参数或项目文件。编辑器正文优先于同名物理文件和用户 VFS，未参与快照的头文件保持普通文件系统回退；use-external-names=false 保留原始路径、相对包含和 F12 目标。不同 generation 的临时目录与文件完全独立；停止或失败先回收服务器，再关闭句柄和移除已创建的空目录。

随后发送全部 didOpen（头文件优先），再以空 contentChanges 和 forceRebuild 刷新诊断。**这不是预编译头的就绪屏障**：固定版本 clangd 的 AST 请求允许在重建时使用较早的 preamble，仅靠通知顺序不能保证新头文件正文已进入首次解析。VFS 使第一次和后续预编译解析从一开始就读取同一批草稿。wire 版本 1/2 独立于 IDE 版本；只接收版本 2 的诊断，对外事件仍绑定真实 IDE 版本。`READY` 表示初始化和完整同步已排队，未承诺所有解析已结束。此修复只覆盖批次中复制的文档，不扩展为完整磁盘依赖图。

tests/c_draft_test.c 使用真实 clangd，把依赖头文件的 didOpen 延迟到首个语义响应之后：旧服务返回磁盘 int，修复后悬停及补全读取草稿 double；还覆盖并行版本、新建未落盘头文件、带保护的循环包含、已有 VFS/-- 参数优先级、原始文件不变、写入拒绝与停止清理。原生测试另验证 types.h → 未保存 leaf.h 的悬停，以及 leaf 草稿修改后旧结果失效并返回新类型。首次失败的原始 JSON 保存在 build/tests/c-draft-before，原便携回归中的偶发失败仍保留；该次回复未被旧测试记录，因此不能将其具体因果当作已证明。

异步预编译行为依据固定源码 [TUScheduler.cpp](https://github.com/llvm/llvm-project/blob/ea7d852a70e8bdfaf601d6626a760f9771b2c4b4/clang-tools-extra/clangd/TUScheduler.cpp)：PreambleThread::build 和 ASTWorker::getPossiblyStalePreamble 独立运行，updatePreamble 明确采用最终一致性。

completion、definition、hover、documentSymbol 请求通过请求 ID 关联稳定文档和 generation。取消先清除本地槽位，晚到回复不会生成结果事件。停止会话回收整个 stdio 进程；新会话重新发送全部输入。接口与返回事件只在 owner 线程消费，后台回调只用于唤醒。

## 响应与容量

- 文档最多 64 份，每份原始 UTF-8 不超过 8 MiB，合计 32 MiB；仅接受命名 `.c/.h`、绝对路径、有效 UTF-8 和唯一稳定 ID/物理或词法身份。
- 编译条目最多 2,048 个，原始 database 序列化不超过 4 MiB。所有打开文档必须有明确参数；拒绝重复路径、相对 workingDirectory 与无效参数。
- 启动前核对 JSON 转义后的每个真实 didOpen 帧：最多 8 MiB；整批最多 56 MiB，为初始化/重建预留发送队列空间。转义膨胀超限在启动进程前拒绝。
- 并发请求最多 64 个；补全与诊断最多 4,096 项，符号最多 8,192 个且深度最多 64，定义位置最多 128 个。UTF-16 范围按当前不可变正文批量核对，拒绝越界及代理项中间位置。
- 只接受普通 textEdit，不接受 snippet 或额外文件编辑。补全 GUI 校验普通 textEdit 后执行完整标识符替换与一次撤销。
- 校验 JSON-RPC 版本、方法/回复形状及互斥 result/error。坏回复或诊断终止会话；未知通知和取消后回复被忽略。未实现的服务器请求回复 -32601。
- 每次 Next 最多消费 32 条无关消息或 8 ms，另加一条有界回复的校验时间；剩余队列重新唤醒 owner。通用客户端仍限制每向 64 MiB/2,048 帧，stderr 独立读取，不进入协议正文。

后端对关闭文件定义校验 file URI 与范围形状；GUI 只接受本会话初始化前已观察的显式磁盘文件，并在用户导航时读取至多 8 MiB 目标、前后核对摘要/身份和实际打开后的原始基线。未观察的隐式/系统目标暂时拒绝跳转。存在多个目标时当前使用服务器返回的第一个位置。结果不代表整个项目成功构建。

## 原生 C 补全

从“文件 → 选择 C 语言服务器…”选择 clangd，保存到偏好文件的 `toolchain.cServer`，重启恢复。选择后覆盖当前命令行指定；启动时 `--c-server PATH.exe` 优先于已存偏好。CLI 相对路径基于启动目录，偏好里的相对路径基于偏好文件目录。未配置服务器时继续使用目录候选。

```powershell
.\release\xlang_ide.exe --c-server .\build\providers\clangd-23.1.0\clangd_23.1.0\bin\clangd.exe --open-file <绝对 C 文件路径>
```

- 命名 `.c/.h` 的 C 模式支持 Ctrl+Space、`.` 和 `->`。候选打开期间输入会在 SDK 事务结束后，按实际新版本延迟 80 ms 查询。字符串/注释、只读、多选区、外部冲突或模态时不请求。
- `c_completion_view.c` 复制全部打开命名 C/header；先使用目标文件所选拥有项目的活动配置，同一路径存在多个归属且没有所选拥有者时拒绝查询。明确未归属文件用 C11/Windows GNU target/本文件目录的独立参数。没有伪造 TCC 构建一致性。
- 标准 CompletionItem 普通文本编辑映射到已有 CodeEdit 窗格。语义 filterText 与目录候选去重。UTF-16 范围必须落在当前前缀和当前 token 内；提交扩展至 token 末尾以去掉光标右侧后缀，Enter/Tab 形成一次撤销。函数按服务器实际文本插入，当前普通模式返回函数名。禁止 snippet、额外文件编辑、换行插入和错误范围。
- 发布和 PREVIEW 提交均复核目标稳定 ID、版本、路径、显示模式、光标、选区、焦点、全部 C/header 版本、外部状态、项目导出/配置/选择和服务器选择。Esc、关闭/控件复用、IME、模态及输入变化撤销候选；取消后不能再取得结果副本。
- `c_completion_job.c` 在后台启动不可变真实会话、计算文件摘要并消费协议；UI 不等待进程结束或读取磁盘摘要。每份 job 30 秒请求上限；旧 job 取消后异步退役，最多保留 4 份，容量满时等待回收。READY 后保持真实服务器存活，退出/协议失败立即使 job 失效，下一次查询重新同步。

**普通 clangd 的磁盘观察范围**：compilation database 中全部显式 C/header（打开的文件以已复制正文/版本为准）、规范 -include/-imacros 的文件操作数、相关拥有项目清单和 clangd.exe。强制头若已作为打开文档同步则以未保存快照为准。后台在启动前、发布前及发布后每 1 秒加扫描耗时复核存在性、原始字节 SHA-256、长度、时间、卷/文件身份和属性；无法读取、消失/出现或原子替换均撤销。每轮串流最多 1 GiB，单文件沿用 512 MiB 上限。这是有限观察范围，并非依赖图：未显式列出的 include、系统/内置头、目录候选、响应文件及未显式列出的生成桥接声明尚未完整观察；强制头内部继续包含的文件并不会自动入图，应将需要守护/导航的生成头加入明确 header 条目。轮询间隔内也不保证即时失效。不能用此补全结果宣称完整项目输入已核验。

`tests/c_completion_view_test.c` 使用官方真实 clangd 和独立 Windows 桌面的真实 XGE/XUI。覆盖整个 token 替换、Enter/Tab/撤销/重做、BOM/CRLF/中文/非 BMP、未保存头类型、点号/箭头、函数/宏/关键字、目录去重、12 组变异真实回复、修改/配置/标签/身份失效、显式磁盘内容/身份/删除/不可读、取消/真实服务器退出/重启和原文件字节保护。原始 C 回复与浅色/深色实际候选截图随测试保存。受控协议夹具不替代这些语义验收。

## 原生 C 诊断、定义、大纲与悬停

`ui/c_analysis_inputs.c` 为独立语义会话复制所有打开的命名 C/header 正文、稳定 ID、版本、路径、显示模式和外部状态，保留实际项目导出、选择、活动配置与服务器选择。同一个拥有项目的编译参数只生成一次；不明确的多项目归属拒绝分析。`c_completion_job.c` 的持久分析模式由后台线程拥有真实 clangd，会话查询使用本地 token 匹配真实远端 ID，标准 JSON 不加入伪造的 checkId 等字段。

- 命名 C 模式文档配置服务器后，500 ms 延迟启动/复查。显式“检查当前文档”同步未保存 C/header 并显示已有问题页；取消停止自动复查，下一次显式检查重新同步。
- 标准 publishDiagnostics 映射到 CodeEdit 错误、警告、信息和提示等级。问题页标明已收到的文档数；部分批次不宣告所有文档成功。单条范围用当前原始快照严格转换 UTF-16，Enter/双击选择实际范围。
- 问题页记录 C/xlang 结果归属及跳转回调；任一语言清理不擦掉另一语言或构建结果。后台更新不夺取语义补全的输入焦点。
- F12/菜单仅在真实能力和当前批次有效时可用。打开目标使用实际未保存正文；关闭目标复核初始化时的显式文件守卫、读取前后及打开后的基线和身份。磁盘校验发生在用户触发的导航，通常守卫仍由后台线程完成。
- 同一个中央大纲 dock 客户端在 C/xlang 两棵原生树之间切换，不改变左右 dock 或底栏范围。层级 DocumentSymbol 全量范围及 selectionRange 均验证，Enter/双击选择实际符号名称。
- 鼠标命中字符后等待 350 ms；Ctrl+I 查询光标符号。只显示协商的 plaintext，多行原文保存在可换行滚动的原生只读 TextEdit 提示窗格中。鼠标可以进入窗格阅读；移出、光标/版本/标签/焦点/模态/补全变化撤下结果。固定 SDK 默认提示框为单行高度，因此应用使用公开 popup/TextEdit API，SDK 未修改。
- 请求 30 秒上限、最多 64 槽；单条取消撤销本地结果并通知远端。每轮最多 32 事件/8 ms 加单条有界验证，守卫沿用显式文件范围。会话退出后撤下所有结果；初始化失败需显式重新检查，已经就绪的会话失效后延迟重启。

`tests/c_language_view_test.c` 使用真实官方 clangd 与独立 Windows 桌面的真实 XGE/XUI，保存标准诊断、定义和符号回复、多行悬停以及两张实际浅/深色图；覆盖未保存头文件类型、Unicode、问题与大纲定位、打开/磁盘 F12、鼠标进入提示窗格、错误/过期范围、模态、64 请求容量/取消、真实服务器退出/重启、配置/模式/身份/控件复用和原始字节保护。最新全套证据见 [C 原生语义最终核验](../build/c-semantic-final-verification.json)。

## C 分析配置与真实桥接验收

新增 tests/c_environment_test.c 验证活动配置、宏/容量校验的事务失败、未知字段和另存为路径；使用官方 clangd 验证系统头、强制头、命令行宏及取消定义、头内重定义的真实静态断言/悬停、未保存强制头覆盖与磁盘变更撤销。另调用实际 DEMO6 cgen --emit-header，以真实 runtime 声明和生成头分析 C 对 xlang 导出函数的调用，再真实混编、运行，未补造声明。原始命令、标准诊断/悬停、生成头和编译日志留在独立测试目录。原生配置列表和 C 窗口另有实际点击与失效测试，重命名和目录迁移纳入原有路径事务验收。

## 固定验收依赖

本轮使用 [官方 clangd 23.1.0 Windows 发行](https://github.com/clangd/clangd/releases/tag/23.1.0)，上游版本 `ea7d852a70e8bdfaf601d6626a760f9771b2c4b4`。ZIP SHA-256 为 `23412a240756a162e7b98a282f36aa2a23a88db5ce16a0cbc4fef7253768c810`；354 份解压文件、内置头文件及 LICENSE.TXT 独立保留在开发构建目录，没有安装到系统或加入正式发布包。

`tools/verify_clangd.ps1` 直接从经过固定摘要核验的 ZIP 逐项比较全部解压文件，同时核对服务器版本和未保存头文件选项。它只验证，不下载、不修改包。依赖来源与摘要见 [开发工具链清单](../build/c-language-clangd-toolchain.json)。将官方 ZIP 放在 `build/providers/clangd-windows-23.1.0.zip`，解压为 `build/providers/clangd-23.1.0/clangd_23.1.0`，然后执行：

```powershell
.\tools\verify_clangd.ps1
.\tools\build.ps1 -Configuration Release -Tests
.\tools\test.ps1 -Configuration Release -SkipBuild -CServerPath "$PWD\build\providers\clangd-23.1.0\clangd_23.1.0\bin\clangd.exe"
```

完整 xlang 兼容性验收另传 CompilerPath 和 CompletionCompilerPath。未提供 CServerPath 时仅运行协议合同与受控传输用例，输出明确标记真实 C 服务门禁省略；不能用此结果宣布真实语义能力完成。用户 C 服务器选择已接入偏好存储；正式发布依赖、许可证收录和离线安装仍待验收。

## 实际验证

`tests/c_language_program.c` 只用于受控传输、错误协商和通知洪泛，不模拟语义。`tests/c_language_test.c` 将真实后端与官方 clangd 一起运行，保存原始 JSON：

- Unicode/空格路径、空参数、引号/反斜杠与 caller 修改后的独立副本；stderr 隔离与唤醒。
- 真实 C 诊断、成员补全、头文件定义、悬停和大纲；UTF-16、错误响应/范围/容量/身份拒绝。
- 磁盘字段 `int`，未保存头文件字段 `double`；更换会话后字段 `const char*`。另外验证头文件包含随后同步的未保存头文件，读取其宏定义的 `double` 字段类型。
- 并发槽位容量、取消后晚到回复、新 generation 重同步、JSON 转义帧超限与有界通知消费。测试结束核对原 C 源码及头文件仍保持原正文。

后端阶段完整核验见 [后端证据](../build/c-language-final-verification.json)；原生补全阶段的完整构建、回归和指纹见 [原生 C 补全最终核验](../build/c-native-final-verification.json) 与 [实施进度](progress.md)。

## 上游依据

[编译参数设计](https://clangd.llvm.org/design/compile-commands)、[初始化 compilationDatabaseChanges 扩展](https://clangd.llvm.org/extensions)、[JSON compilation database](https://clang.llvm.org/docs/JSONCompilationDatabase.html)。未保存头文件及 forceRebuild 依据本轮固定版本的 [ClangdMain.cpp](https://github.com/llvm/llvm-project/blob/ea7d852a70e8bdfaf601d6626a760f9771b2c4b4/clang-tools-extra/clangd/tool/ClangdMain.cpp)、[ClangdServer.h](https://github.com/llvm/llvm-project/blob/ea7d852a70e8bdfaf601d6626a760f9771b2c4b4/clang-tools-extra/clangd/ClangdServer.h) 和 [ClangdLSPServer.cpp](https://github.com/llvm/llvm-project/blob/ea7d852a70e8bdfaf601d6626a760f9771b2c4b4/clang-tools-extra/clangd/ClangdLSPServer.cpp)。扩展存在版本依赖，本轮验证不外推到任意 clangd 版本。

宏选项的顺序依据 [GCC 预处理器调用说明](https://gcc.gnu.org/onlinedocs/cpp/Invocation.html)；强制头完整解析选项已核对上述固定版本 ClangdMain.cpp 和实际 --help-hidden，并由真实静态断言与悬停验收。

## 自动桥接的编译器基础

新增 [同批次 C ABI 声明查询](../../demo6/docs/IDE原生C声明桥接接口.md)：真实 XLIR 生成头与来源报告在一次查询中输出，最后重新核验，失败不发布旧声明；1 MiB 查询头上限不影响普通 Cgen。新接口只在隔离开发工具链中构建。原生 C 会话现已自动消费此结果，当前实现见 [自动桥接](c-xlang-bridge.md)。有序 TCC 配置和真实内嵌头已通过下述编译器增量输出，完整 C 预处理图继续实施。

## 自动桥接的配置与内嵌头

编译器 --c-environment / xlcCBridgeWithEnvironment 复用实际 TCC 配置访问器，返回 exe/shared 输出类型、有序选项/定义/目录、特性模块选择和准确内嵌头字节。165 份公开 .h 原字节共 13,972,580，含完整 xruntime 实现头；消费端需要后台解码及专用磁盘缓存，不能按普通 4 MiB 查询响应或 8 MiB didOpen 文档处理。传输限额见 [接口合同](../../demo6/docs/IDE原生C声明桥接接口.md)。

真实 clangd 与混编门已通过，标准声明查询和现有语义协议保持兼容。环境基础阶段未改动 IDE 应用或发布程序。随后新增的 [原生自动桥接](c-xlang-bridge.md) 已把拥有项目的入口/活动配置及同批未保存 xlang 正文加入异步输入模型，复核 sourceReport、候选、包环境和工具链，将准确生成头作为专用强制头。应按真实搜索次序在对应规范虚拟目录旁添加内嵌回退，并守护已选择的磁盘/缓存文件；不能前置所有缓存目录而改变磁盘优先级。修改、关闭或配置变化须取消旧结果，服务重启同步整个批次。

环境 complete 只覆盖配置操作与公开资源传输，cPreprocessorGraphComplete 和 abiEquivalenceGuaranteed 均为 false。完整 include 闭包、TCC 内建宏、生成模块预处理后的宏状态、复杂工程性能及正式依赖发布仍需验收。实际结果见 [本轮最终核验](../build/c-bridge-environment-final-verification.json)。

## 原生自动桥接当前增量

共同输入捕获现同时绑定所有打开 C/header 与命名 xlang 草稿、拥有项目配置、编译器和服务器。真实 ABI 声明及准确内嵌头在后台查询、核验和保留，原生诊断、悬停与补全可直接使用草稿导出类型。输入变化撤下整批旧结果，失败按有界退避恢复，旧编译器明确提示后使用显式配置。补全使用独立真实解析和一份私有生成头索引，用户正文/坐标不变；缓存生命周期与有限观察范围见 [服务合同](c-xlang-bridge.md)。完整预处理图、跨语言定义映射、宏/ABI 等价性和正式发布仍未完成。


## 实验性编译输入观察（2026-10-08）

开发版本支持协商 `capabilities.experimental.xlangInputTrace=1`。普通 clangd 继续采用前述显式文件守卫；只有声明同版扩展的隔离提供者才启用实际观察。早期便携候选保留官方服务器；当前候选已默认交付固定输入观察提供者及其适配材料，见 [默认 C 输入观察交付](c-input-trace-delivery.md)。普通 clangd 继续可选。

提供者基于同一上游提交 `ea7d852a70e8bdfaf601d6626a760f9771b2c4b4`，在真实 clangd 的物理 VFS 和两处 compilation database 响应文件展开入口记录文件操作。观察包含真实读取字节的 SHA-256、Windows 卷和文件索引、长度、100ns 修改时间、属性、失败的存在性查询、物理路径解析以及实际目录枚举。未保存正文仍由既有不可变 VFS 快照覆盖；观察的是覆盖层所访问的物理输入，不能把磁盘正文当作未保存正文。IDE、原项目和官方提供者不被该构建覆盖。

读取采用同一 Win32 文件句柄，返回给 Clang 的 MemoryBuffer 来自该句柄实际读出的字节；打开时使用共享删除以允许原子保存。目录状态比较存在性、身份及属性，创建无关子文件不单独改变目录状态观察；若编译器实际枚举目录，则另外保存准确候选列表。状态矛盾、读取前后变化、不可读、重解析叶节点和容量不足使该会话失败。

服务器在标准语义消息之前，通过 `xlang/inputTrace` 按连续 sequence/previous 发送增量。`complete` 仅表示这次通知已发完当前待发送记录，不表示所有项目、后台未来操作、TCC 预处理或 ABI 均已完整分析。单条记录限 256 KiB，通知记录区限 512 KiB / 128 条；会话最多 65,536 路径、1 GiB 实际读取字节和 128 MiB 元数据。IDE 校验协商、序号、稳定状态、空错误和容量，并使用服务器已经观察的基线核对物理文件；不会在收到结果后重新采集基线并接受旧结果。

后台在语义发布之前核对完整已知观察集，发布后每 1 秒加扫描耗时再次核对。实际读过的间接头文件可提供关闭文件 F12 的基线。文件变化、原子替换、先前缺失路径出现和已枚举候选变化，会撤下整个 job 的诊断、悬停、大纲、定义及补全；原生界面按现有规则延迟重建。UI 不进行摘要扫描。轮询期间的短暂变化仍可能不可见，最大长路径、复杂 VFS/配置、文件身份重用和大工程性能尚未完成矩阵验收。

`tests/c_input_trace_test.c` 以实际 clangd 验证未列入项目的间接头、宏展开的 `__has_include` 缺失/出现、更早搜索目录的遮蔽、`include_next`、嵌套响应文件、内置头、悬停与补全失效/重建，并保留原始标准响应和缺失探测观察。原生 `c_input_trace_view_test` 验证真实悬停撤下、诊断/大纲清理、当前类型恢复及隐式头 F12；截图输出在项目目录之外。

固定官方资源目录的 352 份头及 LICENSE.TXT 逐字节复制到独立 runtime，与新服务器组合。源码构建生成的头使用 LF，官方头使用 CRLF，二者不声称原始字节相同；原生成文件和旧原型仍保留。开发构建需 Git、GCC/G++、CMake/Ninja，IDE 运行本身不新增这些要求。上游构建使用的 GNU 静态运行库、全部许可和正式发布资格仍需专项审计。

可用偏好中的 C 服务器选择或命令行 `--c-server` 指向实际提供者。复现构建先在新的自有 stage/source 中检出固定提交，再使用 `tools/build_clangd_input_trace.py --stage <stage>`。脚本要求原树干净、变更与回执一致，默认仅构建独立 exe；资源 runtime 的装配不覆盖已有路径。长构建路径可使用明确指向该 stage/compile 的自有短连接点及 `--build-alias`，脚本核对实际解析目标。

完整回归另传 `-CInputTraceServerPath <traced-clangd.exe>`；该参数运行真实后端和原生观察门，未传时明确报告省略。`-CServerPath` 可以继续使用固定官方 clangd，独立验证普通服务器兼容性。真实语义门不使用模拟语义回复替代。

完整预处理/项目观察图、TCC 内建宏与 ABI 一致性、公共类型跨语言映射和正式首版仍未完成。进度与本轮冻结证据见 [实施进度](progress.md)。

上游依据：固定提交的 [ThreadsafeFS.cpp](https://github.com/llvm/llvm-project/blob/ea7d852a70e8bdfaf601d6626a760f9771b2c4b4/clang-tools-extra/clangd/support/ThreadsafeFS.cpp) 和 [GlobalCompilationDatabase.cpp](https://github.com/llvm/llvm-project/blob/ea7d852a70e8bdfaf601d6626a760f9771b2c4b4/clang-tools-extra/clangd/GlobalCompilationDatabase.cpp)，实际共享删除行为依据 [Windows ReplaceFile](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-replacefilea)。普通依赖文件对失败 HasInclude 探测的处理见 [Clang DependencyFile 源码](https://clang.llvm.org/doxygen/DependencyFile_8cpp_source.html)；本扩展记录真实 VFS 探测，不用普通 depfile 冒充失败候选图。


## C 导航的项目上下文（2026-10-10）

F12 打开未列入工程的 C/header 后，文档保留经过当前输入和目标守卫核验的来源项目与源文件。后续诊断、悬停、大纲与补全复用当前项目配置，配置变化撤下旧结果；隐式头不会因此变成项目的编译输入。稳定文件 ID 跟随源码改名，会话记录保留关联和未知扩展字段。来源被移除或不再可用时拒绝沿用该上下文，关闭文档清除关联。规则和真实原生回归见 [导航上下文](c-navigation-context.md)。
