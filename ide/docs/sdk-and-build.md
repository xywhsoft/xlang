# 构建与 SDK

当前发行只提供图形 IDE `xlang_ide.exe`，自动化诊断也使用该程序；旧阶段记录中的控制台副本不再构建或打包。源码和来源材料位于仓库的 `dev/release-materials/`。

2026-10-31 更新：GUI 默认以 `XIDE_ENABLE_C_LSP=0` 编译，不包含 C LSP 后端或 clangd，C 编辑／编译运行与 xlang-ls 保留。显式开发开关、当前构建命令和验证范围见 [C LSP 构建选项](c-lsp-build-option.md)；以下 C 服务相关阶段记录按原日期保留。

## 固定依赖

本项目使用 `third_party/xge_sdk/` 中的成套 SDK。2026-10-31 更新到 XGE 3.0.0 提交 `6176f6f8d6ce36c9253f0fb5b8459a65b7a985d1`，在 Label 多约束测量缓存、滚动位移与 CodeEdit 光标布局失效修复之上，修复弹出菜单内部重复焦点边框。隔离构建保留 IDE 的五份明确列举的兼容补丁；公开头、生成配置、DLL、导入库及来源记录整体匹配，XRT 使用 XGE 随附的匹配快照。当前说明见[编辑器 minimap 与菜单边框](editor-minimap.md)和[帮助交互性能](xge-help-performance.md)，历史版本和代理迁移见[XGE 3 更新](xge3-upgrade.md)。最初的 AutoTestIDE SDK 及 SDK67 等阶段属于历史记录；下文相应阶段的数量和限制不代表当前版本。

构建前 `tools/verify_xge_sdk.ps1` 检查所有受保护文件 SHA-256、共享运行时策略、XRT 声明头来源、预处理后的模块配置和 DLL 导出。更新 SDK 必须整体替换头文件、导入库、DLL 及来源记录，再重建全部程序。测试在独立副本中修改头文件，确认混用会被拒绝。

日常开发使用 `build_current.bat`，测试在对应源码快照内执行，便携装配使用 `package_current.bat`，保持工作区 `release` 只存已验证运行载荷。以下直接构建命令保留作旧入口说明，目录维护见[工作区与发布目录](workspace-hygiene.md)。

应用使用 `XGE_DLL`、`XUI_DLL`、`XGE_XRT_PROFILE_IDE` 及匹配的生成配置头，只链接 SDK 导入库及 Windows 系统库。`xge.dll` 是 XRT 唯一实现所有者。应用不可定义 `XRT_IMPLEMENTATION`，不能链接另一份 XRT 实现。上游 XGE 3 的 `ide` 配置保留 HTTP/TLS，并在同一 DLL 内构建自带 xllm／xllm-session／xwork 扩展；IDE 业务仍是传统 IDE，没有接入 AI 或自动化脚本面板，也不把这些扩展另行链接一遍。发布程序不依赖开发用的 Python、ADB 或 scrcpy。

## 本地构建

Windows PowerShell、GCC、windres 和 objdump 需要在 PATH 中。当前机器通过 `E:\software\w64devkit\bin` 提供编译工具。

```powershell
.\build.bat                         # Debug，GUI 与诊断可执行文件
.\build.bat -Configuration Release  # 优化构建
.\test.bat -Configuration Release   # 已注册 69 类测试；按编译器/服务参数执行对应门，含双语法/布局图与 SDK 负面测试
.\test.bat -Configuration Release -CompilerPath D:\GIT\x-lang\release\xl.exe  # 额外真实任务、混编、五张任务图及一张真实工具链图
.\release\xlang_ide.exe
```

独立 `.c` 文件编译为 `build/<Configuration>/` 下的对象，再链接 GUI 和控制台版本。使用 `-Wall -Wextra -Werror`。所有 SDK 和应用头文件取自本工作区，不查找参考项目目录。

独立服务产物为 `release/xlang-ls.exe`，和 GUI 共用相邻固定 `xge.dll`。服务入口单独编译并只链接协议、保存后检查、工具链与平台模块；不初始化图形窗口，编译器继续是子进程。

产物位于 `release/`，测试图片位于 `build/tests/render/`。字体暂使用 Windows 字体：微软雅黑用于 UI、Consolas 用于代码，并给代码字体注册中文回退。界面和代码字体可以在各自的原生设置中独立选择并保存，默认均为 16 DIP；控件行高和模态内容随界面字号更新，代码区域依代码字号度量。全工作台 DPI 渲染已接入，实体混合 DPI 验收继续实施。

诊断程序支持：

```powershell
.\release\xlang_ide_console.exe --help
.\release\xlang_ide_console.exe --hidden --frames 3 --demo c --theme graphite-dark --screenshot .\build\c-dark.png
.\release\xlang_ide.exe --workspace .\MyWorkspace.xlw --open-file .\src\main.xl
.\release\xlang_ide.exe --recovery-manager  # 打开只读恢复管理入口，修复仍需窗口内确认
```

`--hidden` 和 `--screenshot` 必须同时给出有限 `--frames`。程序会检查实际帧数、帧错误和 XUI 错误回调，不以 XGE 主循环单独返回成功作为绘制成功。隐藏开关只隐藏本程序自己的测试窗口。

## 复用来源与适配

- `src/theme/theme.c` 与 `theme.h` 来源于 AutoTestIDE 的独立主题模块，保留四套色板和控件样式。移除了参考应用的全局偏好路径与读写函数。
- `src/platform/xide_input.c` 使用固定 SDK 的 `xuiProxyXgePumpInput` 有序桥，参考 AutoTestIDE 的正式主循环；XUI HotKey 注册 IDE 动作，Escape 交给控件。应用层修正补全立即取消与 Dock 捕获失焦，SDK 保持原样。详见 [原生输入与 IME](input-and-ime.md)。
- `src/lang/xlang_literal_span.h` 是 DEMO6 字面量跨度扫描器的快照，原路径为 `src/lexer/xl6_literal_span.h`，SHA-256 为 `3d1b3f3a71adaeb8d76910ece765f433e899190b2f153990feb9c7c6529e0db7`。复用它避免对嵌套插值字符串另造一套规则；编译器变更时需核对并显式更新。
- `src/lang/languages.c` 的关键字依据 `src/lexer/xl6_lexer.l`，C 使用 SDK 内建语言。此模块提供词法展示与折叠，不提供完整语义分析或编译诊断。

当前 SDK 的 CodeEdit 绘制路径在没有有效缓存时直接使用 C 词法。仅调用 `xuiCodeEditSetLanguage` 不足以让 xlang 正确显示。因此 `xideRefreshEditorSyntax` 显式执行当前语言，按文档版本写入覆盖全文字节范围的 token 缓存。DocumentService 已在文档修改后刷新该适配层；语言切换也必须经过 `xideConfigureEditorLanguage`。不能只测试独立 lexer，然后把语言注册成功当作实际渲染成功。

XUI 文档会将 CRLF 规范化为内部换行形式，token 偏移指向内部 UTF-8 文本。保存服务已记录 UTF-8 BOM 和换行偏好，不能直接用原文件字节位置解释 token。结构未变的折叠状态会保留；边界平移、增量词法性能及多视图共享仍待继续处理。
## 当前文档与 UI 适配

`src/platform/path.c` 使用 Windows Unicode 路径、真实路径和已有文件身份做去重；不存在的路径保留大小写差异。`src/model/workspace.c` 使用动态集合，读写新工作空间和编译器兼容项目格式，保留未知字段；加载失败不替换当前模型，保存失败不清理脏状态。保存已有清单前对比加载基线与磁盘 JSON，外部变化需要重载。

`src/platform/text_file.c` 负责有大小限制的 UTF-8 文件读取、BOM、换行规范化和输出编码；`src/editor/documents.c` 维护唯一文件、文本基线、撤销与脏状态、语言覆盖和保存冲突。`src/ui/editor_view.c` 将它接入可编辑原生 Dock 标签、文件对话框、关闭确认和状态栏。固定 SDK 的 CodeEdit 公开接口持有内部文本与撤销文档，没有外部文档绑定入口；当前每个唯一文件复用一个编辑器，跨项目打开不复制文本。

SDK 控件的字符串所有权不能一概假定为拷贝。状态栏 `SetItemText` 借用文本，动态语言/编码/行列状态使用应用持有的缓冲区，不能传入临时栈字符串。工程树节点文本由视图绑定持有。文档变更回调在销毁 XUI 上下文前解除。

Dock 激活与关闭通知可能延迟交付，适配层忽略已被新激活覆盖的旧通知，取消关闭在事件派发后恢复标签。命中检测和树行矩形采用局部坐标，输入事件是世界坐标；双击文件使用 SDK 的正式双击事件并转换坐标。编辑标签 Dock 完成布局后再设置焦点。

空工作空间仅显示属性引导；选择可用项目后显示表单，避免绘制无意义的禁用编辑控件。原生 smoke 对任何 XUI 错误回调判定失败，防止缓存跳过被当作绘制成功。
## 构建运行适配

`src/platform/process.c` 深拷贝 XRT 进程参数，后台启动和读取合并标准流，独立写线程处理 stdin。UI 输入队列限制 64 KiB，输出队列最多 1 MiB；原始输出另写任务日志。直接使用 SDK 的参数转义、继承环境、工作目录和独立进程组，停止使用 `xrtProcessKillTree`，不拼接 shell 命令。当前平台管道轮询、工作线程和日志句柄使用 Windows API。

`src/services/tasks.c` 拥有独立请求、输入基线、唯一阶段目录、日志和诊断。只允许一个执行任务，重复未完成请求合并；排队任务不借用工程数组指针。文件身份扫描与别名变更检查在后台，UI 只交付路径通知。退出先取消并等待任务，再释放文档与 XUI。UI 线程通过 `src/ui/task_view.c` 更新底栏控件。

`tools/test.ps1` 含中文路径字符串，保存为带 BOM 的 UTF-8，以兼容 Windows PowerShell 5；不能在普通文档编辑操作中无意移除其 BOM。十四类测试包含命令目录、原生补全、进程、任务服务、真实任务视图、配置和便携设置，指定 `CompilerPath` 后还执行正向构建运行、真实失败构建，以及五张任务面板原生图。失败构建的 smoke 预期进程退出码为 1，仍要求实际绘制、结构化失败状态和没有运行旧产物；SDK 错误仍判失败。
固定 SDK 的 TextEdit 自缓存会在空多行内容时报告 `cache.prepare` 失败。项目描述用正式 `XUI_CACHE_POLICY_NONE` 直接绘制，保留真实空字符串和正常编辑；单独选择空描述项目已做实际三帧核对。原始 SDK 文件未修改。任务输出首帧显示等待状态，滚动不使用超范围偏移；UTF-8 转换按块追加，防止显示达到上限后逐字符搬移大缓冲。
配置与便携设置增量后，项目编译十六个独立 C 模块，测试增至十一类。配置页额外执行浅/深色原生绘制；含真实编译器时共十九张截图。配置草稿基线和复制条目使用 XRT 的 `xrtValueDeepClone`，避免通过借用嵌套值修改共享 COW 容器。普通标量及不可变元数据仍沿用既有 SDK 所有权契约。
命令目录增量后，项目编译十八个独立 C 模块，测试增至十三类；包括浅/深色目录图，含真实工具链时共二十一张原生截图。构建复制离线 data/commands.json，运行及普通构建不依赖 Python；重新生成元数据才使用开发者工具。详见 [命令目录与帮助](commands-and-help.md)。

长帮助增量后，项目编译十九个独立 C 模块，测试增至十四类；长章节、窄窗口、目录恢复的三张图进入基线，含真实工具链时共二十四张原生截图。文档解析使用固定 SDK 的 xui_document 接口，渲染继续使用同一 XUI 上下文中的原生控件；SDK 文件保持原样。

会话实现位于 services/session.c、ui/layout_state.c、platform/state_file.c 和 editor/coordinates.c，均独立编译。当前共 23 个应用模块；完整回归新增 state_test 与 session_test。GUI 先链接到构建目录，再以 Windows 原子替换发布，并把旧产物保留在构建目录；无需直接截断已存在的发布可执行文件。状态栏动态会话错误借用服务持有的缓冲区，不能传入临时错误字符串。

发布 DLL 与固定 SDK 的 SHA-256 相同则跳过重复复制，避免对已经映射的相同 DLL 执行截断写入；不同内容仍须成功复制，不能静默使用旧 SDK。完整性检查在构建开始时执行。

输入增量新增 input_test，在真实 XGE 窗口运行循环中检查 C/xlang 组合、事件顺序、焦点、候选锚点及快捷键；另有一张原生 IME 组合截图。输入检查位于 build/tests/input/，完整 Release 回归仍检查全部原生绘制的 SDK 错误回调。

字体增量新增 services/editor_font.c 与 ui/font_view.c，共 25 个独立应用模块。font_test 使用真实 Windows 字体与原生运行循环；字体和编译器偏好共用应用设置对象，原设置文件保留未知字段和冲突保护。严格字体加载及复用补全列表适配详见 [代码字体设置](editor-fonts.md)，SDK 文件保持原样。

主窗口增量新增 platform/window_state.c，共 26 个独立应用模块、19 类测试。window_test 在独立 Windows 桌面运行真实 XGE/XUI，桌面不切换到前台；覆盖最大化、正常几何、最小化恢复、工作空间切换与损坏会话隔离、文件冲突和原生过程清理，另保存三张窗口图，包含真实发布程序进程启动与 --size 900 600 覆盖。生产 SDK 未修改，几何 DPI 规划与完整界面缩放分别验收，详见 [主窗口状态](window-state.md)。

DPI 增量新增 platform/dpi_proxy.c，共 27 个独立应用模块、20 类测试。代理适配完整公开回调表，逻辑字号与度量保持稳定，实际绘制字体和目标按 DPI 更新，缺失后端能力仍不宣告支持。dpi_test 保存五张原生比例图，CLI 验收另增加两张，详见 [DPI 与界面字体](dpi-and-ui-fonts.md)。SDK 快照保持原样。


界面字体增量新增 services/ui_font.c，共 28 个独立应用模块、21 类测试。代码字体和界面字体共用严格加载入口，设置与度量独立；弹窗私有内容和打开的覆盖层显式重绑。完整 CompilerPath 回归包含 27 张基线、五张任务和十二张输入/两套字体/窗口/DPI 原生图，共 44 张，退出码 0；SDK 与发布 DLL 指纹一致。设备边界见 [DPI 与界面字体](dpi-and-ui-fonts.md)，当前工具链指纹与历史失败见 [实施进度](progress.md)。


阅读状态增量新增 ui/reading_state.c，共 29 个独立应用模块、22 类测试。完整 CompilerPath 回归 27 张基线、五张任务及十四张输入/两套字体/窗口/DPI/阅读图，共 46 张，退出码 0。reading_test 在独立 Windows 桌面运行真实高 DPI XGE/XUI；发布程序跨进程检查窄窗口的帮助内容锚点，补充旧会话兼容验收通过。协议和限制见 [会话与布局恢复](session-and-layout.md)。


恢复维护增量新增 platform/recovery_store.c，共 30 个独立应用模块、23 类测试。最终严格 Release 重建通过，最近引用保护后的完整 CompilerPath 回归退出码 0，仍含 46 张原生图；recovery_test 另启动实际原生进程核对检查点锁并创建真实 Windows 连接点验证拒绝，未切换用户桌面。完整协议、七天精确边界、当前/上一引用和不可删除文件规则见 [恢复存储维护](recovery-maintenance.md)；备份修复 UI 继续实施。

恢复管理增量新增 platform/recovery_backup.c、services/recovery.c、ui/recovery_view.c，共 33 个独立应用模块、25 类测试。最终严格 Release 含重建完整 CompilerPath 回归退出码 0：29 张基线图、五张真实任务图及十七张输入/字体/窗口/DPI/阅读/恢复图，共 51 张。恢复窗口只使用固定 SDK 公共 CodeEdit/Popup/字体/缓存状态接口；后台服务不调用 XUI，服务退出先取消/回收，未提交的撤下结果回退。损坏 JSON 原字节与普通恢复文件独立备份、当前作用域重建和双语法未命名取回已经实际验证；SDK 及发布 DLL 保持相同指纹。备份协议、作用域边界及后续全局状态修复见 [恢复存储维护与恢复管理](recovery-maintenance.md)。

外部文件增量新增 platform/file_watch.c、services/file_sync.c、ui/file_changes_view.c，共 36 个独立应用模块、28 类测试。最终严格 Release 含重建完整 CompilerPath 回归退出码 0：29 张基线、五张真实任务和二十一张独立原生图，共 55 张。file_watch_test 实际跨进程写入及完成端口通知，file_sync_test 检查真实模型/编辑器协调，file_changes_view_test 通过真实原生输入比较和确认；固定 SDK 未修改。

固定 SDK 的 StatusBar SetItemText 借用调用方文本指针（公开实现 xui_statusbar.c 保存 `.sText`），不会复制动态服务缓冲区。外部监视服务停止时先将状态项恢复为静态文本，再释放服务；停止后的真实重绘及再次启动已经验证。比较 CodeEdit 自持正文副本，列表标签由视图持有并在替换条目成功后释放旧标签。公共按钮状态缓存按已禁用/活动/悬停/焦点优先级同步，与原生恢复窗使用同一已验证方式。

监视队列与待处理队列各自最多 4,096 条/128 MiB，原始快照合计上限 256 MiB，另有读取中的单个 16 MiB 缓冲区和编辑/预览副本。最多 20,000 个目标不代表已经通过大型工程性能验收。具体协议、测试范围与限制见 [外部文件变化与清单协调](file-coordination.md)。

动作增量新增 app/actions.c，共 37 个独立应用模块、29 类测试。最终严格 Release 含重建完整 CompilerPath 回归退出码 0：29 张基线、五张真实任务和二十四张独立原生图，共 58 张。actions_test 使用真实原生菜单/工具栏/按钮及键盘、运行子进程和字体失败回滚，另检查动作 ID/名称/快捷键唯一性与重排绑定；真实 DEMO6 混编及失败构建保护由完整回归另行验证。

SDK 文件保持原样。应用保存并包装公开 CodeEdit/菜单事件处理器，使用公开 provider、菜单条目、按钮状态和字体接口；原生编辑菜单纳入字体重绑。绑定记录由应用拥有并在 XUI 销毁后释放，未注册命令转交原处理器。协议、三个实际菜单图及未实现的快捷键自定义边界见 [动作注册与统一路由](actions.md)。

引用管理增量新增 ui/references_view.c，共 38 个独立应用模块、30 类测试。最终严格 Release 含重建完整 CompilerPath 回归退出码 0：29 张基线、五张真实任务和二十七张独立原生图，共 61 张。references_test 使用真实属性按钮、列表/菜单/键盘输入、两向滚动及实际后台文件协调；指定真实编译器后构建运行 C/xlang 项目，并通过 Windows 版本 API 读取生成 EXE 的 2.3.4.5 版本资源。Windows version 库用于该验收，应用公共 SDK 和发布 DLL 指纹保持一致。

事务同步 files/cSources/resources 与 libraries，保存保留未知字段和磁盘基线；新窗格加入既有 UIFont 绑定/回滚及模态路由。没有更改编译器或 SDK；协议、实际图片、资源内容和库发现的后续边界见 [工程文件、资源与库引用](project-references.md)。

模板增量新增 services/project_templates.c 和 ui/template_view.c，共 40 个独立应用模块、32 类测试。完整严格 Release 重建与 CompilerPath 回归退出码 0，含 64 张新鲜原生图：29 张基线、五张真实任务和三十张独立测试图。template_test 使用真实 DEMO6 验证四模板、C extern 调用及 DLL 导出函数的宿主调用；template_view_test 验证实际原生向导和三张图。窗口子进程失败诊断保留标准流和具体缓存控件信息，原通过标准不变。新预览的词法缓存对应替换后的文档版本；SDK 快照与发布 DLL 不变。协议、边界及图片见 [项目模板与新建向导](project-templates.md)。

文件另存为增量新增 services/file_rebind.c，共 41 个应用模块、33 类测试。完整严格 Release 重建与 CompilerPath 回归退出码 0，包含 66 张新鲜原生图及 SDK 负面检查；两张新图来自 file_rebind_test 的真实后台桌面。更新后的入口、C 源和资源清单经真实 DEMO6 构建运行验证。保存结果回调保证准备拒绝、失败、无变化与成功都清理临时计划；模型提交不依赖写盘后的内存分配，SDK 保持原样。见 [文件另存为与工程引用协调](file-save-as.md)。

文件重命名新增 platform/file_rename.c、ui/rename_view.c，共 43 应用模块、34 类测试、46 公共动作。完整严格 Release 重建及 CompilerPath 回归退出码 0，包含 69 张新鲜原生图和 SDK 负面检查；后台桌面提供三张新图，改名后的入口/C/头文件/编译资源通过真实构建运行。SDK 保持原指纹；只读句柄改名的实际失败记录及显式保护见 [文件物理重命名](file-rename.md) 和 [实施进度](progress.md)。

工程树重命名沿用上述 43 模块，新增 tree_rename_test，当前 35 类测试、47 公共动作。严格完整 Release/CompilerPath 回归通过，含 71 张新鲜原生图及 SDK 负面检查。平台后端新增 Windows BCrypt SHA-256，构建链接系统 bcrypt；未打开文件按 64 KiB 流式核对，无文本解码与整份正文分配。SDK 头、库及 DLL 保持原指纹。二进制、精确硬链接路径、真实混编构建运行与两张原生图见 [文件物理重命名](file-rename.md)。

迁移后端新增 services/project_migration.c，复用 platform/file_rename 的 Windows 句柄机制并增加普通目录身份检查，构建当前为 44 模块、36 类测试，47 动作与 71 张回归图。目标已有对象不覆盖，SDK 与工具链未修改。[FILE_RENAME_INFO 官方说明](https://learn.microsoft.com/en-us/windows/win32/api/winbase/ns-winbase-file_rename_info)描述平台接口；本机具体行为由目录/清单及旧文件改名测试验证。此后端阶段尚未包含原生入口和文档/会话；其后的原生接入已经完成，见 [迁移协议](project-migration.md)。

原生迁移增量继续使用 44 个应用模块，新增 project_migration_view_test，现为 37 类测试、49 动作、73 张新鲜原生图。严格完整 Release/CompilerPath 回归和 SDK 负面检查通过。新夹具使用后台独立桌面，验证实际菜单/按钮、C/xlang 批量文档绑定、任务/监视以及工作空间和默认独立项目的会话重启；没有更改 SDK、工具链或编译选项。日志、指纹和使用方法见 [项目迁移](project-migration.md)。

旧格式导入后端新增 services/legacy_import.c 和 legacy_import_test，最终严格完整 Release/CompilerPath 回归为 45 模块、38 类测试、49 动作、73 张新鲜原生图，退出码 0，SDK 负面检查通过。历史清单从已存在 Git 提交逐字节复制到新测试夹具，参考仓库不写入；导入只读原清单，使用 Windows 句柄锁与新目录发布，不新增运行时依赖。该次验收仅覆盖后端，随后原生接入见下方，协议与原始数据来源见 [旧格式导入](legacy-import.md)。

旧格式原生导入新增 ui/import_view.c 与 import_view_test，严格完整 Release/CompilerPath 回归为 46 应用模块、39 类测试、50 动作、76 张新鲜图（29 基线、5 任务、42 独立原生图），退出码 0。新测试使用独立后台 Windows 桌面，验证原生菜单、报告确认、工程切换、会话保护、C/xlang 语法和真实混合构建运行、缺失文件与实际应用重启；SDK 与独立 DEMO6 验收快照的前后指纹保持一致，详见 [旧格式导入](legacy-import.md)。

本次最终原生导入验收改用独立 DEMO6 Windows 运行依赖快照：共享 xlc.dll 在首次回归期间发生变化，记录保留；2098 项逐文件复制校验后重新严格完整回归，结束时再次核对全部快照内容。工作区共享工具链不锁定、不写入，详见 [实施进度](progress.md)。

Windows 资源属性新增 model/resources.c、ui/resource_view.c 及 resource_view_test，当前 48 模块、40 类测试、51 动作。最终严格完整 Release/CompilerPath 回归为 79 张新鲜图，退出码 0；JSON/有类型 XLON、真实 PE 版本/图标/清单和路径/文本/Base64 均通过。沿用原有 2098 文件独立 DEMO6 快照，SDK 未修改；基础 xrtBase64DecodeNew 使用实际导出，空 TextEdit 背景和有正文缓存分别适配。完整使用与边界见 [Windows 资源属性](windows-resources.md)，日志、指纹及磁盘空间修复记录见 [实施进度](progress.md)。

库候选信息与包文档增量新增 services/library_info，当前 49 模块、40 类测试、51 动作。严格完整 Release/CompilerPath 回归退出码 0，82 张新鲜图；原引用测试增加候选路径、版本差异和声明文件的只读检查，包文档在中央底栏展开，返回保留原独立草稿和提交保护。固定 SDK 无 xrtEnvGet 导出，UI 用 Windows Unicode 环境接口，仍由 xge.dll 唯一持有 XRT。完整检查边界见 [工程引用与包文档](project-references.md)，失败重试、最终时间和指纹证据见 [实施进度](progress.md)。

工具链档案增量新增 services/toolchain 与 ui/toolchain_view，现为 51 模块、41 类测试、52 动作。完整 Release/CompilerPath 产生 86 张新鲜原生图（29 基线、5 任务、52 独立），严格编译和 SDK 负面检查通过。受控工具链夹具使用 windres 生成真实文件/产品版本资源；缓存/异常/超时/取消及真实编译行为经独立 toolchain_test 验证。所有应用模块在 app 状态结构变化后完整重编，固定 SDK 与独立编译器快照未改写。协议见 [工具链信息](toolchain-profile.md)。


语言服务基础新增 protocol/lsp_wire、services/language_check、services/language_client 与 ui/language_view 四个应用模块，以及独立 server/xlang_ls 入口。新增 lsp_test、language_view_test 和受控进程夹具；process_test 验证 stdout/stderr 分离、独立有界环和原始日志。完整回归继续执行既有双语法、工程、保存/迁移/恢复及真实 C/xlang 混编验收。详见 [语言服务](language-service.md)。

最终语言服务基础增量在完整重建下通过 55 应用模块、43 类测试、54 动作和 90 张新鲜原生图验收。独立 xlang-ls.exe 的导入表不包含编译器 DLL 或 XUI 调用，使用固定 xge.dll 的 XRT；核心文件和完整 2098 文件快照保持一致。最终日志及指纹见 [实施进度](progress.md)。


编译器覆盖前置使用独立源码构建与新的 2098 文件运行快照，保留旧快照与固定 SDK。原生 C API 宿主和 CLI 20 组门禁验证虚拟入口/递归模块、源码包依赖、来源、边界/隔离、Cgen/build/run 及原字节不变；Python 仅为开发验收驱动。新工具链下既有 IDE 协议/原生视图也通过。已有前端 char* 文本断言和两个 API 范例的失败经旧 DLL 对照复现，未称全编译器回归通过。路径、日志、指纹与具体边界见 [实施进度](progress.md) 和 [源码覆盖接口](../../docs/IDE源码覆盖接口.md)。

编译器来源报告继续在全新隔离源码目录构建，不向应用/服务引入 xl/xlc DLL，也不改固定 XGE SDK。报告实现为 C，独立 native/CLI 质量门使用 Python 驱动；实际 loaded-sources-and-manifests 范围和 GUI 尚未接入的边界见 [语言服务](language-service.md) 与 [源码覆盖接口](../../docs/IDE源码覆盖接口.md)。本轮证据与保留的 CLI 首次失败见 [实施进度](progress.md)。


全文快照后端新增 `services/language_snapshot.c`，当前严格 Release 构建为 56 应用模块、44 类注册测试、54 公共动作。独立服务只链接协议/平台/后台模块，继续由固定 xge.dll 拥有 XRT；不载入 xl/xlc DLL。`snapshot_check_test` 及开发 Python 协议门需要声明 check JSON/source-overlay/source-report 且返回 resolutionReport 的现代 CompilerPath；旧编译器仍可执行已有保存后检查。旧的 55/43/54、90 图全量基线为历史验收，本增量执行相关 9 类原生回归及 44 项快照协议门，没有把历史全量结果当作本轮重跑。


GUI 全文快照接入后仍为 56 应用模块，注册测试增加为 45 类，公共动作增加为 55；新增 snapshot_view_test 使用真实 CompilerPath 和原生 UI 验收多文件标注与定位。独立服务的文件组携带规范化正文摘要及磁盘报告，传输队列覆盖整批 64 文档的关闭/全文重开和加载图通知。当前完整回归记录见 [实施进度](progress.md)。


最终 GUI 快照增量严格重建全部 56 模块、独立服务和 45 类测试，随后完整 CompilerPath 回归通过 44 项协议与 94 张新鲜原生图及 SDK 负面门。snapshot_view_test 直接编译相同 language_view 实现以回放坏结果，链接时排除该重复对象；没有产品测试接口。含中文字面量的 PowerShell 脚本保留 UTF-8 BOM，兼容 Windows PowerShell。完整清单与失败记录见 [实施进度](progress.md)。


Windows 便携发行增加独立载荷装配与原生迁移验收，应用仍为 67 模块，注册测试为 57 类。`tools/package.ps1` 只复制四个 IDE 主程序/运行时、离线目录和显式验证的 xlang/clangd 运行资产，不复制开发测试程序或用户状态。来源、摘要、核心 PE 导入与发布材料范围保留。包内默认工具路径为相对路径；独立 GUI、诊断程序和原生 C/xlang/F12/混编在中文空格目录中验收。开发 Python 仅驱动打包和测试。

本轮为编译器 `xlGetModuleDir` 使用 Windows Unicode 模块接口，再转为 UTF-8；现有 512 字节路径缓冲保持，不据此宣告所有长路径可用。修复在新的独立来源快照中完整构建，已有候选、固定 SDK 和共享 `release` 保持。便携流程与材料边界见 [便携发行](portable-distribution.md)，完整回归、保留失败和后续首版工作见 [实施进度](progress.md)。
