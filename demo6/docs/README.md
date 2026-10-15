# DEMO6 设计文档口径

本目录是 DEMO6 的唯一设计口径目录。

后续讨论、设计、重构与实现默认只参考本目录下的文档。`demo3`、`demo4`、`demo5`、`dev` 目录中的旧文档只允许作为历史事实或迁移参考，不能自动提升为 DEMO6 标准。

## 当前权威设计

- [Process 派生 IO 的拥有图](stdlib53-b312-process-io-graph.md)：唯一流上下文的真实拥有边、Reader 转移与流租约的可重试完成。
- [Process 共享拥有图](stdlib53-b311-process-owning-graph.md)：真实后台引用、共享终态 Future、消费者独立取消，以及冻结外的管道退役。
- [IO 的驻留实现和文件流回调](stdlib53-b309-resident-io.md)：51 个适配入口迁出 Cgen，内建流拥有策略、独立组件和错误回滚；进程派生流的拥有边见上文专项。

- [VFS 的驻留适配与资源清理](stdlib53-b308-resident-vfs.md)：私有 namespace、内建后端与返回对象清理，移除 VFS 和 Pack emitter，IO 与任务组继续审计。

- [取消选项和基础资源的驻留清理](stdlib53-b307-cancel-rng-file-owners.md)：取消令牌真实拥有边、Random 和 File 析构驻留，另六项资源继续审计。

- [原生资源的精确拥有边与销毁](stdlib53-b306-native-resource-graphs.md)：22 项析构策略、真实内部拥有边、并发关闭和独立 AOT 验收，另九项资源继续审计。

- [任务池空闲线程的拥有图准入](stdlib53-b305-task-pool-idle.md)：clear 续体 AOT 退役失败根因、确定性回归与核心修复的精确同步范围。

- [原生 Context 和 Buffer 的驻留拥有合同](stdlib53-b304-xlang-context-policy.md)：资源布局审计、声明式策略、显式宿主 ABI 与可达入口的批量清单。

- [独立程序入口与对象图退役](stdlib53-b303-static-entry.md)：声明化部署合同、共同原生驱动、独立读取准入及双端真实源码 AOT。

- [对象图退役的精确策略目录](stdlib53-b302-ownership-policy-catalog.md)：独立能力声明、JIT 共用解析器和按需部署元数据；入口接线见上一项。

- [独立模块拥有记录与可重试退役底座](stdlib53-b301-static-module-owner.md)：共同原生驱动、真实 lease 与可恢复清理；独立入口进展见当前入口合同。

- [独立程序循环对象图退役缺陷与验收门](stdlib53-b300-aot-graph-retirement.md)：双端真实 AOT 的核心生命周期失败、无环对照及驻留协议修复要求。

- [当前产品构建与安全数值转换验收](stdlib53-b299-current-products.md)：新 CLI／DLL／SO、完整产品清单及十个数值用例的独立 AOT。

- [专用宿主验收与终端尺寸观察](stdlib53-b298-hosted-source-contracts.md)：六项完整模式矩阵、精确证据接线及真实尺寸事件回归。

- [语言数值构造的精确文本与反射边界](stdlib53-b297-language-scalar-contracts.md)：完整字节长度、驻留解析、反射拒绝和明确状态输出。

- [动态方法默认值的 IR 执行合同](stdlib53-b288-runtime-method-defaults.md)：调用时求值、接收者与前序参数借入、拥有结果及统一失败边界。

- [动态函数默认参数的 IR 执行合同](stdlib53-b289-runtime-callable-defaults.md)：普通 xcall、匿名闭包捕获、前序参数私有 cell 和显式实参先校验。

- [动态调用的函数参数合同](stdlib53-b290-runtime-callable-func.md)：typed／nullable 函数参数、完整签名、子桥接方向和拥有清理。

- [可变参数展开与可空方法回调的 IR 合同](stdlib53-b291-variadic-nullable-functions.md)：位置／关键字帧快照、提供方裁剪、精确签名与独立拥有清理。

- [动态调用的 Future 合同](stdlib53-b292-runtime-futures.md)：公开异步签名、精确身份投影、可空载体与成功／失败拥有清理。

- [动态方法的精确标量与绑定签名合同](stdlib53-b293-method-scalars.md)：共享标量 codec、可空载体、原生纯匹配及单声明绑定身份。
- [C 标量显式转换与精确投影](stdlib53-b294-native-scalar-conversion.md)：静态／动态安全构造、checked IR、独立纯读取供给方与精确 nullable 投影。
- [动态数值构造的反射身份边界](stdlib53-b295-numeric-reflection-boundaries.md)：规范类型表、反射 UInt 与数值区分、V2 原生入口及输出事务。
- [泛型 C 数值构造与原生字段合同](stdlib53-b296-generic-scalar-contracts.md)：待实例化合同、具体重新验证、共用零值／装箱与整批执行验收。

- [整数转换默认值的统一 IR 合同](stdlib53-b287-integer-defaults.md)：先登记身份再接纳合同，精确常量默认值与物理桥接输出顺序。

- [动态方法的精确回调合同](stdlib53-b286-dynamic-callables.md)：完整签名选择、checked closure 原生合同与投影 owner 清理。

- [泛型依赖闭包与二进制默认初始化](stdlib53-b285-generic-graph.md)：晚登记的具体方法统一收集依赖，空 bytes 与 null 分离。

- [动态调用的二进制默认值](stdlib53-b284-bytes-defaults.md)：显式长度常量、借入默认值临时 owner 与通用调用清理。

- [资源引用投影与 float32 无损转换](stdlib53-b283-resource-domains.md)：完整元数据见证、同一资源身份、无损读取与 checked 失败边界。

- [原生容器的统一动态投影](stdlib53-b282-container-projection.md)：六类容器的完整策略、共享 checked 事务、独立提供方及实际裁剪产物验收。

- [Array 与 Stack 的动态类型投影](stdlib53-b281-sequence-projection.md)：完整原生策略校验、同一对象引用、nullable 错配和 checked 拥有事务。

- [驻留导出归属与源码构建校验](stdlib53-b280-resident-registration.md)：精确 Stack／Sequence 归属、实际提供方及源码消费快照绑定。

- [动态标量装箱与 float32 显式转换](stdlib53-b279-scalar-conversion.md)：明确缩窄与无损读取的边界，独立成功状态和按需驻留组件。

- [Nullable callable 的存在性与载荷合同](stdlib53-b278-nullable-callable.md)：声明化 null 表示、基础载荷编解码、参数副本与专项验收边界。

- [动态方法多返回值与 Stack 安全接收](stdlib53-b277-dynamic-method-products.md)：规范返回 product、完整绑定向量、原生 Stack 角色检查及双平台专项验收。

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
- [封闭类 JSON/XLON 恢复切换](stdlib53-b166-native-restore-cutover.md)：独立目标拥有值消费、声明 clone 隔离与整图提交；旧 opaque 模块明确待迁移，三个裁剪产物执行通过但完整导出审计仍失败。
- [驻留组件导出与依赖验收](stdlib53-b167-resident-catalog.md)：修复 B166 的 60 项审计差异；双端完整符号目录与负控、Windows 裁剪 JIT/AOT 通过，生成 C 不变，不代表发布总门完成。
- [DynamicFields 驻留 ABI 与语言拥有图](stdlib53-b169-dynamic-fields-resident.md)：精确字段长度、声明化错误合同、可裁剪复制和宿主准入；双端专项通过，Open 恢复事务继续。
- [List 快照迭代与 checked 载体边界](stdlib53-b175-list-iterator-language.md)：原生快照进入 foreach、精确驻留创建和细化 Array 修正；双端 C/JIT 与故障批处理通过，整体载体切换仍待完成。
- [类型细化后的容器键与 List 动态调用](stdlib53-b176-refined-list-consumers.md)：细化 List/Array/Dict 的精确键合同和原生 List 动态方法；源工厂、索引、转换与恢复仍待联动迁移。
- [List 动态索引与容器查询](stdlib53-b177-list-index-consumers.md)：原生 List 的开放索引和 in 查询、独立驻留组件及纯 Array/Dict/Set 裁剪产物运行；源工厂和静态索引尚未整体迁移。
- [List 静态索引的原生合同](stdlib53-b178-list-static-index.md)：精确标量/文本及拥有投影读写、checked 输入消费、真实原生语言输入与独立裁剪产物；工厂、转换和恢复继续。
- [List 浅复制与代数操作](stdlib53-b179-list-copy-algebra.md)：独立 typed 存储、右侧覆盖合并、缺键删减及明确 NULL_RESULT 合同；生成 C 不再实现 List 算法，工厂与深复制仍待切换。
- [开放复制与 List 递归克隆](stdlib53-b180-open-copy-list-clone.md)：开放 var 存储隔离、共享图 memo、XLIR 提供者闭包及双端裁剪执行；工厂、准入与恢复继续。
- [动态字段复制与恢复隔离的 List 接入](stdlib53-b181-dynamic-fields-list-copy.md)：七个内部消费端共享图 memo、声明 clone 隔离、发布后清理错误及双端完整故障前缀；源工厂与 wire codec 继续。
- [List 的 JSON/XLON 投影与恢复](stdlib53-b182-list-persistence-native.md)：紧凑稳定键投影、原生恢复、作用域创建失败和最小产物依赖；普通源工厂及 cast 继续迁移。
- [List 稀疏输入与共享导入事务](stdlib53-b183-list-language-input.md)：语言输入保留完整键、真实槽位快照与首次失败锁定；普通源工厂及 cast 的前置协议。
- [List 原生语言构造与稀疏导入](stdlib53-b184-list-source-native.md)：普通字面量、空默认值及目标类型边界直接使用原生 List，完整 IR 合同与嵌套 ANY Array 转换；嵌套 List recipe 和发布总验收继续。
- [List 与 Array 混合配方](stdlib53-b185-list-mixed-recipes.md)：闭合嵌套 List、Array/List 交错链、原生 ANY List 隔离及 JSON/XLON 恢复；Cgen 只消费统一配方，Set/Dict 和发布总验收继续。
- [Set 显式键策略底座](stdlib53-b186-set-key-policies.md)：相等与哈希独立于排序，驻留引用/Future 策略、失败边界和精确 typed_set 快照同步；语言 Set/Dict 与发布总验收继续。
- [Set 原生存储与稳定引用键](stdlib53-b187-native-set.md)：真实 typed Set owner、完整逻辑域、集合运算、快照与分阶段退役；可变引用按身份作为键，语言 Set/Dict 和发布总验收继续。
- [Set 驻留值转换与方法合同](stdlib53-b188-set-value-methods.md)：完整域投影、boxed 方法与快照步骤，取走前验证及无诊断失败；语言入口和递归配方继续。
- [List 与 Set 的统一值域配方](stdlib53-b189-value-collection-recipes.md)：共享完整值域、独立 Set 键合同、封闭目标构造与拒绝验证；不启用语言 Set 入口。
- [Set 的递归引用类型域](stdlib53-b190-set-reference-domains.md)：完整子域、混合深度、可选驻留 schema 与语言 Array/Stack 路径；Set 源工厂和持久化继续。
- [Set 原生消费者与按需遍历](stdlib53-b191-set-consumers.md)：通用消费者、静态和 erased foreach、混合图复制与序列化输出；Set 工厂、导入和恢复继续。
- [Set 语言入口与原生载体迁移](stdlib53-b192-set-source.md)：原生工厂、静态方法、代数与独立错误合同、checked 导入和受支持恢复；不关闭 Dict 或发布总验收。
- [Dict 原生存储与生命周期底座](stdlib53-b193-native-dict.md)：精确字符串键、完整值域、独立 typed 快照、真实拥有边与 checked 退役；语言 Dict 工厂、方法、导入和恢复仍待迁移。
- [Dict 驻留投影与动态值边界](stdlib53-b194-dict-projection.md)：不可变驻留转换、动态方法、拥有键快照和显式单层导入导出；语言配方、图消费者和恢复仍待迁移。
- [Dict 通用消费者与宿主拥有图接入](stdlib53-b195-dict-consumers.md)：原生遍历、共享图复制与投影、通用动态方法及实际宿主解析器；语言 Dict 工厂与恢复继续迁移。
- [Dict 统一构造配方与嵌套准入](stdlib53-b196-dict-recipes.md)：完整值域、精确文本键、封闭原生工厂和混合子域；不授予旧 wire Dict 生产者权限。
- [Dict 动态索引与拥有边界](stdlib53-b197-dict-index.md)：原生 Dict 接通生成代码的动态索引、精确键和 checked 拥有合同；静态工厂、直接槽和恢复继续。
- [Dict 静态索引与精确键合同](stdlib53-b198-dict-exact-index.md)：静态键、计算键和赋值统一驻留 owned／checked 索引，拒绝旧 literal 指令；原生工厂、直接槽和恢复继续。
- [Dict 原生语言工厂与驻留方法](stdlib53-b199-dict-source.md)：字面量、默认值、命名参数包与方法／代数迁移原生所有者；直接槽、递归导入、恢复和发布总门继续。
- [Dict 精确键递归导入与原生恢复](stdlib53-b200-dict-import.md)：共享事务接通 Dict 原生文本键导入／恢复；语言 IR 配方及发布总门仍待接入。
- [Dict 统一 IR 导入与原生持久化](stdlib53-b201-dict-language-import.md)：语言消费导入、封闭原生恢复与按需图投影接通；直接槽、深复制完整性与发布总门继续。
- [统一图深复制合同与恢复隔离](stdlib53-b202-graph-clone.md)：完整 IR 提供方计划与单一图引擎通过；混合全局读边界、直接字段迭代及发布总验收继续。
- [模块图读取能力与字段集合迭代](stdlib53-b203-module-collections.md)：混合全局恢复和直接字段迭代缺口关闭；IR 数据能力、生命周期及裁剪专项通过，发布总门继续。
- [Dict 直接槽读写合同](stdlib53-b204-dict-direct-slot.md)：封闭物理槽按统一 IR 直接访问，擦除和不同表示保留转换；双端 checked、故障前缀及裁剪专项通过，发布总门继续。
- [SlotMap 原生拥有权与实例键](stdlib53-b205-slot-map-native.md)：复用槽表代际，精确 typed 元素、独立键记录与可重试退役；双端原生专项及相邻回归通过，语言接线继续。
- [SlotMap 键的动态值投影](stdlib53-b206-slot-map-key-value.md)：驻留 converter、相等和内容键能力独立裁剪；双端原生专项通过，语言直接 ABI 与 IR 接线继续。
- [拥有型原生值的容器传输](stdlib53-b207-native-value.md)：驻留记录传输、失败原子复制与封闭键 Set 内容策略，冻结双端原生及相邻产品验收通过；语言 API 与 frame 继续。
- [不可变原生值在开放 Set 中的内容语义](stdlib53-b208-native-leaf-value.md)：通用驻留投影、生产者能力与身份键边界；双端原生、生成 C／JIT 及相邻产品验收通过，语言键直接 ABI 与公开 SlotMap 继续。
- [原生值复制与失败原子性](stdlib53-b209-native-leaf-copy.md)：键容器与动态投影共用 checked 复制事务，检查空输出、对齐、重叠与晚期失败；语言键直接 ABI 与 frame 继续。
- [拥有型原生值的同步 IR 合同](stdlib53-b210-native-value-ir.md)：声明化直接记录、checked 复制与投影、参数和多返回值生命周期；跨挂起与 SlotMap 公开 API 继续。
- [拥有型原生值的容器接入与键 ABI 校正](stdlib53-b211-native-value-collections.md)：Array／List／Set／Dict 精确值域、原生提取及语言身份／描述符赋值校正；其余消费边界和总验收继续。
- [拥有型原生记录的声明式调用](stdlib53-b213-native-value-foreign.md)：闭合原生值调用登记、借用参数与独立拥有结果；双端实际 C／JIT 和失败清理验证通过，公开 SlotMap 完整接线继续。
- [SlotMap 核心语言 API](stdlib53-b214-slot-map-core.md)：显式构造与九个 checked 核心方法，共享精确 XLIR／槽位 ABI；双端严格 C／JIT 和失败清理通过，快照、迭代与完整封板继续。
- [SlotMap 独立快照与原生值遍历](stdlib53-b215-slot-map-snapshot-language.md)：公开 keys／values 与通用 checked 原生值 foreach，双端严格 C／JIT、真实 AOT 和按需独立程序通过；成对迭代、其他消费边界及发布总门继续。
- [OrderedMap 与默认 Queue](stdlib53-b216-container-public-names.md)：有序映射公开名与同身份 MPMC 默认别名；双端批量源码、严格 C／JIT、真实 AOT 和裁剪通过，多返回值清理及发布总门继续。
- [OrderedMap 拥有结果查询](stdlib53-b218-tree-owned-queries.md)：十二个公开查询、精确缺失与拥有多返回值、泛型实例化、旧 Into 清退及双端专项验收范围。
- [Channel 常驻拥有值底座](stdlib53-b219-channel-native.md)：精确状态、准入、操作 pin、循环图退役与双端专项；语言接线、WaitQueue 和发布总门继续。
- [Channel 拥有接收结果的统一 IR 合同](stdlib53-b220-channel-ir.md)：规范消息类型、独立拥有结果、严格类型与原生工厂前置验收；公开接收切换继续。
- [Channel 拥有接收的 typed 与 lowered IR 接线](stdlib53-b221-channel-receive-ir.md)：同表操作身份、泛型逻辑合同、真实 lowering 与 checked outcome 双端专项；公开语法和原生 Cgen 切换继续。
- [OrderedMap 原生查询与独立结果](stdlib53-b217-tree-query-native.md)：原生结果槽与 checked 事务的底座专项；公开语言切换见拥有结果查询。
- [SlotMap 的原生键和值快照](stdlib53-b212-slot-map-snapshots.md)：精确拥有快照、失败回滚与独立键组件；双端原生专项及值快照实际裁剪程序通过，公开语言 API 和成对迭代继续。
- [List 方法接入与集合异常合同](stdlib53-b174-list-methods-language.md)：checked 驻留方法、may-raise 和真实 C/JIT/continuation 验证；工厂、动态分发及恢复仍待切换。
- [List 元素准入与目标构造合同](stdlib53-b173-list-target-recipes.md)：XLIR 精确元素 witness、泛型闭包和目标工厂；错误元数据与分配/作用域失败批处理验证，源载体尚未切换。
- [List 原生动态方法与快照迭代](stdlib53-b172-list-erased-native.md)：复用 typed List 存储与游标，checked 动态方法、完整稀疏键和单步拥有投影；语言载体切换仍待完成。
- [Open 动态字段语言恢复与 clone 隔离](stdlib53-b171-open-restore-language.md)：受支持 JSON/XLON Open 根切换驻留事务；明确 XLIR 数据和可达性、双端语言与完整夹具 OOM 批处理通过，其余载体和发布总门继续。
- [Open 动态字段恢复事务](stdlib53-b170-open-restore-native.md)：声明 clone 隔离、独立新存储及整批原子槽提交；B170 原生底座验收，语言切换见 B171。
- [DynamicFields 原生拥有图与退役合同](stdlib53-b168-dynamic-fields-ownership.md)：逐槽拥有边、精确存储准入、弱提升隔离与分阶段清理；双端原生及语言邻接回归通过，Open 语言切换继续。
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
- `stdlib53-b222-channel-public.md`：Channel 显式状态与拥有接收值的公开接入，及双平台 C/JIT/AOT/裁剪专项与剩余验收范围。
- `stdlib53-b223-channel-regressions.md`：Channel 存量回归迁移与空值、错误清理修复；双端批量验证、故障前缀及未完成范围。
- `stdlib53-b224-compiled-contract-gates.md`：IR 编译后所有权目录与常驻线程验收，及静态总门完整诊断和剩余范围。
- `stdlib53-b225-identity-fields-gates.md`：类身份与开放动态字段的常驻路由修复、源码／独立程序／生命周期验收，以及剩余总门范围。
- `stdlib53-b226-persistence-batch.md`：boxed 投影与原生 Dict 配置修复，四格式严格 C／JIT／独立 AOT、完整故障前缀和图合同批量验收；33 条静态诊断及发布总门继续。
- `stdlib53-b227-container-contracts.md`：隐式 Set 类型闭包与精确测试 C ABI 修复，双端专项验收；union 容器、原生 Dict 点访问及发布总门继续。
- `stdlib53-b228-dict-named-route.md`：原生 Dict 点／索引共用拥有值与精确键路由；封闭 XLIR、故障前缀、独立程序及组件裁剪验收，union 容器和总门继续。
- `stdlib53-b229-thread-retirement.md`：worker 完成与真实引用退役原子提交，修复捕获清理竞态；确定性坏实现拒绝、双端完整线程与故障压力验收。
- `stdlib53-b230-union-containers.md`：普通 union 原生集合配方与可空载体，精确身份／大小准入、故障前缀、坏实现和双端专项；发布总门继续。
- `stdlib53-b231-slot-map-pairs.md`：SlotMap 同次键值捕获、共同发布与回滚，双端原生／插桩及公开快照回归；成对 foreach 接线继续。
- `stdlib53-b232-slot-map-loop-plan.md`：单变量完整键、双变量同次键值的 Sem／XLIR 合同，原生拥有键与精确投影通过双端专项；配对游标和公开 foreach 接线继续。
- `stdlib53-b233-slot-map-iterator.md`：原生游标直接复制完整键，四种最小入口、真实拥有环、故障前缀、坏实现与独立程序裁剪通过；lowering 与公开 foreach 接线继续。
- `stdlib53-b234-slot-map-loop.md`：同步 SlotMap 遍历、直接完整键、四种最小入口、退出清理、语言故障前缀与独立裁剪通过双端专项；原生键跨挂起及总验收继续。
- `stdlib53-b235-native-capture.md`：原生拥有叶子值的独立 V2 捕获描述、typed detach、首个失败、实际长度与坏实现对照通过双端专项；真实挂起所有者及 IR 接线继续。
- `stdlib53-b236-native-generator.md`：真实生成器帧接通完整键跨 yield 和独立参数复制，统一 IR 与 checked 清理、直接原生闭包、双端故障前缀及旧生成器回归通过；await 和原生语言结果继续。
- `stdlib53-b237-native-generator-values.md`：generator<SlotMapKey> 直接产出和 take，结果专属发布依赖、嵌套转发与来源退役、双端独立 C／JIT／AOT 及完整失败前缀通过；异步槽和 Future 元素继续。
- `stdlib53-b238-native-async-frame.md`：完整键跨两次 await、新版捕获／输送布局、真实 Watch／producer／取消与私有退役通过双端专项和旧压力回归；Future 原生结果／callback 捕获继续。
- `stdlib53-b239-native-future-payload.md`：完整原生 Future 结果发布、重复 await 的独立拥有值、部分复制回滚、busy 清理重试及双端独立 C／JIT／AOT／选定插桩通过；原生捕获、Future 元素与总验收继续。
- `stdlib53-b240-native-callback-capture.md`：原生键 task.spawn／then 与 async 参数捕获、checked 清理和初始化后 auto 推导通过双端专项及旧捕获／异步压力回归；Future 元素与总验收继续。
- `stdlib53-b241-future-iteration.md`：五类容器的 Future／nullable Future 元素循环、拥有绑定及跨 await／yield 清理通过双端专项、独立裁剪和旧异步压力回归；生产改动仅 IR，Channel／WaitQueue 与总验收继续。
- `stdlib53-b242-native-value-fields.md`：原生拥有值类字段读取、临时接收者与返回 finally 通过双端专项、故障前缀和选定插桩；Channel 公共切换状态已核实，WaitQueue 拓扑与总验收继续。
- `stdlib53-b243-queue-pressure.md`：三种队列公共并发收发、实际线程内故障隔离及字符串发布前 checked 复制通过双端专项、六个坏供应方和选定插桩；WaitQueue 与总验收继续。
- `stdlib53-b244-three-group-discovery.md`：双端 288 个夹具统一生成／严格 C 检查、Stack 原生 Array／Dict 子域修复及独立运行、完整分配前缀和坏 IR 拒绝；不等同于 53 模块发布验收。
- `stdlib53-b245-stack-native-value.md`：Stack 的原生拥有值单项／批量、寿命及错误边界通过双端专项、完整分配检查和三个实际坏实现；nullable／Future／函数值域及发布总门继续。
- `stdlib53-b246-nullable-native-foundation.md`：可空原生键的显式存在位、拥有值复制与失败清理底座通过双端专项、真实坏供应方及选定插桩；语言 nullable／tryPop 接线尚未完成。
- `stdlib53-b247-nullable-native-language.md`：可空原生键基本语言操作和 Stack 三个 try 方法通过双端实际运行、完整分配检查与四个坏实现；默认值／转换／合并／捕获及可空容器元素继续。
- `stdlib53-b248-nullable-native-controls.md`：可空原生键默认值、静态／动态转换和惰性空值合并通过双端实际运行、54 次分配检查与五个坏实现；捕获／容器元素与总门继续。
- `stdlib53-b249-nullable-native-capture.md`：可空原生键只读闭包、task／then、Future／await 与生成器通过双端运行、故障注入及安全插桩；可空容器元素、可变原生 cell 和总门继续。
- `stdlib53-b250-nullable-native-elements.md`：可空原生键接通六类容器元素域；双端独立 C／JIT／AOT、故障前缀、IR 拒绝和选定插桩通过，其余组合与发布总门继续。
- `stdlib53-b251-native-cell.md`：普通／可空原生键的共享可变闭包 cell、统一参数拥有取得与函数签名推断通过双端执行及 Linux 插桩；其余域与总门继续。
- `stdlib53-b252-stack-future.md`：Future Stack 采用 owner 局部物理配方，双端执行、故障前缀、循环图与 Linux 插桩通过；函数值域与发布总门继续。
- `stdlib53-b253-stack-callable.md`：函数值 Stack 先完成拥有投影再弹出，独立闭包与代码租约、双端故障前缀及 Linux 插桩通过；只读最小程序按需裁剪，其余域与总门继续。
- `stdlib53-b254-runtime-discovery.md`：Dict 命名空间和 PatternBuilder 私有状态表示边界修复；双端实际运行把七个失败收缩为一个嵌套编译器 SDK 问题，专用宿主与发布总门继续。
- `stdlib53-b255-wait-queue-native.md`：独立 typed MPMC 等待底座复用队列与 Sync 生命周期；双端并发、Future 图、故障前缀和坏实现控制通过，WaitQueue 语言切换仍继续。
- `stdlib53-b256-wait-queue-owned.md`：WaitQueue 状态与拥有接收完成语言接线，旧 Into 和生成队列算法清退；双端 52 阶段、301 夹具严格 C 通过，嵌套 SDK 和发布总门继续。
- `stdlib53-b257-nested-sdk.md`：默认与嵌套 Context 自动解析模块旁 SDK；双端专项各 13 阶段、301 项声明与实际执行零失败，生成资源和发布总门继续。
- `stdlib53-b258-nested-aot.md`：真实 CLI/DLL/VFS 的嵌套编译器与独立 AOT 双端各 32 阶段通过，Sync 恢复各 26 阶段通过；声明资源和核心源码漏项收口，完整 53 模块总门继续。
- `stdlib53-b259-hosted-acceptance.md`：六项专用宿主夹具双端各 309 阶段、3364 次计量生命周期通过，五类损坏报告拒绝；工作线程／图 OOM、卸载压力与完整函数覆盖继续。
- `stdlib53-b260-native-ssa-async.md`：独立原生 SSA 跨 await 的取得与失败清理、保护借入零额外复制、双端终态／故障前缀与 Linux 插桩；generator 独立 spill 和完整总门继续。
- `stdlib53-b261-generator-prepare.md`：generator checked 准备／成功提交 V4、原生 SSA 取得与 item 回滚、finally 和双端批量回归；专用宿主压力与发布总门继续。
- `stdlib53-b262-generator-hosts.md`：关闭路由／首错／帧借入／拥有图专用宿主双端完整执行、原生 Array 拓扑严格检查与 Linux 插桩；卸载和发布总门继续。
- `stdlib53-b263-generator-lifetimes.md`：生成器跨线程冻结／循环图退役／真实回调库解除映射双端通过，补 V3 独立 control/frame 描述符卸载；任意生成模块及发布总门继续。
- `stdlib53-b264-current-products.md`：当前 IR 双端 CLI／库／VFS 重建、302 项实际产品清单及完整子门复测，路径构建迁到驻留 Array 输入事务；平台／全函数／worker OOM 总门继续。
- `stdlib53-b265-worker-completion.md`：当前 worker 捕获和精确 NUL 字符串合同，完整完成／取消场景及真实工作线程五类分配前缀双端通过；调度器整体 OOM 与发布总门继续。
- `stdlib53-b266-resolved-coverage.md`：双端实际登记和统一 IR 源调用的 228 个签名绑定、重载／泛型反例及数组算法真实产品运行／模块／AOT；类方法与运行时全函数覆盖继续。
- `stdlib53-b267-class-surface.md`：30 个实际展开类的 329 个公开签名绑定、类身份反例及四组真实产品补测；内建容器／动态调用／字段与完整运行时覆盖继续。
- `stdlib53-b268-nominal-predicate.md`：错误类型 class tag 的纯判定缺陷与双端原生修复；真实产品确认 Native var 方法 ABI 缺口，保留 pending 回归，不声称动态接入完成。
- `stdlib53-b269-dynamic-value-domains.md`：动态方法精确值域、名义和容器匹配，bytes 拥有／可空语义及双端严格 C／JIT／module／AOT；Native 原生字段投影继续。
- `stdlib53-b270-native-dynamic-envelope.md`：Native 动态参数 checked 投影、直接结果事务、负默认值及双端 104 项验收；Stack 动态资源分派与其余 Native 编码继续。
- `stdlib53-b271-stack-projection-foundation.md`：Stack 精确角色的原生投影、失败原子 Pop 与克隆策略，双端原生、Linux 插桩和错误实现对照通过；统一 IR 动态分派继续。
- `stdlib53-b272-stack-dynamic-ir.md`：显式 Stack 擦除、核心动态方法、双端 72 项与完整拥有前缀通过，原 pending 端到端通过；动态完整公开面及字符转换继续。
- `stdlib53-b273-char-dynamic-checked.md`：动态 char 构造／类型断言、独立成功状态、双端 107 项与 52 个 IR 变异；原生插桩、错误实现对照和 typed 裁剪通过，完整总门继续。
- `stdlib53-b274-stack-dynamic-batch.md`：Stack 动态批量／equals、IR 结果 Array 身份和 checked 清理；双端 85 项、81 个 IR 变异与完整 OOM 前缀通过，动态 try／容器转换及总门继续。
- `stdlib53-b275-stack-try-multivalue-native.md`：Stack try 原生双结果拥有底座；双端 84 项、18 条前缀及八个错误实现对照通过，语言多值接收和 product IR 仍继续。
- `stdlib53-b276-dynamic-function-products.md`：动态函数的上下文 product 与独立成功状态；双端 85 项、30 个 IR 变异和完整同步前缀通过，动态方法／绑定方法及 nullable 编解码继续。
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

## 标准库驻留迁移专项记录

以下是限定验收和历史检查点。当前状态以三组长期任务账本及最新专项为准，
不能将各批局部通过合并为完整标准库或现有发布库的通过结论。
逐项决策和未决策状态见 [标准库决策清单](标准库决策清单.md)。

- [生成 C 原生声明门](generated-c-declaration-gate.md)：准确函数原型和隐式声明拒绝。
- [字段发布后退休](stdlib53-b133-post-commit.md)：真实语言对象图与字段替换清理。
- [Queue continuation](stdlib53-b134-queue-continuations.md)：拥有值跨 await 的限定验证。
- [原生 Tree 检查点](stdlib53-b135-native-tree.md)：驻留迁移与后续真实产品入口。
- [Queue 域认证](stdlib53-b135-queue-domain-probes.md)：物理族与完整准入域探测。
- [原生 Semaphore](stdlib53-b135-semaphore.md)：私有生命周期前置能力。
- [同步资源 checked 退休](stdlib53-b135-sync-retirement.md)：手动解锁与宿主恢复检查点。
- [原生 Thread](stdlib53-b141-thread-native.md)：驻留拥有者前置能力。
- [Thread 语言迁移](stdlib53-b142-thread-language.md)：语言入口与生命周期专项。
- [Thread 真实产品](stdlib53-b143-thread-shipping.md)：产品及持锁恢复限定回归。
- [Stack 批量接口](stdlib53-b144-stack-batch.md)：独立 Array 发布与产品专项。
- [Tree 与 Queue 真实产品图](stdlib53-b145-container-graph-shipping.md)：循环图与 continuation 的产品证据。
- [私有 List 底座](stdlib53-b146-list-native-foundation.md)：稀疏原生存储，非语言载体切换。
- [List 身份与合并](stdlib53-b147-list-identity-merge.md)：完整域查询与事务合并。
- [List 动态边界](stdlib53-b148-list-value-boundaries.md)：私有投影与显式稀疏导入。
- [List 嵌套类型](stdlib53-b149-list-nested-schema.md)：跨容器不可变元数据链。
- [List 共享图](stdlib53-b150-list-shared-graph.md)：私有共享深复制与数据投影。
- [List 与 Array 恢复](stdlib53-b151-list-recursive-restore.md)：私有混合类型链事务。
- [类字段共享恢复](stdlib53-b152-class-shared-restore.md)：类字段与容器复用恢复事务。
- [Nullable 恢复错误](stdlib53-b153-nullable-restore.md)：数据拒绝和运行失败的分离。
