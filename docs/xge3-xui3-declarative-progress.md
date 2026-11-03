# XGE3 / XUI3 声明式接入实施状态

11 月 4 日实例工厂／权威头验收完成：Windows 完整编译 1,414 单元、1,562 个 TCC 对象，
24 组接受。原批次保留四处旧 `extern` 文本断言失败，改验精确函数指针断言后仅补跑四组，
其余 20 组沿用首次成功结果。Linux 完整编译 1,413 单元及 10 组 GCC O0/O2 检查通过。
实例工厂沿用已有输出探针／rollback／代码驻留事务，支持直接、命名、绑定及已推断动态路径；
每个优化等级拒绝 8 项 IR 篡改、16 次空 C/header 输出。头声明额外检查错误结果及参数类型。

XGE `0d8f6340` 增加默认／取消动作、光标、IME、cache render、update 的类型化 Owned 槽。
Windows 七组 O0/O2、Linux 七组 O0/O2 + ASan/UBSan/泄漏检查通过。
两端完整默认 O2 宿主引擎均构建 139 单元、共用 174 个 XRT 导入；Windows 为 8,674,923 字节，
Linux 为 9,396,080 字节。真实声明组合在两端 GCC O0/O2 通过九种注册 API，包含 void、
整数、按值矩形、测量指针输出、保存事件／输出后过期拒绝，以及回调中关闭 context。
200 次预热测量无新增 XRT 分配／释放／realloc；该实际组合没有验收 TCC，也没有做 GPU、
桌面输入或网络测试，不能推出所有调用路径零成本。

完整声明包、其他回调族、独立资源、自动初始化和发布迁移仍待完成；八项整体任务没有封板，
发布 DLL／SDK／IDE 未替换。原生受管值类直接提升为 protocol 的独立表示缺口也仍需处理，
本批未放宽验证器或加入按名称猜测的回退。
证据：`dev/release-materials/provenance/foreign-instance-xui-checkpoint-20261104.json`。


11 月 4 日 `@c_deref` 完成正式验收：Windows 完整编译 1,414 单元、1,562 个 TCC 对象和
23 组联合检查全部通过；Linux 完整编译 1,413 单元及 9 组 GCC O0/O2 检查通过。
回调输出指针直接读写原生 pointee，保存后读／写均 checked 拒绝；请求持久文本时直接使用
XRT 已有的 `xrtStrView`。每个优化等级拒绝 40 项 IR 篡改、80 次空 C/header 输出；
8 项负例明确到达语义层。200 次预热调用无新增 XRT 分配／释放／realloc。
此提交只封存该通用能力；真实 XUI 接线发现的实例工厂／权威头声明修正另行验收，
其他回调族、独立资源、完整声明和发布迁移仍未完成。
证据：`dev/release-materials/provenance/foreign-pointee-checkpoint-20261104.json`。


11 月 4 日 XGE `07a6b765` 为内容测量和布局 Prepare／Children／Complete 增加原生 Owned 入口。
函数及 userdata 保持类型化；稀疏拥有目录只用于所有权，记录独立于 Reference.Data 的 userdata。
替换事务在 Hold 重入后重新查找实际槽，清理发生在发布后；调用快照保活至完整 thunk 返回，
底层 native operation 继续保护余下原生访问。完整拥有图包括这些实际槽。
Windows 六组原生 O0/O2 与 Linux 六组 O0/O2 + ASan/UBSan/泄漏检查通过。
完整默认 O2 宿主引擎构建 138 单元、导入 174 个共用 XRT 符号；与当前 xlang 核心的实际组合
O0/O2 通过，包含布局回调替换／关闭、闭包回到 context 的循环及 200 次预热后无新增 XRT 分配／释放／realloc。
该实际组合复用已校验的完整编译器对象，只重编相关原生运行时单元，不是另一次整套编译器重建。
其他原生回调族、完整语言声明和发布迁移仍待完成。证据见
`dev/release-materials/provenance/xui-owned-layout-checkpoint-20261104.json`。

11 月 4 日计数字段投影及匿名函数解析修复通过下一份完整冻结队列：Windows 1,414 单元、
1,562 个 TCC 对象和 22 组联合验收全部通过；Linux 1,413 单元及 8 组 GCC O0/O2 专项通过。
`@c_length` 从同一受准入接收者直接读取字节长度，保留 NUL，拒绝负数、固定数组越界、
NULL 非零长度和超出语言范围的指针长度。结果在接收者清理前复制；视图本身不分配。
每个优化等级拒绝 27 项长度合同篡改并检查 54 次空 C/header 输出。
同时修复匿名函数体中 `value = object.field` 与下一行赋值被错误合并为限定类型的问题，
恢复语句边界后再识别限定声明，并保留函数类型分类。该问题有修复前失败及修复后运行夹具。
这仍是通用前置能力检查点，八项整体任务及发布包迁移尚未完成。证据见
`dev/release-materials/provenance/foreign-counted-fields-checkpoint-20261104.json`。

11 月 4 日新增原生字段／整数状态／单个平凡输出值合同。字段生成直接 C 成员表达式，
状态成功路径直接比较返回码；输出参数由声明指定物理位置，语言直接取得值，失败不提交。
同一冻结队列完整编译 Windows 1,414 单元、1,562 个 TCC 对象；21 组验收已接受。
首次批次保留字段负例的测试定位错误，修正为独立错误字段类型夹具后，仅重测该组通过。
Linux 完整编译 1,413 单元，7 组 GCC O0/O2 专项通过，未包含 Linux TCC。
原生字段、整数状态和输出值每个优化等级分别拒绝 80／110／68 项 IR 篡改，C 与头输出均为空。
同一 caller 字段 getter 与手写 C 的 TCC 指令字节一致（27 字节），GCC O2 均为一次字段读取和 return；
该结论不覆盖受管 scope、错误边界或回调适配。另修复 XGE Hold 重入覆盖拥有槽及关闭后提交，
上游提交 `5e6fda67`，Windows O0/O2 与 Linux ASan/UBSan 均通过。
八项整体任务及发布迁移仍未完成。证据见
`dev/release-materials/provenance/foreign-native-fields-status-checkpoint-20261104.json`。

11 月 4 日回调期视图已接入声明／XLIR／Cgen：真实 C 栈事件在回调内可读，保存到
类字段、共享 cell、数组、动态值和绑定方法后，过期访问 checked 拒绝；派生视图
共享原有效期。Windows 完整重建 1,414 单元、1,562 个 TCC 对象及 VFS，18 组联合
验收通过；包含 GCC O0/O2、公开头和 TCC JIT／模块／独立程序。每个优化等级拒绝
47 项 scope 合同篡改并检查 94 次空输出。预热后 200 次 typed 回调没有新增 XRT
分配／释放／realloc。Windows 原生测试另覆盖跨线程／fiber、嵌套及地址复用。
Linux 完整重建 1,413 单元，managed／span／C-string／scope 四组 GCC O0/O2 验收通过；
scope 原生修改单元 O0/O2 + ASan/UBSan/泄漏检查通过，复用基础对象未重新插桩。
此处尚未更新真实 XGE/XUI 的其余 callback owner 槽和发布包。
证据见 `dev/release-materials/provenance/foreign-callback-scope-checkpoint-20261104.json`。


11 月 4 日：受管视图的语言／XLIR／Cgen 接入已通过同一隔离源码队列的 17 组联合验收。
本队列完整编译 1,412 个 Windows 单元，生成 1,560 个 TCC 对象并重建隔离 VFS；
GCC O0/O2、公开头及 TCC JIT／模块／独立程序均通过相应专项。
新增 split-span、Buffer 存储排他准入、长期类型化回调及终止符字符串声明；
C 生产者错误 ABI 与参数准备／结果准入的错误合同分别验证。
原生结果在输入清理抛错或代码驻留准入重入销毁期间保持精确回滚，不重复释放。
旧手工 AST 的 `xlir_function_lower` 回归已改为真实前端驱动并恢复执行。

这完成了上表中分离指针／长度、Buffer span、managed 工厂、语言受管存储及长期
回调自动 thunk 的通用编译器底座。真实 XGE/XUI 的其余 callback owner 槽、
独立资源、回调期 Event/DrawContext、包初始化及最终产品迁移仍需完成；
以下历史检查点按原记录保留。发布 DLL、SDK、IDE 未由本次验收替换。
证据见 `dev/release-materials/provenance/foreign-managed-calls-checkpoint-20261104.json`。


更新：2026-11-03。当前是编译器基础能力检查点，八项任务尚未全部完成，发布包尚未迁移。

底座已有受管视图的类字段、Array／Stack 物理存储和循环拥有图。长度保持的文本／二进制视图声明、caller 视图的类型化绑定方法值及精确调用目标验证已完成。XUI 原生数据模型的拥有槽及真实控件循环回收已具备，作为受管语言接入的前置。完整原生回调面、受管语言及包合同仍在开发，八项尚未全部完成。

| 项目 | 已落地 | 剩余工作 |
| --- | --- | --- |
| 1. 原生类型 | 精确 C 接收者／参数／返回值、可空、静态继承和别名构造；caller 生命周期声明 | 与受管 owner 的接入归入第 6 项 |
| 2. 原生数据 | 按值 C record、内联固定数组、精确 const 指针、权威头布局检查、同步／异步／生成器读写 | 分离指针／长度参数、可变 Buffer 数据视图及实际绘制接口验收 |
| 3. 工厂 | int 状态／指针输出、部分失败回滚、const 记录地址、函数值适配 | 受管／复合输出、可能失败的参数清理期间的原生结果保护 |
| 4. 回调 | 原生记录／指针 ABI；无捕获 no-raise 函数直接传 C 地址；私有 retained userdata、独立 body／adapter 驻留、原生 XUI 主事件／分类事件槽、重入／关闭与循环图 | 其余原生回调拥有合同、语言声明／XLIR 和 C thunk 自动接入 |
| 5. 文本与二进制 | 精确 xstrview／xbytesview 输入与借用结果声明；实际长度及 NUL 保留；结果在参数清理前物化；生产者 no-raise 与复制失败分开；自由函数、直接方法、属性、绑定方法、协议／动态调用和异步／生成器 | 终止符 C API 合同、分离的指针／长度参数、可变 Buffer span 与受管作用域组合 |
| 6. 生命周期 | caller 责任边界及精确指针的绑定方法捕获；XUI 全实例回收、弱通知和调用作用域；FlowGraph／Workflow 当前值、历史快照及控件 payload 追踪；内部控制记录、拥有域、闭包／cell／Future graph、普通类字段存储及 Array／Stack 拥有槽通过 Windows／Linux 检查 | 语言字段／容器 schema、owner／transfer／context 与 XLIR 接入、其余原生回调／独立资源的拥有合同及受管视图绑定方法值 |
| 7. 效果与生成 | C 生产者与工厂准入分开；静态回调效果检查；平凡直接 C 转发的参数放置 | 受管准备／结果／清理的完整效果合同，以及其余路径的性能核对 |
| 8. 包迁移 | XRT 源码对齐；XGE 宿主模式不内嵌 XRT，完整引擎已验证共用实际 xl.dll／xlc.dll | 包自动初始化、XGE/XUI 完整声明清单、移除旧转发包、SDK／IDE、示例和交付验收 |

`@lifetime("caller")` 和 `@c_static_callback` 都是声明作者承担的原生／模块生命周期合同，不能冒充第 6 项的受管安全能力。

11 月 3 日二进制声明检查点：`@c_bytes_view(index)` 将非空 bytes 映射为按值
`xbytesview`；`@c_result("bytes_view")` 将借用结果在输入／接收者清理前复制为
bytes／bytes?。实际长度、NUL 和非 UTF-8 字节保持不变。文本／字节共用封闭的
结果转换枚举；协议分派也按登记的物理视图调用，不再把全部显式参数限定为直接值。

本批最终隔离快照完整编译 1,410 个单元，重新生成 1,558 个 TCC 对象和 VFS。
字节专项 34 阶段通过，包括公开头、GCC O0/O2、TCC JIT／模块／独立程序、
12 项明确到达语义层的声明负例、函数值／绑定／协议／动态调用及异步／生成器。
每个优化等级拒绝 98 项视图 IR 篡改，并检查 196 次空 C/header 输出。
聚焦的序列化 schema 回归每个优化等级拒绝 482 项投影及 3 项图合同篡改，
不代表重跑完整序列化矩阵。输入检查实测无 XRT 分配、释放或 realloc；借用结果
物化一次分配并复制数据，保留 checked scope 成本。

既有基础 11 组、文本 25 阶段和绑定 68 阶段通过。总计 14 组的结论保留首次
批次的 13 组成功记录，只重测失败的字节组，不重写原始失败报告。Linux 对同一
生成 C 和原生提供方的 O0/O2 + ASan/UBSan/泄漏检查通过；宿主 SO 和 Linux
前端没有重建或重新插桩。详情见
`dev/release-materials/provenance/foreign-bytes-view-checkpoint-20261103.json`。

本批还纠正了旧文本负例的系统编码写入：此前部分拒绝实际发生在读文件阶段。
现在写入 UTF-8，要求退出码 1 和明确的 `[sem]` 诊断，历史报告保留原样。
额外发现 `xlir_function_lower` 的手工 AST 运行夹具在上一批编译器及原始 HEAD
源文件上同样因异常清理合同失效；本轮仅验证其枚举迁移能编译，不计为运行通过，
另记待修。当前前端生成的 IR 合同验收仍完整执行，没有放宽验证器。
受管类型语言／XLIR、长期回调与最终产品迁移尚未完成，发布 DLL、SDK、IDE 未更新。

11 月 3 日原生 payload 检查点：XGE `20fad3e8` 增加 `xuiContextTraceOwnership` 和类型级只读追踪合同，
逐项追踪 FlowGraph 的当前配置、撤销／重做和批量删除快照，以及 Workflow 的配置 schema 与变量默认值。
拥有模型的控件负责这些实际槽；Workflow 的画布和其他借用画布不重复计入模型拥有边。
自定义类型按父链分别追踪自己引入的槽，存在派生实例时禁止替换合同，未挂树实例也包含在 context 中。
普通绘制／事件路径不新增 payload 追踪；每个注册类型增加一个函数指针，控件实例无新增字段或分配。

Windows／Linux 的五组原生 O0/O2 检查通过，包含既有 47 项拖放重入和 200 次无分配／释放／realloc 的追踪；
Linux 对该原生检查的 XRT、XUI core 和模型单元启用 ASan/UBSan/泄漏检查。
完整默认 O2 宿主引擎在两端构建完成，实际组合夹具同时回收事件闭包、动态值、
FlowGraph／Workflow 及旧／新快照回到 context 的四种循环，并检查重复借用画布不增加拥有边。
xlang 驻留核心采用已校验对象的增量验收，不能描述为本轮重新构建全部编译器或给整个引擎做 sanitizer。
受管视图的语言／XLIR 接入、其他原生回调的拥有入口和最终包迁移仍未完成。

11 月 3 日绑定方法检查点：显式 `func(...)` 类型的 caller 视图绑定复用原有闭包及 XLIR CFG，
环境只保存一份精确 C 指针；接收者重赋值、绑定逃逸、默认参数、别名／静态继承和文本视图合同均已验证。
普通类仍采用拥有引用捕获，不把具有原生析构的资源放宽为平凡捕获。验证器还修复了一个通用缺口：
主调用即使被改指向同签名的其他方法，也必须拒绝，不能只检查调用 ABI。

本批隔离快照重新编译全部 1,410 个单元，并生成 1,558 个 TCC 对象及当前 VFS。
绑定专项 68 阶段通过，包括既有 22 项绑定正例的 Cgen 回归、22 项负例、GCC O0/O2 和 TCC JIT／模块／独立程序。
每个优化等级的接收者专项拒绝 76 项 IR 篡改并验证 152 次空 C/header 输出；既有静态／虚拟分派专项也通过。
文本专项 25 阶段和声明式基础 11 组复验通过。Linux 对同一生成 C 与原生夹具的 O0/O2、
ASan/UBSan/泄漏检查通过；复用的宿主 SO 和 Linux 前端没有重建或重新插桩。

当前 64 位夹具中，首次绑定在 GCC 原生程序实测 2 次 XRT 分配、144 字节请求存活量；
TCC JIT／模块／独立程序为 3 次、200 字节，两者均无 realloc。
稳定 200 次调用没有增加 XRT 分配或 realloc，但仍有闭包间接调用、环境读取、拥有权及代码活动登记成本。
此测量不等同于与手写 C 的全路径性能比较，也不构成零开销结论。受管视图语言／XLIR 及产品迁移仍待完成。

11 月 3 日文本声明专项 25 阶段通过，包含 GCC O0/O2、TCC JIT／模块／独立程序、公开 C 头、10 项声明负例；每个优化等级另运行 41 项独立 IR 破坏性检查，C／头输出均为空。只读视图输入未增加 XRT 分配，借用结果物化实测一次分配；它仍有复制和 checked scope 成本。既有声明式基础 11 组复验通过。Linux 对同一生成 C 与原生夹具运行 O0/O2 + ASan/UBSan/泄漏检查，未重新构建／插桩宿主运行库或 Linux 编译器。

当前 Windows 目录按用户新约定为 `release/win_x64`。源码验收工具已增加显式 RuntimeDirectory，并修正 SDK 目录链接的预编译指纹；正常工作区没有重建旧 release/include。因为平台目录迁移还有旧相对路径，本批使用独立源码快照及仅限该快照的旧 SDK 路径映射：完整 1,410 单元构建、1,558 个当前 TCC 对象和隔离 VFS 通过。这不是整个工作区平台迁移／发布包已经验收的声明；发布 DLL、SDK 与 IDE 未由本批替换。

11 月 2 日 Windows 源码构建为 1,410 个编译单元，输入校验一致（7 个修改单元重编，其余经过预处理内容及对象字节校验复用）。该批声明式 11 组通过，受管容器 Windows／Linux 修改单元专项各 50 阶段通过。11 月 3 日重新构建上述隔离快照的全部 1,410 个单元，文本专项和基础 11 组通过；Linux 的完整编译器验收仍来自此前构建，不扩大为本轮已重建全部 Linux 编译器。

同一平凡 getter 的 Windows TCC 代码从 51 字节／15 条指令／48 字节栈降至 32 字节／10 条指令／32 字节栈，与手写 C 指令字节一致。Linux TCC 对照也一致；GCC O2 两者均采用相同尾调用。该结果仅适用于已验证的精确转发形态。

实现与声明语法见 [foreign-declarations.md](foreign-declarations.md)。证据摘要在 `dev/release-materials/provenance/`；当前发布索引和 `release/` 的引擎、SDK、XGE/XUI 产品未由本批更新。

原生生命周期前置已在 XGE 提交 `a382e6b8`、`6f064f53`：修复未挂树实例随 context 泄漏及构造重入回滚，并提供在用户析构前清除弱视图的监听。Windows O0/O2 和 Linux O0/O2 + ASan/UBSan 均通过，包含既有 47 项拖放重入测试。语言控制记录和拥有图仍待接入。

XGE `eb13f054` 新增 `--host-xrt` 构建。完整默认 O2 引擎需要 174 个宿主 XRT 符号，分别与发布目录的 `xl.dll`／`xlc.dll` 完成直接及嵌入式 TCC 调用、真实宿主分配计数和完整销毁检查。相同源代码、功能和 O2 配置，独立引擎 9,224,256 字节，宿主引擎 8,661,099 字节，净减少 563,157 字节。XRT 调用采用精确函数指针槽；相对同编译单元内嵌实现会失去相关内联机会，不能由此宣称所有路径零开销。当前 DLL 仍位于隔离验证目录，后续包／SDK／IDE 集成会落实自动绑定与代码驻留合同。

XGE `79423767` 提供可嵌套的 `xuiCallScopeBegin/End`，逻辑失效立即发生，物理释放推迟至原生操作退出。内部受管运行时已验证实例复用、线程约束、通知栈延迟释放、checked 重试和循环图；完整 XUI base widget 在实际 `xl.dll`／`xlc.dll` 上也通过 O0/O2。它尚未接入编译器、捕获 schema 或发布包，不能当成第 6 项全部完成。具体边界见 [foreign-managed-view-design.md](foreign-managed-view-design.md)。
