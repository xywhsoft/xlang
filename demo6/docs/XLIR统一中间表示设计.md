# XLIR 统一中间表示设计

本文档定义 DEMO6 后端重构的目标边界：`AST -> XLIR -> C`。XLIR 是 C 后端唯一允许消费的规范表示；AST 只服务于解析、诊断和前端语义分析。

## 1. 当前迁移状态

当前已落地 `src/ir/xl6_ir.h` 与 `src/ir/xl6_ir.c`。`XL6_CompileUnit` 在语义分析后直接从 AST 构建并验证 owned typed XLIR，`xl ir` 和 `xl cgen --dump-dir` 的 `.xlir` 输出已经使用 XLIR。

XLIR 复制并拥有节点文本、类型池、子节点索引、符号索引和源码位置；完成构建后没有 `XL6_AstNode*` 回指。`XL6_IrTypePool`、`XL6_IrNodeKind`、值类别和所有权枚举都是 XLIR 自有定义。旧 HIR 类型、构建器和迁移适配器已经从源码中删除，公开头与编译单元都不再包含 HIR 状态。

`cgen`、`run`、`build`、编译器 DLL 和调试构建都只进入 XLIR。旧 AST/HIR Cgen 的入口、实现、构建接线和公开枚举已经物理删除；`--backend legacy` 会在参数解析阶段被拒绝。任何新语义都必须形成可验证的 XLIR 合同，不能在 C emitter 内重新推导。

## 2. 统一模型与阶段不变量

同一 `XL6_IrModule` 在不同阶段收紧约束，而不是维护两份相互漂移的 HIR/CIR：

1. `XL6_IR_STAGE_TYPED`：每个值节点具有 `TypeId` 与 `storageType`；符号、调用目标、左值、所有权和退出计划已经确定。
2. `XL6_IR_STAGE_LOWERED`：函数体变为基本块和指令；控制转移、清理、`await`、异常与 runtime intrinsic 显式化。

第一条 lowering 已落地于 `src/ir/xl6_ir_lower.c`：每个函数（及模块级初始化函数）拥有独立的连续 CFG/指令/值区间和参数符号表；已验证的局部声明、字面量、已解析名称、已解析直接调用、赋值和返回会变为 `CONST / LOAD / STORE / CALL / RETURN` 指令，无法映射的语义直接报错。`XL6_IrFunction.kind` 显式区分 `MODULE_INIT` 与 `USER`：模块初始化不再依赖 `symbol == 0` 这种 Cgen 私有约定，验证器同时固定其无参数、无返回 ABI。模型中的 `XL6_IrFunction`、`XL6_IrBlock`、`XL6_IrInstruction` 和 `ValueId` 因此已不是预留结构。纯临时计算采用 SSA 风格；可变局部、字段、索引和解引用在后续 lowering 中使用 `load/store` 的 Place 模型表达。不要把所有内存状态伪装成 SSA 值。

`if` 已拆为条件块、then、else 和 merge 四个显式 CFG 块，条件分支与跳转均为块末终结指令，并由验证器检查条件和目标完整性。`elseif` 必须先在语义阶段规范化为嵌套 `if`，否则 lowering 失败；不能让 C emitter 自行拼接条件链。

当前普通二元值指令会先求值两个 operand。语言的 `and` / `or`（以及规范化输入中的 `&&` / `||`）已采用显式 CFG value-merge 规则：左值首先写入一个 IR merge value，分支决定是否进入右值块，右值块只在需要时覆盖该值，随后汇合。C emitter 仅发射 `VALUE / BRANCH / JUMP`；它不借“C 恰好有相同拼写”决定短路语义。

条件表达式同样不以 C `?:` 作为语义载体：lowering 创建带明确存储 ABI 的 XLIR compiler temporary，then/else CFG 块分别写入，merge 块再 `LOAD` 结果。该 temporary 是 `XL6_IrStorage`，具有所属函数和类型，Cgen 只按该记录声明局部变量。若条件表达式的静态分支是 typed array/dict、而其声明、赋值或函数返回边界要求 `var`，lowering 在该边界创建独立的 `RUNTIME_XVALUE_PTR` temporary，并在两条 branch 内完成 array/object 构造、元素/字段 boxing 和 `STORE`；不会让 Cgen 根据条件语法或静态容器类型再选择运行时表示。

`null` 不是无类型常量，也不允许在 C emitter 中按使用位置猜测。`XL6_IrType.nullAbi` 明确区分 `POINTER_SENTINEL`、`XVALUE_SENTINEL` 与 `TAGGED_VALUE`：class/`var` 等 xvalue handle 以 XRT null value 表示，raw pointer、future、error、closure、native handle 与 managed string 使用空指针 sentinel；相应的构造和判定分别是 `NULL` 与 `NULL_TEST` 指令。声明、赋值、直接/闭包调用参数、返回和条件分支统一经 `xl6IrLowerNodeAsType` 固定目标 TypeId，源码 `null` 不会独立 lowering；与 null 的等值比较也只消费 operand 的 `NullAbi`。`nullable<T>` 在 `T` 已有 sentinel 表示时复用完全相同的 C/ownership/null ABI，具体 class 到 `nullable<class>` 的静态扩宽由 verifier 可见的 `VALUE` 表达。`var` 与 `nullable<class>` 的 `??` 现在也统一降为 `NULL_TEST/BRANCH` lazy CFG：左值先成为 owned temporary，present 分支以显式 `RETAIN` 转移结果所有权，nullable class 另有同表示 `NULLABLE_UNWRAP` 语义投影；两个分支都显式 `RELEASE` 左 temporary，fallback 只在空值分支求值。源码 literal `null ?? value` 在 lowering 时按已解析结果 TypeId 直接选择 fallback，不生成无类型 null SSA。直接 storage 的 `?=` 同样不再借用算术 compound opcode：xvalue-sentinel 目标降为 `LOAD/NULL_TEST/BRANCH`，只在 null 分支 lowering RHS，再以显式 `RELEASE/STORE` 替换 owner；`NullAbi=NONE` 的目标静态消除整个 RHS 与分支。旧 Cgen 对 nullable class 会先求值 RHS 再测试，因此这一惰性合同是 XLIR-only 运行门，而不是伪装成跨后端差分。普通 trivial scalar 的 nullable 则使用独立 `TAGGED_NULLABLE` ABI：类型池给每个 TypeId 稳定的 `struct { bool hasValue; T value; }` C 名称，`NULLABLE_WRAP` 构造 present 值，`NULLABLE_UNWRAP` 只出现在 `NULL_TEST` 已分出的非空 CFG 边；`??` 在 fallback 块中才求值右侧并以 `VALUE` 合并。类型池还以 `errorResultCName` 固化 MAY_RAISE carrier；Cgen 只消费该名称并使用统一 compound-zero，不再按 `cAbi` 重建 carrier/失败值。`bool/int/uint64/float` 的默认 null、赋值、比较、内部参数/返回、公开普通/may-raise export 和 coalesce 已由文本/JSON、严格 C、宿主与 legacy 差分固定。trivial tagged nullable 已进一步覆盖 immutable closure capture/result、mutable capture cell、async result/frame、typed continuation coalesce 和 erased class field 生命周期。class 字段以 `tagged-null` 固定默认语义，以 `xvalue-tagged-nullable-box` / `xvalue-object-get-literal-tagged-nullable` 固定 erased storage 边界；Cgen 只按 verified TypeId 选择对应 helper。erased collection 边界现已进一步接纳 tagged nullable、nullable string、nullable closure 与 nullable protocol：容器写入按精确物理表示 boxing，索引/迭代先取得 borrowed canonical xvalue，再由 TypeId 专属 projector 复制成独立 nullable owner；安全 array/list 缺项先归一为 xvalue null。`XVALUE_RETAIN` 被严格限定为同 TypeId 的所有权操作，任何 nominal/nullable 到动态类型的静态擦除都必须紧随一条显式 `VALUE`。其他复杂 product 组合仍需独立所有权/边界 ABI，不能因这些 nullable 分区已完成而默认接纳。

索引 `?=` 也已经成为同一层的一等控制流语义。lowering 只接纳稳定的 xvalue array/list receiver 与 i64 key，在前驱块各求值一次，以 `XVALUE_ARRAY_AT_I64` / `XVALUE_INT_MAP_AT_I64` 取得 borrowed current；`NULL_TEST/BRANCH` 之后只在空值块继续。该分支先用 `XVALUE_RETAIN` 和 compiler temporary 锚定 receiver，再 lowering 已证明 no-raise 的 RHS，替换值交给 consuming `*_SET_I64_NEW`，随后 `TRAP_IF_FALSE` 并显式释放 receiver anchor。borrowed current 仅供 null-test，不产生 store/release；key 的 SSA 值直接跨边复用。临时 receiver 与可能逃逸异常的 RHS 在专用 cleanup edge ABI 明确前继续精确拒绝；Cgen 不读取 index AST，也不自行决定惰性或所有权。

语义已归一的 `while` 已降为 preheader、condition、body、exit 四个 CFG 块；`do while` 已降为 preheader、body、condition、exit，保证 body 至少执行一次。`break` 与 `continue` 依据前端确定的目标 region 直接变为到 exit/condition 的跳转。range-for 不再由 lowering 根据 target 数量猜测：Typed `XL6_IrLoopAbi` 明确区分 `RANGE_I64_VALUE` 与 `RANGE_I64_INDEX_VALUE`，后者固定第一 target 为从 0 开始的 ordinal、第二 target 为当前 progression value。两种布局都消费 verified RANGE descriptor，将 start/end/step 各求值一次并保存为 storage，按 step 正负分流为 `<` 或 `>` 条件块；continue 固定跳到 increment 块，并同时递增 value 与 ordinal。三参数 range 的 loop 节点独立携带 `loopErrorAbi=MAY_RAISE`：CFG 先判 `step > 0`、再判 `step < 0`，零值进入 owned message `ERROR_RAISE("RangeError")` 与显式 exception transfer；descriptor 本身仍是 `NO_RAISE`，异常效应不会错误归给 builtin call。基础 managed-string TypeId 与 bool/error 一样由 Typed XLIR 必备词汇提供，因此无 string 字面量的程序也能表达该失败边。Cgen 只看到 storage、算术、branch、error 与 jump 指令，不观察 iterable、target layout 或 loop ABI。range increment 在 int64 极值处的溢出策略仍须形成明确 IR 规则；当前 range-for 门只覆盖不会溢出的 progression，不能把该边界冒充为已经证明。

storage-backed iteration 的首个资源 ABI 也已经进入同一模型。Typed `XVALUE_ARRAY_I64_VALUE / XVALUE_ARRAY_I64_INDEX_VALUE` 接纳精确 `array<int>`，单 target 是元素，双 target 固定为零基 ordinal/元素。稳定命名 iterator 携带 `XVALUE_SHALLOW_SNAPSHOT_RELEASE + iterator TypeId`，在 preheader 做一次 `XVALUE_CLONE`；owned rvalue iterator 则携带独立的 `XVALUE_OWNED_ITERATOR_RELEASE + TypeId`，把数组字面量或函数返回值直接转移进 compiler temporary，绝不多做 clone。两种获取方式在此后共享 `XVALUE_COUNT`、显式 i64 position storage、`XVALUE_ARRAY_AT_I64_INTEGER` 和 condition/body/increment/exit CFG。`loopErrorAbi=NO_RAISE` 只描述获取后的 count/projection 等迭代机械动作，不会把 iterator 求值或循环体自身的调用/异常效应误写成 no-raise。命名源数组重绑不会改变本轮遍历；continue 保持资源并进入 increment，当前循环的 break 与自然结束汇入唯一 exit block，由那里显式 `RELEASE` 快照或 transferred iterator。

iterator resource 的具体 storage 是 lowering 才能创建的，因此它不伪装成前端已有的 lexical cleanup action，而由一等 active LoopResource 栈物化。栈项记录 acquisition/release ABI、TypeId、storage symbol、loop depth 与激活时的 exception depth，并区分跨迭代存活的 hidden cursor 与仅在当前 iteration 存活的 target owner。直接 return 在词法局部逆序清理后释放所有跨越的资源；异常进入激活后建立的循环内 handler 时保持当前 key/item 供 catch 观察，进入更外层 handler 或未处理 throw 时先完成内层词法清理、再按 item、key、cursor 的逆 acquisition 顺序释放；continue、break 和自然 fallthrough 会结束当前 iteration，跨 `finally` 的 return、throw、break/continue 则保持资源直到 finally 真正完成，并只在最终 continuation 越过相应边界时释放。return 若读取当前 managed key/item，会先产生独立 owned copy，不能把原 storage move 空而破坏 finally 的可见语义。专用门同时固定命名 clone、literal/returned owned transfer、本地 typed catch、向外 catch、exceptional finally、return-through-finally、break-through-finally 的 CFG 与运行语义，Cgen 仍只机械渲染 runtime op、`RELEASE/JUMP/RETURN/THROW`。循环体中的普通调用、可抛调用、throw 与 return 因而不再被 iterator admission 一刀切拒绝；await/yield 等若其外围 continuation/frame ABI 尚不能承载该资源，则由对应 lowering 边界精确拒绝。非容器仍无 iterator ABI。generator/protocol/async iterator 以及超大 cardinality 到 i64 的边界策略仍须分别定义元素、顺序、失败、资源与 suspension ABI。

xvalue-backed collection 不再为每个容器复制一套循环 CFG。通用路径以 compiler-only `VALUE_ITERATOR<CollectionTypeId>` 表示唯一 owned cursor；它与源 collection 的精确 TypeId 绑定，并通过 `XVALUE_ITERATOR_CREATE / ADVANCE / ITEM_{I64,U64,BOOL,F64,STRING_COPY} / KEY_{I64,STRING_COPY}` 和 `XVALUE_ITERATOR_DESTROY` 表达 XRT 的稳定 backing snapshot。`LoopAbi` 明确区分 value、key-only 与 key/value 三种目标形状，`LoopKeyAbi`、`LoopItemAbi` 和各自的 `LoopTargetLifetimeAbi` 则正交固定 projection 与替换/释放规则：list 的 i64/u64/bool/f64/string、set 的 i64/bool/f64/string，以及 dict<string, T> 的 i64/u64/bool/f64/string item 都共享同一控制流、resource ABI 和 C helper；list key 独立投影 i64，dict 单 target 固定为 string key-only，双 target 固定 string key/item，二者都复制 borrowed runtime text 为 owned managed string。string item 与 dict key 作为 per-iteration owner，在普通 fallthrough、continue、break、return、throw 以及 finally continuation 的正确边上显式释放；dict<string,string> 以 cursor→key→item 获取、item→key→cursor 释放。set 的双 target 在 Typed IR 明确拒绝，绝不臆造 ordinal/key；当前 XRT set 校验也拒绝后来追加的 `XVALUE_UINT` tag，所以 `set<uint64>` 在 Typed IR 保持明确未接纳，而不是由 getter 做宽松转换。命名 source 和 owned rvalue 都只求值一次，创建 cursor 后不再依赖源变量。Cgen 不识别容器名、不回查 loop AST，也不补资源边。managed string 的 UTF-8 迭代、开放动态 `var` 的 checked iterator、xvalue-retain item 及 nullable item projector 均已拥有独立 ABI；剩余 generator、protocol/async iterator 仍需要各自的顺序、失败与 suspension 契约。typed list 原地写入已经由独立 `IndexWriteAbi` 固定为 int-map set，与 iterator ABI 保持正交。

每个 `return`、`throw`、`break` 和 `continue` 在 typed XLIR 中都有唯一的 `XL6_IrExitPlan`：它固定退出类别、目标 scope/region 以及由 XLIR 所有的 cleanup action slice。每个 cleanup action 还显式记录拥有该退出计划的 callable identity：普通/async 函数使用 `FUNC_DECL`，模块初始化使用 module root；泛型具体化必须同时重映射 owner、origin 与 storage symbol。lowered 模块可以保留只供检查的 dormant 泛型模板 action，但只有 owner 出现在 function arena 的 action 才是可执行计划并必须 materialize。标量退出的 action slice 显式为空；`RELEASE / DROP` action 在 lowering 中必须先按已验证的 storage ABI 放入终结 CFG 边，随后才发射终结指令。同步标量错误 ABI 已进入 C ABI；未迁移的 ownership instruction 或可执行但未 materialize 的 cleanup action 仍给出明确诊断，绝不回读 AST 或模拟 legacy 的异常标签协议。 mutable capture 把 lexical storage 提升为 cell 时，lowering 同步把对应 `RELEASE` action 规范化为 cell TypeId 的 `CELL_RELEASE`；因此验证器只检查 exact action/instruction ABI，Cgen 不再维护 cleanup materialization 扫描。

`future<T>` 是独立的 `RUNTIME_FUTURE_PTR` 受管句柄 ABI：其 `T` 保留在 canonical type 参数中，局部 scope 退出发射 `RELEASE`，Cgen 机械发射 `xrtFutureDestroy`。`await future<bool|int|uint64|type|float|string|var>` 已迁移：lowering 接受已解析的借用 future 存储，或已解析的直接 future 调用结果；后者先写入 synthetic future storage、再 `LOAD → AWAIT → RELEASE`，所以临时句柄在错误测试之前也有固定释放边。两种形态都验证 `future` 的唯一 type parameter 与 await 结果 ABI 完全一致，再产生 `AWAIT → ERROR_TEST → success / cleanup + THROW` CFG。`AWAIT` 从 XRT 取得已完成值；XRT 失败、取消、关闭或无效句柄均写入既有错误槽，`ERROR_TEST` 读取该槽进入已编号的失败边。Cgen 仅依据 `AWAIT` 的结果 C ABI 发射对应 helper，绝不读取 AST、future 类型名或调用上下文。

source `async func` 已进入 XLIR：`INTERNAL_ASYNC` target 固定公共 `future<T>`、worker 结果、launch/worker error ABI 与 dispatcher；调用点只能降为 `ASYNC_START`。无挂起函数使用 verified ready-Future 或无 machine 的 task-pool ABI；含挂起函数仍以 task-pool 作为公开 launch 角色，但 lowering 必须建立唯一 `ANALYZED` machine，launcher 只能直接执行该 machine。不存在按表达式形状选择的 continuation dispatcher。

旧 source-shape planner 与 `AsyncContinuation/AsyncFrameSlot/AsyncResumeOp` 类型、arena、验证、文本/JSON dump 已从实现中物理删除。所有 callback、frame、分支、错误边和 ownership 都从 Lowered CFG 经统一 suspension split 与 liveness 分析进入 `AsyncMachine`；因此参数/局部/立即数、string/xvalue、extern/parameter Future、单项/多项 capture 不会形成第二套语义。

这条 CFG 路径现已形成可由后端直接消费的状态机骨架。所有含 `AWAIT` 的 `ASYNC_WORKER` 都会生成一个 `XL6_IrAsyncMachine`。分析只读取 Lowered XLIR 的 block successor、instruction operand/result/target storage：每个 `AWAIT` 对应连续的 `XL6_IrAsyncSuspension`，固定 input/result ABI、success/failure edge、稳定 stateId、词法 scope/region，以及来自唯一 `THROW` ExitPlan 的 failure cleanup transition；标准反向数据流同时计算 SSA value 与 mutable storage 的 live-in/live-out。原始 block 在每个 `AWAIT` 后切成 `EXECUTE / SUSPEND` machine block，`LAUNCH / RESUME_SUCCESS / RESUME_FAILURE` entry 明确指向恢复后的第一条 Lowered 指令，Cgen 不再自行寻找回调落点。跨挂起源先按 value/storage identity 合并为机器级唯一 `XL6_IrAsyncMachineFrameSlot` layout，由它独占 TypeId、parameter provenance、`LAUNCH_PARAMETER / FRAME_STORAGE / SSA_SPILL` residence、精确 state range 与 managed init bit；`transferAbi` 明确区分 frame-storage 的 `NONE`、借用源的 `COPY/RETAIN` 和 owned SSA 的 `MOVE_OWNED`，`destroyAbi` 独立固定逆操作。拥有型透明 SSA 别名按 canonical owner 合并物理存储；每个 suspension 的连续 `XL6_IrAsyncLiveSlot` slice 独立保存 frameSlot 与所需视图 ValueId。机器拥有规范顺序的 `XL6_IrAsyncFrameAction` arena：prologue 做参数初始化与 storage 绑定，每个实际到达的状态执行 resident spill/install，再由显式 project 恢复 canonical resident 与活跃视图；逐消费指令的 transfer slice 指定准确 operand ValueId 与交付时机，YIELD 仅在成功提交后清除 owner，其他 consuming 指令在进入其消费 ABI 前移交；epilogue 按 frame 逆序执行 guarded destroy。旧单点 `takeInstruction` 已移除。trivial/string/xvalue/future/closure/cell/error 各有明确 lifetime，尚无安全 frame 表示的 product/native/iterator 等会把 machine 标成 `BLOCKED_LIVE_ABI`，不会交给 Cgen 猜测。每个 lowered internal-async `CallTarget` 直接持有唯一 `asyncMachine` 索引；typed/no-await target 使用 `SIZE_MAX`，Cgen 不按 symbol 扫描 function 或 machine arena。Verifier 独立重算双向链接、machine/function/dispatch 角色、`AWAIT → ERROR_TEST` 边、唯一 failure ExitPlan、frame source 唯一性、residence/state range/init bit、无 orphan layout/membership/action、slice 顺序以及 transfer/project/destroy 关系，并从指令与 CFG 重新分析精确活跃集合。Cgen 现在直接执行 launch 与每个 suspension 的 resume program：单一 layout activation 在多次挂起间按 live-set 移交，外层 Promise 只在首次嵌套 continuation 时取得引用，string/xvalue/closure 的 copy/retain/move/take/destroy、Future handle、catch 的 error-take/release、带分支 resume CFG 和 tagged-nullable `NULL_TEST/UNWRAP` 都只消费已验证 TypeId 与 action。代表性 admission 门覆盖 48 个 fixture、46 个 analyzed machine 和 60 个 canonical suspension，要求每个 suspension 恰有一个 direct callback；完整已支持 inventory 另覆盖 80 个 fixture、45 个 analyzed machine 和 54 个 canonical suspension，且不得出现 non-analyzed machine、orphaned suspension 或 legacy renderer。两组均固定旧 chain/capture/catch renderer 为零、重复 C 逐字节一致，并通过 `-Werror` 与运行时终态。对 analyzed machine 的 Cgen admission 已改为 fail-closed：任何尚未接纳的 instruction/frame ABI 都给出 `no admitted direct C program ABI` 诊断，绝不静默回退第二套 continuation 语义。旧 continuation 数据模型及其 source-shape selector 已物理删除；文本/JSON dump 只公开 canonical machine、suspension、frame、liveness 和 action 数据。

连续、多分支和多 Future 的状态转换统一由 machine block/entry 表达；每个 suspension 的 success/failure callback 只恢复对应 live-set，并沿同一 CFG 执行 scalar、string、xvalue、closure、error 与 tagged-nullable 指令。frame acquisition、move/take、init bit 和逆序 destroy 都来自 `AsyncMachineFrameSlot/AsyncFrameAction`，注册失败、reject/cancel/close 和正常完成不再由不同 renderer 各自复制资源规则。

旧 `AsyncContinuation/AsyncFrameSlot/AsyncResumeOp` arena、lookup、runtime-needs 推断和 chain/scalar/string/xvalue/catch renderer 均已删除。普通函数与 async machine 的 `CLOSURE_MAKE/CLOSURE_CALL` 共用同一指令 helper；合同门反向禁止这些旧类型、字段、枚举和 JSON 属性重新进入实现。

构建 typed XLIR 时，作用域内的 `MANAGED` 局部声明会按其语义 region 归入每个实际离开该词法块的 exit plan，并以逆声明顺序形成 `RELEASE` slice。兄弟分支、已结束的内层 block、退出点之后的声明均不会进入该 slice。此时它仍是后端前的显式 ownership contract；在完整 runtime ABI 与 `RETAIN/MOVE` lowering 落地前，受管值不会被伪装成可发射 C。

`src/cgen/xl6_ir_cgen.c` 已建立独立的纯 XLIR C emitter。当前覆盖标量、tagged nullable、`string`、`var`、closure 与 `future<T>` ABI、函数原型、基本块、常量、局部 load/store、已规范化的位置参数直接调用、runtime ownership 指令、白名单二元/一元运算、分支、跳转、错误结果 carrier，以及由 machine entry/block/suspension/frame action 驱动的通用 async 状态机。closure make/call 的 C ABI 由同步函数与 async machine 共用指令级 helper；machine Cgen 对未接纳的 verified program fail-closed，不再选择兼容 continuation renderer。它不读取 AST，也不调用旧 Cgen，且已经成为 `cgen`、`run`、`build` 的默认后端；后续语义簇仍必须先扩展 XLIR 与 verifier，不能绕回 compatibility Cgen。发射前 `xl6IrCgenVerifyEmittable` 会完整检查现有 C ABI；`xlcEmitXlirC` 在创建目标文件前调用它，因此“不支持”不会写出半截 C 文件。

Lowered XLIR 现在还显式拥有 callable emission closure。`XL6_IrFunction`、`XL6_IrCallTarget` 与 `XL6_IrClosureTarget` 均携带 `UNREACHABLE / ROOT / TRANSITIVE`：module-init、公开 export 和 process entry 是根；普通/async 调用、closure construction、virtual/protocol implementation row 是传递边，async machine 的 extern input 也来自其 canonical Function 指令。Lowering 在全部函数与 dispatch 表完成后求不动点，verifier 独立重算并要求三张表完全相等。Cgen 不再扫描全模块指令猜测“哪个 body 需要”，只发射非 `UNREACHABLE` 的 extern prototype、函数原型/函数体、closure environment/invoke 和 async launcher；runtime needs 也只扫描可达函数。这既防止未使用标准库 wrapper 引入外部符号，也把 dead-code elimination 的语义边界固定在 IR，而不是堆成后端特例。

task-pool worker 的跨线程错误桥接现已具备固定 ABI：worker 以 XRT task failure 交还自身线程的错误，等待侧 helper 在 `xrtFutureWait` 后读取 `xrtFutureError`，并以 `xrtErrorRef → xrtSetErrorTake` 将错误移入 caller 的错误槽。因此 `AWAIT → ERROR_TEST` 能进入现有 typed catch CFG；scalar、string 与 `var` Future 失败不再被当作默认成功值。

### 2.1 异常 ABI：先固定控制流契约，再迁移 assert/throw/try/await

旧 Cgen 的异常实现不能作为 XLIR 后端的接口：它在每个生成 C 文件中根据 AST 临时拼装 `XL6_Error`、TLS 状态、catch 标签和 finally 的 pending-control 变量。把这套逻辑搬进新 Cgen 只会把语义回溯换一种位置保留下来。

XLIR 的同步异常 ABI 采用“显式错误状态 + 显式 CFG”模型，分三个层级落地：

1. `XL6_IrFunction.errorAbi` 与 `XL6_IrCallTarget.errorAbi` 必须具有前端/XLIR lowering 固定的 `no-raise / may-raise` escaping effect；直接 `assert/throw` 只有越过本函数 CFG 时才标记函数，随后按已解析的内部 `CALL` 做不动点闭包。被 canonical catch 消费的失败仍保留为 call/result CFG，但不会无故扩大外层函数 ABI。Cgen 不得通过函数名、返回值或运行时类型猜测一个调用是否可能失败。
2. lowering 将 `assert`、`throw`、可能失败的 call/await 后续动作展开为明确的 `ERROR_RAISE`、`ERROR_TEST` 和 `BRANCH/JUMP` 指令或等价的 runtime intrinsic。错误 kind、源位置和静态消息是 XLIR 所有的 canonical immediate；动态消息是已验证的受管字符串 operand。成功/失败边都属于 CFG，不能由 Cgen 临时插入 `if (error)`。
3. 每个 `try` region 具有由 XLIR 持有的 handler entry、finally entry、normal continuation 和异常 continuation。当前 `bool/int/float/string/var` 的 `return` 穿越 finally 时使用 typed pending-return storage：lowering 先 `STORE` 并跳到专属 finally CFG，finally 后 `LOAD → RETURN`；受管 `string/var` 在进入 pending storage、转交外层 route 与最终 return 时都以显式 `MOVE` 转移所有权。若外层仍有 finally，则值转交同 ABI 的外层 pending storage，再跳至其 return-finally CFG。`break/continue` 则以 lowering-time finally barrier 解析：每个跨越该屏障的已解析循环目标拥有一个去重后的 finally CFG route，finally 后直接 `JUMP` 到固定目标或外层 route，不需要运行期的 pending-control discriminator。cleanup action 仍在 CFG 边上按 exit plan 物化。尚无该 storage/ownership contract 的 return ABI 必须明确拒绝。Cgen 只发射这些已编号的块与 storage。

当前阶段已将 `may-raise CALL` 展开为 `CALL → ERROR_TEST → success / cleanup + THROW` CFG；调用点的 propagation cleanup plan 与两个目标块均属于 lowered XLIR。标量 `bool/int/float` 的 `may-raise` 函数使用固定的 `{ bool ok; T value; }` C result ABI：`ERROR_RAISE` 将 owned `xerror` 交给 XRT 当前错误槽，`ERROR_TEST` 仅按 result 的 `ok` 字段进入已编号的成功/失败块，`THROW` 返回 `{ false, zero }`。这使 Cgen 无须再临时插入 `if (error)` 或回溯调用语义。

第一段结构化捕获也已落地：无类型的单个 `catch`、无 `finally` 的 `try` 被 lowering 展开为 protected、handler、merge 三个 XLIR CFG 区域。直接 `throw/assert` 与失败 `CALL` 都由前端标注的 target region 跳入同一 handler；handler 的首条指令是 `ERROR_TAKE`，它把 XRT 当前错误槽转为拥有型 `xerror*` catch storage。正常离开 handler 会发射 `RELEASE`；`return`、`throw`、`break/continue` 等非局部退出会先按作用域边界释放活动 catch payload，再执行前端 exit plan 的外层 cleanup。Cgen 只机械发射 `xrtTakeError/xrtErrorFree`，不再解释 catch 语法或猜测错误所有权。

无 catch 的标量 `try/finally` 也已迁移：lowering 为可能失败的 protected 区分别建立 normal-finally 与 exceptional-finally CFG 块。异常边保留 XRT 错误状态，执行同一份已 lowered 的 finally body，最终以 `THROW` 返回既有错误结果；正常边执行 finally 后跳至 merge。标量 `return` 则先存入 compiler-created pending storage，再进入独立的 return-finally CFG，finally 后 load 并 return，因而不会依赖 Cgen 的 pending-control 变量。受保护区已证明 `no-raise` 时，不创建虚假的 exceptional-finally/`THROW` 边，因此函数错误 ABI 不会被无故扩大。

单 catch 加 finally 同样已经是 XLIR CFG：protected 错误进入 `ERROR_TAKE → STORE` handler；protected 正常出口和 handler 正常出口共同进入 normal-finally，handler 在进入该 finally 前释放 payload；handler 内新的异常由活动 payload cleanup 后进入 exceptional-finally，执行 finally 后以 `THROW` 传播。无 payload 参数的 `throw`（rethrow）也已采用 `LOAD → MOVE → ERROR_SET_TAKE`：先把 catch storage 的拥有权显式转给 XRT 错误槽，再走既有 exceptional CFG，因此不会重复释放。protected 或 handler 中的 supported `return` 同样复用 pending-return storage 和 return-finally CFG；受管值以 `MOVE` 交接，嵌套 finally 会显式把同 ABI 的 pending value 转交给外层 route。`break/continue` 在 protected 或 handler 中也复用 finally barrier 的固定 CFG route，并在 handler 路径先释放 payload。每个 lexical `BLOCK` 的普通 fallthrough 都由 lowering 以逆声明顺序发射 direct managed local 的 `RELEASE`；非局部退出仍由语义 exit plan 在 terminator 前清理。因此 finally 中的嵌套 block 和 loop body 也遵守同一 ABI，循环回边不会遗漏受管 local。这一分离保证 handler 不会重新捕获自身的异常，也不需要 Cgen 的 catch 标签或 pending-control 状态。当前仍明确拒绝无 pending storage/ownership contract 的 return ABI。catch binding 的 bare expression 现 lower 为已验证的 borrowed error-observer `DISCARD`：它只发射 `(void)value`，catch storage 仍是唯一 owner 并在 handler exit `RELEASE`；字段、复制、传参等 payload 操作、分类 handler、多 catch、导出函数，以及跨线程、future 和 generator 的 error-result adapter 仍未迁移，必须明确拒绝；后续每个 ABI 都需要先进入 XLIR 再由 emitter 机械发射。

catch payload 成员已开始以独立 runtime intrinsic 迁移：`e.message`、`e.domain`、`e.operation`、`e.data` 和 `e.kind` 分别 materialize 为 owned string；`e.code` 与 `e.systemCode` 分别将借用的 XRT `int32` 规范化为 XLIR i64；`e.cause` 是 `ERROR_CAUSE_REF`，先从 XRT 借用 cause 再显式 retain 为局部存储可拥有的 `xerror*`。`ERROR_RAISE` 会把已验证的 xlang kind 放入 XRT `Data`，kind getter 对原生错误使用固定映射 helper。单一分类 `catch Kind e` 已展开为 `ERROR_TAKE → ERROR_KIND_IS → BRANCH`；`ERROR_KIND_IS` 由固定的 canonical 错误父链判定，因而 `RangeError` 可被 `ValueError` 捕获，不能退化为 immediate kind 字符串相等。匹配路径进入带 catch payload cleanup 的 handler，未匹配路径显式 `MOVE → ERROR_SET_TAKE` 并跳至已解析的外层 exception target；仅在没有外层 target 时才 `THROW`。单一和多 handler 加 finally 均已具备同一 ABI：predicate dispatcher 的每条匹配路径只将 payload `MOVE` 给一个 binding 后进入 normal-finally；无匹配路径则 `MOVE → ERROR_SET_TAKE → exceptional-finally`，finally 结束再按已解析的外层 exception target 传播。因此 finally 不是 Cgen 重新推断的清理补丁。跨线程 error adapter 仍必须明确拒绝。

分类 handler 已进一步扩展为统一 dispatcher：`ERROR_TAKE` 的唯一 owner 先存入 compiler-created dispatch storage；每个 typed handler 都有独立的 `ERROR_KIND_IS` test block 和 body block，失败边保持该 storage 并进入下一测试，命中 body 才通过 `MOVE` 转交到对应 catch binding；最后的 catch-all 直接跳入 body，没有 catch-all 的末端则 `MOVE → ERROR_SET_TAKE` 后跳到外层 exception target 或在最外层 `THROW`。当附带 finally 时，normal handler body 和 protected normal path 共用 normal-finally，未匹配及 handler 异常共用 exceptional-finally，return/break/continue 仍使用原有 typed pending-exit/finally barrier ABI。Cgen 只发射这些块和 storage。

异常迁移的质量门必须同时覆盖：

- assert 成功、失败、静态/动态消息和副作用只执行一次；
- throw 到同函数 catch、跨调用传播、rethrow，以及 finally 下的 return/break/continue；
- 受管局部在 normal、catch、finally 和未捕获边均恰好清理一次；
- 连续两次 C 发射逐字节一致，且生成 C 不包含 AST/HIR 指针、类型名分类或错误标签的后端临时推导。

对尚未具备指令、验证器和 runtime ABI 的异常语义，XLIR lowering 必须明确拒绝，绝不能回退其他表示或把语言异常降成无诊断的进程终止。

二元/一元运算在 lowering 时从源拼写归一为 `XL6_IrOpcode`；C emitter 只依据 instruction opcode 映射 C token，不读取节点文本来决定运算语义。

运算的 operand/result ABI 同样在 XLIR verifier 固定：算术只能接受同 ABI 的 `I64/F64` 并返回同类型，位运算只能是 `I64`，比较返回 `BOOL`，一元正负/逻辑非/位非各有对应 ABI。任何依赖 C 整数提升、浮点提升或比较结果隐式转换的指令都会在 emitter 之前被拒绝。

常量同样在 lowering 固定为 `CONST.immediateText`；该 payload 由指令独占，独立于 XLIR 节点文本。C emitter 直接发射 immediate，不通过 `origin` 回查节点文本。

调用结果 ABI 由 `CALL.result` 固定：已验证为 no-raise `void` 的直接调用不生成 `ValueId`；may-raise `void` 调用产生只供 `ERROR_TEST` 使用的控制结果，不作为语言表达式值。有返回值的调用必须携带已验证的物理结果值。后端不再根据 C 函数文本或调用上下文猜测是否赋值。

调用的语义绑定由独立 `CallSite` 固定，而不是由 lowering 或 Cgen 重放参数匹配。每个 site
拥有 call kind、target symbol、result TypeId、可选 receiver，以及连续的 `CallArgument`
slice。参数 arena 保持 ABI parameter index 顺序；每项另有独立、稠密的 `evaluationIndex`，
记录源码显式实参的求值顺序，随后才按声明顺序执行省略的默认参数。命名参数改变绑定位置，
不改变显式表达式的执行顺序。`receiverEvaluationIndex` 表示 receiver 之前执行几个实参，
无 receiver 时为 `SIZE_MAX`（JSON 为 null）。显式来源、默认表达式、命名身份和 byref/out
wrapper 都有独立验证，交换表达式及其求值序号不能绕过形参身份检查。

普通函数、class constructor 和协议调用消费同一个绑定模型。`dispatchKind` 区分 direct、
class slot、protocol slot 和 callable value；`dispatchSlot` 只在对应的域中解释，不能混用
class/protocol 数字槽或在 Lowering 中按声明重新搜索 witness。`CallTarget` 或已选 dispatch
slot 拥有物理参数 TypeId、参数模式/所有权 ABI、结果及异常 ABI；verifier 在 C 输出前证明
其与 site 一致。Lowering 按 evaluation 顺序取得和保护 operand，按 parameter 顺序组装 CALL，
调用完成后按实际取得顺序逆序清理，不能按 ABI 参数位置倒序释放。

CallSite 不局限于源码 `CALL` 节点。class unary/binary、membership、index、缺失 member
和 callable syntax 只要在语义阶段选中了具体 magic method，就进入同一个 arena。
unary/binary/index 的显式表达式直接成为 CallArgument；`__field__(string)` 的 member 名不是
AST 子表达式，因此使用 `IMPLICIT_STRING` binding 保存 canonical UTF-8 payload。lowering 根据
目标 string parameter 的 borrow ABI 物化 owned managed string、建立 temporary 并对称 release；
非 member `__call__` 的 callee expression 本身是 receiver。由此 receiver 顺序、参数来源、
重载目标和生命周期都在 Cgen 前可验证，emitter 不允许搜索 `__contains__/__item__/__field__/__call__`
或根据节点拼写补齐调用。泛型 class syntax 在 lowering 前按 concrete receiver 单态化；Typed
模板目标由 symbol-node 表验证，Lowered synthetic symbol 则必须由 `CallTarget.declaration`
反向证明其 concrete callable，二者不混用过期模板返回类型。

索引写同样是调用，而不是特殊存储补丁。简单 `receiver[index...] = value` 的语义目标
保存在 ASSIGN 节点，CallSite 固定 `receiver`；其 ABI 参数为全部 index 加最终 value，
求值顺序则是 RHS value、receiver、各 index。`in` 的求值顺序是左 operand 再右 receiver；
目标 `__setItem__` 必须具有完全相同的固定参数数目和 void return ABI。左侧 INDEX 只承载
receiver/key tuple，不附加或执行 `__item__` getter。lowering 因而直接生成已验证的 call，
不得根据 lhs 类型把它重新分类成 xvalue array/object set。


索引属性的 getter 以整个 INDEX 为 CallSite origin；其中 MEMBER 只是无值、无类型的
选择器。返回容器的普通 getter 则保留自己的 MEMBER CallSite，外层 INDEX 是另一项
容器操作，不能把两者折叠为同一个调用。动态字段和 magic-method 索引同样保留各自
明确的 call kind，不因为拥有 CallSite 就被当作 property accessor。

属性复合赋值的 setter CallSite 通过 `preparedGetter` 指向同一事务中的 getter site。
两者具有相同 receiver 和 key binding，receiver/key 按源码顺序只取得一次；最后一个
setter argument 使用 `PROPERTY_UPDATE`，其 expression 指向整个更新操作，而不是 RHS。
它代表 getter 结果经该操作的已验证 opcode/运算符目标与 RHS 计算后的值。执行顺序固定为
receiver、各 key、getter、RHS/运算、setter；这与简单赋值的 RHS-first 明确区分。

`PropertyAccessorSignature` 是已有 CallTarget/class slot/protocol slot 的无分配视图，
不构成新的 IR 层。receiver、显式参数类型及所有权模式、结果、异常契约均来自已选目标。
class/protocol 事务共享求值、参数转交、结果回滚和调用完成路径；byval key 为每次调用
单独保留拥有型引用，事务持有的原始 anchor 持续到 setter 及清理结束。原始调用错误优先于
后续清理错误，setter 完成前不得提前丢弃 receiver/key，await 恢复后也不得重新求值它们。
Verifier 检查 getter/setter 绑定一致性、computed-value 来源及 concrete 物理签名；
未处理的 property update 必须使 Lowering 失败，不能产生遗漏写入的成功模块。

variadic 不是 emitter 的特殊语法补丁。`array_vararg` 与 `dict_vararg` 的每个 tail item 在
CallSite 中分别标记为 `ARRAY_VARARG_ITEM` / `DICT_VARARG_ITEM`；lowering 在首个 tail item
求值前惰性建立并保护 array/object aggregate，随后按 evaluationIndex 交错处理元素 boxing、
写入及其它固定/默认参数。唯一 aggregate 作为普通 borrowed 物理参数调用，其 temporary
参与相同的失败回滚和逆序清理；空 tail 在参数求值后形成显式空 aggregate。native `c_vararg`
则不进入 CallTarget 的物理参数数组：target 记录 `variadic=C`，CALL 保留固定前缀之后的动态
typed operands，verifier 只接纳具有明确 C ABI 的 scalar、managed string、native handle 或
raw pointer，并拒绝 erased runtime handle。Cgen 仅依据该 target 在 extern prototype 追加
`...` 并逐项发射已有 operand；它不做 C 默认参数提升，也不从参数名或源类型重建规则。
文本和 JSON dump 同时公开 CallTarget、CallSite、CallArgument、固定参数数目及 variadic ABI，
使参数绑定和物理调用形状可以在生成 C 之前独立审计。

没有源码 callable declaration 的语言操作不再伪装成“未解析普通调用”。Typed XLIR 以独立
`IntrinsicCall` arena 固定 `CHECKED_INTEGER_CONVERT / STRING_CONVERT /
SCALAR_CONVERT / TRIVIAL_IDENTITY_CONVERT / COPY / CLONE / ARRAY_APPEND / RANGE / XCALL`
的语义身份；每项同时
拥有 result TypeId、`value/owned/descriptor` result ABI、`no-raise/may-raise` effect、可选 receiver，
以及连续的 `IntrinsicOperand` slice。operand 明确标记 `value/borrow/consume`，受检整数还携带
目标宽度与符号性。callee/member 拼写只允许在构建 Typed XLIR 时使用一次，verifier 随后证明
identity、operand type/ownership、receiver、result 和 effect 一致。lowering 只按 enum 与 ABI
分派，`range` descriptor 只能由规范化 for-loop 消费；Cgen 完全不读取该 arena，只接收它已经
降成的 CFG、value、scalar-convert 和 runtime-call 指令。精确同 TypeId、trivial ownership、
no-raise 的类型转换（当前关键路径是 stdlib `BufferIter` 的 `point(point)`）使用独立 identity；
Typed verifier 同时证明 callee 类型名、source/result TypeId 和 value ABI，lowering 将其发射为显式
`VALUE`。这不会放宽 foreign-scalar 的“至少一侧为 C scalar”约束，也不会让 Cgen 消除或猜测调用。
identity 的建立不等于接纳该 ABI：例如不支持的 append receiver 或 xcall signature 仍先保留
`ARRAY_APPEND/XCALL` 身份，并显式标记 `admission=unsupported-abi`。Typed verifier 只证明这条
身份事实及其内部结构合法；XLIR lowering 只消费
`admission=verified`，并按 kind 给出精确拒绝原因，不得因当前不可发射而退化成“无直接调用目标”。
当同名 `int` 同时覆盖 foreign integer cast 与可能失败的 float/xvalue
conversion 时，Typed XLIR 依据已解析 source/result C ABI 选择二者之一，而不是把歧义留给 lowering。

泛型单态化复制的是完整 typed IR 语义切片，而不只是节点正文。clone map 必须同步替换
CallSite 的 origin、receiver、result TypeId、target symbol 和每个 CallArgument expression；
无 expression 的 `IMPLICIT_STRING` 保留 payload 并继续使用 INVALID node identity，不能被 clone map
误报为丢失节点；
同一 clone map 也复制 `IntrinsicCall/IntrinsicOperand` slice，并替换 origin、receiver、result/
operand TypeId 与 expression。具体化后的 operand ownership ABI 按具体 TypeId 重新固定，因此
类型参数不会让 scalar specialization 错误继承 managed borrow 约定；准入状态也只依据替换后的
identity/type/receiver ABI 重新计算，不会继承模板上的过期结论；
调用模板的外部 site 则在实例 symbol 确定时显式 retarget。克隆后的 slice 追加到连续 arena，
原模板 site 保持不变，verifier 对两者分别校验。这样实例 body 内的固定/default/named 调用
仍由自己的规范 site 驱动，不会因为节点已经克隆而回退到源码实参列表。

函数返回 ABI 由 `XL6_IrFunction.returnType` 固定：无返回类型的函数发射为 C `void`，其 `RETURN` 指令必须没有 operand；带返回值函数必须恰有一个 operand。该一致性由 emitter 检查，不能用 Cgen 的默认返回补齐。

`XL6_IrStorage` 为每个可寻址符号固定 `GLOBAL / EXTERN_GLOBAL / LOCAL / PARAMETER / CAPTURE / TEMPORARY`、存储 ABI、声明节点及所属函数。C emitter 只按该表声明和访问 C 存储，不再通过 `STORE` 的先后顺序或 symbol 的数值猜测作用域；没有显式存储 ABI 的 load/store 会在 verifier 阶段失败。外部/package 变量声明虽然把语义 symbol 放在名称子节点，typed XLIR 的 canonical symbol index 仍必须指回真正的 `VALUE_DECL`；后续 NAME 的 storage type 必须与该 binding 完全一致，不能把 `var` 或 initializer 类型差异留给 lowering/Cgen 猜测。

每个 `XL6_IrType` 同时携带前端固定的 `XL6_IrCAbi`。C emitter 只将 `I64 / BOOL / F64 / VOID` 等 ABI category 机械映射为 C token；它不按类型名分类，也不根据源码或目标表达式推断 ABI。不能映射的 category 会在可发射性预检中失败。

名义 class/protocol/enum/union TypeId 还携带 canonical `nominalSymbol`。该 symbol 必须精确解析到
同一 XLIR 的类型声明；尤其 pure native-handle class 不创建 xvalue `ClassLayout`，但仍能由
nominal identity 得到唯一 `void*` receiver TypeId。native factory 固定 owned/trivial handle 结果，
extern method 与内部 wrapper 的 parameter 0 固定为 borrow。lowering 不得通过类名比较或搜索某个
方法体是否碰巧出现 `self` 来恢复 receiver，Cgen 也不参与该选择。

pure-native class 声明的 `native point` 字段也不是 xvalue object slot。Typed XLIR 为每个合法读取建立
`NativeHandleProject`，直接记录 member origin、receiver、nominal owner TypeId、raw-point result TypeId
与 exact field declaration；verifier 要求每个 owner-handle/raw-point member 恰有一个合同。泛型克隆
同步替换 node/TypeId/declaration，lowering 只消费合同并生成 `NATIVE_HANDLE_PROJECT`，Cgen 机械发射
已验证的 `void*` 赋值，既不读取该 arena，也不识别 `handle` 字段名。

同一规则覆盖 concrete xvalue class 的 `create/get/let/func/async func`：receiver 是否存在由
声明种类、`static` 标志和所属 class 决定，精确 TypeId 只取自该 class 的 canonical layout/nominal
identity。函数体中若有 typed `self`，它只提供 parameter storage symbol，并且必须与 canonical
receiver TypeId 完全一致；函数体不引用 `self` 时，lowering 创建 synthetic parameter storage。
CallTarget 和 Function 因而始终共享 slot 0 的 `BORROW` ABI，构造器、getter、setter 与实例方法
不会再因源码使用差异改变物理签名。专用文本/JSON、严格 C 与实际运行门同时固定该契约。

raw `point` 到 pure native-handle class 的构造不是普通 C cast，也不是 xvalue class construction。
Typed XLIR 以独立 `NativeHandleBind` 固定 exact nominal target、source TypeId、可选 source storage、
destroy target 和 `BORROW|TAKE` ABI：没有 destroy 的 wrapper 只借用原始句柄；有 destroy 的 wrapper
取得唯一所有权。TAKE lvalue 在 lowered XLIR 中紧随 bind 发射 typed `NULL` 与 `STORE`，因此源不再
持有句柄；TAKE rvalue 若被 borrowed receiver/parameter 消费，则先物化为 `TEMPORARY` owner，调用后
沿显式 `NULL_TEST -> destroy-call -> merge` CFG 清理。destroy-backed native 参数本身固定为 borrow，
避免仅因 C 表示是 `void*` 而复制唯一资源。泛型单态化必须连同这张 bind arena 克隆并替换节点、类型、
symbol；Cgen 只把 `NATIVE_HANDLE_BIND` 机械发射为已经验证的 `void*` 值传递，不得搜索 class、destroy
方法或源表达式来决定转移、清空和析构。

值声明的初始化同样是一等 typed IR，而不是 lowering/Cgen 按 C ABI 临时选择的默认值。
`ValueInitializer` 以 declaration 为唯一 identity，固定目标签名 TypeId、可选 explicit expression 与
`EXPLICIT / SCALAR_ZERO / STRING_EMPTY / NULL_SENTINEL / XVALUE_NULL /
XVALUE_CONTAINER_EMPTY / CLASS_DEFAULT / CLASS_NULL` ABI。verifier 要求每个当前可
表达的声明恰有一个合同，并证明 expression 是该声明的 canonical initializer、default ABI 与 TypeId
完全一致；文本/JSON dump 可独立审计。lowering 据此生成 canonical `CONST`、string-dup runtime op、
typed `NULL`、xvalue-null 或精确 empty-container runtime op 和 `STORE`，不再扫描声明 children；泛型单态化连同 declaration、
expression、concrete type 与 ABI 一起克隆。Cgen 不允许读取 `ValueInitializer`，只看到这些 lowered
指令。当前 scalar、managed string、pointer sentinel、dynamic/nullable xvalue，以及无显式 `create` 的 concrete
xvalue class 以及 `array/list/dict/set` 已有明确默认值；容器 initializer 直接携带
`array-new/int-map-new/object-new/set-new` 中唯一的零 operand owned runtime op，lowering 不再识别类型名。
`CLASS_DEFAULT` 仅用于无显式 `create` 的 exact class layout，并复用同一个默认 class construction
lowering；`CLASS_NULL` 则固定带显式构造器的 nominal class 模块存储默认语义，lowering 明确物化
owned xvalue-null，绝不调用构造器。前者已覆盖 scalar-zero 与 string-empty 字段的有序
boxing/temporary release。嵌套 class 字段、特殊容器、tagged product 等仍必须先拥有自己的 construction ABI 才能接纳，
不得统一伪装为 `NULL`、`{0}` 或依赖 C 的静态零填充。

语言 package export 与 C data export 是两个初始化域。无显式 C linkage 文本的 `export var/const`
仍是 module GLOBAL，Typed XLIR 为其建立普通 `ValueInitializer`，module-init 显式求值并 `STORE`；
`export "c_name"` 才跳过该 arena 并进入 `GlobalExport` 的 canonical static initializer。这个分类发生在
Typed XLIR，lowering/Cgen 不允许再用 `export` 关键字猜测初始化时机。

显式 `create` 不改变字段声明合同。xvalue class 的构造顺序固定为：写入 concrete runtime tag，
初始化 inherited layout/prefix，按 `ClassField.initializerAbi` 初始化本类字段，最后调用 `create`
Function。own-field slice 复用默认构造器的 verified explicit/scalar-zero/string-empty boxing 与
`object-set-new` 路径；`create` body 可以覆盖已有字段，但不能决定字段 storage 是否存在。这样默认值、
显式字段表达式和 body 赋值的先后完全出现在 lowered CFG，Cgen 不以 constructor 语法补零。

class 字段槽与局部 class 默认值是不同合同：局部 concrete class 可以调用其默认构造器，class 内的
concrete/递归引用字段则先以 `XVALUE_NULL` 建立 retained 空槽，避免 `Node.next: Node` 的无限递归。
若字段类型是带 `destroy` 的 native class，`ClassField.nativeDestroySymbol` 保存精确 extern target；
只有可转移的 rvalue/move 能经 `XVALUE_NATIVE_HANDLE_TAKE` 装箱，XRT handle value 接管资源并在失败
路径调用同一 target，Cgen 不搜索 native 类型名或 destroy 成员。class storage 自身使用独立
`XVALUE_CLASS_NEW`，机械映射到保持声明顺序访问、按逆声明顺序释放 owned values 的 XRT class-object
构造器；普通 dict/object 继续使用 `XVALUE_OBJECT_NEW`，容器析构顺序不受 class 生命周期语义污染。

类型还携带 `XL6_IrOwnershipAbi`。lowering 按该字段确定值是否需要显式所有权指令与退出 cleanup；不再以 `string`、`class` 等类型名临时分类。当前 scalar C ABI 是 `TRIVIAL`，其余尚未具备 C runtime ABI 的类型明确为 `MANAGED`，因此它们必须通过后续的 ownership lowering，不能被 emitter 静默当成普通 C 值。

多返回声明自身也必须补全类型图：lowering 按声明的有序返回 TypeId 调用 `xl6IrTypeInternTuple`，不再依赖某个调用表达式先偶然驻留 tuple；因此未调用或仅由 import 引入的声明不会改变 ABI 是否可用。只要每个元素已有非 void、非嵌套 product 的 C ABI，tuple 就使用按 TypeId 唯一命名的 `PRODUCT` struct；全 trivial 元素使 aggregate 为 `TRIVIAL`，含 string/xvalue/error/future/closure 等元素则为 `MANAGED`。后者不是普通 struct copy：`PRODUCT_MAKE` 取得 owned 元素，`PRODUCT_TAKE` 将一个托管字段移出并失效该字段，`PRODUCT_DROP` 销毁 `_` 对应的未绑定字段，`PRODUCT_GET` 只允许 trivial 字段。Verifier 对每个 managed-product producer 证明二选一：整包经 `RETURN` 转移，或每个托管字段恰好 take/drop 一次；禁止遗漏、重复消费和整包/逐字段混用。Cgen 只据此发射字段赋值、置空和对应 XRT release。严格运行与 legacy 差分已覆盖 managed string 的覆盖赋值、双字段 take 和通配符 drop；may-raise/export product 仍需独立的边界 carrier，不能被这个内部 ABI 顺带放行。

XLIR 的具体类型同一性由 kind、前端 ABI、参数和返回类型决定；仅泛型类型参数以其 source type id 区分。`int` 与 `int64` 这类同 ABI 的前端别名会收敛到一个 canonical XLIR type，保留的 name/source id 仅用于诊断与溯源，不能让后端因别名而产生伪转换或补丁。

### 2.x 泛型实例化 ABI

class 及其直接成员通过 `declarationOwnerType` 保留声明级名义 owner；泛型模板的参数
向量由语义声明建立，即使方法体完全不使用 T/self 也不能省略。实例克隆同时替换该 owner、
CallSite 的 `preparedGetter` 和 computed argument origin。尚未具体化的 receiver 只保留
模板绑定，不提前生成物理调用；具体实例必须有匹配的 receiver、实参向量和函数体 ABI。
不得从首个 class layout、同名类型或兄弟方法的 self 表达式推断成员实例身份。

泛型声明本身不是 Lowered XLIR 的可调用 ABI。lowering 必须先从 typed XLIR 中已经解析的类型实参建立 `GenericInstance`：其 identity 至少包含原始声明、按 canonical `TypeId` 排列的全部实参、唯一的单态 callable symbol、替换后的参数/返回 ABI、以及独立的参数和局部 storage symbol 空间。实例体必须是类型和符号均已替换的 owned IR 副本（或具有等价隔离性的实例视图）；同一泛型函数的 `int` 与 `string` 实例不得共享原始参数/局部 symbol，也不得让一个含 `TYPE_PARAM` 的 call target 同时承担两套 C ABI。显式 `::<...>` 实参直接形成该 identity；省略实参时 lowering 按 typed XLIR 的已解析实参/返回 ABI 统一出相同的 canonical 实参序列。两种写法均在 Cgen 前固化，Cgen 不参与推导。

调用 lowering 只能把 `CALL.targetSymbol` 指向该单态实例；verifier 必须验证实例实参完整、所有参数/返回/存储均已 concrete，且每个实例 target 恰有一份匹配 CFG body。递归和嵌套泛型调用同样经过该实例表去重，而不是由 Cgen 根据调用点临时改名或替换类型。Cgen 因而只看普通的 `XL6_IrCallTarget`、`XL6_IrFunction` 和 storage/instruction ABI，完全不知道泛型语法或类型参数。

在该实例 arena、body clone/substitution 与 verifier 落地前，`--backend xlir` 必须以“需要 lowering-time monomorphization”的诊断拒绝含未实例化 type parameter 的调用；不得复用 legacy Cgen、不得把 type parameter 当作 `var`，也不得仅为 identity 等特例在 emitter 增加规则。

容器 ABI 同样遵循该原则：类型池保留容器名与元素参数的静态同一性，但 Lowered storage/call ABI 固定为 owned `xvalue*`，传参 ABI 为 borrow。`array<T>` 使用 `XVALUE_ARRAY_NEW/APPEND_NEW`；稀疏 `list<T>` 使用独立的整数键 map `XVALUE_INT_MAP_NEW/SET_I64_NEW`，键是显式 `i64` operand；`set<T>` 使用 `XVALUE_SET_NEW/SET_ADD_NEW`，由 XRT 的 hash/equality 契约负责唯一性；`dict<T>` 使用 `XVALUE_OBJECT_NEW/OBJECT_SET_LITERAL_NEW`，字符串 key 保持 canonical literal ABI。元素均在容器写入前 boxing 为 owned xvalue，写入 intrinsic 在成功和失败时消费它；`.length` 固定为返回 `i64` 的 `XVALUE_COUNT`。带目标类型的 `dict{...}` 只在 lowering 透明展开为其内层 literal。Cgen 只发射已验证 intrinsic 和 `xrtValueCount`，不查看容器/成员 AST，也不由 C 类型或成员文本重建语义。

INDEX lvalue 不再让 lowering 根据 key 类型临时挑选容器操作。Typed `XL6_IrIndexWriteAbi` 先把 array-i64、list-i64、object literal key、object managed-string key 与 erased xvalue key dispatch 固定为五种 canonical identity，并同时验证 receiver `CollectionAbi`、key C ABI、元素 TypeId 与可用 boxing。普通 `=` 与 null-aware `?=` 都只消费该 enum：array 形成 `XVALUE_ARRAY_SET_I64_NEW`，list 形成 `XVALUE_INT_MAP_SET_I64_NEW`，object 和 erased dispatch 形成各自 consuming RuntimeOp；每项在成功/失败路径消费 owned item，bool 结果由唯一 `TRAP_IF_FALSE` 观察。Lowered verifier 反向禁止 list TypeId 携带 array-set、array TypeId 携带 int-map-set。泛型模板在元素仍为 type parameter 时保持 `NONE`，具体化替换 TypeId 后统一刷新 `IndexWriteAbi`，所以 Cgen 从不接触泛型、容器名、key 语法或该 enum。bool/i64/u64/f64/string array/list、dict literal/string key、dynamic key 与 list `?=` 已严格运行；完整 U64 fixture 因 legacy 的 signed-xvalue 装箱缺陷作为 XLIR-only 合同，int/string 泛型子集用于跨后端差分。

动态 xvalue key 的索引读写也具有独立指令，而不是在 Cgen 内按源码类型补分支。`XVALUE_INDEX_GET_XVALUE_KEY` 借用稳定的 erased collection 与 key storage，返回 borrowed item；缺项、无效 key 或 collection/key 组合不兼容时返回 xvalue null singleton，绝不返回 C `NULL`。`XVALUE_INDEX_SET_XVALUE_KEY_NEW` 同样借用 collection/key，并在成功或失败的每条路径消费 owned item，返回 bool 后必须紧邻显式 `TRAP_IF_FALSE`。运行时分派只读取 tag：i64 key 对 array(tag 9)/integer-map(tag 10)，string key 对 object(tag 12)；不得通过调用可能失败的 getter 来探测容器类型，以免污染线程 error slot。当前 lowering 要求 receiver 和 key 均来自稳定 `NAME` storage；临时 receiver/key 在 cleanup 和借用锚点成为显式 XLIR 前精确拒绝。typed `dict<T>` 的 literal/managed-string key 标量写入，则先按已经解析的元素 TypeId 显式 boxing（当前为 bool/i64/u64/f64/managed-string），再复用 `OBJECT_SET_LITERAL_NEW/OBJECT_SET_STRING_NEW` 的 consuming ABI。其余尚无精确结果类型、所有权或失败边的索引/可变容器功能仍必须在 lowering 拒绝，不能借用另一类容器规则。

容器成员关系同样在 lowering 完成语义消歧。`XVALUE_CONTAINS` 固定为两个 borrowed xvalue operand（右侧 container、左侧已 boxing 的 item/key）和 bool result；item 与临时 container rvalue 都先进入一等 `TEMPORARY` storage，并在操作后显式 `RELEASE`。typed array/list/set 的元素类型、typed dict 的 managed-string key，以及真正 builtin dynamic 的开放输入由 TypeId/`collectionAbi` 验证；concrete class、protocol、range 和静态 managed-string 不因物理表示相近而混入该操作。typed set 继续使用直接 `XVALUE_SET_HAS`，统一操作则仅按 XRT tag 分派：array(tag 9)/integer-map(tag 10) 通过稳定快照迭代并使用 `xrtValueEqual` 搜索值，set(tag 11) 查元素，object(tag 12) 查 string key，runtime string(tag 4) 做 substring；不兼容 tag 组合确定返回 false。Cgen 不读取集合类型名、元素参数或源 `in` 运算符。

静态 managed-string 的 `in` 使用独立的 `STRING_CONTAINS`，而不是先 boxing 再进入 erased tag dispatch。lowering 固定 operand 顺序为 borrowed haystack、borrowed needle 和 bool result；literal、调用结果等 owned rvalue 分别物化为 managed-string `TEMPORARY`，操作后显式 `RELEASE`。Cgen 仅机械发射 `xrtStrContains(xrtStrView(haystack), xrtStrView(needle))`。该指令与标准库 `string.contains` 的私有 CallTarget runtime bridge 是两个层级：前者表达语言运算符，后者表达已验证的直接调用目标，二者都不允许按源码函数名回溯。

非调用成员表达式也必须在 Typed XLIR 完成语义消歧。`XL6_IrMemberAbi` 是成员语义的 canonical identity，源码成员拼写只参与前端分类、诊断以及已验证的 literal object-key payload；lowering 不再按拼写选择结果表示。`STRING_UTF8_COUNT` 固定 `string.length` 为一个 borrowed managed-string operand和 canonical i64 result，语义是 UTF-8 code-point count，而不是 C 字节数。class 的 `value.length` 若由语言 `__len__` 协议提供，则固定为 `LENGTH_PROTOCOL_CALL`，并在同一 Typed 节点保存唯一方法 `targetSymbol`；真实 `length` 字段优先，重复/不匹配协议目标不予接纳。lowering 仅检查目标的一参 borrow receiver 与 i64 result ABI 后发射普通 `CALL`，Cgen 因而看不到成员名、协议搜索或特殊长度分支。concrete xvalue class 字段则先以 receiver TypeId、`ClassLayout` 字段 slice、字段 TypeId 和名称精确匹配，再分别固定为 xvalue borrow、bool/i64/u64/f64、owned string copy、raw-pointer borrow、non-owning native-pointer、owning-handle borrow、tagged nullable 或 closure ABI。泛型 class 单态化追加 concrete layout 并替换 cloned TypeId 后，lowering 必须先通过 `xl6IrRefreshMemberAbis` 统一重算全部成员 identity，之后才允许降低任何 body；不能让模板期 xvalue 表示泄漏到 concrete scalar 字段。lowering 只读 `memberAbi` 并形成一一对应的 literal-key RuntimeOp；raw `point` 写入 erased 字段另以 `XVALUE_RAW_POINTER_BORROW` 显式 boxing，不能借 native class bridge 表示。Verifier 重算 Typed identity并验证 Lowered operand/result/ownership/隐藏元数据；Cgen 不得读取 `memberAbi`，只机械调用各 RuntimeOp helper 或普通 CALL。safe nullable、error location、union 与尚未 concrete 的 generic 字段仍须各自扩展 ownership/effect ABI，不能恢复按字段名或 C 类型分支。

`range` 已由前端表示为 verified `IntrinsicCall` descriptor，成员关系直接消费这份语义事实。lowering 将 1/2/3 参数形式统一补齐为 `item/start/stop/step` 四个 i64 operand，并发射 bool `I64_RANGE_CONTAINS`；descriptor 本身从不物化为 C 值。C helper 对 step=0 返回 false，对正负 step 使用相应半开区间边界，并以 `uint64_t` 计算非负 distance 和 step magnitude，避免 `value-start` 或 `-INT64_MIN` 的 signed overflow。Cgen 不识别 `range` 名称、不重放默认参数，也不从源调用语法恢复步长。

表达式语句也必须闭合结果所有权。`DISCARD` 只观察 trivial 值、已验证为非拥有的 borrowed managed `LOAD`，或专门允许的 error observer；它不能静默吞掉 managed owner。对已经验证为 owned rvalue、且 TypeId 具有现成 storage-release ABI 的 string/xvalue/error/future/closure，lowering 创建一等 `TEMPORARY` storage，依次发射 `STORE` 与 `RELEASE`，verifier 继续证明 storage/type/function owner，Cgen 只按该 TypeId 渲染释放。其他 borrowed managed 来源、managed product 与 native handle 在各自 observation/value-drop/destructor ABI 完成前精确拒绝，不能仅因 ownership 分类为 managed 就猜测析构函数。

函数值/闭包是独立 ABI 分区，不能被表示成无签名的 `void*` 或由 Cgen 回溯匿名函数语法。Lowered XLIR 拥有 `ClosureTarget`（独立 body symbol、函数签名 TypeId、capture slice）、`ClosureCapture`（source symbol、closure-body synthetic storage、类型及 copy/retain ABI）和 `CLOSURE_MAKE / CLOSURE_CALL / RETAIN` 指令。无捕获匿名函数及来自外层参数的 scalar、`string`、`var` 和 `future<T>` capture 已端到端迁移：lowering 分别生成 `COPY_VALUE`、`COPY_STRING`、`RETAIN_XVALUE` 与 `RETAIN_FUTURE`，并建立 `STORAGE_CAPTURE`。Cgen 只依据 capture slice 生成独立 environment struct、析构函数、带 `environment` 参数的 CFG body 与已验证签名的 thunk：字符串构造时以 `xrtStrDup` 复制并在析构时 `xrtFree`，xvalue 以 `xrtValueRetain/xrtValueRelease` 管理，future 以 `xrtFutureRef/xrtFutureDestroy` 管理；因此闭包绝不借用构造者参数指针。capture 分析是传递式的：嵌套 closure 需要的祖先 binding 先物化到直接创建者的 frame，再复制到内层 frame，任何跨函数 storage 读取仍由 verifier 拒绝。函数接口注解与等价 lambda 推导类型 ID 不同的情形只允许经参数/返回 TypeId 完全一致的显式 closure retype：直接 call 与 return 均先以目标签名发射，再以 `VALUE` 显式转换。由既有闭包 name 初始化另一个闭包存储会先生成 `RETAIN`，Cgen 机械递增引用计数；局部闭包 return 则经 `MOVE` 转移。`xlir_function_value` 覆盖泛型函数值传参；`xlir_closure_capture`、`xlir_closure_return` 和 `xlir_closure_nested_capture` 覆盖 scalar frame 及复制/返回；`xlir_closure_string_capture`、`xlir_closure_xvalue_capture` 与 `xlir_closure_future_capture` 分别覆盖三类受管 capture 的严格宿主 C 执行。函数值自身 capture、可变 capture 和非参数局部 capture 仍必须在 lowering 明确拒绝，直到 `RETAIN / MOVE` 的完整 environment ABI 或 cell ABI、析构和 cleanup CFG 完整落地；绝不能借用构造者局部变量或回退旧 Cgen。

补充：来自外围作用域的只读 lexical local 已使用同一 `ClosureCapture` ABI 迁移。lowering 仅接受有 `VALUE_DECL` storage 的局部，将其按静态类型复制或 retain 到 environment，closure body 则只读取 synthetic `STORAGE_CAPTURE`；`xlir_closure_local_capture` 验证该值不会借用外层 C 栈帧。对 capture storage 的 `STORE` 仍明确拒绝，因此该扩展不暗示按引用的可变 capture 已实现。

闭包值本身同样具有固定共享所有权 ABI：`RETAIN_CLOSURE` 在构造 environment 时调用 `xlir_closure_retain`，environment 析构时调用 `xlir_closure_release`，并在 allocation failure 路径释放已取得的引用；`xlir_closure_closure_capture` 覆盖一个闭包捕获并调用另一个局部闭包。因而未迁移的边界收敛为可变 capture（需要 cell ABI）及没有 lexical value storage 的来源，不能以裸指针或 Cgen 回溯补齐。

闭包进入动态运行时调用也不再由 Cgen 猜测签名或复用 legacy `XL6_Function`。`XVALUE_CALLABLE_FROM_CLOSURE` 借用一个带完整函数 TypeId 的 closure，产生 owned callable xvalue；`XVALUE_DYNAMIC_CALL` 借用第一个 callable operand，在成功和失败路径都消费其余 owned xvalue 参数，并产生 owned 动态结果 carrier。两条运行时操作都必须紧邻消费该结果的 `ERROR_TEST`，使成功、传播或捕获边完全属于 XLIR CFG。当前封闭 ABI 分区接受任意个 `VALUE bool|int|uint64|float` 参数和单一 `bool|int|uint64|float` 结果、位置参数及具名函数值来源；Cgen 按已经验证的 TypeId 去重生成机械 box/unbox adapter，只调用公共 `xrtCallableCreate / xrtValueCallableTake / xrtValueInvoke / xrtCallResult*` API。`uint64` 必须经 `XVALUE_UINT`、`xrtValueUInt/xrtValueGetUInt` 保持全 64 位无损，不能复用 signed i64 payload。may-raise closure 继续使用统一 `bool + out-result` effect thunk；XRT 的 callable-entry wrapper 失败时，dynamic-call 按该 op 的契约将 owned cause 投影回 XLIR 错误槽，因而 typed catch 观察原始 kind/message，而无 cause 的 XRT 参数或签名错误保持原样。`int,int -> int`、bool、混合 bool/int/uint64/float 及零参 may-raise bool 均有严格运行门；string/xvalue/void/product 结果、关键字、变参和临时闭包来源仍必须先扩展 lowering、verifier、所有权及差分门禁，不能在 emitter 内加隐式适配。

动态 U64 值桥接与动态 U64 运算是两个独立契约。前者已经由 `XVALUE_U64`、typed U64 容器/类投影、受检 `xvalue -> u64`、dynamic string、xcall adapter 与 Future aggregate payload 完整固定。直接动态 `+/-/*` 也已经分别降为 `XVALUE_BINARY_ADD/SUB/MUL`：每条指令固定两个 borrowed xvalue operand、一个 owned xvalue result，runtime 数值 promotion 在任一 operand 为 U64 时产生 U64，并明确采用 modulo-2^64 运算；临时 owner 的锚定与释放存在于 CFG。Cgen 只把三个已验证 identity 映射到 runtime mode，不观察源码运算符或 tag 来选择语义。除法、取模、比较以及不支持类型的 error effect 仍必须先拥有各自 opcode、promotion/result TypeId、错误与 ownership 契约；即使已有 helper 能计算，也不得在 emitter 内扩展。

语言 `type` 值使用独立的 `TYPE_ID` C ABI，虽然物理表示同为 `uint64_t`，但不得与语言 `uint64` 合并。`XL6_IrType.typeValueId` 复现既有 `type:` 前缀、结构类型拼写与历史 FNV 哈希，文本 dump 使用固定十六进制，JSON 使用字符串而非会丢失 64 位精度的 number。裸类型名由 `typeValueTarget` 指向被指称的 canonical TypeId；`type(expr)` 则是 typed `TYPE_OF` IntrinsicCall，携带 `STATIC_ID + typeQueryTarget` 或 `DYNAMIC_XVALUE` 查询 ABI。静态查询严格求值 operand 一次：trivial/borrowed 结果用非拥有 `DISCARD` 观测，owned managed rvalue 仍走 temporary/release，然后产生规范的 `0xHHHHHHHHHHHHHHHHULL` TypeId 常量。动态 `var` 查询在 runtime identity/tag 语义尚未形成明确 ABI 前以专用诊断 fail-closed，不能假装返回静态 `var` 类型。公开/外部 `type` 全局、参数/返回、may-raise `xlir_result_type_id`、`future<type>`、immutable capture 与 mutable `cell<type>` 都使用独立枚举和 helper 名称；C header 才把该契约机械映射成 `uint64_t`。class field 与 typed container element 的 TypeId ABI 尚未完整接纳，不能复用 U64 member/container opcode 绕过。

可变 capture 采用独立 `CELL` ABI，而非把 `STORE` 特判为写 environment 字段。内部 `cell<T>` TypeId、`RETAIN_CELL` capture、cell-backed storage 与 `CELL_NEW / CELL_LOAD / CELL_STORE / CELL_RETAIN / CELL_RELEASE` 已由 verifier 定义；它拒绝把 cell storage 伪装成普通 `LOAD / STORE / RELEASE`。第一阶段已完成 `bool/int/uint64/type/float`、`string` 与 `var` lexical local、参数：local 创建时由 `CELL_NEW` 物化；参数保持外部 callable ABI，但在函数入口提升为 synthetic local cell，后续 body/capture 都只指向该 storage。创建者和每个 closure environment 分别持有显式 cell 引用，Cgen 仅依据 instruction/storage/capture ABI 发射 `xlir_cell` 的 retain/release/析构，不读取赋值 AST 或闭包嵌套结构。scalar/TypeId `CELL_STORE` 无元素析构，并分别使用 `u64` 与 `type_id` helper 保留 ABI 身份；`string` cell 独占其 `char*`，load 只产生 borrowed observation，parameter promotion/return 则显式 `STRING_COPY`，store 消费已拥有的替换值并先释放旧元素，最后一个 `CELL_RELEASE` 才析构其元素。`var` cell 独占 owned `xvalue*`：从 cell 返回或转换为独立动态值时 lowering 显式发射 `XVALUE_RETAIN`，store 消费新值后释放旧值，最后一个 `CELL_RELEASE` 再释放元素。return 穿过 finally 时，lowering 在 return 点冻结活动 cell storage 列表，先转移 pending return，再执行 finally，最后在同一 return-finally CFG 边发射列表中的 `CELL_RELEASE`；因此 finally 永不观察已销毁的 cell，Cgen 也没有 pending-control 或补丁式 cleanup。future/closure 等其余受管元素仍需要各自的替换/析构 ABI 与运行时门；在它们齐备前，语义与 lowering 明确拒绝。

Lowered XLIR 的每个 operand 都必须引用同一函数中先前定义的 `ValueId`，且 `LOAD / STORE / CALL / RETURN / BRANCH / VALUE` 分别校验存储、参数、返回和布尔条件 ABI。函数、block 和 instruction arena 必须互相精确分区；跳转目标也必须位于当前函数的 block 区间。因此 C emitter 不会把跨函数标签、跨作用域值、孤立/重叠指令区间或 C 的隐式参数转换当作可接受输入。

每个可达的非 `void` 函数 CFG 路径必须以 `RETURN`、`THROW` 或显式转移结束；这在 XLIR verifier 中检查，而不是等 C 编译器报告“control reaches end”。`void` 函数的自然尾部由 lowering 补成显式 `RETURN`，除非该末块没有入边。

结构化 `MODULE/BLOCK` 不含可重新进入同一语句序列的标签；lowering 在当前 block 已由 `RETURN / THROW / BRANCH / JUMP / ERROR_TEST` 终结后停止物化后续不可达语句。这样 `throw` 后用于前端完备性的 fallback return 不会污染同一 block，verifier 仍严格拒绝任何真实的“terminator 后还有指令”。

模块级运行语句在 XLIR 中属于合成初始化函数。普通 `GLOBAL`（包括无 C linkage 的 package export）存储先静态声明，再由幂等 `xlir_module_init()` 的初始化 CFG 写入。显式 C 数据导出是例外：它由独立 `GlobalExport` 记录 storage symbol、声明与 initializer 节点、精确 TypeId、mutable/const、公开 C name 和 canonical static scalar initializer；C 定义在加载时即完成初始化，module-init 不再为同一 storage 发射第二次 STORE，因此 C 宿主可在调用函数 adapter 前直接读取，const 也不会产生非法运行时赋值。当前只接纳可静态表达的 trivial bool/integer/type/float；TypeId initializer 还必须证明裸类型名的 `typeValueTarget` 与规范哈希载荷一致。string/xvalue/future/closure 等托管公开全局必须等待模块析构与跨边界所有权 ABI，不能伪装成标量。正常结束不依赖 C 文件中“最后一个 label”的自然落空；lowering 会在实际 continuation block 写入零 operand 的 `RETURN`，Cgen 机械地将其发射为设置初始化标记并返回成功。异常 `THROW` 仍显式返回失败。顶层值返回、运行时初始化/析构等尚无 ABI 的形态必须明确拒绝；绝不能静默跳过顶层语义。

`xlcEmitC` 与 `xlcEmitXlirC` 进入同一显式 API 路径：先把已验证的 Typed XLIR 收紧为 Lowered XLIR，再直接发射 C；lowering 失败会回报前端诊断，不存在替代后端。`XL6_IrExport` 是独立函数公共边界，而不是 CallTarget 的别名：它固化 C linkage name、每个参数的 type 与 `value/borrow` 模式，以及返回的 `void/value/owned` 转移；verifier 再证明该 adapter 与内部 target 精确相接。`XL6_IrGlobalExport` 则是独立数据公共边界，不复用函数参数/返回模型；verifier 证明它与唯一 GLOBAL storage、声明 binding、静态 initializer 和 const 写保护一致。反方向的 `XL6_IrExternGlobal` 只表示由其他翻译单元拥有的 C 数据存储：它固定 `EXTERN_GLOBAL` storage、TypeId、read/write access 与 C name，没有 initializer，也不会泄漏到本模块 export header。当前 extern 数据同样只接纳 trivial bool/integer/type/float，托管外部对象必须先定义读取、替换和生命周期所有权。函数、公开数据和外部数据 C name 在同一链接命名空间中统一查重。名义类型重载的自动 C 名称使用 canonical Type ABI ID，不以显示名称或相同的 `xvalue*/void*` C 表示碰运气消歧。`xlcEmitXlirHeader` 从函数与公开数据两张 export ABI 表发射声明，因此生成 header 不读取 AST 或 ExternGlobal；C 与 header 任一侧无法表达的 ABI 都在发射前拒绝。

命令行可用 `xl cgen file.xl [--backend xlir] [--emit out.c] [--emit-header out.h] [--dump-dir dir]` 调用此路径。`--emit-header` 走上述 IR-derived header emitter；`--dump-dir` 只写入 typed IR、lowered IR、XLIR C 和 IR-derived header，不混入 tokens 或 AST，因而不会形成隐式前端依赖。

后端质量门至少包括：XLIR verifier、C ABI 可发射性预检、同一 XLIR 连续两次发射的 C 文本逐字节一致、生成 C 的严格编译检查，以及用 C host 执行生成函数的语义检查。非平凡（非 `int/bool/float/void`）所有权若没有完整的 action/cleanup CFG 必须在 lowering 失败；若已 lower 为 `RETAIN / RELEASE / MOVE / DROP`，Cgen 在 runtime ABI 实现前必须明确拒绝，绝不能静默擦除标记。行为基线由语言断言、IR 变异拒绝、严格 C、JIT/AOT 和宿主 API 共同固定，不依赖另一套代码生成器作为真值来源。

发射器可省略无前驱的空 merge 块，避免 C 的未使用标签诊断；此规则只作用于无可达边的空块，不会重写或猜测 CFG。

## 3. C 后端最终约束

完成迁移后的 C emitter 只读取 `XL6_IrModule`：

- 不读取 `XL6_AstNode`，不读取 `inferredType`。
- 不进行 overload、泛型实例、成员/协议派发、类型推导或所有权推导。
- 只把已确定的 IR 类型/ABI、调用、基本块、retain/release/move/drop、runtime intrinsic 和声明顺序映射为 C。

当某项语义未能落入 XLIR 时，应在 IR lowering 报错，而不是在 C emitter 重新解释源码。

## 4. 已完成的迁移收口

### 方法启动与 worker 的分离契约

class virtual 与 protocol witness 的槽位描述调用方可见的结果，而不是方法体的
return 声明：普通方法返回 T，async 方法返回 Future<T>，普通或 async generator
方法返回 generator<T>。async generator 不再套一层 Future。前端预检查类型查询、
override 匹配、Typed 槽位建立和 Lowered 验证必须采用同一个调用结果规则。

generator 启动是惰性的，但不是 no-raise：frame、string capture 和 generator
对象分配均可能失败。CallTarget 使用 MAY_RAISE + NULL_RESULT；workerErrorAbi
独立描述恢复执行的失败。class/protocol 槽位汇合启动效应，使用 RESULT_CARRIER；
共同 witness adapter 将启动器的空指针结果转为该 carrier，不推断源码或方法体。
GENERATOR_START 与其它 may-raise 调用一样必须立即有 ERROR_TEST，再进入调用者
清理或后续求值。普通、async、generator 发射器共享物理 outcome 检查。

隐式 receiver 仍以 BORROW 传入启动器，其 frame 通过已验证的 RETAIN_XVALUE
持有同一个对象；Future 参数对应 RETAIN_FUTURE。完成、worker 失败或提前关闭
释放 frame owner；启动失败只回滚已初始化的 capture，不执行 worker。
该契约由 `tools/check_xlir_virtual_activation.ps1` 的语言、非法 IR、严格 C、
JIT/AOT、实际挂起和 allocator 故障测试守护，不能以仅放开 dispatch kind 代替。

### activation 的 consuming 参数与 worker 清理

`byval` 的受管参数使用 OWNED 调用 ABI。统一的
`xl6IrParameterFrameTransfer` / `xl6IrAsyncParameterCapture` 将它固定为
`TAKE_PARAMETER`，与跨挂起 SSA 的 `MOVE_OWNED` 分开：前者没有 SSA take 指令，
而是启动边界的所有权转交。两种 machine 的 prologue action 均先接管所有 OWNED
参数，再进行可能失败的 borrowed copy/retain；验证器检查顺序、类型、析构 ABI
和初始化位。未使用的 consuming 参数也必须有明确的 owner，不能由 liveness 丢弃。

帧分配失败时，启动器逆序释放尚未入帧的 consuming 输入；部分捕获失败时按已验证
的 frame destroy action 回滚已初始化字段。共用的 C 发射函数只读取 transfer ABI、
物理位置和初始化位；清理使用公共 error scope 保留第一失败，析构异常不得覆盖原
启动失败。ready-Future 在 worker 调用前完成结果盒分配，避免 worker 已返回拥有值
后因盒分配失败而遗失该值。

worker 的 lexical owner 与 frame 的 retained borrowed capture 不混为一谈。
`xl6IrFunctionParameterBindingOwnsStorage` 让普通、async、generator worker 的
OWNED 参数统一生成显式终态 RELEASE/CLEANUP_CAPTURE，并由 parameter lifetime
verifier 检查。任务池适配器调用 worker 前将 consuming 字段 detach 到局部实参，
最终 frame callback 不再释放已被 worker 消费的 owner。continuation worker 的
RELEASE 清除相应初始化位；取消、关闭或 generator 尚未恢复时由 frame 回收剩余 owner。

`tools/check_xlir_owned_activation.ps1` 覆盖 byval class、unused owner、先 borrowed
后 owned 的参数顺序、ready/pool/continuation/generator、真实分配失败和抛错析构。
这不等于所有受管/native 类型的捕获组合均已验收。任务池尚无规范的 module resource
unload action；当前专项在宿主中显式 close/wait/destroy，不能作为自动卸载已完成的证据。

### 阶段结果

1. Typed XLIR 验证器和 dump 是前端语义事实基线。
2. 函数调用、值/Place、局部声明、返回、分支、cleanup/异常、class/protocol、泛型实例和 async/generator 状态机均通过 Lowered XLIR 进入 C emitter。
3. 旧 HIR 类型、构建器、AST 驱动 Cgen、兼容 API、构建清单和后端选择分支均已删除。
4. 后续扩展只能增加 typed/lowered XLIR 合同、验证器与机械发射，不得恢复第二条语义路径。

`tools/check_xlir_contract.ps1` 和历史命名的 `test/cases/hir_module.c` fixture 守护 XLIR 的无 AST/HIR 读取、类型/存储类型、树完整性、lowering 指令、CLI dump 和编译单元接线约束。

### 模块任务池资源与宿主生命周期

`XL6_IrModuleAbi.taskPoolAbi` 独立于语言全局 cleanupCount。guarded-quiesce
契约由已物化 Lowered worker/runtime op 推导和验证：无 await 的任务池 worker
与 task.spawn 共用懒初始化池，ready Future 与直接 continuation 不单独要求池。
不存在通过 Cgen 扫描源码来决定资源归属的路径。

激活创建 gate；所有池提交经该 gate 检查 closing，并在同一临界区完成池创建与
提交。MODULE_FINI 在执行语言全局清理前禁止新提交，在 gate 外 join/destroy
worker 和 frame callback；失败保持模块、全局和代码存活，供外部重试。global
cleanup 完成后才销毁 gate 并允许重新激活。即使全局析构失败，也走资源终态。
资源检查、MAY_RAISE finalizer、首错误保留和可重试未静止状态不可混为一谈。

`check_xlir_module_task_pool.ps1` 验证生成模块的并发卸载、self-worker 拒绝与
外部重试、closing 时嵌套提交拒绝、回调退出和全局析构顺序。嵌入 API 仍须拥有
entry/activation/unload 协调及 code lease；保留的 Future、closure、callback
可能超过 pool worker 生命周期。仅完成池 quiesce 不能证明 TCC/DLL 代码可释放。

`xlir_module_task_pool.strict.exe --global-construction-fault` 保留完整构造故障扫描。
construction-v7 修复了全局 class 分配失败后类型绑定 abort；projection-outcome-v5
进一步修复读取 string 字段的分配失败及临时字符串释放，完整 gate 已通过。
这不等于其它分配、构造 escape 或 DLL/API 卸载边界已经完成验收。

未调用的 async 声明也拥有独立 invocation type。Typed 导入在已有源类型之后
补齐声明的 Future<T>，void 使用 Future<null>，generator 保持原 handle。
该事实不能依赖偶然调用点，也不能由 Cgen 从函数体猜测。

### 构造期间的未提交 owner 与可失败物理结果

CLASS_NEW 的 NULL 结果与 CLASS_FIELD_INIT_NEW 的 false 结果是可验证的失败，
不是 runtime shape invariant。pending object 从检查分配成功后到字段、create
和继承 create 完成前，始终由 evaluation-owner storage 保护；异常 unwind 回收
已初始化字段，成功 commit 才移交对象及绑定用户 finalizer。不分配内存的
type/identity binding 仍受“唯一新建、非空、匹配布局”的前置条件约束。

native owning factory 必须先将 raw handle TAKE 成有析构职责的 box，再为外层
class 分配；两者在构造期间具有独立 owner。named raw source 在接管边界清空。
若包装仍可能失败，调用参数的借用临时所有者也必须保留到包装结果可提交为止。

construction-v7 已修复原全局构造 NULL type-bind abort，并验证嵌套、继承和 native
构造故障。它保留的 string 字段复制故障由下面的拥有型投影契约继续收口，不能仅将
helper 改为返回 NULL 而不为各消费者声明失败结果、建立 ERROR_TEST 和回滚 owner。

### 拥有型投影的失败效果与 case 绑定作用域

projection-outcome-v5 为 class/dict 的 literal string copy 及 enum string payload
建立 `projectionErrorAbi`。它独立于 accessor 的 call/error ABI；enum match 的效果
来自已解析 EnumLayout，而不依赖恰巧生成的某个 payload 指令。Lowered 复制操作以
NULL-result 作为失败结果，必须紧接成功/失败目标不同的 ERROR_TEST，之后才允许使用
复制值。同步、async、generator 继续消费同一个物理结果契约。

借用读取必须保护拥有型 receiver，失败边和成功边关闭同一求值 owner。字符串
`.length` 本身不消费字符串，但 owning projection 或调用结果仍必须在借用结束时
释放；命名局部字符串保留原词法所有者，不能被长度查询偷走。

语句型 match 的 pattern 是 case block 的隐式绑定声明，先于该 block 的显式局部。
复制 payload 的过程中，已完成的绑定与临时 subject 由求值事务保护；全部绑定完成
后，所有权提交给经验证的词法清理计划。正常离开、return、break/continue、throw、
finally 及抛错析构共享现有 scope exit route；不能依靠 match 发射器末尾的一段
RELEASE 来补清理。失败发生在 case block 已清理之后时，不得再次纳入这些绑定。

持久化 parse/read 在构造目标对象之前也必须保护输入文本的临时 owner。该修复只
证明目标构造失败时输入得到回收，不等于 parser/stringify 的全部分配错误已验收。
其它 array/list/nullable/iterator 投影、boxing，以及表达式型 match 的全部抛错清理
组合仍需继续按各自真实物理行为核对；不得从本专项外推语言所有权已完全闭合。

### 求值结束的清理事务与循环进入阶段

borrow-cleanup-v4 将操作成功与整个求值成功区分。Typed 节点的
`evaluationErrorAbi` 由求值操作数的释放效果确定，与操作本身的错误 ABI 和
`projectionErrorAbi` 分开。普通调用、借用读取和任务类 intrinsic 使用共享的
CompleteOwnedCleanup / CompleteEvaluationBorrows；不能等到 export 边界才观察
用户析构设置的错误，否则失败后的源程序副作用已经发生。

Lowered 的 `evaluationScope` 是 CLEANUP_BEGIN 的一基索引，明确标识求值清理
事务及其失败 unwind；`evaluationResult` 是待提交结果的精确临时 storage。
结果 STORE、成功 LOAD/MOVE 和失败 RELEASE 重复该事务标识。验证器要求正常
清理不释放这个结果，CLEANUP_END 后立即检查两个不同目标，失败先回滚结果、
再执行 cleanupPlan 指定的有序退出路线；成功才移出结果。pending finally 原始
错误的恢复前缀也必须与该 cleanup outcome 对应。词法/循环清理通过事务标识与
求值清理区分，不再把缺少 cleanupPlan 当作“可能是调用临时量”的暗示。

for 的 iterable 子树在迭代器及当前项建立之前求值；其中的失败不能关闭尚不存在
的循环 owner。Typed exit route 只对循环体内的退出插入该循环的 iteration/resource
步骤，迭代器获取及 next 的失败仍使用 LOOP 的显式阶段出口。这一规则由 Typed
父子角色确定，不由 Cgen 从失败指令的来源猜测。

task.spawn 的闭包、固定参数以及正在构造的可变参数数组，都必须在求值后续参数
之前纳入共享 owner 栈。Future continuation、数组 all/race/whenAny 同样先观察
操作结果，再关闭借用；返回 Future 也需要待提交 owner，不能在清理失败后泄漏。
本轮不声称数组创建/append、其它 boxing 的全部分配错误契约已经迁移完成。

### 表达式 pattern 的求值生命周期与不可变分支结果

match-cleanup-v8 将表达式型 pattern binding 定义为具名的 TEMPORARY storage：
语义来源仍是 canonical NAME/PATTERN/EnumLayout payload，物理生命周期则属于
本次 match 求值。复制/保留分支结果后，先完成 payload 与 subject 的共享清理事务，
成功才将结果写入 merge storage；失败释放待提交结果并执行原始错误的退出计划。
语句型 case 仍在投影全部成功、subject 清理成功后提交到词法作用域，两者不能混用。

捕获类型从 EnumLayout 的精确 payload TypeId 取得，不使用可能尚未填充 storageType
的 pattern NAME。已有 mutable var capture ABI 使用 cell 物理所有者；其初始化、
读取、引用和释放分别表现为 CELL_NEW/CELL_LOAD/CELL_RETAIN/CELL_RELEASE。
这不等于新增 class-typed mutable capture 的前端支持，也不等于 cell 分配错误已验收。

所有 SSA 值不可改写。旧 targetValue 分支赋值不是 phi，跨 continuation 的函数拆分
会丢失声明/定义归属。match（标量与托管结果）、短路、nullable coalesce 现在统一
使用 typed STORE/LOAD；托管 join 在 LOAD 后有显式 MOVE。普通 conditional 已使用
同一存储模型。VALUE 仅定义新的不可变值；验证器拒绝旧 targetValue 改写，Cgen 不
通过收集更多 C 局部变量来容忍错误 IR。targetValue 字段暂留为必须 INVALID 的旧编码。

async pattern owner 使用公共 frame residence、初始化位与 destroy action，而非
match 专属 frame 字段。真实未完成 Future 测试覆盖 8 类结果/分支，确认挂起时对象
尚未析构、恢复失败时没有后续副作用、正常与失败退出分配平衡。

取消/关闭仍缺少完整的 IR 终态退出模型：当前机器入口只表示 LAUNCH、RESUME_SUCCESS、
RESUME_FAILURE，C emitter 对 CANCELLED/CLOSED 提前完成 promise。现有探针证明这会
跳过 finally，且 frame 销毁的析构错误可泄漏到调用线程的错误槽。后续须把终态原因、
有序退出/清理和清理结果归属显式放入 continuation IR；不能在发射器末尾清错掩盖，
也不能把正常/失败恢复的通过证据扩大为取消/关闭已验收。

### Future outcome 与规范恢复入口（terminal-outcome-v3）

每个 AWAIT 和 continuation machine 必须显式指定
XL6_IR_FUTURE_OUTCOME_XRT_ERROR_KIND。挂起点的四个恢复记录分别代表成功、失败、
取消和关闭；四者都进入 AWAIT 后的规范指令片段，随后执行 Lowered CFG 自己的
清理和异常分派。Cgen 只转换物理 Future 状态与 typed error kind，不以 terminal
状态提前退出 callback，也不追查 AST 找 finally。验证器拒绝缺失/互换/跳过指令
的恢复入口，不能用 success/failure 双入口旧模型蒙混通过。

阻塞 await 与 continuation 共用观察规则；状态机最终 THROW 依该 ABI 反向映射
Promise 完成方式。自动清理保留原异常，显式 finally 抛错/await/return 可覆盖
pending exit；该优先级在 lowering 的共享 pending-exception/exit 协议中决定。
跨 finally 的第二次真实挂起与全分支 return 已纳入运行门禁，而不只检查生成文本。

边界尚未闭合：无 await 的 task-pool async 仍通过 XRT task outcome 完成，
取消/关闭错误被归为普通失败的探针已确认。ready/no-await completion 必须统一
到相同语义契约，同时保持调度、反向取消传播和拥有型结果；不得用 Cgen 临时
转发包装掩盖未定义的物理生命周期。此阶段不在 CallTarget 宣称未实现的终态 ABI。

### 无挂起 async 的规范完成（pool-outcome-v3）

futureOutcomeAbi 现在覆盖所有 INTERNAL_ASYNC CallTarget，而不是只承诺含 AWAIT
的 machine。它在最终 callable role 确定后赋值；async generator 仍是 generator
activation，其 CallTarget 不携带 Future 完成契约。

XRT task outcome 是物理传输，不等于语言 Future 的四态。pool launcher 在提交前
备齐无分配 Watch 的存储、输出 Promise/Future 和 parent-linked cancel token；
受理后仅注册 Watch，在原任务完成并清理 frame 后规范化错误 kind。成功值使用
PromiseForward 保留源 owner，不复制 value，也不改变 worker 调度。注册失败发生
在提交后时，必须成为 Future 的失败，不能返回 NULL 后让已受理代码继续运行却
没有可观测结果。取消输出只请求生产者停止，不提前伪造终态；Notify/Release
保护调用线程已有的错误上下文。ready 与 machine 使用相同的 Promise 完成规则。

该路径由已验证的 CallTarget role/dispatch/outcome/payload ABI 选择，不读取 AST。
task.spawn 的 AsyncError（保留 cause）包装及用户 continuation 是其它边界，后续必须在各自明确
的语义契约下接入，不能因为共享线程池而直接套用同一错误包装行为。

受理后注册失败的顺序补充（pool-outcome-v4）：由于 XRT 只接受对 PENDING Future
的取消请求，转换失败必须先保护错误并取消生产，再发布 FAILED 终态。该顺序由
独立处理过程和父/子取消令牌测试约束，不能先完成输出再调用 FutureCancel。

## 回调也是独立激活：捕获事务与完成发布（2026-09-08）

callbackBoundary 连接 RuntimeOp 与 CallbackBoundary/CallbackCapture arena。
Capture 固定操作数、SSA 身份、TypeId、source/callable/argument 角色及获取/析构
ABI；Boundary 固定调度载体、清理事务、错误策略与源结果 owner 转交策略。
这是 Lowered XLIR 的显式激活计划，而不是 Cgen 从类型拼出另一套所有权流程。

捕获按规范顺序获取、逆序释放；每个字段在调用用户析构前脱离激活，防止重入或
fallback finalizer 重复消费。待发布结果只有在清理成功后才交给 Promise/task result。
失败回滚保护主异常；输入的失败/取消/关闭在自动清理时受到保护，但显式 finally
回调失败仍能覆盖输入。任务错误跨边界保持 AsyncError 及原始 cause。

语言 continuation 不能委托给可能在取消时跳过语言回调、提前发布的通用适配器。
FUTURE_WATCH 载体用预分配激活直接处理 READY/PENDING，取消也执行同一清理屏障。
透传保留源 Future owner，不按 payload 类型复制；结果转换则持有自己的 pending
owner。原生回调返回及 fallback release 恢复宿主原错误上下文。Watch 和取消相关
声明必须独立可用，不能由其它 task/async 模块偶然补齐。

callback-boundary-v4 的永久验收同时覆盖主模块与 Future-only 模块；IR 变异、
严格 C、JIT/AOT、分配失败和跨线程观察记录见迁移验收矩阵。未来需继续压实直接
名义 task 参数、完整 owning payload 析构边界与挂起回调的代码租约；当前计划没有
把这些未完成部分伪装为已具备的保证。


### 任务类型准入与运行时引用边界（2026-09-08 补充）

task.spawn 的语言准入不再使用旧 xvalue call bridge 的类型白名单。参数遵守
已解析签名的语言类型兼容规则；单返回值与普通函数共用可返回类型判断。
IR 对精确类型、callable 兼容、显式动态 boxing、同类 erased collection 和
已验证 class upcast 进行绑定，不能用相同 C 指针表示代替语言类型关系。
frame 参数支持与绑定判断由 IR 公开合同提供，lowering/verifier/Cgen 共用。
byref/out 不能把调用者存储捕获进逃逸任务；拥有型 BYVAL 与 generator 的完整
任务调用/捕获/结果 ABI 仍待完成，不能放宽为默认 borrowed 参数冒充支持。

跨线程引用计数操作是生成代码依赖的运行时基础合同。某个线程释放独立的
非最后 owner，不应因另一 owner 正在执行只读集合查询而丢弃这次释放。
当前 XRT BUSY 前置拒绝已被公开 API 的纯 C semaphore 测试确定性复现为泄漏；
这不是通过 Cgen 复制 payload、延迟释放或重试可以正确解决的语义问题。
该合同已接入任务类型永久门禁，当前仍失败，须在 XRT 源库验证修复后生成并同步
对应运行时。新的语言准入与局部异步测试通过不构成此运行时合同已满足的证据。

### BYVAL 捕获与调用提交边界（2026-09-08 补充，未完整验收）

CallbackCapture 的 transferAbi/destroyAbi 描述 frame 取得、回滚和销毁独立
owner；新增 invokeAbi 单独描述调用时的 CONTEXT/VALUE/BORROW/TAKE。
task.spawn 的调用者始终把受保护的借用交给 launcher。托管 BYVAL 参数先由
frame 独立复制或保留，直到调用前的分配全部完成，才脱离 frame 并交给 callee。
调用失败与 callee 重新绑定不改变此转移：callee 负责释放已接收的 owner；
调用前失败则由 frame 清理尚未提交的字段。类型和 invokeAbi 由 IR 验证，
Cgen 不依据 C 指针类型推断参数是否被消耗。

命名函数适配器是独立的 XLIR 函数。其 BYVAL 输入及省略参数的默认值临时存储，
必须先 LOAD、再在调用提交点 MOVE，最后进入 consuming CALL。没有 MOVE 的
LOAD 仍是借用，仿射所有权验证器必须拒绝；不能把验证器放宽为允许隐式消费。

error.cause 等拥有型错误接收者也属于普通表达式生命周期。继续读取 message、
code 或另一个 cause 时，lowering 保护接收者，在取得独立投影结果后释放它；
Cgen 不负责扫描链式成员表达式补释放。本轮已实际复现并修复 cause.message
每次留下一个错误对象的泄漏。

上述直接 callable 路径已有运行和故障 IR 验证，但不等于 boxed callable 完成。
当前双向 runtime-callable 桥仍只接纳默认参数模式；不能只删除这条准入限制。
下一步需要把两向桥接的解码、独立取得、调用提交、失败回滚和输出清理作为
明确 IR 契约：xvalue call frame 的借用输入不能直接交给 OWNED callee；
反向适配器收到的 OWNED 输入必须在装箱失败、调用失败和成功时都正确结束。
保留完整容器往返用例为必过门禁，不以直接 callable 的通过替代它。

### 双向 callable 参数程序（2026-09-08，承接上述 BYVAL 边界）

前节的参数计划现已落地为 CallableBridge / CallableBridgeParameter。每个闭合的
可支持签名拥有规范排序的计划；decodeAbi 指定借用 xvalue frame 如何取得 native
参数，encodeAbi 指定 native 输入如何成为 boxed 参数，invokeAbi 指定调用提交。
decodedDestroyAbi 与 inputDestroyAbi 分别覆盖两向尚未提交的拥有者；这两个销毁
程序不能用参数的物理 C 类型代替，因为同一种指针可能是借用，也可能已被消费。

字符串解码复制；error 解码取得引用；xvalue 的 borrowed 和 owned 解码分别是借用
与保留。反向 OWNED xvalue 直接转移进 arguments 槽，其 native 输入清空；未转移的
OWNED 字符串/error 由参数程序结束生命周期。变长尾仍只允许默认 borrowed 模式，
dict 变长当前不承诺反向桥。未支持的签名不能由 emitter 临时推导计划。

verifier 从共享语言/ABI 规则重算完整计划，含没有被某条指令使用的闭合签名；缺失、
错序、重复、范围越界或销毁/转移不一致均拒绝，C/header 输出也必须拒绝且不留内容。
失败路径清理先脱离槽，再运行析构；析构产生次生错误时保留原始错误。只有调用与
参数清理成功才允许继续发布结果。该参数边界已用真实分配故障和抛错析构验证，
但不能泛化为所有结果槽、Future 回调、任意名义动态类型或安全 DLL 卸载已完成。

### callable 结果槽与事务提交（2026-09-08）

结果传输现在也是 XLIR 程序：CallableBridgeResult 逐槽记录类型、encodeAbi、
decodeAbi 和 destroyAbi；签名计划另记录结果 carrier、结果范围、resultCommitAbi
与 frameCleanupAbi。多返回值的元组字段是这些槽的物理布局，不由 Cgen 重新推断
复制、转移或销毁。verifier 重算完整结果分区，缺失或篡改计划均在任何 C/header 输出前拒绝。

forward 先把结果装箱到私有 call-result frame，释放未转移的 native 结果并确认清理
成功后，才把结果移交调用者；反向先解码到私有 native carrier，清理 boxed frame 成功
后才写入输出。每个失败路径都用共享 error-scope 的失败位和拥有型诊断保存第一次失败，
不能仅依靠非空错误指针，也不能让次生析构错误覆盖类型转换、分配或调用错误。

boxed frame 的拥有权先通过公开 xrtCallResultMove 移到清理局部，再倒序使用
xrtCallResultSet(..., xrtValueNull()) 替换既有槽。替换不分配、先脱离旧值，再运行析构，
每槽独立观察错误；最后 Unit 释放空值和容量。不直接写 XRT frame 字段，不改变运行时 ABI。

返回闭包的解码使用可失败的 try-copy，并由结果事务回滚。显式 xcall 的返回签名现在
递归检查闭合 callable，仍拒绝返回 callable 中的 byref/out/c_vararg 等不支持参数，
不开放隐式 func-to-var。返回桥接的通过不能当作所有闭包分配路径已完成；普通投影
的后续收口见下一节，闭包构造、其他复制/装箱及迭代器构造仍须独立验证。

### 可失败投影与双向 callable 错误边界（2026-09-08）

普通 closure/string 投影、nullable closure/string 投影和 callable 字段读取现在
由 typed projectionErrorAbi、loopErrorAbi 及 lowered 物理结果 ABI 表达失败。
非空结果用 NULL_RESULT；nullable 成功允许空指针，使用独立 STATUS_OUTPUT 成功位。
verifier 要求紧随生产者的 ERROR_TEST 和不同的成功/失败出口，所有执行模式共享
同一物理状态观察器。错误对象分配失败不等于操作成功，不依靠 TLS 指针推断成功。

lowering 在释放临时接收者前建立失败出口；循环在首次取值前登记游标清理资源。
字段默认初始化的诊断位置属于声明，而异常出口属于实际构造调用点。safe member
访问已可准确区分给普通字段增加 nullable 与对已有 nullable 字段保持扁平结果。
语言值进入 erased 容器时，string/closure/error/Future 的装箱观察原生拥有者；
表达式必须另行结束原始拥有权，不能把装箱保留误当成消费原始闭包。

CallableBridge 另记录 forwardErrorBoundaryAbi 和 reverseErrorBoundaryAbi：
forward 是 NATIVE_CALLBACK 边界，未处理的失败通知原生调用方；reverse 是
ISOLATED_STATUS，调用 xrtValueInvoke 时建立局部作用域，返回显式失败给语言 CFG。
因此语言 catch 成功处理的内部调用错误不会污染外层 export/native 调用的失败位。
两向参数清理和结果清理都保留独立的首次失败位及可选诊断；无诊断失败后的析构
错误也不能取代首次失败。两个边界枚举和所有结果计划均被独立重算、变异拒绝并导出。

永久门禁 check_xlir_checked_projection.ps1 用真实语言的字段、索引、循环、空值、
catch/finally 做分配故障验证，并检查 IR 变异、重复生成、严格 C 与 C-host JIT/AOT。
这不代表 closure/cell 构造、全部容器/迭代器分配、生成器任务 ABI、native borrow
或 DLL/API 安全卸载已完成，也不抵消独立 XRT 的非末引用释放失败。

### 从消费次数到拥有者闭合证明（2026-09-08）

OwnerConsumption 现在枚举每个非 product 的独立 managed SSA 生产者，而不是仅从
消费操作反查。状态机区分未获取/已消费、借用 LOAD、已拥有及失败后的空结果；
在所有可达有限出口、以及同一生产者再次执行之前，拥有权必须已被转移或销毁。
互斥分支各消费一次有效；没有任何使用、仅观察、仅有一侧消费、循环反复覆盖和
LOAD/MOVE 后遗失均无效。NULL 字面量仍是可复制的空值，不产生真实资源义务。

失败结果是否为空由 IR 的物理结果契约确定，不能把所有失败一概视为空返回值。
原生调用的失败位与原生返回值相互独立：外部 string 借用转为语言字符串后，或
外部调用返回 Future/xvalue 后，即使调用失败，也须在失败分支 STORE 到明确的
回滚存储，并在保留首次失败的 unwind 内释放。内部结果 carrier、checked pointer
及已定义的空输出失败契约则没有成功拥有者。ASYNC_START/GENERATOR_START 的
BYVAL 参数消费来自目标参数 ABI，与普通 CALL 一致，不再被标为纯观察。

这一证明暴露并推动修正了未完成容器构造、bare func 初始化、属性 length/null
观察、字符串插值和 nullable 字符串 coalesce 的 lifetime 缺口。临时值的拥有权
来自物理生产者及精确 MOVE，不再由前端 RVALUE/LVALUE 分类决定是否清理。
array/list/dict/set 的部分容器进入统一 evaluation-owner 栈；插值累积值在下一段
表达式求值前就受保护。nullable 字符串在 present 分支转移，在 fallback 分支结束。
这些均为 lowering 输出的显式存储、转移、清理和异常边，不增加 Cgen 语义回查。

check_xlir_owner_completion.ps1 使用当前源码编译契约测试，并验证控制流图、IR
变异、重复 C/header/JSON、部分构造和原生失败结果的真实运行及 C-host JIT/AOT。
本次只补齐非 product SSA token 的退出/重获取义务：STORE 之后的存储 lifetime、
product 逐字段路径、alias/cell/capture、全部分配失败和安全卸载仍有独立验收义务。

### Product 逐字段路径证明与绑定事务（2026-09-09）

managed product 不再用整个函数中的字段消费次数判断正确性。PRODUCT_TAKE 和
PRODUCT_DROP 具有明确的 CONSUME_FIELD 输入 effect：只消耗 elementIndex 对应的
owner；其他字段保持可用。统一 OwnerConsumption 对每个 managed 字段运行相同的
获取/借用/拥有/空失败状态机，检查可达有限出口和循环重获取。整值转移或精确 MOVE
作用于全部字段。不同字段无需形成指数规模的组合状态，且不能在部分消费后再转移
完整 product。互斥分支可以各自完整拆解，或者一侧整值转移、另一侧逐字段消费。

结构验证仍限制 product 的合法输入和使用方式，禁止重复输入拥有者及含糊的聚合
别名；逐字段证明没有替代类型、精确存储 MOVE、借用生命周期及结果可用性检查。
PRODUCT_TAKE 的结果是新的独立 SSA owner，而不是指向原 carrier 字段的借用。
空失败 carrier 不允许 TAKE 获取成功字段。

声明拆解与多重赋值共用 xl6IrLowerProductBindings。RHS 仅求值一次，其私有 carrier
先通过 TAKE/GET 拆成字段；managed 字段立即进入现有 evaluation-owner 临时存储。
随后仍按从左到右的语言顺序求值目标并提交。索引装箱观察受保护字段，获取独立的
容器元素 owner；丢弃字段与其余临时拥有者通过共同的 outcome-bearing cleanup
结束。直接存储替换把旧值精确 MOVE 到独立拥有者，先提交新值，再清理旧值；
析构失败保留已提交的新值并中止后续目标，不能漏掉等待后续目标的字段。所有动作
均为明确 XLIR 指令及异常边，没有增加
Cgen 的字段补救、AST 回查或语义推断。

这证明 product SSA 和已覆盖绑定事务的资源义务，不表示存储/alias/cell/capture
的全路径证明已经全部完成。cell 载荷替换遵循下一节的显式交换契约。跨 await 的新 product 绑定
专项、完整分配失败边界及 DLL/API 安全卸载仍须独立验收。

### 存储替换、cell 显式交换与析构重入（2026-09-09）

替换语义分为新载荷获取、发布、旧载荷清理三个阶段。获取失败不得写入目标；
发布之后才析构旧载荷。旧值析构失败时，新载荷已经提交，错误仍须中止后续操作；
不能回滚目标，也不能在析构回调返回后再执行一次过时的 STORE。这样回调总能观察
到有效的新载荷，并且回调对同一目标的再次写入不会被外层替换覆盖。此约束针对
语言求值、所有权和重入顺序，不承诺线程间原子交换或无锁并发存储。

CELL_EXCHANGE 消费独立的新载荷 owner，将其发布到已有 cell，并返回旧载荷的
独立 SSA owner。指令自身没有分配、析构或用户回调；返回值进入既有临时清理槽，
由显式清理操作和失败控制流处理。CELL_STORE 仅允许无需析构的 trivial 载荷；
managed 载荷使用 CELL_STORE 在 module verifier 中被拒绝。cell 对象本身并未被
释放或重新激活，async liveness 将 exchange 视为已有 cell 的使用，借用分析则
将它视为相应载荷借用的失效点。CELL_NEW 的分配失败仍是独立的未完成边界。

普通、间接参数和全局存储使用不调用用户代码的 LOAD/MOVE/STORE 表达同一交换。
xl6IrLowerReplaceStorageValue 统一普通赋值、复合赋值、条件赋值和 product 命名
目标的替换；旧载荷清理复用 xl6IrLowerCompleteOwnedCleanup，保留最初的失败位，
即使该失败没有可分配的诊断对象。同步、async 和 generator 的 MOVE 共用一个
物理输出函数，消费已验证的全局 C 名称、参数间接 ABI 或 frame 字段/init bit，
不通过 AST 或表达式文本判断目标。

check_xlir_storage_exchange.ps1 覆盖非法 IR 的 C/header 输出前拒绝、真实 cell、
global/byref/generator 析构重入、重复生成和严格 C/C-host JIT/AOT。product owner
门禁另覆盖可变 var cell 的逐字段赋值，以及首次 77/无诊断失败不被后续 99 覆盖。
这些测试不替代 product 跨实际 await 绑定、所有 alias/capture 路径或全部构造
失败的专项验收。

## 2026-09-09：受保护的 continuation 帧借用与独立操作失败

SSA 的借用跨挂起存活，不等于该 SSA 应成为新的拥有者。隐式 retain 会让对象
越过原本的词法清理边界，直到 Future 已完成、catch/finally 已离开后才析构。
frame finalizer 不能替代源程序中的清理路径，也不能用清空线程错误掩盖晚析构。

BORROW_STORAGE 是显式、非拥有的帧传输 ABI。borrowFrameSlot 指向同一机器内
保护它的拥有型存储槽；该借用无独立析构、无 init bit，也不执行 copy/retain。
共享的 async/generator 分析从既有借用 provenance 推导该关系，验证器独立重算：
只接纳非投影的私有 local/temporary 或按值捕获的参数 LOAD（含透明表示别名），
不接纳 cell、global、捕获存储、间接参数或有 STORAGE_ADDRESS 逃逸的存储。
每次借用存活的挂起必须也包含其保护槽。已有 CFG 借用验证继续保证所有使用
均发生在该存储失效之前；保护关系不能以最后一个状态编号的区间近似代替。
未符合条件的来源不会自动变成非拥有指针，其显式快照及清理仍需分别证明。

Product 的 MAKE/GET/TAKE/DROP 使用同一类型与所有权合同及共用 C 发射函数。
容器替换另区分 native boolean commit 与用户析构/回调失败：前者成功不意味着
后者成功。操作使用隔离的错误结果 ABI，紧邻 ERROR_TEST，先决定失败控制流，
再清理操作数；native false 的赋值语义仍由独立的显式 IR 分支处理。

非空 map/object 字面量的插入也可能替换重复键并析构旧元素，因此 Typed 节点
以 constructionErrorAbi 单独记录插入失败，不与操作数清理效果混为一谈。
这不是对所有构造分配失败均已完成的声明。函数值的 variadic 容器使用与普通
调用相同的早期拥有者登记；动态容器和 persistence 调用的参数使用统一求值
清理事务，在后续参数失败时仍有明确的拥有者和逆序释放路径。

check_xlir_product_await_owner.ps1 已接入核心第三组，覆盖两次真正的 pending
await、逐目标提交、输入 reject/cancel/close、析构失败与无诊断失败、错误 IR
准入、严格 C、C-host JIT/AOT、确定性及内存基线。该门不等于全部 generator
借用形态、共享引用快照、CELL_NEW/closure 构造或 Future 最终载荷的完整验收。

## 2026-09-09：yield 观察边界与无诊断的帧清理失败

YIELD 与 AWAIT 同样允许任意外部执行插入。统一 value-lifetime CFG 分析将其视为
共享 global/extern-global/cell 借用及可变投影的观察失效边界；私有存储未被写入或
转移时，其借用仍可继续使用。不能因为当前 lowering 每次都重新 LOAD，就允许
优化后的 IR 携带失效共享借用跨 yield。帧存活分析与借用有效性是两项独立证明。

generator_frame_loan 契约在合法 Lowered XLIR 中复用参数的首次 LOAD，再重建
generator 机器：string 与对象参数的两处 SSA spill 均使用 BORROW_STORAGE，
无额外 retain、析构或 init bit。对共享字符串作相同复用，则必须在 C/header
输出前拒绝。这是 IR 合同的优化形态测试，不代表已新增通用 CSE 优化 pass。
基线与复用后的 C 分别执行正常迭代、各个 yield 后提前关闭、共享存储更新及
参数析构错误，检验运行结果、析构次数、finally 次数与分配基线。

物理 frame-free 回调返回 void，并不意味着其语言清理是 no-raise。共用 activation
cleanup 结束时必须向包围的调用/清理 scope 传递完整 xl6_error_result；仅恢复
error 指针会把 failed-with-NULL 降格为成功。此修正复用既有错误 ABI 和共用发射
函数，不增加 generator 特有的错误猜测，也不回溯 AST/HIR。它不宣称没有包围
scope 的所有外部终结器边界已经处理完毕。

## 2026-09-09：continuation 的 owner、值投影与消费边（基础实现已回归）

owner-views 实际反例推翻了“一项 SSA spill 就是一份物理拥有者”的假设。
VALUE/nullable 等透明表示并不创建新 owner；即使各别名的借用/消费 CFG 合法，
逐 ValueId 建立 MOVE_OWNED 字段仍会重复析构。takeInstruction 只寻找相同 ValueId
且只有一个消费点，也不能表达别名消费、分支消费或一条调用消费多个 owner。

continuation 表示分离以下事实，而不是在 Cgen 中追踪别名后去重：

- 物理驻留：同一 canonical owner 在一个 activation 中只有一份拥有型存储和
  init bit。借用的词法存储保护关系仍单独保留，不能把 owner 与借用混为一谈。
- 值投影：每个恢复入口显式给出需要恢复的 SSA ValueId、TypeId 和来源物理槽。
  一个物理 owner 可投影成多个合法视图；视图自身不 acquire/drop。
- 状态进入：spill/install 必须按实际 CFG 路径执行，不能只凭“最小状态编号”
  推断某个 owning 字段已初始化。分支、循环及重入激活需要确切的状态证明。
- 消费边：从共享 operand-ownership catalogue 推导完整的逐指令 transfer 动作，
  用 canonical owner 身份关联别名，支持互斥分支上的消费以及多拥有者输入。
  每个动作的提交时机必须明确；不能让失败的交付既遗失 owner 又清除其析构位。

验证器从源独立的指令/类型/CFG 重算精确 live-set，并逐字段检查规范 action
序列与切片。async 与 generator 共用 xl6_ir_continuation_liveness.c 的反向数据流，
构建及验证都不以已有 frame/live/action 表作为分析依据。Cgen 只安装、投影和
清除已验证动作，不按 VALUE 链、源变量名或 AST/HIR 推断所有权。

基础实现已通过永久 owner-views 门禁：相同 owner 的单视图与双视图均只有一个
MOVE_OWNED 槽，分别拒绝 20/26 项非法 IR；包括同步删除 live alias 与对应
project 并修复全部切片的反例，验证器仍从 CFG 检出缺失。两版各 300 次执行、
1,500 次平衡分配，检查 double-free/invalid-free/use-after-free，严格 C 与
C-host JIT/AOT 通过。generator frame loan、product await、owned activation、
async terminal 和 value lifetime 回归也通过。

独立 owner-cfg 门禁进一步覆盖四个有效 Lowered generator CFG：互斥分支通过
不同别名消费同一 owner、跳过首个 suspension、循环重新获取，以及恢复后
STORE 将 owner 移入局部存储。四个机器共五条消费边、五处 resident install，
15 项非法 IR 被拒绝；3,500 次实际执行、20,200 次平衡分配、每个 yield 的
提前关闭、严格 C/C-host JIT/AOT 与 C/H/JSON/binding 确定性均通过。

以上不证明所有 continuation 的 owner 生命周期已经完成。单调用消费多个 owner、
async 多别名跨挂起、acquired borrow 的重新激活和失败清理仍需扩展正反例。
非私有借用、detached LOAD/MOVE、构造失败、Future 最终载荷与发布准入继续保留。

### 2026-09-09：物理槽移交与跨两次 await 的消费边

嵌套 async continuation 需要新建 activation。旧 Cgen 按 live membership 搬运
字段；同一个 owner 的两个 SSA view 会搬两次，第二次从已清空的旧字段取值，
把新 activation 的 owner 覆盖成 NULL。两个 pending await 的有效 IR 反例实际
返回 30，而正确值是 35。仅共享物理 layout 和 resume project 还不足以保证安全。

现在每个 async suspension 独立拥有 firstHandoffAction/handoffActionCount，
HANDOFF_RESIDENT 只枚举唯一物理槽，并记录 canonical ValueId（storage 为零）。
共同 action 构造/验证从已重新验证的 live-set 推导准确序列，检查完整性、顺序、
字段及切片。Cgen 的 nested activation 只执行这段命令，不再读取 live membership
作为所有权移交程序；init bit 与指针共同转移。Generator 的 activation 原位
持续存在，因此没有这类 handoff。async 的动作准入也明确包含消费动作和 handoff。

永久 owner-calls 门覆盖一个 generator 和两个 async 的三组有效 SSA owner 区域：
跨两个 await 的同一 owner 多视图，以及 generator/async 单 CALL 消费两个 owner。
三组共六条精确消费动作，94 个 C/header 输出前拒绝反例，包括一致删去消费动作
或 handoff 并修复切片。8,500 次执行、7,200 次真实挂起、116,502 次平衡分配，
ready/pending、两次输入的 resolve/reject/cancel/close 组合、消费调用失败、两处
generator 关闭与严格 C/C-host JIT/AOT 通过。这关闭上述具体表示缺口，仍不把
有限测试等同于所有 callable/borrow/分配失败/最终载荷生命周期已完成。

### 2026-09-09：挂起准备失败也是显式 await 失败边

实际分配故障证明，原先 next-frame 或 continuation 注册失败会直接关闭输出
Future，把内存错误留在线程错误槽中，跳过 await 所在的 catch。内存平衡本身
不能证明语言语义正确。每个 AsyncSuspension 现在明确引用 setupFailureEntry，
验证器独立要求它就是该 await 的 RESUME_FAILURE 入口；其后保留原有 lowered
cleanup/ERROR_TEST/handler CFG，不另造一个 Cgen 异常流程。

注册是一项所有权事务。HANDOFF_RESIDENT 将唯一物理槽移交给候选 activation；
ROLLBACK_HANDOFF_RESIDENT 以相反顺序归还尚未受理的槽及初始化位。完整动作序列
由 IR 构造并验证。Cgen 的程序可达性包含 setupFailureEntry；准备或注册失败时
在当前 activation 内进入该入口，已经移交时先执行回滚，并归还外层 Promise
引用。动作不由 emitter 从 SSA 视图或语法重新推导。注册成功后（包括同步执行
ready Future）仍由 runtime 接管候选帧，不触碰可能已经释放的 next 指针。

owner-calls 专项扩展到同一 owner 跨三次 await，覆盖普通与已持有 outer Promise
的嵌套 activation。四个连续真实分配故障点覆盖新帧以及注册内部的三次分配；
3,840 次故障均进入源码 catch，后续 await 与消费结果正确且 TLS 无残留错误。
8,500 次常规执行、7,200 次常规真实挂起及故障矩阵合计 205,462 次分配释放平衡；
206 个非法 IR 反例、严格 C、C-host JIT/AOT 和确定性输出通过。此处没有将最终
返回载荷分配失败、所有 borrowed spill 失败、无诊断错误和全部 finally 组合
一并宣称完成；最终载荷分配仍有独立的 CLOSED/TLS 残留反例。

### 2026-09-09：拥有型返回载荷的提交事务

AsyncMachine 现在持有 resultCommitAbi=OWNED_TRANSACTION。原先按 string、
xvalue、closure、error、Future 和 trivial C 表示分别拼装的 RETURN 已合并，
销毁操作直接由已验证的 AsyncResultAbi 选择。这个合同区分尚未创建 Promise、
已创建 Promise 但结果盒分配失败、以及结果盒未被 ResolveOwned 接管三种状态：
第一种回滚后向调用方报告启动失败，后两种释放未受理的结果并以原始错误完成
已有 Future，不把内存错误转成 CLOSED。成功提交后只有 Future 拥有结果盒。

结果回滚使用独立的已知失败作用域。对象或闭包析构的后续诊断/无诊断失败不能
替换首次分配错误；此作用域的结果不传播到恰好执行 continuation 的生产者
foreign-call 作用域。最终向 Promise 发布错误后清空当前 TLS，生产者继续成功。
兜底 MemoryError 使用 XRT 静态类别对象，不为 OOM 再分配诊断。

同一 owner 跨三次 await 的专项新增 1,600 次最终结果盒分配失败，Future 全部为
FAILED/MemoryError，生产者错误作用域仍成功。独立 result-commit 专项覆盖 int、
string、class、closure、error 五种源码可表达的载荷，以及提前返回、ready 输入
和真实 pending 输入。测得提前返回在 arm 后恰有两次 Promise 内部分配和一次
结果盒分配，resume 只有一次结果盒分配，逐点全部注入失败。对象/闭包还覆盖
析构成功、诊断失败、无诊断失败。直接 async future/task 返回仍遵守已有语言
拒绝规则；嵌套 Future 作为 task/callable 载荷的独立 worker 路径没有因此验收。
任务池/无挂起 ready worker、所有剩余载荷生命周期、completion 后帧析构失败
与 DLL code lease 仍须独立验证，不能由这个返回提交专项推出全局完成。

### 2026-09-09：activation 清理先于 Future 发布

AsyncMachine.completionAbi=RETIRE_BEFORE_PUBLISH 是 RETURN 和 THROW 的共同
完成协议：先执行已有且独立验证的 destroy-action 程序，清空各 resident 的
初始化位，再发布成功载荷或最终失败。帧内持有值的退休与 frame 内存/outer
Promise 的物理释放分开；outer 引用必须保留到结果发布结束。运行时随后调用
frame-free 时，已退休的 resident 不会再次释放。这里复用同一 IR 动作序列，
没有在 Cgen 重新推导活跃性、所有者、别名或源语法。

退休函数显式接收并返回拥有型 (ok,error)，在独立错误作用域内按首次失败
合并全部析构结果。已有 THROW、结果盒分配失败和无诊断失败不能被后续析构
替换；成功结果若遇到帧析构失败，先回滚尚未发布的结果，再发布失败 Future。
作用域不把语言失败传播给执行回调的外部生产者。XRT 的 PromiseReject 禁止
NULL error，因此无诊断失败只在最终 Future 传输边界映射为静态 XERR_INTERNAL；
真正的分配失败仍在失败发生处固定 MemoryError，而非把所有清理失败归为 OOM。

专用正例涵盖两个 borrowed class 参数的最后引用和独立拥有型返回对象。
双析构按已验证动作逆序执行，结果回滚析构也可能失败；覆盖原始诊断、原始
无诊断失败、cancel/closed、提前返回、ready 和 pending。发布观察者确认帧
持有值及失败时返回值均已销毁，生产者作用域仍成功；另逐点注入 Promise/
结果盒分配失败。外部主动取消导致 continuation 被跳过的 teardown、无 machine
worker、嵌套 Future 载荷与完整 code lease 仍是独立待验收项。

### 2026-09-09：async Watch 载体、取消投递与拥有引用的存活期

AsyncMachine.transportAbi 复用 CALLBACK_TRANSPORT_FUTURE_WATCH，
cancellationAbi=AWAIT_ONCE 固定协作式取消的语言入口。首次实际挂起建立唯一
outer Promise；后续 activation 只移交该 Promise 与 cancelObserved 状态位。
每个 activation 内嵌 Watch，并保留其输入 Future 的传输引用，READY/PENDING
均执行同一个已验证 resume 程序，不能被运行时 Continue 的取消分支跳过。

取消请求在下一次 await 恢复点作为 CANCELLED 输入投递一次，不强杀线程，也
不取消可能被其他消费者共享的输入 Future。已有 typed catch/rethrow/finally
决定最终结果；catch 和显式 finally return 可以按既有异常语义处理或覆盖它。
投递状态必须跟随 handoff，以便 finally 内再次 await 时不重复注入同一请求，
使实际清理得以完成。自动析构仍保护主异常，显式 finally 操作仍可覆盖它。
本协议不保证尚未完成的输入立即唤醒；它遵守 task.cancel 的协作取消约定。

同一测试暴露了所有权与读活跃性的区别：某些 frame owner 虽然不再有后续读取，
却没有任何 RELEASE/消费动作释放其责任。原先只移交 live membership 会让旧
物理帧在 finally 再次挂起时擅自销毁它们，并把析构失败泄漏给生产者。现在
HANDOFF_RESIDENT 按唯一物理槽枚举：所有带 init bit 的拥有槽都移交，trivial
槽只需按读取活跃性移交；回滚采用完全相反顺序。初始化位清零表示责任已被
明确释放/消费，不再有读取不能充当隐式销毁命令。构造与 verifier 使用同一
规范动作程序，Cgen 仍只执行动作，不进行现场保活或别名修补。

Watch 注册本身不分配，跨挂起从 next frame + Promise/cancel/continuation node
四次分配收缩为一次 next-frame 分配。真实分配失败与真实注册拒绝分开测试：
前者覆盖每个现存分配点，后者使用原生边界给出的空 Future 验证已经移交的
resident 和 outer Promise 回滚及源码 catch。未伪造 allocator 触发次数。
传输源 Future 的最终载荷析构、借用重新激活、跨线程取消竞争与完整 DLL/code
lease 仍须独立验证，不能由本节的取消恢复专项宣称全部生命周期完成。

## 2026-09-09：await 结果取得、传输租约与操作数清理分离

源 Future 的 Watch 引用只保护结果观察和 copy/retain/duplicate，不能把语言
拥有引用的最后一次析构推迟到结果发布以后。machine.sourceLeaseAbi 固定为
CAPTURE_THEN_RELEASE，构造、独立验证器、文本/JSON 与 Cgen 准入一致要求这一
协议。resume 先按 resultAbi 取得独立结果，再结束传输租约，之后才执行语言 CFG。
取得失败优先于租约清理失败；清理失败时回滚已经取得的拥有型结果。完整失败位
在独立清理作用域中保留，无诊断传输失败在既有 Future outcome 边界编码为
静态 INTERNAL。共享输入的其他拥有者保持其自身寿命，不承诺其析构提前发生。

对 await 调用、索引或条件式产生的临时 Future，lowering 必须先分流 await
成败。失败路径在原异常保护下释放操作数；成功路径复用 CompleteOwnedCleanup，
临时保存拥有型结果、清理源操作数，再提交或回滚。不得用裸 RELEASE 后读 TLS
指针代替完整失败事务，也不得将这种源表达式特判放到 Cgen。

扩展验收同时揭示两项尚待完成的共用模型工作：LOAD 后 MOVE 虽然清空存储，
其 SSA 结果仍被静态分类为借用，导致 throw await 的拥有型消息准入失败；字符串
返回路径的 RETAIN/复制分配失败仍可能缺少显式失败边。下一阶段需要表达原子
取出拥有权及可失败取得操作，而不是放宽 ERROR_RAISE 或在 async Cgen 补查来源。

## 2026-09-09：原子存储取出、可失败字符串取得与入口适配

STORAGE_TAKE 直接从已激活的非 cell 本地/临时槽产生拥有型 SSA 值，并清空槽位
及 continuation 初始化位。LOAD 仍为借用；所有权不因后面出现 MOVE 而被追认。
本次先迁移共用 evaluation cleanup 的成功提交，存储激活 CFG 证明唯一安装和
退休，值消费验证器证明取出后的独立责任。其他 LOAD/MOVE 协议尚未全部迁移。

STRING_COPY 使用 NULL_RESULT，STRING_NULLABLE_COPY 使用 STATUS_OUTPUT；后者
必须区分合法的空字符串指针和分配失败。lowering 立即发出 ERROR_TEST 与失败
路由，Cgen 只执行已经验证的结果协议。evaluationErrorAbi 同时包含隐式取得与
操作数释放的潜在失败。RETURN 在提交退出前可能取得失败，因此同一 origin 的
BEFORE_NODE 可以分别拥有 RETURN 和 THROW 计划，唯一性按 origin/point/kind
验证；二者具有各自的词法清理、handler 和 finally 路由。

普通函数的 BORROW 参数适配不再是扁平 LOAD/COPY/STORE 前缀。字符串副本取得
成功才安装；失败只释放此前安装的 BORROW 副本，并释放全部已经移交的 BYVAL
输入，包括签名中更靠后的参数，不释放调用方的借用。验证器检查入口成功链、
失败边唯一入口及精确释放集合，不能靠伪造诊断 origin 绕过终端清理。字符串
转换及插值中的拥有型临时操作数也接入共用 evaluation owner 清理事务。

源 Future 与入口适配专项通过 80 项输出前拒绝变异、900 次 async 真实分配失败
及 300 次入口真实复制失败，后者包括 nullable null、多个副本及后部 BYVAL
对象。这不是全局完成：命名适配器默认参数仍有限制，手工构造 continuation
SSA owner 图的专项还须为新复制失败边补齐回滚，完整发布与其余生命周期仍待验收。

## 2026-09-09：命名适配器默认参数失败与字符串拼接结果

命名适配器拥有独立的 MAY_RAISE 函数结果协议，不再以被适配目标的 no-raise
效果拒绝可失败默认参数。全部传入 BYVAL 槽在计算任何默认参数之前就加入共用
evaluation owner 事务；默认值取得独立所有权后也加入该事务，所有输入绑定成功
才提交给目标。默认表达式保留自己的诊断及退出计划来源，不借用函数值创建点
的调用方词法清理。可抛错默认值及带/不带诊断的失败已验证；一般适配器的可失败
默认值析构与独立函数边界验证仍须继续完善，不能将此阶段视为完整生命周期验收。

STRING_CONCAT 的物理失败协议是 NULL_RESULT，必须立即 ERROR_TEST，禁止在
STORE/RETURN/清理之后才检查可选的线程诊断。普通二元、插值累积与复合赋值共享
evaluation owner 的成功和失败清理；复合赋值的 RHS 临时值已接入该事务。Cgen
直接使用既有物理结果测试，不增加拼接或适配器特有的源码追溯。

永久专项覆盖普通/嵌套/插值/复合/BYVAL 默认适配/inout 提交六种形态，逐点故障
验证原始 MemoryError、失败返回和所有权平衡，inout 失败不得提交新值。旧拼接
成功误报反例及其源码冻结于 build/tmp/xlir_adapter_outcome_v1_negative_snapshot.json。

扩大测试仍复现 STRING_DUP 和 I64_TO_STRING 的未检查失败，带字面量前缀的插值
甚至会成功返回剩余后缀。这些尚未修复。下一步应统一字符串分配操作的结果合同，
在完整操作数/字面量/payload 绑定后构造检查边；不得把新控制流加入后继续假定
“最后一条指令就是刚才的生产者”。相关反例保留在 xlir_string_allocation_v1_remaining
产物中，生成成功不能作为语言语义已经闭环的证据。

## 2026-09-09：统一字符串分配结果与操作数构造事务

12 类字符串分配操作统一由 xl6IrRuntimeStringAllocationErrorPhysicalAbi 分类。
STRING_NULLABLE_COPY 使用 STATUS_OUTPUT，区分合法 null 与失败；其余 11 类使用
NULL_RESULT。Typed 字符串转换标记 MAY_RAISE，lowering 在完整绑定 runtimeOp、
immediate、payloadType 和操作数以后立即生成 ERROR_TEST。插值片段的物理生产者
来源与整条表达式的失败退出计划可以不同，但两者都在 lowering 明确绑定，Cgen
不追溯 AST。自定义 __toString__ 使用目标已声明的调用结果协议传播失败。

派生字符串运行原语必须履行该 NULL_RESULT 合同：每次拼接或字段转换失败后
释放已经取得的部分字符串并立即返回，不得 abort 或继续产生缺失前缀的结果。
中央错误运行时声明同时覆盖 nullable 状态结果需要的 xrtSetErrorKind。

容器代数、remove 和 typed member 的接收者在求值后取得独立拥有者，并在后续
操作数求值前加入共享 evaluation owner 事务。成功与失败都通过共用清理路径；
managed 结果在清理成功后才提交。显式持有接收者还保证 out/ref 参数求值替换
原存储时，操作仍作用于最先求值得到的对象，而不是已经替换的新对象。

tagged enum 的部分构造对象也属于该事务。已安装载荷由对象持有，后续载荷
失败时一起回滚；全部安装成功后用 STORAGE_TAKE 原子取出并清空临时槽，再将
拥有者交给消费方。此次不宣称 enum 分配及载荷插入原语自身的失败结果已全部
收敛，相关 TRAP/分配结果协议仍在语言正确性工作范围内。

raw callback 来源约束按 LOAD 定义所在基本块验证 STORE，不再假设后续 CALL
与参数生产者在同一块。独立验证器仍要求精确 capture-free thunk/受约束参数，
没有放宽 ABI 或将来源识别转移到 Cgen。

## 2026-09-09：continuation 合成 IR 的完整所有者区域

owner views/CFG/calls 专项按 YIELD/STORE 的 SSA 操作数定位真实定义，不依赖
COPY 紧邻 YIELD 或固定十指令函数。将可失败生产者替换为透明别名时，同时将
它的 ERROR_TEST 改为原成功续接的 JUMP；仍然存在的分配操作保留真实失败边。

测试人为延长一个 SSA owner 的寿命，也必须承担这段新寿命中的失败责任。
测试专用编辑器沿 CFG 计算该 owner 的活跃状态，只在明确未捕获的字符串分配
失败出口建立 STORE/RELEASE。字符串释放不可失败，两条指令放在词法异常清理
区之前；STORE 不是清理指令，不能塞进 ERROR_UNWIND_BEGIN/END。编辑器同步
修正指令切片、控制资源身份、pending exception/callback 索引及统计，随后重建
全部相关 continuation machine，再由独立模块验证器和 C/header 门复核。

这不是生产编译器的兜底修复 pass。正常 CALL/YIELD/STORE 消费动作仍有精确数目
和目标检查；失败路径的 frame 所有权转交必须对应这些明确的回滚 STORE。真实
故障覆盖第一次挂起前、恢复后仍驻留的 owner、循环重新取得和 async 启动。
async 首次 await 的建立失败若被源码 catch 捕获，必须继续执行该 catch；测试
不得将所有分配故障一概断言为失败 Future。无副作用状态检查使用 FutureResult，
而不是会把失败写入当前错误上下文的 FutureValue。

## 2026-09-09：枚举构造原语的显式失败结果

tagged enum 的 constructionErrorAbi 由构造本身决定，空变体也可能分配失败，
不能依赖字符串参数间接提供错误退出计划。TAGGED_ENUM_NEW 的物理结果为
NULL_RESULT，TAGGED_ENUM_PAYLOAD_SET_NEW 为 FALSE_RESULT；两者必须紧邻
ERROR_TEST，并在绑定完整 EnumVariant/EnumPayload 元数据之后建立控制流。
插入操作在成功、失败两条路径上均消费载荷，部分枚举继续由共享求值事务拥有，
只有全部载荷插入成功后才能 STORAGE_TAKE 提交。

C 后端只实现该固定物理协议：创建对象或安装 tag 失败时释放局部半成品并返回
NULL，不再 abort；载荷插入保留原生布尔结果，由 IR 的失败边处理清理和传播。
没有新增 AST 追溯、字符串名字判定或生成后修补。

表达式模式绑定还有一个独立的规范化边界：受管理的载荷必须进入拥有者槽；
未被读取的平凡载荷可以被死存储规范化为 DISCARD，但仍须来自同一个模式绑定
及精确的 EnumPayload 投影，并属于当前函数。验证器核验这两种规范形态，
不通过保留多余平凡存储、跳过未用载荷或免除托管清理来规避问题。

这仍不是全部装箱协议的完成证明。当前标量/字符串的部分装箱失败会延迟到
SET_NEW 的 false 才被观察；数值枚举的 I64 装箱结果在 TYPE_BIND 之前尚未检查，
已构造出该点分配失败导致进程退出的反例。下一阶段必须使每一种可失败装箱
生产者本身具有规范结果、即时观察和明确的求值失败边界；不能把所有标量读取
一概标为可失败，也不能依靠后续插入或 Cgen 的 NULL 特判代替 typed 转换效果。

## 2026-09-09：装箱结果与具体化后的转换边界（仍在验收）

I64/U64/F64/foreign-float/time/string/nullable-string/tagged-nullable/union
九类装箱现在共享 NULL_RESULT 合同，并在完整 RuntimeOp、payloadType 和
runtimeIdentityType 安装后立即 ERROR_TEST。语言 null 是非 NULL 的 XRT 单例，
因此 nullable 的成功空值不与分配失败混淆；数值枚举类型身份只在装箱成功后绑定。
union 盒分配失败返回 NULL，不在 C helper 中终止进程。

转换效果属于上下文边界，不属于每一次标量读取。声明初始化、参数擦除、返回、
集合元素及多赋值分别保存或汇总失败效果。泛型具体化完成后，统一刷新声明初始化
表和节点隐式转换信息，然后重建 ExitPlan；不能只替换 TypeId 并保留模板中的
NO_RAISE，也不能在克隆器中另写一份只认识 callable 的初始化规则。

多返回值仍只求值一次，字段先投影并注册临时所有者，再按目标物理类型转换。
命名存储和容器元素共用已求值字段转换入口。装箱借用原字段而产生独立的盒所有者；
原托管字段在成功转换后释放，未提交的其余字段在失败路径上由共享求值事务回收。
后端只接收明确的装箱、标量转换和消费指令，不补写目标类型转换。

永久装箱门已同时保留 24 种实际上下文和逐处分配失败测试，但尚未通过完整验收。
它暴露的容器创建/插入与 copy/clone 失败路径属于语言核心义务，不能从测试中删除，
不能将相关错误统一放宽为“任意异常即通过”，也不能将此问题转移到延期标准库。

## 2026-09-09：容器分配与回调失败的统一结果

容器构造与复制的失败效果现在由操作本身表达：ARRAY_NEW、INT_MAP_NEW、
SET_NEW、OBJECT_NEW、XVALUE_CLONE、XVALUE_DEEP_CLONE 采用 NULL_RESULT。
ARRAY_APPEND_NEW 和 SET_ADD_NEW 采用 SCOPED_BOOL_RESULT：原生布尔结果成功，
且隔离的回调错误结果也成功，才允许进入成功边。回调失败不能因原生返回 true
而丢失；原生插入失败也不能被随后析构产生的次要错误覆盖。

上述生产者在完整元数据安装后紧邻 ERROR_TEST，由共享失败 CFG 和求值所有权
事务处理回滚。字面量（包括空容器）、默认字段、泛型具体化后的 copy/clone 与
插入效果同步成为 MAY_RAISE。临时容器复制的源对象在成功和失败边均受保护，
不把异常观察延迟到 TYPE_BIND、下一次插入或最终返回。Cgen 仅实现物理结果
协议与已验证的分支，不增加源 AST/HIR 追溯或生成代码修补。

06342732 源码的永久装箱/容器门完整通过：32 种上下文、6,240 次真实分配故障、
478 项畸形 IR 拒绝，严格 C、生成产物 JIT/AOT 及确定性均通过。额外使用预先
分配的错误对象检验 true-result 回调失败，以及插入失败后确实抛错的析构，
避免故障注入同时阻止测试错误对象创建而得到假阳性。C758 检查点中的 43 个
失败已被修复，其历史负例证据仍保留。这不是所有运行原语和任务生命周期的
完整验收证明，其他独立语言边界及最终发布义务仍然有效。

## 2026-09-09：生成函数的独立异常边界与默认值清理

具名适配器是一次独立函数调用，不是创建函数值的 NAME 表达式，也不是被适配
函数的参数声明。新增 FUNCTION_FAILURE 退出点明确记录这一边界：没有创建方
的词法清理项、scope depth 或 catch region，规范路径只有该适配器自己的
PARAMETERS 与 TERMINAL。默认表达式中发生的调用、构造、转换和求值清理失败
都归入这一边界；诊断来源仍可以指向原默认表达式。

适配器不再维护专属的“逐个 RELEASE 默认值”或手工拼接 call/test/throw 路径。
默认值求值完成后，消耗型参数按统一提交规则移交被调用方；借用型默认值则由
共享 CompleteCall 清理事务保有。结果在清理完成之前只是待提交的所有者：
清理成功才返回，清理失败则释放单值或产品结果，并通过适配器的独立出口抛出。
这对被适配函数本身 NO_RAISE 的标量/void 返回同样成立；适配过程仍可失败。

验证器要求每个具名适配器恰有一个函数失败计划，并独立核验两步规范路径。
适配器每个 ERROR_UNWIND_BEGIN 必须引用自身计划，不能删除标记，也不能替换为
创建位置的 try/catch 路径。可失败释放必须属于显式结果事务；待提交结果的
成功转交、失败回滚继续使用现有的精确存储/CFG 验证，不添加 Cgen 特判。

永久门直接调用生成的适配器，检验其自身的 ok 和载荷，而不是仅通过外层调用
观察 thread error。后者可能把错误的“成功 + 残留诊断”遮蔽成一次表面正确的
异常。测试覆盖独立的创建方 catch、NO_RAISE 目标、标量/对象/产品/void、默认
析构有诊断/无诊断失败、原始错误优先及真实分配故障。FB983813 的专项通过
288 项畸形 IR 拒绝、400 次执行与 2,080 次分配失败。本次 Cgen 源码没有改动。

## 2026-09-09：闭包创建的结果与创建方异常边界

CLOSURE_MAKE 现在是一项 MAY_RAISE / NULL_RESULT 的规范分配操作。具名函数值
与匿名函数表达式的 constructionErrorAbi 均记录创建效果；lowering 在创建后
立即发射 ERROR_TEST，并复用共有的异常保存、词法退出和求值所有权清理路径。
验证器拒绝缺少检查、检查其他值、伪造调用目标或合并成功/失败边的 IR。

匿名函数节点同时标识创建表达式与被调用函数体，但两者的执行边界不同。
BEFORE_NODE 退出计划以创建表达式所在环境确定所属函数、活动局部和 handler；
函数参数与函数体清理仍属于匿名函数自身。ExitPlanEvaluationOrigin 在计划构造
和独立验证时统一表达这一差异，不能以扩大创建方的 may-raise 效果来掩盖
本应进入本地 catch 的失败，也不能使用函数体的词法边界清理创建方局部。

唯一具名函数的完整签名由语义层已有的候选解析器统一提供给类型推断，条件
表达式不再遗漏这种函数值的规范类型。重载/泛型候选仍要求明确签名消歧；
此处没有依赖声明顺序选择候选。

可空字符串捕获新增 COPY_NULLABLE_STRING ABI：NULL 不分配、不变成空字符串；
非空输入产生独立副本，复制失败时整个环境创建失败。普通 COPY_STRING 与
可空复制的 TypeId 合同由验证器分别检查；同步、async 和 generator 的渲染器
消费同一捕获 ABI，不读取原表达式或根据名称重建空值语义。

当前 C11F4AD5 检查点的永久闭包门覆盖具名/匿名、默认参数、条件表达式两条
分支、字段恢复、混合捕获、可空值和创建方 catch。工厂返回结果与闭包调用
结果分别检查，不能把调用失败写回成工厂失败。同步逐点故障门覆盖 13 上下文、
820 次真实分配失败；93 项拒绝变异中 90 项属于同步模块、3 项属于 suspension
模块。await 后和 generator 挂起前后的空/非空捕获已通过严格 C、C-host JIT/AOT
冒烟；这些冒烟不等于异步分配故障或取消竞争的完整验收。

可变捕获槽 CELL_NEW 的 abort 和初始值移交仍未修复。后续需显式区分分配空间
与成功后安装初始值，并验证参数、普通声明、模式/循环绑定的失败清理和存储
激活；不能把本节闭包创建结果合同当成捕获槽分配已完成的证明。

## 2026-09-09：捕获槽分配与成功安装（入口专项通过，循环绑定未闭合）

CELL_NEW 现在只分配具有精确元素类型的空槽，产生独立的拥有型 cell SSA 值，
不消费初始元素，也不激活 targetSymbol 存储。结果采用 MAY_RAISE / NULL_RESULT，
紧接 ERROR_TEST。新增 CELL_INIT 消费两个独立拥有值：成功分配的 cell 和初始
元素；此指令不分配、不调用析构、不产生结果，只在成功分支安装并激活存储。
这样分配失败时不存在需要释放的 cell，初始元素仍由求值事务负责回滚。

共享 ConstructCell lowering 保护托管初值，发射受检查分配，成功后提交初值
所有权，再发射 CELL_INIT。普通局部、参数提升及表达式模式绑定共用这套路径。
捕获分析引入的声明/参数构造效果在退出计划与调用图效果闭包之前固定。参数
元数据只在成功安装后登记；验证器独立检查入口成功链、必要的提升记录，以及
每个失败出口实际已初始化的参数槽前缀。

验证器还要求空槽结果只能被其 ERROR_TEST 与唯一 CELL_INIT 使用，安装块只有
该成功边能进入，安装前只允许无失败的所有权移交。每个局部 cell 的全部 CFG
路径均接受存储激活/关闭检查。Cgen 共用一个 cell emitter；CELL_NEW 写 SSA，
CELL_INIT 写局部/帧槽并设置初始化位，构造器不再在分配失败时 abort。

声明在自身初始化过程中尚未激活，部分结果属于求值事务而非词法清理。删除
旧的“扫描全部捕获槽补发释放”分支；包括标量提升在内的词法清理由明确的
退出计划生命周期锚点负责。模式绑定仍须证明取得的是该绑定的精确投影，
不能用任意同类型值替换受保护初值。

8BF716A0 的永久入口专项验证了 14 次规范分配、7 个参数槽、224 项畸形 IR
拒绝，13 上下文中的 1,800 次真实分配失败和 9,700 次平衡分配。另有三个跨
await/yield 的槽构造和三项畸形 IR 拒绝，严格 C 与 C-host JIT/AOT 冒烟通过。
挂起冒烟不代表异步故障注入、跨线程引用计数或取消竞争已经验收。

扩大的合法循环捕获用例暴露：for item 绑定仍登记为普通 LOCAL，闭包却要求
RETAIN_CELL。该回归保留在 test/cases/xlir_cell_loop_allocation_outcome.xl，
当前失败而非预期拒绝，不能计入已完成。本专项没有把数组、区间、字符串、
协议或 generator 的循环绑定拆成 Cgen 特例；后续须将迭代绑定的物理 owner、
逐次安装、部分初始化失败和循环退出纳入同一可验证协议。

## 2026-09-09：循环绑定的物理 owner 与初始化前缀

BD52332A 将循环目标接入共享 InitializeIterationTarget / FinishIteration，
而非在 Cgen 按循环种类寻找捕获信息。目标仍具有元素 storageType；需要可变
捕获时，storage.cellType 与 LoopOwner.type 明确表示实际拥有的 cell。每次
进入循环体都执行 CELL_NEW / ERROR_TEST / CELL_INIT，离开该次迭代释放其引用，
逃逸闭包继续持有该次 cell。区间的内部 position/ordinal 与用户绑定分离，
修改用户绑定不改变迭代器的推进。

ExitStep.loopTargetCount 是源顺序中已安装的绑定前缀：第 N 个绑定构造失败，
只关闭其之前的绑定；循环体内退出则关闭全部绑定。Typed IR 父子关系确定
该前缀，验证器独立重算；lowering 的活动资源实例化它，不扫描函数存储补释放。
物理 owner 记录必须保持目标列表顺序，实际释放逆序进行。CELL_RELEASE 与
普通 RELEASE 都必须带精确 LoopOwner 身份并通过单次关闭 CFG 检查。

所有循环先安装 key/ordinal，再安装 item。协议 NEXT 的返回 payload 在条件
分支前属于临时求值 owner，结束边直接清理；成功边保护它直到绑定安装，不能
提前激活用户变量。generator 在 ordinal 安装成功前仍拥有已 yield 的 item。

真实分配失败测试同时补齐了三个基础操作的缺口：UTF-8 字符投影、容器迭代器
字符串 key/item 复制，以及静态容器游标创建。它们现在具有显式 NULL_RESULT
失败 ABI 和立即检查；叶层 helper 返回失败，不因分配失败 abort。临时 iterable
的释放走结果保护的求值清理事务。错误对象构造失败使用 XRT 静态 MemoryError，
MemoryError 到 XRT 的类别映射也保持一致。

永久门 check_xlir_cell_loop_binding.ps1 覆盖 range、array、string、list、dict、
set、protocol、generator 八类遍历，包含单目标字典、跨迭代逃逸闭包、内部游标
独立性和 continue/throw。17 个物理 cell、93 个前缀、11 个受检查获取操作、
427 项 C/header 输出前拒绝，11 上下文的 5,480 次真实分配失败与 94,060 次
平衡分配通过；严格 C、C-host JIT/AOT 和 C/header/JSON/binding 确定性通过。

这还不是完整循环/并发捕获验收：循环体自身跨 await/yield、复杂 managed cell
析构失败、嵌套退出及跨线程捕获仍需进一步组合验证；遍历 generator 不等于
循环体挂起。其他语言生命周期与发布准入工作继续保留，标准库只维持最低基线。

随后加入 xlir_cell_loop_suspension.xl：循环体内 await 后使用捕获槽，循环体内
两次 yield，以及遍历者提前关闭 generator。永久循环门同时验证两个 cell
构造与两项拒绝变异，并通过严格 C、JIT/AOT 与确定性冒烟。这仅补充跨挂起
正常路径和一次提前关闭，不是异步分配失败、复杂嵌套退出或取消竞争的完整证明。

### 循环捕获跨挂起的真实故障门（2026-09-09）

check_xlir_cell_continuation_outcome.ps1 使用真实源程序完成 parse/sem/XLIR
lowering，不通过合成 owner 图替代源语言证明。字符串循环的 ordinal 和 item
均提升为 cell；generator 每轮两次 yield，async 在两个独立输入 Future 之间
挂起，并在循环退出后再次调用逃逸闭包，验证逐次绑定身份与受管内容仍然有效。

故障注入的边界是 generator 的每次 next，以及 async 在 await 成功后调用的
源级 extern 标记。async 不在 PromiseResolve 之前开启分配故障，以免把生产者
自己的分配失败误算为接收方恢复失败。四种 ready/pending 组合分别测量实际
分配次数，逐一注入；恢复失败必须进入输出 Future，不能污染生产者错误作用域。
输入失败、取消、关闭另行验证，不等同于跨线程取消请求的竞争证明。

受管析构测试使用 array<var> 的循环 cell：原数组只保存 null，对象在循环内
写入 cell，因而不能由容器的额外引用掩盖 cell 退出清理。析构有正常、带诊断
失败和无诊断失败三种行为；正常路径上的析构失败阻止成功发布，已有输入失败
或取消/关闭则优先保留。每个对象只能析构一次，并须在输出发布前完成清理。

前端仍明确拒绝 generator<类>、generator<var> 以及直接可变捕获类变量。本门
不声称扩展这些语言能力；受支持的 var cell 持有对象与名义类 cell 是不同边界。
同样，跨线程 cell 引用计数/取消竞争、复杂嵌套退出和其他 frame 载荷仍待验收。

### 共享捕获绑定与跨线程引用生命周期（2026-09-09）

捕获的物理表示属于词法绑定，而不是某个闭包自己的读写模式。闭包注册先收集
全部捕获；任一闭包写入某个 sourceSymbol 后，其全部捕获（包括先创建的只读
兄弟、只读后代和参数转发）统一为同一个 cellType / RETAIN_CELL。该步骤发生
在错误效果闭包、存储注册和 CFG lowering 之前。没有可变捕获的绑定仍使用
原有按值/独立引用捕获，本次不把所有捕获无条件转换为 cell。

验证器独立核对同一 sourceSymbol 的捕获身份。把其中一项降回普通值/引用捕获
会在 C/header 输出前被拒绝，不能依赖 Cgen 从外层环境猜测是读取 cell 还是
保留 cell。原先的真实嵌套用例在只读子闭包创建时对已提升存储发出 LOAD，
导致合法程序被拒绝；统一绑定表示修复了该问题，并防止只读兄弟取到陈旧快照。

cell 的引用保留/释放统一使用 XRT 公共 xrtRefRetain / xrtRefRelease 原子合同。
最后一个持有者关闭 payload 并释放 cell；无效引用计数属于内部所有权协议
损坏，不能伪装成一次成功保留。volatile 只保持 XRT 引用计数 ABI，原子性来自
XRT 操作本身，而非 volatile。payload 的 LOAD/STORE 不因此变成原子或受锁保护。

真实并发门分开验证原始引用操作和源语言闭包：8 个线程累计保留，全部 join
后才读计数；再并发释放和保留/释放交错。源程序通过闭包工厂并发生成独立
reader 环境，全部共享同一个 var cell，且不并发写 payload。工厂关闭后，
最后一组 reader 竞争释放；对象只能析构一次。旧生成代码的 200,001 次预期
持有数实际只剩 34,966，新代码计数准确，并通过严格 C、C-host JIT/AOT。

该证据证明有效持有者之间的引用生命周期，不允许从无所有者的裸指针复活
对象，也不证明捕获值的并发读写、Future 取消竞争、代码卸载或外部 XRT 的
全部并发行为安全。这些仍属于完整语言生命周期验收的剩余范围。

### 2026-09-10：捕获槽取消/恢复及重叠 activation 验证

check_xlir_cell_cancel_race.ps1 用真实源程序持有一个拥有对象的 var cell，
经过 try 的 await 和 finally 的 await。两个源级 await 在正常/异常 finally
路径形成三个 Lowered suspension state；验证器必须保持 cell 在三个移交
动作中的责任，不能把原始 await 数量直接当作状态机状态数。

协作取消的可观察边界如下。本门不改变 AWAIT_ONCE 的投递语义：

| 请求发生位置 | 语言观察结果 |
| --- | --- |
| 第一个输入仍 pending | 在该 await 恢复点投递，可由对应 catch 处理或重抛 |
| 第一个恢复之后、finally 的 await 之前 | 在 cleanup await 投递，不由已经离开的 try catch 处理 |
| 最后一个 await 已恢复 | 无后续取消检查点，不强制改写正常返回 |
| 自动析构正在执行 | 不提前发布；保留既有主结果/异常，析构失败按正常清理规则处理 |

确定性时序用信号量暂停源级标记/析构；自由竞争让取消线程与一个或两个输入
生产者同时运行。另一个严格重叠场景调用真实 xrtFutureWatchAdd，获得真实
PENDING 后才暂停注册调用者：第二生产者在另一线程执行新 activation，完成
清理和输出发布，旧 activation 的注册调用栈随后才返回。测试不伪造 Watch
状态，不读取不透明 Watch 内部，也不在生产 Cgen 增加调度钩子。

每个对象恰好析构一次，输出必须在清理之后发布，观察器及 Watch 释放回调
各执行一次。两个输入仍正常完成，不因输出取消受到强制取消。每个生产者的
外层错误作用域分别覆盖成功、原有诊断失败、原有无诊断失败；原有错误必须
按对象身份保留。原生 XRT 观察器可以看到恢复后的生产者原始诊断，但不能
看到语言 continuation 的取消或析构错误。原诊断的独立引用归各生产者持有，
根引用活到全部生产者和观察器结束，测试自身不使用跨线程可变错误指针。

本门覆盖所列 cell/两次 await/catch/finally 的取消竞争及 activation 重叠，
不证明 payload 的并发写入安全，也不替代无 await worker、嵌套 Future、复杂
borrow/move、native/callable 或代码卸载边界的独立验收。
## 2026-09-10：无挂起 worker 的统一完成屏障

`XL6_IrCallTarget.asyncCompletionAbi` 现在明确约束所有内部 async 目标，而不只
约束需要状态机的函数。非 async 目标必须为 INVALID；内部 async 必须是
RETIRE_BEFORE_PUBLISH。机器分析从目标继承该合同，验证器独立检查目标角色及
机器一致性。文本/JSON dump 同时公开该字段，Cgen 不按函数形状推断完成规则。

无 `await` 的任务池 worker 在返回 executor 前执行捕获向量的逆序 retirement，
而不是把可失败的语言清理交给 executor 的 void 数据释放回调。传入的 consuming
参数在调用前从 frame 分离；借用参数的独立 capture owner 留给 retirement。
每个释放先清空物理槽，使后续物理 frame-free 幂等。结果 box 在调用 worker 前
分配；失败时未交付的输入仍由 frame 释放，不进入源函数。未发布的受管结果在
清理失败或取消时回滚，清理保持第一个 `(ok,error)`，不能以 error==NULL 当作成功。
只有到 XRT 拒绝结果的边界，才把无诊断失败表示为静态 InternalError。

任务池的既有取消点保持不变：XRT 可在调用前跳过被取消的任务；已经执行的
worker 在其函数完成后采样取消。worker 本身的失败优先；成功 worker 观察到的
取消优先于随后捕获清理的错误。在该采样点之后、借用捕获的析构过程中提交的
取消，不追溯修改已选择的正常/失败结果。consuming 参数的析构属于函数自身的
完成，因此在采样之前；这与 frame 中借用捕获的清理有明确区别。

真实反例显示旧代码已析构两个捕获对象并报错，却仍发布成功 Future。新永久门
`check_xlir_async_worker_completion.ps1` 覆盖六个无挂起 worker、五种结果 ABI、
借用/消耗参数、析构错误、无诊断失败、结果回滚、取消点和实际 worker 分配失败；
没有修改 XRT，也没有加入源名称或 AST 回溯。提前丢弃公开 Future 的回归不保留
宿主或观察器引用，另验证 transport 完成参数和结果的最终回收；内存统计在
模块关闭并 join 工作线程后进行，不以开放池的瞬时空闲代替生命周期结束。
嵌套 Future 的前端接纳仍属未完成项。

### 2026-09-10：动态迭代投影分配与 generator 关闭缺口

开放动态循环的键并不是永远可借用的值：array/list/set 的逻辑键需要装箱，
dict 的字符串键需要构造拥有型 xvalue。单目标动态循环在 dict 上同样产生键，
而非 dict 上仅保留 item。`XVALUE_ITERATOR_KEY_DYNAMIC_XVALUE` 与
`XVALUE_ITERATOR_DYNAMIC_LOGICAL_XVALUE` 因而统一使用 NULL_RESULT / MAY_RAISE；
非分配分支不伪造分配，也不把合法语言 null 与分配失败的空指针混为一谈。

Typed XLIR 的目标 constructionErrorAbi 同步表达此效果。只增加 runtime op 的
错误物理 ABI 是不充分的：目标初始化前的异常必须已有精确的退出计划，包含
已初始化的目标前缀和仍活动的游标。Lowering 通过既有分配错误边发射紧邻的
ERROR_TEST，Cgen 共用同一个 helper，只返回实际结果。没有添加同步、async、
generator 三套名称特判。快照 advance 的无效内部状态检查不作为普通 OOM 处理。

新增真实分配门证明上述投影/循环 cell 的失败能够保留 MemoryError，即使游标
最后释放的源级对象析构同时产生带诊断或无诊断失败。async 循环挂起时，游标由
现有共享 continuation frame ABI 保存；独立验证器检查其 destroy/transfer/initBit。

该门也揭示了尚未实现的语言边界：generator 的物理 frame-free 仅执行 owner
释放，不执行挂起点对应的 finally。真实源程序 `for value in generator ... break`
已复现对象及游标释放、finally 却执行零次，不能把“没有泄漏”当作关闭语义完成。

下一阶段的关闭实现必须在 IR 中建模，而不是在 Cgen 查找 finally：

- 每个 YIELD 的继续消费与关闭入口必须有明确的控制流和退出理由；关闭不是一次
  普通 NEXT，也不是可被用户 catch 任意吞掉的伪造分配/取消异常。
- 关闭入口复用统一的 scope、循环目标、游标、catch payload、finally、参数清理
  路由，并使只在关闭路径使用的值参与活跃性和 frame 驻留分析。验证器应从真实
  CFG/退出合同重算，拒绝删除或改接关闭边、清理步骤和必需的 frame 槽。
- 未开始、正常耗尽、暂停在 try/catch/finally、已有待完成的 return/throw、清理
  失败及多次关闭分别验收。发布和物理回收不能抢在可执行的语言清理之前。
- 现有前端允许 finally 内 yield，源探针已成功生成。不能通过禁止这类既有合法
  程序规避关闭，也不能将此类产出及挂起中的 pending outcome 无声丢弃；其关闭
  协议必须随完整 continuation 合同明确并验证。

本节最后四项是待实现约束，不是已完成声明。新增质量门保持关闭断言并返回失败，
完整语言验收与发布准入仍未通过。

### 2026-09-10：显式 generator close 路线首次接通

上述失败不再通过物理 frame-free 处理。Typed XLIR 为普通 yield 和包含 yield 的
已完成 try 边界生成 GENERATOR_CLOSE / RETURN 退出计划；处于 finally 内的 yield
使用 YIELD_DRAIN_FINALLY，不在清理体中途重新安排一次退出。父级 finally 中的
嵌套 try 同样保持 drain，避免跳过外层清理体的后半段。

Lowered XLIR 的 GENERATOR_CLOSE_TEST 是读取 activation.closing 的零操作数 bool，
紧邻 BRANCH，真边进入既有 scope/loop/catch/finally/parameter 退出路线，假边
继续普通执行。关闭路径因此进入同一套 CFG 活跃性与 frame owner/view 分析。
Cgen 只发射字段读取及已验证机器指令，不寻找源码 finally、不模拟下一次普通
消费，也不向用户 catch 注入伪造异常。

GeneratorMachine.closeAbi = UNWIND_DRAIN_FINALLY：尚未开始的 activation 不进入
函数体；已开始的 activation 恢复到显式关闭边或执行中的 finally。关闭期间
finally 产生的拥有型 item 由关闭协议接收并释放，不增加消费者循环次数。
每次恢复和 item/frame 回收有独立错误边界，关闭事务保留首个失败，包含
false/NULL；已有 pending return/throw 仍由 IR 中原有的完成路线传播。

独立验证器核对节点、计划、yield、machine、suspension 的关闭 ABI，以及紧邻
条件分支。真边到首个必需 finally/terminal 的前缀只能执行退出清理，不能执行
普通 post-yield 源语句；反接分支并同步重建全部机器缓存仍须拒绝。JSON 暴露
关闭点、计划、机器与挂起点 ABI，文本指令转储也包含关闭理由。

首次永久门通过 19 个源级关闭路线、117 项 C/header 输出前变异检查及严格 C、
JIT/AOT 和确定性检查；原动态迭代门的 160 个关闭场景也已通过。这里证明的是
这批实际覆盖的行为，而非所有关闭/所有权语义完成。进一步需要覆盖清理体自身
失败、消费者原有失败、未开始/重复逻辑关闭及更多嵌套捕获与循环组合；全局语言
验收、同源码发布准入和 legacy 退役仍是长期目标的必要条件。

### 2026-09-10：关闭模式的完整 CFG 与清理边界

关闭验证不能只核对仍存在的 CLOSE_TEST 与机器缓存。新增验证从每个真实 YIELD
的恢复位置解释 CFG，令 closing 在后续清理挂起中始终为真：关闭分支只走真边，
普通分支与错误结果分支检查两条边；RETURN/THROW 终止路径。源级计算必须属于
当前挂起 activation 的处理器或 finally 区域，其余指令须有独立的清理/所有权
合同。验证不依赖缓存中的关闭检查清单，因此一致删除检查并重建机器仍不能
让清理体越界执行普通代码。清理体内合法循环不被禁止，也不要求静态证明终止。

Typed 关闭边界包括三类需要非异常 RETURN 路线的点：普通 yield 恢复、try 的
清理/处理器正常合流，以及离开最外层活动清理区域的 break/continue。最后一类
使用 ABRUPT_UNWIND；目标循环仍在外层 finally 内时不生成关闭返回，必须继续
清理体内部控制流。共享 finally 的剩余退出计划按计划种类匹配，同一个源跳转
节点的普通 BREAK/CONTINUE 与关闭 RETURN 不再通过节点形态混淆。

AFTER_TRY 不仅用于 finally 内存在 yield 的情形：关闭清理的失败可以进入 catch，
而 catch 完全可以不再 yield 就正常结束。因此，有处理器且包含挂起点的 try
也需要关闭合流边界。没有处理器、finally 又不挂起的 protected yield 则沿自身
退出计划完成，不要求无实际用途的额外检查。嵌在外层 finally 内的 try 继续 drain。

Catch 参数绑定、类型过滤及多处理器共享分派属于处理器区域。Lowering 将它们的
origin 分别归属到绑定节点或 handler list，而不是笼统归属整个 try；ERROR_TAKE
等错误路线入口仍保留经验证的 try 身份。这是 IR 操作归属的修正，不放宽关闭
验证的普通 STORE/LOAD 白名单，也没有修改 Cgen 的错误类型选择或源码追溯规则。

该轮新增 finally 内外循环跳转、带类型/多 catch、处理器有无 yield、内外 finally
组合的源级运行回归，以及删除所有跳转检查副本、删除非挂起处理器合流检查并
重建缓存的拒绝测试。已通过的具体版本、数量与未完成项以迁移验收矩阵为准。
