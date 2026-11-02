# Windows OpenGL 启动兼容

2026-11-02：用户在 Windows 7 本地物理桌面、AMD Radeon HD 4200（256 MiB）上启动 IDE，报告 `sokol_app.h:9064` 的 `_sapp.wgl.gl_ctx` 断言；机器内存为 2 GiB。本轮修复其对应的版本请求和失败处理问题，目标硬件尚未实测。

## 原因与修复

已按 SDK 的源文件清单核对匹配的 XGE/Sokol 源码，且源构建 DLL 与原 release DLL 的 SHA-256 相同。XGE 后端自称 `opengl33`，渲染器使用 GLSL330，但 Sokol 描述没有设置版本；该 Sokol 版本在 Windows 上默认请求 OpenGL 4.3。AMD 的 [Catalyst 10.6 官方说明](https://drivers.amd.com/relnotes/Catalyst_106_release_notes.pdf)列出 HD 4200，并说明 Radeon HD 系列的 OpenGL 3.3 支持。这为本机的兼容路线提供依据，不代表已经核实用户安装的具体驱动。

截图行号实际属于上下文销毁函数。原 Sokol 的 WGL 创建流程在 PANIC 日志后仍会继续，而 XGE 日志回调会返回；失败可能进入没有上下文的循环和清理，最终触发二次断言，掩盖原始驱动错误。

本轮修改仅应用于固定 SDK 的匹配源快照，未修改其他项目中的 XGE 工作区：

- Windows GLCore 描述明确请求 3.3，其他平台的版本选择保持原行为。
- WGL 库、必要导出、辅助窗口／DC、临时上下文、扩展和实际上下文的已覆盖失败路径立即返回；真正的 `wglMakeCurrent` 失败也会终止启动。
- 部分初始化只清理已取得的 WGL 资源；没有上下文时不删除空句柄，不调用场景初始化或帧回调。
- XGE 保存失败状态并让 `xgeRun` 返回错误，通过既有错误回调传递原 Sokol 消息、日志项、位置及 Win32 错误。
- IDE 复制并保留首个 `graphics/sapp/wgl` 原因，后续通用退出信息不覆盖它。图形尚不可用时沿用已有 Win32 启动失败提示及日志路径。

没有关闭断言，没有更换渲染框架或加入软件 OpenGL 库。仍要求驱动支持 OpenGL 3.3 Core。

## 验证与交付

固定 759 项源材料前后相同，XGE 全新构建 165 个单元、零复用；沿用原 SDK 的编译警告配置，不将其描述为全 SDK Werror 验收。六份公开头及 XRT 源／声明资产保持，公开导出名称仍为 6,495 个。构建指纹随当前编译器标识重新生成，GUI 101 个单元按同一 SDK 完整严格重编。

专用测试 DLL 在真实 Win32/WGL 生产边界注入故障，只存在于私有验收目录，不进入发行 DLL。兼容控制拒绝高于 3.3 的请求，观察到实际请求 3.3；另七种情形为缺失库、必要导出、临时上下文创建／绑定、扩展入口、实际上下文创建／绑定失败。均得到错误返回、零帧、一次错误回调，所有创建的 HGLRC 均释放。

GUI 在相同故障下退出 1 并保留原图形诊断，无 CRT 断言；正常及兼容控制可渲染。六项回归为 SHA-256、工作台、编辑器、原生输入、窗口状态和 DPI，覆盖 C／xlang 输入、删除、撤销、Unicode IME、焦点及 100–200% 缩放。首轮控制器误把原有 `.log` 诊断当作 JSON 查找，报告保留；只修正测试夹具，未修改产品源码，后续验证通过。

为保护可用 release 并进行匹配 SDK／真实故障对照，构建材料位于 `ide/build/Release/wgl-compat-20261102/`，较大的候选 SDK、运行资源和验收位于 `E:/codex-builds-xlang-wgl-compat-20261102/`。验收后已成套更新固定 SDK、root release 的 `xge.dll` 和 `xlang_ide.exe`，实际 root C／xlang 隐藏启动通过。引擎、LSP、用户设置和命令目录数据保持原字节。

原命令目录与引擎绑定不一致仍是已有构建限制，本轮沿用定向 GUI 重建，不把它记为通过，也不构成完整发行或 Windows 7／HD 4200 实机准入。证据见 [ide-wgl-compat-20261102.json](../../dev/release-materials/provenance/ide-wgl-compat-20261102.json)，原始源码与控制器保存在 `dev/release-materials/sources/ide-wgl-compat-20261102/`。更新运行包需要同时替换 GUI 和 XGE DLL。
