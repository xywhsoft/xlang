# C 分析的原生标量目标

2026-10-11 当前 C 聚合与位域交付：SDK67，84 模块／105 产物重新严格构建，含渲染完整回归及八项实际便携门通过。默认 C/xlang 桥接新增实际 TCC 位域布局查询；开发与随包提供者各通过 23 类聚合、74 项尺寸／对齐／偏移对照，22 项元数据和用户参数优先级检查、三个实际 TCC 状态执行、九项编译器兼容检查及 41 项源码材料检查通过。原有双语法、定版布局、三层管理、编辑与构建运行保持可用，C 输入观察继续启用。 来源、适用范围和失败记录见[C 聚合与位域布局](c-record-layout.md)，启动入口及本轮精确构建见[当前候选](../CURRENT-CANDIDATE.md)。下文的既有数量和候选链接保留为相应历史阶段记录。

xlang 项目中的 C 自动声明桥接可读取编译器同次查询返回的 `nativeEnvironment.scalarTarget`。字段是可选扩展；旧编译器缺少它时仍沿用原有桥接配置。独立 C 文件的目标配置不受此扩展影响。

## 修正依据

固定 v6 编译器实际构建并执行 C/xlang 混合工程，测得 Windows x86-64 的 `long double` 大小和对齐均为 8 字节，含一个前导 char 的结构体大小为 16。此前默认 GNU Windows clangd 目标使用 16 字节的 long double，结构体大小为 32。原始对照同时发现 `__TINYC__`、`__GNUC__`、`__GNUC_MINOR__` 和 `__clang__` 不一致，七条静态断言失败。证据保存于 [原始对照](../build/c-native-scalars-probe-v1.json)。

新的私有编译器查询遵循实际 TCC 类型映射后调用 `type_size`。Windows 上 `TCC_USING_DOUBLE_FOR_LDOUBLE` 把源码 long double 映射到 double；不能直接使用仍为 16 的原始 x87 `LDOUBLE_SIZE`。首个元数据版本被真实执行对照拒绝，见 [保留的失败记录](../build/c-native-target-metadata-failure-v1.json)。

## 查询与应用

`scalarTarget.schema=1`、`scope=tcc-win64-scalar-target`，返回 `tinyCVersion`、`pointerBytes`、`longBytes`、`longDoubleBytes` 和 `longDoubleAlign`。数据来自编译器内置 TCC，不使用 IDE 宿主 GCC 的类型大小，也不在 GUI 内加载编译器 DLL。

客户端只接受当前已验证的 Windows x86-64 配置：指针 8、long 4、long double 大小及对齐 8，且原命令只有一个 `-target x86_64-w64-windows-gnu`。不兼容或无效目标使后台桥接准备失败，原命令保留。通过核对后添加 `-mlong-double-64`、移除有限的 Clang/GNU 身份宏，并按查询值定义 `__TINYC__`。该选项的定义见 [Clang 参数文档](https://clang.llvm.org/docs/ClangCommandLineReference.html#cmdoption-clang-mlong-double-64)。项目和用户分析宏在这些默认值之后应用，保留原有覆盖顺序。

客户端对齐的身份子集是 `__TINYC__`、`__clang__`、`__GNUC__`、`__GNUC_MINOR__`、`__GNUC_PATCHLEVEL__`。不宣称其他 Clang 预定义宏、扩展、位域、复杂聚合或调用约定与 TCC 全部相同。环境的 `cPreprocessorGraphComplete=false` 和 `abiEquivalenceGuaranteed=false` 继续保留。

## 验收工具

`tools/check_c_native_scalars.py` 实际构建并运行混合工程，取得十二种标量的大小和对齐、十一项编译器/平台宏、char 符号性和一个 long double 结构体，再将这些实际测量值写成 C 静态断言。扩展的 `c_bridge_test` 用真实 IDE 后台桥接、未保存 xlang 声明及官方 clangd 解析，并检查不兼容目标的事务拒绝。原始程序、命令、stdout/stderr、保护摘要和每次结果均写入新的私有验收目录。

标准 `tools/test.ps1` 提供可选 `-NativeScalarCompilerPath` 门，另外需要 `-CServerPath` 和用于旧编译器回退对照的 `-CompilerPath`。源码补丁和重现方法见 [私有编译器补丁](../third_party/compiler_patches/native-c-scalars-v2/README.md)。

2026-10-10 首次源码增量验收通过：83 模块严格 Release 构建、修正后的编译器完整 DLL/CLI 构建、37 项实际标量与宏对照、十次顶层检查调用、25 项 class 标量混编检查和十一张新鲜原生截图；其中四张已人工核对。旧查询无 scalarTarget 的兼容性、独立 C 配置、未保存 xlang 声明、真实悬停/补全/跨语言定义、无效目标事务拒绝均通过。原公开声明和映射在新旧编译器间一致。见 [最终核验](../build/c-native-target-final-v2-verification.json) 和 [本轮运行](../build/c-native-target-checks-v2-run.json)。

开发主程序为 [release/xlang_ide.exe](../release/xlang_ide.exe)。通过“文件 → 选择 xlang 编译器…”指定 [本轮验收编译器](../build/compiler-c-native-target-v2-984db9a8777949519fa48b8c9fe2f95f/release/xl.exe)，才能使用新的可选目标查询。首次源码增量时旧便携包未更新；下述新候选已默认提供该查询。

首次源码增量只运行相关的 C/桥接/混编验收，未重跑全部 88 类回归、九套编译器完整兼容性或便携安装验收；之后的完整执行见下节。1523 份输入的额外重现只完成源码准备，没有进行另一遍编译。首版发布、完整 C 输入图与 ABI、干净 Windows 和实体输入/DPI 验收继续独立推进，长期任务 active。


## 完整回归与便携默认工具链

2026-10-10 完整标准运行回归已重新执行并退出 0，含新的原生标量对照门。九套编译器兼容性检查也重新执行并通过；开发和包内版本分别使用真实 C/xlang 混合工程、原始深目录源码及实际执行结果验收。88 类标准测试与独立便携门通过，83 模块严格构建及私有编译器 DLL/CLI 全构建沿用本次源码增量的有效记录，没有把此次运行验收重复计作重新编译。

新 [开发候选](../dist/portable-20261010-native-c-scalars-final-v1/xlang-ide/xlang_ide.exe) 和 [ZIP](../dist/portable-20261010-native-c-scalars-final-v1/xlang-ide-windows-x64.zip) 默认使用随包编译器与官方 clangd，不需要手动选择构建目录中的验收编译器。两组各 37 项实际 C 标量对照通过；包内对照使用随包提供者和同份已严格编译的开发版 IDE 桥接夹具，该夹具不随包分发。实际包内程序另通过默认配置、搬迁和真实混合构建/运行验收。

便携检查核对 2808 份载荷和 15 项实际拒绝门；两次原生搬迁运行、四次包内程序运行及十二张图通过。四个实际 GUI 场景覆盖开发/包内来源的 C 与 xlang，父进程只在本次私有桌面中结束，子进程完成 600 帧后正常退出。新图和人工核对范围见 [最终核验](../build/c-native-portable-final-v3-verification.json)。上一候选、两份被拒绝的标量实现及原始日志均保留。

本节证明有限原生标量配置与当前提供者的实际检查；`cPreprocessorGraphComplete`、`abiEquivalenceGuaranteed` 仍为 false。完整输入图与 ABI、实体 IME/混合 DPI、干净 Windows、无短名深工作目录及其他既有发行验收继续推进，长期任务 active。
