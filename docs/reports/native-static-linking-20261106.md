# XGE 静态接入与 TinyCC 按需链接验收（2026-11-06）

核心实现与静态 SDK 工具已落地，正式发行文件尚未替换。Windows 的 TinyCC PE／TLS 系统装载验收仍受 Defender 拦截；Linux 的记录覆盖隔离 SourceDriver／VFS 和真实独立 ELF，并非重建正式 Linux DLL。

## 实现

- 包清单通过 `artifact.nativeLink` 声明函数分节、数据分节及最终链接 GC。旧提供方缺能力会明确拒绝，普通包保持原链接行为。
- TinyCC 为函数和数据分别生成节，沿真实重定位保留依赖、回调、导出、构造／析构与展开信息。语言 JIT 模块显式保留模块 ABI 和导出入口。
- Windows x64 可读取已限定、验证的 GCC COFF 对象及静态库，保留原生 O2 机器码。系统导入以有证据的函数／DATA 分类和 `.def` 描述，不把 GNU 导入碎片当普通代码链接。
- XGE／XUI 可生成独立静态包根，继续使用 `import xge`／`import xui`。包内引擎归档不定义第二份 XRT，通过声明使用程序的同一份 XRT；动态包继续保留。
- 固定运行库预编译对象启用独立节，配套生成两份固实 VFS。增量清单记录每个对象的真实编译器、参数、来源和继承链，避免把复用对象描述成全量重编。

新增裁剪与对象读取工作位于编译／链接阶段。普通声明方法没有新增转发函数、动态装箱或操作码分派；既有拥有权与错误边界继续保留。宿主和 GCC 引擎使用 `-O2`，未启用 Oz／LTO。

## 最终集成证据

| 范围 | 结果 | 限定 |
| --- | --- | --- |
| Windows 正式 runtime／compiler DLL 配置 | 22 项通过 | CPU JIT、模块、嵌套编译、错误恢复；并非 SourceDriver 实现对象替代正式 API |
| 同一正式 DLL + 静态 XGE | 6 项通过 | 颜色、本地拥有对象、共享、内嵌 NUL；JIT／模块 |
| 既有 CLI + 新正式 DLL | check／cgen／run 全通过 | 静态 XGE 示例，未产生 TinyCC PE |
| Windows 配套 VFS | 12 项源码隔离 JIT + 3 项静态 XGE 通过 | 1,566 个对象，最后 6 个重编、1,560 个保留原始路径与哈希 |
| Linux 配套 VFS | 源码隔离 40 项通过；静态 XGE 6 项通过 | 1,564 个初始新对象，后续 6 + 2 个定点更新；独立 ELF 已移位执行 |

40 项 Linux 源码隔离验收先于最后的 COPY 修复；API、Core 和目标 runtime 对象未再变化，因此保留该报告。最后两项 linker 对象变更使用独立 COPY 回归和真实静态 XGE 独立程序复测覆盖，没有伪称重跑 40 项。

集成中修复了默认动态 VFS 缺少库搜索路径、ELF 节数预算误拒合法大对象，以及 GC 后迟分配的 COPY 重定位丢失 `.bss` 分配标记等问题。相关专项报告单独保留。

## 使用方式

设计与参数合同见 `docs/package-native-link.md`、`docs/tcc-section-gc.md`、`docs/tcc-coff.md`。静态 SDK 构建与接入见 `docs/xge3-static-packages.md`。

使用配套的新编译器，将 `XLANG_PACKAGE_PATH` 指向独立静态包根，再运行正常的 `xl build program.xl -o program.exe`。静态包和动态包使用相同导入名称，因此每次构建应明确选定包根。当前已安装的旧 DLL 尚未包含本轮能力，不应把候选编译器的验收结果套用到旧发行文件。

候选 Windows 正式 DLL 位于 `F:/xlang-validation/native-static-20261106/production-win-r3/release/win_x64/`；静态包位于同一验证根的 `xge/packages-win-r5/`。这些是本地候选产物，尚未发布。

## 验收边界

Windows 实际 PE 系统装载、静态 TLS 初始化／线程隔离及相关静态 XGE AOT 尚未封板；此前被拦截文件为 `F:/xlang-validation/native-static-20261106/coff/gate-r3/tinycc.exe`。没有改名执行被拦截映像或调整防护设置。

Windows JIT 不具备 OS 静态 TLS 的装载合同：触及这类活跃代码时 checked 拒绝，不能用动态 TLS 模拟后宣称等价。无引用的 TLS 节可以被真正裁剪。

本轮未执行图形、设备、网络或安全专项测试；CPU 接入和链接证据不能替代这些验证。当前生产发行目录保留原文件。

交付索引：`dev/release-materials/provenance/current-delivery.json`。本轮总收据：`dev/release-materials/provenance/native-static-integration-20261106/receipt.json`。
