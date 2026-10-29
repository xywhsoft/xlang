# IDE 原生 C 声明桥接查询

本增量是自动同步未保存 xlang 声明的编译器基础。编译器 xlc.dll 的公开 API xlcCBridge(ctx, outputPath) 和 CLI xl c-bridge main.xl --json 进行一次新编译单元的加载、语义验证、XLIR lowering 和真实 C ABI 头生成，随后复核同批 sourceReport 与 resolutionReport。支持现有 --source-overlay path snapshot.utf8。不会保存编辑器正文，不运行或链接程序。

cBridge.schema=1，scope=lowered-xlang-native-declarations，complete 表示声明生成和来源复核均成功。headerGuard 固定 XL6_MODULE_H；runtimePreamble 与实际 C sidecar wrapper 共用同一个常量；declarations 由同一个 xlEmitUnitHeaderToMemory/XLIR Cgen 路径生成。文本包含真实导出、类型、重载和条件宏，查询端不得自行推断 C 原型。生成声明正文最多 1 MiB，超限整批拒绝。该限额仅限制查询结果，普通 Cgen/构建合同保持。

语法、导入、语义、IR、lowering、输出生成或来源复核失败时，API 返回 0，CLI 返回非零；能够写出诊断时 cBridge.complete=false，declarations 和 runtimePreamble 均为空。不会返回上次成功单元的头。outputPath 的文件输出沿用已有诊断输出合同；输入未配置或输出路径失败可能没有可解析 JSON，调用方必须同时检查退出状态和完整结果。

结果总含 sourceReport、resolutionReport 与标准 diagnostics。加载来源的 SHA-256、物理身份、存在性候选和包环境服从原有源码报告合同。CLI .xlp 输入的根项目清单在驱动层读取，尚未纳入同次报告；使用 IDE 项目模型时仍必须单独观察实际拥有清单。显式字符串 import 的既有磁盘描述探测与覆盖冲突仍可能安全拒绝；本增量不改变解析优先级。

cInputsObserved=false 明确声明没有完整观察 C 预处理图。普通声明查询不附带 TCC 环境；可使用下述 --c-environment 增量获得有序配置和真实内嵌头。Clang 分析方须继续核验配置及头输入，不能仅凭生成原型宣称与 TCC 环境相同。原生 IDE 已消费此结果；实际输入观察和有限 ABI 对齐范围见 IDE 桥接文档。

验收工具见 [check_c_bridge.py](../tools/check_c_bridge.py)：真实未保存入口/递归依赖、失败无旧声明、原字节保留、既有 Cgen 逐字节对照、真实 C sidecar 混编和执行，以及公开 DLL API 重用上下文的刷新行为。此次使用 IDE build 下的独立编译器构建，正式发布目录二进制保持原指纹。

运行版 xl.dll 延续原有精简合同，不导出 XLC 上下文或新查询；验收单独确认这种明确的能力缺失。

## 有序配置与真实内嵌头增量

新增 opt-in --c-environment 和编译器 API xlcCBridgeWithEnvironment(ctx, outputPath, shared)。普通 c-bridge 保持原声明合同。环境查询复用 TCC 实际配置路径的同一个访问器，按调用顺序导出 options/include/define 操作、工作目录、exe/shared 输出选择和原生 C 特性模块选择；不复制一套推测参数。CLI 项目 kind 与 -shared 采用既有构建冲突合同。查询不链接或运行 C 程序。

内嵌头从当前 DLL 的真实 TCC 资源表按 include/、lib/xrt/、lib/xruntime/ 公开 .h 根枚举，使用同一个 LZMA 解包路径，导出准确虚拟名、原始长度、SHA-256 与 Base64 字节。每头最多 16 MiB、总原字节 32 MiB、最多 512 头；配置操作最多 2,048 项，环境 JSON 最多 64 MiB。失败不发布部分环境/头或旧声明；非内嵌工具链不宣称有内嵌头，可返回明确空资源表。

TCC VFS 读取优先级仍为动态覆盖、真实文件、规范虚拟名内嵌回退。新只读 builtin 读取接口只访问编译进当前模块的资源，不受同名磁盘文件或动态覆盖影响；调用方提供缓冲区，内部解包内存仍在原分配堆释放。传输字节不等于已选择预处理文件：消费者须按实际搜索次序维护磁盘候选与内嵌回退，不能直接前置所有缓存目录。

环境 complete 仅指编译器配置操作和公开内嵌资源传输完整；scope 为 compiler-configured-native-c。TCC 内建宏、生成模块预处理后的宏状态、隐式系统搜索和实际 C include 闭包尚未全部观察，C 预处理图仍明确不完整；Clang 分析与 TCC ABI/预处理等价性不由本接口保证。原生 IDE 已接入环境查询与实际头文件输入守卫；完整预处理图和任意 C ABI 等价仍不由此接口保证。

配置操作文本累计最多 1 MiB；内嵌原字节为二进制传输，不假定头文件编码或重写换行。

验收工具 [check_c_bridge_environment.py](../tools/check_c_bridge_environment.py) 使用当前隔离构建的真实资源 stamp，逐项核对解码字节与原始资源。已通过 17 次实际命令和 12 份响应，9 份成功环境各含 165 头（共 1,485 次字节/长度/摘要对照），单份原字节 13,972,580。公开 VFS 宿主验证大小查询、不足容量、缺失准确名、规范化与磁盘/动态覆盖优先级，配置超限后普通声明查询继续可用。真实 clangd 使用导出的运行时头和生成声明分析通过，实际 C/xlang 混编和运行通过。有限的配置/资源合同不构成完整 C 预处理图。


## 同批导出符号及名义类型的声明来源映射

隔离候选在 cBridge 中附带 declarationMap。schema=1，scope=lowered-public-symbol-source-bindings，records 覆盖真实 lowered export function/global 记录及公开 ABI 实际使用的命名 union。每条由实际 C ABI 头发射点产生 cName、kind、symbolId 和 headerRange；不是重新解析头文件或按源码名字匹配。普通头发射继续调用相同核心，输出字节保持原格式。

headerRange 的 startOffset/endOffset 是 declarations 原始 UTF-8 字节的半开区间，只包含实际 C 标识符，不包含 runtimePreamble。客户端把前导与声明组合成私有文件时，须依据真实前导字节数和该文件原文转换位置，不能把 C 的行列当成 xlang 的坐标。

来源关联使用当前 checked AST 的 semanticDeclaredSymbolId，与 lowered 导出记录的 symbol/storageSymbol 做准确身份连接；同名重载各自关联。target.sourceId 为同批 sourceReport.files 的一基索引；range 和 selectionRange 使用已有源码范围结构，包含 startOffset/endOffset 和一基 Unicode scalar 行列。字节位置位于可选 BOM 之后，保留查询实际正文的换行；未保存覆盖沿用原来源报告与摘要。全局变量的语义声明节点可能是名称节点，真实 range 可与 selectionRange 相同，不补造完整声明范围。

命名 union 的 kind=type 记录在真实 `typedef union <tag> { ... } <typedef>;` 的两处 C 标识符发射点产生。symbolId 使用实际 IR nominalSymbol，连接 checked union 的 semanticDeclaredSymbolId；不分配新运行时 SymbolId。额外 typeId/typeAbiId 使用同次语义事实，tag 与 typedef 的符号、类型身份、ABI 和来源完全相同。可靠目标必须具有非零 TypeId 和十六位小写十六进制非零 TypeAbiId；无类型事实时两者为零且 target=null。function/global 的原字段和记录保持。

进入公开 ABI 的嵌套 union 亦可映射；别名 canonicalize 后定位底层名义 union。未进入公开 ABI 的 union 不产生头声明或记录。class/protocol、被擦除的标量别名和 tuple/nullable/callback 等共享载体缺少唯一 C 声明身份，不能根据类型的首次来源或生成名字补造映射。union 字段尚无独立绑定。

没有可靠来源、范围未知、语义身份出现不同声明或生成节点没有加载源文件时，target 明确为 null。complete 只表示本次实际输出记录都有可用来源，不表示所有语言类型、编译器生成辅助入口或 C 预处理图已经全部映射；消费者仍需逐项验证目标、原文摘要、版本与输入守卫。声明失败时映射清空且 complete=false，不能使用旧结果。

映射最多 32,768 条、JSON 最多 8 MiB，来源关联最多 262,144 个声明事实、2,097,152 次 AST 访问和 512 层。映射超限或无法建立时可返回 complete=false、reason=source-map-unavailable、records=[]；已成功生成并复核的 ABI 声明继续返回，保持原 1 MiB 头限制。客户端须给可选映射设置相应解析预算，不能用新增元数据的失败冒充普通 ABI 成功或源码导航完成。

真实查询门禁 [check_c_bridge_map.py](../tools/check_c_bridge_map.py) 保留函数/全局、覆盖/导入、失败恢复与 32,769 条实际导出的容量回退。新增 [check_public_type_map.py](../../ide/tools/check_public_type_map.py) 验证实际名义类型、tag/typedef、嵌套/别名/导入、未保存依赖、字节与身份兼容及 C/xlang 混编运行。原生 IDE 已接入 F12 与同项目、版本、源摘要/物理身份守卫；类型范围和最新验收见 [IDE 桥接](../../ide/docs/c-xlang-bridge.md) 与 [进度](../../ide/docs/progress.md)。


## 内嵌 TCC 标量与位域目标查询

在内嵌 Win64 TCC 下，`--c-environment` 可附带 `nativeEnvironment.scalarTarget`。schema=1、scope=`tcc-win64-scalar-target`；`tinyCVersion`、`pointerBytes`、`longBytes`、`longDoubleBytes` 和 `longDoubleAlign` 从实际 TCC 构建常量及其 `type_size` 查询产生。Windows 解析器将 long double 视为 double 时，查询采用该有效源类型，不能使用内部 x87 类型的原始大小。其他工具链或目标不提供此可选对象，客户端须显式处理缺失。

可选 `nativeEnvironment.recordLayout` 的 schema=1、scope=`tcc-win64-bitfield-default`，`msBitfields` 来自按本次 exe/shared 选择和实际原生 C 配置完成初始化的 TCCState。`tcc_xlang_record_layout` 读取状态，支持 `-mms-bitfields` 与随后 `-mno-ms-bitfields` 的覆盖；空状态或空输出拒绝且不改写输出。这个布尔值描述默认位域模式，不描述 pragma、每个记录的完整布局或任意 ABI 等价。

私有 TCC 查询声明位于 `lib/tcc/libtcc.h`，实现位于 `lib/tcc/libtcc.c`。运行版 `xl.dll` 仍不新增 XLC 上下文查询接口。检验工具 [check_native_c_target.py](../tools/check_native_c_target.py) 比较真实查询与 TCC 内存编译后执行的大小、对齐、版本及三种位域配置，拒绝把硬编码的目标资料当作运行证据。

## 外部函数原型的准确来源

实际公开 ABI 头中的外部函数原型也在真实标识符发射点调用相同的来源访问器，使用 lowered call target 的实际 symbol 连接 checked extern 声明。这让 CRC32、MD5 等原生扩展的 C 函数能定位到 xlang 声明；跳过的运行时桥接入口不补造记录。普通 Cgen 使用空访问器，头文件字节格式保持一致。[check_c_bridge_map.py](../tools/check_c_bridge_map.py) 同时验证 extern 的磁盘与未保存覆盖身份、UTF-8 范围及普通头逐字节一致。

内嵌 `stdio.h` 的外部 `vsnprintf` 声明只在 `__NO_ISOCEXT` 下启用，普通模式保留原静态内联定义，避免真实原生扩展构建的外部／内部链接冲突。IDE 的 [check_c_extensions.py](../../ide/tools/check_c_extensions.py) 使用真实 CRC32、MD5、vsnprintf 函数指针、snprintf 与 C/xlang 混编执行验证这条路径。

这些增量同步到当前 DEMO6 主源码时，保留当前所有权、集合及运行时实现。DLL 响应清单补齐已存在的 SlotMap／原生叶值／数组原生值源文件，避免接口注册后缺失实现导致链接失败。独立构建与验收记录位于 IDE build 中；本次源集成不替换 DEMO6 正式发布二进制，也不自动提升 IDE 默认私有工具链。
