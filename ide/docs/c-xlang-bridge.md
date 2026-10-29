# 原生 C / xlang 自动声明桥接

2026-10-11 当前扩展库交付：SDK67，84 模块／106 产物重新严格构建，含渲染的完整回归和十项实际便携门通过。默认编译器修正 stdio 外部／内联声明冲突，并为真实扩展函数 C 原型提供可验证的 xlang 来源映射。CRC32、MD5 的实际混编、stdio 格式化、诊断、悬停、补全、F12 和间接头变化撤回已通过开发与包内检查；最终构建的 17 项原生界面断言和三张截图、九项编译器兼容、41 项源码材料及原有 74 项聚合／37 项标量对照通过。 详见[扩展库分析与导航](c-native-extensions.md)与[当前候选](../CURRENT-CANDIDATE.md)。下文保留此前阶段的数量和验收记录。

2026-10-11 当前 C 聚合与位域交付：SDK67，84 模块／105 产物重新严格构建，含渲染完整回归及八项实际便携门通过。默认 C/xlang 桥接新增实际 TCC 位域布局查询；开发与随包提供者各通过 23 类聚合、74 项尺寸／对齐／偏移对照，22 项元数据和用户参数优先级检查、三个实际 TCC 状态执行、九项编译器兼容检查及 41 项源码材料检查通过。原有双语法、定版布局、三层管理、编辑与构建运行保持可用，C 输入观察继续启用。 来源、适用范围和失败记录见[C 聚合与位域布局](c-record-layout.md)，启动入口及本轮精确构建见[当前候选](../CURRENT-CANDIDATE.md)。下文的既有数量和候选链接保留为相应历史阶段记录。

在拥有 `.xl` 入口的项目中，C 分析和补全会自动查询当前编译器的真实 ABI 声明。先选择支持 `c-bridge --json --c-environment` 的 xlang 编译器和已经验证的 clangd 23.1.0；打开项目内的 C 文件即可。打开的命名 `.xl` 草稿参加查询，无需保存或手动填写生成头。其他 C 分析配置仍可叠加。独立 C 文件沿用独立配置。

本轮应用源码位于 `services/c_bridge.c`，共同输入捕获位于 `ui/c_analysis_inputs.c`。编译器接口见 [同批次查询合同](../../docs/IDE原生C声明桥接接口.md)，整体服务见 [C 语义服务](c-language-service.md)。完整 IDE 仍在实施，最新验收以 [实施进度](progress.md) 为准。

2026-10-10 增量：支持编译器可选的真实 Windows x86-64 标量目标查询，对齐 long double 大小/对齐和有限的 TCC/Clang/GNU 身份宏。独立 C 文件及旧环境接口保留原有配置；完整预处理和 ABI 等价标记仍为 false。验收范围与失败副本见 [C 原生标量目标](c-native-scalars.md)。

完整标准回归与九套编译器检查现已重新执行，新便携候选默认包含标量查询编译器；开发和包内提供者的实际对照均通过。范围和证据见 [完整回归与便携默认工具链](c-native-scalars.md#完整回归与便携默认工具链)。

## 输入与执行

- UI 复制全部打开命名 C/header 和 `.xl` 的正文、稳定 ID、版本、路径、显示语法与外部状态，另复制项目导出、所选拥有项目、活动配置、工具链和服务器。读取磁盘、编译器查询、解码、文件摘要、服务器启动和缓存回收均在后台执行。
- 每个 C 编译条目只选择一个项目上下文；当前所选拥有项目优先，缺少该选择的多重归属拒绝。不同项目的声明分别强制包含，避免相同 `XL6_MODULE_H` 保护符相互覆盖。C 的独立配置不被强制分配给 xlang 项目。
- 查询使用模型中的 `.xl` 入口、活动 include 目录、真实 C 源列表和 exe/shared 类型，不通过重读旧清单覆盖未保存的工程模型。实际入口、依赖、导入候选、包路径及编译器摘要沿用来源报告的后台复核。
- 接口成功后核对 schema、scope、complete、输出类型和工作目录；声明必须为真实 lowering 生成的 ABI 头，前导和保护符必须符合编译器合同。不得推断导出原型或保留失败前的旧头。
- 165 个内嵌公开头保留原始字节，每个都核对规范名称、长度、Base64 和 SHA-256。同一编译器的多个项目共享一份缓存，后续查询逐项复核同包字节。
- LLVM VFS 将准确规范路径映射至私有缓存，使用 `redirecting-with: fallback` 和 `use-external-names: false`，保留真实磁盘文件优先级与虚拟目录的相对包含行为。TCC 配置目录和定义仍按真实操作顺序写入命令；用户分析宏、系统头和强制头继续保留。

每份私有文件通过 `CREATE_NEW` 和删除时关闭的句柄持有；服务器停止后才释放文件，随后仅删除自己记录的空目录。没有递归删除用户目录、覆盖源头文件或将临时 ABI 头写入项目。普通进程输出上限仍为 16 MiB；桥接采用显式 80 MiB 有界接口，单次编译器查询限时 30 秒。传输读取在短突发中让出线程，空闲仍等待，避免 Windows 定时器粒度节流大响应。

## 结果失效与恢复

C 和 xlang 正文/身份/路径/语法、工程配置/选择、工具链或服务器发生变化，界面撤下整批旧诊断、悬停、大纲和候选，再异步重新同步。补全提交再次校验全部输入；旧 Enter/Tab 无法应用此前草稿的声明。后台持续观察编译器报告中的来源、候选、包环境、显式 C 输入、清单、缓存，以及每个内嵌头规范位置上的磁盘存在性/摘要/身份。

观察周期为一秒加扫描耗时，不能保证磁盘变更瞬间失效。桥接生成失败不发布旧声明，自动诊断按 1.5 / 3 / 6 / 10 秒上限退避，修复后重新生成。取消停止自动行为；显式检查可重新启动。编译器没有声明新接口时显示能力提示，继续使用原有显式 C 配置；不会冒充已完成自动桥接。

## 补全与导航

强制头诊断保持完整解析，避免 clangd 预编译头重放命令宏影响原有宏诊断。独立补全会话启用预编译解析并等待真实 Sema。在空前导 C 文档中，clangd 对强制头全局符号的索引不足，因此加入一个**真实原文的私有生成头**和相同项目的编译条目，等待该头的当前版本解析事件后再查询。用户正文、UTF-16 坐标和 wire JSON 没有添加前缀或改写；没有由 IDE 拼造候选。64 个用户文档上限保留，补全会话只额外允许一份有界私有头，整体发送预算继续核验。

C 编辑器中的 F12 和“转到定义”支持导出的 xlang 函数、全局变量及公开 ABI 实际使用的命名 union。C 的 union 标签和 typedef 名称均定位同一个 xlang 声明。编译器必须同时提供准确的 `declarationMap`；没有该可选字段的旧环境接口继续提供 ABI 分析与补全。正常已观察 C/header 定义导航继续可用。

后台保留同次查询的真实生成头、正文前导长度、查询项目的 C 文件归属及验证后的源码绑定。只处理实际 clangd 回复的首个 Location/LocationLink，要求 URI 对应本批次私有头、查询 C 文件属于同一项目，且 UTF-16 区间准确覆盖该头中的 C 标识符。原始 wire JSON 保留，源码目标另置于事件的本地 `sourceDefinition`；不会按相同导出名或显示名称猜测目标。

源码区间根据来源报告的 sourceId 与准确 UTF-8 正文校验，在每个源文件的一次有序扫描中转换成 LSP UTF-16 坐标。BOM、CRLF、中文和非 BMP 字符沿用原始来源合同；编辑器内部 LF 规范化后再定位。已打开目标必须是同批捕获的 xlang 文档，版本、稳定 ID、显示语法、正文长度及 SHA-256 均一致。闭合目标仅允许本会话已观察的磁盘 `.xl`，用户调用时复核最多 8 MiB 的正文和物理身份，打开后的基线再次核对。命名恢复草稿可以作为真实 overlay 目标，不会因此创建源文件。已打开却未绑定到此次输入的文档不会被磁盘目标替换。

声明映射和转换后的绑定各限 8 MiB、32,768 条；桥接专用 JSON 解析限 80 MiB 输入、24 MiB 单字符串、2,097,152 个值、32,768 个容器成员和 32 层深度。整个后台会话的导航注册表限 32 MiB。缺少、超限或无效的可选映射会撤下相应导航，已验证的 ABI 仍可使用；完整性为 false 时仅保留有准确来源的记录。没有绑定的私有头不会作为可编辑磁盘文件打开。

命名 union 的 `kind=type` 记录来自真实类型头发射点。tag 与 typedef 共用实际 nominalSymbol、同次 TypeId、非零 TypeAbiId 和源码目标，客户端拒绝同一符号的不同类型身份或范围。别名若已被 lowering 合并为底层 union，定位底层声明。没有进入公开 ABI 的类型没有头声明与绑定。class/protocol、标量别名及 tuple/nullable/callback 等共享或结构化载体没有唯一的名义 C 类型身份，不按生成名字或结构类型的首次来源猜测 xlang 声明。union 字段尚不映射。

## 验收与限制

`tests/c_bridge_test.c` 调用真实固定编译器和官方 clangd：未保存的 `c_double` 声明、另一个项目的 `c_int` 声明、调用者修改后的独立副本、真实磁盘头优先、规范候选出现和源码变化失效、坏草稿清空、旧编译器明确回退、重复归属与取消。`tests/c_bridge_view_test.c` 使用真实 XGE/XUI 和独立 Windows 桌面：C 静态断言、真实 hover/completion、修改 xlang 草稿后的自动撤销/恢复、过期 Enter 拒绝、语言绑定、取消及原始源字节保护。浅色/深色截图为实际渲染。

`tests/c_bridge_map_test.c` 使用实际 4,097 条导出验证原生大映射解析、Unicode/BOM/CRLF 坐标、无效区间拒绝与可选接口降级。`tests/c_bridge_definition_view_test.c` 使用真实编译器、clangd 与原生 F12，验证未保存函数/全局/重载、两个项目的同名 C 导出、闭合导入文件、过期版本和磁盘变更拒绝，以及没有物理文件的命名恢复草稿和语言绑定。后端另用真实定义回复检验错误项目、错误标识符区间及等价 LocationLink。

`tools/check_public_type_map.py` 验证实际嵌套/别名/导入 union 的声明身份、两处精确 C 标识符区间、BOM/CRLF/非 BMP 来源与未保存依赖，并用查询返回的真实 C 名称混编运行。可选 `--baseline` 比较语义索引、原函数/全局映射及普通头字节。两个原生测试的 `public-types` 模式验证类型身份冲突拒绝、两个项目的同名类型、未保存目标与闭合类型源码变更守卫；C 名称均由实际编译器返回。

```powershell
.\tools\build.ps1 -Configuration Release -Tests
.\tools\test.ps1 -Configuration Release -SkipBuild `
  -CompilerPath <旧兼容性验收编译器的绝对路径> `
  -CompletionCompilerPath <补全验收编译器的绝对路径> `
  -CServerPath <clangd 的绝对路径> `
  -BridgeCompilerPath <支持环境查询的编译器绝对路径> `
  -MappingCompilerPath <支持 declarationMap 的编译器绝对路径> `
  -PublicTypeCompilerPath <支持名义 union 来源映射的编译器绝对路径>
```

未提供 BridgeCompilerPath、MappingCompilerPath 或 PublicTypeCompilerPath 时，分别明确省略对应门禁；没有对应省略标志的成功日志才算真实桥接或相应映射验收。

环境 `complete` 只覆盖配置操作与准确资源传输；`cPreprocessorGraphComplete` 和 `abiEquivalenceGuaranteed` 仍为 false。C 间接 include、目录搜索候选、响应文件和系统输入尚未形成完整预处理闭包。TCC 内建宏、clang/TCC 解析差异及预编译补全的宏重放边界不能据此宣称等价。复杂工程的开销、ABI/宏矩阵、更多跨语言导航、实体输入/DPI、正式依赖与许可证、干净安装和完整首版交付继续实施。

上游格式与解析依据：[LLVM VFS](https://llvm.org/doxygen/classllvm_1_1vfs_1_1RedirectingFileSystem.html)、[clangd 补全选项](https://github.com/llvm/llvm-project/blob/main/clang-tools-extra/clangd/tool/ClangdMain.cpp)、[真实补全解析流程](https://github.com/llvm/llvm-project/blob/main/clang-tools-extra/clangd/CodeComplete.cpp)。上述规则另由固定版本的实际服务验证，不外推到任意服务器。

## Class 标量字段与混编

IDE 使用的隔离编译器已补齐 class 中 C 原生标量和 float32 的默认零值与精确字段读取。字段布局生成和校验接受相同标量集合；原生标量读取使用已有的精确类型投影，默认构造使用已有的 foreign-scalar 装箱并记录字段 TypeId。float32 使用已有 binary64 载体及 binary32 投影。未新增 IR/API 字段、运行时符号或 C 声明形式。

`tools/check_class_scalar_abi.py` 验证 20 种 C 标量及 float32 的字段桥接，实际检查默认值、赋值回读、继承和泛型，并由 C 调用 xlang 创建、验证与释放 class 对象。查询头与普通头在相同模块名下逐字节相同；BOM/CRLF 与未保存 Unicode 草稿保留原 ABI。可选 `--baseline` 保存旧编译器失败对照。`tools/test.ps1 -ClassScalarCompilerPath <编译器绝对路径>` 将此检查接入完整回归。

根编译器的 `test/cases/xlir_class_scalar_defaults_contract.c` 验证 23 个标量字段、一份 managed string 及 70 次非法初始化信息拒绝。验证程序链接真实补全模块；测试不使用补全桩。正常 class 对象依然以 `xvalue*` 对外传递，没有唯一物理 C 名义声明的 class/protocol 不生成类型跳转。

这组检查针对 Windows x64 的上述字段与调用路径；它不代表任意平台、宏环境或全部 class ABI 均已完成正式发布验收。实际修复、原生任务和便携包状态见 [实施进度](progress.md)。


2026-10-11 默认 C 输入观察交付：默认便携 C 后端已加入实际输入观察，未打开的间接头文件变化会撤回旧语义结果并重新分析；目录守卫支持超过 MAX_PATH 的 Unicode 深目录。SDK67 沿用已严格构建的匹配材料，应用重新严格构建为 84 模块／104 产物，含渲染的完整回归及六项实际便携门通过。真实输入观察 113 项接口、21 项界面及 52 项导航上下文检查，69 项短／长目录观察检查和 13 项材料拒绝检查通过；普通 clangd 的五项兼容门和修正后草稿启动门也通过。 提供者为此前成功源码构建的精确复用，本轮没有重新编译 LLVM。材料范围为适配差异和既有构建记录，完整上游源码按固定提交获取；正式发行核对仍继续。详见[默认 C 输入观察](c-input-trace-delivery.md)和[当前候选](../CURRENT-CANDIDATE.md)。
