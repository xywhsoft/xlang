# 固定来源与发行材料

2026-10-11 当前扩展库交付：SDK67，84 模块／106 产物重新严格构建，含渲染的完整回归和十项实际便携门通过。默认编译器修正 stdio 外部／内联声明冲突，并为真实扩展函数 C 原型提供可验证的 xlang 来源映射。CRC32、MD5 的实际混编、stdio 格式化、诊断、悬停、补全、F12 和间接头变化撤回已通过开发与包内检查；最终构建的 17 项原生界面断言和三张截图、九项编译器兼容、41 项源码材料及原有 74 项聚合／37 项标量对照通过。 详见[扩展库分析与导航](c-native-extensions.md)与[当前候选](../CURRENT-CANDIDATE.md)。下文保留此前阶段的数量和验收记录。

2026-10-11 当前 C 聚合与位域交付：SDK67，84 模块／105 产物重新严格构建，含渲染完整回归及八项实际便携门通过。默认 C/xlang 桥接新增实际 TCC 位域布局查询；开发与随包提供者各通过 23 类聚合、74 项尺寸／对齐／偏移对照，22 项元数据和用户参数优先级检查、三个实际 TCC 状态执行、九项编译器兼容检查及 41 项源码材料检查通过。原有双语法、定版布局、三层管理、编辑与构建运行保持可用，C 输入观察继续启用。 来源、适用范围和失败记录见[C 聚合与位域布局](c-record-layout.md)，启动入口及本轮精确构建见[当前候选](../CURRENT-CANDIDATE.md)。下文的既有数量和候选链接保留为相应历史阶段记录。

当前 SDK67 开发候选提供 243 份匹配源码材料，覆盖 119 个翻译单元及 213 项实际预处理依赖，完整 SDK 严格构建与公开接口比较通过。应用完整回归和同一精确构建的实际便携门通过，见[正则边界](find-boundaries.md)与[当前候选](../CURRENT-CANDIDATE.md)。下文原有数量与构建记录对应早期冻结阶段；整体分发／重链接材料核对及正式发行状态保持未完成。

`tools/package.ps1 -SdkSourceRoot <源码快照目录>` 将固定 XGE SDK 的来源和已有许可文本加入开发候选。目录必须匹配 `third_party/xge_sdk/xge-source-files.json`；当前 XGE 工作树不能替代旧快照。工具不重新构建、更换 SDK 或工具链，也不修改来源目录。

`data/xge-sdk-materials.json` 固定了 SDK DLL、原来源清单及所选文件摘要。选取范围来自旧快照 `build_dll.bat` 的 119 个 C 编译单元，以该脚本的 IDE profile 宏做真实 GCC `-MM` 预处理，得到 206 个项目内依赖文件；加上构建/资源入口、原 `lib/` 文件及具名材料，共 236 个文件、39,941,790 字节。编译器自带的系统头文件不在这个来源范围内。此范围用于核对已发现的输入，不宣称重建出的 DLL 必然逐字节相同。

包内 `sources/xge/` 保留原路径和字节；`notices/xge/` 另列 7 份具名文本，包括 libunibreak 的 `LICENCE`、Unicode、md4c、zstd、Twemoji 和 DockPanelSuite 文本。stb 三份头文件、sokol、miniaudio 与 XRT 的内嵌文本保留在完整原头文件中，材料索引指向它们。原快照中的 XGE/xlayout 整体分发条款仍需核对，不能据同作者或某个依赖的许可推断整个 SDK 的条款。未选取开发工具程序、无关字体文件及测试二进制。

`sources/tcc/` 继续保留原来的 95 份 TCC 文件，`notices/tcc/COPYING` 保留原文。当前源码候选另以 `-CompilerSourceManifest <显式源码清单> -CompilerSourceBuildReport <实际构建记录>` 提供 `sources/xlang/`：原冻结清单的 1,512 份源码／资源，加上同一冻结副本中 `build_dll.bat` 直接使用的 9 个 DLL 示例／测试源文件，以及 11 个根构建脚本，共 1,532 份输入。原清单、补充清单、修改描述和源码摘要保存在 provenance；`sources/xlang/SOURCE-MATERIALS.json` 关联随包 xl.exe/xl.dll/xlc.dll 的摘要，`sources/COMPILER-REBUILD.md` 给出构建步骤。

首轮 GUI 父进程验收的第四场景在清理后的瞬时窗口响应断言失败，原检查器、日志与失败记录保留。原记录缺少失败点的等待状态／窗口错误，因此不推定历史原因；检查器新增有界响应等待、进程身份及每次观测，再单独重跑全部四个 GUI 场景。此前六项通过门沿用其原记录，没有重复计作重跑。

本机实际从只含这些输入、无生成对象或既有编译器的独立副本执行 `cmd /d /c build.bat`，生成三个程序／DLL，退出 0；原始输入保持字节不变。新产物通过实际 C/xlang 混编及 IDE/clangd 标量对照，随包运行编译器仍保持原字节。两份 DLL 的重建摘要与运行版不同，本材料不承诺逐字节可复现。构建需要 Windows x64、MinGW GCC 与 PowerShell；IDE 运行不依赖这些开发工具。所附根验收脚本保留来源字节，其全套测试素材不属于本次构建输入清单。

首次独立构建缺少上述 9 个 DLL 构建素材而失败，随后一次准备检查存在旧数量断言，两次记录均保留；补齐后第三版实际独立构建通过，未将失败计作成功。39 项材料检查覆盖冻结来源、遗漏、摘要、构建日志、实际输入绑定、越界与重解析目录，以及组装前拒绝。见[独立构建](../build/compiler-source-materials-preflight-v3-run.json)、[便携验收](../build/compiler-source-materials-portable-v2-run.json)与[最终核验](../build/compiler-source-materials-final-v1-verification.json)。

源码和实际构建材料已补齐技术范围；TCC 静态链接／重链接义务、整体来源与分发条款的核对继续，`relinkReviewComplete` 和 `legalDistributionReviewComplete` 保持 false。clangd 原始文本位于 `toolchains/clangd/LICENSE.TXT`，编译器扩展的原始文本随运行资产保留。扩展素材与 IDE/xlang 自有项目分发条款仍在材料清单中。省略编译器源码参数时，打包器明确记录源码未提供；给出构建报告但没有源码清单会拒绝。

不给出 SDK 来源参数时，打包器记录来源缺失，不搜索当前 XGE/XRT 工作树并把新版文本归于旧 DLL。给出了参数时，缺文件、长度或摘要变化、链接/重解析路径、错误来源清单或 DLL 摘要均在建立打包阶段前拒绝。复制后和发布前再次核对全部实际输入。原有 ZIP/文件数量、长度、路径及复制预算保持不变。

开发机可运行 `python tools/check_sdk_materials.py --sdk-source-root <目录>` 核对真快照和私有副本中的破坏案例；候选仍需通过 `tools/check_portable.py` 的实际迁移及 C/xlang 验收。来源核对与便携验收不会将 `formalRelease` 或 `legalDistributionReviewComplete` 置为 true。

开发机可运行 `python tools/check_compiler_source_materials.py --source-manifest <清单> --build-report <记录> --compiler-manifest <运行输入清单> --c-server-root <clangd目录> --c-server-manifest <clangd清单>` 验证真实材料和私有破坏案例。


编辑命令的新开发候选 [portable-20261010-editor-commands-v1](../dist/portable-20261010-editor-commands-v1/) 继续携带上述固定 SDK、编译器源码和独立构建材料；本轮仅更换 IDE 应用，未重建／替换编译器或重跑材料夹具。新包实际装配核对、搬迁、深路径、标量及四个 GUI 场景通过。材料与正式发布边界保持原状态，见[本轮核验](../build/editor-commands-final-v1-verification.json)。


状态栏优化候选 [portable-20261010-editor-status-v1](../dist/portable-20261010-editor-status-v1/) 沿用固定 SDK 与编译器材料，只更换 IDE 应用。83 个应用模块严格重编译、完整运行回归及新包五个专项门重新执行通过；SDK 源码、编译器源码核对和独立编译器构建沿用既有证据。私有大文件实验 SDK 未装入该包，许可完整性与正式发布仍按原边界推进。见[本轮核验](../build/editor-status-final-v1-verification.json)。


2026-10-11 默认 C 输入观察交付：默认便携 C 后端已加入实际输入观察，未打开的间接头文件变化会撤回旧语义结果并重新分析；目录守卫支持超过 MAX_PATH 的 Unicode 深目录。SDK67 沿用已严格构建的匹配材料，应用重新严格构建为 84 模块／104 产物，含渲染的完整回归及六项实际便携门通过。真实输入观察 113 项接口、21 项界面及 52 项导航上下文检查，69 项短／长目录观察检查和 13 项材料拒绝检查通过；普通 clangd 的五项兼容门和修正后草稿启动门也通过。 提供者为此前成功源码构建的精确复用，本轮没有重新编译 LLVM。材料范围为适配差异和既有构建记录，完整上游源码按固定提交获取；正式发行核对仍继续。详见[默认 C 输入观察](c-input-trace-delivery.md)和[当前候选](../CURRENT-CANDIDATE.md)。
