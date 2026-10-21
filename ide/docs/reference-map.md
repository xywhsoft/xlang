# 参考项目复用清单

本清单把已有实现映射到新 IDE 的目标模块。路径按当前开发机记录，实施时以实际源码和 SDK 核对为准。状态为 2026-10-05 设计定版。

## 参考优先级

`D:\GIT\auto_test_ide` 是主要实现参考，提供 C、XGE、XUI 工作台、主题、补全、帮助和协议经验。`D:\GIT\xlang_ide` 提供工作空间、项目、文件管理的业务语义；`D:\GIT\x-lang\ide_old` 提供 xlang C 界面的早期接入经验。

新 IDE 使用独立工程，不将 AutoTestIDE 的业务功能和环境依赖整包带入。旧 IDE 的 C#、ScintillaNET 与 WeifenLuo 技术栈不进入新程序。

## AutoTestIDE 源码映射

下列相对路径均基于 `D:\GIT\auto_test_ide`。

| 来源 | 复用内容 | 新 IDE 处理 |
| --- | --- | --- |
| `src/main.c` | 初始化、主循环、输入转发、菜单和编辑器装配经验 | 抽取通用部分，拆分生命周期、动作、服务和视图；不复制整个应用结构 |
| `src/dock_windows/dock_layout.c` | Dock 保存、加载、过滤、空分区修复与标题管理 | 改为稳定逻辑 ID；默认树固定为左右 Dock 加中央编辑器与底栏 |
| `src/dock_windows/integrated_help.c` | Column 文档流、参数表、返回值、只读范例、滚动测量 | 重点参考；替换命令模型和示例语言，支持长文档，避免照搬短描述高度上限 |
| `src/dock_windows/development_assistant.c` | 命令分类、条目选择与帮助联动 | 提取命令浏览，去除自动化开发辅助业务 |
| `src/dock_windows/workspace.c` 与 `resource_manager.c` | 树、菜单和资源视图交互 | 接新三层模型，树节点与持久化对象分离 |
| `src/editor/completion_native.c` | 候选过滤、会话、确认、插入、大候选集处理 | 接统一目录与语义候选，不保留 Python/cyou 绑定 |
| `src/editor/python_language.c` | CodeEdit 注册、高亮与折叠接入方式 | 改为当前 xlang 词法规则，不直接复用 Python 规则 |
| `src/lsp/lsp_client.c` | stdio 分帧、请求关联、文档同步、诊断和定义跳转 | 拆出协议层，替换 `ty` 和 Python 环境，补文档版本与坐标验收 |
| `src/debugger/dap_client.c` | DAP 分帧、UI 线程轮询与事件管理 | 后续参考，替换 debugpy/runner；不能当作现成 xlang 调试器 |
| `src/dock_windows/task_manager.c` | 任务表格、状态呈现和日志交互 | 只保留 IDE 检查构建运行测试任务，去除云与设备业务 |
| `src/theme` 与 `docs/THEMES.md` | 角色令牌、切换、持久化、控件覆盖和渲染测试 | 使用新应用配置命名空间，覆盖全部新面板 |
| `docs/XGE_SDK.md` | 成套 SDK、来源指纹、隔离候选与回滚原则 | 固定为新 IDE 依赖规范，不允许混用 DLL 与头文件 |
| `tools/build_xge_sdk.py` 与 `verify_xge_sdk.ps1` | SDK 快照构建与校验 | 参考并移除应用专有断言，保留一致性和来源验证 |
| `tests/completion_native_test.c` | 真实补全行为及大目录回归 | 改为 xlang 目录与插入契约 |
| `tests/layout_native_test.c` | 原生布局测量断言方法 | 重建定版 Dock 与新属性窗格 fixture |
| `tests/theme_native_test.c` 与 `theme_render_fixture.c` | 控件主题断言及真实 GPU 截图 | 为新工作台做覆盖，避免依赖原 AI runtime archive |

## SDK 与构建边界

AutoTestIDE 的 SDK 位于其保护快照 `third_party/xge_mainline`。新 IDE 的目标位置为 `third_party/xge_sdk`，成套包含同次构建的 DLL、导入库、公开头、XRT 声明头、配置和来源记录。

应用构建只使用已验证快照。上游升级先产生隔离候选，验证后成套部署。SDK 与应用一起回滚，不直接补丁修改生成头，不每次构建追随浮动上游。

AutoTestIDE 的 build.bat 同时包含 AI archive、Python/cyou、scrcpy、设备与多个程序打包，不能原样作为新 IDE 构建入口。新脚本只编译新 IDE 与明确依赖，保留 SDK 校验、GUI/诊断模式、资源和字体部署。

## 旧 xlang IDE

`D:\GIT\xlang_ide\TeemoBot IDE\Module\GlobalDefine.cs` 的 EditWorkSpace 与 EditProject 体现三层管理、工程引用和保存规则，适合借鉴业务行为。其 TreeNode、窗体和模型混合，不照搬为新 C 模型。

`D:\GIT\x-lang\ide_old\src\workspace\xide_workspace.h`、`src/views/xide_project_view.c`、`src/lang/xide_xlang_language.c`、`src/ui/xide_dock_layout.c` 可参考早期接入。固定容量、树节点 ID、展开状态与业务对象耦合需要拆除。

`ide_old/docs/IDE工程组织.md` 中从 AutoTestIDE 复制 DLL、从浮动 XGE 读取头文件的方式仅代表旧阶段，已由成套 SDK 规范替代。旧界面不作为本次布局标准，旧工程格式必须通过明确迁移进入 DEMO6 格式。

## xlang 当前权威接口

| 来源 | 用途与限制 |
| --- | --- |
| `D:\GIT\x-lang\demo6\docs` | 语言、ABI、标准库、import/package 和工具链设计的权威入口 |
| `demo6/src/driver/xl6_xlc_shell.c` | 当前 CLI、项目根字段及资源输入约束 |
| `demo6/release/include/xlc.h` | 编译上下文、源码输入、check、AST/IR/C、exe/shared 等接口；不代表完整语言查询接口 |
| `demo6/docs/调试器与task_async_generator设计.md` | 明确完整调试器仍为后续设计，正式 CLI 不提供完整 dbg/dap/xldbg |

实现每个模块前阅读对应来源，再改造为新服务接口。文档中的新目录、协议和清单字段是设计契约，不能反向当作已有源码事实。

## 已实现的外部文件协调参考

2026-10-06 增量参考 AutoTestIDE `src/main.c` 中外部文档变化、保留当前内容与下一次变化重新提示的行为。新实现拆为 platform/file_watch、services/file_sync、ui/file_changes_view，使用完成端口和动态目标队列；不复制固定编辑器数组、主线程逐个内容读取或应用全局状态。清单事务解析、稳定项目引用与原生只读 C/xlang 比较使用新模型及固定 SDK 公共接口。实际范围与独立原生验收见 [外部文件变化与清单协调](file-coordination.md)。

工程引用窗格借鉴 AutoTestIDE resource_manager 的原生文件选择、独立 UI 操作和目录/编辑器分离，以及新工作台的 Popup/List/字体/滚动模式；三层归属继续遵循定版模型。实现为独立 ui/references_view.c，事务在 model/workspace.c，未复制 AutoTestIDE 的脚本业务或大型 main.c。旧工程格式的资源与库还需专门导入，未将旧 BootCode/.xson 作为当前编译器字段。详见 [工程文件、资源与库引用](project-references.md)。

2026-10-06 模板向导参考 AutoTestIDE src/dock_windows/resource_manager.c 的新文件/模板选择和草稿交互经验，改为新 IDE 的项目级完整生成服务。固定四模板依据实际 DEMO6 export_to_c、export_add 与 c_abi_types 案例，使用 c_int 和真实导出 ABI；没有复制 Python/cyou 模板发现及脚本业务。模型、目录提交事务和只读 C/xlang/清单预览分属独立服务/视图，实际验收见 [项目模板与新建向导](project-templates.md)。
