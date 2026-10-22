# XGE 3 更新

2026-10-21 将 IDE 的固定 SDK 更新为 XGE 3.0.0，上游提交 `f248fd17192b3eb81c2a29dc34549b696e6e4c43`。更新开始时已通过 `git ls-remote origin HEAD` 确认此提交与 `D:\GIT\xge` 的干净工作树一致。上游地址为 `https://gitee.com/xywhsoft/xge`。

## 成套 SDK

从该提交的 Git archive 在 `build/xge3-upgrade-20261021/source` 中独立构建 `ide` 配置，初次上游构建实际编译 165 个单元，未复用旧对象。相邻回归发现最新上游尚未包含此前 SDK67 的 Unicode 查找边界修复，因此在隔离源码中保留一份有明确差异与摘要的兼容补丁，再重编相应单元并重链 SDK；`D:\GIT\xge` 保持原样。当前 SDK 是 XGE 3.0.0 加这一份 IDE 兼容补丁，不能称为未修改的上游二进制。XGE 使用自己的正式构建参数；IDE 的 C 模块继续使用 `-Wall -Wextra -Werror`。

`third_party/xge_sdk` 包含匹配的公开头文件、`xge.dll`、`xge.lib`、`xge_build_config.h`、XRT 声明与配置、来源清单和实际构建报告。XRT 沿用此 XGE 提交随附的 `ca0856824f649147419f3dc1342d5e8ce5a893ff` 来源快照；其自身的 `source_worktree_dirty` 来源信息保留，没有另行替换为其他 XRT 版本。

新的 `tools/import_xge_sdk.py` 只在全新目录装配 SDK。它核对 Git archive 的提交号，根据编译器实际依赖记录收集源码闭包、构建入口、公开头和许可材料，并逐文件与原 Git archive 比较。差异仅接受显式补丁清单中原始摘要、实际摘要和补丁载荷都相符的文件，其余源码变化拒绝装配。当前材料含 752 份文件，覆盖 165 个编译单元、HarfBuzz、SheenBidi 和原有第三方来源；完整发行条款审核仍沿用既有状态。

兼容补丁位于 `third_party/sdk_patches/xge3-find-compat`，修改 `src/xui/controls/xui_find.c` 与 `src/xui/code/xui_code_edit.c`：按完整 UTF-8 标量推进零宽匹配、保留原字节、检查转义长度／UTF-16 代理对／非法 UTF-8，在替换前验证输入，并保留无效表达式提示及旧结果清除。公开头和 XGE 3 API 未改。补丁清单、差异、载荷与实际修改后的源码一并进入 SDK／便携包的来源记录。

验证器从清单读取 XRT 目录和生成的配置头，检查全部文件摘要、共享 XRT 模块、声明来源与必要导出。应用按相同配置头重编；DPI 原生检查还将实际 DLL 的 API 主版本、ABI 指纹与编译时常量比较。`xge.dll` 继续是 IDE 的唯一 XRT 实现所有者。

## IDE 适配

- XUI 代理接口由版本 10 更新到 16。文本测量、塑形和绘制改用 `xui_text_item_t`，保留全文上下文、语言、脚本、范围和颜色信息。
- DPI 代理保持逻辑布局、物理绘制字体分离。保留形状的绘制结果分别持有逻辑与物理资源，并转发新版范围测量／绘制接口；销毁时释放两组资源。缩放发生变化后拒绝不匹配的旧绘制缓存，由控件重新绘制。
- 使用新版公开文本接口与编辑器自身的布局实现，移除旧 SDK67 的私有流式塑形和游标注册调用；对应性能观察程序也更新为公开接口。
- 纹理创建与更新按 XGE 3 的左上坐标合同处理，移除旧 OpenGL 后端专用的纵向翻转。
- 主题删除已移除 RichEdit 控件的注册和颜色键，保留四套主题与实际使用的文本控件。
- 资源序列化与往返检查使用当前 XRT 的 `xrtXlonParse`／`xrtXlonStringify`。非 JSON 资源表单继续使用已知字段投影，扩展值由原文服务维护，避免新版解码器扩大 DOM 后改变既有编辑／删除行为。
- 编译器输入收据及语义快照继续使用原有 Unix 微秒合同。新版 XRT 的时间表示不直接用于此协议：平台边界从同一文件句柄读取 FILETIME，保留亚毫秒精度，同时继续检查身份、大小、时间、内容摘要和读取前后一致性。测试中的 XRT 等待时间改用具名单位，保持原有秒数。
- 窗口状态钩子兼容新版 TSF 的消息链。IDE 状态关闭后若仍有外层原生钩子引用其过程，就保留无主转发链；重新打开时复用原链，避免再次插入产生递归。测试模拟外层钩子晚于 IDE 安装，覆盖关闭、消息分发、重开与最终还原，并保留实际最大化、最小化、工作空间切换与独立进程重启检查。

## 开发与发布

```powershell
.\build_current.bat -Configuration Release -Tests -ToolchainDirectory E:\software\w64devkit\bin
.\package_current.bat --output D:\GIT\x-lang\ide\dist\新的候选名称
```

固定输入配置 `data/portable-build-inputs.json` 指向新 SDK 的源码根，编译器与 C 服务器保持原有已验证版本。测试和开发输出留在 `build`，通过验证的完整便携载荷再同步到 `release`；发布目录不混入测试程序和探针。实际当前构建、候选与验证范围见[当前候选](../CURRENT-CANDIDATE.md)。

## 本轮验证

2026-10-21 已更新为 XGE 3.0.0（`f248fd17`，XUI 代理 16），保留有来源记录的 Unicode 查找兼容补丁。严格构建 `cb-4a6a6215` 为 88 个编译对象（87 个 IDE 模块与 1 个 C 输入观察辅助对象）／108 项产物，构建期间源码变化为空；当前提供者对应的标准检查分段完成（含 29 张基础渲染，原脚本资源夹具分支失败保留），14 项实际编译器／语言服务检查及便携搬迁验收通过。`release` 为完整干净载荷，`dist` 仅保留本候选。适配、证据与范围见 [XGE 3 更新](xge3-upgrade.md) 和 [当前候选](../CURRENT-CANDIDATE.md)。

SDK 来源材料 18 项、导入拒绝 5 项检查通过；原生 DPI 保留文本绘制覆盖 100／125／150／175／200% 缩放，文件收据测试显式保留亚毫秒精度。窗口检查覆盖外层原生钩子的关闭／重开、实际几何恢复和三次独立 IDE 进程。最终发布后再次检查 C／xlang 启动与完整载荷摘要。历史失败、中间构建与修复记录可在 [本轮核验](../build/xge3-upgrade-20261021/result.json) 和工作区外归档中查到；本次未重新验收可选旧编译器回退、全部 ABI 专项及性能。


## 2026-10-23 开发 SDK：背景指示器与折行视口

LSP 接入准备阶段 M7a2b2a 在同一上游 f248fd17 基线上扩展 BACKGROUND 指示器的实际绘制。私有可见范围索引复用诊断排序方案，合并重叠背景，使用原生完整行字形／制表布局并按折行起点定位。原查找兼容补丁保留，预留查找 ID 7。同步修正反复求解滚动条引起的折行索引重置，以及布局变更后光标补测长度未及时发布的问题。只支持新增 BACKGROUND 绘制，未扩展所有样式。

新匹配 SDK 由 `build/lsp-next-20261022/m7a2b2-sdk-native-v1/source` 构建，旧 `build/xge3-upgrade-20261021/source` 不变；`data/portable-build-inputs.json` 和 `data/xge-sdk-materials.json` 绑定新来源。`third_party/sdk_patches/xge3-find-compat` 沿用历史目录名，现含查找、注释仓库和编辑器三个有原始摘要／差异／载荷的补丁；同一载荷进入 SDK provenance，组件测试直接使用 SDK 内精确来源。公开头全部一致，6463 个导出名称及序号一致，XGE 继续独占 IDE 的 XRT。

完整开发构建 `cb-59c083d6`、25 组原生、424 项协议、22 组 SDK／IDE／旧服务兼容检查通过；8 场景像素、138345 项范围专项及 18 项来源材料检查通过。当前 SDK 与测试位于开发目录，已发布的 `release`／`dist` 保持原候选。性能测量的边界和未完成项见 [LSP 分析核心](lsp-analysis-core.md#m7a2b2a原生背景绘制与布局)，总体响应速度目标及 IDE 自动光标高亮接入继续。
