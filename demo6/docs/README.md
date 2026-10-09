# DEMO6 设计文档口径

本目录是 DEMO6 的唯一设计口径目录。

后续讨论、设计、重构与实现默认只参考本目录下的文档。`demo3`、`demo4`、`demo5`、`dev` 目录中的旧文档只允许作为历史事实或迁移参考，不能自动提升为 DEMO6 标准。

## 当前权威设计

- `标准库FixedStack.md`：固定逻辑容量、普通泛型实现、精确资源/可空结果合同与专项验收范围。

- `文件系统标准库实施.md`：22 个原生文件系统模块与 4 个 VFS 模块的接入范围、基础批次验收、资源合同及未完成项。

- `设计理念.md`：xlang DEMO6 的基础设计理念、第一绝对原则、语言取舍原则和代码风格约束。
- `XRT集成原则.md`：XRT 优先、禁止重复实现、XRT 缺口补全层和同步流程约束。
- `语言基础设计.md`：源码编码、标识符、注释、保留字、字面量、基础类型、变量与常量声明。
- `表达式与控制流设计.md`：基础运算符、表达式、优先级、`range`、`in`、控制流和循环。
- `函数设计.md`：函数声明、参数、命名参数、可选参数、可变参数、多返回值、func 类型、匿名函数和闭包捕获。
- `函数重载设计.md`：函数、方法、构造、native-backed class extern 方法和泛型函数的重载解析。
- `类型与隐性类设计.md`：隐性类、类型标识符三重角色、`type` 类型、透明别名和类型值比较。
- `类型转换规则.md`：普通隐式转换、安全拓宽、显式构造转换、`as` 运行时检查和 XRT 层统一转换 API。
- `类型系统设计.md`：统一堆 class、native-backed class、透明 C payload、继承、生命周期和 C opaque handle 映射。
- `对象与参数传递设计.md`：class/容器受管理引用语义、`union` 值语义、`point<T>`、`byval/byref/out`、多态和 C ABI。
- `class_object协议设计.md`：class/object 统一对象模型、协议方法、derive 能力、copy/clone、动态字段和持久化设计。
- `泛型设计.md`：泛型声明、类型参数、显式实例化、静态泛型容器、泛型约束和 C 混编限制。
- `XLIR统一中间表示设计.md`：`AST -> XLIR -> C` 的统一 IR、阶段不变量、C emitter 边界和已完成的旧后端退役约束。
- `XLIR迁移验收矩阵.md`：当前已接纳的 XLIR 语义簇、可执行质量门和单一后端准入条件。
- `本批五项能力实现口径.md`：泛型约束、字段级持久化注解、泛型 class 迭代协议、native-backed class 和 class 成员 C ABI 导出的当前实现口径。
- `ABI设计.md`：内部 ABI、C ABI、C 兼容类型、class 数据 ABI、成员导出 ABI、符号生成、类型映射和 ABI dump。
- `C混编设计.md`：C 与 xlang 互相声明、导出、生成头文件和 C ABI wrapper。
- `命令行设计.md`：CLI、四阶段编译管线、调试 dump、构建、运行和 C 混编命令。
- `Linux兼容性设计.md`：Linux x86_64 平台边界、构建产物、TCC/VFS、XRT 系统依赖和发布验收口径。
- `标准库设计.md`：标准库默认注册、短命名空间、XRT/CRT 边界、第一批标准库函数、暂缓能力和后续设计议题。
- `日志标准库接入计划.md`：已批准日志模块的公共 API、资源合同、按需组件和批次验收。
- `三组标准库接入长期任务.md`：系统/进程、文本及容器 53 模块的完整目标、逐模块状态与冻结验收证据。
- [数据恢复实例域](stdlib53-b155-restore-instance-domains.md)：静态字段配方与实际初始化子对象、共享 memo 身份和 checked 拒绝；不认证外部共享默认对象或替代完整语言切换。
- [共享默认对象的隔离合同](stdlib53-b156-restore-isolation-contracts.md)：声明 clone 能力、精确调用和错误 ABI；运行时私有性认证与 Cgen 切换仍须完成。
- [对象字段恢复事务](stdlib53-b157-restore-field-transactions.md)：真实拥有图认证后整批发布，失败保持原字段；这是声明 clone 执行与 Cgen 切换的前置能力。
- [数据恢复控制器的整图字段事务](stdlib53-b158-restore-controller-transactions.md)：整图暂存、实际源实例 pin 和共享对象 checked 拒绝；声明 clone 执行与语言入口切换仍须完成。
- [共享默认对象的声明克隆恢复](stdlib53-b159-declared-clone-restore.md)：在私有原生控制器执行声明 clone、重认证结果和整图提交；活动模块及 Cgen 实际入口继续。
- [数据恢复的宿主模块读取边界](stdlib53-b160-host-restore-read-boundary.md)：完整跟踪证书、真实入口引用和非阻塞读取能力；白盒及严格 API 语法验证通过，实际 Cgen/TCC 入口继续。
- [C 导出函数的运行时执行跟踪](stdlib53-b161-generated-export-execution.md)：明确 XLIR 执行 ABI 和 resident CodeScope；真实 TCC 调用/关闭/卸载通过，完整读取证书及恢复入口继续。
- [宿主入口和退役回调的无分配执行登记](stdlib53-b162-host-execution-activity.md)：执行活动与真实代码引用分离，关闭和 OOM 下保留清理；双端真实 TCC 与所选原生 Linux ASan/UBSan 通过，读取证书及恢复切换继续。
- [原生 C 回调的无分配执行登记](stdlib53-b163-raw-callback-execution.md)：保存回调与外线程执行使用明确 XLIR 合同，前后对照检出旧漏登记；双端真实 TCC、关闭/OOM 和相邻回归通过，代码生命仍由调用者保留。
- [动态绑定方法保活与模块读取证书](stdlib53-b164-bound-method-read-certificate.md)：绑定方法统一使用 resident closure/box；跨模块代码保活、外线程读取拒绝、双端前后对照及真实宿主证书通过，JSON/XLON 切换和打包 AOT 仍待验收。
- [数据恢复的宿主只读能力绑定](stdlib53-b165-restore-read-binding.md)：可达恢复调用按 XLIR 合同选择独立 resident 能力，真实 API 一次性绑定并封闭；双端前后对照、原生读取与所选 sanitizer 通过，目标所有权转移和实际恢复切换继续。
- [XLIR数据恢复构造图](stdlib53-b154-ir-restore-graph.md)：可达根、节点、字段上下文、默认工厂和迁移边界；与旧恢复六表共享失败原子构建，不等同于 Cgen 或语言载体切换完成。
- `文本标准库接入.md`：精确 string/char/bytes、UTF-8/16/32、BOM、数字与流式文本的公开合同。
- `XRT扩展标准库API.md`：当前已公开的扩展标准库对象、函数、常量、异步语义和 C/XRT 对照接口。
- `进程标准库异步等待.md`：Process.waitAsync 的 typed 结果、协作取消、共享源隔离、声明 ABI 与按需原生闭包。
- `进程文件重定向.md`：Command 的独立 OS 句柄、File/VFS 边界、配置回滚、声明 ABI 与按需重定向组件。
- `进程终端.md`：PTY/ConPTY 的配置、resize、原始流、独立裁剪及 Windows 原生平台资源限制。
- `数据格式与归档扩展库设计.md`：JSON/XSON、codec、hash/crypto、archive、template、encoding 等数据格式与归档扩展库设计。
- `网络服务基座补全设计.md`：XRT 网络服务基座、HTTP server 标准库、路由、静态资源、请求响应对象和 xs 静态原型支撑设计。
- `异常处理与文档注释设计.md`：异常处理、断言、高级错误模型和 `#doc ... #end` 文档注释。
- `调试器与task_async_generator设计.md`：完整调试器体系、高层 `task` API 和 `async generator<T>` 的设计口径、开发顺序和验收标准。
- `扩展库系统设计.md`：source import、package、同名 C companion、版本隔离、依赖解析和扩展库分发模型。
- `扩展库建设Spec.md`：扩展库批量建设范围、交付物、实现顺序和统一验收规范。
- `DLL嵌入式运行库设计.md`：`xl.dll` / `xlc.dll` 嵌入式 API、上下文、脚本运行、exe 编译、C 函数注册和 XRT 注入。
- `TCC内嵌与运行入口设计.md`：内嵌 `libtcc`、运行版/编译版 DLL 分层、`xlang_main(xvalue arg)` 入口 ABI、VFS 资源包和 TCC IO 边界。
- `脚本嵌入与TCC模块设计.md`：`tcc` 模块、脚本嵌入、动态 VFS、运行版/编译版能力边界和按需嵌入裁剪口径。
- `union设计.md`：C 风格 `union` 的语法、规则、代码生成、C ABI 和ABI 元数据。
- `xs2生产级重构设计.md`：XS 2.0 的 HTTP/HTTPS、TCP/UDP、动态 C/xlang handler、虚拟主机、原子 reload、生产配置、稳定性和博客系统验收口径。

## 当前工程与质量口径

- `RC0工程基线.md`：RC0 守门入口、当前基线目录、禁止回退项、XRT/CRT 边界和诊断基线。
- `RC0变更摘要.md`：RC0 变更范围、已完成能力、不属于 RC0 的范围和风险建议。
- `质量压实长期任务.md`：已完成的质量压实项、守门结果、覆盖检查命令和持续优化队列。
- `三轮问题统一修复任务.md`：dev 工具三轮问题的统一根因、修复分组、实施顺序和发布级验收账本。
- `RC三轮问题统一修复任务.md`：本轮 24 项 RC 审计问题、class 引用语义和发布收口验收账本。
- `运行时类型系统基线审计.md`：当前运行时类型系统事实基线、边界、风险点和补测范围。
- `XRT标准库完整接入任务.md`：XRT 扩展能力、专用数据结构、原始 TLS、Task/Future 的实现范围与发布验收记录。
- `XRT与xlang系统整合任务.md`：当前长期整合范围、快照契约、实施顺序和 HTTP/WebSocket 快慢路径边界。
- `扩展库开发问题清单.md`：六轮扩展库批次发现的语言/编译器/运行时问题账本（P0 崩溃类到 P4 C 库兼容），含最小复现、规避方案与修复优先级。
- `剩余未压实扩展库清单.md`：删除 22 个占位库后 109 个未压实包的处置规划视图（按编解码改造/平台收编/真外部服务/迁移队列分类）。
- `acme扩展库设计.md`：对标 acme.sh 的 ACME v2 客户端设计（JWS 密码核心 / 编排 / DNS 提供商三层），含 XRT 依赖核查、mock CA 测试策略与三期计划。
- `XRT与xlang能力矩阵.md`：从双产品快照和当前语言实现生成的模块覆盖、低层符号与扩展延期边界。
- `xruntime语言桥接契约.md`：定义 xruntime 的语言内核语义、语言级抽象和显式底层能力三层边界。
- `extlib_ecosystem_audit.generated.md`：由扩展库生态审计工具生成的包状态、实现范围和阻塞原因清单。
- `extlib_smoke.generated.md`：由扩展库 smoke 工具生成的逐包可执行验证结果。
- `featured_extlibs_rc.md`：官网重点扩展库的 RC 范围、清单与可执行门禁。
- `扩展库ABI迁移队列.md`：仍使用旧 XRT ABI 的历史扩展库迁移顺序与验收约束。
- `扩展库生态压实计划.md`：扩展库状态模型、首期范围和自动审计入口。
- `扩展库占位实现预算评估.md`：简化实现扩展库的代码预算、取舍依据和延期边界。
- `文档整理收口.md`：本轮文档整理后的索引、收口口径、剩余非阻断文档增强和验证命令。
- `文档状态矩阵.md`：逐份标明当前文档身份、使用方式、维护触发条件和冲突处理顺序。
- `文件系统接入前置基线验证.md`：文件系统接入所需的字符串 ABI 与源码基线验证；不代替当前发布验收。
- `发布级验收_2026-10-01.md`：两平台文件系统专项、字符串 ABI 修复、批量发布门结果与仍未关闭的全仓阻塞；未宣称 RC0 通过。

### XLIR 实现合同与审计附录

以下文档是统一 XLIR 当前实现的可核验合同和阶段审计证据。它们补充上层设计，
不另行定义语言语义；发生冲突时仍以代码、主回归、质量门和当前权威设计为准。

- 核心产物与执行边界：`XLIR独立产物依赖合同.md`、`XLIR对象拥有图接入评估.md`、`XLIR恢复执行与派发合同.md`、`XLIR批次收敛流程.md`。
- 批次与视图所有权：`XLIR多视图发送拥有关系.md`、`XLIR连接候选拥有关系.md`、`XLIR完成事件拥有权交接.md`、`XLIR引用批次拥有关系.md`。
- 原生传输与缓冲：`XLIR原生传输操作拥有权.md`、`XLIR原生传输调度拥有链.md`、`XLIR原生传输缓冲与冻结边界.md`、`XLIR原生缓冲拥有链.md`、`XLIR原生Transport壳生命周期.md`。
- 原生端口、发送与父对象：`XLIR原生端口拥有链.md`、`XLIR原生发送与Accept拥有链.md`、`XLIR原生UDP发送拥有链.md`、`XLIRTCP父对象拥有图.md`、`XLIRUDP父对象拥有图.md`。
- 原生网络对象与服务退休：`XLIR原生Dial拥有链.md`、`XLIR原生Engine端口拥有图.md`、`XLIR原生Engine拥有链.md`、`XLIR原生Socket拥有链.md`、`XLIR原生Watch拥有链.md`、`XLIR原生服务退休合同.md`。
- Future、resolver 与系统请求：`XLIR原生Future映射合同.md`、`XLIR原生Future拥有关系.md`、`XLIR原生Resolver拥有图合同.md`、`XLIR原生ResolverFuture拥有链.md`、`XLIR原生OS请求拥有链.md`。
- 回调、嵌入与任务：`XLIR原生回调收集.md`、`XLIR原生回调拥有链.md`、`XLIR原生嵌入命令拥有链.md`、`XLIR原生网络任务拥有链.md`。

## 历史任务单

以下文件记录阶段性任务和立项背景。它们可以帮助理解实现来路，但若与“当前权威设计”冲突，以当前权威设计和当前代码为准。

- `第二阶段功能清单.md`：第二阶段基础语义夯实目标、XRT 改造边界、xvalue/type desc/type ops、静态泛型容器和 xcall 设计。
- `本轮开发任务_import_混编.md`：source import、package、零配置 C 混编、main 入口、`xlInit`/`xlUnit` 生命周期的成熟化任务。
- `本轮工程骨架优化任务.md`：CompileUnit、ABI 元数据 内部模型、独立临时目录和 package resolver 地基任务。
- `工程收口10项任务.md`：canonical type、ABI 元数据 权威化、package/import、callable descriptor、后端接口和生命周期强化任务。
- `RC0稳定化与import闭环.md`：旧 import 闭环阶段记录；当前实现已切换为 source import/package。
- `RC0工作区清单.md`：RC0 工作区规模、变更分组和提交前建议的历史快照。
- `现代类型语义与HIR改造.md`：统一 XLIR 建立前的 TypeId/SymbolId、nullable/weak、protocol、enum 与 match 设计背景；现行 IR 边界以 `XLIR统一中间表示设计.md` 为准。
- `并发异步长期实现任务.md`：async/await、thread、channel、同步原语和 generator 的长期实现任务边界。
- `扩展库系统RC实现任务.md`：扩展库系统进入 RC 实现阶段的任务边界和验收口径。

## C 发射器合同

`../cgenrule/README.md` 是当前 C emitter 的边界入口。旧 AST Cgen 的编号规则表、
验证矩阵和函数索引已随实现退役；现行语义合同统一由 XLIR 设计、verifier、专项质量门
和可执行测试维护，不再保留一套按源码语法分支解释 C 输出的平行文档体系。

## 范例文档

范例目录中的 README 不作为语言设计来源，但属于当前可用功能的说明入口：

- `../release/examples/README.md`：范例目录总入口、根目录范例和标准库范例索引。
- `../release/examples/tutorial/README.md`：教学范例阅读顺序和运行入口。
- `../release/examples/feature_suite/README.md`：功能范例套件索引、宿主文件和回归入口。
- `../release/examples/apps/README.md`：复合应用范例索引和运行入口。

## 测试资产文档

以下 README 只描述测试资产，不作为语言设计来源：

- `../release/extlibs/README.md`：package/import 发布扩展库目录索引和验证入口。
- `../test/README.md`：测试目录入口、用例前缀索引和测试资产守门脚本说明。

## 工程目录文档

以下 README 只描述工程目录职责，不作为语言设计来源：

- `../README.md`：DEMO6 根目录入口、常用命令和维护原则。
- `../src/README.md`：源码分片职责、生成文件和源码维护规则。
- `../tools/README.md`：工具链入口、守门脚本索引和 RC0 入口。
- `../release/include/README.md`：公开 C API 头文件说明。
- `../lib/README.md`：XRT 单头文件、libprobe fixture 和依赖维护规则。
- `../release/README.md`：构建产物目录说明。
- `../release/projects/xs_static/README.md`：静态 xs 原型项目说明和 smoke 入口。

## 维护规则

- `Process配置与运行设计.md`：精确 OS 输入、owned Command/Options、bytes 结果与声明式执行边界；B28b 增补 typed pipeline、独立 stage/status、配置快照与共享期限。live/Future 另有专项；文件重定向、PTY 和总验收不冒称完成。

- 新设计先落到本目录，再进入实现。
- 修改 C emitter 行为时，同步维护 `cgenrule/README.md`、XLIR 设计/验收文档和对应可执行质量门；不得恢复按源码语法维护的第二套规则表。
- 修改标准库命名、参数默认值或 XRT/CRT 边界时，同步维护 `标准库设计.md`。
- 修改 import/package 行为时，同步维护 `扩展库系统设计.md`、`C混编设计.md`、XLIR dependency plan 合同和对应质量门。
- 历史任务单中的旧状态不能作为当前实现结论直接引用，必须回查当前权威设计或代码。

模块产品声明、闭包 cell 和 `println` 原始错误传播的 B40 验收范围见
`三组标准库接入长期任务.md`；对应跨平台门为
`../tools/check_xlir_module_product_globals.ps1`/`.sh`。专项通过不等于
三个分类全部 53 模块已封板。

Buffer Hex/Base64 与 static/free native factory 的 B41 声明式 ABI 和
失败清理范围见 `三组标准库接入长期任务.md`；跨平台专项门为
`../tools/check_stdlib_buffer_codecs.ps1`/`.sh`，不涵盖旧 Buffer core
的剩余迁移，也不等于全部容器或 53 模块完成。

B42 公共 Buffer 核心的直接 ABI、查询裁剪、u8 索引和失败事务见
`标准库设计.md` 与 `三组标准库接入长期任务.md`；对应专项门为
`../tools/check_stdlib_buffer_core.ps1`/`.sh`。旧 file/network 转换
帮助函数仍是独立待迁移范围，不能混入本批封板结论。

B43 Stack 原生存储前置批次见 `标准库设计.md` 与
`三组标准库接入长期任务.md`；对应冻结专项门为
`../tools/check_stdlib_stack_native.ps1`/`.sh`。
仅移除旧生成 vector 并复用 typed stack；公开 owned-return、char/bytes、
batch 和强引用拥有图仍待完成，不能视为 Stack 或全部 53 模块已封板。
