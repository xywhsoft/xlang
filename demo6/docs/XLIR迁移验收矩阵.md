# XLIR 迁移验收矩阵

本文件是 `AST -> XLIR -> C` 迁移的当前可执行验收口径。历史快照记录各阶段的
接纳范围，不代表所有语言功能均已验收。未落入可验证 XLIR ABI 的语义必须在
lowering 阶段明确拒绝，不能被 XLIR Cgen 回溯为 AST/HIR 或暗中调用 legacy Cgen；
但明确拒绝只是迁移期间的安全边界，不能代替已承诺语言功能的实现与验收。

## Open 动态字段恢复的语言验收

2026-10-10：受支持 JSON/XLON Open 恢复图切换驻留事务。XLIR 保存并
独立验证计长静态键、存储 clone、错误物理 ABI 和隐式调用可达性；Cgen
只发数据与物理适配，不重建赋值语义。双端各 96 语言恢复、82 驻留和
12 原生阶段、22 项 IR 损坏拒绝及完整 Closed/Open 夹具故障前缀通过。
218 组件/1,179 导出目录通过，所选 Linux 原生闭包 sanitizer 通过，见
[语言恢复验收](stdlib53-b171-open-restore-language.md)。Set/List/Dict
等载体及发布总门未关闭；不是完整 AOT、发布 DLL/SO/VFS 或全仓 RC0。

## 当前任务范围与完成标准（2026-09-06 用户调整）

本节优先于后文历史快照中的工作顺序：标准库后续将重新设计，本次以最低可用
基线处理；语言本身及统一 IR 的正确性不降级。不再以完整迁移
`08_stdlib_extended.bat` 的全部 API 家族作为推进语言验收的前置条件。

### 标准库：最低可用基线，完整设计延后

- 优先保证语言回归所需的基础输入输出、字符串/容器支撑、必要文件与原生调用
  能够编译、链接和运行；沿用现有 XRT 能力和共享组件，不扩张 API 或重做标准库设计。
- 已进入基线的桥接仍须具有精确参数/结果 ABI、所有权、错误效果与清理路径。
  最低标准不允许错宽度、悬空借用、泄漏、重复释放、吞异常、静默错误结果或
  借助 legacy 回退凑出输出；必要的严格 C 和实际运行验证保留。
- encoding 的完整 11 入口迁移、非必要字符串辅助操作、专属网络/协议功能及
  扩展库完备性降为后续标准库工作。若某项成为语言测试的必要依赖，只补齐所需
  的明确合同及最小实现，不顺势扩展整组 API。
- 未实现、尚未验证和明确延期分别登记，不把已知链接失败计为通过，不将未支持
  的入口宣传为可用。已有测试和负例保留；不删除断言或放宽 ABI 以改变验收数字。

### 语言与 XLIR：本次必须完成的硬门槛

1. 类型和表达式：精确标量宽度、转换/溢出、nullable/union/enum、泛型具体化、
   类型缩窄、容器元素表示与身份，以及求值顺序、短路和副作用次数必须一致。
   字符串、容器虽有标准库外观，其语言级类型与生命周期不能按库功能延期。
2. 调用和对象：普通函数、默认/命名/可变参数、函数值与闭包、receiver、构造/继承、
   字段/属性、虚分派/协议，以及 extern/export 的物理 ABI 与调用点绑定必须明确。
3. 所有权和退出：borrow/copy/move、临时值、捕获、返回值、丢弃值及原生资源在
   正常路径、异常、break/continue/return、提前销毁等路径上均有可验证的清理计划。
   保留弱引用和精确析构断言，不靠进程退出掩盖泄漏。
4. 异常和 continuation：错误效果传播、typed catch、rethrow、finally、挂起/恢复、
   Future 完成/失败/取消与 generator 提前终止必须遵守同一语义；frame/capture 的
   存活性、状态转换和释放不能由 Cgen 针对样例拼补。通用调度、回调和线程问题
   即使通过标准库用例暴露，也不得移出核心验收。
5. IR 边界：Typed/Lowered XLIR 作为同一抽象层的明确阶段，保存充分的 TypeId、
   操作、物理表示、所有权、效果、控制流和清理信息。验证器拒绝畸形 IR，Cgen
   只机械发射已验证事实，不追溯 AST/HIR、不按源名称猜测语义、不暗中走旧后端。
6. 执行和产物：CLI run/build、严格 C 宿主及编译器/DLL 嵌入边界都须验证实际行为。
   编译器源接线、声明头/符号依赖和生成产物自包含属于核心质量，不因扩展库降级
   而豁免。涉及通用生命周期/并发的长跑证据仍需补齐，专属协议覆盖可单列延期。

### 执行顺序与证据要求

- 下一阶段先跑语言前端、核心运行、类型、OOP/协议/enum、异常、闭包、async/
  generator、任务与必要的产物边界分组；不继续沿标准库整组逐个补齐非核心入口。
  沿用现有 `01`—`07` 相关套件和 `09_artifact_closure`，逐项区分语言合同与库依赖，
  不能仅按文件名或套件编号把混合用例整组标为通过/延期。
- 每个有效语言缺口都需要持久正例与反例、明确的 IR 合同及实际生成产物运行证据。
  修复应落在统一表示、lowering 或共享操作上，不增加同步/async/generator 的平行补丁。
  现有合法特性不能通过改成“预期拒绝”而结项；非法程序和畸形 ABI 的负例另行登记。
- legacy 差分用于发现偏差，不作为唯一正确性标准。旧实现错误的用例保留语言语义
  断言并明确列为 XLIR-only，不复制错误，也不把差分不可比当作无需运行验证。
- 在同一最终编译器快照上记录语义运行、ABI/所有权单元、IR 变异拒绝、严格 C、
  确定性及必要的嵌入运行结果。接纳率、Cgen 成功率、专项用例数不等于完成比例；
  受环境阻断的验证仍是未验证，不是通过。
- 最终分别报告“语言/IR 验收”“标准库最低基线”“延期库功能”。正式总回归保留
  真实失败/未执行记录；已知语言缺口或必要运行证明未闭合时，不得宣布本次完成。

## 质量门

- `tools/check_xlir_native_future_map.ps1`：从精确 runtimeBridge 身份导出原生
  Future 映射 ABI，25 项畸形合同须在 IR 验证、C/header 发射前拒绝；产物计划
  明确依赖常驻 callback/payload。GCC O0/O2 与 TCC 验证独占取消、同步重入、
  分配失败和竞争；真实生成 TCP/resolver/TLS 适配器完成 600 次 DLL 卸载，
  其中 50 次结果对象独立存活于 Future 之后。拥有型 TLS 上下文仍按未知原生
  图拒绝集合准入；受控 Promise 的适配器卸载不等于活动服务的完整模块卸载。
- `tools/check_xlir_network_lifetime.ps1`：显式磁盘工具链下，保留 5 个原网络
  程序并增加 TCP buffer command 正例，执行 30 项 JIT/AOT/EXE 检查和
  30 次真实保存 AOT main 的公开生命周期；逐轮检查非法释放等计数，最终
  线程退休后精确平衡。TCP/TLS 各 16 次真实停止 worker 提交拒绝还须保留
  CLOSED 错误并回收事件句柄。代码保持常驻，不将该门视为混合图或卸载证明。
- `tools/check_xlir_member_call_diagnostics.ps1`：11 个参数数量/类型/命名、
  继承、泛型、async、原生方法和非可调用字段反例必须在 sem 阶段有定位诊断，
  且尚未生成任何 IR 节点。合法的方法、函数字段、动态及 async 调用继续完成
  严格 C 编译与公开运行；不能把拒绝所有成员调用作为修复。
- `tools/check_xlir_dynamic_compare_contract.ps1`：验证动态比较的精确 borrowed
  xvalue/xvalue → bool 合同、畸形 IR 拒绝、同步/async/generator 共用发射；实际
  运行时间与 bytes 标签、64 位整数精度、NaN/null 及原有 `cgen_var_values` 断言。
  新增边界用例按语言语义登记为 XLIR-only，不把旧后端行为作为唯一正确性标准。
- `tools/check_xlir_core_expression_contract.ps1`：核心第四组的类型、索引写入、
  动态表达式、字符串与入口合同；配套批处理保留原有用例并逐一执行严格 C、
  run、build 和运行结果断言，不依赖 legacy C 局部变量/辅助函数拼写。
- `tools/check_xlir_contract.ps1`：检查 XLIR 所有权、lowering、验证器、API 和
  C emitter 的单向边界。
- `tools/check_xlir_cgen_determinism.ps1`：对所有 `xlir_*.xl` 及迁移矩阵明确接纳的
  既有 `cgen_*` 代表样例连续生成两次 C，要求字节一致并以 `-Werror` 严格编译。
- `tools/check_xlir_cgen_runtime_matrix.ps1`：使用适配各 ABI 的 host 编译并执行
  canonical XLIR C 产物，覆盖标量（包括 `f64 -> i64`）、受管错误、动态值以及
  Future/async 的可观察行为。
- `tools/check_xlir_migration_coverage.ps1`：把下表的语义簇、代表性用例、全量
  生成门和 canonical 运行时合同绑定；删除或脱接任何一项都会失败。
- `tools/check_xlir_xcall.ps1`：验证 `xcall` 不再进入 legacy `XL6_Function` 兼容层。
  closure→callable 与 dynamic call 分别是显式 runtime op；后者消费拥有型参数，二者
  均紧邻 `ERROR_TEST`。生成 C 按 verified TypeId 去重 bool/int/float adapter，只调用
  公开 XRT callable API；may-raise entry cause 投影、严格编译和运行也属于同一门禁。
- `tools/check_xlir_dynamic_key_index.ps1`：验证 erased collection/key 的索引读写不是
  Cgen 类型猜测。读取用两个 borrowed xvalue operand 并以 null singleton 表示缺项或
  不兼容组合；写入借用 collection/key、在全部路径消费 owned item，并以 bool + trap
  表达失败。运行时只读取 tag 分派 array/int-map/object，不以失败 getter 探测类型或污染
  error slot；同时覆盖 typed `dict<T>` 标量值的显式 boxing、临时 receiver/key 的精确拒绝、
  文本/JSON/C、`-Werror` 与运行时行为。
- `tools/check_xlir_collection_member_intrinsics.ps1`、
  `tools/check_xlir_collection_algebra.ps1` 与
  `tools/check_xlir_dynamic_collection_methods.ps1`：静态泛型容器在 Typed XLIR 固定
  `TypedCollectionOp + CollectionAbi`，开放 `var` receiver 则固定 12 种
  `DynamicCollectionOp`。lowering 只消费 enum、TypeId、参数 slice 和所有权 ABI；Cgen
  不读取成员拼写。静态 `array.take` 返回精确元素 TypeId，可能缺键的
  `list/dict.take` 返回 owned 动态 TypeId。门覆盖 bool/i64/u64/f64/string 的
  pop/take/resize、list/dict 动态 take、capacity/trim/swap/insert/indexOf/reverse/isDisjoint。
  静态 array/list/dict/set 的 `+ - | & ^`、对应复合赋值及 set 关系运算由 opcode +
  精确 operand/result TypeId 唯一映射为 13 种封闭 runtime op；Cgen 不再用通用动态算术
  猜测容器语义。门覆盖 22 个表达式/复合赋值实例、生成 C 确定性、严格 C、运行语义，
  以及动态操作全集。
- `tools/check_xlir_collection_type_rebind.ps1`：验证 dynamic/裸容器经 checked
  cast/rebind 成为精确 typed collection 后，运行时 TypeId 变化不是普通 one-shot bind。
  每个 checked 结果先由唯一 `ERROR_TEST` 判定，再由 `xvalue-type-rebind-owned` 在成功分支
  消费并写入目标 TypeId；稳定借用来源必须先 `xvalue-clone` 获得独立 COW wrapper，owned
  调用结果则直接转移。验证器重算 checked owner 与 identity TypeId，Cgen 只机械调用
  `xrtValueTypeIdRebind`。两个既有 fixture 同时覆盖确定性 Lowered JSON/C、`-Werror`、
  失败原子性和四类容器运行语义。
- `tools/check_xlir_nullable_container_projection.ps1`：验证 `array/list/set/dict` 的
  nullable 元素不会在 erased xvalue 边界丢失表示或所有权。门同时覆盖 tagged `int?`、
  pointer-sentinel `string?`、nullable closure 与 nullable protocol，直接/安全索引、borrowed
  iterator item 投影、显式 discard key 和独立 target owner。安全缺项先归一为 canonical
  xvalue null，再由精确 TypeId projector 重建 nullable；`XVALUE_RETAIN` 只能保持原 TypeId，
  静态擦除必须另发射 verifier 可见的 `VALUE`。生成 C 还以命名 XRT tag ABI 展开 `println`
  vararg array，门固定确定性 Typed/Lowered JSON/C、`-Werror`、退出期堆清理和 legacy 差分。
- `tools/check_xlir_index_write_abi.ps1`：验证所有普通 INDEX lvalue 先在 Typed XLIR 固定
  `IndexWriteAbi`，明确区分 array/list、object literal/string key 与 erased dynamic key。
  lowering 只按该 enum 选择 consuming RuntimeOp，并要求每个 bool 结果由唯一 trap 消费；
  list 必须使用 int-map set，不能再误选 array-set。门覆盖 bool/i64/u64/f64/string boxing、
  null-aware list 复用、泛型具体化后的 ABI 刷新、机械 C、`-Werror` 与运行语义；全宽 U64
  主 fixture 为 XLIR-only，legacy 差分使用双方都能精确表示的泛型 int/string fixture。
- `tools/check_xlir_typed_container_index_assign_objects_functions_abi.ps1`：把普通索引写入扩展到
  `Pair`、union、pure-native class 与 function 元素。Typed XLIR 固定四类 `IndexWriteAbi`，
  lowering 分别显式执行 retain/rebind、union box、native handle + TypeId bind 和 checked
  closure→callable box；每次函数装箱都有唯一 `ERROR_TEST`、THROW ExitPlan，写入 bool 结果
  由唯一 trap 消费。动态 key 也走相同 typed item 合同。读回端使用显式 non-null nominal retain、
  union unbox、native handle 或 closure projector，Cgen 不回溯容器/元素源类型。门同时固定
  Typed/Lowered JSON、C 确定性、`-Werror`、运行断言和 legacy 差分。
- `tools/check_xlir_dynamic_key_scalar_projection.ps1`：验证 typed collection 使用动态 xvalue key
  时，运行时分派与静态元素 ABI 仍是两个独立阶段。`xvalue-index-get-xvalue-key` 只产生
  canonical borrowed carrier，随后 `xvalue-project-scalar-copy + payloadType` 精确恢复
  bool/i64/u64/f64/time trivial scalar；getter 不匹配是已验证容器不变量破坏并直接 trap，
  不是隐式动态转换或新的 may-raise 语义。Cgen 只按 operation/result ABI 机械选择 XRT getter，
  专用门固定 IR 相邻关系、确定性 C、严格运行和 legacy 差分。
- `tools/check_xlir_typed_point_projection_abi.ps1`：验证 `array<point<T>>` 的写入与读取是
  对称的显式 erased boundary。写入用 `xvalue-raw-pointer-borrow` 产生非拥有 pointer-tag，
  读取先产生 canonical borrowed item，再用
  `xvalue-project-raw-pointer-borrow(payloadType)` 恢复精确 pointer-sentinel 静态类型。
  XRT pointer tag 不携带泛型名义身份，因此参数化 TypeId 保留为静态 XLIR 事实；Cgen 只按
  operation 调用 `xrtValueGetPointer` 并检查 tag，不回看 `point<T>` 或容器源语法。门覆盖
  Typed/Lowered JSON、确定性 C、严格运行与 legacy 差分。
- `tools/check_xlir_xvalue_scalar_bridge_abi.ps1`：验证开放 `var` 与精确标量/string 边界不再
  依赖 Cgen 回溯。managed string 返回先显式装箱为 owned xvalue；非 class `is` 被分解为
  `xvalue-type-id + TypeId const + EQ`；流缩窄后的 string 名称从动态物理槽经
  `xvalue-project-string-copy(payloadType)` 直接产生 owned 结果，避免重复 stringify/copy。
  `create(var)` 内 checked 转换使 CallTarget 成为 may-raise 时，新对象先进入 owner storage，
  失败边 release 后传播，成功边 load/move 转移。门同时实际触发构造失败并 catch，覆盖
  Typed/Lowered JSON、确定性严格 C、运行时清理和 legacy 差分。
- `tools/check_xlir_string_conversion_abi.ps1`：验证 `string(value)` 在 Typed XLIR 中固定为
  标量/xvalue/string copy、TypeId name、class derived 或 exact class protocol call，而不是由
  Cgen 从源类型和方法名重新分派。`ClassLayout.stringAbi/stringTarget` 固化派生或显式实现，
  `ClassField.stringDeriveIncluded` 固化 `@string(skip)` 分区；lowering 将 TypeId 与派生 class
  分别降为携带完整 ABI 的 runtime op，显式 `__toString__` 则成为普通 verified CALL。Cgen
  只按模块 TypePool 名称表和 exact ClassLayout 生成 owned string helper。专用 fixture 覆盖
  Typed/Lowered JSON、确定性 C、`-Werror`、运行语义及 legacy 差分。
- `tools/check_xlir_class_value_identity_abi.ps1`：验证 `derive hash` 与 `derive equals`
  共同进入一个 `ClassLayout` 身份合同。Typed XLIR 分别记录 `hashAbi/hashTarget`、
  `equalityAbi/equalityTarget`，字段以 `compareDeriveIncluded` 固定 `@compare(skip)` 分区；
  lowering 用 `xvalue-class-identity-bind(payloadType)` 为新对象安装完整策略，并用
  `class-derived-equal(payloadType)` 表达直接 `==/!=`。没有 hash/equality 协议的普通 class 会显式
  选择成对的 `runtime-reference` 对象身份；只声明单边或尚未具备 callback wrapper 的协议组合则在
  typed set lowering fail-closed。Cgen 只按 exact ClassLayout 生成引用或稳定 TypeValueId-domain
  结构 identity callback，不再字符串化字段或搜索 AST/协议名。专项门覆盖 Typed/Lowered JSON、
  确定性 C、`-Werror`、引用身份、`@compare(skip)`、set 去重/查找以及 legacy/XLIR 运行差分。
- `tools/check_xlir_native_class_object_identity_abi.ps1`：验证仅声明一个 `native point`
  字段、但没有 native extern receiver 的 native-backed class 仍使用普通 managed class ABI。
  Typed XLIR 以 `class-field-borrow + fieldDeclaration` 固定 `point -> class` 构造边界；lowering
  显式生成 class allocation、TypeId/reference identity bind、runtime tag、raw-pointer borrow box 与
  exact field set，Cgen 不按类名、`handle` 拼写或容器位置重新判断表示。专项门覆盖三个独立构造
  与一次重复引用插入，证明 typed set 保留对象身份而不是把相同底层指针错误合并，并执行
  Typed/Lowered JSON、确定性 C、`-Werror` 以及 legacy/XLIR 运行差分。
- `tools/check_xlir_managed_native_extern_receiver_abi.ps1`：验证不声明 owned factory/destroy
  lifecycle 的 native-backed class 在 extern receiver 调用处仍保持 managed xvalue 语义。
  `CallTarget` 同时记录 semantic class TypeId 与 `native-class-handle-borrow + exact
  fieldDeclaration` 物理 ABI；lowering 在 `CALL` 前显式生成 raw-pointer 字段投影，Cgen 只接收
  已验证的 `void*` operand，不搜索 class、成员或 `handle` 拼写。专项 fixture 还用两个包装同一
  raw pointer 的对象验证 reference identity 不会退化为句柄身份，并覆盖 Typed/Lowered JSON、
  确定性 C、`-Werror`、严格运行与 legacy 差分。
- `tools/check_xlir_typed_native_handle_index_abi.ps1`：历史文件名保留，但合同已经迁移为
  field-only native-backed managed class 的容器索引语义。array/list/dict 读取直接产生精确
  nominal class xvalue，并各自执行一次 retain；随后声明字段读取使用
  `xvalue-object-get-literal-raw-pointer`。门明确禁止恢复 `xvalue-project-native-handle-borrow`，
  并覆盖确定性 C、严格运行及 legacy 差分。
- `tools/check_xlir_typed_native_handle_iterator_abi.ps1`：历史文件名保留，但循环项固定为
  `xvalue-retain + owned-iteration-release`，不再是裸句柄 borrow。lowering 只生成一次
  `xvalue-iterator-item-xvalue-retain`，字段访问继续使用精确 raw-pointer member op；cursor 与
  per-iteration owner 的清理均由 IR 表达，门覆盖 Typed/Lowered JSON、确定性 C、严格运行和差分。
- `tools/check_xlir_owned_expression_discard.ps1`：验证未使用的 owned managed rvalue
  不会被当作 trivial `(void)` 丢弃。lowering 必须将其存入一等 `TEMPORARY` storage 后
  发射 `RELEASE`，verifier 证明 storage/type/owner，Cgen 只机械调用 TypeId 对应的释放
  ABI；trivial 观察仍使用 `DISCARD`。borrowed managed observation 与尚无 storage-release
  ABI 的 product/native handle 保持精确拒绝，并由负例、文本/JSON/C、`-Werror` 和运行门固定。
- `tools/check_xlir_xvalue_membership.ps1`：验证 typed array/list/set/dict 与 builtin dynamic
  的 `in` 在 lowering 中成为两个 borrowed xvalue operand 和 bool result；item/key 与临时
  container 均由 `TEMPORARY/RELEASE` 明确保活。C helper 只按 XRT tag 分派稳定迭代、set、
  object key 与 runtime string substring，不以类型名或失败 getter 探测语义；文本/JSON/C、
  legacy 差分、`-Werror` 与运行门共同固定合同。静态 managed-string 由独立的下述门覆盖；
  range 与 protocol `__contains__` 保持独立 ABI 边界。
- `tools/check_xlir_string_membership.ps1`：验证静态 managed-string 的 `in` 使用独立
  `string-contains`，operand 顺序固定为 borrowed haystack/needle，所有 owned rvalue 经
  `TEMPORARY/RELEASE` 保活。Cgen 只发射 `xrtStrContains/xrtStrView`；既不借用 xvalue tag
  分派，也不回溯标准库 bridge。专用 fixture 与既有 mixed-container fixture 均进入严格 C、
  运行和 legacy 差分门。
- `tools/check_xlir_string_iterator_abi.ps1`：验证 managed string 的 `for` 在 Typed XLIR
  固定为单 value 或 `ordinal/value` 两种 UTF-8 LoopAbi，稳定命名 source 复制为 owned
  `string-snapshot-release`，literal/调用等 owned rvalue 则直接转移。lowering 只用
  `string-copy/string-utf8-count/string-utf8-at`、普通 CFG 和 active LoopResource 表达码点
  投影与所有退出清理；Cgen 仅机械调用 `xrtUtf8Count/xrtUtf8Substr`，不实现字节扫描器。
  专用 fixture 覆盖中文、四字节 emoji、空串、snapshot rebind、continue/break/return/finally/
  throw，并与 legacy 严格运行差分；既有 `stdlib_string` 同时进入确定性 XLIR Cgen 与严格编译门。
- `tools/check_xlir_protocol_iterator_abi.ps1`：验证普通 class 与 pure-native class 的
  `__iter__()/__next__()` 在 Typed XLIR 固定为精确 receiver TypeId、两个 direct target、
  `(bool,T)` product 和 iterator resource ABI。trivial item 使用 `product-get`，managed string
  item 使用 `product-take`，并在耗尽、continue、break、return、throw 上显式释放 item/iterator；
  native iterator 则携带精确 extern destroy symbol。pure-native class 只解析名义 handle ABI，
  不伪造 `ClassLayout`。门同时固定 Buffer/File extern property 的 `EXTERN_C` 分类、确定性 C、
  `-Werror`、native 析构次数和 legacy/XLIR 运行差分。
- `tools/check_xlir_range_membership.ps1`：验证 `in range(...)` 只消费前端已验证的 RANGE
  descriptor，lowering 将默认 start/step 物化为四个 i64 operand 并产生 bool 操作。helper
  覆盖正/负/运行期零 step，并用 unsigned distance/magnitude 避免边界有符号溢出；专用与既有
  fixture 均进入文本/JSON/C、严格运行和 legacy 差分门。复杂 async range 用例已越过该
  语义边界，其剩余未解析 `task.spawn` 调用不由 range Cgen 补丁掩盖。
- `tools/check_xlir_range_for_abi.ps1`：验证 range-for 在 Typed XLIR 先固定单 value 或
  `index/value` target layout，三参数形式把动态零 step 的 `RangeError` effect 归给 loop，
  而不是无异常 descriptor。lowering 只消费该 ABI，start/end/step 各求值一次，continue 经
  increment 同时推进 value 与从 0 开始的 ordinal；零值形成 owned message、`ERROR_RAISE`
  和显式 exception transfer。专用与既有 fixture 均进入 Typed/Lowered JSON、机械 C、严格
  运行和 legacy 差分门。int64 极值 increment 的溢出策略仍是独立未完成合同，门内不声称覆盖。
- `tools/check_xlir_array_i64_for_abi.ps1`：验证精确 `array<int>` 的 storage-backed iteration
  在 Typed XLIR 固定为单 value 或零基 `index/value` LoopAbi，且迭代机械自身 no-raise。
  稳定命名输入固定 `xvalue-shallow-snapshot-release + iterator TypeId` 并 clone 一次；数组字面量和
  函数返回等 owned rvalue 固定 `xvalue-owned-iterator-release + TypeId` 并直接转移、不得 clone；
  lowering 每个循环恰好生成一次 `XVALUE_CLONE` owned 浅快照、count、i64 position、typed item
  projection 与唯一 canonical exit release。active LoopResource 栈按 loop/exception depth 为 return、
  outer throw/catch 和 finally continuation 生成路径专属 release，循环内 handler 与 continue 保持快照；
  重绑源数组、普通/可抛调用、直接及 finally return、local/outer catch、exceptional finally、continue、
  break、函数参数、literal/returned iterator 均由严格运行和结构门固定。动态 `var` 非容器 source 不进入
  array ABI，而由独立 checked iterator ABI 在运行时产生精确 `TypeError`；await/yield 若
  尚无外围 suspension/frame 资源承载协议，由对应 lowering 边界拒绝，而不再污染 iterator ABI。
  门同时验证机械 C 和 `-Werror`，不允许 Cgen 补资源边。
- `tools/check_xlir_xvalue_iterator_abi.ps1`：验证 xvalue collection 使用独立的
  `VALUE_ITERATOR<CollectionTypeId>` owned cursor，而不是把 runtime `void*` 泄露成类型系统事实。
  当前 `list<int>` 的 value 与 integer-key/value 两种循环由正交 `LoopKeyAbi/LoopItemAbi`
  固定投影；命名 source 与 owned rvalue 均只求值一次并由 XRT cursor 持有稳定 snapshot。
  lowering 只生成 create/advance/item/key、普通 CFG 和一等 LoopResource destroy，所有
  return/throw/finally 逃逸边均由 IR 显式释放。门检查 Typed/Lowered JSON、每条资源边、机械 C、
  `-Werror`、运行语义和 legacy 差分；后续容器必须复用该抽象，不得新增容器专用 Cgen 控制流。
- `tools/check_xlir_set_i64_iterator_abi.ps1`：验证 `set<int>` 原样复用同一 cursor TypeId、
  create/advance/item/destroy RuntimeOp、LoopResource 和 C helper，只扩展 Typed collection/projection
  准入。set 固定 value-only；双 target 样例必须保持 `LoopAbi=none` 并在 lowering 精确拒绝，不能
  臆造 key。稳定命名及 owned rvalue、return/throw/finally、continue/break、严格 C、运行和 legacy
  差分均进入专用门，从而证明增加容器没有增加 Cgen 控制流分支。
- `tools/check_xlir_xvalue_iterator_scalar_projection.ps1`：验证 item projection 与容器控制流
  正交。`list<uint64>` 以及 list/set 的 bool/f64 组合分别使用 exact u64/bool/f64 RuntimeOp，
  list key 仍独立为 integer-i64；验证器同时核对 source-bound cursor TypeId 与结果 C ABI。
  当前 XRT set 元素校验拒绝 `XVALUE_UINT`，所以 `set<uint64>` 有专门负例并保持 Typed
  `LoopAbi=none`，不允许 getter 宽松转换。专用门覆盖 JSON、机械 C、`-Werror`、运行与差分。
- `tools/check_xlir_xvalue_iterator_managed_projection.ps1`：验证 list/set 的 string item 使用
  `string-copy + owned-iteration-release`，而不是把 borrowed XRT text 冒充 owner。per-iteration
  target 在 fallthrough/continue/break/return/throw/finally 的正确边释放；return 会先建立独立
  owned copy，使 finally 仍可观察原 target。门同时核对 item-before-cursor teardown、机械 C、
  `-Werror`、运行语义与 legacy 差分。
- `tools/check_xlir_xvalue_array_iterator_projection.ps1`：除保留 `array<int>` 的专用
  storage-backed ABI 外，其余可精确投影的 typed array 复用同一 `VALUE_ITERATOR<ArrayTypeId>`。
  ordinal key 与 list integer key 分成独立 RuntimeOp；uint64/bool/float/string/class/var/nullable-class
  item 分别固定为 scalar、copy-owned 或 retain-owned 投影。门覆盖 source 重绑定下的 snapshot、
  return/continue/break/finally 清理，以及 await 位于循环体内时 cursor/string item 的 frame storage、
  init bit、逆序 destroy action 与 activation handoff。非空输入覆盖重复 suspend/resume，空输入覆盖
  首次 suspension 前由 launch `RETURN` 生成 ready Future；同时固定确定性 C、`-Werror` 和运行。
  该完整合同暂列 XLIR-only：legacy 的非 int typed-array for 路径会生成不一致容器表示并崩溃，
  因而不能作为可靠差分基线。
- `tools/check_xlir_dict_iterator_projection.ps1`：验证 dict 的单 target 是显式 key-only LoopAbi，
  双 target 是 key/item LoopAbi；string key 总是 copy-owned，item 支持 i64/u64/bool/f64/string。
  `dict<string,string>` 的 key/item 双 owner 以 item→key→cursor 逆序清理，覆盖所有非本地退出与
  finally；key-only 不生成虚构 item projection。Typed/Lowered JSON、机械 C 与严格运行固定完整
  合同；另有共享差分 fixture 覆盖 legacy 也能精确表示的 i64/bool/f64/string 组合。完整
  `dict<uint64>` 保持 XLIR-only，因为 legacy 会错误地通过 signed xvalue 装箱该元素。
- `tools/check_xlir_dynamic_var_iterator_abi.ps1`：验证真正的动态 `var` source 不再由 Cgen
  猜测容器形状。Typed XLIR 固定 may-raise loop effect、source-bound `VALUE_ITERATOR<var>`、
  单 target 的 `dynamic-logical-xvalue`（dict 为 key，其余为 item）及双 target 的
  `dynamic-xvalue key + retained item`；lowering 固定 checked-create 后紧邻 `ERROR_TEST`，
  对 array/list/set/dict 之外的 tag 抛出精确 `TypeError`。set 的动态 key 是稳定快照 ordinal，
  array/list/dict 分别装箱 index/integer/string key；临时 owned source 在成功/失败分支均释放，
  cursor 和迭代 target 在 continue/break/return/finally 上由一等资源逆序清理。专用门覆盖
  确定性 Typed/Lowered JSON/C、机械 helper 映射、`-Werror`、运行语义和 legacy 差分。
- `tools/check_xlir_class_field_member_abi.ps1`：验证 concrete xvalue class 字段在 Typed
  XLIR 中按精确 layout field 固定为 xvalue/bool/i64/u64/f64/string/raw-pointer/native-pointer/
  native-handle MemberAbi，lowering 只按 enum 选择 literal-key RuntimeOp；字段名只保留为已验证
  key payload。raw `point` 写入另用 `xvalue-raw-pointer-borrow`，不借 native-class bridge。
  Cgen 只调用固定 helper；文本/JSON/C、`-Werror`、运行生命周期和 legacy 差分共同固定合同。
- `tools/check_xlir_class_protocol_syntax.ps1`：验证 class 的运算符、`in`、`[]`、缺失
  member 与 callable syntax 全部复用 canonical `CallSite/CallTarget`。语义层固定具体
  magic method；`__field__` 的成员名成为 `implicit-string` CallArgument，lowering 负责
  owned string 的 materialize/borrow/release，rvalue receiver 同样由 temporary 保活。
  普通 `grid[index] = value` 则由 ASSIGN 节点绑定唯一 `__setItem__`，CallSite 依次保存
  全部 index 与 value，左侧 INDEX 不执行 `__item__`，也不回退为动态数组写。
  Cgen 只发射 direct/virtual call，专用门同时覆盖 typed/lowered IR、严格 C、运行和
  legacy 差分；泛型 `__field__` 还要求 concrete return、synthetic CallTarget declaration
  与 implicit payload 一同通过单态化，不允许按语法拼写重新搜索方法。
- `tools/check_xlir_class_virtual_dispatch_model.ps1`：先在
  `xlir_class_virtual_dispatch` 上验证 typed XLIR 的 virtual slot/base-derived
  implementation 表、`virtual-call` 指令、属性 getter/setter 与数组中的派生类值；再在
  `cgen_class_polymorphism` 上验证上行转换、`is/as` type-tag、`array<base>.append(derived)`
  与虚调度组合。两者均以 `-Werror` 编译并执行。
- `tools/check_xlir_protocol_dispatch_model.ps1`：验证普通、重载、泛型协议实例、
  getter/setter、async requirement 以及泛型 class 具体实例的 witness 表；后者同时固定具体 class
  layout、字段类型替换、构造/get/let 单态实例和 result-void ABI，并以 `-Werror`
  编译执行生成 C。async witness 的 slot 返回 public `future<T>`，dispatcher 只调用
  已验证 launcher，worker result ABI 不泄漏到协议调用边界。
- `tools/check_xlir_async_generic_planner.ps1`：固定 planner-free async machine；同一
  fixture 覆盖 parameter 与零参 extern Future 输入、direct scalar/string literal、
  pre-await alias、嵌套左右 operand 顺序和 mixed capture slice。门检查 3 个 worker 全部
  具有唯一 machine，旧 shape selector、continuation 类型/arena 与 JSON 属性均不存在，重复
  Cgen 逐字节一致，并以 `-Werror` 执行生成 C；同一 fixture 还进入 legacy/XLIR 运行差分。
- `tools/check_xlir_async_cfg_liveness.ps1`：固定 Lowered CFG 驱动的通用 async machine
  分析。带分支的 task-pool worker 连续执行两次 `AWAIT`；门检查 block successor、稳定
  suspension state、SSA/storage live set，以及第一个挂起点中仍活跃 Future 的
  `retain-future/destroy-future` 和标量 storage 的 `copy-value/none`。同一 verified machine
  现在由 Cgen 直接执行分支 resume program，重复生成逐字节一致，并以 `-Werror` 和加/减两条运行路径固定语义。
- `tools/check_xlir_async_machine_multi_await.ps1`：覆盖二段、三段、混合运算和三段 managed-string
  continuation；检查单一 frame layout activation handoff、逐 suspension live-set 移交、managed
  move/take 所有权、成功/失败/取消/关闭传播以及旧 chain renderer 为零。
- `tools/check_xlir_async_machine_admission.ps1`：完整枚举 48 个代表 fixture，要求 46 个 analyzed
  machine / 60 个 canonical suspension 全部一一生成 direct callback；完整已支持 inventory 另覆盖
  80 个 fixture / 45 个 analyzed machine / 54 个 canonical suspension，且不得出现 non-analyzed
  machine、orphaned suspension 或 legacy renderer。门同时覆盖 xvalue、closure/future capture、
  catch recovery 与 tagged nullable continuation，并固定 fail-closed admission、确定性 C、严格编译和运行语义。
- `tools/check_xlir_async_class_method.ps1`：验证普通及泛型 async instance method 的
  receiver 是 worker 参数 0，task frame 以 `RETAIN_XVALUE` 持有 receiver，并在 frame
  析构时对称释放；泛型 class 参数由 concrete receiver 决定，worker result 与 public
  `future<T>` 分别固化；同时验证 `time.sleep` 的 `c_uint` 参数经显式
  `scalar-convert` 进入精确 `unsigned int` 外部 ABI；返回 Future 的 may-raise 普通函数
  使用显式 `xlir_result_future` carrier，使局部 receiver 在任务完成前由 frame 保活；生成
  C 均须通过严格编译和运行。条件 future 在 lowering 中把 borrowed storage 分支经 nullable
  `retain` 与 owned async-call 分支合流为单一 owned temporary；`await` 借用后显式释放。
  未初始化 `future<T>` 是经验证 typed `NULL`，并产生语言级
  `AsyncError("await on null future")`。异步普通参数按已验证 frame capture ABI 分别复制
  标量/string、retain xvalue/future/closure；结构等价但 type-id 不同的函数值在调用参数
  边界先经显式 `VALUE` rebind。`string(bool/i64/u64/f64/xvalue)` 分别降低为独立、返回 owned
  managed string 的 runtime op，Cgen 不再从源类型或 builtin 名称推断转换。前端已解析为
  string 的 `string + var` 会先显式执行 `xvalue-to-string`，将 owned 结果锚定后再
  `string-concat`，随后释放临时值；同一命名 Future 的重复 await 保持借用语义。第一类
  函数类型自身携带每个参数的 `value/borrow` ABI；closure-call 与 direct-call 共用受管
  借用证明。lowering 生成的所有权槽使用一等 `TEMPORARY` storage ABI，与用户 `LOCAL`
  明确分离；验证器按实际 call 指令索引证明同基本块的 materialize/call/release 生命周期，
  文本和 JSON dump 均公开该分类。标量复合赋值展开为已验证的
  `LOAD/CELL_LOAD -> BINARY -> STORE/CELL_STORE`，浮点仅接纳加减乘除；
  不支持的成员、索引或受管复合赋值必须在 lowering 报错，不能退化为只存 RHS。
- `tools/check_xlir_copy_clone.ps1`：验证 typed class layout 明确选择派生
  `runtime-shallow/runtime-deep` 或用户 `internal-call` 复制协议；lowered XLIR 以
  `xvalue-clone/xvalue-deep-clone` 表达借用输入、拥有同类型结果，自定义
  `__copy__/__clone__` 则成为普通已验证内部调用。文本与 JSON dump 均公开协议选择，
  泛型字段的无显式初始化不由 Cgen 猜测：具体化后的 bool/i64/f64 通过 class-field
  `scalar-zero` ABI 生成显式常量与 boxing。泛型模板 cleanup action 显式记录 callable
  owner（模块初始化使用 module root），具体化时连同 origin/storage 一起重映射；只有 owner 已进入 lowered function arena
  的 action 才是必须落地的可执行 cleanup。普通/泛型派生类及自定义协议路径都必须经
  `-Werror` 编译并运行。
- `tools/check_xlir_export_boundary.ps1`：验证公共 export 不是 Cgen 对内部函数签名的
  临时投影。lowered XLIR 文本/JSON 必须同时公开参数的 `value/borrow` 与返回的
  `void/value/owned` ABI；C 与 header 只消费该表。class 构造把 owned xvalue 交给 C
  调用者，class 参数只在调用期间 borrow；名义类型重载以稳定 Type ABI ID 形成不同 C 名，
  并经 `-Werror` 宿主编译和运行。
- `tools/check_xlir_global_export_boundary.ps1`：验证公开数据符号使用独立
  `GlobalExport`，而非函数 export 或 Cgen 源码特判。文本/JSON dump 必须公开 storage、
  declaration/initializer、TypeId、mutable/const、C name 与 canonical static initializer；
  C/header 的定义、声明和内部 LOAD/STORE 都只消费该表。宿主在尚未调用函数 adapter 前
  直接读取初值，并验证 mutable 可由 C 与 XL 函数双向修改、const 不产生 lowered 写边。
- `tools/check_xlir_extern_global_boundary.ps1`：验证外部 C 数据依赖使用独立
  `ExternGlobal` 与 `EXTERN_GLOBAL` storage，不伪装成本模块 GLOBAL/GlobalExport。文本与
  JSON 固化 TypeId、read-write access 和 C name；生成 C 只发出 extern 声明，内部
  LOAD/STORE 共用链接名，IR-derived export header 不泄漏该实现依赖。宿主提供真实存储并
  验证顶层 XL 初始化对它的读取与修改。
- `tools/check_xlir_null_abi_boundary.ps1`：验证 `null` 只能在声明、赋值、参数、返回或
  比较等已知目标类型的边界形成 `NULL / NULL_TEST`。class 与 `nullable<class>` 共享
  xvalue sentinel 表示，raw pointer 使用 pointer sentinel；具体 class 写入 nullable 必须有
  显式 `VALUE` rebind。文本/JSON type table、严格 C 与宿主运行共同防止 Cgen 猜测空值
  表示，也禁止回退到 `CONST immediate=NULL`。`nullable<bool/int/uint64/float>` 使用独立
  trivial tagged struct C ABI；标量→nullable、present payload 提取分别为显式
  `NULLABLE_WRAP/UNWRAP`，`??` 由 `NULL_TEST/BRANCH/VALUE/JUMP` 组成 lazy CFG。`var` 与
  `nullable<class>` 的 xvalue sentinel coalesce 也使用同一 lazy CFG；present/fallback 两边显式
  `RETAIN/RELEASE` 左值 temporary，class present 分支通过同表示 `NULLABLE_UNWRAP` 投影，literal
  `null ?? value` 则在 lowering 中按结果 TypeId 直接选择 fallback。直接 storage 的 `?=` 对
  xvalue sentinel 生成 lazy test/branch/release/store CFG；静态非空 TypeId 消除 RHS，禁止旧 Cgen
  对 nullable class 的 eager fallback 行为进入新后端。内部
  参数/返回、未显式初始化、赋值、比较和 coalesce 均由同一 TypeId 契约覆盖；
  `errorResultCName` 固化 may-raise carrier，公开普通/may-raise export 均经宿主验证。
- `tools/check_xlir_callsite_binding.ps1`：验证普通函数与 class constructor 的 positional、
  named 和 default 参数都先形成 canonical `CallSite`。固定参数绑定按声明顺序排列，
  每项保留 explicit/default 来源、表达式和 parameter index；lowering 只能消费该表，不能
  回读原始调用 AST 重做匹配。文本/JSON dump、严格 C 与宿主结果共同固定这一边界。
- `tools/check_xlir_callsite_variadic.ps1`：验证 `array_vararg` / `dict_vararg` 的 tail item
  已在 CallSite 中显式分类；lowering 分别生成 typed array/object aggregate、boxing、写入、
  borrow call 和对称 release，包含空 tail。Cgen 只看到普通的固定物理参数调用。
- `tools/check_xlir_callsite_c_vararg.ps1`：验证 native C variadic target 从物理参数表移除
  `c_vararg` 伪参数，并以独立 `variadic=c` ABI 保留动态 tail。verifier 逐项限制可传递的
  native scalar、managed string、handle/pointer 类型；Cgen 只据 CallTarget 发射 `...`，
  不执行默认提升或源码类型猜测；严格宿主通过 `va_list` 验证实际调用。
- `tools/check_xlir_callsite_generic_specialization.ps1`：验证泛型单态化与节点/类型/符号
  一起克隆 CallSite 和 CallArgument slice，并把外部调用点 retarget 到唯一 concrete symbol。
  原模板与实例元数据保持隔离；文本/JSON、严格 C 和运行共同防止 lowering 回退原始实参树。
- `tools/check_xlir_class_protocol_syntax.ps1`：验证非 `CALL` 的可调用语法也使用同一
  CallSite arena。unary/binary/index 保存显式参数；member `__field__` 保存无 AST
  expression 的 `implicit-string` 参数及 canonical UTF-8 payload。verifier 证明该参数只
  出现在固定参数槽；index ASSIGN 保存 receiver/index/value 和 void result ABI。lowering
  依据目标参数 ABI 物化并释放，Cgen 不读取成员名或重新搜索 `__item__/__setItem__`。
- `tools/check_xlir_intrinsic_calls.ps1`：验证没有 callable declaration 的受检整数、string、
  foreign scalar、精确同 TypeId trivial identity、copy/clone、array append、range 与 xcall 均先形成 typed `IntrinsicCall`；连续
  operand slice 明确 `value/borrow/consume`，result/effect 明确
  `value/owned/descriptor` 与 `no-raise/may-raise`。identity 与 admission 分离：unsupported ABI
  仍是可 dump、可供 legacy 差分的合法 Typed XLIR，XLIR lowering 只消费 `verified` 并按 kind
  精确拒绝。`point(point)` 由独立 `TRIVIAL_IDENTITY_CONVERT` 固定为 exact TypeId/value/no-raise，
  lowering 发射显式 `VALUE`；它既不冒充 foreign scalar cast，也不落入未解析普通调用。门禁同时
  禁止 lowering 比较这些拼写、禁止 Cgen 读取 intrinsic arena，并以 range 与 point identity 的
  严格 C 编译、运行固定 descriptor/identity 到 CFG 的边界。
- `tools/check_xlir_member_abi.ps1`：验证非调用成员在 Typed XLIR 中先形成 canonical
  `MemberAbi`。当前 `string.length` 固定为 `string-utf8-count`，其 ABI 是 borrowed managed
  string 到 i64；lowering 只消费 enum 并生成 `STRING_UTF8_COUNT`，Cgen 只消费该 runtime op，
  不读取成员名。ASCII、空串和多字节 UTF-8 均通过严格 C、运行与 legacy 差分。
- `tools/check_xlir_length_protocol_member.ps1`：验证 class `value.length` 在真实同名字段不存在时
  固定为 `length-protocol-call`，Typed 节点携带唯一零参 `__len__` target symbol。普通 xvalue class、
  pure native-handle class 与标准库 `Buffer` 共用该身份；lowering 只生成 one-borrow direct CALL，
  Cgen 不搜索类或 magic method。专用 fixture 通过严格 C、运行与 legacy 差分，`stdlib_buffer`
  当前 Typed probe 也证明两个位置绑定到同一协议目标。
- `tools/check_xlir_native_nominal_receiver.ps1`：验证 class TypeId 显式携带 nominal symbol，
  pure native-handle class 即使没有 `self` 语法使用也能解析唯一 receiver；factory、extern method
  与内部 wrapper 都使用 parameter 0 的 `borrow` ABI。读取声明的 `native point` 字段由 Typed
  `NativeHandleProject` 固定 exact access/receiver/owner/result/field declaration，lowering 生成
  `NATIVE_HANDLE_PROJECT`，而不是把 pure-native receiver 当成 xvalue object。文本/JSON、精确 C
  prototype、析构计数和严格宿主运行共同固定生命周期；Cgen 不按类名或成员名补齐句柄。
- `tools/check_xlir_class_implicit_receiver.ps1`：验证函数体完全不引用 `self` 的 concrete class
  构造器、getter、setter 与普通实例方法仍各自拥有 canonical class TypeId 的 parameter 0 `borrow`
  ABI；CallTarget/Function 签名必须逐一对应，生成 C 保留四个显式 receiver 参数，并通过严格 C、
  运行及 legacy 差分。typed `self` 仅能提供同 TypeId 的 storage symbol，不能决定 receiver 是否存在。
- `tools/check_xlir_native_handle_bind.ps1`：验证 raw `point` 到 pure native-handle class 的构造
  在 typed XLIR 中形成独立 `NativeHandleBind`。无析构类是 `BORROW`，有析构类是 `TAKE`；后者
  必须在 lowered XLIR 中清空可寻址 source，rvalue owner 借用传参后由 temporary 持有，并通过
  null-test CFG 调用精确 destroy target。文本/JSON、严格 C 与 lvalue/rvalue 生命周期运行共同
  固定该契约；Cgen 只发射已验证的 `void*` 绑定。
- `tools/check_xlir_value_initializer.ps1`：验证声明初始化不依赖 C 局部/全局的隐式零值。
  typed XLIR 的 `ValueInitializer` 分别固定 explicit、scalar-zero、string-empty、null-sentinel、
  xvalue-null、携带精确 runtime op 的 xvalue-container-empty、class-default 与 class-null；无显式
  `create` 的 concrete class 才默认构造，有显式 `create` 的模块存储默认值则是独立的 nominal class-null，lowering 只消费该表
  生成常量、runtime op、NULL、默认 class construction 与 STORE。array/list/dict/set 空容器、基础默认值、无析构
  native handle 的复制/赋值，以及有析构 native handle 的 null-safe cleanup 均以严格 C 和运行固定；
  无显式 C linkage 名的 `export var/const` 是 package/module GLOBAL，同样拥有 explicit initializer 并在
  module-init STORE；只有 `export "c_name"` 数据进入静态 `GlobalExport`。两类 export 由严格运行分别固定，
  Cgen 不读取这张 typed 元数据表。
- `tools/check_xlir_explicit_constructor_field_initializer.ps1`：验证显式 `create` 仍先消费
  `ClassField.initializerAbi`。runtime tag、继承字段、本类 explicit/scalar-zero 字段和 create body
  形成固定顺序；严格 C 与 legacy/XLIR 运行共同验证声明值、未赋值零值和 body 覆盖值。
- `tools/check_xlir_class_reference_field_initializer.ps1`：验证 concrete/递归 class 字段在构造时先写入
  retained xvalue-null 槽，而不是递归默认构造；native class 字段由 `ClassField.nativeDestroySymbol`
  固定精确析构 target，赋值产生 `XVALUE_NATIVE_HANDLE_TAKE`。class storage 使用独立
  `XVALUE_CLASS_NEW`，保持声明顺序读取、最终按逆声明顺序释放 owned 字段；普通 dict/object 仍使用
  `XVALUE_OBJECT_NEW`。reference、nested/async/generic 与 native 两字段场景均通过严格 C 和运行。
- `tools/check_xlir_reachability.ps1`：验证 module-init/export/entry 根与 direct/extern、closure、
  virtual/protocol row、async worker/continuation 边在 Lowered XLIR 中形成精确的
  `root/transitive/unreachable` callable closure。Verifier 独立重算该闭包；Cgen 只发射可达
  prototype、body、closure、dispatcher 与 launcher。门禁特别证明未调用的标准库
  `string.count` wrapper 不再把 `xl6_xrt_string_count` 带入 class-default 产物，并以严格 C 和运行
  验证真正的 live extern/closure 路径。
- `tools/check_xlir_product_result.ps1`：验证多个语义返回值在 callable boundary 上统一为一个
  canonical tuple TypeId；函数类型通过 `callableResultType` 显式记录该物理结果，lowered XLIR
  由声明返回列表主动驻留 tuple，不依赖调用点；trivial 字段使用 `product-get`，托管字段只允许
  `product-take/product-drop`。自由函数、class method 与匿名 closure 共用同一
  named-struct C ABI，纯标量泛型实例在单态化时克隆 concrete product result，并通过 JSON、
  严格 C 和运行固定。managed string product 的覆盖赋值、逐字段移动与 `_` 丢弃已经严格运行并与
  legacy 差分；verifier 要求每个托管字段恰好消费一次。may-raise product carrier 和公开 export
  product 仍保持精确拒绝。
- `tools/check_xlir_named_function_adapter.ps1`：验证命名函数值不再被当作 declaration storage
  读取，也不由 Cgen 临时生成函数指针包装。lowering 按 resolved target + canonical signature
  去重无捕获 adapter，生成独立 `NAMED_ADAPTER` CFG；转发参数、默认表达式、direct call 和
  return 全部成为普通已验证指令。所有 first-class callable 统一使用 `bool success + optional out-result`
  thunk ABI；may-raise target 的 adapter 具有显式 CALL/ERROR_TEST/THROW/RETURN CFG。顶层函数值、
  函数参数、默认参数、多返回 Product、异常传播、C callback export、extern C target 及其
  `xcall` 包装/动态调用均须通过
  JSON、严格 C 和运行；暂不接纳 erased/variadic adapter。
- `tools/check_xlir_xcall.ps1`：验证 `xcall(function-value)` 先生成携带完整
  `calleeType` 的 `xvalue-callable-from-closure`，动态调用再生成借用 callable、消费其余
  owned xvalue 参数的 `xvalue-dynamic-call`。两种可失败操作都必须由下一条
  `ERROR_TEST` 分流到既有 ExitPlan；Cgen 机械发射 `xrtCallableCreate`、
  `xrtValueCallableTake`、`xrtValueInvoke` 和 `xrtCallResult*`，不允许出现
  `XL6_Function/xl6Xrt*`。当前封闭分区为任意个值传递 `bool/int/uint64/float` 参数和单一
  scalar 结果；每个 TypeId 只生成一个 adapter。零参 may-raise bool 同时验证 XRT
  entry wrapper 的 cause 被投影回原始 XLIR error kind/message。
- `tools/check_xlir_c_scalar_abi.ps1`：覆盖全部现有 `c_*` 标量类型。typed/lowered
  XLIR 必须保留各自精确 C 表示及 trivial ownership，所有兼容参数适配和显式 cast
  都必须成为 `scalar-convert`；生成的 extern prototype、显式 C cast、严格编译、宿主
  链接和运行结果共同构成 ABI 门。
- `tools/check_xlir_u64_abi.ps1`：语言 `uint64` 使用独立、无损的 U64 执行 ABI，不复用
  foreign `c_ulonglong` 或 signed i64。上下文字面量统一为目标 TypeId 的 `ULL`，算术、
  位运算、比较、全局/extern/export、`u64-to-string`、may-raise result carrier、async Future
  payload 以及 immutable/mutable closure capture/cell 均由 typed/lowered JSON、严格 C 和
  宿主运行共同固定。`XVALUE_UINT` 已提供无损 dynamic box/unbox/string、受检
  `i64/f64/xvalue -> uint64`、typed array/list/dict/class field 投影、runtime callable/xcall
  与 `task.all` payload ABI；直接动态 `+/-/*` 另有三个显式 runtime opcode，固定 borrowed
  xvalue 输入、owned xvalue 结果及 U64 modulo-2^64 promotion。动态等值、四种排序、近似比较
  和 truth predicate 也各有 verifier 可见的 runtime identity；静态 `i64/u64/f64/string ~=`
  固定独立算法 ABI。除法、取模等其余动态运算仍等待各自统一 IR 契约，不能由 Cgen 隐式补齐。
- `tools/check_xlir_integer_conversion_abi.ps1`：整数转换把 source ABI、target normalization
  domain 和 failure effect 固定为三个正交事实。`managed string/f64/xvalue -> integer` 使用显式
  checked op，并要求紧邻的 `ERROR_TEST` 消费同一结果；静态 `i64/u64 -> bounded integer` 与
  `u64 -> i64` 使用独立、无异常的 modulo wrapping op。每个整数 TypeId 保存自己的 `i8/u8/
  i16/u16/i32/u32/i64/u64` domain；声明初始化、普通赋值和有界 `+=/-=/*=` 都先显式归一化，
  Cgen 不再从源节点或 C 类型名称重建语义。专项门固定字符串解析错误、全宽 U64 wrapping、
  byte-for-byte 确定性、严格 C，以及 legacy/XLIR 运行差分。
- `tools/check_xlir_scalar_conversion_abi.ps1`：所有 trivial scalar cast 除精确 source/result
  TypeId 外，还携带 canonical `ScalarConvertAbi`：bool→bool、bool→integer、integer→bool、
  integer→integer、integer→float、float→integer、float→float。Typed scalar-convert intrinsic、
  generic specialization 与 Lowered `SCALAR_CONVERT` 共用唯一 classifier；verifier 禁止缺失或错配
  family，Cgen 只消费该字段和目标 TypeId 发射机械 C cast。七类基础转换均通过 JSON、确定性、严格 C
  与 legacy/XLIR 运行差分。
- `tools/check_xlir_time_conversion_abi.ps1`：语言 `time` 使用独立 `TIME_I64` ABI 与历史
  TypeId，不因物理表示同为 `int64_t` 而退化为普通整数。time→bool/integer/float 与 integer→time
  是四类独立 `ScalarConvertAbi`；extern C 的 time 参数与返回值也由前端、Typed/Lowered verifier
  和生成 prototype 共同固定。专项门覆盖 JSON、确定性、严格 C 与 legacy/XLIR 运行差分。
- `tools/check_xlir_type_value_abi.ps1`：语言 `type` 使用独立 `TYPE_ID` ABI，不因 C 物理表示为
  `uint64_t` 而复用 U64 语义。类型池保存历史结构哈希，JSON 以精确十六进制字符串导出；裸类型名
  通过 `typeValueTarget` 指向 canonical type，静态 `type(expr)` 通过 verified `TYPE_OF /
  STATIC_ID / typeQueryTarget` IntrinsicCall 表达，并保证 operand 恰好求值一次。门禁覆盖 TypeId
  export/extern/global、函数参数/返回、may-raise result carrier、`future<type>`、async copy、
  immutable/mutable closure capture/cell、生成 header、严格 C 与宿主运行；同时复跑 `type_values`、
  `type_aliases`、泛型 class 类型值的历史哈希。动态 `var` 查询在 runtime identity ABI 完成前必须以
  精确诊断 fail-closed，class field/typed container TypeId 也不得偷用 U64 opcode。

索引空感知赋值的新增合同：稳定 xvalue array/list receiver 与 i64 key 各求值一次，`*_AT_I64` 返回 borrowed current；null 分支以 `XVALUE_RETAIN` 和 compiler temporary 锚定 receiver，只接纳 no-raise RHS，替换值由 consuming `*_SET_I64_NEW` 接收并紧随 `TRAP_IF_FALSE`，最后显式释放 anchor。borrowed current 仅供 null-test，不存储也不释放。临时 receiver 与 may-raise RHS 在专用 cleanup edge ABI 完整前仍精确拒绝。该结构由 null ABI 门的文本/JSON/C/严格运行断言固定，并进入 legacy/XLIR 差分。

## 当前已接纳的 XLIR 语义簇

| 语义簇 | 代表性证据 |
| --- | --- |
| 标量与 CFG | 短路、while/do-while；整数转换在 IR 中分别保存 source ABI、目标 `i8/u8/i16/u16/i32/u32/i64/u64` normalization domain 与 failure effect：`managed string/f64/xvalue -> integer` 是紧邻 `ERROR_TEST` 的 checked op，静态 `i64/u64 -> bounded integer` 与 `u64 -> i64` 是独立 modulo wrapping op；声明初始化、赋值及有界 `+=/-=/*=` 显式归一化存储。数值运算先按语义结果 TypeId 形成显式 execution normalization，`i64/u64 -> f64`、foreign integer literal 与 `c_bool -> bool` 均产生独立 `SCALAR_CONVERT`，BINARY/UNARY verifier 仍只接受同型 operand/result；受支持的不同定宽整数可经已校验的 sign-extended i64 执行 ABI 比较；`uint64` 使用独立无损 U64 执行 ABI，覆盖上下文 `ULL` 字面量、算术/位运算/比较、全局/extern/export、owned string 转换、may-raise carrier、async Future、closure capture/cell、`XVALUE_UINT` 动态 box/unbox/string、typed array/list/dict/class field 投影、runtime callable/xcall 与 `task.all` payload；静态 string 等值/排序、`i64/u64/f64/string ~=`、动态等值/排序/近似比较和短路 truth conversion 均使用独立 runtime op，混合静态操作数在 lowering 显式装箱并由 temporary/release 固定 lifetime；直接动态 `+/-/*` 使用显式 xvalue runtime opcode 并验证 U64 modulo-2^64，除法/取模等其余动态运算仍未迁移；range 正负 step、match、conditional |
| canonical TypeId 值 ABI | `type` 拥有独立于 U64 的 `TYPE_ID`/`xlir_result_type_id`/`copy-type-id`/`cell_*_type_id` ABI；类型池以历史结构哈希保存每个 canonical type identity，裸类型名保存 `typeValueTarget`，静态 `type(expr)` 保存 verified `STATIC_ID + typeQueryTarget` 并精确求值一次。export/extern/global、参数/返回、异常 carrier、Future、async 与 closure/cell 已由文本/JSON、生成 header、严格 C、宿主运行和历史哈希回归固定；动态 xvalue identity 已通过 runtime tag map 与显式 TypeId bind 接纳并由 `check_xlir_type_value_abi.ps1` 验证；class field 与 typed container element 的支持边界仍按各自显式 ABI 判断 |
| typed IntrinsicCall ABI | `checked integer/string/foreign scalar/trivial same-TypeId identity/copy/clone/array append/range/xcall` 各有唯一 enum identity、显式 `verified/unsupported-abi` admission、typed operand slice、receiver、result 与 effect ABI；`point(point)` 精确降为同 TypeId `VALUE`；泛型单态化同步克隆、替换该 slice 并重算 admission；lowering 无名称比较且只消费 verified 记录，Cgen 只看到已经验证的指令 |
| runtime resource storage-ref / mutable capture ABI | channel/special-queue/stack/AVL 的输出参数在 Typed XLIR 中是精确 `storage-ref<T>` 写入。若目标是 closure mutable capture，capture 分析将 binding 提升为 `retain-cell`；lowering 以 canonical zero/null 初始化函数局部 temporary，只把该 temporary 的 `STORAGE_ADDRESS` 交给 runtime，并按 bool 结果在成功边执行 trivial `LOAD + CELL_STORE` 或 managed `STORAGE_TAKE + CELL_EXCHANGE`，失败边释放 managed temporary 且不改变 cell。Cgen 不取得 cell payload 地址、不回查 AST。channel 专项门覆盖 int/string/var、recv/try/timeout、成功写回、失败保值、JSON 结构、确定性 C、严格 C 和运行语义；其他 resource family 复用同一 lowering ABI并继续由各自专项门扩展语义组合 |
| 精确 foreign C 标量 ABI | `c_char/c_schar/c_uchar/c_short/c_ushort/c_int/c_uint/c_long/c_ulong/c_longlong/c_ulonglong/c_size/c_ssize/c_ptrdiff/c_intptr/c_uintptr/c_bool/c_float/c_double/c_wchar` 保留目标平台 C 表示；语言标量与 C 标量之间的调用适配及显式 cast 都携带精确 source/result TypeId 和七类 canonical `ScalarConvertAbi`，不再是语义未分类的通用 C cast；extern prototype 不由 Cgen 按名称重建；`bool -> c_int` 已覆盖 JSON/stdlib 调用边界，`c_uint` 已覆盖 stdlib `time.sleep -> xrtSleep` |
| 受管值 | string copy/replace、`bool/i64/f64/xvalue -> owned string` 独立 runtime op、xvalue nested collection、数组更新、`array<int>`、`array<bool>`、`array<float>` 有界索引读写（各自显式 xvalue-array ↔ scalar 元素 ABI，写入经 owned xvalue）、`array<string>` 有界索引读写（读取复制 borrowed runtime text 为 owned managed string，写入经 owned xvalue）；显式 `array/list/set/dict(dynamic)` 先形成携带 `collectionAbi` 的 erased checked-cast，dynamic 到 `array/list/set/dict<T>` 再形成携带容器与 item TypeId 的 checked-rebind；两者消费 source、紧邻 `ERROR_TEST`，失败不修改既有 destination。成功结果随后由独立 `xvalue-type-rebind-owned` 写入目标 runtime TypeId：borrowed/stable source 必须先 clone 出唯一 COW wrapper，owned rvalue 直接转移；one-shot `TypeIdBind` 不承担语义重类型。模块 global、class field、local、assignment 与 call argument 共用同一 projection；顶层显式 initializer 的潜在失败进入 module-init may-raise carrier。erased collection + erased key 由 `xvalue-index-get-xvalue-key` 的双 borrow/null-sentinel ABI 和 `xvalue-index-set-xvalue-key-new` 的 collection/key borrow + item consume + trap ABI 固化，按 runtime tag 分派 array/int-map/object且不污染 error slot；typed `dict<T>` 的 bool/i64/u64/f64/string 写入先显式 boxing 后复用 object set consuming op；未使用的 owned string/xvalue/error/future/closure rvalue 物化为 `TEMPORARY -> RELEASE`，trivial 结果及已验证的 borrowed managed `LOAD` observation 用非拥有 `DISCARD`，其他 borrowed managed/product/native-handle 边界仍精确拒绝；临时 receiver/key 仍在 lowering 精确拒绝 |
| canonical IndexWriteAbi | INDEX lvalue 在 Typed XLIR 固定 array-i64/list-i64/object-literal/object-string/xvalue-key-dispatch 五种 consuming identity；verifier 重算 collection/key/item TypeId，lowering 只按 enum 选择 array-set、int-map-set、object-set 或 erased dispatch，所有 owned item 在成功/失败路径消费且 bool 结果紧邻唯一 trap。array/list 的 bool/i64/u64/f64/string、dict literal/string key、dynamic key、list `?=` 和泛型 int/string specialization 均有 JSON/C/严格运行覆盖；Cgen 不读取 IndexWriteAbi。完整 U64 fixture 因 legacy signed-xvalue 装箱缺陷保持 XLIR-only，泛型共享子集进入 legacy 差分 |
| xvalue 容器成员关系 ABI | typed array/list/set/dict 与 builtin dynamic 的 `in` 由 TypeId/`collectionAbi` 先验证元素或 string-key 契约，再形成 borrowed container + boxed item/key -> bool；typed set 保留直接 set-has，其余统一 runtime op 仅按 tag 选择稳定 array/int-map 值遍历、set、object key 或 runtime string substring，不兼容组合为 false。所有 owned item 与临时 receiver 都用 `TEMPORARY/RELEASE` 明确保活；静态 string、range、protocol dispatch 不混入此 ABI；专用运行门和 legacy 差分覆盖 |
| class 结构身份与 typed-set ABI | `ClassLayout` 同时拥有 hash/equality 的 `none/runtime-reference/runtime-derived/internal-call` 选择与精确 target，`ClassField.compareDeriveIncluded` 是派生对共享的唯一 `@compare(skip)` 分区。普通 class 默认使用成对 `runtime-reference`，因此 set 只去重同一对象；完整 `derive hash + derive equals` 对使用结构值身份。对象在 TypeId bind 后显式执行 `xvalue-class-identity-bind(payloadType)`，直接派生 `==/!=` 执行 `class-derived-equal(payloadType)`；Cgen 仅为 reachable exact layout 生成对应 callback，XRT value-set 由该策略完成去重和 lookup。只具备单边或尚未接入 callback wrapper 的显式用户协议在 typed set lowering 继续 fail-closed，不由 Cgen 临时拼接 |
| field-only native-backed managed 对象身份 ABI | 仅含一个 `native point` 字段的 class 使用普通 `RUNTIME_XVALUE_PTR`/managed ClassLayout；`point -> class` 在 Typed XLIR 固定为 `class-field-borrow + exact fieldDeclaration`，lowering 生成 class-new、TypeId/reference identity bind、tag、raw-pointer borrow box 和 exact object field set。相同底层 point 的两次构造保持两个对象身份，同一变量重复进入 typed set 才被去重。array/list/dict 索引读取精确 retain nominal object，xvalue iterator item 使用 retain-owned 生命周期，随后 `.handle` 才通过 exact raw-pointer field op 读取；两条路径都禁止 native-handle projector。无 owned factory/destroy 的 native extern receiver 已使用显式 physical field-borrow ABI；带 factory/destroy 的完整 owned lifecycle 仍保留 transitional direct-handle ABI，待独立迁移 |
| managed native extern receiver physical ABI | lifecycle-free native-backed class 的 extern receiver 在语义层仍是 managed nominal class；CallTarget 以 `parameterTypes/parameterAbis` 保存 semantic borrow，以 `parameterPhysicalAbis=native-class-handle-borrow` 和 `parameterPhysicalFieldDeclarations` 保存唯一底层字段。lowering 显式发射 `xvalue-object-get-literal-raw-pointer`，验证 CALL 消费该 exact projection，Cgen 只按已验证物理 ABI声明 `void*` 并转发 operand。相同 raw pointer 的不同 wrapper 保持 reference identity；direct nominal-handle projector 被质量门禁止。带 owned factory/destroy lifecycle 的 native class 也已使用 managed nominal 表示，通过 factory field TAKE、method BORROW、explicit destroy TAKE 和自动 cleanup 固定生命周期，并由独立运行门验证 |
| canonical collection-method and algebra operation ABI | 静态泛型 receiver 以 `TypedCollectionOp + CollectionAbi` 固定 pop/take/clear/capacity/reserve/resize/trim/trimTo/swap/insert/indexOf/reverse/isDisjoint；开放 `var` receiver 以独立 `DynamicCollectionOp` 固定 push/insert/remove/removeValue/pop/take/clear/reserve/swap/add/contains/indexOf。静态标量/string pop 与 array take 保留精确元素 TypeId；稀疏 list/dict take 因缺键语义保留 owned 动态 TypeId。静态 array/list/dict/set 代数与 set 关系由 canonical opcode + exact operand/result TypeId 唯一解析为 13 种 Lowered runtime op，复合赋值是显式 read/op/release/store；通用 xvalue 算术不再包含容器分支。lowering 显式 borrow receiver、box/anchor item、consume 动态参数并发射唯一 runtime op；Cgen 仅按 verified enum/TypeId ABI 机械选 helper。typed 全物理 ABI、动态 12 操作、代数 22 个表达式/复合赋值实例、JSON、确定性、严格 C 和运行门已接线；typed U64 pop 的 legacy signed-box 历史差异使完整 typed fixture 保持 XLIR-only，其余动态/代数全集进入 legacy 差分 |
| managed-string 成员关系 ABI | 静态 string 的 `in` 独立降为 borrowed haystack + borrowed needle -> bool 的 `string-contains`；literal/调用结果各有一等 managed-string temporary 与显式 release，Cgen 机械调用 `xrtStrContains/xrtStrView`。与 xvalue tag dispatch、标准库 CallTarget bridge、range 和 protocol dispatch 保持分离；专用 fixture 与 mixed-container 既有用例均严格运行并与 legacy 差分 |
| canonical non-call member ABI | 非调用成员表达式先在 Typed XLIR 固定 `MemberAbi`，源码拼写在此后仅用于诊断。`string.length` 是 `string-utf8-count`：borrowed managed string -> i64；class `value.length` 在无真实同名字段时是 `length-protocol-call`，并携带唯一零参 `__len__` target symbol。前者降低为 `STRING_UTF8_COUNT`，后者降低为普通 one-borrow CALL；Cgen 不读成员名或搜索协议。UTF-8、普通 class、native class fixture 均通过严格 C、运行时及 legacy 差分，标准库 `Buffer.length` 也已在 Typed IR 精确绑定 |
| canonical class-field projection ABI | concrete xvalue class 的字段 projection 先以 receiver TypeId + `ClassLayout` field slice 匹配，再分别固定 xvalue borrow、bool/i64/u64/f64、owned string copy、raw pointer borrow、无析构 native pointer、带析构 native handle borrow。lowering 只按 MemberAbi 生成一一对应 literal-key RuntimeOp，raw point 写入用独立 borrowed boxing op；Cgen 不读 MemberAbi、字段类型或析构信息。专用 fixture 严格运行 scalar/pointer/handle 读写与 borrowed/owned native 生命周期，并与 legacy 差分 |
| canonical range membership ABI | verified RANGE IntrinsicCall descriptor 是唯一语义输入；1/2/3 参数在 lowering 补齐为 item/start/stop/step 四 i64 -> bool，descriptor 不物化。生成 helper 对正负/运行期零 step 使用半开区间和 unsigned distance/magnitude，避免 signed overflow；Cgen 不识别 builtin 名或重建默认参数。专用 fixture 与既有 `cgen_in_range` 严格运行并与 legacy 差分；async 组合已推进到独立调用边界 |
| canonical range-for ABI | Typed LoopAbi 精确区分单 value 与零基 ordinal/value 双 target；verified RANGE descriptor 保持 no-raise，三参数 loop 独立携带 may-raise effect 与 throw exit plan。lowering 将边界/步长各求值一次，显式构造正负 CFG、continue increment、动态零 step 的 managed message/RangeError/exception transfer；Cgen 不读 iterable、target count 或 LoopAbi。专用 fixture 与既有 `cgen_range_assert` 通过严格 C、运行和 legacy 差分；int64 极值 increment 溢出策略仍待独立 ABI |
| canonical storage-backed array iteration resource ABI | 精确 `array<int>` 在 Typed LoopAbi 区分 value 与 ordinal/value；稳定命名输入携带 `shallow-snapshot-release + TypeId` 并 clone，owned literal/returned rvalue 携带 `owned-iterator-release + TypeId` 并直接 transfer。lowering 以 resource storage、count、i64 position、typed projection 和唯一 canonical exit release 构造普通 CFG。active LoopResource 栈按 loop/exception depth 为 return、outer exception 和完成后的 finally continuation 逆序释放，循环内 catch、continue 和同循环 break 保持到 canonical exit；调用、直接/finally return、local/outer catch、exceptional/break finally、源数组重绑、literal/returned acquisition 均有结构、严格运行与 legacy 差分。动态 `var` 明确排除在静态 array ABI 外并路由到 checked runtime iterator；managed array element、suspension frame 与超大 cardinality 边界仍待独立 ABI |
| canonical xvalue snapshot iterator ABI | xvalue-backed collection 使用与精确 source TypeId 绑定的 compiler-only `VALUE_ITERATOR` owned cursor；LoopAbi 固定 value、key-only 或 key/value 目标形状，key/item projection 与 `LoopTargetLifetimeAbi` 正交表达。list 的 i64/u64/bool/f64/string、set 的 i64/bool/f64/string，以及 dict 的 string key-only/key+item（item 为 i64/u64/bool/f64/string）共用 create/advance/projection/destroy、普通 CFG、active LoopResource 和 C helper。managed key/item 从 borrowed runtime text 复制为 per-iteration owner，并在 fallthrough/continue/break/return/throw/finally 上按 item→key→cursor 逆序释放；return 先复制独立 owner，finally 仍可观察原 target。稳定命名及 owned rvalue、参数、调用、catch 与所有退出均有覆盖，严格运行已接线；legacy 差分覆盖其能精确表示的 i64/bool/f64/string dict，完整 `dict<uint64>` 因 legacy signed-xvalue 装箱缺陷保持 XLIR-only。当前 XRT 还拒绝 `set<uint64>`，对应 Typed 负例保持显式；xvalue-retain item、generator/protocol/async iterator 尚待扩展。typed list 原地写入现由独立 `IndexWriteAbi` 正确选择 int-map set，不再属于 iterator 阻塞项 |
| typed callable container conversion/projection ABI | `array/list/set/dict<function(...)>` 的精确 closure 元素在 Typed XLIR 中以 `implicitConversionErrorAbi=may-raise` 标记隐式 runtime callable 装箱，由字面量节点拥有唯一 THROW ExitPlan；lowering 只根据 canonical boxing matrix 生成携带完整 function TypeId/erasure ABI 的 `xvalue-callable-from-closure` 并紧邻 `ERROR_TEST`。容器读取和 array/list/set/dict 循环统一使用 owned `xvalue-project-closure-copy` / `closure-copy + owned-iteration-release`；索引结果等复杂 callee 在 Typed 阶段规范化为无 target symbol 的 `function-value`，再降为唯一 `CLOSURE_CALL`。四类字面量、四类循环、直接/复制/索引调用均已通过 JSON、确定性 C、`-Werror`、运行断言和 legacy 差分；Cgen 不读容器类型或 callee 源语法重建语义 |
| typed safe-index nominal presence/projection ABI | `array/list/dict<Pair|Payload|Native|func>` 的安全索引统一先返回 canonical dynamic xvalue carrier，再按结果 TypeId 显式投影为 retained nominal xvalue、tagged union copy 或 owned closure copy；整数、字面量 string 与动态 xvalue key 不再有直出 `nullable<T>` 的旁路。`nullable<Native>` 使用 managed xvalue presence carrier，因而能区分“缺失”与“元素存在但 native handle 为 NULL”；非空 retained projection 必须核对 payload TypeId。24 个 lookup、24 个精确投影及 callable 调用通过 Typed/Lowered JSON、确定性 C、`-Werror`、运行断言和 legacy 差分；Cgen 只消费 operation + payload/callee TypeId |
| typed nominal index assignment/readback ABI | `list/dict<Pair>`、`array/list/dict<Payload|Native|func>` 的普通写入由 Typed `IndexWriteAbi` 唯一选择 array/list/literal/dynamic-key consuming op；Pair 保持绑定后的名义 xvalue，union 显式 box，pure-native handle 显式绑定 stable TypeId，function 装箱携带完整签名并紧邻 checked error edge。读回端用 `xvalue-project-xvalue-retain`、union unbox、native handle 与 closure projector 恢复静态 ABI；dynamic-key scalar 读取则分解为 canonical lookup carrier + `xvalue-project-scalar-copy(payloadType)`，不迫使 Cgen 推断元素类型。14 个名义写入、四个 checked callable effect、12 个名义读回投影及 scalar 动态键投影已通过 JSON、确定性严格 C、运行断言与 legacy 差分 |
| xvalue scalar bridge / exact flow / may-raise create ABI | exact scalar/string 到开放 `var` 的返回边界显式装箱；非 class `is` 是 `xvalue-type-id + stable TypeId const + EQ`，流缩窄名称仍保留 dynamic storage TypeId，但通过统一 exact projector 得到精确 scalar 或 owned string。string 转换按 lowered value TypeId 选 operation，string projector 直接转移 owner。may-raise `create` 使用 void result carrier、`ERROR_TEST`、失败 release/传播与成功 load/move；Cgen 只消费这些 IR。正向和实际失败/catch 路径、确定性严格 C 与 legacy 差分均已接线 |
| statically shaped erased collection iterator ABI | 裸 `array/list/set/dict` 在 Type 表中保留各自精确 `CollectionAbi`、runtime TypeValueId 与零参数形状；零参数明确表示元素使用 canonical fallback `var` TypeId，不表示来源形状未知，也不与 `collectionAbi=none` 的动态 `var` 混同。默认值通过已验证的 array/int-map/set/object empty-constructor RuntimeOp 初始化。for-loop 复用与 source TypeId 绑定的 cursor、独立 ordinal/integer/string key projection、owned xvalue-retain item projection和 active LoopResource；array/list/set/dict 的 value、key/value、key-only，snapshot mutation、continue/break/return/finally、参数与 owned rvalue 已通过确定性 JSON/C、严格运行及 legacy 差分 |
| runtime-checked dynamic var iterator ABI | `collectionAbi=none` 的动态 `var` 使用独立 may-raise dispatch，而不伪装成任一静态 collection。checked-create 仅接纳 array/list/set/dict runtime tag 并紧邻唯一 `ERROR_TEST`；失败产生精确 `TypeError`。单 target 由 `dynamic-logical-xvalue` 固定 dict-key/其他-item 语言语义，双 target 由 `dynamic-xvalue key + xvalue-retain item` 固定装箱与所有权；显式 `_` 分别固化为 `key=discard` / `item=discard` 且 lifetime 为 `none`，lowering 只推进 cursor，不生成投影、storage 或 target cleanup。source-bound cursor、临时 source 成败释放和 target/cursor 的 continue/break/return/finally 清理全部存在于 verified XLIR。专用 fixture、既有动态容器循环与非容器负路径进入确定性、严格 C、运行及 legacy 差分/接纳审计；Cgen 只机械调用 helper，不回查 source 类型或目标数量 |
| canonical UTF-8 string iterator ABI | managed string source 固定为 `string-utf8-value` 或 `string-utf8-index-value`，key 是 canonical i64 Unicode scalar ordinal，item 是每次迭代新拥有的 managed string。稳定命名 source 经 `string-copy` 获得快照，owned rvalue 直接转移；`string-snapshot-release` 与 per-iteration target resource 在 fallthrough/continue/break/return/throw/finally 上逆序清理。lowering 生成 count/position/branch/increment 普通 CFG，字符仅由精确 `string-utf8-at(string,i64)->string` 投影；Cgen 不读取 LoopAbi、不计算字节边界。中文、emoji、空串、重绑快照、参数/返回值与既有 `stdlib_string` 已进入确定性、严格 C、运行和 legacy 差分门 |
| canonical class/native protocol iterator ABI | stable named class source 由精确 nominal TypeId 解析本声明上的零参 `__iter__`，其返回 class 再解析零参 `__next__ -> (bool,T)`；继承、virtual 和未具体化泛型在尚无显式 receiver upcast/dispatch ABI 时继续 fail-closed。Managed iterator 是 owned class resource，managed item 从 product TAKE 后成为 per-iteration owner；native iterator 以 opaque handle + exact destroy CallTarget 清理，绝不要求或制造 xvalue `ClassLayout`。单/双 target、trivial/string item、正常耗尽、continue/break/return/throw、Buffer native handle、File extern getter、确定性 C、严格运行及 legacy 差分均已接线；标准库 bridge 的独立可链接 runtime 分发仍是后续标准库收口项，不由 iterator Cgen 补定义 |
| 上下文 null 表示 ABI | 类型表以 `NullAbi` 区分 pointer/xvalue sentinel 与 tagged product；`null` 只在目标 TypeId 已知时产生 `NULL`，比较产生 `NULL_TEST`；class→nullable 的同表示转换使用 verifier 可见的 `VALUE`；`var`/`nullable<class>` 的 `??` 使用带显式 `RETAIN/RELEASE` 的 lazy CFG，class present 分支以同表示 `NULLABLE_UNWRAP` 投影，literal null 由 lowering 按结果 TypeId 折叠；直接 storage 的 `?=` 对 xvalue sentinel 生成 lazy test/branch/release/store CFG，静态非空 TypeId 不生成 RHS，旧 Cgen eager-class-fallback 被列为 XLIR-only 历史差异；`nullable<bool/int/uint64/float>` 使用 compiler-owned tagged struct 与显式 `NULLABLE_WRAP/UNWRAP`，`??` 是 lazy CFG merge；`errorResultCName` 是 verifier 可见的 may-raise carrier，不由 Cgen 重建。覆盖内部参数/返回、默认 null、赋值、公开普通/may-raise export、immutable closure capture/result、mutable capture cell、async result/frame、typed continuation coalesce、class erased-field 默认/显式初始化/赋值/读取/重置和 legacy 差分。class 字段通过 `tagged-null` 与独立 box/projection runtime op 保留精确 TypeId；managed nullable payload 与复杂 product 组合仍须扩展所有权/边界 ABI |
| canonical CallSite / CallTarget ABI | 普通函数与 class constructor 共用按 parameter 排序的 explicit/default/named binding slice；ARRAY/DICT variadic tail 在 CallSite 中保留 item 来源，并由 lowering 显式物化为唯一 aggregate 物理参数；native C variadic target 保留固定 prototype 前缀和动态 typed operand tail；泛型单态化克隆内部 site/argument slice，并把外部 site retarget 到 concrete symbol。Cgen 只消费经验证的 CallTarget/CallSite 事实并机械发射 `...`。文本/JSON 同时公开 target、site、argument 与 variadic ABI，严格宿主覆盖默认/命名、空/非空 aggregate、单态化以及 `va_list` |
| class protocol syntax CallSite ABI | class unary/binary、`in`、`[]`、index assignment、缺失 member 和 callable syntax 在语义阶段选择唯一 magic method，统一生成 CallSite；`__contains__` 固定反向 receiver/item 顺序，`__item__` 固定索引参数 ABI，ASSIGN 的 `__setItem__` 固定 receiver/全部 index/value 与 void ABI，`__field__` 使用 `implicit-string` CallArgument，`__call__` 的非 member callee 本身成为 receiver。简单索引写不执行 getter，也不伪装为动态数组写；lowering 只按目标 parameter ABI 物化参数并用 TEMPORARY/RELEASE 保活 rvalue receiver/managed string，Cgen 不搜索 `__*__` 名称。三个既有 protocol fixture 及泛型 `__field__` specialization fixture 进入确定性、严格运行与 legacy 差分门 |
| TypeId 与 class string conversion ABI | `IntrinsicCall.stringConversionAbi/stringClassType` 在 Typed XLIR 固定 TypeId name、derived class 与 explicit protocol call；`ClassLayout.stringAbi/stringTarget` 和 `ClassField.stringDeriveIncluded` 是唯一 class 协议与字段分区输入。lowering 分别生成 `type-id-to-string`、携带 layout TypeId 的 `class-derived-to-string` 或普通 exact CALL；Cgen 只消费 TypePool/ClassLayout 数据并返回 owned string。专用 fixture 覆盖 member sugar、`@string(skip)`、显式 `__toString__`、确定性严格 C、运行和 legacy 差分 |
| copy/clone 所有权 ABI | class layout 固化 `copy/clone` 协议选择；`derive copy/clone` 分别降为精确同类型的 owned `xvalue-clone/xvalue-deep-clone`，用户 `__copy__/__clone__` 降为 borrowed receiver 的 internal call；具体化后的无初始化标量泛型字段使用显式 `scalar-zero` class-field ABI；泛型 cleanup action 以 callable owner 区分 dormant template 与 executable specialization；覆盖普通/泛型 class 字段浅拷贝、嵌套容器深拷贝和动态容器，并与 legacy 运行结果差分 |
| 异常与 cleanup | typed/multi catch、finally、rethrow、受管 return、error cause/fields | cleanup materialization 已由统一 XLIR verifier 验证；mutable closure cell promotion 同步把 lexical RELEASE 规范化为 cell TypeId 的 CELL_RELEASE，后端不再扫描或推断 cleanup。
| 泛型与闭包 | 推导泛型容器、函数值签名参数 `value/borrow`、动态实参 borrow `TEMPORARY` storage（与用户 `LOCAL` 分离并验证同块 release）、嵌套/闭包捕获、mutable xvalue cell 与 finally；标量复合赋值经显式 load/binary/store |
| 标准库桥接 | `println` 经 `array<var>` 的显式 xvalue 外部 ABI 调用受检打印片段；`string.contains`、`string.splitLines` 私有依赖与 print 均由 CallTarget 的 `runtimeBridge` 显式固定并验证完整签名，Cgen 不再按 C 名称推断 helper 语义。`splitLines` 的 bridge 固定 `borrow string -> owned xvalue`，生成 C 自包含 XRT string-list → xvalue-array 适配，不依赖 `xl.dll` 隐式补符号；完整 dynamic-container fixture 在仅链接 `xrt_cgen.o` 时以 `-Werror` 编译并运行。静态元素类型的泛型容器仍未因该桥接而视为 xvalue |
| native 标准库 handle | class TypeId 以 nominal symbol 关联声明；`DynamicFields` 与用户 native class 共用 native `void*` ABI、成员 receiver、factory 和内部 wrapper，parameter 0 固定为 borrow。pure-native class 的 `native point` 字段读取由独立 Typed `NativeHandleProject` 固定 declaration 与非拥有方向，再降为 `NATIVE_HANDLE_PROJECT`，不会误走 xvalue object lookup。`var` 值桥接、显式 `destroy()` 与作用域自动析构由 typed cleanup action 降为普通已解析 call；严格运行验证生命周期，Cgen 不以类名、`self` 使用或源码成员名识别它 |
| native handle bind/take 所有权 | raw `point` 构造 pure native class 时，typed XLIR 用 exact nominal target 选择 `BORROW` 或 `TAKE`，并记录 source storage 与 destroy symbol；TAKE lvalue 显式置空源，TAKE rvalue 在借用调用边界物化为唯一 temporary，正常/退出 cleanup 都是 null-safe destroy CFG。Cgen 不从类名、构造语法或析构存在性重新推断所有权 |
| canonical value/default initializer ABI | value declaration 由独立 `ValueInitializer` 固定 explicit expression 或 scalar-zero/string-empty/null-sentinel/xvalue-null/xvalue-container-empty/class-default/class-null；无 C linkage 的 package export 是普通 module GLOBAL initializer，显式 `export "c_name"` 才是静态 GlobalExport。无显式 `create` 的 concrete class 使用 class-default 构造，具有显式 `create` 的 module GLOBAL 使用 nominal class-null，二者即使物理表示同属 xvalue 也不混淆。容器记录直接携带 array/int-map/object/set 构造 runtime op，泛型实例克隆 declaration/expression/type/ABI 整体，lowering 生成显式 CONST/NULL/runtime/default-class-construction/STORE。已运行覆盖 bool/int/float/string/point、array/list/dict/set、package int/var/const、显式构造 class 空默认值、无析构 native handle 复制与赋值、有析构 native handle 空 owner，以及 scalar/string 字段的零参 concrete xvalue class；嵌套 class field/product 默认构造仍须等待各自一等 ABI，不能退化为空指针或 C 零初始化 |
| explicit constructor own-field initializer ABI | concrete xvalue class 即使有显式 `create`，仍按 runtime tag → inherited fields → own `ClassField.initializerAbi` → create body 的顺序构造；本类 explicit/scalar-zero/string-empty 字段复用默认构造路径，body 只覆盖已存在字段。顺序完整存在于 lowered CFG，Cgen 不补字段或依赖 C 零值 |
| callable reachability / emission closure | Lowered XLIR 将 module-init、export、entry 标为 root，并沿 direct/extern call、closure make、virtual/protocol implementation row 与 async 状态传递为 transitive；其余 Function/CallTarget/ClosureTarget 明确为 unreachable。Verifier 重算闭包，Cgen 仅消费并发射可达实体和 runtime needs；未调用的 stdlib wrapper 不再产生 C 声明、函数体或链接依赖 |
| canonical product result ownership ABI | 多返回 callable 的语义返回列表保留在 function type 中，声明无论是否被调用都主动驻留 canonical tuple，唯一物理结果由 `callableResultType`/CallTarget return TypeId 固定。return、direct/class/closure/named-adapter call 与声明/赋值解构只产生显式 `product-make/get/take/drop`；GET 仅观察 trivial 字段，TAKE/DROP 分别移动或销毁 managed 字段。Verifier 要求 managed aggregate 整包返回或每个托管字段恰好消费一次；Cgen 机械发射按 TypeId 去重的 named struct、字段置空和对应 release，不回查 AST。已运行覆盖全 trivial 自由函数/class method/匿名 closure/具名函数/纯标量泛型，以及 managed string 覆盖赋值、双字段 take、通配符 drop 和 legacy 差分；exported class-method product 已由 `check_xlir_export_boundary.ps1` 验证，包括 C/header 一致性和 module-init 失败返回；其他 product effect/ownership 组合按精确 TypeId 与 carrier 合同接纳 |
| named function adapter / callable effect CFG ABI | 命名函数值按 resolved target + signature 去重为 capture-free `ClosureTarget`，并由 synthetic `NAMED_ADAPTER` Function 持有完整 CFG。默认参数在 adapter 调用时经显式 const/storage 求值，direct/extern call、Product return 和 closure construction 都由 verifier 固定；first-class callable thunk 统一返回 success carrier，并以 out-result 携带非 void 值，may-raise adapter 显式分支到 THROW。Cgen 只发射已验证 thunk。已运行覆盖顶层赋值、函数参数、重载选择、默认参数、trivial Product 多返回、异常传播、C callback export 与 extern C target；open function erasure 与 fixed/array-variadic adapter 已由 `check_xlir_open_function_abi.ps1` 验证，adapter 的参数和失败 CFG 均为显式 XLIR |
| runtime callable `xcall` ABI | `xcall` 的 closure→runtime callable 与动态调用分别为 `xvalue-callable-from-closure` / `xvalue-dynamic-call`；前者携带精确 function TypeId，后者借用 callable 并在成功/失败均消费 owned xvalue 参数。两者以 nullable owned result carrier 紧邻 `ERROR_TEST`。Cgen 仅使用 public XRT callable/result API，并按 verified TypeId 去重 adapter；已严格运行验证 extern/internal `int,int -> int`、任意参数个数的 value `bool/int/uint64/float` 单结果签名、借用 nominal class 参数及 scalar/owned nominal class 结果，以及零参 may-raise bool 的原始 error cause 投影。标量与 nominal class 签名均与 legacy 差分，effect 投影由 XLIR 专用运行门覆盖（legacy 零参 may-raise xcall 仍触发其历史 ABI violation）。string/xvalue/void/product、命名/变参动态调用尚未接纳 |
| 默认 xvalue class layout | 无显式 `create` 的零参 class：typed XLIR 持有基类、字段、字段类型与 initializer slice；lowering 按基类→子类顺序形成 object allocation、boxing、literal-key field set 和 failure edge，覆盖 `Point/Point3` 继承、读写及临时构造 |
| instance xvalue class method | concrete class 的 `create` 与非 static `func` 具有 receiver parameter 0 的 internal callable ABI；成员调用、方法内 field read/write、返回值均 lower 为普通 call/CFG 和 object runtime instruction，`object` runtime/protocol root 不在该 ABI 切片中 |
| declaration-driven class receiver ABI | concrete class 的 `create/get/let/func/async func` 是否具有 receiver 完全由声明和所属 class 决定；TypeId 来自 canonical class identity，typed `self` 只提供可选的同类型 symbol，缺失时由 lowering 建立 synthetic parameter storage。CallTarget/Function slot 0 固定为 borrow，Cgen 不搜索函数体；已覆盖无 `self` 构造器、getter、setter、实例方法的严格运行和 legacy 差分 |
| explicit class constructor/default-base | 即使基类未独立出现在 runtime type pool，typed XLIR 也保留 declaration inheritance edge，并将其默认字段形成派生 layout 的 inherited slice；对象先写 concrete runtime tag，再初始化 inherited defaults，最后调用 `create` body |
| static class namespace/storage | static field 以 class declaration identity（可选 runtime type）归档到 typed XLIR，lowering 为其分配唯一 `GLOBAL` storage；static method 是无 receiver 的 direct callable，字段读写及初始化只生成 load/store，不要求把静态 namespace 伪装为 xvalue object |
| virtual class dispatch / polymorphism | typed XLIR 的 slot/implementation 表是唯一分派依据；lowering 将语义成员调用固定为 `virtual-call(slot)`，并以已写入 object metadata 的 concrete runtime type tag 选择实现。每个非泛型名义 class 由声明主动驻留 canonical TypeId 与 layout，即使基类从未在表达式或 `self` 中出现；继承边通过 declaration symbol 绑定，不能依赖类名搜索或偶然的类型使用。派发表构建对不完整签名、返回/参数 TypeId、缺失/歧义 slot 分别给出精确诊断。已接纳无异常 ABI 的 concrete xvalue class virtual 方法、override、基类属性内的 virtual 调用、class `let` setter、派生→基类显式 xvalue rebind、`is/as` type-tag 和 `array<base>` 的 owned 派生项 append。Cgen 只消费 slot/row/type-tag/runtime-op 数据，不回查源码方法名或继承关系 |
| protocol dispatch ABI | typed XLIR 持有 requirement slot 与 concrete witness row；已接纳零参数、重载、具体泛型 protocol requirement、getter/setter、result-void 及 async Future ABI。泛型 class 只为实际使用的 `Box<int>` 一类 concrete type 建立 layout，并在 lowering 中生成隔离的构造/get/let 单态实例；async dispatcher 根据 witness target kind 调 launcher 而非 worker；Cgen 不读取 class/protocol 声明重新匹配 witness |
| Future 与 async | await/finally、三段 continuation、string/xvalue capture、即时 Promise reject；async concrete/generic class instance method 使用显式 receiver 参数和 retain/release task frame ABI，泛型 worker 与 public Future 拥有分离且已验证的 result ABI；may-raise 普通函数可通过 `xlir_result_future` 成功传递 owned Future、失败传递 `NULL`，覆盖局部 class receiver 跨任务生命周期；条件 future 以 owned join temporary 表达 borrowed/call/null 分支，AWAIT 之后对称释放并保留语言级 null-await 错误；普通 async 参数覆盖 string copy、xvalue array/class/dynamic 与 closure retain/release frame ABI，函数值参数先显式 rebind 到 canonical signature type；命名 Future 支持重复借用 await，动态字符串拼接经显式转换和临时释放。所有含 await 的 worker/分析 CFG 现拥有 `AsyncMachine → Entry + MachineBlock + AsyncSuspension + AsyncMachineFrameSlot + AsyncLiveSlot + AsyncFrameAction`：只从 Lowered block/instruction、ExitPlan 和数据流计算状态边、逐 AWAIT suspension split、SSA/storage liveness 与 failure cleanup transition。机器级唯一 frame layout 保存 source/type、`LAUNCH_PARAMETER / FRAME_STORAGE / SSA_SPILL` residence、精确 state range 与 managed init bit；`transferAbi` 区分 `NONE / COPY / RETAIN / MOVE_OWNED`，`destroyAbi` 独立固定 drop glue，移动字段以 `takeInstruction` 指明成功 CFG 中精确的 ownership sink。prologue 按 frame 顺序初始化参数/绑定 storage，各状态按 first-live spill 和 live project 执行，destroy 按 frame 逆序 guarded 释放；不可表示或存在多重 move sink 的 live type 成为显式 `BLOCKED_LIVE_ABI`。lowered internal-async `CallTarget` 直接持有唯一 machine 索引，verifier 独立重算 entry/block split、唯一 failure ExitPlan、layout/membership/action 顺序及 transfer/take/destroy 关系。Cgen 已直接执行 launch/resume machine program，覆盖 extern/parameter Future、二/三段 suspension、复杂分支、managed string、xvalue、closure capture/result、catch error channel 和 tagged nullable coalesce；多段 activation 仅按 live-set 移交字段并保留唯一 outer Promise。代表性 admission 门确认 48 个 fixture / 46 个 analyzed machine / 60 个 canonical suspension 全部 direct，完整已支持 inventory 另确认 80 个 fixture / 45 个 analyzed machine / 54 个 canonical suspension 无 non-analyzed machine、orphaned suspension 或 legacy renderer；旧 chain/capture/catch renderer 为零，C 文本确定且严格运行通过。analyzed machine 若出现未接纳 instruction/frame ABI 会 fail-closed，不再静默回退兼容表。Cgen 源码已删除 continuation lookup、runtime-needs 推断和 chain/scalar/string/xvalue/catch renderer，普通/async closure 指令共用同一 helper；旧 `AsyncContinuation/AsyncFrameSlot/AsyncResumeOp` 类型、arena、验证与 dump 也已物理删除，合同门禁止其重新进入实现。 |
| 模块与导出 | module init、普通全局、auto/带错误 ABI 的函数 export；公共参数 `value/borrow`、返回 `void/value/owned` 由独立 `XL6_IrExport` 表固化，class xvalue owned-return/borrow-parameter 与 Type-ABI 名义重载名已经运行验证；显式 C 标量数据导出由独立 `XL6_IrGlobalExport` 固化 storage/声明/初值/type/mutable-or-const/C name，C/header 和内部 LOAD/STORE 共用该映射，并已验证函数调用前直接宿主访问；外部标量数据依赖由 `XL6_IrExternGlobal`/`EXTERN_GLOBAL` 固化 read-write/C-name ABI，只进入生成 C、不进入 export header，并已验证宿主提供存储的读写 |

`check_xlir_migration_coverage.ps1` 将这些簇具体绑定到 fixture 名称；测试名是
此表的权威可执行索引，而不是仅供阅读的状态描述。

## 网络与外部 ABI 组件

网络/TLS 运行时仍可复用后端无关的 transport 机械代码，但组件选择和语言对象 ABI 必须来自
verified XLIR。可达 `CallTarget.runtimeBridge` 决定 TCP/TLS 大类；TCP file 等子操作选择由
lowering 固化的 `CallTarget.runtimeBridgeOperation` 完成；Stream/NetBytes wrapper
的 TypeId 从 physical receiver/Future payload ABI 取得。共享组件内部固定存在的 NetBytes
adapter 只允许读取两个 recv-owned-bytes operation 的已验证 `Future<Class>` 结果，不得聚合
同 bridge 的任意 `Future<Class>`，也不得按 C 符号或源码类名补类型。TCP stream/listener stats
及 stats value/destroy ABI 由 shared emitter 唯一定义；已有 bundled XRT 声明时，fallback
extern 必须由 `XLIR_XRT_DECLARATIONS` 抑制。

operation 的 verifier 同时校验 borrowed native class receiver、path/handle 参数区别、
offset/size/timeout 的有符号整数 ABI、result-code/bool 返回区别，以及异步接收的
`c_size -> future<managed class>` 结果。`test/cases/xlir_cgen.c` 对四种文件传输声明和
TCP/TLS 异步接收执行正负 ABI 检查，并直接篡改已验证模块的 operation 确认最终 verifier
拒绝不一致元数据。文本/JSON dump 都公开 operation，Cgen 合同门禁止按上述 C 符号推断。

`check_xlir_foreign_function_value_abi.ps1` 固定 TLS verify callback 的 exact `bool +
XL6_Function` 物理 ABI和组件定义唯一性；`check_xlir_async_machine_admission.ps1` 的网络/TLS
fixture 固定组件闭包、严格 C 和运行语义；`check_xlir_default_admission.ps1` 进一步保证这些
裁剪不会降低默认 XLIR 可接纳覆盖。

## 默认后端条件

Base64 文本编解码拥有独立的 `base64-text-codec` 运行时桥接：输入固定为
borrowed managed string、`c_size`、可空的字母表 raw pointer，结果是由 `TAKE_CSTR`
消费的 owned raw text。lowering 与 verifier 均验证完整签名；文本独立使用不能依赖
Buffer 或其他 codec 的偶然可达性。新旧后端共享 `xl6_cgen_text_codec.c` 的实现，
XLIR Cgen 只消费 bridge marker，声明面只包含该组件所需的公开 XRT ABI。
`check_xlir_base64_text_codec_abi.ps1` 验证签名负例、组件裁剪、严格生成 C 以及两后端的
`run/build`，不把仅成功生成 C 视为标准库链接已完成。

字节编解码另有 `hex-bytes-codec` / `base64-bytes-codec` 桥接：编码输入是精确长度的
raw bytes，零长度不能回退到 `strlen`；解码结果是 owned Buffer，不能经文本临时量
丢失内嵌零字节。两后端共享同一 codec 组件，文本和字节族分别裁剪。
`check_xlir_byte_codec_abi.ps1` 检查两种独立字节程序、三类签名负例及严格 C/运行差分。

动态 `var` 的整数下标写入不能因 `CollectionAbi=none` 被解释为 array。Typed XLIR
必须选择 `xvalue-key-dispatch-set-new`，lowering 显式装箱 key、锚定临时接收者，完成
写入后在成功/失败 CFG 分支之前释放地址临时量。已有运行时 dispatch 负责区分 array
边界写入和 list 稀疏整数键插入；Cgen 不新增类型猜测。
`check_xlir_dynamic_integer_index_write.ps1` 覆盖嵌套别名、负整数键、求值顺序与次数、
可捕获的越界失败及新旧后端运行差分。

核心旧回归的输出拼写断言按语义迁移，而不是继续固定旧 Cgen 的私有临时变量：
`check_xlir_current_core_contract.ps1` 检查调用结果锚定、投影、链式接收者和显式释放；
`check_xlir_scalar_container_contract.ps1` 检查逻辑整数 normalization domain、物理存储、
容器 runtime opcode 和迭代 key/item 所有权。原 suite 的编译、运行、错误与输出断言保持接线。
`check_xlir_container_ownership_contract.ps1` 将返回容器的 27 个循环、类型标识去重、
共享引用赋值以及 callable/class 容器投影统一检查为具体 IR 合同。index 虽可作为左值，
读取其中的 closure 仍产生 `MOVE` 所有权；closure invocation 与普通 borrowed 参数
共用 `xl6IrLowerAnchorResolvedCallBorrow`，按已验证的所有权而不是仅按 value category
锚定结果。投影所有者必须在调用之后、成功/失败 error-test 之前恰好释放一次。
直接编译默认 `cgen` 模块的回归使用已有 `xlir_generated_module_host.c` 显式调用私有
模块初始化器；生成模块只被包含进宿主一个翻译单元，外部支持 C 文件独立编译，
不要求编译器重新发射进程级 `main`。迁移后的这些编译调用使用 `-Werror`。

`cgen`、`run`、`build`、调试构建和编译器 DLL 只使用 XLIR。旧 AST/HIR Cgen、HIR
构建器、构建接线和 `XLC_BACKEND_LEGACY` 已物理删除；CLI 在参数解析阶段拒绝
`--backend legacy`。`xlcSetBackend` 只接受 `XLC_BACKEND_XLIR`，保留该调用是为了
显式选择和 ABI 稳定，而不是保留第二条语义路径。
`cgen` 的产物边界是可嵌入 C 模块：有语言入口时保留 `xlang_main`，但不生成进程级
`main`；`build` 才请求 native main wrapper，内存 `run` 则请求 script host entry。

单一后端不允许恢复语义猜测或静默回退：可表示语义必须先形成已验证 XLIR，无法满足
明确类型、所有权、effect、调用或运行时桥接契约的输入继续在 lowering/verifier fail-closed。
`check_xlir_default_backend_boundary.ps1` 同时验证 API/CLI 静态边界以及默认与显式 XLIR
生成 C 的逐字节一致性；它还验证非法后端值被拒绝后，同一 context 可以继续正常编译。

`check_xlir_current_source_admission.ps1` 守护 canonical backend、退役 selector 拒绝、
冻结的源码 provenance、批量 identity 证据和场景所有权；`check_xlir_migration_coverage.ps1`
继续从 suite 和专项门提取语义覆盖。suite 临时生成的 `build/tmp` 源仍由原 suite 验证，
不以残留产物充当持久准入基线。后续仍必须维持 verifier 变异拒绝、严格生成 C 编译、
JIT/AOT、宿主 API 与运行时专项合同，而不是把旧文件删除本身视为语义验收完成。

## 2026-09-05 阶段性验证与未完成项（18:28 快照）

本节是有边界的执行记录，不表示总回归已经通过：

- 完整 `build_dll.bat` 重建通过，未出现编译警告；本轮最后一次编译器产物时间为
  2026-09-05 18:28:11（本地时间）。
- 全源文件审计：1,726 个源，1,233 个 legacy 可生成案例，其中 XLIR 接纳 1,198 个，
  35 个为预期拒绝负例，0 个非预期拒绝，审计期间编译器哈希保持稳定。
  这是生成 C 的准入证据，不是 1,726 个运行测试通过。默认准入另保持
  512 个持久候选、499/499 个双后端正例、0 个缺口、0 个执行异常。
- 384 个正例通过重复生成的字节一致性和严格 C 编译；两个 `xlir_string_bridge_bad_*`
  文件作为明确的错误签名负例验证诊断和无 C 产物，不计为成功生成。
- 上一轮核心回归 `03_core_base_02.bat`、`03_core_base_03.bat` 完整通过；这两项是
  16:25 及此前编译器的证据，不冒充本次构造器 ABI 调整后的整组重跑。
  相关动态 key 写入、IndexWriteAbi、callable container、function value ownership
  专用运行/差分门通过。第 3 组的 87 条 legacy C 拼写断言已迁移为 8 个具体验证合同。
- 独立 Base64 文本、HEX 字节、Base64 字节程序通过严格 C、XLIR/legacy `run/build`；
  两种 codec 合同各有三类错误签名拒绝用例。
- JSON 的 5 个值操作及 Writer/visitor 组件已使用 lowering/verifier 共用的声明式
  ABI 合同。Cgen 只消费已验证的操作/组件标识，新旧后端共用 `xl6_cgen_json_value`
  和 `xl6_cgen_json_stream` 适配器；值操作逐项裁剪，Writer 与 visitor 互不强制引入。
  streaming 使用公开 declaration SDK，不包含 XRT/xruntime 实现头。
  `check_xlir_json_value_bridge.ps1`、`check_xlir_json_stream_bridge.ps1` 已进入标准库套件，
  覆盖 10 个错误 ABI 拒绝用例、确定性、严格 C 独立链接及运行。
- JSON Writer 的共享别名、事件的保存/释放、可复用 visitor 已通过 weak-reference
  生命周期探针和双后端 `run/build`。临时 visitor 的捕获对象回收是独立
  `xlir_json_visit_capture_lifetime.xl` 合同：XLIR 通过，legacy 已实测仍保留捕获对象，
  因此明确列为 XLIR-only，而不是降低回收断言来迁就旧后端。
- native 构造器身份及 owner/native-field/destroy 返回 ABI 现在由声明和类布局建立，
  不再依赖是否扫描到某个构造调用。无调用点、无宿主实现的重载声明通过
  `check_xlir_native_factory_declaration_abi.ps1` 的 IR、裁剪、严格 C 和运行门；
  managed-native 生命周期及 extern receiver 的运行/差分门也已重新通过。
- XSON 的 5 个值入口、42 个 Writer 入口和 visitor 已进入同一序列化声明式 ABI 表。
  适配实现抽取为共享 `xl6_cgen_xson`，Cgen 依据验证后的操作集合裁剪值、Writer、
  visitor 及配置依赖，仅使用声明 SDK。两项专用门验证 10 个错误 ABI、Writer 全部
  操作、选项分支、custom/bytes/time 事件和独立严格 C 链接、双后端运行。
  仅使用 `XsonWriter`、`xson.writer()` 或 `xson.visit` 时缺少类依赖的问题已修复。
  visitor 构造失败时提前分配 key/index 的泄漏也已移除。
- XSON 临时回调捕获和 bytes payload 动态字段读取的回收保留为明确的 XLIR-only
  生命周期合同；legacy 对 owned 动态字段结果存在多余 retain。正常事件投递、
  事件保存/释放和 native Writer 生命周期仍作双后端对照。抛异常 visitor 尚未验收，
  不能把上述正常事件路径的通过等同于 callback error ABI 完成。
- 宿主入口依据已验证的初始化成功值和函数 error-result `ok` 处理语言执行结果：
  成功边界清理不代表语言异常的 XRT 诊断，失败保留异常，非零用户返回码不被覆盖。
  不在语言函数体内清理状态，原 XSON 示例未修改即可恢复。六个专用用例验证
  script/main/module-init、真实 throw 和非零返回的双后端 `run/build`。
  DLL module dependency lifetime 检查也通过。
- 覆盖登记更新为 99 个语义行、316 个代表用例、123 个双后端运行对照和
  30 个明确的 XLIR-only 运行合同；这些登记数量不代表完整批处理总回归已通过。

剩余工作继续执行，长期任务不得据此标记完成：

- 完成其余核心、类型、OOP/ABI、异常/并发和产物回归；仍需迁移的旧 C 输出形态断言
  必须由具体 IR 合同和运行检查接替，不能简单删除或把默认测试改回 legacy。
- `08_stdlib_extended.bat` 本轮已通过 JSON/XSON 值/流式门、原用例严格链接运行、
  xlang/C 示例输出对照和新增宿主错误边界门；当前推进到 `stdlib_print`，
  严格链接缺少 `xl6_xrt_write`、`xl6_xrt_writeln`。该套件仍以失败结束。
  write/writeln、encoding、crypto、process 等组继续逐组收齐显式 ABI 与运行组件。
  仅 `CallTarget.runtimeBridge=none` 的静态清单不能单独证明链接缺失，
  还需检查对应类型/资源组件是否已经提供实现。
- 回调异常传播探针 `build/tmp/xlir_serialization_callback_error_probe.xl` 已确认
  `xson.visit` 内 visitor 抛异常不能到达调用者的 try/catch：XLIR 失败、legacy 通过。
  当前 visitor bridge 的 no-raise 合同不足以表达该语义，必须在 IR/物理调用结果中
  区分正常解析失败状态和 callback exception，再由 Cgen 消费；不得恢复把任意
  XRT 诊断猜成未捕获语言异常的宿主行为，也不得在测试中补 clearError 掩盖。
- visitor 生命周期测试构造另确认：裸 `array.clear()` 尚无完整的 XLIR 成员操作合同，
  会在 lowering 报未解析目标；`array<var>.clear()` 已有 typed-collection 合同。
  后续应补齐 erased-array 成员语义，不能在 Cgen 根据 `clear` 拼写补生成逻辑。
- 此前 async task soak 和 WebSocket managed callback 可执行文件遭安全软件阻断的
  运行证明仍未补齐；不得计作通过，也不得通过绕过安全软件获得伪通过。

## 2026-09-05 序列化回调异常与状态机补齐（19:33 快照）

本节更新上节的 callback error 未完成项，不代表长期任务或完整总回归完成。

- JSON/XSON visitor 的语义 effect 改为 `may-raise`，物理错误 ABI 是明确的
  `result-carrier`（`xlir_result_c_int`）。正常解析失败、提前停止、完成继续返回状态码；
  回调 throw 或适配失败返回独立的失败标志，不把残留 XRT 诊断当作语言异常。
  lowering 和 verifier 使用同一合同，拒绝 visitor 的 no-raise、线程诊断观察及错误
  callback 参数/结果 ABI；Cgen 依照物理 ABI 发射调用和错误分支。
- `xl6_cgen_serialization_callback` 为 JSON/XSON 共用的边界组件，每次调用持有独立
  context，保存一个 owned callback error，经过原生解析器回退后恢复原始错误。
  调用隔离已有原生诊断；事件、回调返回值各释放一次。组件只依赖公开声明 SDK，
  同时使用 JSON/XSON 时也只发射一次，不引入 legacy 实现头。
- 错误的 12 类属性/类型测试，以及 raise/take/set-take/clear 所有权转交已由
  `xl6_ir_cgen_error_runtime` 共用实现；同步、async 和 generator 不再各自复制发射。
  typed catch 不匹配后的原错误转交、挂起前 catch、generator move 清空源所有权
  均已接入。字符串相等及四种排序也进入统一的共享发射路径。
- async/generator worker 的错误 effect 现在参与调用图固定点传播，覆盖通过普通
  标准库包装函数间接抛出的异常。worker effect 与 launcher effect 保持独立，
  generator 构造仍是惰性 no-raise，不执行函数体。
- 新增 `check_xlir_serialization_callback_error.ps1` 并接入标准库套件与覆盖登记。
  六个持久用例通过确定性、严格 C 独立链接运行和 XLIR `run/build`：同步与嵌套
  visitor、checked conversion、async 挂起前/恢复后捕获、typed catch 不匹配转交、
  generator 捕获后继续 yield、12 类错误属性、5 类字符串比较、事件/临时捕获回收，
  以及 JSON/XSON 未捕获异常的退出码和原始消息。状态失败不靠测试中的 clearError 掩盖。
- 同步及两个未捕获异常用例也通过 legacy `run/build`。其余三个是明确的 XLIR-only
  合同：legacy 多保留临时捕获；async 用例没有捕获到预期的两次异常；generator 用例
  以 `temporary scopes ended out of order`、退出码 3 失败。不修改断言迁就旧实现，
  也不把这三个用例登记成双后端运行通过。
- 最终完整 `build_dll.bat` 通过且无 warning/error；编译器产物时间为本地
  2026-09-05 19:33:16。`xlir_cgen.c` 底层 ABI 单元（含 callback 物理 ABI 变异拒绝）
  通过；390 个用例通过重复生成一致性和严格 C 编译。
- JSON/XSON 的值与流式四个既有专用门、六用例宿主错误边界门均在最后一次编译器上
  重跑通过。DLL module dependency lifetime 和使用正式 toolchain 环境的 threaded host
  API boundary 检查也通过。静态 IR 合同、覆盖登记、批处理接线、329 个 Markdown
  文件的链接检查通过；确认不存在比最后编译器更新的 src C/H 文件。
- 全源审计为 1,732 个源、1,239 个 legacy 可生成案例、1,204 个对应 XLIR 正例、
  35 个预期拒绝和 0 个非预期拒绝；编译器哈希稳定。默认准入保持 512 个持久候选、
  499/499 个双后端正例、0 个缺口。覆盖登记为 100 个语义行、322 个代表用例、
  126 个双后端运行对照和 33 个 XLIR-only 合同；登记数量不是完整总回归通过数。

剩余工作仍包括标准库 write/writeln 独立链接及后续分组、裸 array 成员合同、其余核心/
类型/OOP/ABI/异常并发与产物总回归，以及此前受安全软件阻断的运行证明。
不得将本节的生成 C 准入和专项运行结果扩大解释为这些工作已经完成。

## 2026-09-05 文本输出与擦除集合成员收口（20:52 快照）

本节更新上述历史快照中的 write/writeln 和裸集合成员缺口；长期任务仍未完成。

- `write/writeln` 具有独立的 `text-write/text-writeln` RuntimeBridge 身份，
  lowering 与 verifier 共用严格的 borrowed managed-string、void、no-raise、
  direct ABI 检查。共享 `xl6_cgen_text_output` 只依赖 stdio，保持原有 flush、
  空文本和换行行为；只用 write 或 writeln 时不强制引入另一个公开入口、
  value boxing、print formatter 或私有实现 SDK。
- 四个文本输出用例验证 UTF-8、空文本/默认参数、精确换行、返回/拼接临时字符串、
  closure capture、async/await 与 generator 输出；严格 C 独立链接和双后端
  `run/build` 通过。另有四个错误 ABI 用例验证参数类型、返回类型及参数数量的
  精确拒绝，不允许生成 C。质量门已接入标准库套件。
- 裸 `array/list/set/dict` 以及零参数的擦除泛型集合保留原有 TypeId、
  CollectionAbi 和 runtime TypeValueId，不改写成动态 `var` receiver。
  元素擦除通过 canonical fallback `var` 表达；`pop/take` 返回 owned 动态元素，
  `insert/contains/removeValue` 等在 lowering 中显式选择元素 boxing carrier。
  boxed item 不再被误标为零参数 receiver 的集合 TypeId。
- `xl6IrTypedCollectionSignatureMatches` 统一源操作数与物理 boxed 操作数两种形式，
  Typed admission、lowering、Typed verifier 和 lowered verifier 共用一张签名表。
  元素兼容、集合形状及 boxing TypeId 查询也共用；已删除相应的重复条件分支。
  Cgen 中集合成员的唯一发射器为 `xl6_ir_cgen_shared_collection`，同步、async 和
  generator 使用同一条路径，不从成员拼写、AST 或源表达式重新推导语义。
- 集合初始化的 type-bind/rebind、动态值字符串化、checked/wrapping 数值转换，
  以及 generator 的 discard/分配失败 trap 也接入已有共享合同和发射路径。
  同步和 async 中重复的 type-bind 发射已删除；新增
  `xl6_ir_cgen_scalar_conversion` 复用既有、显式的 source/target ABI。
- `check_xlir_erased_collection_members.ps1` 的五个用例通过确定性 Typed/lowered JSON、
  C 输出、严格 C 独立链接运行和 XLIR `run/build`。三个新用例还通过 legacy
  `run/build`：四类裸集合、四类擦除泛型集合、closure/async/generator 中的成员操作，
  包括挂起恢复后保留元素内容及 checked conversion 异常捕获。既有 typed-method
  用例继续验证全部 13 类 TypedCollectionOp，不把它改列为双后端合同。
- 生命周期用例保留严格 weak-reference 断言，验证 `pop/take` 转交、借用参数、
  临时 clear/insert receiver 和作用域结束后的回收。XLIR 全部通过；legacy 的
  `run/build` 均在 `temporary insert receiver survived the operation` 精确失败，
  因而整体明确登记为 XLIR-only，不删断言、不掩盖引用残留。
- 最终完整 `build_dll.bat` 无 warning/error，`xlc.dll` 为 10,532,352 字节，
  本地时间 2026-09-05 20:52:54。`xlir_cgen.c` 底层单元已重新编译并通过，
  包含错误 receiver/结果/boxed 元素/参数数量、篡改目标和操作数 arena 的拒绝。
- 最终编译器上重新执行并通过九项既有门：collection-member intrinsics、
  xvalue membership、collection type-rebind、integer conversion、checked float/time
  conversion、generator machine、text output、serialization callback error。
  DLL module dependency lifetime 和 threaded host API boundary 也重跑通过。
  静态 IR 合同、覆盖登记、批处理接线及 329 个 Markdown 文件的链接检查通过；
  确认没有比该编译器更新的 src C/H 文件。
- 398 个正例通过重复生成字节一致性及严格 C 编译。全源准入审计为 1,744 个源、
  1,251 个 legacy 可生成案例、其中 1,212 个 XLIR 正例、39 个预期拒绝、
  0 个非预期拒绝；审计期间编译器哈希稳定。默认准入维持 512 个持久候选、
  499/499 个双后端正例、0 个缺口、0 个执行异常，哈希稳定。
  这些是准入/编译证据，不等于全部源文件都已执行通过。
- 覆盖登记为 102 个语义行、330 个代表用例、133 个双后端运行对照和
  34 个明确的 XLIR-only 合同。集合质量门已接入语言前端套件，并检查唯一签名表、
  唯一共享成员发射器、错误 ABI 单元与运行测试的接线。

剩余工作继续推进：

- 本轮 `08_stdlib_extended.bat` 已通过文本输出及此前 JSON/XSON 和回调异常专项，
  推进至 `stdlib_error_paths`，其严格链接仍缺少 `xl6_xrt_dir_list`、
  `xl6_xrt_process_exec`、`xl6_xrt_process_success`、`xl6_xrt_process_exit_code`。
  现有 `xl6_cgen_dir.c` 和 `xl6_cgen_process*.c` 仍是 legacy 侧组件，需要建立
  显式桥接 ABI 与共享发射并补齐独立运行门；不能只删掉链接检查。
  在最终 20:52 编译器上通过 `xl build stdlib_error_paths.xl --link cgen_class_host.c`
  再次复现了 `xl6_xrt_process_exit_code` 未解析，标准库整组仍未验收通过。
- 四类集合的源语言成员可用性仍由语义规则决定。本轮没有扩张擦除集合的 API，
  也没有放宽 generator 参数稳定复制/释放的前端限制；生成器测试使用跨 yield 的
  本地裸集合，其 frame/元素回收已验证。
- 其余核心、类型、OOP/ABI、异常并发、标准库后续分组及产物完整回归仍须执行；
  此前受安全软件阻断的 soak/WebSocket 运行证明仍未补齐。不得据专项通过标记
  长期任务完成，也不得通过关闭安全软件或恢复旧后端规避验收。

## 2026-09-06 目录与同步进程桥接收口（23:47 编译器快照）

本节更新 20:52 历史快照中目录与同步进程的链接缺口。编译器产物时间为
2026-09-05 23:47:08，验收跨入 2026-09-06；长期任务仍未完成。

- `xl6_ir_direct_bridge` 建立单一的直接、no-raise 桥接合同表，lowering 与 verifier
  共用。除原有 write/writeln 外，新增六类目录操作（list/glob/entries/is-empty/
  count/size）及三类同步进程操作（exec/success/exit-code）；参数宽度、借用字符串、
  返回类型、direct 物理 ABI 和效果均精确验证。错误声明在生成 C 前拒绝，
  不允许退回普通 extern C 路径。Cgen 仅按已验证 RuntimeBridge 身份选择组件。
- `xl6_cgen_dir` 从 legacy 私有包含移至共享运行时，按操作集合裁剪公开入口及
  遍历/元数据辅助实现。目录遍历失败丢弃部分结果并返回空数组，不再返回需要失败
  checked cast 的 null carrier；保留 XRT 原生错误，不通过 clearError 掩盖失败。
- 新的共享 `xl6_cgen_process_run` 仅实现同步 shell 执行及结果投影，不引入 legacy
  live-process/pipeline/回放实现。stdout/stderr 结果在成功、失败及超时路径均经
  `xrtProcessResultUnit` 回收。exec 返回原始拥有型分配，由语言包装层显式转交
  take-cstr；不把原始分配误当成 managed xstr。两后端的三个旧重复入口已合并。
- 集合 count 与原始 array-at-i64 的验证集中到一个 IR 合同，共享集合发射器服务
  同步、async 和 generator；xvalue retain 的 TypeId 合同及发射也合并到共享路径。
  删除原有同步/async 重复分支，跨 await/yield 的目录结果保留及读取不再增加
  generator 专用补丁。
- 新增 `check_xlir_system_bridges.ps1` 并接入标准库套件与覆盖矩阵。11 个正例通过
  精确 lowered JSON、操作级隔离、确定性 C、严格 C 独立链接运行及 XLIR run/build；
  覆盖递归、筛选、目录元数据、挂起恢复后的集合/文本生命周期。三个进程宿主还
  注入带已分配 stdout/stderr 的失败与超时，实际检查配置、结果投影及析构次数。
  六个错误 ABI 源用例验证参数、结果、宽度、bool、集合及参数数量的拒绝。
- 其中八个正例通过双后端 run/build；三个独立 empty/count/size 用例明确登记为
  XLIR-only，legacy run/build 仍分别遗漏其公开符号。完整目录用例覆盖这三种操作
  并通过双后端运行，不把独立 legacy 链接失败计作运行对照成功。
- `stdlib_error_paths` 已通过严格 C 独立链接与运行，stdout/stderr 均为零字节。
  套件的旧 extern-only 检查已更新为共享目录实现的精确签名；原有进程源代码
  拼写检查由上述析构探针取代，未删去实际运行与错误路径检查。
- 最终完整 DLL 构建无 warning/error，`xlc.dll` 为 10,540,032 字节，SHA-256 为
  `11D4CC4AEA1265A4BC087BA82B4CDF127333CA5FCBF7C4A5846A68338D0027B0`。
  23:37 与 23:47 两次构建字节一致；最后编译器上重新编译执行 `xlir_cgen.c`
  ABI 单元通过，包含桥接变异与集合读取 arena/类型/载荷拒绝。
- 同一编译器内容上，system bridges、text output、erased collection members、
  collection type-rebind 和 generator machine 五项运行门均通过；409 个正例通过
  重复生成一致性及严格 C 编译。全源审计为 1,761 个源、1,268 个 legacy 可生成
  案例、其中 1,223 个 XLIR 正例、45 个预期拒绝及 0 个非预期拒绝，编译器哈希稳定。
  默认准入为 512 个持久候选、499/499 个双后端可生成正例、0 个缺口、0 个执行异常，
  哈希稳定。这些准入数字不是全部语言用例的运行通过数。
- 使用正式 toolchain 环境重跑 threaded host API boundary 与 DLL module dependency
  lifetime，两项均通过；目录和 continuation 用例创建的专属测试目录已自行清理。
- 覆盖登记为 103 个语义行、341 个代表用例、141 个双后端运行合同及 37 个
  XLIR-only 合同；实际确定性/严格 C 集合为 409 个用例，不能混同为整仓覆盖率。
- 静态 IR 合同、覆盖登记、批处理接线、核心 C fixture 接线（419 个入口、11 个
  包用例）及 329 个 Markdown 文件的链接检查通过；src C/H 均不晚于最终编译器。
  全仓质量门并非全部通过：source wiring 仍报告 `xl6_cgen_tcc.c` 兼有直接编译/
  包含身份；tool wiring 报告 `check_extlib_serial.ps1` 未登记；非 CoreOnly 的
  C fixture wiring 报告 mqtt/serial 的七个包测试未登记。根测试还受扩展库四篇
  状态文档未纳入文档索引/状态矩阵阻断。本轮没有改动这些关联区域或放宽质量门。

剩余工作与已推进边界：

- `08_stdlib_extended.bat` 最后一次从头复测已通过 JSON/XSON、回调异常、文本输出、
  系统桥接、`stdlib_error_paths` 及 `stdlib_codec`，后者的语言/C 示例输出均为
  `codec=ok`。随后在 `stdlib_encoding` 的严格 C 链接失败；整组尚未通过，
  后续分组不能计作本轮已执行。
- encoding 缺失的 11 个入口是 `xl6_xrt_encoding_` 前缀下的 char_size、is_utf8_text、
  is_utf8_bytes、detect_text、detect_bytes、to_bytes、from_bytes、convert_bytes、name、
  swap_utf16、swap_utf32。其实现仍仅在 legacy `xl6_cgen_encoding` 中发射，下一步需
  建立字符串/Buffer 原生句柄的精确 IR 合同、共享组件及独立运行门；不移除严格链接。
  最终整组日志为 `build/tmp/xlir_system_stdlib_suite_final.log`。
- 此前独立探测还发现 `string.trim` 的 `xl6_xrt_string_trim` 缺少链接实现。本轮进程
  测试使用精确换行断言，没有绕过或宣称修复该字符串缺口。
- 其余核心、类型、OOP/ABI、异常并发、标准库后续分组与产物总回归，以及此前受
  安全软件阻断的 soak/WebSocket 运行证明仍待完成，不据本轮专项标记长期任务完成。

## 2026-09-06 核心表达式与公开 ABI 校正

按用户最新范围，将标准库完整迁移移出语言验收的前置条件。本轮从核心运行组
开始，保留原有源程序和结果断言，发现并处理以下真实问题：

- 动态时间排序与近似比较曾把 XRT 标签 `5` 当作 time；当前它是 bytes，time
  为 `6`。动态比较与共享数值访问改用公开 `XVALUE_*` 标签，避免再次维护数值镜像。
- 纯整数动态排序不再先转成 double；i64/u64 各自精确比较，混合有符号/无符号
  显式处理负数。存在浮点操作数时保持浮点域比较；NaN 的等值和四种排序均为假，
  包括两个 var 指向同一个 boxed NaN 的情况。
- `math.nan` / `math.inf` 曾发射为 `strtod("NAN"/"INFINITY")`，Windows TCC
  实际运行链得到 `0.0`，原生 C 宿主单独通过不足以验收。现在由明确的特殊 f64
  RuntimeOp 选择 binary64 位模式，以 memcpy 构造并静态验证浮点宽度/格式；
  同步、async 和 generator 使用共享发射入口，不依赖 CRT 文本解析。
- 动态比较的六种 RuntimeOp 共享 IR 合同与发射，验证精确 managed xvalue 借用、
  bool 结果及无多余载荷；删除同步/async 的重复分支。新增畸形 IR 单元，涵盖
  操作数范围、类型、所有权、null ABI、结果和附加载荷变异。
- 所有运行时依赖分析在发射声明之前完成。xcall frame/result、union bytes 视图、
  Template 配置以及集合合并策略使用公开声明/枚举，不重复定义结构或把枚举参数
  手写成 int；仍然只包含声明头，不嵌入完整 XRT 实现。
- 核心第四组从旧 C 局部变量/辅助函数拼写检查迁为明确 IR 合同、严格 C、CLI
  run/build 及执行断言。原有 14 个源用例、入口输出/退出码和专用字符串宿主保留。
  动态比较和特殊浮点门同时纳入该核心组，特殊浮点门新增 CLI run/build。
- 之前 source wiring 的 TCC“直接编译/包含双重身份”是假阳性：标准库批处理
  直接搜索 src C 文件被接线器视为编译入口。两项精确注册签名/调用检查已集中到
  DLL API 边界门并继续调用；未添加接线豁免。

后续核心项仍包括第五组起的旧生成代码检查迁移、完整类型/OOP/异常并发/产物
回归与长跑证据。generator 的 var 形参当前被语义层以 frame 安全性拒绝，属于
语言层 admission/所有权合同的待审计项，不能算作标准库延期；本轮 generator
正例验证受支持的标量形参装箱后跨 yield 存活与比较，没有伪装支持 var 形参。

标准库延期清单继续保留 encoding 11 入口与非必要 string.trim。另一次独立
Template CLI 探测仍报 `xl6_xrt_template_wrap_handle` 未链接：其专属包装实现只在
legacy Template 组件中，不能因本轮消除重复声明而将完整 Template API 标为通过。
若它成为核心语言回归的必要依赖，再按精确合同补齐最小实现。

本轮最终验证快照（2026-09-06 01:46 编译器）：

- 完整 `build_dll.bat` 无 warning/error；`xlc.dll` 为 10,541,056 字节，SHA-256 为
  `B6F3CDD836A0FE67C8F49E11AA3EFC09297441A06E38363D35F6929712A20EA9`，
  src C/H 均不晚于该编译器。构建日志为
  `build/tmp/xlir_dynamic_compare_build_collection_20260906.log`。
- 正式核心总入口上，第 1—4 组均通过，包含动态比较畸形 IR 单元、严格 C、
  特殊浮点 CLI run/build、类型/索引/字符串/入口合同与实际运行。总入口在第 5 组
  `cgen_auto_inference_matrix.c` 的旧 `long long __xl6_user_global_literalInt;`
  拼写检查处停止；第 5—10 组不能记作整组通过。日志为
  `build/tmp/xlir_language_core_final_20260906.log`。
- 同一编译器独立执行第五组七个原有源程序：auto inference、var/scalar call
  arguments、var return、expression discard、array expression elements、var values，
  全部通过 CLI run、build 和产物运行。递归 var 基准 `array 18` 返回 2584，
  `dict 23` 返回 28657 且满足原有 10 秒限制；没有改断言、增加时间上限或把程序
  改成预期拒绝。这不替代第五组尚待迁移的完整 IR/严格 C/生命周期验证。
- 412 个用例通过重复生成一致性与 `-Werror` 严格 C 编译；新增的两个既有 Cgen
  候选覆盖对象/函数索引赋值和集合运算的公开声明组合。日志为
  `build/tmp/xlir_language_determinism_final_20260906.log`。
- 默认准入为 512 个候选、499/499 个双后端可生成正例、0 个缺口和执行异常；
  全源审计为 1,762 个源、1,269 个 legacy 正例、1,224 个 XLIR 正例、45 个预期
  拒绝和 0 个非预期拒绝。报告分别为
  `build/tmp/xlir_language_default_final_20260906.json` 与
  `build/tmp/xlir_language_audit_final_20260906.json`；这些数字不是全部程序运行通过数。
- IR 静态合同、104 行/342 代表用例覆盖登记、源码接线（283 个源文件）、80 个
  DLL 公共 API 边界、批处理接线、核心 C fixture 接线（420 入口/11 包用例）以及
  329 个 Markdown 链接检查通过。覆盖登记含 141 个双后端运行合同和 38 个
  XLIR-only 合同，不代表本轮重新执行了全部历史运行门。
- 全仓总质量门未宣告通过：扩展库 serial 工具接线、mqtt/serial 包测试接线及
  扩展库文档索引的既有问题仍单列；完整语言余项和通用生命周期/并发长跑仍待完成。
  长期任务保持 active，本轮不以核心专项通过代替最终结项。

## 语言加固续批：部分求值所有权与核心 5/6 组（2026-09-06）

本批按“标准库最低基线、语言正确性不降级”继续推进，发现并修复了实际生命周期
问题，没有在 Cgen 追加源语义判断：

- 局部 `auto left, right = pair()` 在正常块退出时原本无法 lowering；其两个字段
  也没有进入 return/throw 的词法清理。新增共享 `XL6_IrDeclarationBinding`
  抽象，标量与 product 声明提供同一种有序绑定视图。Typed 清理计划与 Lowered
  正常清理共同消费它，逐字段逆序释放，`_` 仍是丢弃字段而非虚构的局部所有者。
  producer 初始化失败时，尚未取得的接收字段不参与清理。
- `return make(), fail()` 的第一个已产生受管值会在第二个字段抛错时泄漏；
  `first(make(), fail())` 也有同类实参泄漏。Lowering 新增统一的部分求值所有者
  栈，将存活值表示为明确的临时 storage。后续求值失败时逆序 RELEASE；成功消费
  product/owned 实参前明确 MOVE。借用实参的正常所有者仍由调用后清理释放。
- 同一机制接入普通调用、闭包调用、动态调用、直接 receiver 和 C 可变尾实参。
  动态调用的 callable 临时所有者跨调用保持存活，而已交给调用框架的实参不在
  callee 失败边上重复释放。异常目标深度用于区别“离开该求值”与“内部处理后继续”。
- 新增 `xlir_core_call_value_lifetime.xl`：20 轮、460 次创建/析构精确相等，覆盖
  直接/命名动态调用顺序、product 字段、discard/条件/合并表达式、提前 return、
  throw/catch/finally、第二字段失败、直接/闭包/动态实参失败及嵌套调用部分求值。
  已通过 CLI run、build/产物运行、严格 C 宿主运行，并检查临时 storage 的 MOVE、
  失败 RELEASE 及调用移交后无重复清理。登记为 XLIR-only 运行合同，不冒称 legacy
  生命周期等价。绑定单元包含标量、product、`_`、未解析名称、字段数量/类型、越界
  children 和空 arena 负例，并在 Cgen 单元中实际执行。
- 核心第五组保留原有八个源程序、两个专用 C 宿主和递归性能工作量；替换约千行
  旧 C 拼写断言，改验推导 storage TypeId、调用 frame/错误边、结构化 product、
  投影 retain-before-release、显式 CFG 与 RuntimeOp。七个一般程序通过严格 C、
  CLI run/build，递归 `array 18=2584`、`dict 23=28657` 均满足原有 10 秒上限。
- 核心第六组复用并补强已有 runtime-callable 合同：7 个动态调用、2/5 字段
  product、具体 callable TypeValueId、名义 class 身份、native handle TAKE、
  默认/可变参数与 owned-first 结果策略。原有程序与 C 宿主保留，严格 C、CLI
  run/build、既有 legacy 差分通过。

本批验证使用同一份 2026-09-06 02:24:46 编译器：完整 `build_dll.bat` 无
warning/error，`xlc.dll` 为 10,545,664 字节，SHA-256 为
`B45D4B4915A7A7910BA16E62BEF9FF90460194D105E76CE52D28A65B54E70DBE`，
src C/H 均不晚于它。构建日志：`build/tmp/xlir_core05_callable_evaluation_build.log`。

- 第五组独立整组通过，第六组独立整组通过；日志分别为
  `build/tmp/xlir_core05_current.log`、`build/tmp/xlir_core06_current.log`。
- 同一编译器从正式核心总入口复跑，第 1—6 组全部通过，包含绑定畸形 IR 单元、
  新增生命周期门及所有保留的原始运行断言；在第 7 组旧
  `cgen_runtime_type_baseline.c missing boxed class runtime type query` 拼写检查
  停止。日志为 `build/tmp/xlir_core_evaluation_final.log`，不能将 7—10 组记为通过。
- 413 个用例通过确定性与严格 C 编译：`build/tmp/xlir_evaluation_determinism.log`。
- 默认准入仍为 512 候选、499/499 双后端正例、0 缺口/执行异常，编译器 hash
  稳定：`build/tmp/xlir_evaluation_default.json`。
- 全源审计为 1,763 源、1,270 legacy 正例、1,225 XLIR 正例、45 预期拒绝、
  0 非预期拒绝，hash 稳定：`build/tmp/xlir_evaluation_audit.json`。这是生成准入
  统计，不是 1,763 个程序全部运行通过。
- 覆盖登记为 105 行、343 代表用例、141 双后端运行合同与 39 XLIR-only 合同。
  静态 IR 合同、283 个源码接线、批处理接线和核心 C fixture 接线通过。

本批不代表所有部分求值路径已闭合：构造/协议调用、其它多操作数表达式、解构后
捕获，以及跨 await/yield 的临时所有权仍须逐类验证；第七组起、完整语言功能及
长跑质量门继续推进。标准库与扩展库延期项不因此重新升级为前置工作。
全仓接线检查仍列出既有 serial 工具、mqtt/serial C fixtures，以及一组标准库桥接
负例的接线识别问题；它们没有被删除或添加豁免来制造全绿。长期任务保持 active。

## 语言加固续批：拥有型读取、闭包重赋值与核心第七组（2026-09-06）

本批继续执行“标准库最低可用、语言语义压实”的范围，修复三个实际问题：

- `DynamicFields` 的 native factory 曾被 lowering 改名为公开
  `xrtDynamicFieldsCreate`，并按 `void*` 结果声明，和公开声明头中的
  `xrtdynamicfields*` 冲突。现在创建、销毁、读、写共享同一精确桥接契约，
  保留私有适配器名称，创建结果具有 native field TAKE 与销毁符号；Cgen
  引用公开声明头并机械生成适配器，不再复制或覆盖 XRT 原型。
- getter 的源语法可以是可赋值成员，但其调用返回值不是字段借用。Typed XLIR
  现在统一规范化并验证 managed 属性/协议 getter 和集合索引读取的 owned
  rvalue/MOVE 契约。实体字段与赋值目标保持独立的 storage 契约，循环取值
  不再根据 PROPERTY_GET 源语法补判临时所有权。畸形 IR 单元覆盖成员/索引、
  property/protocol、xvalue/string/closure 及错误 borrow/lvalue/未规划标记。
- 普通闭包重赋值曾只生成指针 STORE，遗漏旧值 RELEASE 和借用新值 RETAIN。
  声明、直接赋值、静态字段赋值、cell 输入与空值合并赋值现共享
  `xl6IrLowerOwnedStorageValue`；先取得新 owner，再释放旧 owner，最终 STORE。
  这些是 Lowered IR 指令，不增加 Cgen 对源表达式的回溯判断。

新增两个执行合同，并接入核心第七组：

- `xlir_open_object_field_lifetime.xl`：100 轮，2,400 次创建/析构严格相等；
  包含动态字段保存/替换、属性与索引 getter 的保存/查询/discard，以及正常
  退出、提前 return、throw/catch。保留名义类型、声明字段和存活 owner 断言。
- `xlir_refcounted_storage_assignment_lifetime.xl`：100 轮，1,100 次创建/析构
  严格相等；覆盖局部、全局、静态字段的 owned/borrowed 替换、别名、自赋值及
  三种退出路径。IR 门检查 12 次闭包赋值的 acquire-before-release-before-store。
- 两者均通过 CLI run、build/产物运行及严格 C 宿主运行；它们是 XLIR-only
  生命周期合同，不把 legacy 的生成成功等同为资源语义正确。
- 第七组保留全部原有十个正例、一个运行时错误用例和三个语义错误用例；
  旧 C 符号拼写检查改为 TypeId/桥接/转换 ERROR_TEST/闭包 capture ABI 检查，
  复用已有 cast、named-function、open-function 的严格 C 与运行差分门。

本批最终完整构建日志为 `build/tmp/xlir_core7_storage_ownership_build.log`，
无 warning/error。2026-09-06 03:08:33 的 `xlc.dll` 为 10,545,152 字节，
SHA-256 为 `43A6032C7F6B73749913EA962ED07E4DA546313B76DC6466F7355CC7DEC05F19`。

- 第七组独立整组通过：`build/tmp/xlir_core7_current.log`。
- 正式核心总入口从头复跑，第 1—7 组全部通过，包含新增的畸形 IR 单元与两项
  生命周期合同；第 8 组在原有 `missing extern function typed wrapper` C 拼写
  检查停止，不能将第 8—10 组记为通过：`build/tmp/xlir_core7_full_core.log`。
- 416 个用例通过确定性与严格 C 编译，包含补入的原始运行时类型基线：
  `build/tmp/xlir_core7_determinism.log`。
- 默认准入为 514 候选、500/500 双后端正例、0 缺口、0 执行异常，hash 稳定；
  对应最低数量门同步提高到 514/500/500：`build/tmp/xlir_core7_default.json`。
- 全源审计为 1,765 源、1,271 legacy 生成正例，其中 1,226 获 XLIR 接纳、
  45 项维持预期拒绝、0 非预期拒绝，hash 稳定：`build/tmp/xlir_core7_audit.json`。
  这不是 1,765 个程序的运行通过率，也不包括 legacy 未接纳的 XLIR-only 合同。
- 覆盖登记为 107 行、345 代表用例、141 双后端运行合同、41 XLIR-only 合同；
  静态 IR 合同、283 源码接线、批处理接线及核心 C fixture 接线通过。

剩余语言工作仍包括第八组起的核心验收、构造/协议调用等部分求值路径、
解构后捕获和跨 await/yield 的临时所有权。命名函数值的创建/借用归类及模块
最终清理也需继续核查，不能仅凭无捕获闭包的数值结果正确就认定无泄漏。
标准库与扩展库的既有延期项保持单独登记，长期任务保持 active。

## 语言加固续批：命名函数与复制字段的所有权（2026-09-06，验收被隔离中断）

本批已实施以下语言层修复，但完整回归尚未完成：

- 命名函数适配器实际创建一个新闭包，不能以 NAME 的语法形状认定其为借用。
  Typed XLIR 为其统一规划 rvalue/MOVE；被直接丢弃、前端未填写表达式类型的
  函数名，也从已经解析的签名获得完整 TypeId。存储与返回边界只消费所有权
  契约，不再以 NAME 特判附加 RETAIN。
- `xcall` 的 BORROW 操作数可以是新建的命名函数适配器。它现在复用调用借用
  保活与部分求值所有者机制，在成功及失败边分别释放临时 closure，同时保持
  runtime-call 后紧邻 ERROR_TEST。Cgen 无需追溯它是名称、存储还是新对象。
- 会复制字符串、创建 closure 或取得 error 引用的成员 ABI，在刷新成员合同
  时同步规范化拥有型读取。包含隐式字段 NAME；临时接收者在拥有型字段结果
  取得独立所有权后释放。普通 xvalue/原生指针的借用投影仍有独立合同，不能
  据此认定所有字段、逃逸与临时接收者路径均已验收。
- 原始 `cgen_function_null_compare.xl` 暴露默认空函数字段读取会 abort。
  函数字段 GET/SET 的物理合同现在明确包含空 closure，GET 空值返回 NULL，
  SET 空值保存 xvalue null；Cgen 不再从源类型是否 nullable 推断该行为。
  源码层的可空类型约束与物理空函数存储不是同一概念。

验证与实际证据：

- 早期命名函数探针在修复前创建 4,200 个适配器，释放 0 个，剩余 4,200 个：
  `build/tmp/xlir_named_callable_before.log`。后续测试增加了 discard、可空提升、
  xcall、临时接收者和隐式字段覆盖，不能把两次分配数当作同一工作量比较。
- 增强后的 `xlir_named_callable_owner_lifetime.xl` 在最新完整构建上通过严格 C
  分配跟踪：200 轮正常/return/throw 路径，7,200 次生成代码分配全部释放。
  同一宿主运行原始空函数字段比较的真实 user main，12 次分配全部释放。
  跟踪仅覆盖生成代码的分配入口，不冒称统计了 XRT 内部所有分配。
- 第八组保留原始 13 个正例、零步长运行错误、专用动态比较宿主及类返回程序
  的额外三次重复运行。新增命名函数生命周期用例、公开导出头的严格 C 检查、
  两项分配跟踪。私有 class 的物理布局在 IR 验证；公开 C 头按导出 ABI 生成，
  不再要求它泄露未导出 class 的旧 C struct。
- 第八组的 callable/control、named-function adapter、callable-field frame、
  CallSite、range-for 五项合同门通过；包括既有严格 C、运行差分和零步长 CFG。
  第八组整体 **未通过完整验收**：后续 CLI 生成阶段被安全软件中断。
- 源码及新增畸形 IR 单元通过 `gcc -fsyntax-only -Werror`；本批没有完成重新
  执行整个畸形 IR 单元程序。静态 IR 合同、108 行/346 代表用例的覆盖登记、
  283 源码接线、批处理接线和 421+11 个核心 C fixture 接线通过。

当前环境阻塞（本批首次确认，2026-09-06 03:46）：

- `build/tmp/xlir_core8_xcall_owner_build.log` 记录完整构建成功，最新 `xlc.dll`
  写入时间为 03:43:35；Defender 在 03:45:48 检测它为
  `Trojan:Win32/Bearfoos.A!ml`，03:46:03 以事件 1117 成功隔离
  `D:\GIT\x-lang\demo6\release\xlc.dll`。当时文件不可读并消失，不能未经核验断言误报。
- 核心第八组、确定性门、默认准入和全源审计四个进程均已终止，exit 1；不是
  仍在等待的进程，也不能把安全软件造成的不可执行错误算作语言拒绝或测试通过。
  日志为 `build/tmp/xlir_core8_current.log`、`xlir_core8_determinism.log`、
  `xlir_core8_default.log`、`xlir_core8_audit.log`。
- 当前没有可信的完整新审计数字或最终 hash 稳定证明；上一批 416 用例、
  核心 1—7 组和 514/500/500 准入结果仍只是上一编译器快照，不能替代本批。
- 未恢复隔离项、添加排除项、关闭防护或改名绕过检测。需要用户核验并决定
  隔离项如何处理；之后复跑本批完整核心、确定性、默认准入与全源审计。
- 结束前只读复核时，同一路径又可读取，时间戳仍为 03:43:35，大小为
  10,545,664 字节，SHA-256 为
  `986B00B6AF3F6840C26EB96480F99FCC9FCD83EA200492EB0F1941CE3032E424`。
  本轮未进行任何恢复操作，也尚未确认是否由用户核验恢复；未再次执行该 DLL。
  文件恢复可读不等于安全结论，也不能把中断的回归补记为通过。

连续三个目标回合确认同一安全验收阻塞后，长期任务标记为 blocked，等待用户
确认构建文件的核验及恢复来源。只读复核的 DLL 时间戳、大小及 SHA-256 均未变化；
未再次加载 DLL，也未恢复隔离项或调整防护。目标范围不变，不能据此宣告完成。
剩余语言工作包括第八组完成验收、第九组起、
全部部分求值/捕获/await/yield 生命周期、模块最终清理及最终质量门；最低标准库
策略不变。对默认空函数进行调用时的错误行为也须继续专门验证，不能由空值比较
成功推断调用路径已完成。

## 用户调整防护后恢复验收（2026-09-06）

用户说明已尝试手动关闭 Defender，并明确要求继续工作。只读复核确认
`RealTimeProtectionEnabled=false`、`BehaviorMonitorEnabled=false`、
`IoavProtectionEnabled=false`，Defender 服务与篡改防护仍开启。本轮未执行
额外关闭、隔离恢复、排除项添加或防护绕过；这些状态只描述本轮检查时刻，
不构成永久关闭或此前检测为误报的结论。此前 blocked 记录保留为历史，
本轮依据用户明确要求恢复运行验收。

使用同一份 03:43:35 编译器，DLL SHA-256 仍为
`986B00B6AF3F6840C26EB96480F99FCC9FCD83EA200492EB0F1941CE3032E424`。
本轮实际结果（不沿用中断前的未完成结果）：

- 第八组独立完整通过；14 个正例的严格 C、公开头编译、CLI run/build、
  类返回重复执行、动态比较专用宿主与零步长错误断言均执行。
  分配宿主再次确认 7,200 次及 12 次分配全部释放。
  日志：`build/tmp/xlir_core8_resumed.log`。
- 完整核心回归重新通过第 1—8 组。第四组重新编译并执行 XLIR C emitter
  单元，包含本批畸形命名函数、拥有型字段读取的验证器测试。
  聚合在第九组旧 C 文本断言 `static int xl6_range_contains_i64` 停止，
  当前产物使用 `static bool xlir_i64_range_contains`；不能将此算作整个
  核心回归通过，也不应为了满足旧名称重新引入旧 Cgen。
  日志：`build/tmp/xlir_core8_resumed_full_core.log`。
  独立 range membership 门随后通过，覆盖四 i64/bool ABI、正负及动态零
  步长、溢出安全距离、原始用例组合、严格 C 和运行语义；日志：
  `build/tmp/xlir_core8_resumed_range_membership.log`。
- 417 个 fixture 的生成 C 确定性和严格 C 编译全部通过。
  日志：`build/tmp/xlir_core8_resumed_determinism.log`。
- 默认准入：515 候选、500/500 双后端正例、0 缺口、0 执行异常，编译器
  hash 稳定。新增命名函数生命周期用例由 XLIR 支持、legacy 拒绝，不能
  将其虚记为新增双后端成功项；最低候选数量门提高到 515，双后端门保持 500。
  报告：`build/tmp/xlir_core8_resumed_default.json`。
- 全源审计：1,766 源文件、legacy 接受 1,271、其中 XLIR 接受 1,226、
  45 个登记的预期拒绝、0 非预期拒绝，编译器 hash 稳定。
  报告：`build/tmp/xlir_core8_resumed_audit.json`。该统计仅衡量旧后端
  接受集合内的迁移差距，不代表全部语言语义已通过运行验收。

环境等待已解除，但长期目标仍未完成：继续处理第九组起的验收迁移与实际
语言缺陷、空函数调用错误行为、部分求值/捕获/await/yield 生命周期和模块
最终清理。标准库仍按最低标准处理，不降低语言本身的正确性要求。

## 语言加固续批：成员查询求值事务与第九组（2026-09-06）

本批在第九组迁移中发现并修复实际语言问题，而非只修改旧 C 文本检查：

- 最小复现 `make() in failingValues()` 的右侧抛错后，左侧创建的对象泄漏；
  初版失败日志为 `build/tmp/xlir_core9_membership_before.log`。
- 字符串 `in` 原先按运行时形参顺序先计算 haystack，违反源码从左到右的
  求值顺序；集合 `.contains()` / `.removeValue()` 复用 infix 路径时原先
  先算实参、后算接收者。现在降低层先完成源码顺序求值，再绑定运行时实参。
- 已求值操作数以精确 TypeId 的临时存储参与既有 evaluation-owner 机制；
  后续求值失败通过显式错误 CFG 释放，正常消费借用后由
  `xl6IrLowerReleaseEvaluationBorrows` 逆序释放。命名字符串先复制、借用
  集合接收者先 retain，防止后续求值替换其原存储导致悬空引用。
- 中间实现曾漏掉借用集合接收者的 retain，完整核心回归在
  `cgen_container_ops` 的 `indexOf` 断言抓住提前释放。最终实现显式 RETAIN，
  新增连续 `contains/indexOf/removeValue` 与原地修改断言防止复发；中间构建
  不作为验收基线。生产改动仅在 IR 降低层，未向 Cgen 增加源语义推断。

新增 `test/cases/xlir_membership_operand_lifetime.xl` 与
`tools/check_xlir_membership_evaluation_lifetime.ps1`，覆盖：

- infix、字符串查询、contains、removeValue 的求值顺序、单次求值；
- 四种后续求值抛错、准确的错误消息、五条显式失败清理边；
- 接收者字段被后续调用替换、可变捕获字符串被后续调用替换；
- 200 轮共 2,400 个语言对象创建/析构平衡；IR 中精确临时存储、三处借用
  接收者 retain、正常路径逆序释放，以及重复 IR/C 输出确定性。

分配宿主复用既有测试文件，移除不必要的全量 XRT 声明预包含，避免与独立
生成模块的类型声明重复。成员查询模式额外拦截 `xrtStrDup`、`xrtStrDupN`、
`xrtStrConcat` 的拥有型结果：仅拦截生成代码的 `xrtMalloc` 会漏计 XRT 内部
分配的字符串。最终实测 **5,600 次分配全部释放**，数量门同步设为 5,600；
它与 2,400 个语言对象计数是两种不同观测，不能混为同一分配总量。

按用户明确要求，第九组标准库使用最低标准：math 原完整断言、示例 C/xlang
输出对照和裁剪产物仍执行；已实现的 string bridge 严格 C、分配失败、JIT/AOT
及双语示例门仍执行。下列旧扩展 API 用例保留 check/Cgen/严格 C 语法验证，
其运行验收与 string/time 示例对照延期，**不计为运行通过**：

| 原用例 | 当前直接观察到的缺失包装符号示例 |
| --- | --- |
| `stdlib_string` | `xl6_xrt_string_bytes` |
| `stdlib_time` | `xl6_xrt_time_format_owned` |
| `stdlib_time_extra` | `xl6_xrt_time_relative` |
| `stdlib_time_calendar` | `xl6_xrt_time_week_of_month` |
| `stdlib_time_runtime` | `xl6_xrt_time_relative` |
| `stdlib_random` | `xl6_xrt_rng_string` |

设置 `XL6_EXTENDED_STDLIB_RUNTIME=1` 再运行第九组可显式复跑这些已知缺口。
这是标准库范围调整，不是 Cgen fallback；所有语言用例仍强制执行严格 C、
CLI run、CLI build/产物运行，非容器迭代的 TypeError 也必须通过运行验证。

最终完整构建 `build/tmp/xlir_core9_receiver_retain_build.log` 成功，编译器
时间戳 **2026-09-06 04:34:25**，`xlc.dll` 为 **10,546,688 字节**，SHA-256：
`483AAB5FEBF5B7B0F500BFA9D049DDC1A5684151C9A59820377187771FCDA9EE`。

- 第九组独立通过：`build/tmp/xlir_core9_final.log`。
- 完整核心重新通过第 1—9 组；第八组 7,200/12 分配平衡和畸形 IR 单元均
  重跑通过。聚合在第十组开头的旧随机库示例缺失 `xl6_xrt_rng_string` 停止，
  整个聚合仍未通过：`build/tmp/xlir_core9_final_full_core.log`。
- 418 个确定性/严格 C fixture 通过：`build/tmp/xlir_core9_final_determinism.log`。
- 默认准入 516 候选、501/501 双后端生成成功、0 缺口、0 执行异常，hash
  稳定；数量门提高为 516/501/501：`build/tmp/xlir_core9_final_default.json`。
- 全源 1,767，legacy 接受 1,272、其中 XLIR 接受 1,227、预期拒绝 45、
  非预期拒绝 0，hash 稳定：`build/tmp/xlir_core9_final_audit.json`。
- 静态 IR 合同、109 行/347 代表用例登记、批处理与核心 C fixture 接线通过。
- 独立 collection-member intrinsic 门重跑通过，包含可变容器形参与 typed
  collection methods 的严格 C/运行断言：
  `build/tmp/xlir_core9_final_collection_intrinsics.log`。

长期目标保持 active、原范围不变。第十组同时包含标准库与语言所有权用例，
后续必须分辨两者，不能跳过整个第十组。继续压实普通/协议/构造调用的部分
求值、空函数调用错误行为、捕获与 await/yield 生命周期、模块最终清理以及
最后的完整质量门；以上数字均不意味着这些剩余工作已完成。

## 语言加固续批：二元求值事务与跨 await 保活（2026-09-06）

本批继续执行“标准库最低标准、语言本身必须压实”的范围，不扩展标准库。
完整重建的验收编译器为
2026-09-06 05:06:02、10,544,640 bytes，SHA256：
`7C34BB4B01C683B12C4AC2DA7085608B1174BD253B407CEB425B75286D27FFD6`。
构建证据：`build/tmp/xlir_binary_verified_build.log`。

### 修复与架构边界

- 最小反例 `make() == failingValue()` 在右侧抛错时泄漏左侧对象，原始复现
  记录为 `build/tmp/xlir_core10_binary_before.log`。
- `xl6IrLowerManagedBinary` 在求值前选择具体 RuntimeOp，并统一字符串比较、
  拼接/转换、动态比较/算术和派生类相等比较的求值事务。每个需要保活的输入
  都先获得自己的 owner，再以 TEMP STORE/LOAD 交给借用型运算；成功及后续
  操作数失败均逆序释放。可抛错动态算术保留相邻 ERROR_TEST 和独立错误边。
- 清除了旧的三组重复 managed-binary 降低分支。数字执行类型提升、短路逻辑、
  coalesce、协议运算、成员查询和集合代数仍保持各自明确的语义边界。
- 扩展分配核算进一步复现 800 个未释放字符串。原因是字符串装箱复制内容，
  但节点求值产生的原 owned string 没有释放。现在由
  `xl6IrLowerNodeAsRuntimeXValue` 关闭这项所有权；底层 boxing helper 的借用
  合同不变，避免已经保活的调用方重复释放。
- 非空分支中的可空字符串采用实际降低值的 TypeId 复制、保活，不再用原始
  nullable storage TypeId 生成不匹配的 STRING_NULLABLE_COPY。
- 没有给 Cgen 增加 AST 追溯、语义猜测或新临时值规则。

### 持久回归与验收

新增 `test/cases/xlir_binary_operand_lifetime.xl` 和
`tools/check_xlir_binary_evaluation_lifetime.ps1`，接入 core 10 与迁移登记。
用例包括 13 条右侧失败路径、借用值被右侧替换、字符串/动态值转换、动态
除零、嵌套运算、派生相等/不等、可空收窄和不可达短路分支。

- 53 个显式 managed binary RuntimeOp，18 条后续操作数失败边，6 项 checked
  运算错误边，4 个字符串转换保活关系，5 项 owned-string 装箱源清理。
- 200 轮、3,000 个语言对象全部析构；22,000 次生成代码及字符串生产器分配
  全部释放。严格 C、CLI run、CLI build/执行及重复 IR/C 均通过。分配门下限
  提高到 22,000，失败时区分 malloc/字符串生产器，不能靠析构数量掩盖字符串
  泄漏。证据：`build/tmp/xlir_binary_final_contract.log`。
- 419 个 fixture 的重复 C 生成及严格 C 编译通过：
  `build/tmp/xlir_binary_final_determinism.log`。
- 默认命令准入：517 候选、502/502 双后端成功、0 缺口、0 执行异常、hash
  稳定；数量门提高至 517/502/502。证据：
  `build/tmp/xlir_binary_final_default.json`。
- 全源审计：1,768 源文件、legacy 接受 1,273、其中 XLIR 接受 1,228、预期
  拒绝 45、非预期拒绝 0、hash 稳定。证据：
  `build/tmp/xlir_binary_final_audit.json`。
- dynamic binary 和 extended boxing 的独立严格 C/legacy 差分运行通过：
  `build/tmp/xlir_binary_final_dynamic.log`、`build/tmp/xlir_binary_final_boxing.log`。
- async generic planner 和两/三次 await 状态机的严格 C/运行验证通过，后者
  覆盖已有的 success/reject/cancel/close 模式：
  `build/tmp/xlir_binary_final_async.log`、`build/tmp/xlir_binary_final_multi_await.log`。
- 完整 core 入口重跑，1–9 组通过；第十组新增二元事务门也通过，随后在
  random 标准库示例的旧 `xl6_xrt_rng_string` 未解析符号处停止。没有把第十组、
  后续 function ABI 或 current regressions 计为通过。证据：
  `build/tmp/xlir_binary_final_core.log`。

二元事务的跨 await 状态现在包含明确的 operand-owner storage；跨挂起使用的
SSA LOAD 是借用观察，其 spill 必须复制，而不能从已有 storage 的清理路径
中窃取 owner。异步门检查两者的精确 TypeId/存储对应、成功/失败释放以及每个
managed frame slot 唯一的 guarded destructor。原先写死的 SSA 编号和旧
move-owned 形态断言已改成这些关系检查；成员查询门同样按每个临时存储的
精确类型核对唯一释放，并区分 xvalue release 与装箱源字符串 release。

迁移登记为 110 语义行、348 代表用例、403 全 fixture 候选、141 legacy
差分、44 个 XLIR-only runtime 合同。静态 IR 合同、批处理接线、核心 C fixture
接线及 Markdown 链接验证通过。

### 未完成范围

长期目标继续保持 active，不能把上述准入/编译数字解释为所有语言生命周期
已经完成。core 10 仍有需要迁移的旧 C 形态断言及标准库旧桥接问题，本批只
把新二元运算回归放在标准库用例之前，没有跳过整个组。

源码核查仍确认 `xl_script_module_unit` 没有 XLIR global/static finalization
程序；该项是语言生命周期缺口，不属于可延期标准库。后续应在 IR 中建立
模块最终清理、部分初始化、重复 unit 和错误保留合同，再让 Cgen 机械调用。
普通/协议/构造调用的部分求值、空函数调用错误、集合代数、捕获与真正挂起
后的释放边界，以及剩余完整质量门仍须继续逐项压实。

## 语言加固续批：模块生命周期与全局最终清理（2026-09-06）

本批将模块卸载作为独立的 Lowered XLIR 程序，而不是 Cgen 对声明树的清理
推断。`MODULE_FINI`、一基 `moduleFinalizerFunction` 与
`moduleFinalizerCleanupCount` 确定模块边界；Typed IR 的这两项为零。

### 语义与实现

- Lowering 以全局存储的精确物理所有权生成逆序 RELEASE/CELL_RELEASE 或
  原生销毁程序。类静态字段、普通全局、集合、闭包和原生句柄承载对象均归
  模块所有，不再在 initializer 的词法退出中被提前释放。
- 有 owner 的最终清理先 ERROR_TAKE，清理后用同一个 SSA error 执行
  ERROR_SET_TAKE。验证器核对唯一 finalizer、每个 owner 的种类/类型/目标、
  逆序及原始异常的恢复；Cgen 对损坏的 IR 同样 fail-closed。
- 无 owner 的模块也生成只有 RETURN 的 finalizer，卸载仍重置初始化状态；
  不为纯标量清理额外引入错误运行时调用。反例原先会在卸载后跳过初始化。
- 生命周期区分未初始化、初始化中、可用、最终清理中。内部初始化失败可以
  直接进入 finalizer；外部卸载仅接受可用态，防止初始化中的宿主回调卸载
  半成品后重入构造。重复 init/unit、析构期间的递归 unit/init 和卸载后
  重建均有运行断言。初始化重入原始反例：
  `build/tmp/xlir_module_initialization_reentry_before.log`。
- `values[0].id` 暴露索引 rvalue 在标量字段投影后泄漏：ClassField lowering
  现在按明确的字段 ABI 关闭独立标量结果的 receiver owner，不要求结果本身
  必须为 MOVE。指针/对象借用投影仍有独立的父对象生命周期边界。
- JIT 一次性运行不属于 activated module，原先跳过 module unit。因此
  `xlRunModuleMainUnlocked` 现在在入口返回后、TCC 被销毁前调用可选的
  `xl_script_module_unit`，且保持 API 解锁供回调使用。持久激活模块的接口
  不改为一次性清理；旧入口的重复 unit 依赖其既有幂等合同。
- 只有默认空函数全局变量时也会产生 closure RELEASE；Cgen 的辅助函数依赖
  现在直接消费这条指令，而不是依赖其他闭包构造偶然启用运行时。

### 持久回归

`tools/check_xlir_module_finalization.ps1` 接入 core 10，并覆盖以下层次：

- `xlir_module_finalization.xl`：global/static/collection/capture 的跨 main
  生存、初始化失败逆序回收、初始化/析构重入、重复卸载和 200 轮重建。
- `xlir_module_native_finalization.xl`：原生句柄承载对象的空值保护、唯一销毁、
  部分初始化及重试。`xlir_module_scalar_finalization.xl`：无 owner 的模块
  仍能在卸载后重新执行初始化。
- `xlir_module_init_cgen.c`：6 项缺失、重复、顺序错误、错误恢复和 raising
  finalizer 的 verifier/Cgen 拒绝测试。
- CLI 正常与抛错两个用例：JIT/AOT 均应在 main 之后输出逆序析构的 `2, 1`；
  抛错用例保留 `module-main-failure-sentinel` 并精确返回退出码 1。
- 既有 `cgen_function_default.xl` 同时接入确定性扫描和本门的严格 C 检查，
  防止默认空函数的清理辅助函数遗漏。

### 验收证据与剩余边界

本批完整构建：`build/tmp/xlir_module_release_verified_build.log`。
编译器为 2026-09-06 06:29:15、10,550,784 bytes，SHA256：
`B62562DEFD8E4EF9469E5F238D54C1579B2F4C2676BF79AB3AD3FED967477505`。

- 专用门通过：1,221 个语言对象全部析构，1,427 次生成代码直接分配及字符串
  生产器分配全部回收；初始化失败、两阶段重入、重复卸载、重建、正常和抛错
  JIT/AOT 均通过。该核算不等于跟踪了全部 XRT 内部内存分配。
  `build/tmp/xlir_module_release_final_contract.log`。
- 425 个确定性/严格 C 用例通过：
  `build/tmp/xlir_module_release_final_determinism.log`。
- 默认准入 522 候选、506/506 双后端成功、0 缺口、0 执行异常，hash 稳定，
  数量门提高至 522/506/506：`build/tmp/xlir_module_release_final_default.json`。
- 全源 1,773，legacy 接受 1,277，其中 XLIR 接受 1,232，预期拒绝 45，
  非预期拒绝 0，hash 稳定：`build/tmp/xlir_module_release_final_audit.json`。
- 5 项基础 CFG/循环/函数/短路/条件 IR-Cgen 单测在当前源码严格编译并运行
  通过：`build/tmp/xlir_module_verified_unit_shapes.log`。测试保留原来源函数
  内部的块/指令约束，单独核对新增 finalizer；短路断言只检查对应源函数体，
  不把宿主生命周期中的 C 逻辑条件误认为源语言短路退化。
- 6 类既有宿主错误边界在 XLIR/legacy 的 run/build 中通过：
  `build/tmp/xlir_module_release_final_error_boundary.log`。DLL 线程宿主、模块
  依赖生命周期、编译器 API 和 XRT 导出通过，对应
  `build/tmp/*.module_release_final.log`。本批此前的 DLL null-context 与旧
  embed-string 聚合未通过，分别停在 `xl6_xrt_string_trim` 未解析与 `xrtHash64` 重定义；
  不把这两项标准库桥接失败计作通过。
- 同一编译器的完整 core 入口已通过 1–8 组，在第 9 组的旧字符串 membership
  释放总数断言停止：`build/tmp/xlir_module_release_final_core.log`。该断言现按
  每个临时 owner 唯一且同类型释放、全部 IR RELEASE 对应 C 释放核对，独立门
  通过：`build/tmp/xlir_module_release_final_string_membership.log`。
- 修订后的完整 core 重跑在第 8 组受 Defender 中断：2026-09-06 06:39:04，
  `build/test/cgen_member_function_class_return.exe` 被隔离，实时保护已自动恢复。
  按用户此前授权尝试临时关闭实时保护，被系统以权限不足拒绝；没有恢复隔离
  文件或添加排除。中断证据：`build/tmp/xlir_module_verified_core.log`。
  因此未确认整个第 9/10 组、后续 function ABI/current regressions 或完整 core。

登记为 111 语义行、353 代表用例、408 全 fixture 候选、141 legacy 差分及
49 个 XLIR-only runtime 合同；静态 IR 合同、批处理、核心 C fixture 接线及
Markdown 链接检查通过。长期目标保持 active，不把本批进展视为全部完成。
后续仍须压实普通/协议/构造调用的部分求值、其他借用投影、空函数调用错误、
集合代数、真正 pending 的 await/yield 与模块卸载交互，以及剩余完整质量门；
标准库继续遵循最低要求，不跳过与语言语义混合的整个测试组。

## 语言加固续批：公开回调 ABI 与托管结果所有权（2026-09-06）

### 恢复回归后的实际发现

环境恢复后，同一旧基线 `B62562DE...` 的核心入口通过 1–8 组，随后在第 9 组
暴露另一处临时释放总数断言：xvalue membership 的 9 个全局 owner 已有模块
finalizer，不能再算成表达式 temporary。门现在逐个检查临时 owner 的类型和
唯一释放，并独立核对全局 owner 在 finalizer 中逆序、唯一释放。宿主补充三轮
初始化/最终清理及重复清理；第 9 组独立通过，含 5,600 次分配平衡。
证据：`build/tmp/xlir_module_resumed_core.log`、
`build/tmp/xlir_module_recovery_core09.log`。

第 10 组通过模块最终清理和二元求值门，之后停在 `xl6_xrt_rng_string` 未解析；
后续 current regressions 独立运行停在 `xl6_xrt_string_trim` 未解析，均不计为
整个组通过。证据：`build/tmp/xlir_module_recovery_core10.log`、
`build/tmp/xlir_module_recovery_current_regressions.log`。

函数 ABI 组另有真实公开边界缺口：`cgen_c_callback_export.xl` 的原输出把
C 函数指针参数写成内部 `xlir_closure*`，头文件甚至没有该类型的声明。
本批修复该边界，而不是让默认 XLIR 满足旧生成器的内部字符串形态。

### 明确的 IR 合同与实现

- `XL6_IrExportValueAbi` 新增 `raw-c-callback-borrow` 和
  `raw-c-callback-owned-result-borrow`。前者规定包装器按参数顺序取得临时闭包，
  借给内部函数，返回或失败后逆序释放；后者额外规定托管字符串/xvalue 回调
  返回值的所有权转交、失败释放和未使用结果释放。
- Lowering 与 verifier 共用精确签名分类。可空回调必须有唯一的完整函数 payload、
  managed closure 表示和 pointer null sentinel；不能把任意可空值当作回调。
  类、var 等托管回调参数保持借用，托管回调结果保持 owned，不能退回私有闭包
  指针作为公开 C 函数指针。
- 显式 C ABI export 接受可空的完整回调签名，仍保留原有的参数/返回类型限制；
  可空包装不绕过禁止托管字符串回调返回值的现有语义检查。语言级 export 的
  托管回调结果由上述独立 owned-result ABI 表达，覆盖 SAX 和 Agent 的既有接口。
- Cgen 只按验证后的 TypeId/边界 ABI 发射函数指针 typedef、签名适配和固定
  生命周期程序。包装体把真实函数指针保存在有类型的字段中，不将函数指针
  转成数据指针；保存到语言全局后的回调通过引用计数跨调用存活。
- 回调返回 owned 对象却同时报告原生错误时，适配器在释放该对象期间保存并
  恢复原始错误；对象析构产生的另一个错误不能覆盖它。非空回调包装分配失败
  不进入目标函数，回收此前已取得的临时包装，并报告 MemoryError。
- 所有导出实现参数使用位置索引命名，公开头文件仍保留 IR 中的参数名；源参数
  `result`、`xlir_export_arg_0` 不再与包装器内部变量碰撞。严格 C 原始反例：
  `build/tmp/xlir_export_callback_collision_before.log`。
- `03_core_function_abi.bat` 保留旧 C 形态检查作为显式 legacy 差分分支；独立
  XLIR import 分支及新增 export 门验证新体系，不改变默认后端或增加 fallback。

### 当前源码验收证据

完整构建：`build/tmp/xlir_export_callback_managed_build.log`。
编译器 2026-09-06 22:48:59，10,557,440 bytes，SHA256：
`F93AB993D0B51EF4AD949C665AE1DC4F7F6DA6014BAD237728323D3FBF4AC9F4`。
以下数据属于这一版，不沿用旧基线的全组通过记录。

- `check_xlir_export_callback.ps1` 通过公开头文件独立严格编译、分离 C 翻译单元
  链接、1,000 次保存回调、重复卸载/重新初始化、可空回调、部分包装取得失败、
  参数名碰撞、原生错误身份、语言抛错、托管字符串与对象结果的运行断言。
  418 次生成代码直接分配/字符串复制全部平衡，3 个 CallbackMarker 全部析构；
  这不代表跟踪了全部 XRT 内部分配。7 类破坏边界表示、所有权、回调签名及
  nullable payload/null 表示的 IR 同时被 verifier/Cgen 拒绝。另验证只有导出
  回调、没有其他闭包构造需求时的辅助函数依赖。
  证据：`build/tmp/xlir_export_callback_managed_contract.log` 与
  `build/tmp/xlir_export_callback_verified_function_abi.log`。
- SAX、Agent 既有语言导出均通过 Cgen 和公开头文件严格编译。附加探测中 SAX
  完整 C 通过，但 Agent 完整 C 停在既有 `xrtPathParent` / `xrtPathAbs` 的
  `void*` 对 `char*` 声明冲突；不将其算作完整 C 或运行通过。该标准库实现问题
  与回调的公开 ABI 分开记录：`build/tmp/xlir_export_callback_managed_function_abi.log`。
- 既有 class/receiver/product/nominal overload 导出门通过；固定宽度/F32 导出与
  legacy 头文件及运行差分通过。测试仍核对公开类型/名称，但实现参数名按位置
  验证，不要求实现重复用户名字。
  `build/tmp/xlir_export_callback_managed_boundary.log`、
  `build/tmp/xlir_export_callback_managed_width.log`。
- 模块最终清理完整专用门再次通过：1,221 个对象、1,427 次受跟踪分配，初始化
  失败、重入、错误保留、JIT/AOT 和 6 类损坏 finalizer 反例均通过：
  `build/tmp/xlir_export_callback_managed_module.log`。
- 426 个确定性与严格 C 用例通过：
  `build/tmp/xlir_export_callback_managed_determinism.log`。
- 默认准入 523 候选、507/507 双后端接受、0 缺口、0 执行异常、编译器 hash
  稳定；数量下限提高至 523/507/507：
  `build/tmp/xlir_export_callback_managed_default.json`。
- 全源 1,775，legacy 接受 1,278，其中 XLIR 接受 1,233，预期拒绝 45，
  非预期拒绝 0，编译器 hash 稳定。首版缺少 nullable 与托管 callback 签名
  引起的 36/8 个拒绝已由明确 ABI 补齐，没有转入允许失败清单：
  `build/tmp/xlir_export_callback_managed_audit.json`。
- 登记 112 语义行、354 代表用例、409 全 fixture 候选、141 legacy 差分与
  50 个 XLIR-only runtime 合同；静态合同、批处理和核心 C fixture 接线通过
  （11 个根 batch、67 个 suite、428 个 C 入口 fixture、11 个 package fixture）。

### 未完成的语言边界

完整函数 ABI 组仍未通过。当前编译器执行 `cgen_xcall_error_bridge.xl` 时
报告 `xlang callback error was not preserved`，证据为
`build/tmp/xlir_export_callback_verified_function_abi.log`。该宿主会读取并清除
XRT 错误，而既有用例要求语言层仍能捕获原错误；需要继续明确并验证宿主
消费诊断与语言异常传播之间的合同，不能把这个问题归入可延期标准库。

补测还记录了两项 check 接受、Lowered IR 拒绝的语言问题：`c_int` 复合赋值，
以及 `func(c_int)` 调用中整数字面量的隐式参数转换。独立反例和日志位于
`build/tmp/xlir_export_callback_cint_compound_gap.xl` / `.log`、
`build/tmp/xlir_export_callback_cint_literal_gap.xl` / `.log`。本批回调所有权测试
使用显式 `c_int(1)` 隔离所测合同，不代表这两项语言缺口已修复。

长期目标保持 active。普通/协议/构造调用的部分求值、空函数调用的错误路径、
其他借用投影、集合代数、真正 pending 的 await/yield 与模块卸载交互，以及
剩余完整质量门继续在原目标内；不以本批回调和准入数据替代整体完成证明。

## 语言加固续批：闭包调用参数的统一类型边界（2026-09-06）

### 修复范围

上一批记录的 `func(c_int)` 接受整数字面量但 Lowered IR 拒绝的问题已修复。
旧 lowering 在进入统一参数转换流程前，直接比较表达式声明存储 TypeId 与
参数 TypeId；除 `u64` 字面量外均拒绝，因而误挡语义层已接受的转换。

闭包调用现在与直接调用共用 `xl6IrLowerNodeAsType`，先产生显式的
`SCALAR_CONVERT`、`NULLABLE_WRAP`、`NULL`、装箱/投影等 SSA 值，再要求其
TypeId 与签名精确一致。兼容的函数值重绑定仍使用显式 COPY；`byref/out`
仍走精确存储地址流程，不允许借转换产生临时地址。原 verifier 的精确
参数检查保持不变，Cgen 本批没有生产代码变更。

这没有放宽语言隐式转换规则。新增反例同时确认直接调用和闭包调用均拒绝
把普通 `int` 变量隐式缩窄为 `c_int`；字面量、已允许的无损整数提升以及
其他已通过语义检查的转换才进入该流程。

### 当前证据

完整 `build_dll.bat` 成功，包含重新生成的编译器 VFS；本批编译器为
`release/xlc.dll`，10,557,440 bytes，SHA256
`2F4657F7AAE5399B2DCB044993E2A31AE074862929FEE65F4562AEE7D8120D2E`。
构建日志为 `build/tmp/xlir_closure_argument_build.log`。

- `xlir_closure_argument_conversion.xl` 覆盖 9 个闭包调用：`c_int` 字面量、
  精确 C 整数、`int32` 到 `int64`、C 浮点参数、可空注入/null、装箱以及
  多参数转换。门逐一核对实参 SSA TypeId，而不以生成 C 的强制转换文本
  代替 IR 验证；严格 C、重复生成、三轮 init/fini 和 legacy 运行差分通过。
- 源码顺序单列为 `xlir_closure_argument_evaluation_order.xl` 的 XLIR-only
  语言合同，并强制运行三轮。初次将它与 legacy 共用断言时，XLIR 通过，
  legacy 停在 `events == 12`：旧 C 代码把实参求值交给 C，不保证语言源码
  顺序。保留该初次差分失败的 `build/tmp/xlir_closure_argument_gate.log`；
  没有放宽 XLIR 的顺序断言。拆分后通过证据为
  `build/tmp/xlir_closure_argument_gate_verified.log`。
- 428 个确定性/严格 C fixture 通过：
  `build/tmp/xlir_closure_argument_determinism_final.log`。
- 默认准入 525 候选、509/509 双后端接受、0 缺口、0 执行异常、编译器 hash
  稳定；下限提高至 525/509/509：
  `build/tmp/xlir_closure_argument_default_final.json`。
- 既有八类标量转换及混合宽度提升的严格 C/legacy 差分门通过：
  `build/tmp/xlir_closure_argument_scalar.log`。
- 既有函数值所有权门通过，覆盖借用签名、闭包调用错误 effect、嵌套捕获、
  for 循环捕获存储、字符串复制/释放及 legacy 差分：
  `build/tmp/xlir_closure_argument_ownership.log`。
- 全源扫描 1,778，legacy 接受 1,280，其中 XLIR 接受 1,235，预期拒绝 45、
  非预期拒绝 0，编译器 hash 稳定：
  `build/tmp/xlir_closure_argument_audit.json`。
- 静态合同与接线通过：113 语义行、356 代表用例、411 全 fixture 候选、
  142 legacy 差分、51 XLIR-only 合同；11 根 batch、67 suite、429 核心
  C fixture 入口、11 package fixture；329 Markdown 文件链接通过。

### 仍需压实的语言问题

XCall 未在本批修复。当前实际 Lowered IR 中 `xcall_error_bridge_invoke`
仍为 `errorAbi=no-raise`、`errorPhysicalAbi=none`，证据为
`build/tmp/xlir_closure_argument_xcall_diagnosis.json`。旧实现将语言异常
另存并向 XRT 发布诊断副本，宿主的 `xrtClearError()` 不会清除语言异常；
XLIR 尚无对应的跨宿主错误保存及调用失败观察合同。因此必须同时处理
异常所有权与外部调用的失败 CFG，不能只改宿主测试或在 Cgen 追加检查。
完整函数 ABI 组在当前编译器上重跑：新参数门、公开回调的 418 次分配平衡、
7 类 verifier/Cgen 反例与公开头文件门均通过，然后仍停在
`xlang callback error was not preserved`。证据为
`build/tmp/xlir_closure_argument_function_abi.log`，不将整组记为通过。

`c_int +=` 缺口也仍存在。复合赋值需要明确读取类型、运算提升类型和写回
转换；不能把右侧提前缩窄至目标类型来“通过”所有操作，尤其 `/=` 的结果
可能因此改变。当前编译器重跑上一批两个原始最小反例：字面量调用已成功，
复合赋值仍拒绝，分别记录于
`build/tmp/xlir_closure_argument_cint_literal_verified.json` 与
`build/tmp/xlir_closure_argument_cint_compound_still_open.log`。
普通调用部分求值、真正 pending 的 continuation/卸载交互
以及其余全套质量门仍在长期目标内，本批准入数字不代表整体任务完成。

## 语言加固续批：共享外部调用错误域与原生回调（2026-09-07）

### 本批合同

上一节的 XCall 原始错误保留缺口已在专项测试中修复，但扩大外部调用的
错误 effect 后暴露了新的准入回归。本节的当前结果取代上一节的全绿快照；
本批未完成整体语言验收，也没有改变默认后端设置。

- 未识别的同步 `EXTERN_C` / `NATIVE_FACTORY` 使用明确的 `may-raise` +
  `foreign-scope` ABI。不能根据有没有 callback 参数判断是否会回调：原生
  代码可以保存回调后在另一个无参函数中调用它。
- CALL 产生独立的成功标志及拥有所有权的错误，参数清理之后的 ERROR_TEST
  消费该结果；清理或宿主清空 XRT 诊断槽不能抹去已经保留的语言异常。
  原生 factory 的托管包装移至成功分支，ERROR_TEST 不再错误引用包装后的
  非 CALL SSA 值。失败时非空原生返回值的回收仍需补全，见后续工作。
- `src/api/xl_foreign_error.c/.h` 提供共享运行时 scope 栈；Windows 使用 FLS，
  非 Windows 使用 pthread key。失败回调保留首个原始错误，并向宿主发布
  operation 为 `callback` 的诊断副本；内部已经 catch 的异常不污染外层。
  scope 必须同步、严格栈序结束，不允许跨执行上下文悬挂。
- 原生 C 回调新增 `foreign-scope-thunk`。Verifier 根据无捕获、精确 C 签名
  和 may-raise target 验证它，Cgen 只消费该 ABI；失败返回标量/指针零值或
  void，并通知 scope。它不展开 C 栈，C 函数后续代码仍会执行，语言错误
  分支在外部 CALL 返回后执行。无 userdata 的捕获闭包限制没有放宽。
- 运行时助手已接入共享 DLL、JIT 符号注册、独立 AOT 支持源和 XRT 测试对象；
  不在每个生成模块里另造 TLS。跨模块共享需要双方使用同一个运行时实例；
  独立运行时副本之间的状态共享不在本批证据范围。

### 当前验证证据

完整 `build_dll.bat`（含嵌入 VFS）成功；日志
`build/tmp/xlir_foreign_raw_callback_build.log`。当前 `release/xlc.dll`
10,565,632 bytes，SHA256
`3BFF513DED5929086EE414429E7A8029DC30AECD41D516663AD8A589370114E9`。

`tools/check_xlir_foreign_error_scope.ps1` 已接入函数 ABI suite，最新通过日志
为 `build/tmp/xlir_foreign_error_scope_integrated.log`：

- 两个分别链接 `xl.dll` 的客户端进行跨 DLL 错误保留；1,000 次首错误事务、
  旧诊断恢复、嵌套处理、native 直接错误、没有 scope 的回调、线程及 Windows
  纤程隔离通过。诊断副本分配失败、连错误对象都分配失败、乱序 End 反例
  通过；没有把这些检查描述为全局分配计数平衡。
- 未改写原始 `cgen_xcall_error_bridge.xl` 及其宿主断言：原始异常在宿主清空
  XRT 后仍被语言 catch 捕获；错误内容、操作和源位置检查通过。
- `xlir_foreign_scope_nested.xl` 覆盖已处理的嵌套 XCall、保存后无参重入、
  native 直接错误。
- `xlir_foreign_raw_callback_error.xl` 覆盖整数、布尔、浮点、指针、void
  回调失败，原生诊断替换/清空、保存后重入、连续失败保留第一个异常和
  嵌套 catch；宿主也断言失败后的原生代码确实继续执行。
- 上述三个语言用例均通过 JIT、生成 C 与宿主分别严格编译链接并运行、独立
  AOT 构建及运行。既有 `cgen_c_callback_interop` 的严格 C / 运行测试恢复通过。
- 静态合同与迁移接线通过：114 语义行、359 代表用例、413 确定性候选、
  142 legacy 差分合同、53 XLIR-only 合同。接线通过不代表所有合同已经运行。
- 核心 C 接线通过（434 入口 fixture、11 package fixture），batch 接线和
  329 Markdown 文件链接检查通过。全范围 C 接线仍报 MQTT / serial 下
  7 个 package C 文件未声明或未接线，本批未处理，日志
  `build/tmp/xlir_foreign_scope_c_wiring.log`；不将核心接线通过等同全范围通过。

当前默认准入结果为 **527 候选、legacy 511、XLIR 493、18 缺口、0 执行
异常、hash 稳定**，证据 `build/tmp/xlir_foreign_scope_admission_final.json`。
新增两个用例后，下限是 527/511/511，零缺口要求保持不变，**该门实际失败**。
全量确定性门也在 `xlir_export_callback_lifetime` 的 class layout 验证处失败，
日志 `build/tmp/xlir_foreign_scope_determinism_current.log`；不得沿用上一节
428 用例通过的旧结果。本批没有重新宣称全源扫描或完整函数 ABI 组通过。

### 下一批必须收口

当前 18 个准入回归：10 个 TCP Future 原生拥有布局验证、6 个 class destroy
布局验证、1 个 native property setter、1 个 `stdlib_xid_object` intrinsic。
前两类仍依赖旧的 no-raise 析构约束；不能只移除 verifier 限制后忽略错误
carrier。析构/自动释放的错误优先级、传播和完整清理需要作为语言合同落实。
setter 应归并到统一 CallSite / 调用错误边流程，不能另写一套补偿式 Cgen。

其他未完成项仍包括：XCall 结果装箱及多结果部分失败的完整回收和错误发布、
原生 factory 失败且返回非空拥有句柄时的回收、foreign ABI 的畸形 IR 反例、
真正 pending 的 async/generator 生命周期及跨平台执行上下文验证、普通调用
部分求值与 `c_int` 复合赋值提升/写回。声明快照在单独 JIT 宿主文件中的
不相关 feature closure 报错也仍有记录，专项宿主使用最小准确 ABI 声明，
没有关闭 feature 验证。标准库可最低验收，以上语言所有权/异常问题不可豁免。

## 语言加固续批：清理边界与释放重入（2026-09-07）

### 语言约束与运行时基础

最小析构抛错程序在 legacy 上运行成功，外层 catch 能捕获析构的异常；证据
为 `build/tmp/xlir_destroy_error_contract_probe.xl` 与对应 `.legacy.log`。
因此不能把析构改成静默忽略或统一 abort。当前 XLIR 仍需要显式建模自动
释放的错误边，以下运行时基础并不代表该迁移已经完成。

`xl6ErrorScopeBegin/End/CaptureFailure/PropagateFailure` 已在共享运行时实现，
并接入 JIT 符号和 AOT 支持源选择。清理与外部调用使用同一个执行上下文栈；
旧 `xl6ForeignCallBegin/End` 保持兼容包装，不另建第二个 TLS。
清理步骤失败时，CaptureFailure 收走当前诊断并设置独立的失败位，让后续
步骤仍能独立执行；End 分别交回新失败和恢复原有诊断。已有异常与清理异常
的优先级留给显式 IR route 决定，运行时没有暗中选择或执行语言控制流。
void 原生最终化边界可消费失败结果并向直接外层 scope 发布原始错误，不
修改其 operation 或位置；中间已处理的异常不会越过 catch 污染更外层。

专项测试 `xlir_cleanup_error_scope_runtime.c` 通过：跨 DLL 的两步清理链
重复 1,000 次，以及嵌套处理/传播等共 2,004 个步骤，保留原始错误身份、
原有 unwind 诊断、双向 foreign/cleanup 嵌套和缺失错误对象的失败位。
本用例 6 次受监控分配与释放平衡，live count/bytes 未增长，无新增 double
free、invalid free 或 use-after-free 计数。它验证边界机制，不冒充完整
语言析构或所有 continuation 的分配平衡证明。

### 已修复的实际释放重入缺陷

`xlir_cleanup_detach_reentry.xl` 的全局所有者在模块最终清理时触发析构，
析构检查该槽位并再次赋为 null。修改前严格 C 编译后的可执行文件退出 1，
观察值为 -1：旧的 `release(storage); storage = NULL` 顺序让析构仍看到
正在销毁的所有者。失败日志保留为
`build/tmp/xlir_cleanup_detach_reentry.before.log`。

RELEASE 合同现明确为先 detach 并清空 storage，再销毁取出的值；重入期间
新写入的所有者不能被末尾置空覆盖。`xl6_ir_cgen_release.c` 统一渲染此物理
顺序，sync storage、async/generator local/frame release 以及 frame destroy
action 共用它；frame init bit 同样在调用 destructor 前清除。产品析构先
整体取出并清空原聚合，再逆序释放取出的成员；CELL_RELEASE 也先清空槽位。
这些都是对已验证 storage、TypeId、destroy ABI 和 init bit 的机械投影，
没有增加 AST 回查或 Cgen 异常语义推断。

### 本批实际验证

完整 `build_dll.bat` 和嵌入 VFS 重建通过，日志
`build/tmp/xlir_cleanup_detach_build.log`。当前 `release/xlc.dll`
10,566,144 bytes，SHA256
`36C249F9927A4272AB32A80DFE1504102A2CCEA14479D102431214D458297A07`。
中间运行时基础版本为 `F8D760...`，不与当前编译器证据混用。

- `check_xlir_cleanup_detach.ps1`：新重入用例、显式 RELEASE、逐字节重复
  生成、严格 C、三轮 init/fini 通过；日志 `build/tmp/xlir_cleanup_detach_gate.log`。
- 原跨 DLL / XCall / raw C 错误桥及新增清理边界测试全部通过，原三个语言
  用例的 JIT、分离严格 C 和独立 AOT 仍通过；日志
  `build/tmp/xlir_cleanup_detach_foreign_gate.log`。
- async 两次/三次 await 的 scalar/managed activation 转交、成功/拒绝/
  取消/关闭终态、确定性和严格 C 门通过；日志
  `build/tmp/xlir_cleanup_detach_async_multi.log`。
- generator machine 的标量和托管捕获/结果、frame action、GET/TAKE、
  async-start/blocking-await、确定性/严格 C/运行语义门通过；日志
  `build/tmp/xlir_cleanup_detach_generator.log`。
- 产品结果门通过：direct/class/closure/generic 调用、托管 take/drop、
  严格 C 和运行测试；日志 `build/tmp/xlir_cleanup_detach_product.log`。
- 静态类存储的读写/托管替换门通过；文本断言同步加强为先 detach/清空再
  free，日志 `build/tmp/xlir_cleanup_detach_static_storage.log`。
- 静态合同、迁移接线、核心 C 接线通过：115 语义行、360 代表用例、414
  确定性候选、142 legacy 差分合同、54 XLIR-only 合同；436 核心 C 入口、
  11 package fixture。这些接线数量不是本批全量运行数量。

当前准入为 **528 候选、legacy 512、XLIR 494、18 缺口**，0 执行异常、
hash 稳定；证据 `build/tmp/xlir_cleanup_detach_admission.json`。
新用例提高了候选及成功数，但没有修复上一节列出的 18 个 error-effect
回归。门的下限提高为 528/512/512，零缺口要求不变，实际仍失败。

### 后续完整清理错误 CFG，不能跳过

下一步必须把清理作为 IR 中的错误事务落地，覆盖正常 block fallthrough、
return、break/continue、异常展开、模块最终化、参数/部分实参所有者和
continuation frame。尤其 return 的托管 SSA/产品在清理失败后必须回收，
不能仅插入 ERROR_TEST 后遗失尚未转交的返回值；异常展开必须执行完余下
清理，错误优先级由 route 明确表达。要同步扩展 call effect 固定点、
release/cleanup failure 计划和 verifier，再允许 may-raise class destroy
通过布局验证。当前 class destroy 的 no-raise 防线尚未移除，自动析构
的异常传播仍未迁移完成；不得因运行时 scope 单元测试通过而宣称完成。

## 语言加固续批：XCall 返回值事务（2026-09-07）

### 复现与修复边界

恢复后的 CLI 能正常启动，原 `xlc.dll` 哈希与上一批 `36C249...` 一致；
本批没有调整 Defender。随后继续检查清理边界，复现了 XCall 的两项缺陷：
七返回值函数在第一个字符串装箱失败时泄漏 12 个受监控分配对象、670 字节；
native 调用方清掉诊断后，外层 foreign scope 错误地返回成功。修改前证据：
`build/tmp/xlir_xcall_result_failure.before.log`，严格 C 可执行文件退出 3。

该问题发生在闭包成功返回之后的 ABI 传输阶段，不是函数体已经失败的情形。
现在 IR 的 runtime-callable 合同明确：成功 invoke 交出所有 managed 结果；
装箱事务拥有当前 box、已经提交的前缀和未提交的 carrier。managed result
必须具有已验证的 storage-release ABI。Cgen 按这个 TypeId 合同处理：

- xvalue 结果移交到 box 时立即清空原槽位，避免回滚时同一引用被释放两次。
- string、error、closure 的原拥有载体保留至结果提交完毕，失败也统一回收。
- 产品结果复用 canonical product drop，单结果复用 detach-before-destroy；
  不另写一套按 AST 或源代码拼写追溯的回收逻辑。
- 先向共享边界发布失败，再保存 native 诊断并执行回滚；后续清理即使重新
  进入 callback 并发布其他错误，也不能抢占原始失败。回滚后恢复诊断。
  invoke/argument 的既有失败路径同样调整为先发布、后清理。
- 产品 drop 的需求分析计入可达 callable adapter，原型统一提前到类型声明
  之后，以便 adapter 与普通函数体共享同一 helper；不重复生成产品析构器。

这里没有改变 class destroy 的 no-raise 准入条件，没有解决一般 return
清理失败时的 pending owner，也没有宣称完整 cleanup CFG 已落地。

### 构建与实际测试

完整 DLL/VFS 重建成功，日志 `build/tmp/xlir_xcall_result_transaction_build.log`。
`release/xlc.dll` 为 10,567,168 bytes，SHA256：
`22E688CD27E13BD2ECF90274CF1155299FCD3F74206E5E0DCB0B1E57CDF7718B`。
下列证据对应此版本，不复用上一批二进制的运行结论。

- `check_xlir_xcall_result_failure.ps1` 通过。五个精确 callable TypeId，
  显式装箱 ERROR_TEST、七槽位产品以及单个 string/var/error 返回值，
  生成 C 逐字节稳定。严格 GCC/DLL 运行包含 1,500 次失败、400 次成功，
  其中 300 次为真实字符串装箱或第五返回槽扩容的分配失败；逐次检查 live
  count/bytes、分配释放差值以及 double/invalid free、use-after-free，
  全部平衡。返回闭包能继续读取捕获值，错误身份不被重入清理替换。
- 同一生成 C 的 JIT、独立 AOT 各通过 1,200 次注入失败和 400 次成功。
  这两条 lane 不冒充分配计数测试：JIT runtime 无法启用 memory debug，
  因此使用显式 `--smoke` 模式；严格 DLL lane 保留完整计数和真实分配失败。
  C-only AOT 宿主通过 include 使用生成模块，显式链接共享 error-runtime
  支持源；没有为测试改动生产支持源选择器。日志：
  `build/tmp/xlir_xcall_result_transaction_gate.log`。
- 原 foreign scope 单元、清理 scope 单元、三个语言错误桥的 JIT/分离严格
  C/AOT 通过；`build/tmp/xlir_xcall_result_transaction_foreign.log`。
- 原 XCall 类型/运行/legacy 差分门通过；
  `build/tmp/xlir_xcall_result_transaction_xcall.log`。其中旧的单跳 VALUE
  参数形状断言已更新为严格检查 retain/box 后的 STORE、LOAD、MOVE，再进入
  consuming dynamic call；没有取消所有权断言或绕开运行测试。
- direct/class/closure/generic 产品调用、托管 take/drop 和严格 C/运行门
  通过；`build/tmp/xlir_xcall_result_transaction_product.log`。
- runtime-callable value ABI 门通过，覆盖 dict/array variadic adapter、
  owned-first 多结果投影、包运行和 legacy 差分；
  `build/tmp/xlir_xcall_result_transaction_callable.log`。
- 静态合同、迁移接线、核心 C 接线、source/batch 接线通过。迁移接线现为
  116 语义行、361 代表用例、415 确定性候选、142 legacy 差分合同、55
  XLIR-only 合同；核心 C 接线为 438 个入口、11 个 package fixture。
  接线计数并非本批全量运行计数。

全量确定性门仍在 `xlir_export_callback_lifetime` 因 class layout
malformed 停止，日志 `build/tmp/xlir_xcall_result_transaction_determinism.log`；
不能将本批专项确定性通过报告成全量通过。完整函数 ABI 组及全源扫描本批
没有重新宣称通过。Markdown 329 文件链接检查通过。

### 准入与剩余任务

准入仍失败：**529 候选、legacy 513、XLIR 495、18 缺口**，0 执行异常，
编译器哈希稳定；`build/tmp/xlir_xcall_result_transaction_admission.json`。
下限同步提高为 529/513/513，零缺口要求未变。仍是 10 个 TCP Future
拥有布局、6 个 class destroy 布局、1 个 native property setter、1 个
`stdlib_xid_object` intrinsic；本批没有新增或豁免准入缺口。

后续仍须落实上一节的完整清理错误 CFG，包括正常离域、return 的待转交
托管值、错误展开的全部清理、参数/部分实参所有者与 continuation frame。
清理 effect 固定点、显式错误优先级和 verifier 必须先到位，不能通过放松
析构布局校验或忽略错误 carrier 使上述 18 个缺口表面变绿。

## 语言加固续批：正常离域的显式清理计划（2026-09-07）

### 从清理推断收缩到 IR 合同

正常 block fallthrough 原先没有自己的 ExitPlan。Lowering 再扫描直接声明，
搜索清理 arena 确定 native destructor，并单独补充标量 capture cell 的释放。
本批将这一条路径收缩为消费 canonical IR 计划，生产修改只在 XLIR 类型、
计划构建/验证与 lowering；Cgen 没有增加源语义回查或生成补丁。

- `XL6_IrExitPoint` 明确区分 `BEFORE_NODE` 与 `AFTER_SCOPE`。前者保留
  return/throw/break/continue 和表达式失败的原有含义；后者表示 block
  全部直接语句执行后的正常离域，不把 block 自己的 NodeId 当成执行截止点。
- 每个 BLOCK 都有唯一的 AFTER_SCOPE 计划，其 kind 为 NONE，目标深度为
  block 自身深度、目标 region 为 0。计划按直接声明的逆序、多个绑定的
  逆序列出清理；不沿用 BEFORE_NODE 的数值 scopeDepth 过滤。直接构造
  AST 的客户端即使未填写源退出深度，结构中的正常作用域所有权仍然有效。
- trivial 绑定保留 `END_STORAGE` 生命周期锚点，不生成析构。若标量随后
  被 capture cell 化，原位置改为显式 `CELL_RELEASE`；managed 绑定和
  direct native owner 分别保留 RELEASE 与确切的 NATIVE_DESTROY ABI。
- verifier 独立核对计划的 origin、退出点、目标、绑定身份、TypeId、
  owner、callee 和完整逆序。Lowered 阶段只允许省略没有物化 storage 的
  优化掉绑定；已经物化的 owner 不能因此丢失。
- 正常 block cleanup 只按计划发出明确的 ownership 指令。模块全局仍归
  MODULE_FINI；已验证的 trivial 锚点不要求虚构 RELEASE，promoted cell
  则必须走原有 CELL_RELEASE 物化校验。IR 文本/JSON 都显式输出退出点。

这仍是完整清理错误 CFG 的基础步骤，不是最终事务模型。现有物化校验
检查同一 owner 函数内存在对应 ownership 指令，并未证明每条 CFG 出边的
清理都完整且有序。普通析构的 MAY_RAISE effect、清理失败 route、pending
return owner、异常展开的剩余清理和错误优先级均还需落实；本批没有移除
class destroy 的 no-raise 限制，也没有增加自动析构抛错已受支持的承诺。

### 当前构建与专项证据

最终完整 DLL/VFS 重建成功，日志
`build/tmp/xlir_scope_exit_plan_verified_build.log`。本批证据使用
`release/xlc.dll` 10,569,728 bytes，SHA256：
`88706404567F9F69693C2F25C08D224A745B83BE4147E3F2756E627872BE8273`。
运行时 VFS 115 项、5,515,719 → 859,501 bytes；编译器 VFS 532 项、
32,411,848 → 4,602,816 bytes。中间 preflight 构建不作为最终验证版本。

- `check_xlir_scope_exit_plan.ps1` 通过 typed/lowered 计划检查、scalar
  cell promotion、类正常离域逆序、多绑定产品、managed/trivial 泛型实例
  和嵌套泛型调用。严格 GCC/DLL 运行重复 1,000 轮，27,027 次受监控分配
  与释放平衡，live count/bytes 无增长，无新增 double/invalid free 或
  use-after-free。语言 JIT、独立 AOT 和生成 C 逐字节确定性同样通过；
  `build/tmp/xlir_scope_exit_plan_gate.log`。
- 新直接 AST 合同测试在 typed 与 lowered 各破坏 8 项计划字段/动作顺序，
  16 个反例全部被 verifier 拒绝；其中 lowered 的 8 个也在 Cgen 边界
  被拒绝且未输出 C。原始合法 IR 在每次恢复后仍验证通过。
- 既有 `xlir_function_lower`、`xlir_cfg_lower`、`xlir_cgen` 均从当前
  源码严格重编译并运行通过，证据为
  `build/tmp/xlir_scope_exit_plan_xlir_function_lower.log`、
  `build/tmp/xlir_scope_exit_plan_xlir_cfg_lower.log`、
  `build/tmp/xlir_scope_exit_plan_xlir_cgen.log`。旧测试不再假定 arena
  中只有一个退出计划，改为分别验证正常与 return 计划的逆序；Cgen 的
  未物化 DROP 反例保留合法计划 slice，只添加独立错误 action，继续精确
  检查原物化诊断，避免先破坏新计划合同而掩盖原测试目的。
- async 多次 await、generator machine、try/finally pending exit 门通过，
  覆盖各门既有的 frame/返回值所有权、终态和严格 C/运行/差分断言；日志
  `build/tmp/xlir_scope_exit_plan_async_multi.log`、
  `build/tmp/xlir_scope_exit_plan_generator.log`、
  `build/tmp/xlir_scope_exit_plan_finally.log`。不将这些门等同全部 pending
  continuation、取消/卸载重入或跨平台生命周期证明。
- XCall 返回值事务及 foreign/cleanup 共享错误边界门在本批 DLL 上重跑
  通过，分别为 `build/tmp/xlir_scope_exit_plan_xcall_result.log` 与
  `build/tmp/xlir_scope_exit_plan_foreign_scope.log`。XCall 严格 lane 仍有
  1,500 次部分失败（含 300 次真实分配故障）、400 次成功和所有权平衡；
  JIT/AOT 为各自的注入故障 smoke，不冒充 memory-debug 计数 lane。
- 静态合同、迁移接线与核心 C 接线通过。当前为 117 语义行、362 代表
  用例、416 确定性候选、142 legacy 差分合同、56 XLIR-only 合同；核心
  C 接线为 441 入口 fixture、11 package fixture。这些接线数不是本批
  全量运行数，日志为 `build/tmp/xlir_scope_exit_plan_check_xlir_contract.log`、
  `build/tmp/xlir_scope_exit_plan_check_xlir_migration_coverage.log` 与
  `build/tmp/xlir_scope_exit_plan_check_c_fixture_wiring.log`。
- source/batch 接线通过，分别检查 286 个源码文件、11 个根 batch 和 67
  个 suite batch；`build/tmp/xlir_scope_exit_plan_source_wiring.log` 与
  `build/tmp/xlir_scope_exit_plan_batch_wiring.log`。Markdown 链接检查通过，
  日志 `build/tmp/xlir_scope_exit_plan_markdown.log`。

全量 C 确定性门在当前 DLL 上重跑，仍于 `xlir_export_callback_lifetime`
因 class layout malformed 停止，日志
`build/tmp/xlir_scope_exit_plan_determinism.log`，实际退出 1。不能将本批
专项确定性通过描述成全量通过；完整函数 ABI 组本批也未重新宣称通过。

### 准入结论与剩余范围

`build/tmp/xlir_scope_exit_plan_admission.json`：**530 候选、legacy 513、
共同基线上的 XLIR 495、18 缺口、0 执行异常，compiler hash 稳定**。
和上一批 529 个既有候选逐项比较，两后端的退出码均未变化；新增的
`xlir_scope_exit_plan.xl` 在 XLIR 通过，legacy 对嵌套泛型调用报不支持
类型 T，因此不计入 legacy-success 交集。脚本候选下限增为 530，交集
成功下限维持上一批的 513/513，零缺口要求未放宽，**准入门仍实际失败**。
不能把新增 XLIR-only 用例误算为新增的共同基线成功，或称 495 是所有
530 个候选中的 XLIR 总通过数。

剩余 18 项仍为 10 个 TCP Future owning layout、6 个 class destroy
layout、1 个 native property setter、1 个 `stdlib_xid_object` intrinsic。
标准库最低验收不豁免语言清理、异常与所有权问题。下一阶段仍以完整清理
错误 CFG 为主线：正常/突然离域的清理失败计划、参数及部分实参所有者、
待转交返回值、frame/module 最终化，以及先有 effect/verifier、后准入
may-raise destructor 的顺序；不能只删除 no-raise 检查使报表变绿。

## 语言加固续批：返回值持有至清理完成（2026-09-07）

### 返回与清理的统一所有权顺序

本批继续完整清理错误 CFG 的主线，先消除一个关键前置问题：返回值已经
求值完成，但尚未进入可回收的 storage，局部/参数释放却已经开始。直接
在旧路径插入清理失败分支会遗失返回值，或让产品与原局部同时拥有同一引用。

现在普通单值/多值返回先将独占局部移出，再把完整结果存入 TEMPORARY。
产品使用 PRODUCT_STORE_MOVE，不用普通 C 聚合复制冒充所有权转移。待转交
结果按明确的 storage 身份留在 lowering 的所有者栈中；它不是未消费的
额外产品 LOAD，也不能由普通实参求值 commit 顺手转交。局部、迭代资源和
参数清理之后，才发出紧邻的 LOAD/MOVE/RETURN。

底层 `xl6IrLowerEmit` 不再隐含插入参数释放；终止路径显式调用统一参数
cleanup 和最终 commit。正常 return、try/finally、try/catch/finally、
多 catch/finally 的终态遵循同一规则，三处 finally 返回收尾已合并为
`xl6IrLowerFinishDeferredReturn`。内层 finally 向外层交出待返回值时，
也先完成自己的 cell 清理再提交；原先 finally 覆盖 return 的丢弃路径保留。

具名函数适配器先通过 ERROR_TEST 区分调用成功/失败。成功结果进入待转交
槽位后再释放默认参数；失败路径仍释放全部已物化默认参数，再走 THROW。
两条路径共用逆序默认参数 cleanup，没有在 Cgen 中增加函数名特判。

### 验证边界

XLIR verifier 对每个托管 RETURN 要求相同函数、相同 TypeId 的 TEMPORARY
以及紧邻 LOAD/MOVE commit。参数拥有者校验只跨过这对已经精确验证的
非失败指令寻找终态释放，不能跨过任意调用/控制流宽松地寻找一次 RELEASE。

借用实参的临时所有者可以在 ERROR_TEST 后分别清理，但必须由 CFG 证明
每个后继路径都存在匹配 RELEASE；提前 LOAD/STORE/MOVE/DROP、错误 TypeId、
缺失清理、越界目标或未闭合循环均不能建立该证明。这里是通用 CFG 路径
验证，不是豁免具名适配器。共享清理块可复用已验证路径。

新门 `check_xlir_return_commit.ps1` 覆盖参数/局部、重复返回同一局部的
产品、嵌套 finally、单/多 catch、finally 抛错取消产品返回，以及带托管
默认参数的适配器成功/失败。直接 AST 单元对单值和产品分别破坏 7 种
最终移交合同，包括把参数释放移到 commit 之后；14 个反例都要求 verifier
与 Cgen 拒绝且不输出 C。另一个内部 CFG 单元覆盖两分支、共享清理和 8 个
错误路径反例，不把该内部分析单元冒充完整模块或完整语言路径验证。

本批没有放开 may-raise class destroy，没有完成自动析构的失败分支或
错误优先级，也没有证明全部 pending continuation/frame/module 最终化。
待返回 storage 已为清理失败留出明确所有者，但自动清理的 effect、失败
route、完整剩余清理和原始异常保护仍必须接入；终态相邻性不是全 CFG
所有权完备性的替代证明。生产 Cgen 本批未增加语义回查代码。

### 最终构建与当前验证

最终完整 DLL/VFS 构建日志为
`build/tmp/xlir_return_commit_verified_final_build.log`，实际退出 0。
`release/xlc.dll` 10,573,824 bytes，SHA256：
`331628533C05580D3618734E56B17CE940EF9C6784D916F24EBBCF596F30D0C3`。
编译器 VFS 532 项，32,419,293 → 4,604,709 bytes；运行时 VFS 115 项，
5,515,719 → 859,501 bytes。中间构建用于暴露旧参数终态断言、额外产品
LOAD 和借用实参跨错误边的问题，不用它们的准入结果冒充最终版本。

`check_xlir_return_commit.ps1` 在最终 DLL 上通过，日志
`build/tmp/xlir_return_commit_gate.log`：7 个精确待转交 commit、7 条先清理
再提交路径；严格 GCC/DLL 下 1,000 轮事务，80,080 次受监控分配与释放
平衡，live count/bytes 无增长，无新增 double/invalid free 或 UAF。
语言 JIT、legacy 运行对照、独立 AOT、逐字节重复生成 C 全部通过。
14 个模块 verifier/Cgen 反例和 8 个内部 CFG 证明反例通过，范围区分如上。

最终 DLL 上的回归复核也实际退出 0：

- async multi-await、generator、product-result、try/finally pending-exit。
  finally 门不再要求旧的单次 PRODUCT_STORE_MOVE；现在验证完整产品先进入
  局部待转交槽位，再经精确 LOAD/MOVE 进入 finally 槽位，最后经精确
  LOAD/MOVE/RETURN 交出。两个槽位必须不同、同 TypeId、各清空一次，且
  保留 finally 覆盖返回的释放和 typed discard 检查，并非仅放宽计数。
- named-function adapter 的 11 个用例，以及 borrowed-call provenance。
- XCall result failure：严格 C 下 1,500 次部分失败（含 300 次真实分配
  失败）和 400 次成功；JIT/AOT 各 1,200 次注入失败和 400 次成功。
- foreign error scope：共享运行时的 1,000 次首错事务、2,004 个 cleanup
  boundary 步骤，独立 DLL/线程/fiber、独立 C 链接与 JIT/AOT。
  该运行时边界测试不等于语言自动析构错误 CFG 已经接通。
- scope exit plan：1,000 轮 class/cell/product/generic 作用域，27,027
  次平衡分配，16 个 verifier 反例（其中 8 个在 Cgen 边界）仍通过。
- 当前源码直接编译的 `xlir_function_lower`、`xlir_cfg_lower`、`xlir_cgen`
  三个严格 GCC 单元通过；不是仅运行旧的测试可执行文件。

上述复核日志使用 `build/tmp/xlir_return_commit_` 前缀，分别为
`async_multi.log`、`generator.log`、`product.log`、`finally_verified.log`、
`named_adapter.log`、`borrowed_provenance.log`、`xcall_failure.log`、
`foreign_scope.log`、`scope_plan.log` 和对应三个单元名称的 `.log`。

静态合同、源码接线、batch 接线、核心 C 测试接线及源码编码均通过。
核心 C 接线为 445 个入口 fixture、11 个 package fixture；迁移覆盖清单
为 118 个语义行、363 个代表 fixture、417 个确定性候选、143 个两后端
运行对照、56 个 XLIR-only 合同。这些是清单/接线统计，不是全量运行通过数。

全量确定性门本轮实际退出 1，在 `xlir_export_callback_lifetime` 的首次
C 生成处停止，诊断仍为 `XLIR class layout arena is malformed`，日志
`build/tmp/xlir_return_commit_determinism_final.log`。全量 C fixture 接线
也实际退出 1：MQTT 的 5 个和 serial 的 2 个扩展库 C 测试未登记，日志
`build/tmp/xlir_return_commit_c_wiring_final.log`；没有通过排除这些文件或
修改门槛把全量检查变绿。核心接线通过与这两个全量门失败分别报告。

最终默认准入仍失败：**531 候选、legacy 514、共同基线 XLIR 496、18
缺口、0 执行异常、hash 稳定**；证据
`build/tmp/xlir_return_commit_admission_final.json`。上一批 530 个候选的
两后端退出码逐项未变，新用例两后端均通过。下限增为 531/514/514，
零缺口要求保持不变。仍为 TCP Future 布局 10 项、class destroy 布局
6 项、native property setter 1 项、`stdlib_xid_object` intrinsic 1 项。

## 语言加固续批：无错误对象时仍保留首个失败（2026-09-07）

完整清理错误路径要求“失败是否发生”和“诊断对象是否分配成功”互相独立。
本轮检查共享清理边界时发现，虽然已有 `failed` 位，CaptureFailure、
ObserveFailure 和 End 仍以 `pending != NULL` 判断是否存在首错。因此第一次
失败没有分配出 xerror 时，后续析构/回调或残留 TLS 诊断会替换第一次结果。
新增反例在修复前的 DLL 上实际断言失败，证据
`build/tmp/xlir_cleanup_first_failure_before.log`。

现在首次失败由 `failed` 锁定，即使它的诊断是 NULL。CaptureFailure 和
原生边界传播只在尚未失败时保存诊断；End 按失败位选择已锁定结果，并释放
后续诊断。既有错误对象的身份、已有 unwind 诊断的独立恢复、只向直接外层
scope 传播，以及中间 catch 对外层的隔离均保留。没有增加另一套 TLS、
修改公开结构布局，或把清理错误改为静默成功/统一 abort。

`check_xlir_foreign_error_scope.ps1` 在最终 DLL 上实际通过，日志
`build/tmp/xlir_cleanup_first_failure_verified.log`：

- 严格 C/DLL：保留原有 2,004 个清理步骤，另加 12 种首错/后续通知组合、
  1,200 轮事务；其中 400 轮对回调诊断进行真实分配失败注入。后续通知覆盖
  显式 CaptureFailure、失败结果传播、跨 DLL 回调消费诊断，以及残留 TLS
  诊断。306 次受监控分配与释放平衡，live count/bytes 和非法释放计数不变。
- C-host JIT 和独立 AOT 各通过 800 轮、8 种显式缺失诊断组合，验证符号
  注册和嵌入源码链接。两者的编译器运行时不启用 memory-debug，不把这些
  通道冒充实际分配注入或内存计数测试；真实注入由上述严格 DLL 通道覆盖。
- 既有 foreign scope 的 1,000 次首错事务、独立 DLL、线程/fiber、五种
  raw C 返回 ABI、已处理的嵌套回调和独立 C/JIT/AOT 集成均通过。

XCall result failure、return commit、cleanup detach 三个门也在最终版本
复核通过：分别保留 1,500 次部分失败与 400 次成功、80,080 次分配释放
平衡，以及 detach-before-destroy 的重入观察。日志同用
`build/tmp/xlir_cleanup_first_failure_` 前缀，后缀为 `xcall.log`、
`return.log`、`detach.log`。静态合同、源码/batch 接线、核心 C 接线通过；
核心 C 仍为 445 个入口 fixture 和 11 个 package fixture。

完整 DLL/VFS 构建实际退出 0，日志
`build/tmp/xlir_cleanup_first_failure_build.log`。`xlc.dll` 为
10,573,824 bytes，SHA256
`C285F1B9BC14926314670698830D9F45E38B6FED44753BA082BABBAFC5DB66E1`；
`xl.dll` 为 6,727,680 bytes，SHA256
`3DDAC4E45907DD51F0AAEED32B6313F34A999074BD68CDD8C267D65C72C8C62E`。
编译器 VFS 532 项，32,419,576 → 4,604,831 bytes；运行时 VFS 115 项，
5,515,719 → 859,501 bytes。生产修改仅涉及共享错误运行时和其声明，未修改
IR/Cgen，也未放宽 class destroy 的准入规则。

默认准入本轮仍实际失败：531 候选、legacy 514、共同基线 XLIR 496、
18 缺口、0 执行异常、hash 稳定，531 项的两后端退出码与上轮逐项完全相同；
证据 `build/tmp/xlir_cleanup_first_failure_admission.json`。零缺口和成功数
下限未改动。模块最终化门也实际失败，在 `xlir_module_finalization` 的
lowered IR 阶段报 `XLIR class layout arena is malformed`，日志
`build/tmp/xlir_cleanup_first_failure_module.log`。这个 fixture 的 legacy
本身不通过，因此不能将“18 个共同基线缺口”解释为所有未完成语言问题的
总数，也不能把已通过的运行时边界门称为完整模块最终化通过。

下一阶段仍须把自动清理的 effect、错误 CFG、剩余清理、待返回所有者回滚
和原始异常优先级一起接入，并覆盖 frame/module 的全部终态。首个失败的
正确累积是该路径的前置合同，不是自动析构错误路径已完成的替代证明。

## 语言加固续批：原生 TAKE 分配失败时的分离与重入（2026-09-07）

本轮沿清理所有权继续检查。生产 AST 构建的 class 已统一为托管 xvalue
表示；旧裸句柄表示分支不是继续添加原生类语义的入口。实际在用的
`XVALUE_NATIVE_HANDLE_TAKE` 仍有回滚顺序缺陷：XRT 句柄外壳分配失败时，
生成 helper 先调用 destructor，再清空来源指针。析构既能观察到仍附着的
旧所有者，其重入写入的新所有者也会被后面的清空覆盖。

旧 DLL 上的新反例实际断言失败，日志
`build/tmp/xlir_native_take_reentry_before.log`。现在该 IR 原语的合同明确
要求在分配或回滚销毁之前先分离来源。机械 helper 先把旧值取到独立局部、
立即清空来源，再让 XRT 接管局部所有者；失败时只销毁仍由该局部拥有的
旧资源，不再于析构之后写来源槽位。成功、空句柄及 NULL 参数地址的原有
物理 ABI 保留。这次修改没有从 AST 或源名字重新推导析构目标，也没有
扩展旧裸句柄类分支；目标仍完全来自已验证的 TAKE `targetSymbol`。

新门 `check_xlir_native_take_reentry.ps1` 已接入函数 ABI suite、静态合同
及既有 native bind/take 迁移覆盖项。它从 Lowered IR 取得 destructor
symbol，生成测试绑定头，不硬编码 symbol 序号。最终版本实际通过，日志
`build/tmp/xlir_native_take_reentry_verified.log`：

- 严格 C/DLL 下 2,000 次真实分配失败、2,000 次成功；分别覆盖析构重入
  写入 NULL 和写入不同资源。计数验证 6,000 个工作负载原生所有者全部关闭，
  2,000 次受监控 XRT 分配释放平衡，无 live count/bytes 增长或非法释放。
- C-host JIT 和独立 AOT 各验证 2,000 次注入失败、2,000 次成功。该通道的
  shim 仅模拟 XRT 分配失败时保留传入所有者的合同，不改生成的 helper；
  实际分配注入与内存计数仍由严格 DLL 通道承担。
- 两次 C 生成逐字节一致。检查的是已验证 TAKE 原语的物理回滚行为，不
  把 C-host 测试冒充完整源语言构造失败或析构错误 CFG 的验证。

既有 native handle bind（lvalue/rvalue）、managed native lifecycle、
native nominal receiver、unused factory declaration、foreign error scope
和 XCall result failure 门在最终版本均通过。日志使用
`build/tmp/xlir_native_take_reentry_` 前缀，后缀分别为 `bind.log`、
`lifecycle.log`、`receiver.log`、`factory_decl.log`、`foreign.log`、
`xcall.log`。正常路径的类身份、factory field TAKE、方法 BORROW、显式
destroy、自动释放及上一批首错保护未由此出现已测回归。

完整 DLL/VFS 构建实际退出 0，日志
`build/tmp/xlir_native_take_reentry_build.log`。编译器 VFS 532 项，
32,419,947 → 4,604,904 bytes；运行时 VFS 115 项，5,515,719 → 859,501
bytes。`xlc.dll` 仍为 10,573,824 bytes，SHA256
`5AA612D62B4796AF072A47AECE7FEF0785F1D43A56F5DC732118F11BC900B87C`；
`xl.dll` 6,727,680 bytes，SHA256
`3D557B949A03794499DB9BAA706638B0DB4831C747F037B28473B2C088DF03CB`。
生产改动为 IR 合同注释及共享 Cgen helper 的机械所有权顺序。

核心 C 接线增为 448 个入口 fixture，package fixture 仍为 11；静态合同、
源码接线、batch 接线与迁移覆盖检查通过。清单仍为 118 个语义行、363 个
代表 fixture、417 个确定性候选、143 个两后端运行对照、56 个 XLIR-only
合同，不能将清单统计解释为全量执行成功。默认准入实际仍失败：531 候选、
legacy 514、共同基线 XLIR 496、18 缺口、0 执行异常、hash 稳定；531 项
两后端退出码与上轮逐项完全相同。证据
`build/tmp/xlir_native_take_reentry_admission.json`，门槛未放宽。

### 下一处已经复现的所有权缺口

补充探针 `build/tmp/xlir_native_factory_failure_probe.xl` 及其 `_host.c`
在同一最终 DLL 上编译成功、运行退出 1，日志
`build/tmp/xlir_native_factory_failure_probe.log` 明确记录
`caught=1 opened=1 closed=0`：原生工厂返回非空的已分配资源，但同时报告
失败；语言 catch 执行了，资源 destructor 却没有执行。探针宿主在记录
结果后自行回收遗漏的资源，不让诊断本身遗留泄漏。

这是 CALL 物理结果在 ERROR_TEST 失败边尚未进入任何所有者的缺口，与
上述 TAKE 的分配失败回滚不同。后续必须依据 CallTarget 的
`resultPhysicalAbi`、field 和 destroy identity，在 IR 中明确失败结果的
所有权及清理路由；不能通过 Cgen 猜工厂名、提前构造一个可能失败的包装
对象，或忽略原始异常来补救。还须验证 NULL 结果不销毁、非空 owning 结果
恰好销毁一次、borrowed 结果不误销毁、析构重入与次生错误优先级。这一探针
不在 531 个既有候选内，因此 18 个共同基线缺口不是所有剩余问题的总数。
自动析构错误 CFG 和全部 frame/module 终态仍未完成，长期目标保持完整。

## 原生工厂失败结果的显式回滚（2026-09-07）

本批修复上述非空失败结果泄漏。原探针重新生成、严格对应最终 DLL 执行后
退出 0，`build/tmp/xlir_native_factory_failure_probe.after.log` 记录
`caught=1 opened=1 closed=1`；上一批失败日志保留作前后对照。

### IR 所有权边界

新增 `FOREIGN_RESULT_ROLLBACK`，不是 Cgen 的工厂名称特判：

- 输入为失败 CALL 的物理原生结果；TypeId 与 destructor SymbolId 均来自
  已解析的 CallTarget/原生字段合同，不临时创建可能分配失败的语言包装对象。
- verifier 证明 ERROR_TEST 两边的独占所有权分区：失败块入口恰好一次
  rollback，成功块恰好一次 native-handle TAKE；拒绝额外入口、错误目标、
  重复消费、借用结果冒充 owning 结果以及其他原始指针用途。
- rollback 先取走原始诊断、分离并清空原生结果，再调用析构；NULL 不析构。
  析构在独立的共享运行时错误作用域中执行，次生诊断被释放，原始诊断被
  恢复。原始诊断可为 NULL，失败事实仍由既有 CFG/结果载体保留。
- 每次原生工厂调用前清空 owning 物理返回槽，包含循环重新执行。作用域
  建立失败时跳过 native CALL，失败边因此不会读取未初始化或旧指针。
- 同步、async worker 和 generator worker 共用同一个机械发射函数。
  已验证 NO_RAISE 的内部工厂不要求虚构 ERROR_TEST；已验证内部 NO_RAISE
  析构目标与普通 MAY_RAISE extern 析构目标都使用同一隔离回滚语义。

非空结果意味着该执行上下文已经成功执行了工厂的 ForeignCallBegin/End。
回滚重新绑定同一已初始化 FLS/TLS 槽不应再需要新存储；若 OS 仍拒绝绑定，
与现有 ErrorScopeEnd 一样视为运行时不变量失效并 abort，不能跳过析构或
在外层作用域中裸执行析构。这不是对普通析构语言错误的 abort 策略。

### 实际验证与基线

新增 `tools/check_xlir_foreign_result_rollback.ps1` 并接入核心函数 ABI suite。
`build/tmp/xlir_foreign_result_rollback_verified.log` 实际退出 0：

- 严格 C/DLL：同步、async、generator 共 14,800 个案例；包含 4,000 次真实
  错误对象分配失败及 400 次注入的 scope-entry 失败；41,300 次 XRT 分配
  全部平衡，live count/bytes 不变，无新增 double-free、invalid-free、UAF。
- C-host JIT 和独立 AOT 各 10,800 个案例：NULL/non-NULL 结果、NULL/非 NULL
  原始诊断、析构二次失败、native 消耗 callback 诊断、析构重入、借用
  结果、成功包装及清理均覆盖。这里未把 C-host 测试称作语言 JIT 对照。
- 包含标准库展开的真实前端/IR/Cgen 测试拒绝 18 个缺失、重复、错误边、
  错误 destructor 和 borrowed-pointer 冒用变异，且拒绝发生在任何 C 输出前；
  合法模块实际生成 C，CLI 重复生成逐字节一致。

native bind、native TAKE 重入、managed native lifecycle、nominal receiver、
unused native factory declaration、foreign error scope、XCall result failure、
pending return commit
回归均通过。全量准入实际仍退出 1：532 候选，legacy 515，共同基线 XLIR
497，18 缺口、0 执行异常、CompilerStable=true。原有 531 项两后端退出码
逐项完全一致，新增 fixture 两后端均成功。门槛未降低，18 个缺口仍是
10 TCP Future owning layout、6 class destroy layout、1 native setter、1 xid。
证据为 `build/tmp/xlir_foreign_result_rollback_admission.json`。

完整 DLL/VFS 构建实际退出 0：编译器 532 项，32,430,724 → 4,606,861 bytes；
运行时 115 项，5,515,719 → 859,501 bytes。`xlc.dll` 10,578,432 bytes，
SHA256 `A8DE222E1AAEA38039907739005FB3587CADD18F20AA5E4E5A8988BFEB73B756`；
`xl.dll` 6,730,240 bytes，SHA256
`E77D3F9DF5C8EC8FD067588EC60A9B387FAAC828079458E353ED2BC6FD9ECFF5`。
相对上一批分别增加 4,608 / 2,560 bytes，不代表长期迁移总体体积结论。

### 新确认的边界缺口，不计作修复

`build/test/xlir_foreign_result_rollback.exe --export-probe` 实际退出 1；
`build/tmp/xlir_foreign_result_export_probe.log` 记录
`result=0 outer.ok=1 opened=1 closed=1`。资源已经正确回收，但原始诊断为
NULL 时，当前标量 C export 包装器丢弃内部结果的 `ok=false`，外层共享
错误作用域无法观察失败。该探针作为独立红灯保留，不能把绿色 rollback
子门当作 export 边界已完成；本批未被 catch 处理的路径直接核对真实语言
结果载体及其原始诊断，没有用 C export 返回值代替失败状态验证。

下一步需在 IR 的 export/foreign 边界合同中落实诊断分配无关的失败传播。
成功包装期间的分配失败、自动析构的完整错误 CFG、pending return 回滚和
所有 frame/module 终态仍须继续压实；长期目标保持 active。

## C 导出入口的完整错误事务（2026-09-07）

本批修复上一节的 export 红灯，而不是把 NULL 诊断视为成功。最终 DLL 上
`build/test/xlir_foreign_result_rollback.exe --export-probe` 已退出 0：
`result=0 outer.ok=0 opened=1 closed=1`，失败标志和资源回收均可被 C 调用者
观察。该探针已接入原生工厂回滚门；旧失败日志保留，不改写历史结论。

### 明确的 IR 合同与机械发射

- Export 新增 `errorAbi=SHARED_SCOPE_TRANSACTION`，覆盖模块初始化、参数
  callback 包装、函数体、参数清理和待提交结果。`bodyErrorAbi` 单独记录
  私有 CallTarget 的精确载体，必须在调用图错误效果收敛后取得并经 verifier
  校验。函数体 NO_RAISE 不等于整个公开入口不会失败。
- 函数体显式失败立即捕获第一错误，再清理参数。失败事实独立于诊断分配；
  后续清理的诊断不得覆盖第一错误，包括第一诊断为 NULL 的情况。所有已获取
  callback 包装均逆序分离槽位后释放，不因前一个清理报错而跳过。
- 待返回的 owning 值在公开入口完成清理前仍属于入口。清理失败时，根据
  verifier 已验证的 TypeId release ABI 回滚字符串、xvalue 或托管 product，
  在独立错误作用域中释放并丢弃次生诊断；成功后才向 C 调用者提交。托管
  product drop 的生成依赖现已包含 export 结果根，不要求函数体虚构 RELEASE。
- 传入语言的原始 C callback 在每次调用时建立独立错误作用域。callback
  重入一个失败 export 后，即使 native 消耗了诊断，适配器仍返回明确的失败
  bool，供语言 catch/传播 CFG 处理；不会越过合法 catch 直接污染外层入口。
  callback 返回的 owning 原生值在失败或无人接收时也按合同回收。
- 公共 C/C++ 头提供共享错误作用域声明。既有函数签名保留：失败返回零值或
  void；需要检查失败的 C 调用者必须用 ForeignCallBegin/End 包围调用，检查
  `ok` 并消费返回的 owned error，不能仅检查错误指针。成功入口保留调用前
  的诊断。自动命名的语言 export 支持托管返回值，显式 raw-C export 对托管
  返回值的既有语义限制没有放宽。

### 实际测试结果

新增 `tools/check_xlir_export_transaction.ps1`，接入核心函数 ABI suite；
`build/tmp/xlir_export_transaction_verified.log` 对最终 DLL 实际退出 0：

- 13 个公开入口涵盖标量、void、字符串、xvalue、托管多返回值、callback
  重入/catch、第一或第二参数包装失败、参数清理二次失败和模块初始化拒绝。
- 严格 C/DLL 运行 4,200 个案例，含 1,100 次真实错误对象分配失败；6,500 次
  XRT 分配全部平衡，live count/bytes 不变，无新增重复/非法释放或 UAF。
- C-host JIT 和独立 AOT 各执行 3,300 个案例；这不是语言 JIT 差分的替代。
  生成 C 重复输出逐字节一致，公开头独立 C 链接、C++11 严格语法均通过。
- 真实 parse/stdlib/semantic/lowering 流程下，26 项缺失或不匹配的 export
  事务/函数体错误 ABI 变异由 verifier、Cgen 和 header emitter 在任何输出
  之前拒绝。没有用手工放宽的模块结构代替正常前端结果。

原生工厂回滚门重新通过：14,800 个案例、4,000 次真实诊断分配失败、
41,300 次平衡分配，以及两条各 10,800 个案例的 C-host JIT/AOT 路径；
18 个 IR 所有权变异仍被拒绝。记录为
`build/tmp/xlir_export_transaction_factory_rollback.log`。

export class/method/product、primitive-width、foreign error scope、XCall
result failure 和 return-commit 回归通过。class-method 的两处 IR 文本快照
已增加明确的错误 ABI 字段，零值返回快照与统一发射格式同步。公开 callback
独立头/分离 C 链接和原有 7 项合同变异单独通过，证据为
`build/tmp/xlir_export_transaction_callback_public_subset.log`；完整 callback
门仍在 `xlir_export_callback_lifetime.xl` 报既有 class layout 错误，没有
删除该 fixture、跳过失败或将子集成功称为全门通过。

### 构建、准入与未完成项

完整 DLL/VFS 构建退出 0；编译器 532 项，32,437,069 → 4,608,539 bytes；
运行时 115 项，5,515,719 → 859,501 bytes。`xlc.dll` 为 10,582,528 bytes，
SHA256 `B4396A0C963B243CA39E61639910DC993E3C2AD7C2C6AEC46F16FB40F127B5E6`；
`xl.dll` 为 6,732,800 bytes，SHA256
`DF9039A5629F5A8AD9C4B8E2457F1C87DA78B0F95ABB051F6A775414A4D6BC6E`。
相对上批分别增加 4,096 / 2,560 bytes，不作长期总体体积结论。

默认准入仍退出 1，门槛保持不变：533 候选，legacy 516，共同基线 XLIR
498，18 缺口、0 执行异常、CompilerStable=true；原有 532 项的两后端退出码
逐项不变，新增 export transaction fixture 两后端均成功。18 个缺口仍是
10 TCP Future owning layout、6 class destroy layout、1 native setter、1 xid。
证据为 `build/tmp/xlir_export_transaction_admission.json`，这些准入数字不能
换算成整个长期任务的完成百分比。

静态 IR 合同、源码接线、batch 接线、编码与 Markdown 链接检查通过；核心
C 接线为 455 个入口、11 个 package fixture。迁移清单为 118 行、363 个
代表 fixture、419 个确定性候选、143 个两后端运行对照、56 个 XLIR-only
合同；清单统计不代表这些候选已全部运行成功。

本批只闭合公开入口及传入 callback 的事务。语言内部自动析构的完整效果
分析与清理 CFG、待返回所有权在自动析构失败时的回滚、原生工厂成功后的
包装分配失败、class/frame/module 的全部终态仍须继续完成。不能因为公开
入口已有回滚，就宣称这些内部路径已完整；长期目标保持 active。

## 语言异常退出的显式清理区间（2026-09-07）

本批进入语言内部的异常清理，不移除 class destroy 的 NO_RAISE 防线。
首先在原 DLL 生成的程序中复现两项实际错误：

- `build/tmp/xlir_error_unwind.before.log`：原始错误 101，两个资源按 2→1
  释放，但外层观察为 `value=0 ok=0 error=202`。原本应由语言 catch 处理的
  主错误被析构次生错误干扰；资源释放次数正确不等于异常语义正确。
- `build/tmp/xlir_error_unwind_call_cleanup.before.log`：callee 内的清理已受
  保护后，caller 在 ERROR_TEST 前释放参数临时值，仍把原始错误改成 303。
  该日志来自保留的旧生成代码可执行文件，不是修改最终生成 C 的模拟结果。

### IR 与控制流

新增 `ERROR_UNWIND_BEGIN/END`。`errorScope` 是 BEGIN 指令的一基身份，区间
内每条清理指令均明确记录所属区间；它不是源符号，也不是可以溢出到 frame
的 SSA 值。BEGIN 将已知失败的原始诊断移入共享错误作用域，独立锁定失败
事实；END 丢弃次生错误并恢复原始 owned 诊断，然后走已经解析的 catch
或 THROW 边。NULL 诊断始终表示“失败但没有错误对象”，不会被改成成功。

verifier 验证同一函数内的唯一配对、每条指令的区间归属和各 CFG 路径的
状态一致性；区间不能嵌套、回跳、越过 await/yield、漏掉 END 或携带任意
运行时调用/错误清除等非清理工作。END 后只能转入既定异常目标或 THROW。
每次函数/worker activation 只需一个可复用的栈槽，不能作为挂起状态保存。
普通函数、async continuation、generator resume 使用同一个机械发射函数。

异常转移现在在释放 evaluation owners、局部/捕获 cell、循环资源及 callee
参数所有者之前开始区间；exceptional-finally 和默认参数 adapter 的异常
退出也接入同一合同。正常 scope exit 的首次析构失败尚不属于这个“已经
失败”的区间，不能把二者混为同一种入口条件。

普通调用 lowering 进一步改为 CALL→ERROR_TEST，再在成功和失败两个后继
中分别逆序释放参数临时值。失败后继先回滚 owning native factory 的物理
返回值，再进入异常清理区间；callee 失败与参数清理不能继续共享一个未经
分流的正常块。该变化在 IR 中完成，没有在 Cgen 中追查 callee 源码或临时
变量名字。其他独立调用/转换 lowering 家族仍需逐项审计，不能仅凭这一
普通调用路径就宣称所有物理调用边界已经闭合。

### 回归覆盖与边界

新增 `test/cases/xlir_error_unwind.xl` 和
`tools/check_xlir_error_unwind.ps1`，接入核心函数 ABI suite。源码级完整
前端/IR/Cgen 测试已验证 32 个区间、49 条清理成员；262 项缺失/错配/错误
归属、插入非清理错误操作和挂起指令归属变异在任何 C/header 输出前被拒绝。

源码级严格运行通过 9,000 个案例、3,000 次真实诊断分配失败，201,600 次
XRT 分配全部平衡。六条路径为：局部 catch、未捕获传播、资源跨 await 的
async、资源跨 yield 的 generator、callee 参数加 caller 临时值，以及
finally 抛错后待提交返回值的回滚。覆盖原始/次生 NULL 诊断、native 消耗
次生诊断、逆序释放和析构重入；重入成功必须恢复原有诊断指针，而不是
无条件把外层诊断清空。

该结果不等于自动析构完整支持：正常释放首发错误的 Type/CallTarget 效果
收敛与失败 CFG、类析构调用链的 MAY_RAISE carrier、其他调用家族的参数
清理、module/frame 的全部终态仍需继续。建立 OS 错误上下文隔离失败仍采用
运行时 fail-fast，不把普通语言异常或诊断对象分配失败视为 abort 条件。
全量构建与最终 DLL 复验结果另记如下，长期目标保持 active。

### 最终 DLL 复验

`build/tmp/xlir_error_unwind_call_split_build.log` 完整构建退出 0。编译器
VFS 532 项，32,450,850 → 4,610,706 bytes；运行时 VFS 115 项，
5,515,719 → 859,501 bytes。`xlc.dll` 10,588,672 bytes，SHA256
`0E75F55C472FA6D6C9A1F7D6ED0CF9E75D60BAC585860D457E58B7D6104322DD`；
`xl.dll` 6,736,384 bytes，SHA256
`169FCA0722F4BAA675F32953B0B1487BA5897B4D23CC9891EA431F5291E912CA`。
相对上一批分别增加 6,144 / 3,584 bytes，不能作为长期总体体积结论。

`build/tmp/xlir_error_unwind_verified.log` 对最终 DLL 实际退出 0：严格运行
9,000 个案例、3,000 次真实诊断分配失败、201,600 次平衡分配；C-host JIT
和独立 AOT 各 6,000 个案例；32 个区间、49 条成员的 262 项合同变异通过。
故障注入使用真实原生资源的 create/destroy 和共享错误 API，不重写生成 C
的控制流。CLI 重复生成 C 逐字节一致，原生资源释放顺序、原始错误优先级
和重入调用前的诊断状态均有运行断言。

原生工厂失败回滚、export transaction、return commit、scope exit plan、
evaluation lifetime、async multi-await machine、generator machine、foreign
error scope、named function adapter 和 try/finally pending exit 回归在该版本
通过。它们各自的日志位于
`build/tmp/xlir_error_unwind_regression_*.log`，不是用清单统计替代运行结果。

全量默认准入仍退出 1，门槛未放宽：534 候选、legacy 517、共同基线 XLIR
499、18 缺口、0 执行异常、CompilerStable=true。原有 533 项的两后端退出码
逐项不变，新增 fixture 两后端均成功；证据为
`build/tmp/xlir_error_unwind_admission.json`。核心 C 接线为 458 个入口和
11 个 package fixture；迁移清单为 118 行、363 个代表 fixture、420 个
确定性候选、143 个两后端运行对照、56 个 XLIR-only 合同。

### 新复现的属性调用边界，仍是红灯

最终 DLL 的 `build/tmp/xlir_property_cleanup_probe.xl` 及对应 C 宿主编译
成功，运行退出 1。`build/tmp/xlir_property_cleanup_probe.log` 记录
`ok=0 error=202 opened=1 closed=1`：getter 的原始错误是 101，但临时
receiver 的析构把它替换成 202。资源已恰好回收一次，不等于错误载体正确。

这条路径由独立的 `xl6IrLowerResolvedSyntaxCall` 处理，尚未采用普通 CALL
本批已验证的“先检查结果，再在两个后继内清理”顺序。探针保留为后续
统一调用清理入口的证据；不能从普通 CALL 门通过推导 property/operator/
protocol 等路径已全部闭合。它不在 534 个准入候选中，因此原来的 18 项
缺口仍不代表所有剩余语言语义问题。

## 统一调用收尾与 native setter 失败路径（2026-09-07）

### 收敛的是 lowering 合同，不是生成 C 的补丁

`xl6IrLowerCompleteCall` 消费调用目标、结果 carrier、异常退出计划，以及
`XL6_IrLowerCallCleanup` 的临时 storage、原生 TAKE 析构、闭包自身 owner、
构造失败专属 owner。普通调用、resolved syntax、property getter/setter、
closure、constructor 和受限的 base constructor 共用这个入口。

MAY_RAISE 调用先生成同块相邻的 `ERROR_TEST`，再分别生成失败和成功清理。
失败侧在显式 unwind 区间内清理参数、receiver 或 closure capture，并完成
构造失败的对象回滚，然后进入选定的异常处理路径；成功侧只清理调用者
持有的临时 owner。已有 foreign factory 的原始结果回滚仍是失败入口操作，
不与语言 wrapper 的成功 TAKE 混淆。Cgen 本批不增加语义推断或模板修补。

验证器现在对 CALL / CLOSURE_CALL / MAY_RAISE ASYNC_START 强制同块相邻的
结果检查，并要求 ERROR_TEST 对应紧邻的实际调用，拒绝清理后再测结果、
遗漏检查和重测旧 carrier。错误区间的成员及控制流验证继续生效。

本轮复查更正上一节的函数定位：普通声明 getter 的探针先进入
`xl6IrLowerResolvedPropertyGet`，并非 `xl6IrLowerResolvedSyntaxCall`；两者
都曾存在 cleanup-before-test，本批均已接入统一入口。

native setter 不再被统一拒绝为 NO_RAISE：它沿用既有明确的
MAY_RAISE / FOREIGN_SCOPE CallTarget，产生 void carrier 和传播 CFG。
`check_xlir_resolved_property_call_abi.ps1` 相应要求精确的 foreign-scope
ABI、紧邻结果检查，以及成功侧一次 release、失败侧受 unwind 保护的一次
release；未删掉原生字段投影、索引类型、直接调用、确定性 C 或旧后端运行
对照要求。

### 分阶段运行证据

原始独立 getter 探针的红灯仍保留在上一节。扩展 fixture 对旧 DLL 的
`build/tmp/xlir_call_completion_before.log` 另记录 closure 参数释放顺序
`order=12`，预期为 21；此时原始错误 101 尚在，说明顺序和错误优先级必须
分别验证，不能只检查错误码。

第一版完整 DLL 在 `xlir_call_completion_regression_error_unwind.log` 中通过
12 类路径的 18,000 案例、6,000 次真实诊断分配失败和 388,800 次平衡分配；
C-host JIT / AOT 各 12,000。随后 native setter 接入的源码直编版本通过
14 类路径的 21,000 案例、7,000 次真实诊断分配失败、425,200 次平衡分配，
60 个区间、80 条清理成员和 521 项变异，其中 57 项直接破坏调用结果检查。
最终 DLL 复验结果须另记，不能把源码直编当成已发布 DLL 证据。

覆盖包括 getter 临时 receiver、运算符的双参数、闭包参数、携带资源的
临时闭包自身、构造参数与已初始化字段回滚、普通 setter、索引 setter，
以及清理后进入 catch。既有同步/async/generator、pending return、NULL
诊断、二次错误、原生侧取走诊断和重入矩阵继续运行，没有改写生成 C 的 CFG。

第一版全量准入为 534 / legacy 516 / 共同基线 XLIR 498 / 18 缺口，
0 执行异常且编译器稳定。对照上一版 534 项，仅本批扩展的
`xlir_error_unwind.xl` 从 legacy 成功变为失败：旧 HIR 不支持新增的嵌套
constructor expression；XLIR 仍成功。未改动案例无新增退化。没有缩减
fixture 或调整准入门槛来维持旧数字，完整记录在
`build/tmp/xlir_call_completion_admission.json`。

### 尚未宣称完成的边界

正常清理首发错误的效果收敛、pending result 回滚、类析构 MAY_RAISE 调用链、
module/frame 所有终态和原有准入缺口仍需继续。协议/虚调用现有的错误 ABI
限制没有被绕过；本批不会从直接调用路径的通过推导所有动态调用都已完成。
长期目标保持 active。

### 本批最终 DLL 与剩余清理错误证据

`build/tmp/xlir_call_completion_setter_build.log` 完整构建退出 0。编译器
VFS 532 项、32,449,718 → 4,611,233 bytes；运行时 VFS 115 项、
5,515,719 → 859,501 bytes。`xlc.dll` 10,589,184 bytes，SHA256
`FF4ED4FCC7416680A061D11E2642CDC8484BFC3C0D3E1CFEA2B5EAE6D7DBAEBB`；
`xl.dll` 6,736,896 bytes，SHA256
`F9D96437933F95C66B6C0F98A2FA22E4547334B5D5C06FA80B045FB5E5C0F590`。
相对上批最终 DLL 各增加 512 bytes，不代表长期总规模结论。

`build/tmp/xlir_call_completion_final_regression_error_unwind.log` 对这些
最终 DLL 实际通过 21,000 案例、7,000 次真实诊断分配失败、425,200 次平衡
分配；C-host JIT 和独立 AOT 各 14,000；60 个区间、80 条成员的 521 项
变异均在输出前拒绝，其中 57 项破坏调用结果检查。native property 专项
门通过严格 C、旧后端运行对照以及普通、单索引、多索引 getter/setter。

最终默认准入证据为 `build/tmp/xlir_call_completion_final_admission.json`：
534 候选、legacy 516、共同基线 XLIR 499、17 缺口、0 执行异常、
CompilerStable=true，编译器 SHA256 与上文相同。仍按原门槛退出 1。
相比上一批 534 项，`cgen_native_class_property.xl` 的 XLIR 从失败变为
成功；扩展的 `xlir_error_unwind.xl` 仅 legacy 从成功变为失败，原因见上文。
另外 532 项两后端退出码均不变。原有缺口实际减少 1 项，不以共同基线
分母变化冒充覆盖率提升；剩余 10 项 TCP Future owning layout、6 项 class
destroy layout 和 1 项 xid 仍未完成。

原始 getter 探针本批严格 C 编译及运行均退出 0，
`build/tmp/xlir_property_cleanup_probe.final.log` 为
`ok=0 error=101 opened=1 closed=1`：外层正确观察原始失败，不再被 202 覆盖。

独立的 `build/tmp/xlir_cleanup_first_failure_probe.xl` 则故意不在 getter
中报错，只有 receiver 析构发布 202。期望所在语言 catch 返回 202 且外层
成功；最终 DLL 的严格 C 宿主运行退出 1，日志
`build/tmp/xlir_cleanup_first_failure_probe.log` 为
`ok=0 value=0 error=202 opened=1 closed=1`。资源恰好释放，错误也没有丢失，
但它绕过了应接收它的语言 catch。这与“已有失败时保留原始错误”是两个
独立合同，后者通过不能证明前者通过。

下一步需建立类型释放效果与 CallTarget 效果的共同收敛，并为正常释放、
返回提交、字段替换及 module/frame 清理生成完整失败路径。不能只在
`xl6IrLowerCompleteCall` 的成功侧追加一个 TLS 检查，也不能提前解禁类析构
MAY_RAISE；本探针保留为后续该合同的红灯证据，长期目标未完成。

最终串行回归批次退出 0，共 17 项：resolved property、error unwind、
foreign result rollback、export transaction、return commit、scope exit plan、
evaluation lifetime、internal property、property compound assignment、base
constructor、explicit constructor field initializer、closure argument conversion、
named function adapter、async multi-await machine、generator machine、try/finally
pending exit、foreign error scope。每项实际日志均保留在
`build/tmp/xlir_call_completion_final_regression_*.log`。末次检查两份 DLL
哈希不变；合同、迁移清单、source/batch 接线、5771 项编码和 329 项 Markdown
检查通过，核心 C fixture 接线仍为 458 / 11。上述清单规模不是全量运行次数。

## 正常调用清理的首次失败与待提交结果（2026-09-07）

本批在上一批的异常展开合同之外，增加正常调用收尾的失败合同：调用本身
成功不代表表达式已成功，临时实参、临时接收者或临时闭包释放也可能首次
失败。这个失败必须进入当前语言 catch，并在所有清理完成后提交结果。

实现采用显式 XLIR 控制流，而不是在 Cgen 追加 TLS 判断：

- `xl6IrTypeReleaseEffect` 从规范类型得到 NO_RAISE / MAY_RAISE；字符串、
  error 和平凡值不调用用户析构，擦除值、闭包及 continuation owner 保守
  视为可能失败，product 合并各元素效果。无效或递归畸形 product 不可用。
- `CLEANUP_BEGIN` / `CLEANUP_END` 表示不可挂起的正常清理区间，所有成员
  携带同一身份。END 产生独占的 bool outcome，并紧邻 `ERROR_TEST`；失败
  即使没有诊断对象也能进入明确的失败后继。验证器禁止嵌套、跨激活边界
  和非清理工作。同步、async、generator 的 Cgen 仅渲染这个已验证合同。
- 统一调用收尾在正常边释放完整实参序列后再测试；托管结果先落入待提交
  storage，失败时回滚，成功后才 LOAD/MOVE。product 使用已有
  `PRODUCT_STORE_MOVE`，不放宽聚合所有权验证。native factory 的物理结果
  在成功边先转成语言 owner，后续实参清理失败时释放该 owner。
- 构造出的对象在 create 调用前有显式 owner，create 不抛错也不能跳过
  实参清理失败的回滚。复合赋值运算和 length 协议调用也使用统一收尾；
  复合赋值的旧值保持独立 owner 穿过 RHS 求值，属性事务的访问临时值在
  内层运算失败边可见。CallTarget 在 CFG 建立前传播这些潜在释放效果。

真实资源故障测试扩展到正常 getter、双实参、临时闭包及 capture、待提交
返回对象、create、async 恢复、generator 恢复、复合赋值运算失败和正常
清理失败。保留原来的异常展开、重入、无诊断对象与诊断分配失败矩阵。
源代码驱动生成 C 的先行证据为
`build/tmp/xlir_normal_cleanup_fresh_runtime.log`：36,000 案例、12,000 次
真实诊断分配失败、724,500 次平衡分配。此处是源代码驱动证据，最终 DLL
及 JIT/AOT 的验证单列，不能混为同一构建。

上一批独立红灯探针现为
`build/tmp/xlir_cleanup_first_failure_fixed.log`：
`ok=1 value=202 error=0 opened=1 closed=1`。这证明“只在正常 receiver
析构首次失败”已进入语言 catch，不只是保住外层 TLS 错误。

仍未完成：所有 scope / parameter / field / module / frame 释放消费者
的统一失败路径、类 MAY_RAISE 析构载体、完整效果收敛、部分实参求值和
字段初始化失败的构造回滚。本批不能解禁类析构 guard，也不能宣称全部
正常清理问题已经解决。保守效果传播使两个原本可生成的 module-finalization
CLI 案例进入类析构效果拒绝边界；这是实际覆盖退步，不删案例、不降门槛，
需要后续打通类析构和 module-finalization 合同才能恢复。

### 最终 DLL 与准入证据

完整 `build_dll.bat` 退出 0，日志为
`build/tmp/xlir_normal_cleanup_build.log`。compiler VFS 532 资源，
32,464,673 → 4,612,939 字节；runtime VFS 115 资源，
5,515,719 → 859,501 字节。`release/xlc.dll` 为 10,593,792 字节，
`release/xl.dll` 为 6,739,456 字节，比上一批分别增加 4,608 / 2,560 字节。

- xlc SHA256：`4D57FB1F51C2B45398A1A85E81899F8AEC0DCDA3C5DA37FA708945B91D772392`
- xl SHA256：`4B03CE4669944B8EEE81081935B676C0E8CC74CE2BB5C0F2AD1D1014B1DA1480`

最终主门 `build/tmp/xlir_normal_cleanup_final_regression_error_unwind.log`
通过 36,000 案例、12,000 次真实诊断分配失败、724,500 次平衡分配；
C-host JIT / 独立 AOT 各 24,000。151 个区间、190 条清理成员的 1,353 项
变异在任何 C/header 输出前拒绝，含 83 项调用结果检查变异；其中 32 个
是正常清理区间。独立探针也以最终 CLI 和 DLL 重新编译运行，日志
`build/tmp/xlir_cleanup_first_failure_final.log` 仍为
`ok=1 value=202 error=0 opened=1 closed=1`。

最终准入 `build/tmp/xlir_normal_cleanup_final_admission.json` 为 534 候选、
legacy 516、共同基线 XLIR 497、19 缺口、0 执行异常，CompilerStable=true，
编译器哈希与上文一致。相对上一批逐项对比，只有
`xlir_module_finalization_cli.xl` 与 `xlir_module_finalization_cli_error.xl`
的 XLIR 成功变为类析构布局拒绝，其余 532 项两后端退出码均不变。
缺口为 10 TCP Future owning layout、8 class destroy layout、1 xid；
原始门槛 531 / 514 / 514 / 0 未改，仍退出 1。

17 项串行专项中 16 项完整通过：error unwind、resolved property、foreign
result rollback、export transaction、return commit、scope exit plan、evaluation
lifetime、internal property、property compound、base constructor、explicit
constructor field initializer、closure argument conversion、named function
adapter、async multi-await、generator machine、try/finally pending exit。日志
为 `build/tmp/xlir_normal_cleanup_final_regression_*.log`。具名函数门首次的
无目标 ERROR_TEST 总数断言把正常清理 outcome 混入闭包 outcome；现按具体
闭包 CALL 的结果 SSA 校验唯一且紧邻的 ERROR_TEST，保留独立清理检查，
重跑完整专项通过。首次失败日志另存
`build/tmp/xlir_normal_cleanup_named_function_adapter_initial.log`。

foreign error scope 未整体通过，不能并入上述 16 项。共享运行时事务、
first-missing、跨 DLL、JIT 和独立 GCC 部分通过，随后 Windows 于
2026-09-07 07:15（本地时间）拦截并移除了
`build/test/xlir_foreign_raw_callback_error.foreign-scope.aot.exe`，AOT 运行
尚未验证。只读状态查询显示实时保护、行为监控、防篡改均为开启；未修改
安全设置、未重命名程序规避拦截。该门后续独立 callback interop 已单独
编译运行通过，日志 `build/tmp/xlir_normal_cleanup_foreign_interop_tail.log`，
但不据此把整门改判为成功。需恢复被拦截文件后补齐该门。

合同、迁移清单、source/batch 接线、核心 C fixture 接线检查通过；规模仍为
118 语义行 / 363 代表案例 / 420 确定性候选 / 143 差分 / 56 XLIR-only，
source 接线 286 / 49 / 236 / 6 / 2，核心 C fixture 为 458 / 11。
编码 5771 文件、Markdown 329 文件检查通过。长期目标继续保持未完成。

## 2026-09-07：模块清理的显式结果与宿主生命周期边界

本批推进语言本身的资源与错误语义，不放宽标准库或类析构准入。此前
模块 finalizer 的 `ERROR_TAKE → release → ERROR_SET_TAKE` 会在正常卸载时
丢弃首次清理错误；宿主仅查询诊断指针，又会把无诊断对象的失败误判为成功。
这两处现在使用同一套诊断独立的结果合同。

### XLIR 与固定 ABI

- 有全局 owner 的 MODULE_FINI 为 MAY_RAISE，返回 void 结果载体；无 owner
  的模块仍为 NO_RAISE、单 RETURN，不因空 finalizer 引入清理依赖。
- `CLEANUP_INHERIT_BEGIN` 继承正在退出的首次失败，包括已消费 TLS 诊断、
  NULL 诊断和已失败的外层作用域。逆序释放每个 owner 后必须有
  `CLEANUP_CAPTURE` 检查点，最后 `CLEANUP_END → ERROR_TEST → THROW/RETURN`。
  生命周期阶段和 owner 次序由 IR 及验证器约束，不由 Cgen 查询 AST。
- raw native global 的计划先 LOAD/NULL/STORE 脱离全局，再做 NULL_TEST 和
  条件析构。验证器检查完整次序、分支目标和每个检查点；本批真实资源运行
  矩阵覆盖的是 xvalue native handle 及嵌套容器，不能据此声称 raw global
  七指令分支已有独立运行覆盖。
- 注册的 native handle drop 使用共享 `xl6ErrorScopeBeginCleanup` 边界。
  单次容器释放内的多个 native drop 也必须保留首次失败并继续释放其余 owner。
- Cgen 根据 function effect 生成 finalizer 结果类型，固定适配器只承担 ABI
  转换。`xl_script_main_checked(arg, &exit_code)` 将语言成功标志与业务退出码
  分离，`xl_script_module_unit_checked()` 将卸载成功标志与诊断指针分离。
  原 `xlang_main` 和 void unit 符号保留；宿主优先使用 checked 符号，兼容旧 C。
- DLL API 与独立 main 在入口结束后的参数和模块清理间持有同一首次错误
  作用域；初始化后的宿主参数清理失败也会回滚未提交的激活。init 自身失败
  的 finalizer 使用 known-failure 边界，避免 NULL 原始错误被次生诊断替换。

### 最终产物与故障证据

完整构建日志 `build/tmp/xlir_module_cleanup_checked_build.log` 退出 0。
compiler VFS 532 资源，32,480,423 → 4,615,476 字节；runtime VFS 115 资源，
5,515,719 → 859,501 字节。

- `release/xlc.dll`：10,601,472 字节，SHA256
  `DC6E69C1CECA6342329EC12D1D42164C7C67CF92A28A70E4EA5C370BF26FD3CB`。
- `release/xl.dll`：6,745,088 字节，SHA256
  `5FED28F544223361C13A01DDAEBA2B1008A2FBCA60E6A661E9234AAFA156BE57`。
- 相对上一批分别增加 7,680 / 5,632 字节；本批不是代码体积缩减批次。

新增主门 `build/tmp/xlir_module_cleanup_outcome_verified_gate.log` 完整通过：
8,800 案例、6,800 次真实诊断分配失败、188,400 次平衡分配；C-host JIT 和
独立 AOT 各 5,400 案例；9 项 finalizer 变异在输出 C 前拒绝；生成 C 确定性
和严格 GCC 编译通过。覆盖嵌套 native owner、部分初始化、NULL 诊断、原始
诊断已被消费、重入、清理幂等、重新初始化，以及无外层 scope 的 checked unit。

同一门还通过 72 个实际 DLL API run/activate/unload/retry 场景，以及 16 个
实际语言入口 JIT/AOT 场景。故障注入必须在 runtime pool 启动前开启：真实
分配失败在 strict C-host 与 DLL API 中测试，CLI/AOT 语言入口测试显式
NULL 失败，不把后者宣称为真实 OOM 注入。业务退出码 1 仍是成功执行结果；
真正失败即使无错误对象也不得变成成功。初始错误为 NULL 时，后续 cleanup
诊断不得出现在最终报告里。

初次验证曾发现 finalizer void 结果类型的依赖遗漏，已在完整重建前修复。
fixture 还修正了 runtime pool 启动后才开启内存调试的无效测试方式；上述
最终主门是修正后的重新执行结果，不借用早期失败运行。

准入报告 `build/tmp/xlir_module_cleanup_checked_admission.json`：535 候选、
legacy 517、共同基线 XLIR 498、19 缺口、0 执行异常，CompilerStable=true，
哈希与上文相同。新增 1 个两后端成功案例；原有 534 个案例的两后端退出码
逐项均不变。缺口仍为 10 TCP Future owning layout、8 class destroy layout、
1 xid。531 / 514 / 514 / 0 原门槛不变，准入仍退出 1。

用户恢复误报文件后，`build/tmp/xlir_module_cleanup_regression_foreign_error_scope.log`
完整通过，包含此前未执行的 raw callback 独立 AOT；本批未修改 Defender
设置。`build/tmp/xlir_module_cleanup_regression_error_unwind.log` 通过 36,000
案例、12,000 次真实诊断分配失败、724,500 次平衡分配，JIT/AOT 各 24,000，
以及 151 区间 / 190 成员 / 1,353 变异 / 83 调用结果变异 / 32 正常清理区间。
正常清理门按 begin 身份匹配 END，不把模块继承清理区间误算为普通调用区间。

### 尚未完成

类 MAY_RAISE 析构仍未解禁，完整 module-finalization 类案例不能因 native
模块新门通过而改判为通过。下一步必须定义有序类析构调用的结果消费合同，
并贯通所有 lexical/parameter/field/frame 清理消费者和效果收敛。构造部分
初始化、native wrapper 分配失败、异步终态/取消/卸载及 raw global 独立运行
矩阵也仍需后续压实。void `xlDestroyModule` 不提供 checked 失败返回，本批
不把它描述为新增的可报告卸载接口。标准库仍仅按最低验收要求推进。

源码合同、迁移清单、source/batch 接线与核心 C fixture 接线检查通过：
118 语义行 / 363 代表案例 / 421 确定性候选 / 143 差分 / 56 XLIR-only；
source 接线 286 / 49 / 236 / 6 / 2，核心 C fixture 462 / 11。完整 tool wiring
仍只报告已有 `check_extlib_serial.ps1` 的 test/rc0 接线缺口，未扩大本批范围
修改该外部库问题。长期目标继续保持未完成。

最终 20 项串行专项的 19 项通过：foreign error scope、error unwind、native
take reentry、resolved property、foreign result rollback、export transaction、
return commit、scope exit plan、evaluation lifetime、internal property、property
compound、base constructor、explicit constructor field initializer、closure
argument conversion、named function adapter、async multi-await、generator machine、
try/finally pending exit、core expression。日志为
`build/tmp/xlir_module_cleanup_regression_*.log`。core expression 首次仍假设
入口为 NO_RAISE，已改为按 IR 的 errorAbi 检查精确调用形式及结果提交，完整
重跑见 `build/tmp/xlir_module_cleanup_regression_core_expression_contract_retry.log`。
module finalization 整门仍在首个类布局 guard 处退出 1；native/scalar 两个
非类析构子案例和默认 NULL callable 生成 C 的独立严格编译另行通过，不能
替代整门。最终编码检查 5777 文件、Markdown 检查 329 文件通过。

另行检查了新构建的 6 个 DLL API 验证程序，其中线程宿主、模块依赖寿命、
compiler API、XRT exports 通过。首次直接执行遗漏工具链环境，线程测试在
调用 `tools/xl6_toolchain_env.bat` 后重跑成功；配置环境后的 NULL-context
检查仍报 `xl6_xrt_string_trim` 未解析，embed-string 的标准库场景仍报
`xrtHash64` 重复声明类型不兼容。见
`build/tmp/xlir_module_cleanup_api_xl_dll_null_context.retry.log` 和
`build/tmp/xlir_module_cleanup_api_xlc_dll_embed_string.log`。这两项是本批
额外发现的标准库桥接/声明待办，尚未做旧 DLL 对照，不宣称已证明其首次
引入批次；本批未修复，也不据专项通过宣称完整 DLL 集成验收通过。

## 2026-09-07：类析构链的显式结果程序

本节更新上一节“类 MAY_RAISE 析构未解禁”的状态，不宣称所有正常清理消费者
已经支持析构失败。新增 `CLASS_FINI` 是统一 XLIR 的函数角色，不是第二套 IR
或 Cgen 特例树。Typed class layout 保留派生到基类的精确声明/receiver 计划；
Lowered layout 的 `destroyFunction` 指向一个真正的函数和控制流图。

### 实现与验证边界

- `CLASS_FINI` 接收一个不可复活的借用 receiver，返回 MAY_RAISE void carrier。
  普通 `LOAD` / `VALUE` 表达精确类型的基类借用；没有新增隐式 cast 或 self retain。
- 程序以 `CLEANUP_INHERIT_BEGIN` 继承当前首错。逐级 `CALL` 后立即测试 carrier；
  失败边使用 `CLEANUP_FAIL` 锁定显式失败，即使错误对象为 NULL，也继续执行全部
  基类析构。每步 `CLEANUP_CAPTURE` 汇总诊断，最后以 bool `CLEANUP_END` 和
  明确的 `ERROR_TEST -> THROW / RETURN` 返回整个清理结果。
- Cgen 原先自行遍历 `classDestroySteps` 并发射调用的逻辑已移除。原生 void
  callback 只调用一次被验证的 class-fini，并将失败结果传给共享 runtime scope。
  派生/基类调用顺序、借用和失败控制流不再由 Cgen 从 layout 重建。
- verifier 检查计划与函数的一一对应、唯一函数符号、receiver 身份、全部步骤、
  结果消费和清理区间身份。补充入口变异曾实证发现：仅核对指令序列会放过
  “把入口改到末尾 return”的畸形 IR。现已要求入口从 receiver LOAD 开始，
  基本块均非空、在本函数内结束、恰好覆盖每条指令一次，拒绝省略、重叠和绕过。
  早期失败证据保留为 `build/tmp/xlir_class_fini_entry_mutation.log`。
- 新门 `tools/check_xlir_class_cleanup_outcome.ps1` 接入函数 ABI suite；迁移
  清单增加明确的 XLIR-only 析构结果运行合同，不拿 legacy 的错误处理作为规范。
  原有 lifecycle 门改为检查 IR 中的调用顺序以及 callback 的单次结果适配。

### 最终构建与故障注入证据

入口校验修复后的完整重建为 `build/tmp/xlir_class_fini_final_build.log`，退出 0。
compiler VFS 532 文件，32,495,045 字节压缩到 4,617,224 字节；runtime VFS
115 文件，5,515,719 字节压缩到 859,501 字节。

- `release/xlc.dll`：10,607,104 字节，SHA256
  `77D67C2A42F034E4AD7602E3B292F8192AA32F698A1FF016620AADD04BCF97BF`。
- `release/xl.dll`：6,749,184 字节，SHA256
  `9302819F582A21DC3A67AF8669DD8D03FDF723A020A9CE91550D044A7BFF06AB`。
- 相比上批模块清理版本分别增加 5,632 / 4,096 字节。本批是语义与验证迁移，
  不是整体代码压缩批次；不可用删除一个 Cgen 循环宣称完整产物缩小。

`build/tmp/xlir_class_fini_final_class_cleanup_outcome.log` 完整通过：6,000 个
组合、1,000 次真实错误对象分配失败、99,400 次平衡分配；C-host JIT 与独立
AOT 各 5,000 个案例（不含真实 OOM，保留显式 NULL 失败）；3 个析构程序、
6 个精确步骤、237 项 IR/Cgen/头文件变异均在输出前拒绝。覆盖派生→基类→
native 字段顺序、继承原错/已消费的原错、首错 NULL、后续诊断替换防护、
清理重入与幂等、无外层 scope 的 checked unload、重新初始化，以及严格 C
和确定性 Cgen。入口漏洞修复前的 216 项变异不替代最终 237 项结果。

最终准入报告 `build/tmp/xlir_class_fini_final_admission.json`：536 候选、
legacy 518、共同基线 XLIR 506、12 缺口、0 执行异常，CompilerStable=true，
compiler DLL 哈希与上文相同。相对上批 535 源码逐项比较，8 个原有 XLIR
拒绝转为成功、无退出码退化，另新增 1 个两后端成功的 fixture；恢复案例中
`xlir_module_finalization.xl` 的 legacy 仍拒绝，因此共同基线缺口是 19→12，
不是 19→11。剩余为 10 TCP Future owning layout、1 native-backed protocol
class layout、1 xid intrinsic ABI。531 / 514 / 514 / 0 原门槛保持不变，
准入仍退出 1，不把“明确拒绝”或覆盖率改善当作语言功能验收完成。

最终同一构建的 15 项串行专项全部通过，日志为
`build/tmp/xlir_class_fini_final_*.log`：class cleanup outcome、class lifecycle、
module finalization、error unwind、module cleanup outcome、return commit、scope
exit plan、evaluation lifetime、base constructor、explicit constructor field
initializer、try/finally pending exit、async multi-await、generator machine、core
expression、foreign error scope。旧 lifecycle 门首次因合成符号超过 Int32
失败，现使用与 IR 符号域一致的无符号读取后完整重跑；其 NO_RAISE 析构案例
与新门的 MAY_RAISE 案例均已实测，不只检查新路径。

其中 module finalization 整门恢复通过，包含类/native/scalar 所有者、实际
分配配平与 CLI JIT/AOT 正常/异常卸载；模块结果门的 72 个 DLL API 场景与
16 个语言入口场景也重新通过。error unwind 为 36,000 案例、12,000 次真实
OOM、724,500 次平衡分配，JIT/AOT 各 24,000，1,353 项变异。foreign error
scope 再次包含之前恢复的 raw callback 独立 AOT；本批没有修改 Defender。

静态合同、迁移清单、source/batch 接线、核心 C fixture、编码及 Markdown
检查通过。清单为 118 语义行 / 364 代表案例 / 422 确定性候选 / 143 差分 /
57 XLIR-only 合同；核心 C fixture 465 / 11，编码 5782 文件、Markdown 329
文件。完整 tool wiring 仍只有既有 extlib serial 的 test/rc0 接线缺口，未
放宽检查或在本批修复。定向 `git diff --check` 通过；许多 XLIR 文件本就
未跟踪，额外以真实严格 C 编译和全树编码检查验证，不把 Git 空差异当作证明。

### 已实证但仍未解决的语言缺口

`build/test/xlir_class_cleanup_outcome.strict.exe --probe-scopes` 保留一个不降低
断言的失败探针（日志 `build/tmp/xlir_class_fini_scope_exit_open.log`）：
临时对象析构的首个 NULL 失败能进入 catch，返回 202；普通局部块结束时，
同样的析构失败没有进入 catch，export 的失败默认返回值为 0。两种路径都
完成了 3 / 2 / 1 析构，但“清理完整”和“异常路由正确”必须分开验收。

主门只验收 class-fini 与模块卸载的完整链和结果，不能替代此探针；可带
`-ProbeScopeExits` 明确执行尚未通过的语言门槛。后续须把作用域清理的失败
续边放入 IR，而不能在 Cgen 的 RELEASE 后临时读取 TLS 补跳转。

正常 return/break/continue 不能把所有外层 owner 一次释放后再跳入原 catch：
内层作用域析构失败可能取消原退出路线，catch 仍需要外层变量。应按实际
跨越的 lexical/handler 边界完成当前清理、保留未跨越的 owner、回滚 pending
return，再切换到错误续边。参数清理还要区分函数边界与已退出的内部 catch。
这些约束以及 field/aggregate/cell/frame 的正常清理消费者、效果收敛、构造
部分初始化、异步终态/取消/卸载和 native wrapper 分配失败仍是本次语言硬门槛。

标准库维持最低要求；上一节的 string-trim/hash64 桥接问题本批未修复。
长期目标保持 active，不以析构链专项通过或准入数字提升标记完成。

## 2026-09-07：正常词法作用域的清理失败续边

### IR 合同与本批边界

本批接着上节的失败探针实施，范围是普通 BLOCK 正常结束时的清理结果，
不是把 return / break / continue 的清理异常一并宣布完成。没有修改 Defender
设置；用户已手动关闭实时保护并恢复误报文件，本批只读确认其状态。

Typed XLIR 的 `AFTER_SCOPE` 描述当前作用域的直接绑定，新增的
`AFTER_SCOPE_FAILURE` 则描述这些绑定全部清理后、通向实际 handler 的失败续边。
后者只拥有仍存活且确实跨越 handler 边界的外层绑定，不重复释放刚结束的块，
也不释放 catch 仍可观察的外层对象。两类计划都保存标量生命周期锚点，cell
提升时统一将锚点转换成精确 CELL_RELEASE，避免提升后临时扫描补救。

Lowered XLIR 在普通块尾物化以下显式程序：

```text
cleanup-begin
  当前块的逆序 RELEASE / CELL_RELEASE / native-destroy
  每个步骤后 cleanup-capture，保留首次失败（包括 NULL 诊断）
cleanup-end -> bool
error-test
  成功：正常后继
  失败：error-unwind-begin -> 剩余外层绑定 -> error-unwind-end
        -> 精确 catch / finally / 函数失败边界
```

异常 target 现在保存实际 scopeDepth；不再用失败节点深度减一估算 handler
边界。catch payload 与 pending-return 临时值的清理同样使用实际跨越的边界。
`xl6IrVerifyScopeCleanupRegions` 校验普通清理顺序、capture、独占 bool 结果、
剩余绑定和最终目标，Cgen 本批没有添加 RELEASE 后临时读取 TLS 的分支。

效果分析与 CFG 生成共享正常结束判定：必定 return / throw / break / continue
的结构不能额外执行块尾清理；IF 的所有分支与 try/catch/finally 的结构化退出
也参与这一证明，未分析的循环等结构保守允许正常结束。首轮全量准入曾发现
两个 custom-copy 案例被误判为 MAY_RAISE、一个 return-through-finally 案例
制造了假的可达非 void 出口；源码驱动复现后按上述合同修正，未放宽准入门槛。
首轮证据保留在 `build/tmp/xlir_scope_cleanup_final_admission.json`，不能将该
带 3 个回归的报告当作最终验收。

### 新增的所有权回归

可变闭包中 `var` 类型收窄的 NAME 会产生独立持有引用的投影，不再是原始
CELL_LOAD 的借用。此前自赋值路径又 retain 一次，字段读取路径则不释放
投影，导致两个对象在作用域结束后仍存活。现在转交和借用消费者共享投影
所有权识别，VALUE 类型重绑定不改变所有权；字段独立读取显式持有接收者
直到读完再 RELEASE。同一借用读取边界也用于通用 literal-key 字段读取。
这不是改动 Cgen 的引用计数猜测。

另一个新增用例在失败块之外、try 之内捕获可变 int；它证明失败续边也必须
保留 Typed 标量锚点，提升后才可准确释放 cell。上述两类问题都保留真实
运行时内存计数和析构顺序断言，没有删除失败用例或降低断言。

专项入口为 `tools/check_xlir_scope_cleanup_outcome.ps1`，接入
`test/suites/03_core_function_abi.bat` 和迁移清单。7 个用例覆盖普通嵌套块、
深层嵌套、外层 catch payload、catch 后 finally、未捕获传播、managed var
捕获和外层 scalar cell。每个用例包含无错误、真实诊断、首错 NULL、真实
诊断 OOM、后续 NULL 以及首个析构成功但后续失败六种情况。

旧 class cleanup 门的 `--probe-scopes` 现在强制执行；PowerShell 参数
`-ProbeScopeExits` 仅为旧调用兼容保留，不再将探针跳过。

### 不在本批通过声明中的语言工作

正常 return / break / continue 仍须按实际跨越的 lexical/handler 边界分组
处理析构失败，保留 catch 可见变量并回滚 pending return，不能一次释放到
函数边界后再跳回原 catch。参数清理要区分函数出口与已结束的内部 handler。
带未处理原始失败进入 finally、finally 内部又执行成功/失败清理时的首错
保存，尚需专门故障测试；本批的 catch 后 finally 用例不替代该合同。

构造部分初始化、native wrapper 分配失败、异步 frame/capture/终态消费者、
更普遍的所有权生产者/消费者闭合和语言级准入缺口继续保留。标准库不扩展
到重设计，string-trim / hash64 桥接以及现有 extlib 接线问题未在本批修复。

后续所有权审计还应区分 CELL_LOAD 的借用与投影新建的 owner，检查 async
spill 对 cell 读取结果的分类是否与实际运行时一致，以及 CELL_STORE 替换
旧值时的 detach/reentry/失败结果。此次用例只证明 self-assignment 与块尾
cell 释放，不证明任意新旧对象替换的析构异常已被接管。

### 完整构建、故障验证与准入证据

最终源码对应 `build/tmp/xlir_scope_cleanup_verified_build.log` 的完整
`build_dll.bat` 构建，包含编译器和运行时 VFS，不以源码驱动或局部编译代替。
编译器 VFS 532 资源，32,518,106 → 4,620,111 字节；运行时 VFS 115 资源，
5,515,719 → 859,501 字节。产物为：

- `release/xlc.dll`：10,614,272 字节，SHA256
  `6D873ECD2C94DA5628DF6CE43E96AA050B9EB0070DB5F1AD1EA9A2678A6C7709`。
- `release/xl.dll`：6,754,304 字节，SHA256
  `57A754B6996C038ADB5AFB4D0CC74EF8147009A14B312FBD9619D22E9E0A844C`。
- 相比上节 class-fini 构建增加 7,168 / 5,120 字节；本批是语义补全与校验，
  未宣称二进制或整体代码体积下降。

`build/tmp/xlir_scope_cleanup_verified_scope_cleanup_outcome.log` 完整通过：
4,200 故障组合，700 次真实错误对象分配失败，143,300 次平衡分配，零新增
非法释放、重复释放或 use-after-free。C-host JIT 与独立 AOT 各 3,500 个
案例（真实 OOM 留在严格运行时，显式 NULL 失败仍覆盖）。20 个失败计划、
13 个清理区域、10 个剩余绑定和 1 个外层 scalar-cell 动作，345 项变异
全部在 IR/Cgen/头文件输出前被拒绝。两次 C 和同路径头文件生成完全一致；
首轮误将不同文件名的 include guard 作字节比较的问题已在门本身纠正，
没有忽略实质生成差异。

最终准入为 `build/tmp/xlir_scope_cleanup_verified_admission.json`：537
候选、legacy 519、共同基线 XLIR 507、12 缺口、0 执行异常，CompilerStable
为 true，compiler DLL 哈希与上文一致。逐一对比上节 536 候选：0 原有退出码
退化、0 原有拒绝转成功、1 个新增两后端成功用例。编译成功计数的增长来自
新增 fixture，不能把本批运行语义修复冒称为更多历史拒绝项已经开放。

缺口仍为 10 个 TCP Future owning-layout 相关源码、1 个 native-backed
protocol class layout、1 个 xid intrinsic ABI。531 / 514 / 514 / 0 的
门槛不变，因此准入仍退出 1；这是尚未达标，不是全项目已验收。

静态检查通过：119 语义行 / 365 代表案例 / 423 确定性候选 / 143 差分 /
58 XLIR-only 合同；source wiring 286 / 49 / 236 / 6 / 2，batch wiring，
core C fixture 468 / 11，编码 5787 文件，Markdown 329 文件，定向
`git diff --check`。全库 C fixture 检查另报告 mqtt 的 5 个、serial 的 2 个
非核心测试未接线；tool wiring 仍报告 extlib serial 的 test/rc0 两项缺口。
这些文件不属本批修改范围，没有降低全库检查要求或把 core-only 成功当作
全库成功。

额外 nullable/cell 专项首次被旧 C 文本断言挡住（证据
`build/tmp/xlir_scope_cleanup_nullable_initial.log`）：它要求先
`xrtValueRelease(slot)` 再清空 slot，而实际后端已使用先 detach 再释放的
防重入合同。三处正则现严格要求保存原指针、清空同一 slot、释放保存的指针，
并在赋值路径上验证仍写回同一 slot；不通过容许两种顺序来掩盖问题。
另将 5 处 C wrapper 形参名断言从源码 `present` 更新为现有编译器私有
`xlir_input_0`；公开导出符号、bool/i64 类型和运行时语义断言保持不变，
头文件的源码参数名也不改。第二次旧断言失败证据另存为
`build/tmp/xlir_scope_cleanup_nullable_parameter_name.log`。

最终 18 项专项全部在同一 verified 构建上通过，日志为
`build/tmp/xlir_scope_cleanup_verified_*.log`：scope cleanup outcome、class
cleanup outcome、scope exit plan、return commit、error unwind、try/finally
pending exit、copy/clone、module finalization、module cleanup outcome、class
lifecycle、evaluation lifetime、base constructor、explicit constructor field
initializer、async multi-await、generator machine、core expression、foreign
error scope、null ABI boundary。nullable 大门在更新旧文本断言后从头完整
重跑通过，包含 mutable cell、immutable capture、async result/frame、
nullable 字段、惰性合并/赋值和索引替换的严格 C 与运行时，不以部分执行代替。

其中 error unwind 仍通过 36,000 场景 / 12,000 真实 OOM / 724,500 平衡分配，
现在验证 159 区域 / 201 清理成员 / 1,426 变异，并检查 83 个调用结果和
36 个普通清理区域；module cleanup 8,800 / 6,800 真实 OOM / 188,400 平衡
分配，以及 72 个 DLL API 场景均通过。return commit 1,000 场景 / 80,080
平衡分配通过；class cleanup 6,000 场景与 required scope probe 一并通过。
运行时门通过并不抹去上列未验收的析构异常控制流边界，长期目标保持 active。

## 2026-09-07：提前退出的统一 IR 路线合同（CFG 迁移未完成）

本批完成的是退出路线的数据模型和校验，不是 `return/break/continue`
析构失败的完整运行时修复。现有三个出口仍调用旧的整段清理 / deferred
finally lowering；Cgen 未新增语义查询或补丁。本批已经实测仍失败的情况
列在下面，不能以 IR 合同通过代替语言语义验收。

### 已实现的路线模型

`XL6_IrExitPlan.firstStep/stepCount` 对每个 BEFORE_NODE return、break、
continue 指向唯一、连续的 `XL6_IrExitStep` 切片。其他 exit point 不得
附带路线，整个 step arena 不得有游离项、重叠或遗漏。步骤包括：

- scope：该词法块在这个出口前已初始化的绑定，按反向声明顺序清理；
- catch-payload：跨出对应 catch 时结束捕获诊断的生命期；
- finally：执行显式 finally BLOCK，之后才能清理其可见的外层绑定；
- loop-iteration / loop-resource：分别结束当前迭代项和整个循环资源；
- parameters / terminal：函数参数的最终清理边界和确定的最终目的地。

每个非 terminal 步骤都有明确 `failureTarget`、`failureScopeDepth`、
`failureRegion`，目的地是具体 catch TRY、finally BLOCK 或所属 callable。
在 finally 中再次退出不会安排重跑同一个 finally。多层 continue 清理
被跨出的循环，但保留目标循环的 iterator/snapshot；break 则结束目标循环
资源。参数清理的错误属于函数边界，不能回到已退出的内部 catch。

提前退出计划现在也保留 trivial `END_STORAGE` 生命期锚点，cell promotion
统一替换为 `CELL_RELEASE`。这并不表示旧 lowering 的 cell 特例已经删除。
匿名函数的出口只拥有自身绑定，不能把定义点外层变量复制成自己的清理责任。
generic specialization 后重新生成路线；移除未物化的局部清理时同步调整
step 切片。文本、JSON IR 均输出步骤和异常目的地。

Verifier 逐一检查 lexical binding 的身份、物理 cleanup ABI、精确顺序，
再验证路线的作用域 / finally / loop 边界和异常目标。Typed 和 Lowered
阶段都拒绝损坏的路线；Cgen 和头文件生成在写任何内容前拒绝这些错误 IR。
这里尚未建立“每个步骤都确实物化为正确 CFG 及其错误续边”的完整证明。

### 验证和仍失败的运行语义

`tools/check_xlir_exit_route.ps1` 已接入 function ABI suite 和迁移矩阵，
但明确不计入新的 runtime differential / XLIR-only runtime 数量。
`xlir_exit_route.xl` 包含两层 finally、两层 break/continue、catch 中 return、
finally 覆盖 return、不可达声明和匿名捕获；契约使用独立写定的步骤序列
检查拓扑。Typed/Lowered 共 22 条路线、108 步、1176 次变异拒绝通过；
重复 C 及同路径头文件输出完全一致。

另写入独立诊断探针 `build/tmp/xlir_abrupt_cleanup_probe.xl` 和
`build/tmp/xlir_abrupt_cleanup_probe_host.c`，以当前完整构建生成并严格 C
编译运行。`build/tmp/xlir_abrupt_cleanup_probe.log` 记录：三个出口的无故障
路径均通过；每个出口分别注入带诊断失败、`false + NULL` 失败，六个场景
全部仍失败。实际均为 `result=0 / ok=0 / caught=0 / live=0 / order=321`。
正确行为应先在 handler 边界停下，保留外层 Guard，进入 catch 后恢复执行。
测试没有降低这个要求，也没有将预期失败包装成通过的运行时门。

后续 CFG 迁移必须以这些失败场景为起点，同时覆盖 finally 可见绑定、
pending return 的所有权撤销、cell / catch payload / loop resource / parameter
清理失败；不得只是把一次性全部释放后的出口改跳 catch。旧
`deferredCellSymbols` 和整函数 cell 扫描仍需在新路线完全接管后删除。

### 构建和准入证据

完整 `build_dll.bat` 成功：`build/tmp/xlir_exit_route_initial_build.log`。
compiler VFS 532 文件 / 32,530,987 字节压缩为 4,622,128 字节；runtime VFS
115 / 5,515,719 压缩为 859,501。相对上一节 verified 构建：

- `xlc.dll`：10,619,904 字节，增加 5,632；SHA-256
  `DDAB084DBCF092AE92533E3D2790D9DC6BCA775142F3286BF2B2A9462A8258F6`。
- `xl.dll`：6,757,376 字节，增加 3,072；SHA-256
  `1CB5FC19BFC2B7E297C445BF6BB58F2DE1F0ABA4FBC027138E469B58539FB8A8`。

`build/tmp/xlir_exit_route_initial_admission.json`：538 候选 / legacy 520 /
共同基线 XLIR 508 / 12 缺口 / 0 执行异常 / CompilerStable=true。逐项对比
上一节 537 候选：0 旧项退化、0 旧拒绝转成功、1 新增两后端成功 fixture。
原有 12 缺口和 531 / 514 / 514 / 0 门槛不变，准入仍退出 1，不是全面达标。

首批九项回归全部通过，证据 `build/tmp/xlir_exit_route_verified_*.log`：
scope cleanup outcome、scope exit plan、return commit、error unwind、
try/finally pending exit、copy/clone、class cleanup outcome、async multi-await、
generator machine。这些既有运行时通过项与上面的六个新失败场景分开记录。
静态检查目前通过：120 语义行 / 366 代表案例 / 424 确定性候选 / 143 差分 /
58 XLIR-only；source wiring 286 / 49 / 236 / 6 / 2，batch wiring 11 / 67，
core C fixture 469 / 11。未将 core-only 检查说成全库 wiring 已经通过。

第二批九项也全部通过：module finalization、module cleanup outcome、class
lifecycle ABI、evaluation lifetime、base constructor、explicit constructor
field initializer、core expression contract、foreign error scope、null ABI
boundary。至此同一构建的 18 项既有运行时专项和新增路线契约门均通过，
其中 nullable/cell 大门完整执行结束；两个 DLL 的哈希最终复核未变。
编码检查 5790 文本文件、Markdown 检查 329 文件通过，本批涉及的九个
源码 / 测试 / 接线 / 文档文件未发现行尾空白；这些文件目前是继承工作树中的
未跟踪文件，不能以空的 `git diff --check` 输出替代对其实际内容的检查。
未创建提交、未清理继承变更，`__xlvfs__/module.c` 不存在。长期目标仍 active。

## 2026-09-07：提前退出的词法清理 CFG 与 pending return 撤销

本批把上一节的 return / break / continue 路线接入 lowering：逐段消费 scope、
catch payload、finally、loop 和 parameter 边界；词法 scope 的可失败清理
生成独立 CLEANUP_BEGIN / CAPTURE / END 及明确的成功、失败续边。Cgen 没有
增加语义查询。loop resource 和 parameter 清理的失败结果隔离尚未完成，
不能把这批词法 scope 修复说成所有退出边界已验收。

### IR 与 lowering 合同

- CLEANUP_BEGIN 的一基 `exitStep` 明确关联当前提前退出的 SCOPE 切片。
  Verifier 检查实际释放的身份、顺序、检查点及失败目标，不能以完整块的
  普通退出计划冒充“只初始化一部分绑定”的提前退出。
- 每个 BLOCK 都有稳定的 AFTER_SCOPE_FAILURE 计划，包括当前没有可失败
  局部变量的块；lowering 新增 pending result owner 时仍有明确的失败续边。
- finally 覆盖返回值时，`pendingExit` 标识被取消的旧结果 storage。Verifier
  检查 callable、TRY / finally、返回类型、先前存储和精确 RELEASE / CAPTURE。
  旧结果析构失败会撤销新结果并转入外层错误续边，不会悄悄提交新结果。
- 返回局部值前，只有路线中间没有仍可观察该绑定的 catch / finally，才能
  提前 MOVE；否则维持局部 owner，并保护待提交结果，失败时回收待提交值。
- 共享 finally 后的尾段必须具有相同的 canonical route，不能再次推测清理。
  无真实前驱的 deferred-return finally 不生成虚假的激活分支。
- 清理效应参与 callable error ABI 的固定点计算，包括返回时才执行的清理、
  finally 取消 pending result 的清理。自定义 copy / clone 也可以 MAY_RAISE，
  走统一 call completion 处理 receiver / result 及错误续边。
- ERROR_TEST 不允许成功和失败指向同一块。删除了旧 deferredCellSymbols
  及其捕获 / 延迟释放辅助函数；异常清理中整函数 scalar cell 兜底仍存在，
  尚未声称全部 cell 特例清除。

### 本批运行语义与负向验证

`xlir_abrupt_cleanup_outcome.xl` 有十个 C-host 入口，覆盖三个提前出口、finally
可见外层绑定、finally 后才失败的外层清理、返回局部数组、自定义 copy / clone
的 lvalue / temporary receiver，以及无局部变量的 finally 覆盖 owning return。
六种故障模式包含普通诊断、false + NULL、真实诊断分配失败、首个诊断与后续
NULL 的优先级、首个释放成功后下一个释放失败。

源码编译器及严格 C 运行先行通过：6,000 场景、1,000 次真实诊断分配失败，
231,000 次分配 / 释放平衡，无新增 live allocation、invalid free、double free、
use-after-free。契约覆盖 31 个 route cleanup region、1 个 partial scope、
3 个 fallible copy / clone call、1 个 pending cancellation，479 次变异均在
C / header 写出前拒绝。日志为 `build/tmp/xlir_abrupt_pending_runtime.log` 和
`build/tmp/xlir_abrupt_pending_contract.log`。

上一节六个失败探针已全部转为通过，原始失败日志保留；新增 pending override
探针最初因缺失失败续边被拒绝，其原始诊断也保留。本批初次准入发现普通和
generic custom copy / clone 两项退化，已按真实可失败 ABI 修复，不是将析构
效应改回 NO_RAISE，也没有降低任何准入门槛。

### 完整构建与发布编译器复验

`build/tmp/xlir_abrupt_pending_build.log` 完整构建成功。compiler VFS 532 文件 /
32,545,808 字节压缩为 4,624,288；runtime VFS 115 / 5,515,719 压缩为 859,501。
相对上一节路线模型构建，`xlc.dll` 10,625,536 字节（+5,632），`xl.dll`
6,760,448 字节（+3,072）。SHA-256 分别为：

- `78D235E1E15FACC73D2066FEBDCEE775B2E26D4E23EE43F2E45609092BD5BDF4`
- `122CF9009EA5AF80F65EBD1CAD2CBCC469A067CD1B544D2A5B92B51FBA81741F`

`build/tmp/xlir_abrupt_complete_abrupt_cleanup_outcome.log` 使用这次发布产物复验：
6,000 / 1,000 real OOM / 231,000 平衡分配、479 次变异，以及 C-host JIT 和
独立 AOT 各 5,000 场景全部通过；重复 C / 同路径头文件一致，JSON 保留
exitStep / pendingExit。新增门已接入 function ABI suite 与迁移矩阵。

静态门通过：121 语义行 / 367 代表案例 / 425 确定性候选 / 143 差分 /
59 XLIR-only，source wiring 286 / 49 / 236 / 6 / 2，batch 11 / 67，core C
fixture 472 / 11；编码 5795 文本文件、Markdown 329 文件。日志前缀为
`build/tmp/xlir_abrupt_complete_`。本批 11 个源码 / 测试 / 接线 / 文档文件
实际内容没有行尾空白，未用 git diff 输出冒充对未跟踪文件的检查。

上述构建的准入 `build/tmp/xlir_abrupt_complete_admission.json` 为 539 候选 /
legacy 521 / 共同基线 XLIR 509 / 12 缺口 / 0 执行异常 / CompilerStable=true。
相对上一节 538 候选，0 旧项退化、0 旧拒绝转成功、1 新增两后端成功 fixture。
两项 custom copy / clone 中途退化均消失。原有 10 个 TCP Future owning layout、
1 个 native-backed protocol class layout 和 1 个 xid intrinsic 缺口不变；
531 / 514 / 514 / 0 门槛没有降低，准入仍退出 1。

同轮 scope cleanup 运行时 4,200 / 700 real OOM / 143,300 平衡分配及 JIT / AOT
各 3,500 均通过，但其 verifier mutation 曾以栈溢出退出，不能算整门通过。
新增“每块失败计划”扩展了节点破坏测试范围，暴露某些 semantic tree walk
早于节点切片验证。修复将迭代式、线性时间的节点拓扑预检置于所有语义检查前：
检查 child arena、切片、单父节点、根和断开环；不是在递归查询处吞掉错误。
补充 root child cycle 与一致 parent 的 disconnected cycle 拒绝测试，源码
scope 契约达到 60 个失败计划 / 20 个区域 / 33 个剩余绑定 / 2 个 cell 动作，
736 次变异拒绝。失败日志与 `xlir_abrupt_scope_mutation_backtrace.log` 保留。
最终 `build/tmp/xlir_abrupt_topology_build.log` 全量构建成功：compiler VFS
532 / 32,548,057 压缩为 4,624,716；runtime VFS 不变。最终 `xlc.dll`
10,626,560 字节、`xl.dll` 6,761,472 字节，相对上一节路线模型构建分别
增加 6,656 / 4,096 字节。SHA-256 分别为：

- `D55A11140A543D950F760DD921C1652AAAC882FDB67598F2A878D8BA0BC3B244`
- `56FDCDC1CC7F6ED85FCA9A6702A977201275899DBC902F2B3797DEDD1D9035D2`

最终专项日志使用 `build/tmp/xlir_abrupt_verified_` 前缀，与上述中途失败日志
区分保存；新增 abrupt outcome 和 exit-route 两门已在这组产物上再次通过。

同一最终构建的 20 项专项全部通过，失败数 0：abrupt cleanup outcome、exit
route、scope cleanup outcome、scope exit plan、return commit、error unwind、
try/finally pending exit、copy/clone、class cleanup outcome、async multi-await、
generator machine、module finalization、module cleanup outcome、class lifecycle、
evaluation lifetime、base constructor、explicit constructor field initializer、
core expression、foreign error scope、null ABI boundary。scope 门最终为 736
次变异拒绝，abrupt 门 479 次，exit-route 为 22 条 typed/lowered 路线、108 步、
1272 次变异拒绝；nullable/cell 大门完整执行结束，未留后台测试进程。

最终 `build/tmp/xlir_abrupt_verified_admission.json` 再次得到 539 / 521 / 509 /
12 / 0 / CompilerStable=true，逐项比较上一节仍为 0 退化、0 旧拒绝转成功、
1 新 fixture 两后端成功；其 compiler DLL 哈希与上列最终哈希一致。准入
仍不达 514 / 0 的要求，不是全面通过。六项静态门同样再次通过，指标不变；
最终涉及 12 个源码 / 测试 / 接线 / 文档文件，实际检查行尾空白为 0。

剩余语言语义优先项：loop iteration/resource 和 parameter 释放结果隔离、
异常路径整函数 scalar-cell 兜底的移除、更多 pending exception / finally
交互与 async frame/capture 生命周期；原有准入缺口另行保留。标准库没有在
本批扩面。未提交、未清理继承工作树、未改变 Defender 配置；长期目标 active，
不以本批专项全部通过代替统一 IR 全任务完成。

### 2026-09-07：参数激活记录的清理结果与稳定借用效应

本批延续上一节，先处理同步函数参数的退出边界。原实现虽然释放了重新
绑定后的参数，却没有将参数析构的失败并入函数返回结果：探针中先释放
3、2，再释放返回值 4 和调用者外层 1，物理顺序为 3241，但 diagnostic /
false-NULL 两种失败都绕过调用者 catch，以失败返回离开整个入口。
`build/tmp/xlir_activation_cleanup_probe_baseline.log` 保留失败，
`xlir_activation_cleanup_probe_migrated.log` 中三种模式均成功，catch 仍可见外层 1。

这里没有向 Cgen 增加语义追溯或特殊补丁，新增契约位于 XLIR 与 lowering：

- `CallTarget.parameterLifetimes` 按物理参数记录源绑定、是否可能重新绑定、
  参数边界清理效应。绑定分析覆盖直接 / 多目标赋值、嵌套闭包、循环目标及
  out / byref 写回；修改对象字段不等于替换参数绑定。
- 同步稳定 BORROW 的本地 retain 副本与可能成为最终 owner 的重新绑定参数
  分开处理。调用者临时参数的清理效应仍独立保守计算，不能被这一收窄消除；
  async / generator 不继承同步调用期间的存活保证。
- `Function.parameterCells` 显式索引提升后的参数 cell；终结器按逆参数顺序
  释放 cell 与参数 owner。不再将混有命名适配器默认参数的 lowering map 当作
  cell 清单，异常路径原有 scalar-cell 扫描也排除已经显式登记的参数 cell。
- `Instruction.parameterScope` 将参数 CLEANUP_BEGIN 与成功 RETURN、失败
  THROW 绑定到同一事务；每个参数动作后捕获结果，最终以 ERROR_TEST 分流。
  managed / product 返回值先保存，成功时 LOAD / MOVE 提交，失败时在保留
  首个错误的 unwind 中释放，最后向调用者传播，不能重新进入已完成的内部
  catch / finally。已有 primary failure 同样不能被后续清理覆盖。
- verifier 检查绑定效应、参数 cell 的唯一入口创建、精确关闭顺序、捕获点、
  两个结果块的唯一入边、待返回 owner 的提交 / 撤销和终结引用。损坏 IR 在
  C 与 header 输出前拒绝；文本 / JSON 输出保留参数 lifetime 与事务身份。

中途实现曾把所有 managed BORROW 参数（包括未重新绑定的 self）的清理
一律提升为 MAY_RAISE，完整准入因此暴露 17 项退化，而不是得到全面通过。
`build/tmp/xlir_parameter_initial_admission.json` 为 540 候选 / legacy 522 /
共同基线 XLIR 493 / 29 缺口 / 0 执行异常 / CompilerStable=true。退化集中在
构造、继承、虚调用和属性；随后补足上面的 stable / rebound 区分，并保持
调用者临时参数清理与被调用者参数清理为不同效应。该中间报告不可冒充最终结果。

正式 fixture `test/cases/xlir_parameter_cleanup_outcome.xl` 覆盖 11 类路径：
owning / scalar / explicit void / implicit void / product 返回、mutable var
parameter cell、finally 观察参数、已有 primary failure、out 写回、byref 写回、
稳定借用与调用者写回别名。最后一项改写调用者原 owner，验证原借用值仍由
调用期间临时 owner 保持，调用者关闭该 owner 时再处理失败。直接 class 类型
的 mutable capture 仍被语义层拒绝，不能以已通过的 var cell 路径宣称它完成。

最终参数门日志 `build/tmp/xlir_parameter_verified_parameter_cleanup_outcome.log`
通过：6,600 运行场景、1,100 次真实诊断分配失败、209,000 次分配 / 释放平衡；
C-host JIT 与独立 AOT 各 5,500 场景；重复 C 与同路径 header 一致；11 个事务、
8 个 pending return、2 个参数 cell、20 个 rebound / 23 个 stable 绑定，400 次
变异在 C / header 输出前拒绝。场景同时覆盖 first error、false-NULL、后续
diagnostic 与 NULL 的互不覆盖、调用者 catch 的可见 owner 和最终零存活。
门接入 function ABI suite 及迁移矩阵。参数 lifetime 是创建 CallTarget 时、
CFG 发射前的事实；尚未注册目标的原始 typed dump 不要求出现该数组。

全量 `build/tmp/xlir_parameter_final_build.log` 成功：compiler VFS 532 文件 /
32,567,792 字节压缩为 4,627,336；runtime VFS 115 文件 / 5,515,719 压缩为
859,501。`xlc.dll` 10,634,752 字节、`xl.dll` 6,766,080 字节，相对上一节最终
构建分别 +8,192 / +4,608；不是将 generated C 的字节数当作 DLL 增量。
SHA-256 分别为：

- `A2184E69B2D0E94D0C23035F9BDEED388E611AD1D0AFD75EDB59398D42901D78`
- `C3AC0CC4EBFF008318C71C55716793F4F0B8448E1211A2404FE5B4BB086E2C47`

同时保留下一项语言缺陷证据：稳定 protocol iterable 的 normal / break /
continue / return 四种退出，每种 diagnostic / false-NULL 模式均丢失隐藏
iterator 的清理失败，共 8 个失败，记录在
`build/tmp/xlir_loop_activation_stable_baseline.log`；它们仍未修复。临时 iterable
另有“call outcome must be tested before caller cleanup”编译拒绝，已将 __iter__ /
__next__ 调用接入共同的 receiver-call completion，移除旧专用完成辅助函数；
这只修复调用结果的观察顺序，不代表 loop resource / iteration 关闭事务完成。

后续继续优先完成 loop owner / outcome 的明确 IR 契约、剩余整函数 scalar-cell
兜底移除、pending exception / finally 与 async frame/capture 生命周期。标准库
本批不扩面；未提交、未清理继承工作树、未改变 Defender 配置。长期目标仍 active。

同一最终构建的准入 `build/tmp/xlir_parameter_verified_admission.json` 为
540 / legacy 522 / 共同基线 XLIR 510 / 12 缺口 / 0 执行异常 /
CompilerStable=true，compiler DLL 哈希与上列一致。逐项对比上一节最终 539
候选：0 旧项退化、0 旧拒绝转成功、1 新 fixture 两后端成功；对比本节中间
报告，17 项退化全部恢复。缺口仍为 10 个 TCP Future owning layout、1 个
native-backed protocol class layout、1 个 xid intrinsic；531 / 514 / 514 / 0
门槛保持不变，准入仍退出 1，不能说默认后端完整准入已通过。

静态专项 source / batch / migration / core C fixture 四门通过：286 / 49 /
236 / 6 / 2；11 / 67；122 语义行 / 368 代表案例 / 426 确定性候选 / 143 差分 /
60 XLIR-only；475 / 11。编码检查 5800 文本文件和 Markdown 329 文件通过。
日志均为 `build/tmp/xlir_parameter_verified_` 前缀。额外执行的全局
`check_test_case_wiring.ps1` 仍失败，报告 37 个非本批新增 fixture 未接线，
主要是 codec / serialization 桥接审计案例；该门不同于已通过的 core C
fixture 门，不能用后者结果替代。原日志保留，未删除案例或添加虚假接线。

下一项 loop 迁移不能仅在 RELEASE 后补 ERROR_TEST。当前隐藏 resource 与
iteration target 的物理 owner 仍登记在 lowering 局部栈中，异常辅助函数将
它们放在扁平词法清理之后；item 清理失败时，剩余 iterator 与外层局部变量
必须按真实嵌套顺序交织关闭，不能依赖重复释放已置空 storage 证明 exactly-once。
需要同时明确已完成动作、剩余 owner、正常 / 提前退出 / 异常路径与正确外层
handler，才能将该类生命周期契约交给机械 Cgen。

最终专项已全部结束：上一节的 20 门，加上本节参数门与 protocol iterator，
共 22 门在同一发布产物上通过。protocol 门最初因旧断言要求静态只有一条
源对象 RELEASE 而失败，日志 `xlir_parameter_verified_protocol_iterator_abi.log`
保留；新调用完成 CFG 在成功 / 失败两条互斥分支各释放一次，测试因此改为
遍历两条分支的可达 CFG：每个 terminal 恰好关闭一次，并要求释放处于结果
隔离 / unwind 区域。补充丢失释放、缺少隔离、重复释放循环共 5 次变异拒绝，
不是将“等于一条”简单改成“至少一条”。完整复验日志为
`build/tmp/xlir_parameter_verified_protocol_iterator_final.log`，strict C 和
legacy 差分运行均通过。其余 21 门日志继续使用上述 verified 前缀。

最终六项静态专项再次通过，指标不变；本批 12 个实际源码 / 测试 / 接线 /
文档文件行尾空白为 0，未用 git diff 替代对未跟踪内容的检查。最终 DLL 哈希
与准入报告一致，未遗留后台测试进程；`__xlvfs__/module.c` 不存在。上述结果
仍不消除 12 个完整准入缺口、37 个全局测试接线问题或已复现的 8 个循环失败。

### 2026-09-07：循环物理 owner、分阶段清理结果与 generator next carrier

本批继续处理语言生命周期，不扩张标准库功能。上一批 stable iterator 探针中
normal / break / continue / return 的诊断与 false/NULL 共 8 个失败，在源码
编译器上均修复；原始日志 `build/tmp/xlir_loop_outcome_baseline.log` 与通过日志
`build/tmp/xlir_loop_route_probe.log` 保留。不是在 Cgen 里按源语法补释放代码。

- `LoopOwner` 明确 LOOP、函数 owner、物理 storage、类型和 item/key 序号；
  hidden resource 也是 LOCAL owner，不再靠临时栈和“重复释放 NULL 无害”证明生命周期。
- 每个 LOOP 有 before-node、after-loop-iteration-failure、
  after-loop-resource-failure 三个异常拓扑位置。异常 `ExitPlan` 使用有序
  `ExitStep`，将词法局部变量、每轮 item/key 和隐藏 resource 按嵌套边界交织清理。
- 正常耗尽和 break 使用不同 CFG 汇合点。前者在 exit block 关闭 resource，
  后者由自己的 route 关闭后进入 after-loop block，不二次关闭同一逻辑 owner。
- MAY_RAISE loop 清理有独立 BEGIN / CAPTURE / END 和布尔 outcome；失败在已完成
  的 phase 之后继续 unwind，取消 pending owning return，并保留第一个失败。
- verifier 验证每个物理 owner 的 inactive/active CFG 状态、STORE/LOAD/MOVE/RELEASE、
  合流和回边、terminal 前关闭、清理事务与异常计划的关联，以及 loop-close
  failure 的剩余 owner 顺序和 typed handler。文本与 JSON 均导出 loop owner。

扩大 generator 探针时，`generator<var>` 在语义层被拒绝，证据保留于
`build/tmp/xlir_generator_var_item_boundary.xl` / `.log`，不能算作已支持的 managed item。
正式测试使用已准入的 `generator<string>`，两个 class owner 跨 yield 存在于 frame。
它先暴露 generator 缺少共享 class/value 指令，再暴露 false/NULL 被误当成 end：
`build/tmp/xlir_loop_cleanup_initial_compile.log` 和
`build/tmp/xlir_loop_cleanup_initial_strict.log` 保留这两个失败。

普通函数与 generator 现在共用 class/value runtime renderer；literal scalar/string
field projection 以 opcode → helper / result ABI 映射统一发射，移除普通后端对应重复
分支。`GENERATOR_NEXT` 的物理返回明确分离 ok 和 yielded/end，ERROR_TEST 必须立即
观察 ok，成功后才发布布尔 SSA value。generator THROW 向 next 的隔离边界传播失败，
诊断对象可为 NULL。新增变异曾证明删除 ERROR_TEST 会被旧 verifier 接受，该漏洞已
修复；没有使用 xrtGetError 是否非空来替代这项 carrier 契约。

正式 fixture `test/cases/xlir_loop_cleanup_outcome.xl` 当前覆盖 20 类路径：
protocol iterator 的正常耗尽、break、continue、owning return；item 关闭失败；
false-product 的 owning item；外层局部清理失败；嵌套 item/resource 的交错清理；
原始异常、内部 catch 的存活边界、__iter__/__next__ 失败，以及 generator 的正常、
break、continue、owning return。host 检查析构顺序、catch 可见 live mask、结果回滚
与原始错误；strict runtime 以六种模式循环，包含真实诊断分配 OOM。

首轮发布构建 `build/tmp/xlir_loop_final_build.log` 已通过：compiler VFS 532 文件，
32,601,487 字节压为 4,631,860；runtime VFS 115 文件，5,515,719 字节压为 859,501。
`xlc.dll` 10,647,552 字节，SHA-256
`0E545CA103F5F2481AE890CE5BEC36CC29E6E6733A5C2506188A5194968DA463`；
`xl.dll` 6,773,760 字节，SHA-256
`2D3037B0E7BCF149EB261AB1C56D39EDEC94545EE502A73E3C065C019FA9B89A`。
本轮不使用上一批的 DLL 测试结果冒充最新源码验证。

新发布产物的 loop strict 测试已通过 12,000 次运行、2,000 次真实诊断分配失败，
679,200 次分配 / 释放平衡；C-host JIT 与 AOT smoke 各 10,000 次通过。
完整 verifier 变异、准入与既有门的最终状态以本节后续复验记录为准。

本批不能宣告统一 IR 完成：`EmitPendingExceptionRoute` 仍有独立资源路径，
finally/pending-exception 的全量顺序审计尚未完成；此处没有覆盖所有 keyed / native
iterator 析构故障组合，也没有开放 generator<var>。完整准入和历史全局接线债务
必须分别复验，不由这一个生命周期门的成功抵消。
字段初始化的声明来源、构造调用点和 handler 三者之间的独立变异验证也仍需补齐；
本批已经将 invocation 的异常目标写入显式 cleanupPlan，并验证实际 runtime 行为，
但不把这一项当作所有声明重放语义均已完整验收。

首轮完整准入 `build/tmp/xlir_loop_verified_admission.json` 为 541 / 522 / 500 / 22，
执行失败 0、CompilerStable=true。相较上一批新增 10 个共同准入回退，另有
`xlir_error_unwind.xl` 一个 XLIR-only 回退，不能用 loop 专项通过抵消。其原因和修复是：

- initializer error-plan 查找与 module-init effect 分析仍依赖旧的 region=0 哨兵；
  现在分别选择 before-node 计划与显式 terminal=root 的拓扑事实。
- 共享 literal F64 renderer 的结果校验遗漏了已有 C float/double ABI；补齐精确
  映射，不取消 result ABI 验证。
- 字段声明初始化表达式在构造调用处实例化，原声明的异常作用域并不是调用处
  catch。lowering 现在用有界 initializer invocation context 选择构造调用处的
  typed ExitPlan，嵌套初始化继承最外层 invocation；源字段位置仅作为诊断来源。
  已生成的 ERROR_UNWIND_BEGIN 明确记录该 cleanupPlan，Cgen 不再猜测 handler。

以上 11 个原通过源文件在源码编译器上均已恢复。字段实例化修复后，
`build/tmp/xlir_loop_rebased_unwind.log` 的 strict host 通过 36,000 次运行、12,000 次
真实诊断分配失败和 724,500 次平衡分配。该结果不替代修复版完整发布复验。

首轮旧有 22 门中 19 门通过、3 门失败：error_unwind 是上述实际回退；exit_route
旧测试把新增 THROW routes 当成 normal return 的固定序列；try_finally_pending_exit
按 THROW 源行查找 pending-result RELEASE，但新 IR 将该 RELEASE 归属 finally SCOPE。
后两者更新为限定 normal route 序列、或沿显式 unwind / scope / pending storage 身份
验证，没有降低 runtime 或准入门槛。首次失败日志使用 `xlir_loop_verified_` 前缀保留。

修复版完整构建 `build/tmp/xlir_loop_repaired_build.log` exit 0；compiler VFS 532 文件，
32,603,390 字节压为 4,632,131，runtime VFS 指标不变。最终发布 `xlc.dll`
10,647,552 字节，SHA-256
`B68B602AB0414B1D901A156AFA38D4F894BC2891EA6F0B9C682B4F8177FE9E5F`；
`xl.dll` 6,774,272 字节，SHA-256
`3B1DDAF4A3BF0FBE7DEBB657422DFE4FA466D984DB72D485F1DBB79D31439A2D`。

最终完整准入 `build/tmp/xlir_loop_repaired_admission.json`：541 候选、legacy 522、
共同 XLIR 510、缺口 12、执行失败 0、CompilerStable=true，报告 DLL 哈希与磁盘一致。
对上一批 540 候选逐项比较：旧 XLIR 成功回退 0、旧拒绝意外开放 0、缺口集合差异 0。
唯一新增源 `xlir_loop_cleanup_outcome.xl` 是 legacy 拒绝 / XLIR 成功，未混算为共同成功。
最低门槛仍为 531 / 514 / 514 / 0，因此完整准入 exit 1，而不是“全绿”。

修复版 loop 门再次完整通过：36 个物理 owner、58 个事务、197 个 release sites，
含 6 次 enclosing-loop 顺序变异和 4 个 generator-next carrier，共 1,549 次变异在
C/header 输出前被拒绝；12,000 strict / 2,000 真 OOM / 679,200 平衡分配，以及
JIT/AOT 各 10,000 次指标均不变。日志 `build/tmp/xlir_loop_repaired_loop_cleanup_outcome.log`。

六项修复版静态检查已通过：source 286 / 49 / 236 / 6 / 2，batch 11 / 67，migration
123 行 / 369 代表 / 427 确定性候选 / 143 差分 / 61 XLIR-only，core C 478 / 11，
encoding 5,805，Markdown 329。独立全局接线检查仍为原来的 37 项，日志
`build/tmp/xlir_loop_repaired_global_wiring.log`；没有把 core C 检查与全局检查混为一谈。

退出拓扑契约最后单独扩展并复验，保留原有 22 条 normal typed/lowered routes 与
108 个 normal steps，另对 88 条 exceptional routes 验证元数据、步骤来源和失败
目标变异，总计 2,446 次拒绝。日志
`build/tmp/xlir_loop_repaired_exit_route_final_contract.log`；它不把原 normal route
序列断言套到 THROW，也没有简单跳过新增的 THROW 拓扑验证。

最终 23 项专项全部通过：新增 loop 门与上一批 22 门，在修复版同一发布 DLL 上
完成串行运行，日志统一为 `build/tmp/xlir_loop_repaired_<gate>.log`。exit_route
最后新增的 exceptional 变异另外使用上述 final_contract 日志，源码与发布快照一致。
最后 19 个实际修改的源码 / 测试 / 接线 / 文档文件行尾空白为 0；未通过 git diff
忽略未跟踪文件，未删除或提交工作树内容。`__xlvfs__/module.c` 不存在，所有本轮
后台构建和测试进程均已结束。长期任务保持 active，12 个既有准入缺口、37 个全局
接线问题及上面列明的剩余语言验证工作均未被标记为完成。

### 2026-09-07：完成 TRY 后的异常继续传播与剩余 owner 路由

本批首先用 `test/cases/xlir_pending_exception_outcome.xl` 复现两个独立问题。
旧发布版进入外层 catch 时 `live=3, code=0, order=34`：中间局部对象没有释放，
原始诊断 777 也被 finally 内后续操作清掉。基线日志为
`build/tmp/xlir_pending_exception_baseline.log`。这不是工具链无法运行或杀毒拦截。

本批修复的是第一个问题，未宣称第二个问题已解决：

- typed XLIR 为真实 TRY（不含 CATCH 节点）固定一个 `after-try-exception` THROW
  continuation。TRY 自己的 protected/catch/finally 子作用域已结束；计划只拥有
  外围仍然存活的局部、catch payload、循环 item/cursor 和参数边界。
- 未匹配的单个 / 多个 typed catch，以及三种 exceptional-finally 的继续传播，
  全部进入共用的 `EmitExceptionTransfer`，使用该明确计划的 region、scope 和
  有序步骤。删除“选当前最内层 handler、只清扫 loop resource”的旧特殊路由，
  连同仅为它维护的 loop exception-depth 字段和无调用者 return-resource helper。
- lowered unwind 显式引用上述 ExitPlan。验证器检查该身份、剩余直接局部和循环
  owner 的释放顺序，以及进入 typed handler 的终端边；typed / lowered 计划和
  两种文本 dump 同步表达新阶段。本批没有修改 Cgen 来补推断或恢复源语义。

专项 `tools/check_xlir_pending_exception_route.ps1` 默认仅运行 `--routes`：未匹配
单 / 多 catch、三种 scalar-only finally、跨 loop 的 item/cursor/局部顺序。
它刻意不认证 managed-finally 的 pending-error 保存；`-FullOutcome` 可运行保留的
完整故障复现。源码中的 12 个 probe 不能等同于本批运行通过的 6 个 route probe。
专项已接入 `03_core_function_abi.bat`，迁移矩阵行名也明确注明该限制。

发布版专项结果：3,600 次 strict 运行，600 次真实诊断分配失败，103,600 次平衡
分配；C-host JIT / AOT 各 3,000 次。覆盖正常、诊断 777、失败但诊断 NULL、真实
OOM，以及清理再报错时不覆盖原先失败。IR 契约验证 50 个 typed/lowered plans、
13 个 unwind regions、17 个 release sites，含 4 次独立 owner 顺序交换；共 675
次变异在 C/header 输出前拒绝。日志
`build/tmp/xlir_pending_route_pending_exception_route.log`。

首轮完整重建 `build/tmp/xlir_pending_route_build.log` exit 0：compiler VFS 532 文件，
32,603,893 字节压为 4,632,195；runtime VFS 115 文件，5,515,719 字节压为 859,501。
`xlc.dll` 10,648,064 字节，SHA-256
`0A15CAAB0011AA0DC345C4B97383C01D6F6A8D6E1BE4F2463BA5AD89CBED41A4`；
`xl.dll` 6,774,784 字节，SHA-256
`12896939B61DA509663B434F4D3C21CAAA09F15042C0FDA65F0C0EE4A8DFA847`。

未完成的语言契约继续明确保留：

- managed-finally 的原始异常还没有独立的 pending-error owner。修复路由后的完整
  probe 仍失败，但已变为 `live=1, code=0, order=342`，即外围 owner 已关闭，诊断
  777 仍丢失。当前发布版复现日志为
  `build/tmp/xlir_pending_route_full_outcome_remaining.log`。后续需要在 IR 明确保存 /
  恢复 / 取消 pending error，区分自动清理的次生错误与显式新异常、return 等覆盖，
  并覆盖嵌套 handler、NULL/OOM 和 async frame；不能只在 Cgen 条件恢复 TLS。
- `build/tmp/xlir_product_local_return_boundary.xl` 及同名前缀 `.log` 独立证明
  `var item = ProductLocalItem(); return true, item` 仍被拒绝，诊断为
  `XLIR transferred return has no exact source LOAD ABI`。正式 loop probe 用内联
  constructor 只为隔离本批路由问题，不代表这个语言问题已修复。

本批静态检查已通过：source 286 / 49 / 236 / 6 / 2，batch 11 / 67，migration
124 行 / 370 代表 / 428 确定性候选 / 143 差分 / 62 显式 XLIR-only，core C 481 / 11。
全局 test-case 接线仍为原有 37 项；工具接线另报告此前已存在的
`check_extlib_serial.ps1` 未接入两类入口。新专项已接线，没有降低这两项门槛。
静态日志统一为 `build/tmp/xlir_pending_route_static_<gate>.log`。

首轮发布回归捕获 `finallyResult` 的参数清理误拒绝：完成 deferred return 后的
parameter rollback 与 TRY 共用 diagnostic origin，但它不是 post-try continuation。
首轮准入因此为 542 候选 / legacy 522 / 共同 XLIR 509 / 13 缺口。首轮结果保留于
`build/tmp/xlir_pending_route_admission.json` 和对应专项日志，不视作最终通过。
修复将 parameter cleanup 的失败 unwind entry 也显式关联 `parameterScope`，由
参数事务验证器验证 begin、failure entry、success return、failure throw 四个引用；
pending-route 验证器据此区分完成阶段，不再仅凭同一源码位置误判。没有放宽
post-try 计划或其 owner 顺序。参数契约新增 33 项失败入口身份变异，源码专项达到
433 次拒绝；日志 `build/tmp/xlir_pending_route_parameter_repaired_source.log`。

修复版完整构建 `build/tmp/xlir_pending_route_repaired_build.log` exit 0，compiler
VFS 532 文件 / 32,604,467 字节 / 压缩 4,632,275；runtime VFS 指标不变。
最终 `xlc.dll` 10,648,576 字节，SHA-256
`049C0B4E575FA16F14BC2C2D4DB274233B841F3D1AF5A36D89F4A14111609C14`；
`xl.dll` 6,774,784 字节，SHA-256
`61E5E76D6AFA6A7104D661FEAC762FC176DDE7B5A5FF5E425A20D33F3EF07B3D`。

修复版完整准入 `build/tmp/xlir_pending_route_repaired_admission.json`：542 候选、
legacy 522、共同 XLIR 510、12 缺口、执行失败 0、CompilerStable=true，报告哈希
与发布 DLL 一致。对上一批 541 个候选逐项比较：旧成功回退 0、旧拒绝开放 0、缺口
集合差异 0；唯一新增源为 `xlir_pending_exception_outcome.xl`，legacy 拒绝 / XLIR
编译成功。没有把其未执行或失败的 managed-finally probes 当成运行成功。
最低门槛仍为 531 / 514 / 514 / 0，所以准入 exit 1；不是全部达标。

修复版 parameter 专项为 6,600 strict / 1,100 真 OOM / 209,000 平衡分配，JIT/AOT
各 5,500 次，433 次变异拒绝；pending route 专项的 3,600 / 600 / 103,600 及
675 次拒绝均再次通过。日志使用 `xlir_pending_route_repaired_` 前缀。两个未完成
语言用例也在此发布版重现：`xlir_pending_route_repaired_full_outcome_remaining.log`
和 `xlir_pending_route_repaired_product_local_return.log`，均未计入通过数。

额外复验基础 `check_xlir_contract.ps1` 发现它仍要求旧 iterator 专用完成函数、
depth-only loop sweep、handlerScopeFloor 等实现标记；现已对齐共用 receiver-call
completion、typed post-try / parameter-failure 身份及 owner-route 验证，且反向禁止
重新引入已删除的旧清扫 helper。该静态契约已通过，未通过恢复旧函数或降低运行
断言来满足检查。日志 `build/tmp/xlir_pending_route_repaired_static_xlir_contract_updated.log`。

最终在修复版同一发布 DLL 上串行复验 24 项专项全部通过：上一批 23 门加新增
pending_exception_route，日志统一为 `build/tmp/xlir_pending_route_repaired_<gate>.log`。
七项静态检查通过（六项 source / batch / migration / core C / encoding / Markdown
加基础 XLIR contract）；最终 encoding 5,810、Markdown 329。全局 37 个 case 接线
以及 serial 工具接线欠账没有被计为通过。14 个本批实际修改的源码、测试、接线和
文档文件行尾空白为 0，`__xlvfs__/module.c` 不存在；本批后台构建与测试均已结束。
长期任务保持 active，原有 12 个准入缺口和本节两项明确复现的语言问题继续处理，
没有将 route-only 专项或编译准入替代完整语言语义验收。

### 2026-09-07：managed finally 的显式 pending-error owner 与挂起存储

本批继续处理上一节 full-outcome 的确定性失败：资源已按正确顺序关闭，但原始
777 诊断经过 managed-finally 的普通清理事务后被清空。这不是 post-try 路由缺失，
也不能用 Cgen 从 TRY 源码追溯异常来修补。lowering 现在将失败控制流暂存为
`XL6_IrPendingException`：origin / finally body / physical owner / LOCAL symbol /
TypeId / capture STORE / enclosing pending owner。失败状态由 CFG 表达，LOCAL
只拥有可空的 `xerror*`；NULL 不能把已知失败变成成功。

- exceptional-finally 入口执行 `ERROR_TAKE → STORE`，普通 finally 入口不激活
  这个 owner。完成 finally 后执行 `LOAD → MOVE → ERROR_SET_TAKE`，再消费已验证
  的 post-try route；不把原异常留在随后的普通清理事务所使用的 ambient error 中。
- 自动清理产生次生失败且路由确实离开当前 finally 时，恢复操作显式记录
  `pendingFailure = CLEANUP_END.result`。验证器同时验证失败分支、原 owner 与
  cleanup plan 的边界，因此“把恢复替换成平衡释放”仍会被拒绝。
- 显式的新 throw/call/await 失败离开 finally 时取消旧 owner；内部 catch 如果仍
  留在 finally 内则不取消。return/break/continue 的正常退出清理事务显式标记
  它取消的 pending owner。如果这次清理本身失败，不能再次读取已取消的旧异常。
- 嵌套 pending owner 构成提前验证的无环 parent forest，按实际 materialized
  finally 实例区分。每个 owner 的 CFG 激活、借用、转移或取消必须闭合，分支汇合
  的激活状态必须一致，函数终点不得残留 owner。反向检查 LOCAL error storage，
  防止同时删除整个元数据表和全部 instruction tags 绕过验证。

没有引入新的 Cgen 语义指令或 finally 专用打印分支。已有 LOAD / STORE / MOVE /
RELEASE / ERROR_SET_TAKE 直接渲染物理操作。异步帧此前缺少 error LOCAL 的准入，
尽管 IR 已有 FREE_ERROR 析构 ABI；现在 async 与 generator 共用
`xl6IrCgenFrameStorageSupported` 和现有 detach-before-destroy renderer。IR 验证器
负责 TypeId / ownership / frame action / init bit 的匹配，Cgen 只判断已验证的
物理存储与析构是否能渲染；没有放宽尚未支持的 SSA-spill 或 launch-parameter
transfer ABI。

`check_xlir_pending_exception_route.ps1` 已从默认 route-only 改为完整 outcome
门禁，保留 `-FullOutcome` 调用兼容，但不再有默认部分通过的含义。原 fixture
完整 12 × 6 模式覆盖 managed/nested finally、typed/multi-catch、loop owner 和
显式新异常覆盖。新增 `xlir_pending_exception_control.xl` 覆盖 return、break、
continue、finally 内部捕获、嵌套 pending，以及实际未完成 Future 的 resolve /
reject 与 generator yield。host 在完成 promise 之前断言输出仍 PENDING、输入
尚未完成、两个 lexical owners 仍存活且原诊断不在 TLS；因此不是 ready-Future
代替真实挂起。显式 await reject 使用有效 999 诊断，不伪造公共 Promise API
不支持的 failed-with-NULL 终态；可空原异常与真实 OOM 在 primary 调用中注入。

发布版完整 pending 门禁已通过，证据
`build/tmp/xlir_pending_owner_pending_exception_route.log`：

| 检查 | 本批结果 |
| --- | --- |
| 原 full-outcome strict | 7,200 次 / 1,300 次真实诊断 OOM / 247,400 次平衡分配 |
| 新 control strict | 4,800 次 / 1,100 次真实诊断 OOM / 142,100 次平衡分配 |
| 原 full-outcome C-host JIT / AOT | 各 6,000 次，跳过仅严格 allocator 可注入的 OOM 模式 |
| 新 control C-host JIT / AOT | 各 4,000 次，实际 await/yield 路径均执行 |
| 原 route 契约 | 50 个 typed/lowered plans / 13 unwind regions / 17 releases / 675 次拒绝 |
| 新 owner 契约 | 19 owners / 47 restores，其中 31 个 cleanup-preservation / 42 cancellations / 2 error frames / 1,029 次拒绝 |

两套 C/header 都重复生成并逐字比较。owner 契约包含表边界、capture/owner/body/type
误绑定、父链自环、全表和全部 tags 同时移除、恢复原因丢失、原 SSA 误用、平衡取消
代替恢复，以及 async/generator 的错误析构、transfer、init bit 和 residence
变异；每个拒绝同时要求 verifier、有诊断且 C/header 零输出。strict 分配快照验证
live bytes/count、总 alloc/free，以及 invalid free / double free / UAF 无增量。

完整重建 `build/tmp/xlir_pending_owner_release_build.log` exit 0。compiler VFS
532 文件 / 32,629,702 字节 / 压缩 4,635,608；runtime VFS 115 文件 / 5,515,719
字节 / 压缩 859,501。发布 `xlc.dll` 10,661,888 字节，SHA-256
`8795791DC9847A859D34C1A3013DB2642AC6972E4F8D3AA161F2AF6F00E15BED`；
`xl.dll` 6,785,536 字节，SHA-256
`DBDF9981F8FEBA5F1C98FC4B2E45F1D0F4B5695AC71BDF0109464F87DE307F70`。
相对上一节发布版分别增加 13,312 / 10,752 字节；两种 DLL 都编入 IR/lowering，
runtime VFS 不变不代表其 DLL 机器码不变。

边界仍须区分：本批没有验收 cancelled/closed Future 的终态清理矩阵，也不将两条
挂起 probe 外推为任意 continuation/capture 组合全部正确。局部 `var` 放入 owning
product return 的 `xlir_product_local_return_boundary.xl` 拒绝仍是独立语言缺口，
不是通过恢复 pending-error 得到修复。标准库原有准入缺口继续按最低标准保留，
没有为提升通过率放松语言 verifier 或降低准入阈值。

完整准入 `build/tmp/xlir_pending_owner_admission.json`：543 候选、legacy 523、
共同 XLIR 511、12 缺口、执行失败 0、CompilerStable=true，报告 DLL hash 与本批
发布版相符。逐项对比上一节的 542 候选：旧成功回退 0、旧拒绝开放 0、缺口集合
变化 0。唯一新增 `xlir_pending_exception_control.xl` 在两个后端都能生成 C；本批
运行结果仍按 XLIR-only host 契约统计，没有把 legacy 编译成功说成运行差分通过。
531 / 514 / 514 / 0 的准入门槛未变，所以仍 exit 1。缺口仍是 10 项 TCP Future
native owning layout、1 项 protocol native-backed class layout、1 项 xid intrinsic。

七项本批静态门通过：source wiring 286 / 49 / 236 / 6 / 2，batch 11 / 67，迁移
覆盖 125 semantic rows / 371 representatives / 429 deterministic candidates /
143 runtime differentials / 63 XLIR-only contracts，core C fixture 484 / 11，
encoding 5,814，Markdown 329，以及基础 XLIR contract。全局 case wiring 的
37 项和 extlib serial 工具接线两条既有错误仍保留。额外执行非 CoreOnly 的 C
fixture 检查还报告 mqtt / serial 共 7 个 package fixture 接线问题，日志为
`build/tmp/xlir_pending_owner_static_c_fixture_wiring_full.log`；没有把它计入 core
通过数或悄悄修改外部库元数据。

额外检查 `check_xlir_current_core_contract.ps1` 发现旧检查对 chained string
receiver 要求整个指令集合中只有一个 RELEASE，却没有区分成功和异常分支。
它在上一节 hash 的旧发布 DLL 和本批新发布 DLL 上均失败；保留基线
`build/tmp/xlir_pending_owner_baseline_core_string.json` / `.c` 和新结果。
排除本批新增的两个零值 pending 字段后，其 instruction JSON 完全一致，生成 C
的 SHA-256 两版同为
`804BE88834BD223CC34FB67972265732CC66D61E04C8465C2CFF8BB2AECF78BA`。
这项额外旧门禁需要按 CFG owner 路径校正，不能将其失败称为本批语言回退，也没有
通过放宽 RELEASE 数量来假装完成所有权证明。

多值返回边界在本批发布版仍 exit 1，日志
`build/tmp/xlir_pending_owner_product_local_return.log`。其 typed dump 中，返回
NAME 的 expression TypeId 为 5，而 declared storage TypeId 为 4：普通 NAME
lowering 会物化已证明的 flow narrowing；transferred-return helper 却把普通
expression lowering 的结果直接要求为 storage TypeId。后续应在 IR 中分清
物理 owner LOAD 与值投影，验证 representation-preserving transfer，不能删掉
精确 TypeId 检查或交由 Cgen 猜测。

最终在上述同一发布 DLL 上串行复验 24 项专项全部 exit 0，汇总
`build/tmp/xlir_pending_owner_regression_results.json`，逐门日志为
`build/tmp/xlir_pending_owner_<gate>.log`。包含升级为 full-outcome 的 pending
exception 门、parameter / scope / loop / abrupt cleanup、return commit、error
unwind、async multi-await、generator、protocol iterator、module init/fini、class
lifecycle / constructors、copy/clone、evaluation lifetime、foreign error scope 和
null ABI。error-unwind 的运行计数仍为 36,000 / 12,000 OOM / 724,500 平衡分配；
新增 pending owner 的物理释放使 membership 由 236 增至 238，变异拒绝由 1,662
增至 1,666，原有 83 项 call completion 和 49 个 normal cleanup regions 仍通过。
parameter 的 433 项、loop 的 1,549 项契约也无回退。发布 hash 在完整回归结束后
再次核对一致，16 个本批修改文件行尾空白为 0，`__xlvfs__/module.c` 不存在。
本批构建、准入与专项进程均已结束，长期任务保持 active；下一批优先解决上述
语言返回边界，不将剩余 12 个准入缺口、外部库接线或未覆盖的 continuation 语义
报为完成。

## 2026-09-07：返回的物理 carrier、SSA 所有权和 product 输入取得证明

上一节的局部 `var` 多值返回边界已修复。`NAME` 的 flow-narrowed expression
TypeId 与实际存储 carrier TypeId 是两个不同事实：观察一个已知为 class 的
`var` 可以投影为 class，但转移这个 local owner 必须直接 LOAD 其物理存储。
本批将 transferred-return 和同 carrier 的 nullable 边界接入已有的物理读取
helper，最终以注册 storage 的 TypeId 为准，保留 LOAD/MOVE 精确 TypeId 检查。
合法 class → var widening 仍使用表示不变的 VALUE；Cgen 不补转换或追溯 AST。
原始最小复现现在生成 C 成功，证据
`build/tmp/xlir_return_projection_repaired_boundary.c` / `.log`。

扩展为重复返回同一对象后，真实 allocator 检查又暴露了第二个问题：一次转换
已经生成独立 retain 的 SSA owner，product lowering 却再次根据源 NAME 的
borrow 属性 retain，造成对象泄漏。`xl6IrValueOwnershipGet` 现在统一表达
lowered producer 的 none / borrow / copy / move，供普通 lowering 和 continuation
spill 分析共享，并作为派生字段 `resultOwnership(Name)` / `result-ownership`
输出到 JSON / text dump。VALUE 和 nullable 表示包装保留 operand 的所有权；
storage LOAD、CELL_LOAD 是借用，不因 source annotation 而变成独立 owner。

这不是以操作名含有 borrow 与否作启发式分类。实际 runtime ABI 中，container /
iterator / dynamic-field 的 11 个 managed-result 操作返回借用视图，必须明确
分类；例如 raw-pointer-borrow 则可产生独立拥有的装箱 handle。初次扩展的过宽
分类在 dynamic field 读取后触发了运行期释放错误，已按实际 ABI 修正。未知
managed producer 保持阻断；runtime opcode 的有效性仍由其独立 ABI verifier
检查。共享查询不是完整验证器，也不证明一个 owner 尚未被其他消费者取走。

PRODUCT_MAKE 的 managed 字段现在要求独立的 owning SSA，或本 construction
block 内精确匹配的物理 LOAD/MOVE。两字段不能重复消费同一个取得身份。
新增破坏性测试进一步证明，仅检查 temporary MOVE 不够：把 copy/retain 改成
借用 VALUE 后，临时 STORE/MOVE 曾掩盖所有权缺失。最终实现递归验证 temporary
的唯一初始化及其取得来源，使用 CFG 检查初始化支配读取，并穿过表示包装保留
原始取得身份；两个不同 temporary 不能把同一个 SSA owner 伪装成两份所有权。
直接 NULL 可重复，已激活再 detach 的 storage lifetime 仍须单次消费。

本批没有宣称完成全局 affine-use 验证：任意可变 local 的多次 STORE、跨不同
consumer 的 owner 重用，以及所有 managed SSA 的全路径消费证明仍需独立推进。
不确定的 temporary 多初始化路径没有交给 Cgen 猜测或放宽为任意一个 STORE。
本轮生产修改仅涉及 IR header / verifier / lowering；Cgen 文件未新增修改。

新增 `check_xlir_return_projection_owner.ps1` 已接入函数 ABI suite、迁移覆盖和
基础 XLIR 静态 contract。9 条语言路径覆盖 single / product return、重复 alias
及共享对象可变字段、class widening、装箱 int/bool/string、narrowed nullable
string、后续表达式失败、finally 继续读原 local，以及带 `item = item` 的真正
可变 capture cell。7 种 host 模式分别注入成功、析构失败的有效/空/OOM 诊断、
后续表达式失败的有效/空/OOM 诊断，验证对象存活、析构顺序和返回回滚。
nullable helper 的空结果分支参与 lowered IR / 变异检查；本 fixture 的正常运行
选择非空分支，不把它计为额外空 payload 运行覆盖。

同一发布 DLL 的新门禁已通过，日志
`build/tmp/xlir_return_projection_return_projection_owner.log`：

| 检查 | 本批结果 |
| --- | --- |
| strict runtime | 6,300 次 / 1,800 次真实诊断 OOM / 135,700 次平衡分配 |
| C-host JIT / AOT | 各 4,500 次，跳过仅严格 allocator 注入的两种 OOM 模式 |
| producer ABI 检查 | 546 次分类检查，其中实际 fixture 有 4 个 borrowed CELL_LOAD |
| 输入所有权契约 | 9 products / 17 moved fields / 11 narrowed physical storage reads |
| 变异拒绝 | 79 次，含 8 次重复字段、2 次不同 temporary 重用 owner、7 次 borrowed STORE |

所有变异要求 verifier 拒绝、存在诊断且 C/header 零输出。C/header 重复生成
逐字一致；strict 检查 live bytes/count、alloc/free 和 invalid free / double
free / UAF 无增量。最初借用 STORE 逃逸的证据保留于
`build/tmp/xlir_return_projection_owner_borrow_store_probe.log`，最终扩展源契约
为 `build/tmp/xlir_return_projection_owner_contract_expanded.log`。

完整重建 `build/tmp/xlir_return_projection_repaired_release_build.log` exit 0。
compiler VFS 532 文件 / 32,641,004 字节 / 压缩 4,637,276；runtime VFS 115
文件 / 5,515,719 字节 / 压缩 859,501。发布 `xlc.dll` 10,665,472 字节，SHA-256
`90391FF7EDD7D8063AB1C8B4139878BFAF1F6D6EA448FE58D9BA41D5AA43D229`；
`xl.dll` 6,788,096 字节，SHA-256
`ADBE31C1BA1C10979983DFE450E570457233A8CD50C66C3525DA02C0B8C2B599`。
相对上一节分别增加 3,584 / 2,560 字节；没有再次嵌入整份 runtime 头。

上述是本批首版发布记录，不是最终全量验收结论。其准入报告
`build/tmp/xlir_return_projection_admission.json` 为 544 / 524 / 511 / 13 gaps /
0 execution failures / CompilerStable=true。逐项发现旧成功用例
`cgen_var_return_exprs.xl` 回退，不能因总 XLIR 成功数仍为 511 而忽略。串行回归
在前 7 项通过后停止，仅停止本次验证进程树，保留首版报告，未继续混用新源码和
旧 DLL 的验证结果。

根因不只是“多 STORE 不唯一”：三元表达式与 xvalue `??` 的 merge temporary
原来仅 LOAD，没有显式 MOVE，就把观察值写入下一个 owning temporary。最终版
将 managed conditional / coalesce 合流改为明确的 STORE → LOAD/MOVE；managed
string `??` 也不再用跨分支重赋 VALUE 隐藏 owner 合流。多初始化 temporary
逐分支证明输入取得，并以 CFG inactive/active 状态验证每条路径恰好一次激活、
读取前已激活且终态前 detach/cleanup，保留该合流存储的路径相关取得身份。这不
是接受任意一个 STORE，也不是回退到 Cgen 解释源表达式。

源构建已修复原代表用例，证据 `build/tmp/xlir_return_projection_join_exprs.c`
/ `.json` / `.log`。扩展源 runtime 覆盖 15 路 × 7 模式，条件表达式、xvalue
coalesce 与 nullable-string coalesce 的两分支均实际执行：10,500 次、3,000
次真实诊断 OOM、225,600 次平衡分配；日志
`build/tmp/xlir_return_projection_join_owner_runtime.log`。源契约为 12 products /
23 moved fields / 13 narrowed reads / 736 producer checks / 110 mutations；包含
4 个 path-dependent owners 和 4 次绕过 acquisition 的 CFG 变异，日志
`build/tmp/xlir_return_projection_join_owner_contract.log`。原
`cgen_var_return_exprs` 的独立 C ABI host 已加入新门禁，防止以后只测新增 fixture。

扩展时另外隔离出一个前端语言边界：局部
`var optional = present ? item : null` 后使用 `optional ?? item` 被 sem 当作
不支持 coalesce 的左值拒绝。最小复现和诊断保留在
`build/tmp/xlir_conditional_null_flow_boundary.xl` / `.log`。新 owner runtime
通过返回类型为 `var` 的 helper 保持开放 carrier 来单独验证所有权，不把这一
隔离方式说成前端 nullable-flow 已修复。此项属于语言本身，不能列入“标准库最低
标准”后搁置；后续必须验证分支 type join / narrowing 是否丢失 null 成员。
只读定位已确认 `xl6SemExprStaticTypeName` 的 CONDITIONAL 分支以及
`xl6SemInferAutoTypeNode` 的 conditional 分支，都在一侧为 null 时直接取另一侧
类型而未构造 nullable join；修复应统一前端的类型合流事实，不应在 IR/Cgen
对该例另加例外。

最终合流版完整重建 `build/tmp/xlir_return_projection_join_release_build.log`
exit 0。compiler VFS 532 文件 / 32,643,202 字节 / 压缩 4,637,518；runtime VFS
仍为 115 / 5,515,719 / 859,501。`xlc.dll` 为 10,666,496 字节，SHA-256
`CAE1C6CB6A64D920832FA1350A3474F7BE26E21403E9B32EC256E8D389537E8D`；
`xl.dll` 为 6,788,608 字节，SHA-256
`1A20E5952C3CF21D2B66F13BE9BABFC8E6D39A866DBD48AEEA06AE9A6B63B77C`。
相对上一节 pending-owner 发布版分别增加 4,608 / 3,072 字节。15 路新门禁在此
发布版也已通过：strict 10,500 / 3,000 / 225,600，JIT/AOT 各 7,500，110 次
变异拒绝，以及原代表 fixture 的独立 C ABI host；日志
`build/tmp/xlir_return_projection_join_return_projection_owner.log`。

最终准入 `build/tmp/xlir_return_projection_join_admission.json` 为 544 候选 /
legacy 524 / 共同 XLIR 512 / 12 gaps / 0 execution failures /
CompilerStable=true，DLL hash 与最终发布版一致。相对上一节 543 候选逐项对比：
旧成功回退 0、旧拒绝开放 0、gap 集合差异 0；新 fixture 两后端均生成 C。
`cgen_var_return_exprs` 已恢复两个后端成功，首版回退没有被总数掩盖。新增运行
证据仍按 XLIR-only host 契约统计，不冒充新增 legacy runtime differential。
531 / 514 / 514 / 0 门槛不变，所以整体准入仍 exit 1。

最终版七项静态门通过，证据 `build/tmp/xlir_return_projection_join_static_<gate>.log`：
source wiring 286 / 49 / 236 / 6 / 2，batch 11 / 67，migration 126 semantic
rows / 372 representatives / 430 deterministic candidates / 143 runtime
differentials / 64 XLIR-only contracts，core C fixtures 487 / 11，encoding
5,819，Markdown 329，基础 XLIR contract。全局 37 项 case wiring、serial 工具
两条接线错误和旧 current-core string-chain 检查仍未改动；最后一项生成 C 的
SHA-256 仍与上一节基线同为
`804BE88834BD223CC34FB67972265732CC66D61E04C8465C2CFF8BB2AECF78BA`。
不把“静态覆盖候选数”当作本批已经执行全部 143 个 runtime differentials。

最终同一发布版的 25 项串行专项全部 exit 0，汇总
`build/tmp/xlir_return_projection_join_regression_results.json`，逐门日志
`build/tmp/xlir_return_projection_join_<gate>.log`。包括新返回 projection owner、
full pending exception、parameter / scope / loop / abrupt cleanup、exit route、
return commit、error unwind、copy/clone、generator、protocol iterator、core
expression、async multi-await、module init/fini、class lifecycle / constructors、
evaluation lifetime、foreign error scope 和完整 null ABI。后者还覆盖现有 tagged /
xvalue / raw-pointer null 表示、lazy coalesce、closure/cell/async frame，以及
带显式拒绝边界的 indexed null-aware assignment，不能把它外推为所有 RHS 都支持。

关键旧证据无回退：error unwind 36,000 次 / 12,000 OOM / 724,500 平衡分配，
1,666 region mutations、83 call-completion mutations、49 normal cleanup regions；
loop cleanup 12,000 / 2,000 / 679,200，1,549 mutations；pending full/control
计数与上一节一致，owner 的 1,029 mutations 和 parameter 的 433 mutations
均通过。构建、准入和专项进程均已结束，没有将首版中断记录计入最终 25 项。

最终 hash 已再次核对，13 个本批修改文件行尾空白为 0，Markdown 329 文件检查
通过，`__xlvfs__/module.c` 不存在。长期任务仍 active：下一批优先修复已定位的
conditional nullable type join，再推进全局 managed SSA / local-store affine-use
证明、未验收的 cancelled/closed Future 终态及其他显式语言拒绝边界。12 个准入
缺口和全局测试接线债务没有被清零或改低阈值；本批只完成了上述返回所有权及合流
范围的实施与验证，不宣称全部语言工作完成。

### 2026-09-07：conditional 可空类型汇合、分支状态与独立拥有值

本批承接上一节的 nullable join 边界。`cond ? value : null` 原先丢掉
`null` 分支，只推断为 `value` 的类型；随后 `??` 会把本来可空的表达式拒绝为
不可空。当前由 `xl6SemInferConditionalType` 统一完整类型域的汇合规则：
`T / null -> T?`，相同 payload 的 `T / T? -> T?`，已有 nullable 不重复嵌套；
`var`、`point`、`error` 保持各自原有的可空类型域。不同非空 payload 仍按既有
策略汇合为 `var`，没有增加隐式数值提升或猜测公共基类。

三元表达式在推断分支类型及检查分支正文时应用各自条件收窄，并恢复分支外状态。
条件本身执行后的状态是两条分支的共同入口，确定赋值取交集；任一分支发生的写入
都会使相应外层类型事实失效。`and`、`or`、`??` 的惰性右侧不再无条件贡献确定
赋值，只有字面量确定必执行的 `true and`、`false or`、`null ??` 保留右侧赋值。
专门的 `out` 参数检查也遵循相同分支规则，并把调用的直接 `out` 写回放在全部
实参求值之后。条件检查使用完整类型，nullable bool 在语义层就被拒绝。

`var` 保存可空类时保留完整 nullable 类事实，而不是降成“未知 var”或错误声称
非空。NAME lowering 复用已有 nullable xvalue projection，把精确 payload、
运行时类型身份和独立引用固定为指令。`??` 使用求值后 SSA 的实际 TypeId 建立
owner anchor，不再把物理 var 槽误当作表达式类型。判空直接读取 NAME 的物理
sentinel，避免先生成拥有引用、再为一个观察操作丢失该引用。

严格内存测试还定位了借用字符串分支的双重释放：分支合流只复制了指针，两个局部
变量随后都释放它。现在 conditional merge 和 product/return 共用
`xl6IrLowerOwnedValueBoundary`，在消费边界显式取得字符串副本或托管引用。
新建/已拥有的值不重复复制。类型和所有权修复均在语义层 / lowering 完成。

新门禁 `tools/check_xlir_conditional_nullable_join.ps1` 已接入
`03_core_function_abi.bat`、迁移覆盖、XLIR contract 和测试入口说明，覆盖标量、
nullable string、普通/新建类、var 中的可空类、数组、闭包及原生可空类型域。
首轮发布版（后续组合测试仍发现问题，不能作为最终验收）新门禁结果：

- 24 个语义正负例、13 个 nullable conditional、1 个精确 nullable 类 projection；
  3 个 projection 类型/payload/运行时身份变异均在 C/header 输出前被拒绝。
- 严格 C 运行 400 次，8,200 次分配全部平衡；无新增无效释放、重复释放或
  use-after-free，析构顺序与惰性选择均符合预期。
- C-host JIT、AOT 各 400 次通过；C/header 重复生成逐字节一致。
- 日志：`build/tmp/xlir_nullable_join_conditional_nullable_join.log`。

首轮完整 `build_dll.bat` 成功，日志
`build/tmp/xlir_nullable_join_release_build.log`。compiler VFS 532 项，
32,648,320 bytes 压缩为 4,638,770 bytes；runtime VFS 115 项，
5,515,719 bytes 压缩为 859,501 bytes。发布文件：

- `xlc.dll`：10,669,568 bytes；SHA-256
  `E926C42040BF767B025A3D7987B0535ECFA82379339E98211DE6F68E213037A2`。
- `xl.dll`：6,790,656 bytes；SHA-256
  `D90F30085440FFC97D2C5B94A023E85E24448FB81107E0AE7E25B79903E63342`。

相对上一节，本批 sem/lowering 合计源码增加 5,118 bytes，其中 lowering 减少
258 bytes；两个 DLL 分别增加 3,072 / 2,048 bytes。没有恢复旧后端默认值，
也没有修改准入阈值。

范围限制：上述结果不是全局 affine-use 证明，尚不能保证所有 managed SSA 和
local STORE 的任意组合都被验证器覆盖；新分支所有权问题首先由严格运行时测试
发现。`out` 的全部 try/catch/异常完成路径、普通未初始化复合局部变量的调用
实参事务、cancelled/closed Future 终态仍需独立验收。标准库缺口依然按最低
可用边界处理，不据此降低语言语义要求。后续准入及串行回归记录另列如下。

首轮准入 `build/tmp/xlir_nullable_join_admission.json` 为
545 / 525 / 512 / 13，execution failures=0、CompilerStable=true；新增用例
两后端通过，但原先通过的 `perf_json_ast_lifecycle.xl` 回退。它的三元分支现在
正确应用 `is int` 收窄，而 checked integer intrinsic 已冻结的 operand ABI 仍
是 var。已将 checked / wrapping integer 转换接入物理 NAME 读取，保留经过
验证的 conversion ABI，未改验证器去容忍相互矛盾的操作数类型。

把该转换加入连续运行测试后又暴露安全索引的遗留错误状态：`OR_NULL` 操作对
空容器调用 XRT count/get，得到正确的 null 结果，却留下 TypeError，直到下一条
checked 指令才失败。当前修复既有 array/list/dict 安全索引 RuntimeOp 的空
sentinel 行为，相关 C runtime helper 在调用 count/get 之前直接返回 null。
这是固定 RuntimeOp 的实现修正，不是 Cgen 重新判断 AST 或推测语言类型。
新用例已增加 nullable list/dict、后续 checked conversion 和宿主直接错误状态
检查。最终结果需用修复后的完整发布版重跑，不能与首轮 7 个已通过、随后主动
停止的串行回归混用。

修复后的完整发布版构建日志为
`build/tmp/xlir_nullable_join_final_release_build.log`，compiler VFS 仍为 532 项，
32,649,307 bytes 压缩为 4,638,915 bytes；runtime VFS 与首轮一致。
当前发布 `xlc.dll` 为 10,670,080 bytes，SHA-256
`96DC47BD2FFA4FD457F2359784DB71C79DB93E3F249A91B66F47F52A87E9C790`；
`xl.dll` 为 6,790,656 bytes，SHA-256
`1D568BAAB087DD9402C3BF96792A4E2191CFDB39B2875D617291BA2961B47949`。
相对上一节，5 个生产源码文件合计增加 6,105 bytes；Cgen 只增加安全 RuntimeOp
的 sentinel 处理，没有增加 AST 回查。两个 DLL 分别增加 3,584 / 2,048 bytes。

新门禁最终日志
`build/tmp/xlir_nullable_join_final_conditional_nullable_join.log`：24 个语义
正负例、15 个 nullable conditional、1 个精确 nullable projection、1 个物理
转换载体检查；4 个 IR 变异均被拒绝。严格 C 400 次 / 13,000 次平衡分配，
C-host JIT、AOT 各 400 次通过，C/header 确定性及原始 perf 用例 Cgen 均通过。
另一个按求值顺序写入 nullable 后再读取成员的探针，也在 sem 阶段正确拒绝。

最终准入 `build/tmp/xlir_nullable_join_final_admission.json`：
**545 candidates / legacy 525 / common XLIR 513 / 12 gaps / 0 execution failures**，
CompilerStable=true，编译 DLL hash 与上述最终版本一致。相对上一节 544 个
候选，唯一状态变化是新增 positive fixture 两后端通过；旧成功回退数为 0，
旧拒绝被意外开放数为 0，gap 集合变化为 0。阈值仍为 531 / 514 / 514 / 0，
因此该门禁仍 exit 1，不能将 513 个通过表述为全局准入全绿。12 个缺口仍为
10 个 TCP Future owning layout、1 个 native-backed protocol layout、
1 个 stdlib xid intrinsic。

最终同一发布版 **28 项串行专项全部 exit 0**，汇总
`build/tmp/xlir_nullable_join_final_regression_results.json`。逐门日志为
`build/tmp/xlir_nullable_join_final_<gate>.log`，包含新增 nullable join、整数
转换、nullable collection safe-index 及上一节完整的 25 项所有权 / cleanup /
async / generator / class / module / null ABI 回归。返回所有权仍为
10,500 次 / 3,000 OOM / 225,600 平衡分配、110 个拒绝变异；loop cleanup
仍为 12,000 / 2,000 / 679,200、1,549 个变异；error unwind、pending
exception 与上一节关键计数一致。没有把首轮被中断的 7 项算入这 28 项。

最终静态检查汇总 `build/tmp/xlir_nullable_join_final_static_results.json`：
源码接线、batch、迁移覆盖、CoreOnly C fixture、编码、Markdown、XLIR contract
7 项通过。统计为 286 / 49 / 236 / 6 / 2 源码接线，11 / 67 batch，
127 semantic rows、373 representatives、431 deterministic candidates、
143 runtime-differential candidates、65 XLIR-only contracts，490 / 11 Core C
fixture，5,824 文本文件和 329 Markdown 文件。静态候选数仍不代表本批运行了
所有 runtime differentials。

已存在的 37 个 case wiring 错误、extlib_serial 的 2 个 tool wiring 错误未变化。
单独复查 `check_xlir_current_core_contract.ps1` 仍在 chained string 方法的
全 instruction-arena RELEASE 数量断言失败；生成 C 与此前基线哈希仍完全一致：
`804BE88834BD223CC34FB67972265732CC66D61E04C8465C2CFF8BB2AECF78BA`。
日志 `build/tmp/xlir_nullable_join_final_static_xlir_current_core_contract.log`。
没有改低此门禁，应后续以 CFG 路径证明替代全局计数。全量非 CoreOnly C fixture
接线本批未重跑，不能据 CoreOnly 通过宣称外部 package 接线全绿。

最终发布 hash 再次核对一致；15 个本批修改文件行尾空白为 0，
`__xlvfs__/module.c` 不存在。所有本批构建和专项进程已收集完成结果。
长期任务保持 active；下一阶段继续全局 managed SSA / local-store 消费证明、
尚未验收的 nullable 组合与异常赋值路径、cancelled/closed Future 终态，
并区分语言边界与最低标准库缺口，不将本轮限定范围的通过外推为任务全部完成。

## 2026-09-07：所有托管存储输入的独立取得与 match 拥有值汇合

本轮从上一节的 nullable string 复制边界继续向一般存储规则收敛。
旧验证器的反例已实际复现：将 `STRING_NULLABLE_COPY` 改为同类型的透明
`VALUE`，借用值仍被当成 owning STORE 输入而通过验证。
`build/tmp/xlir_storage_owner_baseline.exe` 输出 `accepted=1`，因此不能仅靠
nullable conditional 的语法形态检查代替所有权验证。

生产变更集中在 `src/ir/xl6_ir.c` 和 `src/ir/xl6_ir_lower.c`，没有改动 Cgen：

- `xl6IrVerifyManagedStorageInputs` 对 STORE、PRODUCT_STORE_MOVE、CELL_NEW、
  CELL_STORE 的托管载荷要求独立 SSA owner 或已显式分离的存储 owner。
  CELL_NEW 按 cell 的载荷类型检查，不把捕获一个普通整数误当成转移 cell 指针。
- 原 product 输入的取得证明推广为 `xl6IrOwnedInput`；MOVE 必须由对应 LOAD
  支配，并支配消费位置，因此也支持在前驱块完成分离后进入 nullable 分支。
  借用值经过 VALUE/nullable 包装仍是借用，STORE/MOVE 不能洗成独立 owner。
- 补全 virtual-call 的已验证托管返回值分类；generator 循环按 LOAD、MOVE、
  STORE 顺序转交已保存的 activation，不再先写新 owner 后清空旧存储。
- 托管 scalar/tagged-enum match 改为每分支 STORE、汇合后 LOAD/MOVE，
  不再用跨分支 VALUE 重赋值隐含共享结果。常量 false 的 TRAP 是明确的终止
  路径，CFG 证明不把其后的结构性 JUMP 当作可执行的未初始化入口。
- xvalue 装箱按实际 producer / 显式 MOVE 判定拥有权，移除 NAME、CALL 和
  source value-category 的专用转移捷径。借用 class nullable 字段进入新存储
  会显式 retain；临时动态对象的字段先取得 owner，再释放其父对象。
- checked 整数转换通过已有 evaluation-owner 栈保护输入，在成功和异常路径
  都释放拥有型借用锚点；枚举装箱复制字符串/闭包等载荷后显式关闭原临时 owner。

第一轮通用检查检出 14 个既有代表的所有权边界问题/分类缺失。修正后，源码
候选编译器对 493 个可直接解析的既有成功代表全部通过，报告
`build/tmp/xlir_storage_owner_v3_source_audit.json`。此简化 driver 未配置包解析，
另外 33 个 import/extlib 代表仍须以正式 CLI 准入结果为准，不把 driver 的
undeclared identifier 视为发布编译器回退。v3 后又增加 checked 输入和枚举装箱
泄漏修复；最终发布验证采用下面的完整重建版本。

第一轮成功完整构建 `build/tmp/xlir_storage_owner_final_release_build.log` exit 0：
compiler VFS 532 / 32,655,328 bytes 压为 4,639,647 bytes；runtime VFS
115 / 5,515,719 压为 859,501 bytes。`xlc.dll` 为 10,673,152 bytes，SHA-256
`6F0700A72FD8DF3F70D83F49D96E8C9B65BDED733FD40B6B915D7B9B56B831BA`；
`xl.dll` 为 6,792,704 bytes，SHA-256
`877EFD8B35049198F9EBC4B1BCF34812D2AFF764C5C96B44A8E926467524ADA8`。
相对上一节两个生产源码分别增加 3,836 / 2,185 bytes，共 6,021 bytes；
两个 DLL 分别增加 3,072 / 2,048 bytes。首轮构建因局部变量声明位置错误失败，
已修正并完整重建，不能将 `xlir_storage_owner_release_build.log` 当作成功版本。

新增 `tools/check_xlir_managed_storage_inputs.ps1` 已接入 core function suite、
迁移矩阵与静态 contract，正式日志
`build/tmp/xlir_storage_owner_managed_storage_inputs.log` exit 0：

- 62 个 STORE、2 个 CELL_NEW、2 个 CELL_STORE 借用输入变异；2 个 match
  owner 转移、11 个显式分离输入、1 个 nullable copy 变异，共 80 个无效 IR
  均在 C/header 输出前拒绝，恢复原 IR 后仍验证通过。
- 严格 C 400 次 / 36,600 次分配释放平衡；无新增 live allocation、invalid
  free、double free 或 use-after-free 计数。C-host JIT、AOT 各 400 次通过。
- 覆盖动态临时对象的嵌套字段、已借用 nullable 字段在原字段替换后的存活、
  虚返回、scalar/tagged match、string/xvalue 可变捕获、generator 所有权转交，
  以及 checked 转换抛错时的临时值回收。C/header 确定性及 7 个原始代表 Cgen 通过。

边界仍需明确：本轮证明的是每个托管写入输入已经取得所有权，不是全局
managed SSA 单次消费证明。所有 RuntimeOp 的 consuming/borrowing 输入效果、
重复消费和 use-after-MOVE 的完整跨 CFG 验证、枚举多载荷求值失败时的构造事务、
cancelled/closed Future 终态等仍未全部验收；长期任务保持 active。
完整 CLI 准入 `build/tmp/xlir_storage_owner_final_admission.json` 为
**546 candidates / legacy 526 / common XLIR 514 / 12 gaps / 0 execution failures**，
CompilerStable=true，compiler DLL hash 与上述重建版本一致。与上一节逐文件
比较，旧候选的两后端状态变化为 0、旧成功回退为 0、gap 集合变化为 0；仅新增
本轮 fixture，两后端 Cgen 均通过。阈值仍为 531 / 514 / 514 / 0，12 个既有
gap 未消除，所以正式准入仍 exit 1，不能称为全绿；513→514 也不是修复一个
旧 gap。执行失败计数指编译器进程失败，不代表运行过全部 runtime differential。

附加 trap 变异契约 `build/tmp/xlir_storage_owner_trap_contract.log` 已通过：
将两个 match 不可达分支的 false trap 改为 true，使其实际进入未初始化的
merge，验证器同样拒绝。当前契约总数为 82 个拒绝变异；最初正式门禁日志
中的 80 个是增加此测试之前的版本，不能混淆计数来源。

最终静态汇总 `build/tmp/xlir_storage_owner_final_static_results.json` 中源码、
batch、迁移覆盖、CoreOnly C fixture、编码、Markdown 与 XLIR contract 共 7 项
通过。统计为 128 rows / 374 representatives / 432 deterministic candidates /
143 runtime-differential candidates / 66 XLIR-only contracts，493 / 11 Core C
fixture 和 5,829 文本文件。既有 37 条 case wiring 和 2 条 tool wiring 错误
与上一节逐行相同。非 CoreOnly 外部 package 接线本轮未全量重跑。
`build/tmp/xlir_storage_owner_current_core_contract.log` 仍因链式字符串的
全局 RELEASE 数量断言失败，生成 C 的 SHA-256 仍为
`804BE88834BD223CC34FB67972265732CC66D61E04C8465C2CFF8BB2AECF78BA`，与旧基线
一致；没有降低此检查的要求。

第一轮 28 项串行回归 `build/tmp/xlir_storage_owner_final_regression_results.json`
已全部 exit 0；包括 scope、class/module cleanup、generator、async multi-await、
error unwind、pending exception、return、null ABI 等原有全部专项。没有将旧的
current_core_contract 全局计数失败混入这 28 项，也没有声称它已修复。

随后复核常量路径证明时发现，旧 VALUE target 校验只比较 TypeId，允许重写
CONST 或 CALL 等普通 producer。现在只允许显式 VALUE 定义充当 merge target，
因此 CONST 在 trap 路径证明中不可被改写；增加两个“VALUE 目标替换为常量”
变异。最终补充后的重新构建、84 个变异及完整回归另以
`xlir_storage_owner_verified` 前缀保存结果，避免与上述第一轮证据混淆。

补充后的完整构建 `build/tmp/xlir_storage_owner_verified_release_build.log`
exit 0。compiler VFS 为 532 / 32,655,313 → 4,639,689 bytes，runtime VFS
数量与大小未变；`xlc.dll` 10,672,640 bytes，SHA-256
`AE66AC990515262EFD633268F752B3EB76CDABD32FC19A526C90C625B919EB55`；
`xl.dll` 6,792,704 bytes，SHA-256
`DBE90D40321744CB97260D0B34C8DF75E620F45DDCF1D64A119A2BF1DA1BD725`。
相对上一节最终版本，本轮两个生产源码净增 6,006 bytes；两个 DLL 分别净增
2,560 / 2,048 bytes，Cgen 源码未增加。删除不再需要的同块搜索后，最终 compiler
DLL 比本节第一轮成功构建少 512 bytes。

最终新增门禁 `build/tmp/xlir_storage_owner_verified_managed_storage_inputs.log`
已 exit 0：84 个无效 IR 变异均在 C/header 输出前拒绝；严格 C 400 次 / 36,600
次平衡分配，JIT/AOT 各 400 次，确定性 C/header 与原始代表回归均通过。
7 项静态检查的最终补充版报告为
`build/tmp/xlir_storage_owner_verified_static_results.json`，结果与上文相同。

未完成的单次消费边界已有独立反例，不能以本轮门禁通过掩盖：
`build/tmp/xlir_storage_owner_duplicate_probe.log` 向原本有效的 lowered IR
注入重复 STORE，使 v16 先进入 s67（instruction 29 / line 32），再进入 s76
（instruction 281 / line 56），当前验证器仍输出 `accepted=1`。该探针仅验证
注入后的 IR，没有生成或执行此无效程序；不是宣称原始正例有这个 lowering 错误。
它直接确定下一阶段要证明同一动态 owner 的路径敏感消费状态，覆盖透明别名、
分支互斥、循环重新取得与 RuntimeOp/调用/返回的 consuming operands，不能改成
简单的全 arena 使用次数计数。本轮新增门禁与全局 affine ownership 验收分开。

最终补充版准入 `build/tmp/xlir_storage_owner_verified_admission.json` 保持
546 / 526 / 514 / 12 / 0，CompilerStable=true，compiler DLL hash 为上述
`AE66AC...919EB55`。对上一节 545 个旧候选逐文件比较，两后端状态变化、旧成功
回退、gap 集合变化均为 0；新 fixture 两后端通过。门禁阈值未改，仍因 12 个
既有 gap 非零而 exit 1。最终补充版旧 core 计数门禁日志为
`build/tmp/xlir_storage_owner_verified_current_core_contract.log`，同一生成 C
哈希再次确认未变化。

最终补充版原有 **28 项串行专项全部 exit 0**，报告
`build/tmp/xlir_storage_owner_verified_regression_results.json`；加上新增存储
门禁，最终 **29 项全部通过**的合并报告为
`build/tmp/xlir_storage_owner_verified_all_results.json`。这 29 项均对应最后的
完整重建版本，不将第一轮的重复执行额外计入通过数量。返回所有权仍为 110 个
变异、736 个 producer 分类检查；loop cleanup 为 12,000 次 / 2,000 OOM /
679,200 平衡分配、1,549 个变异；error unwind 的 1,666 个 region 变异与 83 个
call completion 变异均通过，pending exception、async/generator、class/module、
scope、return、null ABI 的原有专项全部通过。

所有本轮构建、专项和准入进程均已收集结果，最终两个发布 DLL hash 再次核对
一致。12 个本轮修改文件无行尾空白，`__xlvfs__/module.c` 不存在，Markdown
链接复查通过。长期目标保持 active；下一阶段以已复现的重复 STORE 反例为
起点建立路径敏感消费验证，再推进构造事务和未验收的 Future 终态，不能将
本轮有限边界的验收替代完整语言所有权证明。

## 2026-09-07：路径敏感 SSA 消费效果与 catch 存储转交

本轮以此前 `v16 -> s67 -> s76` 的重复 STORE 反例为起点，新增独立的
`src/ir/xl6_ir_ownership.c`。它由 `xl6_ir.c` 包含并接入正式 module verifier，
不是 Cgen 的后置修补，也不依赖 AST 名称或源表达式形态。

- `XL6_IrOperandOwnership` 区分 OBSERVE、CONSUME、ALIAS、DETACH；336 个非 NONE
  RuntimeOp 均明确列出输入效果，未知操作拒绝，新增枚举遗漏效果声明也会被静态门禁发现。
  direct/closure/virtual/protocol call 使用既定 parameter ABI；闭包 cell capture
  使用已经显式 retain 的 cell owner，其余 capture 沿原有复制/retain 契约借用。
- 新消费分析按 `(block, token-state)` 遍历 CFG。互斥分支可以各消费一次；进入汇合点
  的任一路径已消费则不能再次转交。循环体重新执行 producer 或 LOAD/MOVE 建立新的
  SSA 生命周期，但循环外取得的同一 owner 不能在回边上反复消费。透明 VALUE/nullable
  包装不改变身份；直接 NULL 可复制；一个产品的两个 consuming operands 不能共用 owner。
- 存储两侧属于相继转交的 token：STORE 消费输入，匹配的 LOAD/MOVE 再取得存储 owner。
  此分析不沿 STORE 反查初始化表达式，避免将一次合法转交两侧误算为重复消费。
- 第一轮源码 driver 对此前 493 个可直接解析的成功代表加新增存储 fixture 共 494 项
  扫描，491 项通过，3 项被新的输入状态检查拒绝。它们是 error_model、cgen_error_model
  和 pending_exception_outcome 的多 catch 转交：旧 lowering 使用 LOAD、STORE、MOVE。
  普通及 finally 版本均已改成 LOAD、MOVE、STORE；3 项和 12 个重点代表随后通过。
- STORE 不再允许省略物理载荷类型：primitive lowering 从已注册的目标存储冻结 TypeId，
  已给出的不匹配类型直接拒绝，module verifier 也要求 instruction/value/storage
  三者类型一致。此前 `type=INVALID` 的 catch STORE 不再绕过托管输入验证。

完整发布重建 `build/tmp/xlir_owner_consumption_release_build.log` exit 0：
compiler VFS 533 / 32,687,430 bytes 压为 4,645,214 bytes，runtime VFS
115 / 5,515,719 压为 859,501 bytes。相对上节 compiler VFS 原文增加 32,117 bytes，
压缩后增加 5,525 bytes；Cgen 源码没有改动。

- `xlc.dll`：10,681,856 bytes，SHA-256
  `7D6E3A1804A6399FAA00FA17B438B88DFF250314CE0874D3DC0E06CEBB410EE8`，增加 9,216 bytes。
- `xl.dll`：6,796,288 bytes，SHA-256
  `21D0344712EECCC4242600D24B8E1B510B903267B7765243B61514528C7258D5`，增加 3,584 bytes。

新增 `tools/check_xlir_owner_consumption.ps1` 已接入 core function suite、静态
contract 与迁移覆盖。首次完整发布门禁日志
`build/tmp/xlir_owner_consumption_owner_consumption.log` exit 0：336 个 RuntimeOp
效果分类、1,204 个 consuming inputs、1,113 个无效 IR 变异、15 个抽象 CFG
单元检查。所有变异在 C/header 输出之前拒绝，恢复原 IR 后验证通过。跨块 STORE
反例明确要求新消费诊断，不能由无关结构错误代替；6 个真实源码代表的 STORE TypeId
及确定性 C/header 同时通过。CFG 单元仅证明该分析的路径状态，不声称合成图满足
完整 module 的其他结构、类型、存储或异常契约。

正式准入 `build/tmp/xlir_owner_consumption_admission.json` 保持
**546 candidates / legacy 526 / common XLIR 514 / 12 gaps / 0 execution failures**，
CompilerStable=true，compiler DLL hash 对应本节构建。Sources、Results、Gaps 与
上节逐项比较差异均为 0，没有旧成功回退；12 个既有 ABI 缺口仍令门禁 exit 1，
不能声称全量准入通过。标准库最低范围不豁免 Future/native-backed protocol 的语言 ABI。

源码、batch、迁移覆盖、CoreOnly C fixture、编码、Markdown 和 XLIR contract
检查通过：287 sources / 49 direct / 237 included / 6 embedded / 2 generated，
129 semantic rows / 374 representatives / 432 deterministic candidates /
143 runtime-differential candidates / 66 XLIR-only contracts，494 / 11 Core C fixtures。
既有 37 条 test-case wiring、2 条 tool wiring 问题保持不变。
`check_xlir_current_core_contract.ps1` 仍因链式字符串的全局 RELEASE 计数断言失败；
生成 C 的 SHA-256 仍为
`804BE88834BD223CC34FB67972265732CC66D61E04C8465C2CFF8BB2AECF78BA`，没有放宽检查。

**未完成边界已有新的独立反例。** 本轮证明的是托管 SSA 转交在每条路径上至多一次，
尚不是完整 borrowed-value 存活、所有存储重新激活和 partial-product 生命周期证明。
`build/tmp/xlir_owner_borrow_after_release_probe.log` 在合法程序的 lowered IR 中，
将覆盖并释放旧字符串后的 STRING_COPY 输入改成旧 SSA，得到：
`Borrow of released owner accepted=1: v1 released from s3; STRING_COPY instruction 7`。
该探针只调用 verifier，没有生成或执行被注入的无效程序；原始正例没有这个 lowering
错误。下一阶段必须把借用来源与 owner 所在存储/作用域的存活关系压实，包括透明别名、
所有权转交、覆盖/释放及调用的影响，不能仅把重复消费门禁的通过外推为完整所有权安全。
构造事务、cancelled/closed Future 终态与原有 12 个 ABI 缺口也继续保留在长期目标内。

最终串行专项 `build/tmp/xlir_owner_consumption_regression_results.json` 的 **30 项
全部 exit 0**：新增消费门禁加上上一节的 29 项，均对应本节完整重建版本。包含
pending exception、parameter/loop/scope/class/module cleanup、return、generator、
multi-await、构造次序、foreign error 与 null ABI 等原有专项。loop cleanup 仍为
12,000 次 / 2,000 次真实诊断分配失败 / 679,200 次平衡分配、1,549 个拒绝变异。
未将既有 current_core_contract 计数失败和全量准入的 12 个缺口混入这 30 项。

补充将跨块反例的测试定位从固定 arena 编号改为同一函数的两个同类型 local STORE，
消除无关指令插入导致的脆弱断言；在隔离的 runtime DLL 目录重新编译、运行当前契约，
`build/tmp/xlir_owner_consumption_stable_contract.log` 再次为 1,113 个拒绝变异 /
15 个 CFG 检查。发布代码没有在专项运行中变化；两个 DLL 的最终 SHA-256 与本节
构建和准入一致。11 个本轮修改文件无行尾空白，`__xlvfs__/module.c` 不存在，
Markdown 链接复查通过。所有本轮构建、专项、准入与补充探针进程已收集终态结果；
长期目标保持 active，下一阶段从上述已释放 owner 的借用反例继续。

## 2026-09-07：借用存活验证与求值事务（开发中，未替换发布 DLL）

本节是源码工作检查点，不是新的发布验收。`release/xlc.dll` 仍为上节
`7D6E3A...410EE8`，正式准入仍以 546 / 526 / 514 / 12 / 0 为准；不得把下列
源码 driver 扫描或局部运行结果冒充最终 DLL 的全量回归。

新增 `src/ir/xl6_ir_lifetime.c`，接在消费分析后验证托管 SSA 的借用保护来源：

- 沿明确 VALUE/nullable 透明别名解析保护来源，按 CFG 跟踪精确 SSA 定义、存储借用、
  MOVE 后的独立 owner，以及 STORE/消费、存储覆盖或释放造成的失效。全局、cell
  借用与字段投影在可能回调/修改的操作后必须重新取得保护；当前采用保守效果分类。
- 托管 TEMPORARY 存储的完整激活区间也进入 CFG 检查：初始化、LOAD、MOVE/RELEASE、
  分支汇合和终止路径不能冲突或遗漏。错误报告区分 SSA v 与存储 s，并定位实际
  失败指令，而不再笼统指向函数入口。
- 原 borrowed-call 的 AST ownership 放行、同块 STORE/LOAD/RELEASE 拼写检查和
  特定后继 RELEASE 搜索已由全局来源/存储证明替代。调用边界只接纳明确的借用观察，
  不要求构造接收者为满足后端拼写而额外 retain 一次。
- 效果预计算按指令使用点缓存，而不是按 operand arena 下标缓存；共享同一操作数
  slice 的 MOVE 和 COPY 仍拥有不同效果。该边界已有独立抽象图检查。

相应 lowering 修改均在统一 IR 内完成，本轮未修改 Cgen：

- 显式构造器从 pending object storage LOAD 接收者，避免 STORE 后继续使用旧 SSA；
  构造器与 resolved syntax call 共用参数求值保护规则，后续参数失败会回收先前 owner。
- 全局/cell/投影借用在进入跨求值边界前按精确物理 ABI copy/retain，并进入临时存储；
  决策依据 lowered producer，而不是仅依据源 NAME/rvalue 类别。
- `task.all/race/whenAny` 的临时 Future 组成同一个求值事务；后续启动失败也清理先前
  已取得的 Future。集合插入、集合复合赋值和动态下标写入相应保护先求得的接收者。
- 集合复合赋值冻结目标存储 ABI，修正临时存储扩容后继续读取旧 arena 指针、可能误发
  CELL_STORE 的问题。字段投影中 STORE 后直接观察旧 SSA 的路径改为显式 LOAD。
- 索引 read/modify/write 地址计划注册同一个求值 owner scope，替代原先遇到临时地址
  加 raising RHS/operator 就拒绝的未完成分支；接收者、key 和旧字符串值均有异常清理。

已落盘的验证证据：

- `tools/check_xlir_value_lifetime.ps1` 从当前源码构建契约及 C/header，已接入 core
  function suite 和静态契约。`build/tmp/xlir_borrow_lifetime_final_source_gate.log`
  通过：2 个完整模块变异分别复现已释放原 owner 和已失效 LOAD 借用，均在 C/header
  输出前拒绝；15 个抽象 CFG/临时存储检查；确定性 C/header 和严格 C 实际运行通过。
  抽象图只验此分析，不声称满足完整 module 的所有结构和类型约束。
- 新源码 fixture 验证动态下标求值替换全局容器时，写入仍作用于先求得的接收者，且
  新全局容器保持自己的内容。原始 released-owner 探针现在输出 accepted=0。
- `build/tmp/xlir_borrow_lifetime_affine_contract.log` 的既有消费契约仍通过：336 个
  RuntimeOp、1,098 个 consuming inputs、807 个拒绝变异和 15 个 CFG 检查。数量与
  上节不同源于 lowering 删除冗余 owner 并重排事务；不是把两次结果相加或沿用旧计数。
- `build/tmp/xlir_borrow_lifetime_strict_runtime_results.json`：当前源码生成的参数清理
  6,600 次 / 1,100 次真实诊断 OOM / 209,000 平衡分配；循环清理 12,000 次 /
  2,000 次真实诊断 OOM / 679,200 平衡分配，均通过。托管存储另有 400 次 /
  36,600 平衡分配通过。container_ops、class_string_ownership、array_negative_index
  和带原始 C support 的 cgen_index_access 均完成严格 C 编译及运行。
- 源码接线 288 / 49 direct / 238 included / 6 embedded / 2 generated、batch 接线、
  Core C fixture 495 / 11 与 XLIR 静态契约通过。静态契约不再要求已被替代的旧
  RELEASE 搜索函数或注释拼写，改查有限透明来源及 CFG storage activation。

扫描必须分清版本：第三版 driver 对 494 个旧代表为 449 通过 / 45 拒绝；第四版
470 / 24；第六版完整结果 `build/tmp/xlir_borrow_lifetime_v6_source_audit.json` 为
486 / 8。其后第七至第九版的局部复验已接纳两个索引复合赋值代表和 stdlib_crypto，
但没有重新对最终源码完成全部 494 项扫描，所以不能宣布最终仅剩 5 项或零回退。

剩余已知源码失败以 `build/tmp/xlir_borrow_lifetime_remaining_diagnostics.json`
的精确存储/指令诊断继续：finally 中的 pending return、包含 class/closure 的索引
赋值和多赋值清理仍被拒绝；task_api/cgen_task_spawn 的跨 await 下标接收者借用还需
处理。总 SSA owner 必须最终消费、部分产品构造/转交事务、原生借用以及 cancelled/
closed Future 终态仍非本节已证明范围，原 12 个发布 ABI 缺口也未消除。
长期目标保持 active；先完成这些语言级路径与最终源码全量复验，再重建和发布。

## 2026-09-07：finally 失败返回、跨 await 接收者与统一 RuntimeOp 发射（源码检查点）

本节继续上一节的借用存活开发，仍未替换发布 DLL。最终源码 driver 为
`build/test/xlir_lifetime_v13_driver.exe`；完整扫描
`build/tmp/xlir_lifetime_v13_source_audit.json` 的 **494 项全部通过**，新增
`xlir_value_lifetime.xl` 则由独立源码构建门禁验证。中间版 v11 为 492 / 494，
最终版修复 task_api/cgen_task_spawn 后完成整组重扫，不把局部复验当作全量通过。
这是旧的可直接解析代表集，不是 546 项正式 CLI 准入或全项目运行时验收。

本轮语言修复：

- 动态 key 写入在 checked closure boxing 前登记 key owner；多赋值的索引写入
  将已取出的非 xvalue payload 临时存储纳入求值事务。后续转换失败时同时回收
  接收者、key 和已取得的写入值，不再只清理地址的一部分。
- `CLEANUP_BEGIN.pendingExit` 明确表示清理开始时仍持有的待返回 owner。
  abrupt finally 在清理事务中取消它；normal finally 仅在清理失败分支的
  ERROR_UNWIND 中取消它，成功分支继续 LOAD/MOVE/RETURN。verifier 检查 finally
  归属、返回物理类型和失败分支的精确释放，存储激活分析独立拒绝遗漏释放。
  没有把已经关闭的词法作用域重新加入 AFTER_SCOPE_FAILURE，也不会重复析构局部对象。
- 数值下标读取分开求值与 ABI 发射：先保护接收者，再求 key，最后投影结果并
  关闭统一求值区间。普通数组/list 写入同样提前保护接收者。调用、抛错和 await
  均使用相同的 owner 规则，不在恢复后重新读取全局变量来改变源求值语义。

新测试还发现同步、async、generator 的 RuntimeOp 发射与准入维护了重复子集。
三种执行模式现共用 `xl6IrCgenEmitRuntimeCallInstruction`，去掉两个独立发射分支
和两个重复 RuntimeOp 准入表；保留全局 IR/指令 ABI 验证，以及机器专有的 frame、
控制流与存储检查。此改动删除约 400 行重复代码，没有新增索引专用异步补丁。
静态门禁要求三个入口均调用统一 renderer，禁止执行模式内部恢复 RuntimeOp 枚举分支。

最终已验证证据：

- `build/tmp/xlir_lifetime_v13_source_gate.log`：3 个完整模块变异在 C/header
  输出前拒绝，分别覆盖 released owner、失效 LOAD 借用、跨 await 的旧全局借用；
  15 个抽象 CFG 检查。C/header/测试绑定确定性通过。
- 同一门禁用未完成的 Promise 真实挂起 async，挂起后替换全局容器再 resolve
  或 reject；读取、写入、安全下标仍保持原接收者。600 次严格 C 运行同时覆盖
  generator 的创建、索引读写及安全投影，13,300 次分配全部平衡；C-host JIT/AOT
  各另执行 600 次。内部 Future 测试绑定来自唯一 global Future 的 IR SymbolId，
  不硬编码编号，也不假定语言已支持公开 Future export ABI。
- `build/tmp/xlir_lifetime_v13_unwind_runtime.log`：37,600 次运行、12,500 次真实
  诊断分配失败、757,200 次平衡分配；新增 finally 清理成功保留返回值、清理失败
  释放返回值两条真实路径。`xlir_lifetime_v13_unwind_contract.log` 为 1,741 个拒绝
  变异（其中 86 个 call outcome 变异）、51 个正常清理区域和 2 个 pending-return
  正常清理失败事务。不得与旧发布门禁的 1,666 / 83 计数混用。
- `build/tmp/xlir_lifetime_v13_runtime_regression_results.json`：11 个现有 multi-await
  严格 C 成功/拒绝/取消/关闭组合和 3 个源码契约共 14 项全部通过；scope 为 736 个
  变异，return commit 为 14 个，parameter cleanup 为 433 个。
  `xlir_lifetime_v13_pending_contract.log` 另有 871 个 pending-exception owner 变异通过。
- 静态源码接线 288 / 49 direct / 238 included / 6 embedded / 2 generated；batch、
  Core C fixture 497 / 11、迁移覆盖 129 rows / 374 representatives /
  433 deterministic candidates / 143 differential candidates / 66 XLIR-only contracts、
  5,838 文本编码、329 篇 Markdown 链接及 XLIR contract 检查通过。

边界：上述成功/拒绝与有限取消/关闭组合不等于完整 Future 终态证明；托管 SSA 的
最终消费、部分产品构造/转交、原生借用以及非数值 key 的完整求值边界仍需继续压实。
原 12 个正式准入 ABI 缺口没有消除，旧静态接线/计数失败也没有在本轮冒充已修复。
尚未用最新源码重建发布 DLL 并重跑全部发布专项；`xlc.dll` 仍为
`7D6E3A1804A6399FAA00FA17B438B88DFF250314CE0874D3DC0E06CEBB410EE8`，`xl.dll`
仍为 `21D0344712EECCC4242600D24B8E1B510B903267B7765243B61514528C7258D5`。
长期目标保持 active，不将本节的源码正例全绿替代整个语言和发布验收。

## 2026-09-08：统一索引求值事务与托管 RHS 保护（源码检查点）

本节继续语言生命周期收口，未重建或替换发布 DLL。源码 driver 为
`build/test/xlir_index_transaction_driver.exe`；
`build/tmp/xlir_index_transaction_source_audit.json` 完整重扫 **494 / 494 通过**。
这个分母仍是可直接解析的旧代表集，不是 546 项正式 CLI 准入或全项目运行时验收。

原始反例 `build/tmp/xlir_index_transaction_before.log` 在
`lifetimeDict[string(await key)]` 拒绝未保护的跨 await 接收者。问题是非字面量
读取先求 receiver/key、最后才保护 receiver；普通索引赋值又有四套独立实现，
没有一致地保护在地址求值之前已经取得的 RHS。

本轮实现：

- 删除四套普通索引写入 helper，普通赋值、复合赋值和产品多赋值共用
  `XL6_IrLowerIndexTargetPlan` 的地址求值和写入流程。普通赋值在建立地址计划前
  保护 RHS；接收者、key 和值进入同一套求值 owner 机制，后续失败按逆序清理。
- 写入值是受保护的借用时，boxing 显式取得独立 item owner；已经独立拥有的值
  才使用 owning erasure。不会把临时存储 LOAD 冒充可直接消费的 owned SSA。
  literal/string 的动态字段与字典写入仍按已验证 TypeId/ClassLayout 选择具体操作。
- 合并 string key / erased key 的读取 helper；在 key 求值之前保护 receiver，
  在投影取得结果所有权之后释放 operand anchors。await、抛错和普通调用不再维护
  三套不同求值补丁。本轮没有修改 Cgen 或放宽 IR 所有权验证。
- 静态契约删除对旧 helper/诊断拼写的要求，改为要求共用地址事务、RHS 的保护次序
  和结果投影后的清理，并禁止重新引入六个已退役的读写 helper。

当前源码测试 `xlir_value_lifetime` 扩展为三组 receiver 的数值/string/erased key
读、写、安全访问，以及 string 和嵌套 array RHS 的跨 await 写入。host 用未完成
Promise 真实挂起，替换全局容器后再完成或拒绝 key；成功时必须写入原容器，失败时
不改动内容且回收先前生成的载荷。同步测试另外验证 string/xvalue 两种 RHS 在
receiver/key 求值失败时的清理和 RHS → receiver → key 顺序。

验收证据：

- `build/tmp/xlir_index_transaction_final_source_gate.log`：**13 个完整模块变异**
  在 C/header 输出前拒绝（2 个原 owner/LOAD 借用反例，11 个跨 await 接收者反例），
  另有 15 个抽象 CFG 检查；C/header/绑定确定性通过。
- 同一源码门禁严格 C 运行 **2,200 个真实挂起用例 + 600 个 RHS/地址成功失败用例**，
  **178,400 次分配平衡**；JIT/AOT 各重复同样的 2,800 个用例。module fini 后的
  LiveCount/LiveBytes 回到 module init 前基线，且无新增 double/invalid free 或 UAF。
- `build/tmp/xlir_index_transaction_regression_results.json`：12 项索引 ABI、泛型、
  class/closure、复合赋值、多赋值、动态整数 key 和入口函数样例，均从本节 driver
  生成确定性 C、严格编译并实际运行通过。
- 补充纠正通用 C 测试 host 的入口参数：variadic main 收到真正的空 argument array，
  不再传 NULL 冒充空集合。独立探针确认 v13 与本节源码在合法参数下均返回 0，
  这是测试调用边界修正，不把非法 host 参数造成的失败冒充编译器回退。
- `build/tmp/xlir_index_transaction_unwind_runtime.log`：37,600 次运行、12,500 次
  真实诊断分配失败、757,200 次平衡分配；当前源码异常契约仍为 1,741 个拒绝变异、
  86 个 call outcome 变异、51 个正常清理区域和 2 个 normal finally pending-return
  失败事务，见 `xlir_index_transaction_unwind_contract.log`。
- `build/tmp/xlir_index_transaction_owner_contract.log`：336 个 RuntimeOp 的显式
  所有权效果、1,098 个 consuming inputs、807 个拒绝变异及 15 个 CFG 检查通过。
  源码接线 288 / 49 direct / 238 included / 6 embedded / 2 generated、batch、
  Core C fixture 497 / 11、迁移覆盖 129 rows / 374 representatives /
  433 deterministic candidates / 143 differential candidates / 66 XLIR-only contracts、
  XLIR 静态契约、5,838 文本编码和 329 篇 Markdown 链接均通过。
  `build/tmp/xlir_index_transaction_source_snapshot.json` 固化 20 个源码/测试/driver
  文件和两个发布 DLL 的 SHA-256；lowering 文件较 v13 减少 19,268 字节。

未完成边界仍须单独处理。特别是本轮重新执行
`bad_xlir_nullable_index_assign_if_null_may_raise.xl`，当前仍因索引 `?=` 的 RHS
可能抛错而拒绝，证据为 `build/tmp/xlir_index_transaction_conditional_gap.log`。
这是尚未完成的语言能力，不应把旧负例名称或旧门禁期望作为永久限制；该分支也仍有
接收者保护过晚和仅接纳 NAME receiver 的限制。下一步应让条件写入复用地址事务，
保留 RHS 的短路语义并补足失败/合流清理，而不是继续要求 no-raise RHS。

托管 SSA 的最终消费、部分产品构造/转交、原生借用、完整 Future 终态、12 个正式
准入 ABI 缺口及完整发布专项仍未完成。标准库最低范围不豁免这些语言问题。
长期目标保持 active；本节源码通过不代表发布 DLL 已包含这些修复。

## 2026-09-08：条件索引写入复用地址事务（源码检查点）

本节移除上一节确认的索引 `?=` no-raise RHS 限制，仍未替换发布 DLL。
最终源码 driver 为 `build/test/xlir_conditional_index_v2_driver.exe`；
`build/tmp/xlir_conditional_index_v2_source_audit.json` 完整重扫 **494 / 494 通过**。
v2 在地址求值前冻结元素的非空判定，避免跨 lowering 保留 Type arena 指针；
该结果是对最终版本重新跑完整组，不是沿用 v1 的 494 项结果或拼接局部复验。

实现不增加 Cgen 语义分支：

- 地址计划在求值前成对冻结 borrowed-read / consuming-write RuntimeOp。
  数值 array/list、literal/string dict、erased key 和动态字段均使用同一模型。
- `xl6IrLowerEmitIndexTargetWrite` 只提交已经求得的 item，不自行关闭地址区间。
  普通写入可立即关闭区间；`?=` 在 null 分支求 RHS，成功路径到 merge 才关闭，
  抛错分支则由相同的 evaluation-owner 栈回收。没有复制一套条件专用 setter。
- 当前元素的借用只供 `NULL_TEST` 观察，不转交或释放它。RHS 的装箱和所有权转交
  复用普通写入；接收者是函数返回值、key/RHS 会替换全局容器或真正挂起时，仍作用于
  已求得的原地址。非空元素类型保留 receiver/key 求值，但消除不可达的 RHS。
- nullable string 和 tagged nullable int 的索引条件写入也走相同 carrier/null-test
  与精确 item 转换，不再把索引 `?=` 限定为 NAME receiver、i64 key 或 xvalue item。

新增 `tools/check_xlir_index_conditional_assign.ps1` 从当前源码编译契约及 C/header，
接入 core function suite 和 null ABI 总门禁。旧 null 门禁中要求特定 C 文本布局和
拒绝 raising RHS 的独立索引段由该门禁替代，没有放宽所有权验证。
原 `bad_xlir_nullable_index_assign_if_null_may_raise.xl` 已重命名为
`xlir_nullable_index_assign_if_null_may_raise.xl`：现在捕获并断言真正的 RHS 异常消息，
移出 expected rejection 清单，并进入正式 legacy/XLIR 差分候选。

已验证证据：

- `build/tmp/xlir_conditional_index_v2_source_gate.log`：13 个 lazy address transaction，
  验证 null 分支能够到达写入、non-null 分支不能到达写入、读写使用同一 receiver/key、
  borrowed current 仅用于 null test，并在 merge 释放地址。
  26 个旧 owner/遗漏 merge release 的变异，既被独立 lifetime pass 拒绝，也被完整
  module verifier 以 lifetime 原因拒绝，C/header 均不输出；不是靠破坏源位置或指令
  格式制造拒绝。C/header/内部 Future SymbolId 绑定的重复生成一致。
- 同一门禁严格 C 运行 **4,100 个同步/异常用例 + 3,200 个异步短路/挂起用例**，
  **490,800 次分配全部平衡**。异步同时覆盖 present 时根本不等待 RHS、absent 时
  真正等待、key 本身必须等待、成功/reject 后的原容器语义；不是已完成 Future 模拟。
  JIT/AOT 各重复同样的 7,300 个用例；模块卸载恢复初始 LiveCount/LiveBytes，
  无新增 double/invalid free 或 UAF。原先的简单 indexed-null 正例也完成严格 C 运行。
- raising RHS 正例在当前源码生成的 XLIR C 和发布编译器生成的 legacy C 中均实际
  返回 0；该差分只证明这个共享语义正例，不声称整个 async host 已做 legacy 差分。
- `build/tmp/xlir_conditional_index_v2_regression_results.json` 的 12 项既有索引 ABI、
  泛型、复合赋值、多赋值、对象/闭包与实际 main 入口回归，确定性 C 和严格运行均通过。
- 既有借用门禁仍为 13 个模块变异 / 15 个抽象 CFG，2,200 个挂起用例 + 600 个
  RHS/address 用例及 178,400 次平衡分配；异常展开运行仍为 37,600 次 /
  12,500 次真实诊断分配失败 / 757,200 次平衡分配。
  当前源码 `xlir_conditional_index_v2_unwind_contract.log` 仍有 1,741 个拒绝变异、
  86 个 call outcome 变异、51 个正常清理区域和 2 个正常 finally 返回失败事务。
- `build/tmp/xlir_conditional_index_null_gate.log` 的完整 null ABI 门禁通过。其旧有
  nullable/closure/cell/async/field 子项使用发布 DLL，新索引子项使用源码构建；这不是
  最新发布 DLL 的全量回归。第一次运行因扫描进程占用 build/test/xl.dll 而复制失败，
  等该扫描进程结束后重跑成功，没有把文件锁误判成语言错误或终止仍在工作的扫描。
- `build/tmp/xlir_conditional_index_v2_runtime_regression_results.json`：11 组多 await
  成功/拒绝/取消/关闭组合及 3 个源码清理契约共 14 项全部通过。scope cleanup 的
  736 个变异、return commit 的 14 个变异、parameter cleanup 的 433 个变异均保留。
  这仍不等于完整 Future 终态证明。
- 源码接线 288 / 49 direct / 238 included / 6 embedded / 2 generated、batch、
  Core C fixture 500 / 11 和 XLIR 静态契约通过。迁移覆盖为 129 rows /
  374 representatives / 435 deterministic candidates / 144 differential candidates /
  67 XLIR-only contracts；后两项是接线清单数量，不是本轮执行了所有候选。
  5,843 文本编码和 329 篇 Markdown 链接检查通过。

另一个未完成的语言边界已实际确认：普通 `int? value = null; value ?= 17` 仍被
直接存储路径的“仅支持 managed sentinel”条件拒绝；见
`build/tmp/xlir_conditional_direct_tagged_probe.xl` 和同名 `.log`。
本节对 tagged nullable **索引** 的支持不等于直接变量、cell、property 等所有 lvalue
路径均已统一。下一步继续按 NullAbi 和精确存储操作收口，不能保留这项临时限制。

托管 SSA 最终消费、部分产品转交、原生借用、完整 Future 终态和旧发布检查点中的
12 个 ABI 缺口仍未完成。新增正例后尚未重跑正式 CLI 全量准入，不能沿用旧分母作为
当前仓库验收；本节 494 个历史代表和新增专项也不能替代它。长期目标保持 active。

## 2026-09-08：条件存储与共享 cell continuation ABI（源码检查点）

本节完成上一节确认的直接 `int? value; value ?= 17` 缺口，扩展到直接局部、全局、
提升后的参数及可变捕获 cell。源码 driver 为 `build/test/xlir_conditional_storage_v5_driver.exe`；
没有重建发布 DLL，也没有重跑正式 CLI 全量准入。

- `xl6IrLowerStorageAssignIfNull` 替代 managed-sentinel 专用路径。NullAbi 决定是否
  测空和进入 RHS 分支，所有权决定是否取得新 owner / 释放旧槽，cell 间接存储独立
  选择 CELL_LOAD/CELL_STORE。类型和存储事实在 RHS lowering 前按值冻结。
- tagged bool/int/u64/float、raw pointer、string 和 xvalue 不再使用不同的条件赋值
  语义分支。零和 false 是有效载荷，不是 null；非空静态类型完全消除 RHS。
  新 owner 在覆盖旧槽之前取得；CELL_STORE 只替换 payload，不销毁共享 cell。
- semantic 的可变捕获准入先解析 alias / nullable wrapper，再检查 cell payload；
  nullable string 与非 nullable string 共用已实现的拥有型字符串 cell ABI。
- 实测发现 async/generator 虽已有 cell frame 与清理元数据，Cgen 尚不接纳 CELL_*
  指令。本节从同步 renderer 抽出 `xl6IrCgenEmitCellInstruction`，三种执行模式共用
  payload 操作；调用者只提供经验证的 local / capture / frame 位置。frame NEW 设置
  init bit，RELEASE 先清槽和 init bit 再释放；没有加入 `?=` 专用 Cgen 补丁。
  v5 在 generator 预验证阶段即检查 cell 的 frame/local 归属，不拖到发射阶段才拒绝。
  async 的普通全局 STORE/RELEASE 同时采用显式共享存储绑定，不伪装为 frame 局部。
- generator 内普通匿名函数曾继承外层 generatorDepth，错误拒绝携值 return；现在
  匿名函数检查期间隔离该上下文并在退出时恢复。两项独立 semantic 负例仍拒绝内部
  普通函数的 yield，以及返回外层后 generator 的携值 return。

最终源码专项 `build/tmp/xlir_conditional_storage_v5_source_gate.log`：

- 49 个条件存储事务，其中 21 个 cell、21 个 tagged、26 个 managed（分类可交叉）；
  验证 read 仅用于 NULL_TEST、true 分支可达 write、false 分支不可达 write、精确
  TypeId、cell 不释放共享槽，以及直接 managed storage 的 acquire/release/store 次序。
- 163 个完整模块 ABI/所有权变异在 C/header 输出前拒绝，包括错误 null-test 类型、
  错误写入类型、把 CELL_STORE 换成 STORE、借用值冒充拥有值，以及 6 个 async/generator
  cell frame 的 init bit / payload TypeId / destroy ABI 变异。
- 严格 C 实际执行 7,200 个同步/异常用例、3,600 个异步短路/挂起用例和 2,400 个
  generator 恢复/提前关闭/异常用例；137,600 次分配全部平衡。模块卸载恢复初始
  LiveCount/LiveBytes，无新增 double/invalid free 或 UAF。JIT/AOT 各重复同样的
  13,200 个用例；不是把已完成 Future 当作真实挂起。
- 新 `xlir_nullable_storage_assign_if_null.xl` 的实际 main 和既有
  `xlir_nullable_sentinel_assign_if_null.xl` 的实际 main 均由当前源码生成、严格编译
  并运行通过。通用 C host 显式声明自己使用的 argument-array 运行时入口，不再依赖
  被测程序碰巧包含 array 构造才提供声明。

差分边界不能隐去：发布 legacy 对新 tagged nullable `?=` 输出 struct 与整数 0
比较的非法 C；对既有 sentinel baseline 又在 `probeClassAssign(true) == 9` 处失败。
诊断见 `xlir_conditional_storage_v4_legacy_tagged.log` / `_acceptance_verified.log`，另有
保留的 legacy C。两项 fixture 属于 XLIR-only 契约，未降低语言断言或将 legacy 错误
固化成目标语义。旧条件索引 raising-RHS 的实际 legacy/XLIR 差分仍通过，见
`xlir_conditional_storage_v5_null_gate.log`；不宣称新增全部 storage 用例已做差分。

回归及统计：

- `xlir_conditional_storage_v5_nullable_regression_results.json`：17 项既有 nullable、
  可变 scalar/string/xvalue 捕获及参数、finally/catch-return 正例，确定性生成 C、
  严格编译和运行全部通过。
- `xlir_conditional_storage_v5_runtime_regression_results.json`：11 个多 await 运行组合
  与 3 个当前源码清理契约共 14 项通过；scope 736、return commit 14、parameter 433
  个拒绝变异保留。
- 原索引专项 7,300 个用例 / 490,800 次平衡分配、借用专项 2,800 个用例 /
  178,400 次平衡分配均重新从本节源码通过。
- `xlir_conditional_storage_v3_source_audit.json` 扫描 494/494 通过，但 v3 driver
  构建于最后的匿名函数 generatorDepth 修正及 generator cell 存储准入复核之前。该扫描不能标成 v5 全量源码扫描；
  v5 的证明是上述真实源码专项、两个 main 及明确列出的回归。
- 源码接线 288 / 49 direct / 238 included / 6 embedded / 2 generated、batch、
  Core C fixture 503 / 11 和 XLIR 静态契约通过。迁移清单为 129 rows /
  374 representatives / 437 deterministic candidates / 144 differential candidates /
  69 XLIR-only contracts；清单数量不是本轮全量执行数量。
- `xlir_conditional_storage_v5_null_gate.log` 的完整 null ABI 总门禁通过；旧子项仍使用
  发布编译器，新索引/storage 子项从源码构建，因此不等同于最新发布 DLL 全量验收。
  5,849 文本编码及 329 篇 Markdown 链接检查通过，20 个当前源码/测试/工具/driver
  文件的哈希保存在 `build/tmp/xlir_conditional_storage_v5_source_snapshot.json`。

下一项反例仍成立：`build/tmp/xlir_conditional_member_next_probe.xl` 的
`holder.value ?= 17` 被拒绝为没有 direct/cell storage ABI。成员、static field、
property / protocol setter 尚不能视作已统一；必须继续建立显式地址与读写调用计划。
本节也未完成所有 managed SSA 的最终消费证明、部分产品事务、原生借用、完整
cancelled/closed Future 生命周期或旧发布准入的 12 个 ABI 缺口。标准库最低范围
不豁免这些语言问题，长期目标保持 active。

## 2026-09-08：字段地址事务与默认值所有权（member-v6 源码检查点）

本节使用 `build/test/xlir_conditional_member_v6_driver.exe`。发布 DLL 未重建，
没有最新正式 CLI 全量准入；本节的证明范围是下面逐项列出的源码契约和实际运行。
最终 v6 driver 的 494 项历史代表扫描已启动，尚未结束；较早 v5 driver 的扫描
也不能替代最终版本结果。进程与源码哈希检查点见
`build/tmp/xlir_conditional_member_v6_source_snapshot.json`。

- 字段与索引共用 `XL6_IrLowerElementTargetPlan`。地址求值前冻结成对读写操作、
  字段析构与 callable signature，receiver/key 只求值一次。普通赋值先保护已求得
  的 RHS；复合赋值保持 receiver/read/RHS/operator/write 顺序；条件赋值在 null
  分支才求 RHS，并在成功 merge 或异常展开中释放地址保护。
- 删除独立的 `xl6IrLowerRuntimeXValueMemberSetLiteral` 和
  `xl6IrLowerClassFieldCompoundAssign`，没有保留旧路径作为失败后的重试。
  普通动态成员 `=` 也复用写入计划；现有语言不允许的动态成员复合赋值及临时
  receiver 写入仍在 semantic 拒绝，未借迁移隐式扩大语言规则。
- 值装箱由 `xl6IrLowerAcquireErasedElementValue` 统一负责，构造时的显式字段
  默认值与后续字段/索引写入共用这一拥有型边界。实际压力测试发现显式字符串
  默认值原先在装箱复制后漏释放原字符串；此处修正同时保留借用 xvalue 的 retain。
  原生字段仍使用实际 RHS 的 typed 转交元数据，不以左值节点冒充 rvalue。
- static field 接入普通 `ValueInitializer`，包括显式初始化和默认初始化，并复用
  现有验证器。字段、静态字段和局部 nullable string 均默认 null；普通 string
  仍默认空串。`NullAbi` 是物理表示，不等同于语言类型的 nullable 标记。
  默认值分类的修正保留在 verifier 及变异测试中。
- parser 的 nullable 后缀分类接纳完整的字段修饰符前缀，覆盖
  `public static int? number`；同时实际解析运行含 contextual modifier 名称的
  普通三元表达式，未将问号普遍重写为类型后缀。
- async/generator 共用 `xl6IrCgenSharedStorage` 区分全局/extern-global 与 frame
  和局部存储。generator 的普通 LOAD/STORE/RELEASE 可使用模块绑定，提前预验证
  位置归属；共享存储没有被伪装成 frame，也未扩大 shared MOVE / product / cell
  的准入。本节实际 generator 在 yield 后替换全局对象，再写此前保留的原对象。

新增 `tools/check_xlir_member_assignment_transaction.ps1`，已接入 core function suite、
null ABI 总门禁和 XLIR-only 迁移清单。最终源码证据：

- `build/tmp/xlir_conditional_member_v6_source_gate.log`：12 个字段和 4 个静态字段
  lazy transaction、2 个 static 默认初始化，以及 43 个完整模块错误变异。
  地址旧 owner 和遗漏 merge release 的变异同时经过独立 lifetime 验证；所有变异
  均在 C/header 输出前拒绝。另有 4 个既有语义负例保留具体诊断。
- 严格 C 运行 2,400 个条件/异常用例、1,200 个真实异步挂起/短路用例、2,400 个
  generator 恢复/提前关闭/异常用例，以及 700 个普通/复合/静态写入用例。
  合计 **6,700 个用例，191,600 次分配平衡**；模块卸载恢复初始 LiveCount/LiveBytes，
  没有新增 invalid/double free 或 UAF。JIT/AOT 各重复同样的用例。
- `xlir_conditional_member_v6_nullable_regression_results.json`：17/17；
  `_regression_results.json`：12/12 索引用例；
  `_field_regression_results.json`：18/18 字段、闭包、原生、静态、union 及 async
  function-field 用例；这些组均从本节 driver 重复生成一致的 C，严格编译并运行。
- `_runtime_regression_results.json`：11 组多 await + 3 个源码清理契约共 14/14；
  scope cleanup 736、return commit 14、parameter cleanup 433 个拒绝变异保留。
- `xlir_conditional_member_v6_check_xlir_storage_conditional_assign.log`：既有 storage
  专项 13,200 个用例 / 137,600 次平衡分配；`_check_xlir_index_conditional_assign.log`：
  7,300 个用例 / 490,800 次平衡分配；`_check_xlir_value_lifetime.log`：2,800 个用例 /
  178,400 次平衡分配，均重新从当前源码通过。
- 静态 XLIR 契约、source/batch wiring、Core C fixture 506 / 11、5,854 文本编码及
  329 篇 Markdown 链接通过。迁移清单为 129 rows / 374 representatives /
  438 deterministic candidates / 144 differential candidates / 70 XLIR-only contracts；
  这些是接线数量，不是本节实际执行全部候选。
- 未带 `-CoreOnly` 的 C fixture 扫描另报 release/extlibs 中 mqtt/serial 等测试文件
  接线缺失；没有修改这些无关包，也未把全范围扫描标为通过。

本节尚不能称为所有 lvalue 的最终统一：`xlir_conditional_member_v6_static_compound_probe.xl`
实测静态字段 `+=` 仍进入不合适的元素地址路径；property `?=` 仍由现有 semantic
规则拒绝。getter/setter 的可抛错复合事务、其它静态/隐式/协议写入边界仍需继续审查。
字段 SET 的失败仍采用现有 TRAP_IF_FALSE，不宣称完整字段分配失败已成为语言异常。
托管 SSA 最终消费、部分构造/转交、原生借用、完整 Future 终态，以及旧正式准入
中的 12 个 ABI 缺口均未因此结案。长期任务保持 active。

当前 lowering 源码为 1,631,908 字节，比上一 storage-v5 检查点少 5,379 字节。
这是局部源码变化，不是发布二进制缩减，也不是总架构规模的完成度指标。

## 2026-09-08：直接、捕获和静态存储更新统一（storage-transaction-v2）

本节使用 `build/test/xlir_storage_transaction_v2_driver.exe`。上一 member-v6 的
历史源码扫描已真正结束，结果为 494/494；本节新 driver 的独立 494 项扫描正在运行，
不能用旧版本扫描代替。本节仍未重建发布 DLL，也不是正式 CLI 全量准入结果。

- `XL6_IrLowerStorageTargetPlan` 在 RHS 求值前固定直接变量、promoted parameter、
  capture cell 或 class static 的 SymbolId、TypeId、NullAbi 和 ownership/cell 表示。
  普通 `=`、`?=` 和复合赋值共用读取与提交边界，删除独立 string/collection/overload
  direct compound 实现，删除 assignment dispatcher 中的 static 特判和 eager-RHS
  scalar fallback。未知目标仍明确拒绝，没有重试旧路径。
- 复合赋值统一为 read/snapshot/RHS/operator/commit。标量旧值先于 RHS 读取；托管旧值
  取得独立 owner 并登记 evaluation cleanup；RHS 可以改写原目标，错误路径不执行提交。
  普通赋值及 null 分支同样先取得新值所有权，再 RELEASE/STORE 或 CELL_STORE。
  `xl6IrLowerAcquireValueOwner` 使用 lowered producer 契约，不以 frontend rvalue/category
  或 ownership 提示把借用伪装成转移。局部 storage 的旧值也显式快照，因为 byref/out
  调用可以使原始 LOAD 失效。既有不稳定借用快照复用同一 copy/retain ABI。
- overloaded compound 继续走统一 CallTarget/error outcome；RHS 参数 TypeId/ABI 在
  求值前固定，RHS lowering 后重新取得 CallTarget，避免继续使用可增长 arena 的旧指针。
- 独立顺序探针 `xlir_storage_transaction_order_probe.xl` 实际对比：member-v6 生成物
  退出 1，新 driver 生成物退出 0；RHS 把同一变量从 3 改成 29 并返回 4 时，复合赋值
  应提交旧值 3 + 4，而非在 RHS 之后重读得到 29 + 4。结果存于
  `build/tmp/xlir_storage_transaction_v2_order_comparison.json`。

新增 `tools/check_xlir_storage_assignment_transaction.ps1` 已接入 core suite 和
XLIR-only runtime 清单。`build/tmp/xlir_storage_transaction_v2_source_gate.log` 证明：

- 32 个有序更新、13 个 managed snapshot、2 个 capture cell 更新、8 个 static 更新；
  47 个 malformed type、错误 cell 指令、缺失 owner acquisition 等变异均在 C/header
  输出前拒绝。正例直接检查源 RHS 子树的第一条 IR 严格晚于旧值读取及托管快照。
- 1,200 个 local/cell/static/global 运行用例与 1,200 个真实 async 挂起用例，包含 RHS
  改写同一目标、RHS 抛错、重载运算符抛错、自赋值，以及挂起期间替换 static owner。
  89,800 次分配全部平衡，模块 finalization 恢复初始内存基线，没有新增 double free、
  invalid free 或 UAF；严格 C、JIT、AOT 各执行同样的语义断言。
- 保留前端对可变 typed collection/class 捕获的既有限制；捕获专项覆盖语言已支持的
  int/string cell，array/class 则通过直接、global、static 及 async 路径实际验证。
- nullable 17/17、index 12/12、field/callable/native/static 18/18、runtime/cleanup 14/14
  均用新 driver 重跑；另有 scalar、collection algebra、protocol compound、参数模式、
  narrow integer 和 checked integer compound 等 11/11 严格 C 回归。
- 既有 storage conditional、index conditional、value lifetime 的源码构建专项再次通过，
  分别保留 13,200 / 7,300 / 2,800 个用例和 137,600 / 490,800 / 178,400 次平衡分配。
- 静态 XLIR/source/batch/core C fixture 门禁通过；core C fixture 计数 509 / 11，
  编码 5,859 文件，Markdown 329 篇。迁移清单为 129 rows / 374 representatives /
  439 deterministic candidates / 144 differential candidates / 71 XLIR-only contracts；
  清单数量不是当前运行完成数。

本节 lowering 为 1,622,772 字节，比 member-v6 少 9,136 字节；这不是发布 DLL 缩减。
下一步仍需关闭可抛错 getter/setter 的复合事务和其它协议/隐式写入边界，完成托管 SSA
最终消费、部分构造/转交、原生借用、Future 完整终态和旧正式准入的 12 个 ABI 缺口。
源码检查点及正在运行的扫描句柄记录在
`build/tmp/xlir_storage_transaction_v2_source_snapshot.json`。长期目标保持 active。

## 2026-09-08：属性事务与完整同步虚调用契约（property-virtual-v7）

本节最终源码 driver 为 `build/test/xlir_virtual_transaction_v7_driver.exe`。
上一 storage-transaction-v2 扫描及本轮较早 v6 driver 的历史代表扫描均已完成
494/494；最终 v7 的独立扫描仍在运行，不能用 v6 结果冒充最后两处 nullable-return
修正后的全量证明。发布 `xlc.dll`、`xl.dll`、`xl.exe` 哈希未变，没有最新正式 CLI
全量准入。源码/测试/工具及扫描句柄见 `build/tmp/xlir_virtual_transaction_v7_source_snapshot.json`。

实现与边界：

- 普通 class/native property 的 getter、setter 和复合赋值共用
  `XL6_IrLowerPropertyTargetPlan`；调用前固定 receiver/key 的类型和参数 ABI，
  receiver/key 只求值一次。普通赋值先求 RHS，复合赋值先读旧值再求 RHS。
  直接存储、字段/索引及属性共用 `xl6IrLowerCompoundValue`，不再各自选择字符串、
  集合或重载运算。协议 witness 普通 setter 保留独立且明确的 IR 路由，不作 class
  setter 失败后的重试；协议的完整异常契约仍未因此完成。
- 调用后的临时 owner 清理由 `xl6IrLowerCompleteEvaluationCall` 接入统一 completion；
  先分裂成功/失败 outcome，再执行清理。compound getter 保留 receiver/key 给 setter，
  setter/运算失败则不继续提交；复合 property setter 也拥有 canonical throw ExitPlan。
- 字符串、引用计数指针的拥有型边界改用 lowered producer 与显式 LOAD/MOVE，而非
  frontend rvalue/ownership 提示。属性结果在可抛错清理后已转移所有权，不再被误复制
  导致泄漏。广泛回归进一步发现普通/延迟 return 在 nullable normalization 之后仍传入
  原表达式 TypeId；现均使用声明的 return TypeId，保留精确类型验证，未放宽为表示相同即接受。
- `XL6_IrClassDispatchSlot` 现在持有 `parameterAbis`、`errorAbi`、`errorPhysicalAbi`。
  所有 override 的效果和 caller body 在同一固定点中合并；基类直接调用的 no-raise
  ABI 不被篡改。Verifier 检查完整 row/signature/ownership 和 joined effect，包括
  没有实际 CALL 的槽。文本/JSON dump 输出这些可检查事实。
- `xl6IrCallInstructionErrorAbi` 描述实际发出的 direct/virtual/closure 调用或 launcher，
  completion 不再把某个基类 CallTarget 的效果当作虚调用效果。void may-raise 虚调用
  具有控制 carrier，ERROR_TEST 必须紧邻调用，不能在之前清理 owner。
- 普通函数、async 和 generator 共用 `xl6IrCgenEmitVirtualCallInstruction` 与
  `xl6IrCgenEmitCallErrorTest`。虚 dispatcher 的声明/定义共用签名；no-raise 实现进入
  may-raise 槽时显式包装 success carrier。参数指针形式由 INOUT/OUT ABI 决定。
  owned 参数的 CFG 消费证明读取槽自身的契约；LOAD/MOVE 转交通过统一 affine 分析，
  不用 producer category 取代沿路径的转交证明。
- generator 的 BORROW 类参数复用已有 RETAIN_XVALUE frame transfer；前端旧标量白名单
  不再误拒具有引用语义的类。`sem_generator_class_param.xl` 从旧限制负例迁移为实际
  正例，覆盖同一对象修改后的恢复读取及临时对象提前关闭；generator suite 同步更新。
  mutable generator byref/out 参数仍拒绝，新源码契约检查具体诊断。此处不宣称已经
  完成 owned/native generator 参数或全部 Future 终态。

最终 v7 源码验证：

- `tools/check_xlir_property_assignment_transaction.ps1`：29 accessor calls、25 outcome
  split、9 对共享 getter/setter operands、71 个输出前拒绝变异。实际执行 8,800 个
  读/写/复合失败场景、300 个析构抛错场景、3,200 个真实 async 挂起场景、100 个
  mixed-effect base dispatch 和 100 个 normalized nullable-return 场景，共
  **12,500 个用例，605,100 次分配平衡**。严格 C、JIT、AOT 均通过；卸载恢复初始
  LiveCount/LiveBytes，无新增 invalid/double free 或 UAF。
- `tools/check_xlir_virtual_call_outcome.ps1`：12 个普通虚调用、2 个 mixed-effect 槽、
  5 个 byval owner 转交和 2 个 generator receiver retain，26 个错误变异被拒绝。
  缺失 MOVE 的变异还独立经过 owner-consumption CFG verifier。严格 C、JIT、AOT
  各执行 100 次完整模块生命周期，覆盖 borrowed/byval/byref/out、base direct 调用、
  generator resume/失败/提前关闭；**15,300 次分配平衡**。
- 两个新门禁已接入 core function suite 和 XLIR-only 迁移清单；共用 renderer、
  槽契约、实际调用 outcome 及 normalized return TypeId 均有静态架构检查。
- 当前源码回归：nullable/capture 17、index 12、field/callable/native/static 18、
  runtime/cleanup 14、virtual/property/protocol/parameter/generator 16，共 **77/77**；
  另有原生 property 实际严格 C 运行通过。scope 736、return commit 14、parameter
  cleanup 433 个拒绝变异保留。
- 当前源码重跑既有 member、storage update、storage conditional、index conditional、
  value lifetime 门禁，分别为 6,700 / 2,400 / 13,200 / 7,300 / 2,800 个运行用例，
  对应 191,600 / 89,800 / 137,600 / 490,800 / 178,400 次平衡分配。
- XLIR/static/source/batch/Core C fixture 检查通过。Core C fixture 515 / 11，文本编码
  5,869 文件，Markdown 329 篇。迁移清单 129 rows / 374 representatives /
  441 deterministic candidates / 144 differential candidates / 73 XLIR-only contracts；
  这些是清单计数，不是最新发布编译器已运行全部候选的证明。

不能据此结案：`cgen_var_class_dispatch.xl:104` 仍实际失败（v6 生成物；问题在本轮
之前已存在）。控制流合并后的 `boxed.label` 被建模成借用的 dynamic-field-get，
而不是具有结果所有权和异常契约的属性 getter；当前 dynamic method rows 只有 `value`
等方法，没有 `label` getter。下一步应补全动态成员访问的显式 IR 契约，不能让 Cgen
根据名称猜测。同步虚方法在 async/generator 中使用已覆盖，不等于 virtual async/
generator 声明族、全部协议调用或原生布局 ABI 都已完成。托管 SSA 最终消费、部分
构造回滚/转交、原生借用、Future 完整终态和旧正式准入的 12 个缺口仍需继续；其中
11 个语言 ABI 缺口不因标准库最低范围而豁免。长期目标保持 active。

## 2026-09-08：动态 getter 与字段读取的拥有型调用契约（dynamic-member-v4）

本节最终 driver 为 `build/test/xlir_dynamic_member_v4_driver.exe`。上一节 v7 的历史
源码扫描已真正结束，494/494、退出 0。本节 v4 的独立 494 项扫描仍在运行；发布
DLL 未重建，没有最新正式 CLI 全量准入。下面的运行结果不以历史扫描代替。

- Typed XLIR 新增 `XL6_IR_DYNAMIC_CALL_MEMBER_GET`，与 FUNCTION/METHOD 分开。
  读取明确借用 receiver、返回 owned xvalue，并要求 may-raise outcome；Cgen 只消费
  linked site 的类别、名称、结果及已验证 registry。复用已有动态调用指令、参数
  所有权与立即 ERROR_TEST，不再把 getter 伪装成借用的 raw dynamic-field-get。
- 动态 registry 的 row 也带有明确 kind；getter 与同名 method 不作为同一种调用
  候选。每个具体类的继承 getter 在 Typed registry 中选择好 declaration/runtime tag，
  Lowering/Verifier 检查实际接收者的基类借用 ABI。Cgen 不追溯 AST 继承链。
  无 getter 时仍可读取普通存储字段；缺失/空值结果获得 owner；不支持的 getter
  adapter 保留为明确错误，不能退成缺失字段。普通动态绑定方法值尚未完成，见下文。
- 动态成员 site 构造移到 closed member ABI 分类之后，避免把 safe collection
  length 同时解释为动态 getter。Throw ExitPlan 与函数效果分析包含动态读取。
- 压力测试发现 string conversion 仍以源节点的 lvalue/rvalue 判断是否释放输入，
  因而漏掉新 getter 的 owned SSA 结果。现改用 `xl6IrLowerValueHasOwner`，保留
  明确 STORE/LOAD/RELEASE；未修改 Cgen 来猜测或回收源表达式。修正前每轮完整
  生命周期残留 5 次分配，修正后恢复初始 LiveCount/LiveBytes。
- 动态字符串 adapter 的 boxing 生成规范的独立语句，并将 nullable string 的
  null 显式装箱为 xvalue-null。普通函数与 async 保持同一个 owned success carrier；
  无效字段 receiver 的运行时错误转换为 NULL failure carrier，不制造带错误的成功值。

新增 `tools/check_xlir_dynamic_member_get.ps1` 已接入 core function suite、静态架构
检查和 XLIR-only runtime 清单。最终 `build/tmp/xlir_dynamic_member_v4_gate.log`：

- 6 个拥有型读取、4 个已验证 getter rows（其中 1 个继承 row）、1 个明确不支持的
  indexed getter；61 个 malformed site/kind/ownership/outcome/registry 变异全部在
  C/header 输出前拒绝。C/header/binding 重复生成一致。
- 100 次完整模块生命周期，覆盖基类/派生/继承 getter、异常、普通字段、嵌套临时
  receiver、null、nullable string、无效 receiver 和不支持的 getter。包含 300 次真实
  async 挂起，覆盖成功、恢复后的 getter 失败及 awaited Future 拒绝。
- 严格 C 内存检查为 **12,700 次分配平衡**，卸载恢复初始内存基线，无新增 invalid
  free、double free 或 UAF。JIT/AOT 均重复通过相同语义场景。
- 门禁另从当前源码直接编译运行 `cgen_var_class_dispatch`、`cgen_safe_member`、
  `cgen_dynamic_member_access`。原第 104 行失败已修复，未替换成静态 receiver 用例。
- v4 定向回归：nullable 17、index 12、field/callable/native/static 18、runtime/cleanup
  14、virtual/property/protocol/parameter/generator 16，加 8 个动态读取/方法/容器用例，
  共 **85 项通过**。各组结果见 `build/tmp/xlir_dynamic_member_v4_*regression_results.json`；
  `_comparison.json` 另保留旧检查点与当前版本的失败比较，不属于通过计数。
- property、virtual、member assignment、value lifetime 四个既有源码构建门禁重跑通过；
  静态 XLIR/migration/source/batch/Core C fixture 检查通过。Core fixture 518 / 11，
  编码检查 5,874 文件。迁移清单为 129 rows / 374 representatives / 442 deterministic
  candidates / 144 differential candidates / 74 XLIR-only contracts，不是全量运行计数。

扩大验证确认的剩余问题：`cgen_dynamic_method_inheritance` 仍实际运行失败；
`cgen_dynamic_method_bound_lifetime` 仍生成缺少 closure retain/release 声明的 C，且
动态 member 作为绑定方法值的 IR/环境 ABI 尚未完成。两者均用上一 v7 driver
重现，不归为本节新增回归，也不作为允许永久保留的兼容限制。动态 setter、协议
异常边界、virtual async/generator 声明、托管 SSA 最终消费、部分构造回滚、原生
借用、Future 终态及旧正式准入的语言 ABI 缺口仍须继续完成。

另一次旧发布编译器 `check_xlir_xcall.ps1` 在 common.bat 复制正在使用的运行时 DLL
时失败；该次未算通过，也不充当当前源码证明。后续同类门禁须避开运行中的源码
driver，或使用不覆盖运行时 DLL 的源码构建路径。源码哈希及扫描句柄记录在
`build/tmp/xlir_dynamic_member_v4_source_snapshot.json`。长期目标保持 active。

## 2026-09-08：动态绑定方法、继承重载与调用边界（dynamic-binding-v6）

本节源码 driver 为 `build/test/xlir_dynamic_binding_v6_driver.exe`。上一
dynamic-member-v4 的 494 项扫描已结束、494/494；本节 v6 的独立扫描也已真正结束，
**494/494、退出 0**，不是复用上一检查点的结果。发布 xlc.dll、xl.dll、xl.exe
哈希仍未改变，没有最新正式 CLI 全量准入。该源码扫描只证明其编译链路，不替代
下面的严格 C 和实际运行结果。

实现与纠正：

- 动态方法继承改在 Typed registry 中按完整参数签名遮蔽：派生类的同名异参方法
  不再丢掉基类其它重载。receiver runtime tag 与实际 inherited declaration 分开，
  Lowering/Verifier 保持精确的派生对象到基类 borrowed receiver 契约。
- MEMBER_GET site 新增 `memberBindingAbi`，明确区分不绑定与
  `XL6_IR_DYNAMIC_MEMBER_BINDING_RETAIN_RECEIVER`。Verifier 要求它与 getter/method
  分类和候选 registry 一致；方法值的可达性和 backend preflight 共用
  `xl6IrDynamicMethodMatchesCallSite`，没有直接 CALL 的绑定目标也不能被删除。
  文本/JSON dump 保留 binding ABI。
- 普通动态方法调用与绑定后的方法值共用一个 dispatcher emitter。getter、现存字段、
  方法组依次有明确行为；字段查询把“键不存在”与“值为 null”分开。不支持的 adapter
  在调用时显式失败，不以缺失字段或返回 null 伪装成功。
- 绑定方法用 XRT callable 环境持有 receiver 的一个强引用，在最后一个 callable
  引用释放时交回；XRT 的 borrowed callframe 参数先 retain，再交给两种 outcome
  都消费参数的共享 dispatcher。结果传输失败和环境/可调用对象创建失败均回滚。
  纯 erased 调用不再无条件生成引用 native closure retain/release 的桥接代码。
- 局部 var 的 flow narrowing 不改变 erased storage：未解析为字段/属性的成员值
  仍获得 canonical var 类型；动态调用的 borrowed receiver anchor 使用 lowered
  producer 的真实 TypeId，避免把 nominal xvalue 填入声明为 var 的临时存储。
  没有放宽 Verifier 的精确 STORE 类型检查。
- 扩大测试发现，async 参数只在首次 await 前改写时，单纯跨挂起 liveness 会漏掉
  可写激活存储。现在 IR 依据 lowered storage write/address 操作建立 owned launch
  parameter frame slot。UINT32_MAX/UINT32_MAX 表示不跨任何 suspension 的激活存储；
  它仍具有精确 retain/copy、init bit、释放 action 和参数身份。Verifier 同时检查
  必需槽不能缺失、无状态槽只能用于这种参数，不让 Cgen 新增参数赋值特判。
- 动态 XCall 现在建立独立 foreign invocation scope。原先绑定方法中的错误虽已被
  语言 catch 处理，callback failure 仍可能锁存到外层 export；现由最近调用边界先
  收束 outcome，再交给语言的 ERROR_TEST/catch。首个错误、参数/结果清理失败和
  没有诊断对象的失败位均由既有 scope ABI 保存，不以线程错误是否非空猜测成功。

新增 `tools/check_xlir_dynamic_bound_method.ps1`，已接入 core function suite、
静态 XLIR 边界和 XLIR-only 迁移清单。最终源码门禁日志
`build/tmp/xlir_dynamic_binding_v6_gate.log` 证明：

- 7 个 owned member reads、7 个 binding sites、1 个继承 method row；**114 个**
  site/kind/结果/错误/registry/可写 async 激活槽变异在 C/header 输出前拒绝。
  C/header/binding 重复输出一致。
- 100 次完整模块生命周期，验证原 receiver 改写后读取同一对象、清空原变量后仍可
  调用、复制方法值后最后一次释放触发析构、临时 receiver、继承重载、默认/命名/
  位置与关键字变长参数、string/nullable 结果、不匹配参数及不支持的 byref adapter。
- 300 次真实 async 挂起，覆盖成功、恢复后方法失败及 awaited Future 拒绝；
  async 参数在 await 前清空，绑定方法独立保活对象。
- XRT 原生对象存储中的同名 null 字段仍优先于方法组。保留前端对 flow-narrowed
  class 方法名写入的既有拒绝；没有为该运行时测试放开语言写入规则。
- 每轮包含结果 SetTake 传输失败；严格内存 lane 额外注入 **400 次真实分配故障**，
  覆盖 receiver 环境、callable、xvalue wrapper 和 borrowed frame 复制。
  **22,900 次分配全部平衡**，模块卸载恢复初始 LiveCount/LiveBytes，无新增 UAF、
  double free 或 invalid free。JIT/AOT 运行相同语言断言和确定性传输失败测试；
  实际分配器故障只计入严格内存 lane。
- 原始 `cgen_dynamic_method_bound_lifetime`、inheritance、overload、generic method、
  dynamic method arguments 均直接从本节源码编译并实际执行，未替换原断言。

本节其它验证：

- 当前源码 nullable/capture 17、index 12、field/callable/native/static 18、
  runtime/cleanup 14、virtual/property/protocol/parameter/generator 16、
  dynamic member/method/container 10，共 **87 项通过**。
- 动态 getter 既有源码门禁重新通过：61 个变异、300 次真实挂起、12,700 次平衡分配。
  属性事务与虚调用门禁也重跑通过，分别保留 605,100 和 15,300 次平衡分配。
- `check_xlir_xcall.ps1` 新增可选 SourceDriver lane，严格宿主不再依赖会覆盖运行时
  DLL 的 common.bat。以本节 driver 通过 bool/int/float、nullable closure、nominal class、
  raw pointer 的原有 IR/严格 C/运行和 legacy 差分断言。nullable 检查更新为显式
  LOAD <- STORE <- RETAIN <- UNWRAP 的所有权链，不能只因 producer 不直接是 UNWRAP
  而误报；未降低类型与 signature 检查。
- XCall result transaction 用本节 driver 的生成物再次通过：1,500 个部分失败
  （含 300 次真实分配故障）、400 个成功，JIT/AOT 的注入失败 lane 同样通过。
  该门禁的原始默认发布编译器路径不作为本节源码证明。
- XLIR/migration/source/batch/Core C fixture、文本编码、Markdown 链接检查通过。
  迁移清单 129 rows / 374 representatives / 443 deterministic candidates /
  144 differential candidates / 75 XLIR-only contracts；这是接线计数，不是正式准入率。
  Core C fixture 521 / 11、编码文件 5,879。证据与源码哈希见
  `build/tmp/xlir_dynamic_binding_v6_source_snapshot.json`。

长期目标仍为 active。动态 setter、完整协议异常边界、virtual async/generator
声明族、托管 SSA 最终消费、部分构造/转交及析构失败的组合路径、原生 borrow/layout、
Future 全终态与最终同快照 CLI/DLL 全量验收仍需继续。旧正式准入的 12 个差距中，
11 个语言 ABI 问题不因标准库最低范围而豁免。本节不能宣称整个 callable、async
或所有权体系已最终完成。

## 2026-09-08：动态 setter、赋值求值顺序与拥有型调用完成（dynamic-setter-v4）

本节最终源码 driver 为 `build/test/xlir_dynamic_setter_v4_driver.exe`。较早 v3 的
494 项扫描已真正结束、494/494；最终 v4 的独立扫描也已真正结束，494/494、退出 0，
不是借用 v3 的结果。下面的门禁与 87 项定向运行均来自最终 v4 源码。编译扫描不
代替实际运行或正式准入。发布
xlc.dll、xl.dll、xl.exe 未重建，哈希不变，没有最新同快照正式 CLI 全量准入。

本轮开始时，`build/tmp/xlir_dynamic_setter_probe.xl` 通过 `var receiver` 写属性会
直接生成 raw dynamic-field-set，绕过 setter 的副作用、校验和异常。旧绑定 v6
driver 的实际运行退出 1；修复后原断言严格 C 运行退出 0。

- Typed XLIR 新增 `MEMBER_SET`，与 METHOD/MEMBER_GET 分开；site 显式携带唯一
  RHS、receiver、成员身份、`ARGUMENTS_FIRST` 求值顺序、MAY_RAISE，以及
  `OWNED_UNIT_VALUE` 结果 ABI。成功返回 owned xvalue-null，物理 NULL 是失败，
  赋值语句通过标准 STORE/RELEASE 丢弃 unit；不把写入伪装成读取或普通方法。
- 公共 LET 声明进入 typed dynamic registry，继承遮蔽仍按声明族和完整参数签名
  选择。普通 setter 精确要求一个非变长参数、零返回值；indexed 等不支持的
  adapter 保留为明确不支持的 row，不能变成缺失属性。Verifier 覆盖 source/site、
  参数 arena、继承 receiver、结果、异常、求值顺序及 lowered 指令的关联。
- Lowering 先求 RHS 并保护 owner，再求 receiver。新增范围提交复用原求值 owner
  栈，只转交调用参数，保留随后取得的 borrowed receiver anchor。所有动态调用
  统一通过 `xl6IrLowerCompleteEvaluationCall` 完成 outcome-first 清理；成功结果
  在可能抛错的 receiver 清理期间同样具有显式回滚存储。
- Cgen 复用 getter/method 的 dispatcher 和 adapter，仅消费验证后的 site/registry。
  没有适配 setter 时，只要 registry 表明该接收者存在同名 getter 或 setter，就
  显式报错；只读属性、类型不匹配和不支持的 setter 都不能退回字段写入。真正的
  普通字段使用既有 consuming set-new 契约，成功/失败均消费 RHS，成功才生成 unit。
- 共享 dispatcher 建立独立 invocation scope，先保存调用失败，再清理 owned 参数；
  方法 adapter 通过既有 error-result ABI 发布失败，不制造额外 callback 诊断副本。
  调用失败与析构失败并存时保留首个失败；被语言 catch 处理的失败不能锁存外层
  export。scope 初始化失败也释放已接收参数，并保留原错误，不进入未建立的 scope。
- 压力测试暴露一个每轮 4 字节的字符串泄漏：把 `"bad"` 作为 var 参数装箱时，
  旧 Lowering 根据 source ownership 而非 lowered SSA owner 决定是否释放。现改用
  `xl6IrLowerValueHasOwner` 并保护字符串存储通过 boxing；仍保留底层借用式复制
  契约，不让 Cgen 猜测输入是否应释放。修复后完整模块卸载回到初始内存基线。

新增 `tools/check_xlir_dynamic_member_set.ps1`，已经接入 core function suite、
静态架构检查及 XLIR-only 迁移清单。最终 v4 门禁证明：

- 8 个拥有型写入、4 个支持的 setter rows（含 1 个继承 row）；153 个非法
  site/kind/参数/顺序/结果/异常/registry/指令变异在 C/header 输出前被拒绝。
  C/header/binding 重复生成一致。
- 100 次完整模块生命周期，覆盖基类/派生/继承 setter、副作用、原值不被失败写入
  覆盖、只读/indexed/参数不匹配、null/无效 receiver、普通类字段及开放对象字段。
  RHS-first 顺序、RHS 失败不求 receiver、receiver 失败回滚 owned RHS、参数析构
  抛错、setter 与析构同时失败均有实际断言。未改变已有语义负例来换取通过。
- 300 次真实 async 挂起，覆盖挂起前/恢复后写入、恢复后的 setter 失败、awaited
  Future 拒绝；200 次真实字段存储分配故障，100 次确定性的 invocation scope
  初始化故障。严格 C 共 21,400 次分配平衡，初始 LiveCount/LiveBytes 恢复，无新增
  UAF、double free 或 invalid free。JIT/AOT 重复语言和 scope 故障断言；真实分配器
  故障只计严格内存 lane，以上计数均不含预热生命周期。

最终 v4 的既有门禁：动态 getter 61 个变异、12,700 次平衡分配；绑定方法 114 个
变异、400 次真实分配故障、22,900 次平衡分配；属性事务 605,100、虚调用 15,300
次平衡分配。XCall SourceDriver 门禁通过；结果事务再次通过 1,500 个部分失败
（含 300 个真实分配故障）及 400 个成功，严格 C、JIT/AOT 均通过。六组定向回归
仍为 17 + 12 + 18 + 14 + 16 + 10 = 87/87，不把编译扫描计成实际运行覆盖率。

静态 XLIR/migration/source/batch、Core C fixture 通过（524 / 11）；文本编码
5,884 文件、Markdown 329 篇通过。迁移清单 129 rows / 374 representatives /
444 deterministic candidates / 144 differential candidates / 76 XLIR-only contracts，
仍只是清单接线计数。额外的全包 C fixture 检查没有通过：MQTT/serial 下 7 个 C
测试源尚未接线，见 `build/tmp/xlir_dynamic_setter_v4_all_c_fixture.log`；不把
CoreOnly 通过写成全包通过。本轮未修改这些扩展包。

下一处语言缺口已用最终 v4 实际复现：`build/tmp/xlir_protocol_outcome_probe.xl`
的 nominal 参数传给协议 var 参数时，Lowering 拒绝其 witness 参数 ABI；去掉参数
后的 `xlir_protocol_outcome_noargs_probe.xl` 可通过 IR 验证并生成 C，但严格 C 报告
协议 dispatcher 声明返回 int64_t、实际返回 xlir_result_i64。说明协议槽的结果/
异常契约尚未覆盖可能抛错的实现；下一步必须补齐统一 IR 契约与共用调用完成路径，
不能在 Cgen 中拆出 value 来掩盖失败。

长期目标保持 active。完整协议调用、virtual async/generator 声明族、最终 SSA
消费、部分构造/转交与析构失败组合、原生 borrow/layout、Future 全终态及最终
CLI/DLL 准入仍未完成；旧正式准入的 11 个语言 ABI 缺口不因标准库最低范围豁免。
本轮源码、driver、门禁和扫描句柄记录在
`build/tmp/xlir_dynamic_setter_v4_source_snapshot.json`。

## 2026-09-08：协议槽异常结果与共享分派调用（protocol-outcome-v5）

最终源码 driver 为 `build/test/xlir_protocol_outcome_v5_driver.exe`。本节不复用
dynamic-setter-v4 的 494/494 扫描作为当前源码证明；没有重跑本快照的 494 项扫描，
也没有重建发布 CLI/DLL 或进行最新同快照正式准入。最终源码树、driver 和 20 个
任务文件哈希见 `build/tmp/xlir_protocol_outcome_v5_source_snapshot.json`。

- 协议槽新增明确的 `errorAbi/errorPhysicalAbi`，在调用图固定点中汇总全部 witness
  的异常效果。普通实现仍保留自己的 direct-call ABI；可能抛错的协议槽统一返回
  result carrier。异步启动的 NULL 失败与 Future 后续拒绝保持区分。
- 协议参数 ABI 根据替换泛型后的完整参数节点建立，保留 byval/byref/out，而非只按
  类型是否托管分类。普通调用/get/let 共用协议 invocation Lowering：先完成目标
  类型转换及装箱，建立参数与 receiver 的 owner，再以共享 CompleteCall 检查
  outcome、回滚结果并清理临时 owner。setter 保持 RHS-first 的取得和逆序清理。
  不再以原始实参 storageType 必须等于协议参数 TypeId 来阻止合法装箱。
- Verifier 要求协议槽效果等于所有实现的汇总值；验证 receiver、参数模式与物理
  参数 ABI、同步 carrier/异步 NULL/外部 scope 的结果适配。may-raise void 调用
  也有专供 ERROR_TEST 消费的 control carrier，每次调用必须立即观察 outcome。
  Typed/Lowered text 和 JSON dump 保留槽的错误 ABI；JSON 同时保留参数类型和模式。
- 虚调用与协议调用共用 Cgen 的物理签名、调用输出及 witness 结果适配；普通 CFG、
  async machine、generator machine 共用同一分派调用及错误检查。派生类接收者
  的名义兼容性由 IR verifier 证明，Cgen 只检查 xvalue* 物理表示；扩展回归曾发现
  新共享检查误用 TypeId 相等，已修正并以最终 v5 重跑全部本节定向回归。
- 外部 C witness 使用 CallTarget 已验证的原生 receiver 字段，建立独立 foreign
  invocation scope，失败时先固定 outcome 再回滚已取得的拥有型结果。scope 进入
  失败也清理尚未转交给 C 函数的 byval 参数，使用隔离的 cleanup scope 保留首个
  失败。typed owner 的释放输出与 XCall 结果事务复用一个机械 helper。
- 可写参数的绑定分析补充显式 byref/out argument wrapper，避免协议调用造成
  参数重新绑定后仍被当成稳定 borrowed binding。这里处理的是 Typed IR 中的
  绑定事实，不在 Cgen 中追溯源表达式。

新增 `tools/check_xlir_protocol_call_outcome.ps1`，接入 core function suite、
静态架构检查和 XLIR-only 迁移清单。最终 v5 日志
`build/tmp/xlir_protocol_outcome_v5_protocol_call_outcome.log` 证明：

- 9 个协议槽，32 个调用，5 个混合效果槽中的 no-raise witness，9 个 consuming
  参数、6 个 mutable 参数、1 个 async witness；121 项非法模块变异在 C/header
  输出前拒绝，C/header/binding 重复生成一致。
- 100 次完整模块生命周期；验证同步 get/let、var 装箱、byval/byref/out、
  生成器恢复/失败/提前关闭、协议方法在 async worker 中调用，以及真正异步的
  协议实现。600 次真实挂起覆盖成功、恢复后方法/get/let 失败及 awaited Future 拒绝。
- 外部原生 witness 的拥有型参数消费、借用字符串结果复制与失败回滚均实际运行；
  每轮 2 次确定性的 scope 进入失败覆盖参数析构同时抛错。调用错误 73/81 与
  析构错误 74 并存时保留首个调用错误；scope 进入错误 78 同样保留。语言 catch
  处理后外层 export 不残留失败，未捕获路径仍能向宿主交付正确错误码。
- 严格内存 lane 有 32,700 次平衡分配，最终卸载恢复初始 LiveCount/LiveBytes，
  无新增 UAF、double free 或 invalid free；严格 C、JIT、AOT 全部通过。本协议
  门禁未新增真实分配器故障注入，不能把确定性 scope 故障计作真实 allocator 故障。

其它最终 v5 验证：

- nullable 17、index 12、field 18、runtime/cleanup 14、virtual/property/parameter/
  generator 16、dynamic 10，共 87 项运行回归通过；另跑 16 个原始 protocol 用例，
  均重复生成、严格 C 编译并实际运行，包括 native-backed 与 async dispatch。
  协议组与既有组有重叠，不把两组相加称为去重后的独立覆盖率。
- 虚调用源码门禁再次通过：26 个变异、15,300 次平衡分配。动态 setter 源码门禁
  再次通过：153 个变异、300 次挂起、200 次真实分配故障、21,400 次平衡分配。
- XCall SourceDriver 门禁通过。XCall result transaction 使用本节 driver 再次
  通过 1,500 个部分失败（含 300 次真实分配故障）、400 个成功及严格 C/JIT/AOT；
  测试期间未覆盖已加载的运行时 DLL。
- XLIR/migration/source/batch/Core C fixture、编码、Markdown 检查通过。迁移
  清单为 129 rows / 374 representatives / 445 deterministic candidates /
  144 differential candidates / 77 XLIR-only contracts；Core C fixture 527 / 11。
  这些仍是接线计数。上节全包 MQTT/serial 7 个未接线测试未在本轮处理或复验。

协议调用尚未完全收口：本轮用
`build/tmp/xlir_protocol_named_argument_probe.xl` 确认前端接受命名参数调用，但
witness Lowering 仍读取 positional wrapper，未消费已有 CallSite 中的规范化
参数映射。当前 CallSite 也尚未记录协议 receiver/slot 的完整关联；下一步应将
这些绑定在 Typed IR 中确定，再让 Lowering 消费，而不是添加 Cgen 命名参数补丁。
协议 indexed/compound property 与复合泛型签名也仍需专门验证。

长期目标保持 active。virtual async/generator 声明族、最终 SSA 消费证明、部分
构造/转交与抛错析构的组合、原生 borrow/layout 和 Future owning payload、
Future 全终态以及最终同快照 CLI/DLL 全量准入仍未完成。原始 native-backed
protocol 用例已在当前源码通过，但旧正式准入的 12-gap 数字没有因此被改写；
它仍是旧发布编译器记录，不是本节源码的最新准入结果。

## 2026-09-08：CallSite 源码求值顺序与参数身份（callsite-normalization-v7）

最终源码 driver 为 `build/test/xlir_callsite_normalization_v7_driver.exe`，旧门禁的
text/JSON/header 检查另用同一源码编译的
`build/test/xlir_callsite_normalization_v7_source_cli.exe`。发布 CLI/DLL 未重建，
本节未重跑 494 项扫描或全量正式准入。源码、任务文件、driver、发布产物与验证记录
保存在 `build/tmp/xlir_callsite_normalization_v7_source_snapshot.json`。

- CallArgument arena 仍按物理形参位置组织，但新增独立 `evaluationIndex`；显式参数
  按源码顺序求值，再按声明顺序求省略的默认参数。CallSite 新增 dispatchKind 和
  receiverEvaluationIndex，分别固定槽的解释域及接收者与实参的交错顺序。
- 协议方法/get/let 消费相同的规范化 CallSite，不再按 positional wrapper 读取
  实参或按声明 symbol 搜索所有协议实例。协议调用效果只取已选具体槽，class 和
  protocol 的数值 dispatchSlot 不再在效果分析中混用。
- 普通函数、构造函数和 C variadic 按 evaluation 顺序求值，按 parameter 位置
  组装物理调用。ARRAY/DICT variadic 在首个 tail item 前惰性建立并保护容器，
  允许关键字 tail 与固定实参交错，默认参数在全部显式参数后执行。
- 普通 magic-method 语法调用也消费 receiver/evaluation 契约。实际复现并修复
  `left in receiver` 被反向求值，以及索引写按 receiver/index/RHS 求值的问题；
  简单索引写现为 RHS、receiver、各 index。`__setField__` 的隐式名称同样是
  一个 IR binding，不在 Cgen 中拼回源语法。
- 临时 owner 与 native destroy identity 从物理参数位置映射到实际取得顺序；
  成功及失败均逆序清理。验证器除稠密唯一求值序号外，还独立证明显式/默认参数
  的来源、命名身份和 byref/out wrapper，交换表达式和序号不能绕过绑定验证。
- 本检查点的编译器修改仅在三个 IR 文件；上一节快照中的 Cgen 文件哈希未变。
  没有通过增加命名参数、索引或求值顺序的 Cgen 特判解决这些问题。

新增 `tools/check_xlir_callsite_evaluation.ps1`，接入 core function suite、静态
架构检查和 XLIR-only 清单。最终日志
`build/tmp/xlir_callsite_normalization_v7_gate.log` 证明：

- 51 个重排绑定、6 个协议 site、10 个 RHS-first site、3 个交错关键字调用、
  2 个 membership site、9 个索引写 site、8 个 mutable 参数；642 项非法模块
  变异在 C/header 输出前拒绝，C/header/binding 重复生成一致。
- 100 次完整模块生命周期验证 direct/protocol/constructor 命名参数、默认参数、
  keyword 聚合部分失败、单/多维索引写、membership 和逆序 owner 清理。RHS、
  receiver、首/后 index 任一步抛错时，尚未求值的表达式不会执行。
- 宿主实际断言调用错误、求值错误和清理错误同时存在时的错误优先级；无调用
  失败时的首个清理错误分别为 103/101，有调用或求值失败时保留 90/91。catch
  处理后外层 export 不残留失败，未捕获路径保留原始错误码。
- 严格内存 lane 41,600 次分配平衡，卸载恢复初始 LiveCount/LiveBytes，无新增
  UAF、double free 或 invalid free；严格 C、JIT、AOT 全部通过。本专项没有新增
  真实分配器故障注入，也不以它替代 async/cancellation 的专门生命周期门禁。

最终 v7 的既有验证：

- 六组 17 + 12 + 18 + 14 + 16 + 10 = 87 项运行/源码契约回归通过；另跑 16 个
  原始 protocol 用例，重复生成、严格 C 和运行全部通过，两组存在重叠。
- 协议结果门禁再次通过 121 个变异、600 次真实挂起、32,700 次平衡分配；虚调用
  26 个变异、15,300 次平衡分配；动态 setter 153 个变异、300 次挂起、200 次
  真实分配故障、21,400 次平衡分配。XCall SourceDriver 及结果事务通过，后者
  包含 1,500 个部分失败（300 个真实分配故障）、400 个成功和严格 C/JIT/AOT。
- 原有 binding、ARRAY/DICT variadic、C variadic、generic specialization 和
  class protocol syntax 五项门禁以同源码 driver 执行原有断言与严格 C runtime。
  检查脚本更新新增文本字段；C variadic 分别验证成功/失败分支各自的两个逆序
  string release，不再按整份模块总数判断平衡。field setter 检查限定单个操作，
  验证 RHS boxing、隐式 name、调用与立即 outcome，避免全模块正则回溯。
  验证期间只替换编译器入口和 host 编译命令，没有调用 common.bat 覆盖运行时 DLL。
- XLIR/migration/source/batch/Core C fixture、编码及 Markdown 检查通过。迁移
  清单 129 rows / 374 representatives / 446 deterministic / 144 differential /
  78 XLIR-only contracts；Core C fixture 530 / 11。清单数字不是运行覆盖率；
  MQTT/serial 的全包 7 个未接线测试未在本轮处理或复验。

下一处语言缺口已有当前源码复现：`build/tmp/xlir_protocol_indexed_property_probe.xl`
的协议 `get value(int key)` / `let value(int key, int next)` 被前端接受，但读取
`box.value[key]` 在 Typed IR 构建时报“value expression has no canonical type”。
它不同于前端本来不接纳的 protocol `__item__` 索引语法，必须继续检查实际支持的
索引属性绑定，不能通过拒绝整个功能或在 Cgen 回查语法绕开。

长期目标保持 active。协议 indexed/compound 与复合泛型签名、virtual async/generator
声明族、最终 SSA 消费、部分构造/转交与抛错析构组合、原生 borrow/layout、Future
拥有型结果和完整终态，以及同快照 CLI/DLL 全量准入仍未完成。

## 2026-09-08：索引属性 CallSite 与声明级泛型 owner（indexed-accessor-v5）

最终源码 driver 为 `build/test/xlir_indexed_accessor_v5_driver.exe`；text/JSON/header
检查使用同源码 `build/test/xlir_indexed_accessor_v5_source_cli.exe`。发布 CLI/DLL
没有重建，本检查点未重跑 494 项扫描或全量正式准入。源码、任务文件、driver、
发布产物和验证日志哈希见 `build/tmp/xlir_indexed_accessor_v5_source_snapshot.json`。

- Typed XLIR 将 `receiver.property[keys]` 的已绑定索引 getter 规范到整个 INDEX
  节点。中间 MEMBER 只保留选择器身份，是无类型、无值类别、无独立调用的节点；
  CallSite 明确记录真实 receiver 和各 index。验证器拒绝选择器冒充值或接收者。
  返回容器的普通 getter 则仍是独立的有类型调用，其后才是普通容器索引。
- class 与 protocol 的索引 getter 分别进入既有统一 syntax/protocol invocation，
  复用参数物理投影、源求值顺序、outcome-first、owned 结果回滚与逆序临时清理。
  class 属性事务的 getter origin 同步指向整个 INDEX，compound 的 getter site
  查询不再展平选择器。协议 managed getter 结果明确为拥有型读取。
- 组合测试定位并修复原有的泛型多实例错误：成员不使用 T/self 时，旧代码没有将其
  识别为泛型成员，并从首个 class layout 选择 receiver；同一类实例化为 int 和
  托管对象后，协议实现绑定到错误的 receiver TypeId。旧 v7 driver 也能复现。
- 节点新增 `declarationOwnerType`。AST→Typed IR 边界从 class 的语义声明及全部
  声明级参数建立名义模板，包括完全未使用的泛型参数；直接成员保留该 owner。
  特化时与函数体一起替换 owner，类型参数顺序先取声明向量，不能由方法体里的
  类型出现顺序或 self 使用决定。Lowering 的 receiver 查询仅消费这一身份。
- Verifier 检查 class 名义 symbol、声明参数顺序、原始成员 owner、特化参数向量
  与实际 callable receiver 的一致性。text/JSON dump 保留 owner。协议 witness
  特化不再跨类型 arena 增长保留类型行指针；名义 class 实例按 symbol 而非名字
  匹配。以上修改均在 IR 层，Cgen 文件与 callsite-normalization-v7 保持一致。

新增 `tools/check_xlir_indexed_accessor.ps1` 及语言、contract、host、runtime 四个
fixture，接入 core function suite、静态架构检查和 XLIR-only 清单。最终 v5 证明：

- 12 个索引 getter（7 protocol / 5 class）、3 个独立容器 getter 读取、
  18 个精确泛型成员 owner；144 项非法 IR 变异在 C/header 输出前拒绝，
  C/header/binding 重复生成一致。
- 实际运行 class/protocol 的索引读写、两种同名泛型实例、双泛型参数在成员签名中
  的不同出现顺序、不使用 T/self 的 getter，以及无字段/无 T 使用的空泛型类成员。
  返回容器的 getter 与其后索引分开求值，setter 保持 RHS-first。
- 100 轮完整模块生命周期覆盖调用失败与 key 析构同时失败、成功产生 owned 结果
  后 key 析构失败的结果回滚、setter 失败、语言 catch 及未捕获错误向宿主交付。
  原始调用错误 70/71 不被清理错误覆盖；没有调用错误时保留首个清理错误 103。
- 严格内存 lane 50,600 次分配平衡，卸载恢复初始 LiveCount/LiveBytes，无新增
  UAF、double free、invalid free；严格 C/JIT/AOT 全部通过。本专项没有新增
  真实 allocator 故障或 async/cancellation 覆盖，不能替代对应生命周期门禁。

最终 v5 的既有验证：

- nullable 17、index 12、field 18、runtime/cleanup 14、virtual 16、dynamic 10，
  共 87 项运行/源码契约回归通过；另跑 16 个原始 protocol 用例，重复生成、
  严格 C 编译和实际运行通过。两组有重叠，不相加称独立覆盖率。
- 属性事务再次通过 605,100 次平衡分配与 3,200 次真实 async 场景；协议结果
  121 变异 / 600 次挂起 / 32,700 次平衡分配；调用顺序 642 变异 / 41,600
  次平衡分配；虚调用 26 变异 / 15,300 次平衡分配；动态 setter 153 变异 /
  300 次挂起 / 200 次真实分配故障 / 21,400 次平衡分配。
- 原 generic-specialization 门禁用同源码 CLI adapter 执行原断言、text/JSON、
  重复 C 生成与严格 C runtime，包括泛型 class protocol syntax。仅替换编译器
  入口和 host 编译命令，没有执行 common.bat 覆盖运行时 DLL。
- XCall SourceDriver 门禁通过；结果事务用同源码 CLI adapter 再次通过 1,500
  个部分失败（含 300 个真实分配故障）、400 个成功及严格 C/JIT/AOT。
- XLIR/migration/source/batch/Core C fixture、编码与 Markdown 检查通过：
  129 rows / 374 representatives / 447 deterministic / 144 differential /
  79 XLIR-only contracts，Core C fixture 533 / 11，编码 5,899、Markdown 329。
  这些清单数不是运行覆盖率；MQTT/serial 全包 7 个未接线测试没有处理或复验。

后续缺口已实际复现，而非仅列待办：
`build/tmp/xlir_protocol_indexed_compound_probe.xl` 的
`box.value[2] += 3` 通过 Typed/Lowered 验证并能生成严格 C，但写入被遗漏，
运行在第 17 行的结果断言失败，日志
`build/tmp/xlir_indexed_accessor_v5_compound_runtime.log`。Lowering 将 property
compound 交给仅接受 class PROPERTY_SET 的处理器，WITNESS 返回未处理却没有
设置失败；CallSite 也没有给该 compound 写入建立完整的读改写契约。不能把本节
索引读与简单写专项通过当成协议 compound 已完成，也不能靠拒绝整个语言功能收口。

长期目标保持 active。下一步补齐协议复合属性的显式读改写契约与共享完成路径；
virtual async/generator 声明族、最终 SSA 消费、部分构造/转交与抛错析构组合、
原生 borrow/layout、Future 拥有型结果和完整终态、同快照 CLI/DLL 全量准入仍未完成。

## 2026-09-08：共享属性读改写契约（property-update-v3）

最终源码 driver 为 `build/test/xlir_property_update_v3_driver.exe`；text/JSON/header
检查使用同源码 `build/test/xlir_property_update_v3_source_cli.exe`。发布 CLI/DLL 没有
重建，也未进行同快照正式准入。已启动 494 项源码生成扫描，写入
`build/tmp/xlir_property_update_v3_audit_source_audit.json`；本节记录时尚未完成，
不能把扫描启动或部分结果写成 494/494 通过。其后续终态需要继续核对。

- 修复上节实际复现的协议 indexed `+=` 遗漏写入：PROPERTY_SET/WITNESS compound
  都建立 setter CallSite，`preparedGetter` 指向规范化 getter site。最后一个参数
  明确为 `PROPERTY_UPDATE`，expression 指向更新操作，不能冒充原始 RHS。getter
  与 setter 共用 receiver/key 绑定；compound 为 receiver/key/get/RHS-operation/set，
  与简单赋值的 RHS-first 分开记录。所有相关字段和来源均进入 text/JSON dump。
- 新增无分配的 `PropertyAccessorSignature` 视图，投影已有 direct/class/protocol
  参数、receiver、返回与异常 ABI。class/protocol compound 共享属性事务 Lowering；
  不再在 Lowering 从 MEMBER/INDEX 重新拼接 receiver/key 或按 symbol 扫描虚槽。
  简单 class 属性读写也复用该计划，普通协议调用继续消费既有统一 invocation。
- 事务持有原始 receiver/key owner 直到 setter 完成；byval key 为 getter 和 setter
  分别取得可转交引用，不能在 getter 后释放整项访问。调用 outcome、owned 结果
  回滚、逆序清理继续共用 CompleteCall/CompleteEvaluationCall。协议错误与清理
  错误并存时保持首个调用失败，setter 已完成后的清理失败仍保留实际提交结果。
- 验证器证明 getter/setter 关联、receiver/key 一致、computed value 身份及 concrete
  参数/结果 ABI。未处理的属性更新现在明确使 Lowering 失败，不能返回一个遗漏
  操作的“成功”模块。Cgen 没有新增属性/协议/泛型特判，相关文件哈希保持不变。
- 泛型实例克隆同步替换 preparedGetter 和 computed argument origin。新测试发现
  generic syntax pass 曾提前实例化模板中的属性写；现仅处理 concrete receiver，
  原始模板仍保留可检查的 Typed 绑定，单态实例才接受物理签名验证。没有依赖
  方法体里的 T/self 使用来选择 owner；无 T 使用成员的两个实例均实际运行。
- 定向回归发现新公共属性路径误接管 dynamic `__getField__` site；已按显式 call
  kind 在进入 accessor 计划前区分。最终 v3 重跑相关回归及调用顺序门禁通过。
  这是操作类别路由，不是在 Cgen 或失败后重试另一种语义。

新增 `tools/check_xlir_property_update.ps1` 与语言/contract/host/runtime 四个 fixture，
接入 core function suite、静态架构检查及 XLIR-only 清单。测试复用原属性事务的运行
断言，将 receiver 换为协议，并补充泛型 owner 与两个 consuming class key 的组合：

- 43 个实际 accessor 调用、27 个立即检查的异常结果、14 个实际共享 getter/setter
  对；另保留 1 个未执行的泛型模板。15 个 prepared update site 中 12 个为协议。
  176 项非法 IR 变异在 C/header 输出前拒绝；C/header/binding 重复生成一致。
- 100 个计量循环包含 8,800 个 read/set/compound 失败组合、300 个 raising-receiver
  cleanup、800 个双 key 消费与首错误保留场景，以及 200 个泛型成员实例。receiver、
  key、getter、RHS、运算符和 setter 均有执行顺序/失败断言；getter 或 RHS 导致
  全局 receiver/key 重绑定时，后续 setter 仍使用最初取得的值。
- 3,200 个 async 场景中，2,400 次实际挂起/恢复，800 次 getter 在 await 前失败；
  二者明确区分。成功、awaited Future 拒绝、恢复后 setter 失败和 alias 替换均
  实际运行。旧 property fixture 的“3200 genuine”输出不应解释为 3,200 次挂起。
- 严格内存 lane 623,100 次分配平衡，计量前后 LiveCount/LiveBytes 一致；最终
  模块卸载恢复初始基线，无新增 UAF/double/invalid free。严格 C、JIT/AOT 全部
  通过。这里是一次模块生命周期内的 100 个计量循环，不是 100 次模块装卸。
  本专项没有新增真实 allocator 故障注入或 cancellation 覆盖。

最终 v3 的其它验证：

- 六组 17 + 12 + 18 + 14 + 16 + 10 = 87 项运行/源码契约回归通过；另跑 16 个
  原始 protocol 用例，重复生成、严格 C 和实际运行通过。两组有重叠。
- 原 class 属性事务、索引属性、协议结果、调用顺序、虚调用、动态 setter 门禁
  全部以最终源码重跑通过；分别保持 605,100 / 50,600 / 32,700 / 41,600 /
  15,300 / 21,400 次平衡分配。动态 setter 的 200 次真实分配故障也重跑通过。
- 原 generic-specialization 门禁使用同源码 CLI adapter 执行原 text/JSON、实例
  retargeting、重复 C 和严格 C runtime 断言。XCall SourceDriver 通过；结果事务
  再次通过 1,500 个部分失败（300 个真实分配故障）、400 个成功与严格 C/JIT/AOT。
  未运行 common.bat 覆盖已加载的 runtime DLL。
- XLIR/migration/source/batch/Core C fixture、编码和 Markdown 检查通过：
  129 rows / 374 representatives / 448 deterministic / 144 differential /
  80 XLIR-only contracts；Core C fixture 536 / 11，编码 5,904、Markdown 329。
  这些仍是清单接线数字；MQTT/serial 全包未接线项没有在本轮处理或复验。

下一处已有当前源码证据：`build/tmp/xlir_virtual_generator_declaration_probe.xl`
的 virtual/override generator 方法被前端接受，但 Lowering 报
“XLIR virtual implementation has no exact synchronous signature ABI”。需把 virtual
调用族的生成器启动、frame receiver ownership 与结果/异常 ABI 一并规范，不能把
同步虚方法在 generator 中调用的已有覆盖当成该声明族已完成。另一个 virtual async
probe 在前端被拒绝；检查确认 dispatch modifier 校验未包含 ASYNC_FUNC，需连同
语言设计和前端约束处理，而非只改后端绕过。

长期目标保持 active。494 项扫描仍需完成观察；virtual async/generator 声明族、
最终 SSA 消费、部分构造/转交与抛错析构组合、原生 borrow/layout、Future 拥有型
结果和完整终态、最终同快照 CLI/DLL 全量准入仍未完成。

## 2026-09-08：虚方法 activation 与启动失败 ABI（virtual-activation-v3）

上一检查点 property-update-v3 的 494 项源码扫描已核对终态：494/494，进程退出 0。
该结果只代表当时源码的 IR 验证与 C 生成，不是运行时或同快照发布准入。

本轮源码 driver 为 `build/test/xlir_virtual_activation_v3_driver.exe`。修复前两个
probe 的拒绝已消除；新增 `xlir_virtual_activation` 语言/contract/host/runtime
fixture 和源码编译门禁，并接入 core function suite 与 XLIR-only 清单：

- 前端允许 async class 方法使用 virtual/override/final，并让它们进入既有精确
  签名、显式 override、final、static/private 禁止规则，不是仅删除诊断。
  async 方法的预检查返回类型查询现在描述 invocation 结果，修复赋值检查在正式
  call checking 之前把 Future<T> 误认成 worker T 的问题。协议调用复用相同规则。
- class/protocol 槽位一致描述普通 T、async Future<T>、generator<T> 结果。
  async generator 仍是 generator<T>，不包 Future。协议 generator 要求、成员调用
  新建的线性 generator handle，以及 generator frame 的 Future 参数进入明确契约。
  仍禁止在 generator 函数中用 return 返回另一个 generator；该语法不是此次放宽项。
- generator 启动不执行 body，但 frame/capture/handle 分配可以失败。因此启动器
  使用 MAY_RAISE + NULL_RESULT，workerErrorAbi 独立。class/protocol 槽位汇合的是
  invocation effect；现有共同 witness adapter 将 NULL_RESULT 转为 RESULT_CARRIER。
  verifier 要求 GENERATOR_START 立即检查结果，之后才能清理或继续求值。
- Cgen 的修改仅为把已有 NULL_RESULT 检查归入共同物理 outcome 发射函数，并让
  普通、async、generator 三种执行模式消费它；移除了重复的 async-start 特判。
  没有新增 AST 回溯、按方法名推断、失败后重试或 generator 虚调用专用语义路径。

最终 v3 专项（`build/tmp/xlir_virtual_activation_v3_gate.log`）：

- 3 个 class 槽、3 个 protocol 槽，10 个立即检查的调用，4 个 generator worker，
  4 个 frame 持有 receiver；27 项非法 IR 在 C/header 输出前拒绝，6 个前端反例
  保持拒绝。C/header/binding 重复生成一致，严格 C、JIT 和 AOT 均通过。
- 一次模块生命周期内 100 个计量循环：900 个 generator 正常完成、worker 失败、
  提前关闭场景；900 次真实 async 挂起/恢复，覆盖直接成员、基类类型与协议 receiver，
  正常 resolve、worker 抛错与 awaited Future reject。局部 receiver 在 frame 完成前
  保持存活，终态后恰好析构；lazy body 在启动时不执行。另运行 class/protocol 的
  async-generator ready-Future await/yield 与 base 显式静态分派。
- 对直接 generator 启动、class dispatcher、protocol dispatcher 三条物理边界分别
  注入 frame、string capture、generator 对象分配失败，共 900 个真实故障；另有
  成功启动后未恢复即关闭的对照。30,100 次计量分配平衡，模块卸载恢复初始基线，
  无新增 UAF/double/invalid free。JIT/AOT smoke 不执行 allocator 故障注入。

最终 v3 既有回归：nullable 17、index 12、field 18、runtime/cleanup 14、virtual 16、
dynamic 10，共 87 项通过；另有 26 个协议/async/generator 用例及 1 个 native-backed
async 方法严格 C 运行通过。以上有重叠，不能相加作为独立覆盖率。
原 virtual-call-outcome、protocol-call-outcome、callsite-evaluation、property-update、
indexed-accessor、dynamic-member-set 六项源码门禁全部通过。静态清单为 129 rows /
374 representatives / 449 deterministic / 144 differential / 81 XLIR-only；这些不是
全量运行覆盖率。MQTT/serial 全包接线项本轮未处理，发布 CLI/DLL 没有重建。

新源码的 494 项扫描已启动，结果写入
`build/tmp/xlir_virtual_activation_v3_audit_source_audit.json`，本节记录时仍在运行。
后续必须核对同一进程的终态，不将上一检查点的 494/494 冒充当前源码通过。
长期目标保持 active：最终 SSA 消费、部分构造/转交与抛错析构组合、原生 borrow/layout、
Future 拥有型结果与完整取消终态、同快照 CLI/DLL 全量准入仍需继续；本专项不证明这些
组合已经全部覆盖，也没有替代最终验收。

下一项已用当前源码复现：`build/tmp/xlir_generator_owned_capture_probe.xl` 与
`build/tmp/xlir_async_owned_capture_probe.xl` 的 byval class 参数通过前端，但分别
在 Lowering 报 “generator parameter has no explicit frame capture ABI” 和
“async task frame has no explicit managed capture ABI”。目前启动器支持 borrowed
参数的 copy/retain，不等于已实现 consuming 参数的 frame take；后续必须明确
启动前转交、部分捕获失败回滚、worker/取消/关闭的唯一 owner，不能靠新增 retain
规避语言的 byval 语义。日志使用 `xlir_virtual_activation_v3_*_owned_capture_probe` 前缀。

## 2026-09-08：consuming activation capture 与 worker owner（owned-activation-v3）

virtual-activation-v3 的 494 项扫描已核对终态：494/494，退出 0。该结果属于上一
源码快照，仅证明其 IR 验证/C 生成，不是本轮或最终发布的全量运行验收。

本轮消除了上述两个 byval class probe 的拒绝，并同时处理四种物理路径：generator
启动、通用 async continuation、ready-Future 和任务池 worker。主要修改如下：

- IR 新增 launch-only `TAKE_PARAMETER`；统一参数 capture 规则，不复用需要 SSA
  takeInstruction 的 `MOVE_OWNED`。machine 构造与 verifier 均要求所有 consuming
  输入先入帧，再进行 fallible borrowed copy/retain。unused byval 也进入 owner 集合。
- 没有帧时逆序回滚传入 owner；部分捕获失败按初始化位释放；清理作用域保留第一
  失败，包括析构也抛错的情况。async/generator/pool 共用 capture 与 rollback renderer。
- 发现并修复旧 worker 终态清理只选择 FUNCTION_USER 的遗漏。拥有型参数在三种
  function kind 中都有显式 RELEASE/CLEANUP_CAPTURE 与参数生命周期验证；borrowed
  参数的 lexical copy 和 frame retain 则保持不同 owner 契约。任务池 worker 调用前
  detach consuming frame 字段，避免最终 callback 重复释放。
- ready-Future 在 consuming worker 调用前分配结果盒，既能回滚输入，也避免返回
  拥有值后才分配盒所造成的结果泄漏。未新增 Cgen AST 回溯或源码名称语义分派。

新增 `xlir_owned_activation` 的语言/contract/host/runtime fixture 与源码门禁，接入
core function suite、静态架构检查及 XLIR-only 清单。最终专项日志为
`build/tmp/xlir_owned_activation_v3_gate.log`：

- 8 个 consuming capture、18 条显式 worker release；52 项 capture ABI、参数 ABI、
  frame transfer/drop/init-bit、prologue 顺序及缺失 worker release 的非法 IR，均在
  C/header 输出前拒绝。C/header/binding 重复生成一致。
- 50 个计量循环：300 次真实 async 挂起/恢复，覆盖成功、body failure、awaited
  Future reject/cancel/close 和析构失败；generator 正常耗尽、未恢复即关闭、body
  failure 与析构失败；ready 与任务池 worker 的正常、析构失败、第一错误保留。
- 1,100 次真实 allocator 故障，覆盖 generator frame/copy/handle、ready promise/
  cancel token/结果盒、任务池初始化/frame/copy、async frame/copy；分别启用和禁用
  抛错析构。42,400 次计量分配平衡，无新增 UAF/double/invalid free。跨线程计数使用
  XRT 原子操作。严格 C、JIT/AOT 均通过；JIT/AOT smoke 不注入分配失败。
- 重要边界：宿主显式 close/wait/destroy 并重置测试池；该动作不是生成的模块卸载。
  当前生成 C 仍有 pool 静态变量而无 module resource unload action，这项语言运行时
  生命周期缺口必须继续处理，不能用本专项结束后的内存基线将其掩盖。

同语义源码的既有 virtual-activation 与 parameter-cleanup-outcome 门禁重跑通过。
后者保留 433 项非法 IR、严格 lane 的 6,600 个场景（含 1,100 个真实诊断分配失败）、
209,000 次平衡分配，以及 JIT/AOT smoke 各 5,500 个场景。当前源码 driver
`build/test/xlir_owned_activation_v3_driver.exe` 另完成 nullable/index/field/runtime/
virtual/dynamic 六组共 87 项，以及 28 个协议/async/generator/native/async-void 用例，
均通过相应源码契约或严格 C 运行；组间有重叠，不相加作独立覆盖率。单独旧
`check_xlir_async_void.ps1` 使用发布 CLI，不能充当新源码验证；此处的 async-void
另由新 driver 重生 C 并严格运行通过。

静态接线：129 semantic rows、374 representatives、450 deterministic candidates、
144 differentials、82 XLIR-only contracts；Core C fixture 542/11。接线数字不是全量
执行结果。发布 CLI/DLL 未重建，正式 admission 未更新。原扩展库延期范围不变。

本轮 494 项扫描已完成，结果为 494/494、失败 0，同一进程已核对退出码 0；结果写入
`build/tmp/xlir_owned_activation_v3_audit_source_audit.json`。这是当前 driver 的 IR 验证与
C 生成结果，不是全量运行覆盖率，也没有复用上一轮扫描。本轮快照文件为
`build/tmp/xlir_owned_activation_v3_source_snapshot.json`。
长期目标保持 active：更多 consuming 类型/alias/cell/取消组合、最终 SSA owner 消费、
部分构造与抛错析构、native borrow/layout、Future owning payload、任务池模块卸载、
同快照 CLI/DLL 全量准入仍未完成；byval class 专项通过不代表所有这些语言边界闭合。

## 2026-09-08：模块任务池资源与卸载前静止（module-pool-v4）

本轮沿用 owned-activation-v3 的语言正确性优先范围；不是标准库重设计或发布。
新增 `XL6_IrModuleTaskPoolAbi` 与 canonical requirement：从已物化的 Lowered
worker / runtime operation 决定是否拥有任务池资源。无 await 的 pool worker 与
`TASK_SPAWN_CLOSURE` 共用该契约；ready Future 和直接 continuation 不单独要求池。
未被调用的已物化函数也纳入契约，但池仍延迟创建，不提前创建 worker。

- 模块激活分配 submission gate；普通 async 与 task.spawn 都经同一个受锁保护的
  `xlir_async_pool_submit`。池的延迟创建、提交与 closing 检查处在同一临界区。
- MODULE_FINI 在任何语言全局 cleanup 前先执行契约规定的 quiesce。它关闭新的
  语言提交，在 gate 外调用 XRT TaskPoolDestroy，并等待 worker **及 frame callback**
  退出；只等 Future ready 或 pool idle 不足以证明回调已退出。
- 自 worker 卸载或重复 quiesce 被拒绝时保留池、gate、模块阶段和全局对象，允许
  宿主之后重试。closing 保持，不能偷偷恢复提交。成功后仍保持 gate 的拒绝状态，
  直到全局清理完成才销毁 gate、回到未初始化；抛错析构也必须走该终态。
- 资源清理与 language-global owner 数分开。即使没有 managed global，拥有池的
  finalizer 也具有 MAY_RAISE 边界。Typed IR 不允许预填资源；缺失、未知或与
  lowered operations 不符的资源 ABI、错误的 finalizer error ABI 在 C/header 前拒绝。
  JSON dump 明确记录 `xrt-guarded-quiesce-before-globals`。
- 原 owned-activation fixture 不再直接 Close/Wait/Destroy 或重置 once；改为真正
  的模块 fini/init。最终同源码专项保留 52 项非法 IR、1,100 次真实分配失败与
  300 次真实挂起/恢复，43,550 次计量分配平衡；严格 C 与 JIT/AOT smoke 通过。

新 `tools/check_xlir_module_task_pool.ps1` 编译当前源码 contract，覆盖 9 个 ABI
分区、36 项非法 IR；C/header/JSON 重复生成一致。运行 lane 中 30 个计量循环：

- 210 次 worker / callback 仍活动时的模块卸载，包括普通 async、task.spawn 与
  XRT frame-finalizer callback；语言全局在它们退出前仍存活。
- 60 次 self-worker 卸载拒绝与随后外部线程重试；300 次 closing 期间来自 worker
  或全局析构的提交拒绝。105 次全局析构抛错仍完成资源释放，保留原错误码。
- 6,645 次分配平衡，15 次真实 gate / frame / pool / submit 分配故障，最终内存
  基线恢复；无新增 invalid/double free 或 UAF。严格 C、JIT/AOT 均通过，JIT/AOT
  smoke 不做 allocator 注入。**15 次不包含下面尚未修复的 class 构造故障。**

定向回归还发现混合 runtime 声明头把 xmutex 定义为 union；新资源 helper 的 opaque
声明现仅在没有 XLIR_XRT_DECLARATIONS 时发射，避免重定义。v3 扫描因此明确停止，
v4 由新的 source driver 重新运行，不能把 v3 的已扫描项累加成 v4 覆盖。

新增语言修复：未调用的 async 声明此前依赖偶然调用点建立 Future<T> 类型。Typed
导入现在在源类型之后，依据 async 声明显式加入 invocation type；void 为 Future<null>，
generator 保持 generator handle。同步修复 class/protocol 的 void invocation 查询。
无调用的 void worker、int ready worker、virtual void worker 均有独立 contract，
不依靠添加伪调用或保留死代码来通过。

### 本轮仍未闭环的语言/API 问题

1. **全局 class 构造分配失败。**真实注入后，`xrtValueObjectLifo()` 返回 NULL，
   下一条仍调用 `xlir_xvalue_type_bind` 并 abort；后续字段初始化也还有 TRAP。
   当前 IR 缺少覆盖 allocation/type/identity/field-init 的完整构造事务及失败边。
   `build/test/xlir_module_task_pool.strict.exe --global-construction-fault` 可复现，
   v4 native exit 为 3；GDB 证据在
   `build/tmp/xlir_module_pool_v3_fault_break.log`。该入口保留为待修复探针，
   不纳入默认池专项的通过计数，不能声称全局部分构造已验收。
2. **DLL/API 生命周期协调。**生成模块契约要求嵌入层串行协调外部 entry 与 unload。
   当前 `xlDestroyModuleUnlocked` 仍可能在 API 锁内执行 unit，并在 quiesce 失败
   后释放 TCC；activeCalls 也未覆盖后台任务、保留的 Future payload 或 callback。
   必须补齐 module lease / deferred destruction 和终态判定，不能仅靠生成 C 中
   的 Destroy 来声称 DLL 可安全卸载。当前专门测试的是生成模块边界，不是该 API。
3. 更多 consuming capture、SSA owner、部分构造/转交、native borrow/layout、
   Future owning payload 与完整终态仍是长期任务的必做项。

最终源码扫描和其它回归的完成记录见本节后续核对；发布 CLI/DLL 未重建，正式
admission 未重跑，legacy 未物理移除。长期目标保持 active。

本轮其它验证已核对：nullable/index/field/runtime/virtual/dynamic 六组 87 项，以及
协议/async/generator/native/async-void 的 28 项，均由 v4 source driver 重生 C、
严格编译并运行通过；组间有重叠，不相加作为独立覆盖率。virtual-activation 仍为
29,500 次平衡分配、900 次真实分配失败和 900 次真实挂起；parameter-cleanup 仍为
433 项非法 IR、6,600 个严格场景（含 1,100 次真实诊断分配失败）、209,000 次平衡
分配，JIT/AOT smoke 各 5,500 个场景。参数专项新增 SourceCompiler 选项，本次
明确使用 v4 source CLI；runtime DLL 哈希相同则不覆盖正在使用的文件。

初始化阶段也补测了“构造回调已经提交任务、随后报告错误”的回滚：模块 phase 1
执行 pool quiesce 并释放 gate，保持原始错误码 73，没有把失败初始化伪装成成功。
这与 allocator 故障导致 unchecked class type-bind abort 是两条不同路径。

静态门禁通过：129 semantic rows / 374 representatives / 451 deterministic /
144 differentials / 83 XLIR-only；source 288、49 direct/238 included/6 embedded/
2 generated；batch 11/67，Core C fixture 545/11。这些是接线数字，不是全量运行。

v4 的 494 项源码扫描已完成：494/494、失败 0，同一会话核对进程退出码 0。
仅证明该快照的 IR 验证与 C 生成；不是全量执行或发布准入。
完成记录和源码/产物哈希见 `build/tmp/xlir_module_pool_v4_source_snapshot.json`。

## 2026-09-08：class 构造失败边与原生句柄接管（construction-v7，未完成验收）

本轮是语言正确性推进，不是标准库扩容或发布。module-pool-v4 保留的全局 class
分配失败 abort 已修复，但扩展后的专项仍有真实失败，不能把本节当作构造事务或
长期目标的最终验收。

- CLASS_NEW 先以明确的 NULL-result ABI 检查失败，再对新建对象执行不分配内存的
  type/identity binding。共享 BeginClassConstruction 随后将对象放入一个受异常
  求值栈保护的临时 storage；字段初始化、显式 create 和基类 create 共享该 owner。
  CommitClassConstruction 成功后才移出对象并绑定用户 finalizer。部分对象的失败
  清理释放已初始化字段，不调用尚未提交的对象自身用户析构。
- 新 CLASS_FIELD_INIT_NEW 与普通对象写入区分，明确借用 pending class、在成功
  和失败两侧均消费 boxed field、以 false 报告失败。verifier 强制 CLASS_NEW 和
  CLASS_FIELD_INIT_NEW 紧接具有不同成功/失败目标的 ERROR_TEST。同步、async 和
  generator 通过已有共享 outcome emitter 消费 IR 的 NULL / false 物理结果，
  不是由 Cgen 查 AST 决定错误语义。
- 默认 class 初始化现在包含分配错误 effect；普通托管字段写入也不再一律 TRAP。
  写入结果在释放参数/地址临时所有者前观察。显式 base(...) 和继承默认 create
  接入共享 CallCleanup / CompleteCall，接受基类 create 的 MAY_RAISE carrier。
- native factory 的拥有型 raw handle 先 TAKE 成 xvalue，再保护这个 box 并分配
  外层 class。包装失败同时回滚句柄与已提交字段；named raw source 在接管边界
  清空，而不是在外层构造成功后才清空。factory 参数临时 owner 保留到包装完成。
- 资源默认字段不再额外复制一个 pending object owner；复用构造求值栈上的 owner
  和保留原错误的 unwind。空容器与默认空字符串先检查分配，再绑定类型/装箱。

### 已核对的 v7 结果

当前源码 driver 为 build/test/xlir_class_construction_v7_driver.exe。
87 项 nullable/index/field/runtime/virtual/dynamic 回归全部通过，另 28 项
activation/protocol/native/generator 回归通过；组间有重叠，不相加作为覆盖率。
显式与重载继承 base create 的两个生成 C 用例另通过严格编译、链接和运行。
v6 回归曾发现 base receiver 临时 owner 在分配前被登记，本轮已修正并由 v7 重跑，
不能引用 v6 的失败记录冒充当前通过。

扩展后的 module-task-pool 源码 contract 通过 9 个资源分区及 108 项非法 IR
拒绝，包含移除构造 ERROR_TEST 和合并成功/失败目标；C/header/JSON 重复输出一致。
严格 C 编译通过；当前完整 gate 的运行阶段仍失败，情况见下节。

- 全局构造从原来的单点故障扩大为完整分配预算扫描：9 次实际分配失败，46 次
  计量分配平衡，失败后的模块/gate/全局状态均可重试。
- --construction-variant 1：嵌套 child 构造、字段初始化和 create body 写入，
  18 次实际分配失败、181 次平衡分配。
- variant 2：继承默认 create，11 次分配失败、76 次平衡分配。
- variant 3：native factory/raw handle/外层 class 接管，8 次分配失败、
  46 次平衡分配。
- variant 4：含拥有型 native 字段的默认构造，17 次分配失败、
  163 次平衡分配。上述局部专项检查 child create/drop 对称、native handle
  归零及最终内存基线；每个独立进程仍执行既有池卸载回归。
- 当前生成 C 的 JIT/AOT pool smoke 单独执行通过；不注入 allocator，也不声称
  覆盖上述局部构造五分区。发布 CLI 仅充当 C-host 编译器，不作为新语言源码编译器。

七项静态门禁均有通过记录。Stack 静态 guard 原来只匹配已删除的手工回滚注释，
现在改为检查共享 Begin/CommitClassConstruction 和 CLASS_FIELD_INIT_NEW 标识。
接线统计仍为 129 rows / 374 representatives / 451 deterministic / 144 differentials /
83 XLIR-only；这不是全量执行证明。

### 当前保留的真实失败和下一步

默认完整 module-task-pool gate 仍退出 1；显式
build/test/xlir_module_task_pool.strict.exe --construction-variant 0 原生退出 3。
GDB 在 v6 定位到：默认字段对象已完成构造，随后读取 string 字段时，
xlir_xvalue_object_get_literal_string_copy 内的 xrtStrDupN 分配失败仍直接 abort。
故障预算 index 为 16。v7 保留此失败，没有删除字符串读取断言、缩短扫描范围、
跳过该分区或将其纳入通过计数。下一步须把拥有型读取/复制的分配错误纳入 Typed
effect、Lowered outcome 和临时 receiver 回滚，不能只删除 helper 中的 abort，
否则其它 dict/enum 等共享 helper 的调用者可能把 NULL 当成成功。

本轮没有重跑 494 全量扫描、最终 default admission 或完整 CLI/DLL 构建；
module-pool-v4 的 494/494 仍只是上轮快照证据。当前源码与相关日志记录见
build/tmp/xlir_class_construction_v7_source_snapshot.json。
仍须处理其它 boxing/复制分配、构造 escape 与提交边界、抛错清理组合、SSA owner、
native borrow/layout、Future owning payload，以及 API code lease/延迟卸载。
legacy 未移除，发布 CLI/DLL 未更新，长期目标保持 active。

v7 的 owning-activation 与 virtual-activation 门禁亦由当前源码重跑通过。
前者保留 52 项非法 IR、1,100 次实际分配失败、300 次实际挂起和 43,550 次
平衡分配；后者保留 27 项非法 IR、6 个前端负例、900 次启动分配失败、
900 次实际挂起和 29,500 次平衡分配。两者均通过严格 C 与 JIT/AOT smoke；
这些专项结果不覆盖本节保留的 string 字段复制分配失败。

## 2026-09-08：拥有型复制投影与 match 绑定清理（projection-outcome-v5）

本轮是语言所有权与失败边收口，不是标准库扩容或发布。construction-v7 保留的
完整 module-task-pool gate 已恢复通过，未删除 string 读取断言、缩小故障预算或
跳过构造分区。class/dict literal string copy 与 enum string payload 具有独立的
Typed projectionErrorAbi；Lowered 必须立即检查 NULL-result，并保留临时 receiver
及此前已完成 payload 的回滚责任。Cgen 不追溯 AST，也不从表达式用途补猜语义。

过程中新增两个真实修复：`.length` 使用完拥有型字符串后释放其 owner；语句型
match 的 payload 绑定成为 case block 的隐式词法声明，由正常/异常 scope route
负责清理。复制尚未全部完成时仍使用求值事务，之后才提交给词法 scope。return、
break/continue、finally、内外层 catch 与抛错 class payload 析构均进入运行测试。

`tools/check_xlir_owned_projection.ps1` 已接入 core-function-ABI suite 和静态 contract。
它从当前源码编译 compiler contract，不使用发布 xlc.dll 代替新编译器：

- 15 个 Typed effects、24 个 checked copy 操作、56 个词法绑定清理动作；152 项
  非法 IR 在 C/header 输出前被拒绝，包括删除失败检查、合并目标、错误 enum 错误
  来源，以及以同类型的另一个 payload 冒充待清理 owner。
- C/header/JSON 重复生成一致。16 个运行分区重复 50 轮，1,400 次真实分配失败、
  27,901 次计量分配平衡；每个预算检查内存基线、class drop 数和错误结果，无新增
  invalid/double free 或 UAF。JIT/AOT smoke 不注入 allocator，日志明确区分。
- 覆盖命名/临时 class 与 dict、单次与中途 enum payload 复制失败、命名字符串借用、
  match 表达式结果、语句 case 正常退出/提前返回/循环退出/finally/内外层 catch，
  以及临时 subject 唯一拥有 class payload 时的正常和抛错析构。

v4 的 494 项源码扫描保留 6 个失败，均为持久化 parse 在构造目标失败时遗失输入
文本 temporary 的 CFG owner。v5 已将文本保护提前到构造的第一条可失败操作之前，
并用同一求值 owner 栈统一释放输入与目标。8 项持久化定向回归由 v5 source driver
重新生成、严格编译并运行通过。回归脚本第一次使用了不存在的 fixture 名称，修正为
gate 实际引用的 cgen_object_var_field_lifetime 后整组重跑；未将脚本错误算作语言失败。

v5 还重新通过既有 87 项 nullable/index/field/runtime/virtual/dynamic、另 28 项
activation/protocol/native/generator 和 7 项 enum 回归。组间有重叠，不累计为独立
覆盖率。scope cleanup、return commit、parameter cleanup 的当前源码 mutation
contracts 分别保留 736、14、433 个拒绝项。

module-task-pool 完整 gate 通过：全局构造 9 次分配故障、46 次平衡分配；局部
构造分区 0 至 4 分别为 17/18/11/8/17 次故障和 163/181/76/46/163 次平衡分配。
既有 210 次 worker/callback 卸载、60 次 self-unload 拒绝、300 次 closing 提交
拒绝、6,645 次平衡分配及 15 次池相关分配故障仍通过，JIT/AOT smoke 通过。
owning/virtual activation 也按 v5 源码重跑通过，保留 1,100/900 次分配故障、
300/900 次真实挂起及 43,550/29,500 次平衡分配。

七项静态门禁通过：129 semantic rows、374 representatives、452 deterministic
candidates、144 differentials、83 XLIR-only；source 288，49 direct/238 included/
6 embedded/2 generated；batch 11/67；Core C fixture 548/11。这些是接线统计，
不是全量运行证明。v5 的 494 项源码扫描结果在本节后续核对，不能复用 v4 的结果。

仍未完成：其它 copying/boxing/nullable/iterator 分配失败、表达式型 match 的全部
抛错清理组合、构造 escape/提交边界、SSA owner、capture/alias/cell/取消组合、
native borrow/layout、Future owning payload，以及 API code lease/延迟卸载。
本轮未重建发布 CLI/DLL，未跑最终 default admission，legacy 未移除，长期目标保持 active。

### 新保留的临时 receiver 抛错清理探针

`build/tmp/xlir_projection_receiver_cleanup_probe.xl` 使用 `makeText().text.length`：
临时 class receiver 的唯一 owner 在读取后释放，其用户析构通过预分配错误对象抛出
code 77。紧随读取的宿主观察函数不应执行。v5 严格 C 探针实际输出
`result=0 dropped=1 after=1 error=77`，原生退出 3，预期 after=0。
说明外层 export 虽报告失败，内部控制流却没有在抛错清理处停止。

该探针未计入上面的 16 分区通过数，也没有修改为接受后续副作用。下一步必须将
非 consuming 借用完成时的清理效果和待提交结果纳入共享 evaluation transaction，
保护投影结果，在 cleanup failure 时回滚并走 IR 错误边；不能依赖 C-host 边界兜底，
或只针对 `.length`/某个 class 在 Cgen 补分支。源、生成 C 与失败日志均保留在
build/tmp/xlir_projection_receiver_cleanup_*。

v5 的 494 项源码扫描现已完成：494/494、失败 0，同一运行会话核对退出码 0。
该扫描仅证明 IR 验证及 C 生成，不能覆盖上述 receiver 析构副作用探针或代替最终
CLI/DLL 运行准入。当前源码树 355 文件 SHA-256 为
0C6078E3C6FD604AFFF3FC69B19B8622119C0464BF3D7363BEB99B50D97A64AA。
完整检查点记录为 `build/tmp/xlir_projection_outcome_v5_source_snapshot.json`。

## 2026-09-08：求值清理事务、进入循环前失败与任务参数保护（borrow-cleanup-v4）

本检查点继续语言语义收口，未重建发布 CLI/DLL，未移除 legacy，也不是最终准入。
projection-outcome-v5 保留的临时 receiver 析构探针已修复：现在
`result=0 dropped=1 after=0 error=77`，失败后的宿主副作用不再执行。普通调用和
非 consuming 求值共用结果保护、操作数清理、成功提交和异常回滚，不新增 Cgen 特例。

- Typed 新增 evaluationErrorAbi，独立记录操作数清理可能抛错；verifier 重新推导
  该效果，并要求相应 throw exit plan。Lowered evaluationScope/evaluationResult
  标识事务及精确待提交 storage，强制检查成功/失败分支、对应 unwind 和结果的
  STORE/LOAD/MOVE/RELEASE。删除元数据、替换同函数同类型 owner、交换/合并结果
  分支、删去退出计划都必须在 C/header 输出前拒绝。
- 更强的验证器暴露了 for iterable 的阶段错误：进入循环之前求值 iterable，却
  带有尚未建立的 iterator/item 清理步骤。已在 Typed exit route 中按父子角色
  修正，未放宽 verifier 接受缺少 owner 的 unwind。
- borrow-cleanup-v3 的完整扫描为 491/494、3 个真实失败，分别是
  release/examples/task_api.xl、test/cases/cgen_task_spawn.xl 和
  test/cases/cgen_task_spawn_range_contains.xl。根因是 task.spawn 的闭包临时量
  未覆盖后续参数的失败边；可变参数数组也缺少构造期间的 owner 保护。v3 源码哈希
  为 1AC0B5BDF7E0239CDAEF8B8A74D5D6357E9CA67A9A9F525B6DC7B002F73183C4，
  失败日志保留，不以 v4 结果覆盖。v4 已将这些 owner 及 Future continuation、
  数组 all/race/whenAny 的完成清理接入共享事务；最小复现恢复通过。

新增 tools/check_xlir_evaluation_cleanup.ps1 已接入 core-function-ABI suite 和
静态 contract，并从当前源码构建 compiler contract。两组 C/header/JSON 重复
生成一致，均通过严格 C、JIT/AOT C-host 执行；发布 CLI 只充当 C-host 编译器。

- 同步清理组：26 个 Typed effects、23 个事务、14 个 pending results、1 个
  pre-loop route，363 项非法 IR，包含 56 次同函数同类型 owner 替换。10 类场景
  各正常/抛错两种模式重复 50 轮，1,000 次执行、10,002 次平衡分配。覆盖临时
  receiver、拥有型结果、catch、原始错误 88 与析构错误 77 的优先级、构造失败、
  finally 的标量/拥有型 pending return，以及 iterable 求值失败。
- 任务参数组：23 个 Typed effects、15 个事务、7 个 pending results，229 项
  非法 IR，包含 28 次同函数同类型 owner 替换。50 次模块激活/退出、2,201 次
  平衡分配；执行含动态条件和二元表达式的 varargs task.spawn，并验证后续参数
  析构抛错时，已建立的闭包/数组/元素被回收且后续源程序副作用不执行。

v4 的 87 项 nullable/index/field/runtime/virtual/dynamic、另 28 项
activation/protocol/native/generator、7 项 enum、8 项 persistence 已由同一
source driver 重跑通过。组间有重叠，不相加作为独立覆盖率。完整 494 扫描及
其余资源专项的最终结果另核对，不能借用 v3 或 projection-outcome-v5 的扫描。

仍需完成：其它 copying/boxing/iterator 分配错误、表达式型 match 全部抛错清理、
构造 escape、SSA owner 与捕获/别名/cell/取消组合、native borrow/layout、Future
拥有型 payload 全生命周期，以及 DLL/API code lease 和延迟卸载。标准库按最低
必要范围维护。源码检查与发布二进制验收分开，长期目标保持 active。

v4 的 owned-projection、module-task-pool、owned-activation、virtual-activation
四个完整源码门禁已通过，另 7 项 task/continuation 定向用例严格编译运行通过。
七项核心静态检查通过：129 rows / 374 representatives / 454 deterministic
candidates / 144 differentials / 83 XLIR-only；source 288（49 direct、238 included、
6 embedded、2 generated）；batch 11/67；Core C fixtures 553/11；encoding 5932；
markdown 329。接线统计不等同于全部测试已执行。

仓库级 C fixture 检查另保留 7 个 MQTT/serial 包测试未登记的失败，见
build/tmp/xlir_borrow_cleanup_v3_c_fixture_wiring.log；CoreOnly 通过不覆盖这些文件。

### 当前保留的表达式型 match 抛错清理探针

build/tmp/xlir_evaluation_match_cleanup_probe.xl 从临时 enum subject 投影两个
class payload，在 match 表达式求值后释放它们。两个析构均设置预分配错误 77。
v4 严格 C 实际为 `result=0 dropped=2 after=1 error=77`，原生退出 3；预期 after=0。
这说明表达式型 case 仍有手动 RELEASE 后继续进入 merge 的路径，不属于上述
专项已修复范围。生成 C、host、运行探针和失败日志全部保留，未放宽断言。
下一步须保护待提交 match 结果并将 payload/subject 清理纳入共享事务；同时验证
case/else、部分投影失败、原始异常和唯一拥有的 subject 等组合。不能只在 merge
或 Cgen 增加错误状态检查。

v4 源码树 355 文件 SHA-256 为
4FAF8567D7A90BF74E2576A8CF4320D8E7D84C8B8DEDD6833E8AC33DA5D3EE8B。
已完成专项和保留失败的文件记录见
build/tmp/xlir_borrow_cleanup_v4_source_snapshot.json；仍在运行的完整扫描日志不作为
冻结文件纳入该快照，须在同一运行会话结束后另核对最终结果。

## 2026-09-08：match 清理、跨挂起 binding 与不可变 join（match-cleanup-v8）

前述 borrow-cleanup-v4 的 494 项扫描已完成：494/494、失败 0、退出 0，快照已
补录完成状态。match-cleanup-v7 的扫描也已核对同一会话终态：494/494、失败 0、
退出 0。两者都只证明各自源码 driver 的 IR 验证及 C 生成，不是最终 CLI/DLL 准入。

### 本次语言修复

- 表达式型 match 的 payload/subject 使用共享求值清理事务，待提交的标量、字符串、
  对象和闭包结果在清理成功后才进入 merge；旧 dropped=2/after=1/error=77 的错误
  路径已修复为 after=0。catch、原始异常优先级、finally、嵌套 match 和字段初始化
  均有运行断言，没有在 Cgen 增加 match 特判。
- pattern binding 的捕获从 canonical EnumLayout 获取类型；readonly class capture
  与已有 var mutable cell capture 得到正确所有权。验证器检查绑定来源、存储种类、
  投影初始化及物理 CELL_RELEASE。class-typed mutable capture 的前端限制仍保留。
- 真实 await 测试在 v7 暴露标量分支的可变 targetValue：生成 C 引用了未声明 v55/
  v176。v8 将 match、短路、nullable coalesce 改为 typed storage join，VALUE 不再
  改写已有 SSA；验证器明确拒绝旧编码。这是共享 IR 表示修复，不是后端补局部声明。

### 已完成验证

新增 tools/check_xlir_match_cleanup.ps1 已接入核心 03_core_function_abi 与静态合同。
它从当前编译器源码构建 contract；release/xl.dll 仅提供既有运行时依赖。

- 同步：48 个 effects、41 个清理事务、17 个待提交结果、24 个求值绑定、1 个
  promoted cell；665 项非法 IR 拒绝，含 68 次同函数同类型 owner 替换。
  12 类场景正常/析构抛错各 50 轮，1,200 次执行、32,752 次平衡分配。
- async：42 个 effects、28 个事务、10 个待提交结果、14 个显式 frame binding；
  516 项非法 IR 拒绝，包括 frame storage identity、destroy ABI、初始化位及 residence
  变异。8 类场景、800 次真实挂起/恢复，24,009 次平衡分配；覆盖正常输入和拒绝输入，
  分别叠加正常/抛错析构。不是 sleep 模拟，也不把 ready Future 计入挂起数。
- 两组 C/header/IR 和 async 测试符号绑定重复生成一致；严格 C、生成 C-host JIT/AOT
  均通过。Future 尚不能直接作为 C ABI export 参数/返回值，测试通过 IR 符号绑定调用
  内部 async 函数，另有合法 export void 入口保留真实语言调用点。
- 10 组同版 source-driver 回归通过：nullable/capture 17、index 12、field 18、
  runtime/contract 14、virtual 16、dynamic 10、activation/protocol/native/generator 28、
  enum 7、persistence 8、task 7。组间可能重叠，不相加为语言完成比例。
- evaluation-cleanup、owned-projection、owned-activation、virtual-activation、module-task-pool
  五个完整源码门禁重跑通过，保留严格 C、运行时、分配故障及 IR 变异断言。
- 核心静态门禁通过：129 rows / 374 representatives / 456 deterministic candidates /
  144 differentials / 83 XLIR-only；source 288；batch 11/67；Core C fixtures 558/11；
  encoding 5940；markdown 329。静态接线统计不是所有测试均已运行。此前仓库级 MQTT/
  serial 的 7 项 C fixture 接线失败仍单列，CoreOnly 没有覆盖它们。

### 保留的真实失败及下一步

1. build/tmp/xlir_match_cleanup_v8_cancel_probe_runtime.c 在 await 输入取消且对象析构
   抛错时，Future 为 CANCELLED、对象释放 2 次，但 frame teardown 后线程错误槽残留
   code 77，退出 3。该失败未计入上述 async 800 次通过数。
2. build/tmp/xlir_match_cleanup_v8_cancel_finally_min.xl 是更小的通用 continuation 探针。
   正常完成 finally=1；取消和关闭均 finally=0，独立严格 C 运行均退出 3。
   对应日志为 *_cancel_finally_min.log 与 *_close_finally_min.log。不能通过清空 TLS 错误
   或修改 finally 预期来验收，须建立显式 terminal exit/cleanup IR。
3. 更大的全分支 return + try/finally 组合探针 *_cancel_finally.xl 另因 reachable
   unterminated block b20 被 lowering 拒绝，待定位；它不是上面最小探针的运行结果。

本版 494 项扫描仍由已冻结 build/test/xlir_match_cleanup_v8_driver.exe 运行，记录到
build/tmp/xlir_match_cleanup_v8_audit*；未完成前不宣称通过。随后只校正 targetValue
头注释并重建 v8_final_driver，生成 async C 与门禁产物字节一致。当前源码、专项日志、
保留失败与发布产物哈希记录见 build/tmp/xlir_match_cleanup_v8_source_snapshot.json。

发布 CLI/DLL 未重建，legacy 未删除，长期目标保持 active。除上述取消/关闭和组合
控制流外，其它 copying/boxing/iterator/cell 分配错误、构造 escape、SSA owner 与
alias/capture 组合、native borrow/layout、Future owning payload 和 DLL/API code lease/
延迟卸载仍须完成；标准库继续按最低必要基线处理。

## 2026-09-08：Future 四态恢复与 finally 组合（terminal-outcome-v3）

本节是 match-cleanup-v8 之后的新源码检查点，不借用 v8 的全量扫描作为本版结果。
v8 的原运行现已结束：494/494、失败 0，见
build/tmp/xlir_match_cleanup_v8_audit.log 和对应 source_audit.json。v8 快照中的
running 元数据仅代表冻结时状态。

### 表示与语义

AWAIT 指令和 async machine 显式持有 futureOutcomeAbi。每个挂起点拥有 success、
failure、cancelled、closed 四个规范入口，全部恢复到 AWAIT 后第一条指令，由同一
CFG 执行 release、ERROR_TEST、catch、finally 和退出；不再在 C callback 中提前
cancel/close 绕过语言清理。阻塞 await 共用相同的终态观察 ABI，CLOSED 不再被
当作默认零值的成功返回。状态机 THROW 按错误 kind 完成 Promise 的终态。

自动析构清理保留正在传播的原始异常；显式 finally 操作可以覆盖它。finally 内的
第二次 await 抛错、显式调用抛错或 return 都属于后一类。初次组合测试混淆这两种
规则而失败，保留 build/tmp/xlir_terminal_outcome_v2_runtime.log 的原始证据；
修正 oracle 后未改编译器以迁就错误预期。

非 void 函数的终止检查改为从 function entry 计算真实 CFG 可达性，不再把孤立
子图中存在入边的 block 误当作可达。旧 *_v8_cancel_finally.xl 的 b20 失败已由
v3 source driver 成功验证并生成 C；永久 fixture 另包含无尾随 return 的全分支
return + try/finally，并实际运行两个分支和所有输入终态。

### 已完成验证

- tools/check_xlir_async_terminal_outcome.ps1 从当前源码构建 compiler contract，
  接入 03_core_function_abi 与静态合同。2 个 machine、10 个挂起点、11 个 AWAIT
  （含顶层阻塞 await）、40 个恢复入口；349 项非法 IR 均被 verifier 以及 C/header
  emitter 拒绝且不产生输出，包括旧双入口表示、缺失 ABI、错接状态和跳过清理。
- 同一 fixture 严格 C 下 2,200 次执行、3,000 次真实挂起恢复、20,363 次平衡分配；
  typed catch、重抛、finally 二次挂起、覆盖 return、全分支 return、顶层 await
  均有断言。C/header/JSON/符号绑定重复生成一致，生成 C-host JIT/AOT 通过。
- match 的永久 async runtime 扩展为四态输入：v3 source contract 下 1,600 次真实
  挂起、47,209 次平衡分配；取消/关闭叠加抛错析构仍执行所有清理，线程错误槽为空。
  同步 1,200 次、32,752 次平衡分配仍通过；665/516 项同步/异步 IR 变异仍拒绝。
  为保留 v8 产物，新生成文件采用 xlir_terminal_outcome_v3_xlir_match_cleanup 前缀。
- 10 组同版 source-driver 回归全部通过：task 7、nullable/capture 17、index 12、
  field 18、runtime/contract 14、virtual 16、dynamic 10、
  activation/protocol/native/generator 28、enum 7、persistence 8。组间可能重叠。
- 既有 cfg-liveness、generic-planner、multi-await 门禁的入口断言已更新为四态；
  这些完整脚本仍依赖发布 CLI，未在本版声称它们已用发布编译器通过。
- 本版 494 项扫描由已冻结 build/test/xlir_terminal_outcome_v3_driver.exe 运行，
  结果另记 build/tmp/xlir_terminal_outcome_v3_audit*，终态前不宣称全量通过。

### 保留缺口

无 await 的线程池 async 仍使用独立 task completion：严格 C 探针
build/tmp/xlir_terminal_outcome_v3_pool_probe_runtime.c 中，成功/普通失败正确，
但 XERR_CANCELLED / XERR_CLOSED 均返回 FAILED，而非 CANCELLED / CLOSED，
实际退出 1、两项不匹配。保留源码、IR、生成 C、host 和日志；它不计入上述通过数。
新 futureOutcomeAbi 目前只承诺 AWAIT/machine，不伪装成全部 CallTarget 的完成契约。
ready/no-await pool 的统一完成 ABI、取消传播与拥有型 payload 仍须一起设计和验收，
不能随手包装一个 Future 而丢失取消传播或 payload owner。XRT 单头为生成快照；
若需运行时扩展，应先改 XRT 主仓并验证，再同步，不手改 lib/xrt/xrt.h。

其它分配错误、构造 escape、owner/alias/cell/capture 组合、native borrow/layout、
Future 全生命周期、DLL/API code lease 与安全延迟卸载仍未完成。发布 CLI/DLL 未
重建，legacy 未删除，标准库仍限于最低必要基线；长期目标保持 active。

本版核心静态检查通过：129 rows / 374 representatives / 457 deterministic
candidates / 144 differentials / 83 XLIR-only；source 288；batch 11/67；
Core C fixtures 561/11；encoding 5945；markdown 329。接线统计不是全部运行结果，
CoreOnly 不覆盖此前单列的 MQTT/serial 包测试接线失败。

v3 源码树 355 文件 SHA-256：
F946F78EB99A094946243258287E3631396F85138A3F17117A8E496C43A2A8ED。
完成的门禁、产物、保留失败和发布哈希见
build/tmp/xlir_terminal_outcome_v3_source_snapshot.json；正在写入的全量扫描日志
不列为冻结文件，须继续等候原运行的最终结果。

## 2026-09-08：无挂起 async 完成边界（pool-outcome-v3）

terminal-outcome-v3 的原扫描会话现已结束，494/494、失败 0、退出 0，旧快照已补
完成状态。该结果不代表下面新增的 pool-outcome-v3 通过全量扫描。

### 实现与边界

所有 INTERNAL_ASYNC CallTarget 现显式持有 futureOutcomeAbi；verifier 拒绝缺失、
错误枚举以及同步/generator target 误带该 ABI。赋值在 callable 的最终角色解析
之后进行，async generator 仍返回 generator handle，不错误声明为 Future。

ready launcher 和 continuation machine 使用同一错误 kind -> Promise 四态规则。
无 await 的 worker 保留 XRT 任务池调度，另用预分配 Future Watch 规范化传输完成：
在任务受理前完成 watch、输出 Future/Promise、取消父链的准备；受理后注册 Watch
无需分配。失败前输入仍归 launcher，失败后错误交由 Future 承载，不把已启动任务
伪装成同步提交失败。输出取消令牌是任务令牌的父节点，协作取消能向生产者传递；
PromiseForward 保留源 Future 的拥有型 payload，且公开终态位于原任务 frame
清理屏障之后。READY/PENDING 通知及 Release 均保护宿主已有错误上下文。

使用的 Watch、取消与 Forward API 已存在于当前 XRT；未手改 lib/xrt/xrt.h，也未
引入 XRT 主仓的其它版本变更。不是从 AST 查找特殊函数，也未按 payload 类型复制
一套任务结果转换流程。

### 验证

- tools/check_xlir_async_pool_outcome.ps1 已接入核心 batch 与静态合同，直接构建
  当前源码 compiler contract：3 pool、1 ready、37 non-async targets；45 项非法
  IR 在 C/header 输出前被拒绝。文本/JSON IR、C/header、符号绑定均重复生成一致。
- 540 次运行、60 次阻塞生产者中的反向取消、21 个真实提交前分配故障，13,640 次
  分配平衡；失败提交不启动用户代码。覆盖 scalar/string/class 拥有型结果、结果
  活过池退出、Future 多引用、提前释放消费者、取消与抛错析构。另 24 项 READY Watch
  专门验证终态映射、SDK 存储大小、payload 不复制及回调错误上下文隔离。
- 新门禁严格 C、生成 C-host JIT/AOT 通过。此前终态门禁同版重跑通过：
  349 项 IR 变异、2,200 次运行、3,000 次真实挂起、20,363 次平衡分配。
- 同版 owned-activation、module-task-pool、virtual-activation、owned-projection
  四个完整源码门禁全部通过；保留真实故障注入、严格 C、JIT/AOT 和内存断言。
  virtual-activation 另新增 4 项 generator 误带 Future ABI 的拒绝测试，合计
  31 项 IR 变异；900 次 launch 故障和 900 次真实挂起/退出测试通过。
- 10 组 source-driver 回归均通过：activation/protocol/native/generator 28、
  enum 7、persistence 8、task 7、nullable/capture 17、index 12、field 18、
  runtime/contract 14、virtual 16、dynamic 10。分组有交叠，不能相加当完成率。

### 真实回归与全量扫描状态

pool-outcome-v2 曾在 virtual-activation 和 async-generator 暴露 ABI 过早初始化。
保留 *_v2_virtual_activation_gate.log、*_v2_activation_regression.log 及 v2
全量扫描中的失败记录；不能将它们解释成环境故障或放宽 generator 断言。
该旧扫描因源码已修复而明确停止（退出 -1，非通过），未删除其日志和产物。
v3 的原始失败源码已成功验证并生成 C，且上述完整门禁和 28 项回归已通过。

最新完整扫描使用冻结 build/test/xlir_pool_outcome_v3_driver.exe，结果记录在
build/tmp/xlir_pool_outcome_v3_audit*，终态前不计为通过。源码树 355 文件 SHA-256：
174F9E9D95E8227E45FC9E44396CFA26F67F6311791C4B75175056657D72EBF2。
完成的产物、门禁日志及发布哈希见随后补验的 build/tmp/xlir_pool_outcome_v4_source_snapshot.json；
运行中的 audit 日志不作为冻结记录。

### 仍未完成

本节明确覆盖内部 async 的 ready/pool/machine 完成，不泛化为所有异步 API。
task.spawn 仍有独立的 AsyncError（保留 cause）包装与 raw pool 传输；用户 continuation 的错误、
取消和 callback 上下文边界也须独立审计后建立契约，不能机械改写其既有语义。
其它分配错误、构造 escape、owner/alias/cell/capture 组合、native borrow/layout、
完整 Future payload 生命周期，以及 DLL/API code lease / 延迟卸载仍待完成。
发布 CLI/DLL 未重建，legacy 未删除；标准库继续只做最低必要范围，长期目标 active。

### pool-outcome-v4：受理后 Watch 注册失败的取消顺序

复核 XRT 实现确认：xrtFutureCancel 只对 PENDING Future 请求取消。因此，受理后
注册失败不能先发布 FAILED 再请求停止生产。v4 将此路径作为
xlir_task_outcome_registration_failed：先取走并保护注册错误，再请求取消，最后
发布该错误并释放转换状态。取消回调不能替换原始注册错误。

新增一个直接调用该失败处理过程的负向生命周期测试：输出 Future 的取消令牌
作为模拟生产者的父节点，验证子令牌确实收到取消，同时输出仍保留原始 FAILED
错误。它是处理过程的专项，不声称强制真实 XRT WatchAdd 发生了 OS 锁错误。
连同已有 24 项 READY Watch 测试，合计 25 项边界专项；v4 新门禁严格 C、JIT/AOT、
45 项非法 IR、C/header/文本/JSON/绑定重复生成检查均通过。540 次运行、60 次反向
取消、21 个提交前故障、13,645 次平衡分配。

上述五个原有完整门禁和十组回归是 v3 的同版结果；v4 目前完成这项新增注册失败
修复及新门禁复验，不冒充已经重跑其它全部专项。仍在运行的 pool-outcome-v3
全量扫描也只属于其冻结编译器。最新源快照单列 v4 源码与 v3 既有验证证据，下一轮
需继续补 v4 的同版回归/扫描；发布产物未改动。

## 2026-09-08：回调激活、捕获清理与发布屏障（callback-boundary-v4）

本节承接 pool-outcome-v4。pool-outcome-v3 的原始扫描已结束：494/494、
失败 0、进程退出 0；不代表下面的新回调实现已完成同版全量扫描。

### 统一表示与实际修复

TASK_SPAWN_CLOSURE 和 FUTURE_CONTINUE_CLOSURE 现在引用一基 callbackBoundary。
模块持有独立 CallbackBoundary / CallbackCapture arena：每个捕获记录精确 operand、
SSA ValueId、TypeId、角色以及 transfer/destroy ABI。lowering 建立规范计划，
verifier 独立重算捕获顺序、身份、ABI 逆操作及完整覆盖，拒绝缺失、串接、孤立记录
和错误策略。任务与 continuation 共用捕获获取、逆序清理和失败回滚发射器，
Cgen 不从参数 C 类型重新选择保留/释放操作。

边界明确指定 transport、cleanup、error、forward ABI。回调先暂存结果，再脱离
捕获字段执行清理，最后发布：自动清理保留原始错误或既有失败/取消/关闭终态；
成功路径的清理异常使待发布结果失败；显式 finally 回调抛错可以覆盖输入错误。
task.spawn 保留既有 AsyncError-with-cause 策略，不改成同名 TaskError，也不吞掉
原始 cause。此前 pool 节的 TaskError 表述已纠正。

最小探针曾显示：捕获对象析构抛出 77，Future 却成功且 TLS 留下 77。v1 已修复
执行回调路径。随后 v2 跨线程探针进一步发现：消费者先请求取消时，旧
xrtFutureContinueOwned 可以绕过语言回调，先发布取消再调用 frame destructor。
即使通知观察者看到 drops=1，也不能证明其它线程无法提前读到终态。

v3 因此使用明确的 FUTURE_WATCH transport：在注册前准备整个激活、输出 Promise
与取消令牌，READY/PENDING 共用语言处理过程，取消也必须经过捕获清理后发布。
注册失败仍是未受理边界，回滚全部已初始化 owner；未引入运行时 API，也未手改
XRT 生成单头。透传使用 PromiseForward 保留源 Future owner，删除逐类型复制源
payload 的分支。v4 另补独立取消类型/函数声明，纯 continuation 模块不依赖任务池
顺带提供声明，并将这一最小模式接入永久门禁。

### 已完成的证据及版本范围

- tools/check_xlir_callback_boundary.ps1 接入核心 batch 与静态合同，直接构建当前
  源码：主 fixture 5 个激活、11 个捕获，156 项畸形 IR 在 C/header 输出前被拒绝；
  Future-only fixture 1 个激活、2 个捕获，32 项拒绝。后者独立严格 C 编译和原始
  析构抛错运行均通过，不靠其它 task/async 模块补声明。
- v4 主门禁通过严格 C、生成 C-host JIT/AOT、C/header/JSON/文本/符号绑定重复生成。
  runtime 共 1,200 次执行、6 次回调前取消、51 个实际分配故障、22,053 次平衡分配。
  覆盖捕获析构、输入错误与调用错误优先级、拥有型返回值失败回滚、Future 多引用、
  源 payload 指针身份、宿主错误上下文恢复及任务清理。另两个跨线程屏障测试在
  析构人为阻塞期间检查 Future 仍为 PENDING，正常和取消路径均通过。
- 原始 v2 首个 observer 断言错误地要求原生 XRT Watch 总看到空 TLS；实际调度
  可在语言回调返回并恢复宿主 sentinel 后执行该 Watch。修正为只允许原宿主上下文
  或空上下文，仍拒绝调用/清理错误泄漏。真正的跨线程提前发布失败另保存在
  build/tmp/xlir_callback_boundary_v2_barrier.log，未通过修改屏障断言来验收。
- v3 同版十组 source-driver 回归全部通过：task 7、activation/protocol/native/
  generator 28、nullable/capture 17、index 12、field 18、runtime/contract 14、
  virtual 16、dynamic 10、enum 7、persistence 8。组间存在交叠，不能相加当完成率。
- v3 同版 terminal、owned-activation、module-task-pool、virtual-activation、
  owned-projection、pool-outcome 六个完整源码门禁通过。v4 的源码差异为独立声明；
  本节不把 v3 的既有专项、回归或扫描冒充 v4 的全部同版验收。
- 新 494 项扫描仍由冻结 build/test/xlir_callback_boundary_v3_driver.exe 运行，
  会话 28229，记录 build/tmp/xlir_callback_boundary_v3_audit*；终态前不计通过。
  这仍只是 IR/C 生成扫描，并非 494 项严格 C 和运行验收。

### 保留的语言缺口与后续工作

试图增加名义类对象作为 task.spawn 参数时，前端仍以旧 XRT xvalue call bridge
限制拒绝它；即使先写入 var 也被拒绝。实际失败源码和诊断保存在
build/tmp/xlir_callback_task_nominal_argument_probe.xl 及对应 .log。
该失败没有改为预期拒绝后计作完成；后续须按准确类型/参数 ABI 重新审计语义接纳，
不能让旧动态桥的限制替代语言规则。本次已验证类对象通过闭包捕获和 continuation
拥有型返回，但不声称直接名义参数/捕获别名组合已经全部完成。

更广泛的分配错误、构造 escape、owner/alias/cell、native borrow/layout、
Future 最后一个 owning payload 的析构边界、挂起回调与 DLL/API code lease /
延迟卸载仍待收口。旧 release CLI 依赖的 continuation 门禁断言已更新为新合同，
未重建发布编译器，故不声称其完整发布路径已运行。标准库仍只做最低必要基线，
legacy 未清退，长期目标保持 active。

## 2026-09-08：task 参数类型契约与 XRT 非末引用释放缺陷（task-types-v5，未验收）

callback-boundary-v3 的原始 494 项扫描已正常结束，494/494、失败 0、退出 0；
旧 v4 快照只补该扫描完成状态，不把结果移算给最新源码。

### 编译器与永久门禁

前端移除旧 task.spawn xvalue bridge 参数/结果白名单，改为普通语言类型兼容
检查；task 结果与普通函数共用 xl6SemTypeCanBeFunctionResult。channel/sem/thread
仍遵守不可复制返回值约束，byref/out 不允许捕获调用者存储。generator 和拥有型
BYVAL 等未建立完整任务 ABI 的组合仍需开发，不能把拒绝算成实现完成。

IR 公开共享 xl6IrTaskSpawnParameterAbiSupported / xl6IrTaskSpawnArgumentCanBind，
lowering、verifier 和 Cgen 共用准确 ABI。相同 xvalue* 表示不意味着语言类型相容：
仅精确类型、兼容 callable、显式 var boxing、同类 erased collection 或已验证的
class upcast 可绑定。同步资源原生前置声明修正为 SDK 的 union，使用 SDK 声明时
不重复声明；编译器拥有的 sync handle/枚举不藏在该 guard 内。未修改 XRT 快照。

新增 xlir_task_value_types 源码/contract/runtime/host 和
tools/check_xlir_task_value_types.ps1，接入核心 batch 与静态合同。当前源码契约
16 个激活、31 个捕获、459 项非法 IR 拒绝通过，C/header/JSON 重复生成一致。
真实任务覆盖类参数/结果与别名、var boxing、typed array/list/set/dict、函数参数/
结果和返回闭包、嵌套 Future、资源借用、error 与 UINT64_MAX。

callbackTask 进一步让同一个 CallbackOwner 同时成为闭包捕获和名义类参数。
更新后的独立门禁通过：主 164 + Future-only 32 项 IR 拒绝；1,200 次执行（其中
160 次别名任务执行）、6 次回调前取消、51 个分配故障、22,213 次分配平衡；
正常/取消跨线程清理屏障、严格 C、生成 C-host JIT/AOT 均通过。
日志：build/tmp/xlir_task_types_v5_callback_gate.log。
当前 tools/check_xlir_contract.ps1 通过；旧语义负向预期和其它全门禁尚未收尾。

### 新发现的确定性运行时问题：不得以偶发通过验收

综合类型门禁严格 C 编译成功，但运行内存平衡失败。旧 v4 的单次 30 轮/
5,310 次平衡分配不足以证明稳定性；本版额外 20 次进程运行中有 4 次失败，
泄漏集中在 set/object 的 xvalue 外壳、backing、元素和桶。
保留 build/tmp/xlir_task_types_v5_values_gate.log 和
build/tmp/xlir_task_types_v5_values_diagnostic.log，未放宽内存断言。

纯 C 确定性复现 build/tmp/xlir_xrt_nonfinal_release_probe.c 不使用生成 C、
XLIR 或 AST。主线程持有集合引用并执行查询，在自定义 key hash 中用 semaphore
暂停；另一线程只释放自己预先 retain 的集合引用。集合仍有主线程 owner，但
xrtValueRelease 因查询期间的 BUSY 标记直接拒绝，未减少引用。
结果 error=operation is not valid in the current state；最终泄漏 6 块/632 字节。
日志 build/tmp/xlir_xrt_nonfinal_release_probe.log，退出 1。

当前 lib/xrt/xrt.h 和单独的 D:/GIT/xrt/src/value/value.c 都含该 BUSY 前置拒绝。
这里只读检查 XRT 主仓，尚未修改它，也未手改生成头或发布 DLL。正确收口需要
在 XRT 源库厘清非末引用释放与 BUSY 重入保护的契约，并以并发/最后引用/重入测试
验证后再生成、同步匹配快照和运行时。不能在 Cgen 加复制、重试或延迟释放补丁。

### 版本与后续

task-types-v5 编译器源码 355 文件 SHA-256：
20D0D01A8E4A0F54115C4FD32F84FCE6D116270E9F6FEF1A4CF4E8898B3CCEEF。
新 494 项生成扫描由冻结 build/test/xlir_task_types_v5_driver.exe 执行，
原会话 2593 已正常结束：494/494、失败 0、退出 0。结果 JSON 已独立核对。
这个生成扫描不能代替上面的运行时缺陷修复和最新源码完整验收。

另外仍需完成旧 task.spawn 语义测试预期迁移、新类型门禁完整 JIT/AOT、同版回归；
以及更广泛分配失败、构造 escape、owner/alias/cell、native borrow/layout、
Future 最终 payload 析构与 DLL/API code lease / 延迟卸载。发布 CLI/DLL 未重建，
legacy 未清退，标准库只做最低必要范围；长期目标保持 active。

### task-types-v5 同版补验与语言准入修订

编译器源码未进一步改动，仍为上面的 355 文件/20D0D01A...B3CCEEF。
本轮改动限于测试、门禁及记录；XRT 独立源库/生成头、发布 CLI/DLL 未改动。

- 新增 `sem_task_spawn_arg_byref.xl` 与 `sem_task_spawn_arg_out.xl`。
  当前源码 contract 的 `--admission` 对全部 43 个任务签名用例逐一解析、语义检查：
  28 个语言接纳、15 个语言拒绝；26 个接纳用例继续完成 typed/lowered verifier
  和 C/header 生成。两个 generator 签名只验语义接纳，明确不算后端完成。
  这些旧签名用例中有未初始化 func，接纳不意味着调用 null func 能运行成功；
  真实 callable 的运行语义由单独 task value types fixture 检查。
- `07a_async_part_b.bat` 与 `07a_async_part_c.bat` 已把 28 个旧白名单负例
  改为语义正例；3 个不可复制资源返回仍拒绝，使用普通函数的同一诊断。
  byref/out 保留准确拒绝。中心 negative-diagnostics 映射已同步，
  静态接线 451 files / 423 expected failures / 423 asserted / 0 baseline-only。
  未运行依赖旧 release 编译器的这两个完整 batch。
- 原 task_spawn_closure_abi 门禁改为检查 callbackBoundary/capture 的精确身份、
  operand、角色与 transfer/destroy ABI；C 断言固定字段脱离后清理、原错保留和
  清理后发布，不再要求旧 launcher/cleanup 字符串。抽取其实际断言对本版
  source-driver 产出的 C/JSON 执行通过；没有冒充完整 release/legacy 差分门禁。
- 纯 XRT 复现已成为永久 `xlir_task_value_types_release_runtime.c`，只用公开 API，
  不依赖私有 xvalue 布局。新类型门禁必须先通过它，才能接受压力运行结果。
  完整新门禁本轮复跑：语言准入、459 项 IR 变异及重复 C/header/JSON 通过，
  然后确定性失败于非末引用释放（6 块/632 字节），总体退出 1。
  `build/tmp/xlir_task_types_v5_values_gate_with_admission.log` 保留完整失败；
  该门禁的后续综合运行/JIT/AOT 尚未完成，不删除、不跳过、不标 expected-pass。

十组同版 source-driver 回归全部通过，日志/结果在
`build/tmp/xlir_task_types_v5_*_regression*`。分组为 activation/protocol/native/
generator 28、nullable/capture 17、index 12、field 18、runtime/contract 14、
virtual 16、dynamic 10、enum 7、persistence 8、task 7；组间重叠不累加为完成率。

六个完整 current-source 门禁均正常结束并通过严格 C、JIT/AOT 与各自的运行/
故障/IR 断言：terminal、owned-activation、module-task-pool、virtual-activation、
owned-projection、pool-outcome。日志 `build/tmp/xlir_task_types_v5_*_gate.log`。
例如 terminal 349 项变异/2,200 次执行/3,000 次挂起/20,363 次平衡分配；
owned projection 1,400 次真实分配故障/27,901 次平衡分配；
pool outcome 540 次执行/60 次反向取消/21 个提交前故障/13,645 次平衡分配。
这些通过项不抵消新类型门禁的 XRT 失败，也不证明完整 Future 最后 owner 或
DLL/API 动态卸载已完成。

静态合同、迁移接线 129 rows/374 representatives/461 deterministic candidates/
144 differentials/83 XLIR-only、source 288、batch 11/67、Core C fixtures 573/11、
encoding 5966、markdown 329 均通过。仍没有把静态数量当成全部运行结果；
CoreOnly 不覆盖单列的 MQTT/serial 包门禁接线问题。

下一步仍需 XRT 源库修复/同步的范围确认；并继续拥有型 BYVAL/generator 任务 ABI、
分配失败与构造 escape、alias/cell/capture、native borrow/layout、Future 最后 payload、
DLL/API code lease / 延迟卸载等完整目标。不可因本节新增回归通过而宣布交付。

### task-byval-v5：直接调用收口，容器桥接仍失败

本检查点编译器源码 355 文件 SHA-256：
EC8EEE67D4907194542A794FA14592585793C77D9D8BBA324177707D0DD00027。
当前编译器不同于上一节 task-types-v5；通过证据不得跨版本混用。

- CallbackCapture 新增独立 invokeAbi，明确 frame 取得和 callee 消费的不同
  边界；task.spawn 的托管 BYVAL 在调用提交前才脱离 frame。
- 命名函数适配器的 BYVAL 输入和默认值现在显式 LOAD/MOVE/CALL。
  测试直接调用仿射验证器，证明删除 MOVE 后不能把借用当作 owner 消费。
- 修复 error.cause.message 中间错误 owner 泄漏；使用已有表达式生命周期，
  不增加 Cgen 链式表达式补偿。原综合日志中后续的 class/closure 重新绑定
  类型错误包含连带诊断，直接用例已经证明这些具体重新绑定可以正确运行。
- 新增 check_xlir_task_byval.ps1，并接入 core 03 与静态合同。它依次运行
  direct 和 full 两阶段，只有两阶段全部通过才返回成功。完整 fixture 保留
  array<func(byval string) as int> 往返，不删除、不跳过其失败。
- direct 阶段：8 个 TAKE、2 个 VALUE、8 个 CONTEXT、9 个命名适配器 MOVE
  （含省略默认值）、1 个 cause 接收者 owner、63 项错误 IR 拒绝。
  重复 C/header/JSON、严格 C、生成 C-host JIT/AOT 均通过；30 轮模块运行、
  240 次任务调用、3,841 次分配/释放平衡。额外 10 个独立进程复跑全部通过。
  该用例的 semaphore 隔离已知 XRT 并发非末引用释放缺陷，不证明该缺陷已修复。
- full 阶段仍在带 BYVAL 的 runtime-callable 装箱处失败，整个新门禁退出 1。
  build/tmp/xlir_task_byval_v5_gate.log 保留直接阶段通过与完整阶段失败。
  尚未放宽双向桥的默认参数模式限制，也没有在 emitter 增加隐式所有权猜测。

同版专项回归：named adapter 11、field 18、dynamic 10、task 7、
runtime/contract 14 均通过；这些组间存在重叠，不累加为整体完成率。
callback 门禁通过 188 项主模块变异、36 项 Future-only 变异、
1,200 次执行、51 次分配故障、22,213 次平衡分配及跨线程清理后发布检查。
terminal、owned-activation、module-task-pool、virtual-activation、
owned-projection、pool-outcome 六个完整门禁亦均通过严格 C、JIT/AOT
及各自的真实挂起、故障、取消、内存断言。

静态合同与接线通过；迁移矩阵仍为 129 rows/374 representatives，
deterministic candidates 增至 463，144 differentials/83 XLIR-only。
source 288、batch 11/67、Core C fixtures 576/11、encoding 5972、
markdown 329 均通过。静态数量不是相同数量的完整运行结果。

本节写入时，冻结 driver 的新 494 项扫描仍在原会话 96653 执行，不能沿用
旧版 494/494 宣布本版全过；以对应 task-byval-v5 source snapshot 和
audit 日志的最终状态为准。XRT 独立源库/生成头、发布 CLI/DLL 未修改。
完整 BYVAL 桥、generator task ABI、运行时引用缺陷、分配失败与构造 escape、
alias/cell、native borrow/layout、Future 最后 payload、DLL/API 安全卸载、
发布验收及 legacy 清退仍未完成；长期目标保持 active。

## 2026-09-08：双向 callable 参数程序与故障回滚（callable-byval-v3）

task-byval-v5 的原 494 项扫描已核对完成：494/494、失败 0、原会话退出 0。
本检查点新增 callable 参数程序，编译器源码 356 文件 SHA-256：
EA4C14F29260E54E5C26F7BB0B658DF4A2BF6F14BB3B189096DA865BCD1753E7。
旧版本的通过项不移算给本版；最新冻结 driver 的扫描使用
build/tmp/xlir_callable_byval_v3_audit*，扫描是生成验收，不是全量运行验收。

### 实现与已通过验证

- XLIR 新增 CallableBridge / CallableBridgeParameter，逐签名记录参数的
  decodeAbi、encodeAbi、invokeAbi、decodedDestroyAbi、inputDestroyAbi，
  并记录事务清理和原始错误保留规则。lowering 构造完整规范顺序的计划，verifier
  独立检查缺失、重复、范围、类型、操作码及所有权。typed IR 不得提前含 lowered 计划。
- Cgen 的两个方向使用该参数程序。forward 从借用 call frame 取得独立 owner，
  调用前提交 TAKE；reverse 对收到的 OWNED 参数在装箱失败、调用失败和正常返回时
  都负责结束生命周期。各槽先清空再析构，保留第一个错误；不追溯 AST 补猜 BYVAL。
- 原 task BYVAL full 容器往返恢复通过；direct/full 各 198 项非法 IR 拒绝，
  各 30 轮模块、240 次调用，分别 3,841 / 4,111 次分配释放平衡，重复 C/header/JSON、
  严格 C 和生成 C-host JIT/AOT 均通过。日志 build/tmp/xlir_callable_byval_v3_task_gate.log。
- 新永久 check_xlir_callable_byval.ps1 已接入 core 03。36 项共享参数计划变异均被
  verifier 与 C/header emitter 拒绝且无输出；重复 C/header/JSON/binding 一致。
  12 种调用/失败组合共 1,080 次执行、840 次真实分配故障、6,597 次平衡分配。
  覆盖 string/xvalue/error 的两向 BYVAL、参数解码中途失败、无效环境/输出、
  原始错误和抛错析构；失败不提交结果，输入拥有者释放恰好一次。
  严格 C、生成 C-host JIT/AOT 均通过。日志 build/tmp/xlir_callable_byval_v3_gate.log。
- 同版 named 11、field 18、dynamic 10、runtime/contract 14、task 7 分组回归通过。
  各组范围见对应 build/tmp/xlir_callable_byval_v3_*_regression.log，不相加当整体完成率。
- 静态 IR 合同、source 289、batch 11/67、Core C fixtures 580/11 通过。
  迁移矩阵仍为 129 rows/374 representatives，deterministic candidates 为 464，
  144 differentials/83 XLIR-only；这些数量不是相同数量的当前完整运行结果。

### 验收边界

新桥承诺现有支持参数族中的 DEFAULT/BYVAL，不代表 Future/closure/nullable 等全部
参数桥、Future.then/catch 的 BYVAL、generator 任务 ABI 或所有名义动态类型验证完成。
结果槽的完整桥接程序与解码/清理失败组合也仍需继续收口。
任务 full 用例仍隔离已知 XRT 并发非末引用释放缺陷，不证明其被修复。
该独立 XRT 失败门禁仍保持红色，源库修复和同步范围仍待确认；未绕过或改为预期通过。
本版七个异步完整门禁和 494 项扫描的最终状态以随后记录的 source snapshot 为准，
运行中不得计为通过。发布 CLI/DLL、XRT 生成头哈希未改变，legacy 未清退。
分配失败/构造 escape、alias/cell、native borrow/layout、Future 最后 payload 和
DLL/API code lease / 延迟卸载仍是长期目标的一部分；本检查点不是最终发布准入。

本版七个异步完整门禁现已核对原运行会话退出 0：callback、terminal、owned-activation、
module-task-pool、virtual-activation、owned-projection、pool-outcome。全部通过严格 C、
生成 C-host JIT/AOT 及各自运行/故障/变异断言；日志为
build/tmp/xlir_callable_byval_v3_*_gate.log。encoding 5,979 与 markdown 329 检查也通过。
这些结果不抵消独立 XRT 失败，也不把仍在运行的 494 项扫描计为通过。
本版源代码、门禁产物及发布哈希快照：build/tmp/xlir_callable_byval_v3_source_snapshot.json。

### 新发现：结果转换失败的原始错误被析构覆盖（未修复）

独立严格 C 探针 build/tmp/xlir_callable_byval_v3_result_probe.c 用真实 runtime callable
返回不匹配 int 的拥有型 Handle；反向适配器先发现类型错误，再释放 boxed 结果。
无析构错误时报 code=1 / value has an incompatible type；析构抛错后变为
code=99 / secondary result destructor。两次结果均未提交，内存平衡，但错误优先级
不正确，运行退出 3。日志 build/tmp/xlir_callable_byval_v3_result_probe.log。
因此，本版参数门禁和七个异步专项通过不代表结果桥接事务完成。下一步应为结果槽
建立完整的解码/取得/销毁/提交 IR 契约，使结果转换失败和后续清理共享原始失败状态，
而不是在现有 Cgen 分支里逐个补错误指针。此失败未标为预期通过，长期目标仍 active。

## 2026-09-08：callable 结果程序与清理后提交（callable-result-v5）

callable-byval-v3 的原扫描已核对完成：494/494、失败 0、原会话 6877 退出 0。
本版编译器源码为 357 文件，SHA-256：
1AD9BD20F39FC56D31F14B3C9605BC4373ED804EE39BFFC2B38F062290B138EC。
本版冻结 driver 的扫描使用 build/tmp/xlir_callable_result_v5_audit*；其完成状态
须独立核对，不能移用上一版 494/494。

- CallableBridgeResult 记录每个结果槽的编解码与销毁程序；CallableBridge 增加
  结果分区、carrier、事务提交和 frame 清理 ABI，均独立验证并进入 text/JSON dump。
- forward 私有装箱 frame 和 reverse 私有 native carrier 在清理成功后才提交。
  两向使用共享 error-scope，逐槽保留原始失败并先脱离拥有者再析构。frame 操作使用
  XRT 公开 Move/Set API，不改写其内部字段或独立运行时源码。
- 原类型错误被结果析构覆盖的严格 C 探针恢复通过：两种析构情形均保留 code=1。
  返回闭包解码改用可失败 try-copy，不再因该路径分配失败直接 abort。
- 显式 xcall 接纳经递归检查的返回闭包；正向语言函数通过数组和 xcall 完整往返。
  三项前端负例验证返回闭包 byref/out 以及隐式 func-to-var 仍拒绝；byref/out 还核对
  具体 xcall 返回类型诊断，避免把无关语义错误当作验证。
- 新永久 check_xlir_callable_result.ps1 接入 core 03。103 项结果/参数计划变异拒绝
  且无 C/header 输出；C/header/JSON/binding 重复一致；严格 C、生成 C-host JIT/AOT 通过。
  760 次执行、560 次真实分配故障、4,135 次平衡分配，覆盖 string/xvalue/error/closure
  产品结果、部分解码/装箱、超过内联容量、失败后无输出提交与抛错析构。
  日志 build/tmp/xlir_callable_result_v5_callable_result_gate.log。
- 参数 BYVAL 门禁仍通过 49 项变异、1,080 次执行、840 次分配故障和 6,597 次平衡分配。
  task BYVAL direct/full 各 282 项变异、30 轮模块、240 次调用与 3,841 / 4,111 次平衡分配；
  以上均通过严格 C、生成 C-host JIT/AOT。日志为本版 callable_byval/task_byval gate。
- 原 check_xlir_xcall_result_failure.ps1 改为当前源码 contract 构建、IR 结果计划断言，
  不再用旧 release 编译器或匹配旧 product-drop 文本充当验证，DLL 复制也改为 hash-skip。
  129 项 bridge 变异拒绝；1,500 次部分失败（含 300 次真实分配故障）、400 次成功，
  原始错误对象身份、诊断被 native 消费和清理重入均通过；JIT/AOT 各 1,200 次注入失败
  与 400 次成功。日志 build/tmp/xlir_callable_result_v5_xcall_failure_gate.log。
- 本版 named 11、field 18、dynamic 10、runtime/contract 14、task 7 分组回归通过；
  有交叠，不累加当整体完成率。七个异步完整门禁及 494 扫描以随后 source snapshot
  的终态证据为准；运行中的任务不能计为通过。

仍未完成：普通闭包/nullable 投影的分配错误边、更广的 bridge 参数/名义动态类型、
Future 回调 BYVAL、generator 任务 ABI、构造 escape、alias/cell、native borrow/layout、
Future 最后 payload、DLL/API code lease / 安全延迟卸载，以及发布准入和 legacy 清退。
独立 XRT 的非末引用释放失败仍保留且待源库修复范围确认；标准库仍只做最低必要范围。
本版未重建发布 CLI/DLL、未更新 XRT 生成头，长期目标保持 active。

本版七个异步完整门禁现已核对原会话退出 0：callback、terminal、owned-activation、
module-task-pool、virtual-activation、owned-projection、pool-outcome。均通过严格 C、
生成 C-host JIT/AOT 和各自运行/故障/变异断言。普通 XCall 门禁也使用本版冻结 driver
通过五族严格 C/运行检查，并完成 class/pointer 两族 legacy 对照。
静态 IR 合同、source 290、Core C fixtures 584/11、batch 11/67、encoding 5,986、
markdown 329 已通过；迁移矩阵 129 rows/374 representatives、465 deterministic
candidates、144 differentials/83 XLIR-only 是覆盖接线数据，不是全量运行通过数。
当前检查点记录于 build/tmp/xlir_callable_result_v5_source_snapshot.json。原扫描会话
90896 仍在运行，最新落盘 280 项、失败 0；扫描和独立 XRT 失败均不计作已完成。

## 2026-09-08：可失败投影、nullable 状态与 callable 错误隔离（checked-projection-v11）

callable-result-v5 原扫描会话 90896 已确认退出 0：494/494、失败 0；其 snapshot
Audit 已更新完成。以下为新源码检查点，不能直接沿用旧版扫描结果。
编译器源码 357 文件 SHA-256：
E87D97E78138F785333346FCB92D6735AA1C197DB32C10F17AF45B1261BC2686。

### 实现与缺陷修复

- closure/string 及 nullable 投影建立 typed 错误效应和立即 ERROR_TEST。新物理
  STATUS_OUTPUT 将合法 null 和失败分开；Cgen 同步/异步/generator 共用结果观察器。
- 游标先登记到清理计划再投影迭代元素；字段默认初始化失败沿实际构造调用点退出，
  safe member 支持已 nullable 字段的扁平结果类型。
- 真实故障门禁发现容器字面量直接产生的 named closure 在装箱后遗漏原 owner，
  曾出现 1 个 / 64 bytes 泄漏；已由表达式拥有权清理修复，不修改 Cgen 做补偿。
- 真实语言 catch 还暴露内部 callable 失败污染外层 native/export 的 sticky 失败位。
  IR 现在显式区分 forward native-callback / reverse isolated-status；reverse
  局部收集 runtime invoke 错误，向语言 CFG 返回失败，不将已捕获异常报告为外层失败。
- 参数回滚改用共享 error-scope 的独立失败位，不再仅保存非空错误指针。无效
  environment/output 的首个诊断分配失败时仍返回 false；即使 owned handle 析构
  抛出 code 99，也保持最初的无诊断失败，不伪造回调错误来填补它。

### 已通过验证

- 新 check_xlir_checked_projection.ps1 接入 core 03：11 项 typed effects、14 个
  checked reconstructions、10 个 nullable status outputs、3 个 fallible loops，
  121 项变异拒绝且无 C/header 输出；重复 C/header/JSON、严格 C、C-host JIT/AOT 通过。
  16 类组合、1,350 次执行、870 次真实分配故障、22,770 次平衡分配；包含合法 null、
  字段/索引/循环、catch/finally、临时接收者和已捕获 callable 错误隔离。
- callable BYVAL：51 项变异、1,000 次执行、760 次故障、5,917 次平衡分配。
  callable result：109 项变异、680 次执行、480 次故障、3,435 次平衡分配。
  比上版少的故障点来自移除不正确的 callback 错误包装分配；原测试族和扫预算循环
  均保留。BYVAL 仅对明确验证的首个诊断分配失败允许 NULL diagnostic，失败位、
  零输出提交、exact-once drop 与正常语义错误诊断的断言仍保留。
- XCall result failure：139 项变异、1,500 次部分失败（300 次实际分配故障）、400 次
  成功，原错误身份和内存平衡通过；C-host JIT/AOT 各 1,200 次注入失败、400 次成功。
- task BYVAL direct/full：各 298 项变异、30 轮模块/240 次调用、3,841 / 4,111 次平衡
  分配。以上永久门禁均已确认原运行会话 83825 退出 0，严格 C 与 C-host JIT/AOT 通过。
- 七个异步门禁 callback-boundary、terminal-outcome、owned-activation、module-task-pool、
  virtual-activation、owned-projection、pool-outcome 均确认原会话 92767 / 76329 退出 0，
  通过各自变异、真实故障/生命周期、严格 C 与 C-host JIT/AOT 验证。
- 本版 named 11、field 18、dynamic 10、runtime/contract 14、task 7 分组回归均已完成；
  普通 XCall 五族 source-driver 检查及 class/pointer 两族 legacy 对照也通过。
- 静态 IR 合同、source 290、Core C fixtures 587/11、batch 11/67 通过。迁移矩阵为
  129 rows / 374 representatives / 466 deterministic candidates / 144 differentials /
  83 XLIR-only。这些是覆盖接线数量，不折算成整体完成率。

当前源码扫描使用 build/tmp/xlir_checked_projection_v11_audit*，原会话 19399，
扫描仍在运行，当前仅确认落盘 40 项、失败 0；它是生成验收，不是 494 项完整运行验收。
门禁日志前缀 build/tmp/xlir_checked_projection_v11_，完整检查点以随后 source snapshot
为准。发布 CLI/DLL 与 XRT 生成头未改动，独立 XRT 非末引用释放失败仍待修复范围确认。

### 未完成范围

闭包/cell/其他复制与装箱、迭代器构造的全部分配失败与构造 escape、更广 callable
参数族和名义动态类型检查、Future 回调 BYVAL、generator 任务 ABI、owner/alias/cell
全路径证明、native borrow/layout、Future 最后 payload、DLL/API code lease 与延迟卸载、
发布准入和 legacy 清退仍在长期目标中。标准库继续最低必要范围；本检查点不是最终完成。

## 2026-09-08：非 product SSA owner 闭合与构造失败清理（owner-completion-v7）

上一检查点 checked-projection-v11 的原扫描会话 19399 已退出 0，494/494 生成通过。
原源码/产物快照为 build/tmp/xlir_checked_projection_v11_source_snapshot.json，
对应审计完成凭据为 build/tmp/xlir_checked_projection_v11_audit_completion.json。
它证明旧检查点生成验收已结束，不表示下述当前源码复用了旧扫描结果。

当前编译器源码 357 项，SHA-256：
`9FF5F115F642EB877B1985F86A700BD78EA71F3755DEBDD94A555A6B68078746`。

### 实际推进

- OwnerConsumption 从独立生产者出发验证有限出口和再次获取，捕捉完全没有消费点
  的临时 owner；保留已有 affine 和 product 的独立检查，不将其包装为全部 lifetime
  已完成。ASYNC_START/GENERATOR_START 按目标 BYVAL ABI 消费参数。
- 14 个原本由 Cgen 分别观察 NULL 的 RuntimeOp，其物理失败结果统一纳入 IR 查询。
  空失败输出与原生失败时仍存在的拥有型结果严格区分，原生返回值在 XLIR 失败分支
  回滚，保留第一次失败位和可选诊断；string 的原生 ABI 是借用，CALL 的语言复制
  结果才是需要回滚的 owner。
- 部分 array/list/dict/set 构造进入 evaluation-owner 栈，bare func 初始化闭包在
  装箱两侧完成清理；属性的 length/null 观察按 SSA 拥有权清理；插值累积值在下一段
  求值前受保护；nullable string coalesce 两个出口均有明确拥有权结束。
- 同一新增测试用旧 v11 编译器生成后，首个数组构造失败确定性残留 4 个分配、264
  字节，负对照退出 3；日志 build/tmp/xlir_owner_completion_v11_negative.log。
  最新源码相同用例及其余七族均无残留。本阶段没有编辑 Cgen 源文件或发布二进制。

### 当前验证

- 新永久 check_xlir_owner_completion.ps1 已接入 core 03，原会话 64107 退出 0。
  337 个 RuntimeOp 输入 effect、1,102 个消费输入、510 项拒绝变异、23 个 CFG 测试；
  8 族 × 2 种诊断模式 × 50 轮，共 800 次执行、8,700 次平衡分配。部分构造、失败
  返回值、缺失诊断、析构再次失败、catch/finally 均通过；严格 C 和 C-host JIT/AOT
  通过。缺失诊断模式是显式无诊断失败，不冒充真实分配故障次数。
- 投影门禁原会话 41686 退出 0：121 项变异、1,350 次执行、870 次真实分配故障、
  22,770 次平衡分配，重复输出/严格 C/C-host JIT/AOT 通过。
- callable BYVAL、callable result、XCall result failure、task BYVAL direct/full 的
  当前源码复验原会话 85115 退出 0；七个 async 专项当前复验原会话 66802 退出 0。
  各门禁保留原来的运行、内存、故障、变异与 JIT/AOT 断言。
- named 11、field 18、dynamic 10、runtime/contract 14、task 7 分组当前复验原会话
  80516 退出 0；普通 XCall 五族及 class/pointer 的 legacy 对照也通过。
- 492 项快速生成扫描原会话 13749 退出 0；其明确排除了两个耗时 task API 用例。
  cgen_task_api 当前源码生成原会话 36784 已退出 0；release/examples/task_api.xl
  原会话 30447 也已退出 0；合计 494/494 生成通过、失败 0。所有扫描使用同一
  冻结 v7 source driver；这仍不是 494 项完整运行时验证。
- 静态 IR 合同、source 290、Core C fixtures 589/11、batch 11/67 通过；迁移接线为
  129 rows / 374 representatives / 467 deterministic candidates / 144 differentials /
  83 XLIR-only。日志前缀 build/tmp/xlir_owner_completion_v7_；完整留档以 source
  snapshot 为准。编码 5,995 项和 Markdown 链接 329 项通过；检查点完整留档为
  build/tmp/xlir_owner_completion_v7_source_snapshot.json。

### 仍未完成

此处的闭合证明不代替 product 逐字段 CFG、存储/alias/cell/capture 的全路径证明，
也未补齐 closure/cell/其他装箱/迭代器的全部分配失败及构造 escape。更广 callable
参数与名义动态边界、Future 回调 BYVAL、generator 任务 ABI、native borrow/layout、
Future 最后 payload、DLL/API code lease 与延迟卸载、发布准入、legacy 清退仍在范围内。
独立 XRT 非末引用释放缺陷仍等待外部仓库修复范围确认；本轮未修改 XRT 或绕过该问题。

## 2026-09-09：Product 逐字段 CFG 与受保护绑定（product-flow-v3）

当前编译器源码仍为 357 项，SHA-256：
`9CB2F667E8CEEF80B60022C25FF51093AB8FDCE92A6B27EEC71C448A50E1B268`。
冻结 source driver 为 build/test/xlir_product_flow_v3_contract.exe。相对上一检查点
只修改 xl6_ir.h、xl6_ir.c、xl6_ir_ownership.c、xl6_ir_lifetime.c、xl6_ir_lower.c；
Cgen 源文件未改动，release 与 XRT 仍未更新。

### 实际修复与证据

- 新 CONSUME_FIELD effect 进入统一 owner CFG 状态机，删除全局 wholeTransfers /
  elementConsumes 次数判定，保留 product 的输入、结构和合法使用 ABI 检查。
- 23 个新增 product CFG 测试覆盖无消费、部分消费、分支各漏一个字段、互斥完整
  拆解、互斥整值/逐字段消费、重复转移、循环覆盖、借用未 MOVE、TAKE 独立结果
  和空失败 carrier；4 项真实模块字段变异在 C/header 输出前被拒绝。
- 原有 xlir_multi_assign_index_abi 与 cgen_typed_container_multi_assign_index
  被新验证器检出失败出口仍拥有第二字段。统一绑定 lowering 先保护各字段，再
  顺序求值目标，修复后均生成通过；声明丢弃和直接替换也共享显式清理结果。
- 同一新语言 fixture 用上一 v7 编译器生成时，第一个索引目标求值失败后，两个
  字段的析构次数均为 0，负对照退出 3。日志：
  build/tmp/xlir_product_flow_v7_negative.log。该负对照证明字段清理遗失，不将
  析构计数冒充已测量的泄漏字节数。
- 新 check_xlir_product_owner.ps1 接入 core 03。检查点门禁原会话 34861 退出 0：
  337 RuntimeOps、1,102 消费输入、518 项拒绝变异、46 个 CFG 检查；新运行时
  1,290 次执行、60 次真实分配故障、33,750 次平衡分配，严格 C、C-host JIT/AOT、
  C/header/JSON 重复输出均通过。无诊断模式另为显式注入失败，不算真实 OOM。
  故障注入在两个 RHS 字段已产生后开启，成功赋值结束后关闭，不覆盖后续断言的
  字符串读取分配。最终诊断输出调整后严格 C/JIT/AOT 又复验通过。
- product-result 旧门禁改用当前源码 driver，并移除 common.bat 与无条件覆盖运行
  中 DLL 的路径；direct/class/closure/generic/managed 五族严格 C/runtime 通过。
- 非 product owner completion 原会话 6856 退出 0（800 执行、8,700 平衡分配）；
  checked projection 原会话 97409 退出 0（1,350 执行、870 真实分配故障、
  22,770 平衡分配）；两者严格 C/JIT/AOT 均通过。
- return regression 原会话 85279 退出 0：1,000 次 return-commit 事务、
  80,080 平衡分配、14 项拒绝变异；return projection 10,500 次执行、
  3,000 次真实诊断分配故障、225,600 平衡分配、110 项拒绝变异，当前源码严格 C
  与契约通过。本次 return regression 没有另行声称 language JIT/legacy 验收。
- async terminal 原会话 45714 退出 0：349 项变异及原有终态、嵌套挂起、清理、
  内存与严格 C/JIT/AOT 断言通过；不代表本次新 product 跨 await 专项已完成。
- 快速生成原会话 93394 退出 0，493/493、失败 0；结果保存在
  build/tmp/xlir_product_flow_v3_fast_audit.json。该范围为上一 492 项加新 fixture，
  明确不包含两个耗时 task API 用例，也不是全部运行时验证。
- 静态 IR 合同与接线通过：source 290、129 semantic rows、374 representatives、
  468 deterministic candidates、144 legacy differentials、83 XLIR-only、
  batch 11/67、Core C fixtures 592/11。接线数量不等同于执行通过数量。

### 当前验收边界

两个独立完整生成以原会话的终态结果入账：cgen_task_api 会话 52399 已退出 0，
release/examples/task_api 会话 56867 也已退出 0。与快扫合计 495/495 生成通过，
失败 0；这仍不是 495 项全部运行时验收。编码 6,000 项和 Markdown 链接 329 项通过。
源码/产物归档及原会话终态以 build/tmp/xlir_product_flow_v3_source_snapshot.json
为准。全路径存储/alias/cell/capture、CELL_STORE 失败边、跨 await 的 product
绑定专项、其余构造失败、callable/Future/generator ABI、安全卸载、发布准入及
legacy 清退仍未全部完成。外部 XRT 非末引用释放缺陷继续等待修复范围确认，
本阶段没有修改 XRT，也没有在 Cgen 中绕过它。

额外的可变捕获探针已确认当前 CELL_STORE 边界仍有真实失败：
build/tmp/xlir_product_flow_v3_cell_probe.xl 使用受支持的 var cell，旧值析构
注入 77 后，后续清理注入 99；实测 result=0、caught=1、code=99、finally=1，
而预期为 catch 返回 23 并保留 77。生成和严格 C 编译通过，但运行退出 3，日志为
build/tmp/xlir_product_flow_v3_cell_probe.log。这是本检查点的已知未修复项，
不能用上述非 cell 的 1,290 次通过结果覆盖。下一阶段须显式化 cell 载荷转移与
旧值析构的 XLIR 失败边界。只读捕获采用复制并不等同于可变 cell；探针明确在
闭包体内写入捕获，以保证实际生成 cell 指令。

## 2026-09-09：显式 cell 交换与统一存储替换（cell-exchange-v2）

编译器源码 357 项，SHA-256：
`F1FC625AF5C6F65ED5FFA7ABE61197386A003667391B4C99117DBC71824D2F47`。
冻结通用 source driver 为 build/test/xlir_cell_exchange_v2_contract.exe。
相对 product-flow-v3 修改 5 个 IR 文件和 9 个 Cgen 文件；新增显式语义同时合并
三处 MOVE 输出，Cgen 源码净减少 1,335 字节。release 与 XRT 均未更新。

### 语义变化与负对照

- CELL_EXCHANGE 消费新载荷 owner，先发布并返回独立旧 owner；旧值析构由 XLIR
  清理槽及失败边处理。managed CELL_STORE 被 verifier 禁止，C 的 string/xvalue
  store helper 不再隐式析构。cell 的载荷借用失效与 cell 对象的存活分别建模。
- 普通、间接和全局存储共用 LOAD/MOVE/STORE 发布及后续清理。旧值析构失败会
  保留已发布的新值并中止后续操作；不执行回滚或回调后的陈旧写入。此约束不
  声称线程间原子性。同步、async 和 generator MOVE 共用精确物理存储输出。
- 原先两个异步赋值 fixture 暴露共享存储 MOVE 的 Cgen 准入缺口；已补齐验证过
  的共享存储绑定，不放宽 CELL_NEW 等指令到未定义存储。
- product owner fixture 新增真正的可变 var cell 多赋值。相同新 fixture 使用
  上一 v3 source driver 的负对照仍在 kind=5、failDrop=10 报 result=0/code=99，
  而预期 result=23/code=77，运行退出 3；当前实现通过。日志保存在
  build/tmp/xlir_cell_exchange_v2_negative.log，不覆盖上一检查点的原始产物。

### 当前源码验证

- source owner contract 会话 44400 退出 0：337 RuntimeOps、1,106 消费输入、
  546 项非法变异拒绝、46 个 CFG 检查。
- 存储事务契约命令退出 0：32 ordered updates、13 managed
  snapshots、2 cell、8 static、47 项拒绝变异。条件赋值契约 49 transactions、
  21 cell、21 tagged、26 managed、6 async/generator cell frames、163 项拒绝变异。
  两者测试共用 xlir_storage_exchange_checks.h 验证提交后的独立旧 owner 清理，
  替换了旧的析构后再提交结构断言；C/header/binding 重复生成验证单独执行。
- build/tmp/xlir_cell_exchange_v2_runtime.log 对应命令退出 0：存储事务 1,200 同步
  加 1,200 实际 async 用例、89,800 平衡分配；条件赋值 7,200 同步、3,600 async、
  2,400 generator resume/early-close/failure 用例、137,600 平衡分配；初始化前
  基线均恢复。严格 C 与 C-host JIT/AOT 通过，不把 smoke 的固定输出标签作为额外
  内存统计。更新后的 product/cell 为 1,490 执行、60 真实分配故障、41,200 平衡
  分配，首次 77/无诊断失败与后续 99 均被覆盖。
- 新 check_xlir_storage_exchange.ps1 接入 core 03，会话 46422 退出 0：6 个显式
  cell exchange、48 项类型/形状/借用/重复消费/遗失旧 owner 变异均在 C/header
  输出前拒绝。1,200 次 cell/global/byref/generator 析构重入执行、32,801 平衡
  分配，回调观察新值并再次改写，外层没有覆盖回调写入。严格 C、C-host JIT/AOT
  和 C/header/JSON 重复输出通过。该门禁与 runtime.log 中早期 900 次探针有区别，
  最终含 generator 的结果以 build/tmp/xlir_cell_exchange_v2_gate.log 为准。
- owner completion 800 执行、8,700 平衡分配；checked projection 会话 4196
  退出 0，121 拒绝变异、1,350 执行、870 真实分配故障、22,770 平衡分配；两者
  当前源码严格 C、C-host JIT/AOT 及重复输出通过。
- async terminal 会话 24927 退出 0：11 await、40 canonical resume entries、
  349 拒绝变异及现有终态/清理/内存断言，严格 C/C-host JIT/AOT 通过。这不等于
  新 product 跨实际 await 绑定专项已经完成。
- return regression 会话 23003 退出 0：1,000 return-commit 事务、80,080 平衡
  分配、14 拒绝变异；return projection 10,500 执行、3,000 真实诊断分配故障、
  225,600 平衡分配、110 拒绝变异。范围为当前源码严格 C/runtime 与 IR/C/header
  契约，不声称新一轮 language JIT/legacy 验收。
- 快速生成会话 59219 退出 0，494/494、失败 0，含新增 storage exchange fixture。
  cgen_task_api 会话 25535 与 release/examples/task_api 会话 50999 均已退出 0；
  总计 496/496 生成通过。终态由本检查点 source_snapshot.json 记录，生成成功
  不等同于全部用例的运行时通过。
- 静态 IR 合同、source 290、migration 129 rows/374 representatives/469 candidates、
  144 legacy differentials/83 XLIR-only、batch 11/67、Core C fixtures 596/11 通过。
  这些是接线检查，不是新增 runtime 执行数。上一检查点的 108 个冻结 build 产物
  已重新核对 SHA-256，0 差异。

### 尚未完成的范围

此检查点修复已知 CELL_STORE 失败边，不代表全部存储/alias/capture 路径已经证明。
新 product 跨实际 await 绑定、CELL_NEW/closure 等完整构造失败及逃逸、其余
callable/Future/generator/native/nullable 边界、Future 最终载荷、DLL/API 安全
卸载、发布准入与 legacy 清退仍有独立工作。外部 XRT 非末引用释放缺陷仍待确认
修复范围；没有修改外部仓库，也没有引入 Cgen 绕过。以
build/tmp/xlir_cell_exchange_v2_source_snapshot.json 区分源码、证据和发布状态。

## 2026-09-09：实际 await 下的 product 与帧借用（frame-loan-v4）

编译器源码 357 项，SHA-256：
`C099F588F454A8322A585D4942843AF1E17B42D16363C94904AC426FD41D252B`。
冻结 source driver 为 build/test/xlir_frame_loan_v4_driver.exe。相对 cell-exchange-v2，
Cgen 改动 8 个文件，源码净减少 834 字节；发布 CLI/DLL 与 XRT 生成头未更新。
上一检查点 134 个冻结 build 产物已重新核对，0 差异。

### 已验证的具体进展

- product TAKE 等指令由同步、async、generator 共用物理发射。旧 product-await-v2
  探针曾在替换成功时漏掉旧对象的析构失败；v3 隔离了操作失败，但四个隐藏的
  retained SSA spill owner 仍导致清理异常 99 在 catch/finally 结束后逃出。
  原始失败日志保留，没有清空错误或修改运行时来绕过它。
- 新 BORROW_STORAGE + borrowFrameSlot 使这四个 SSA 受其原来的三处拥有型存储
  保护，不再成为独立析构责任。验证器检查精确来源、所有挂起的保护槽成员关系，
  并拒绝借用自己的 init bit、析构、消费或伪造的拥有者。
- check_xlir_product_await_owner.ps1 当前源码通过：4 个帧借用、2 个实际 await、
  51 项非法 IR 在 C/header 输出前拒绝；另有 2 项 Typed 构造插入效果删除反例。
  2,400 次执行、2,800 次真正挂起、113,903 次平衡分配；两级输入的 resolve/reject/
  cancel/close、部分提交、最初 77/无诊断失败、后续 99、finally 次数和精确析构
  均有断言。严格 C、C-host JIT/AOT、C/header/JSON/binding 确定性通过。
- cancel/close 的诊断 code 按当前 XRT canonical error 定义为 1；这与无诊断析构
  失败映射为 0 不同。新测试最初尚未走到的这项错误预期已据运行时源码纠正，
  最初析构异常及后续清理异常的断言没有放宽。
- 非空 map/object 构造插入新增独立 constructionErrorAbi。v3 快扫完整执行后是
  492/495，通过数不冒充全绿；3 个失败为 cgen_function_named_value、
  cgen_global_shared_publish、stdlib_error_paths，均暴露新失败边上的临时拥有者
  缺口。v4 将函数值可变参数容器改用共用的早期登记，并将动态容器及 persistence
  参数接入求值清理事务；上述 3 项均重新生成成功，具名函数值严格 C 实际运行通过。
- v4 owner contract 退出 0：337 RuntimeOps、1,114 消费输入、537 拒绝变异、
  46 个 CFG 检查。变异数变化来自新增失败边拆分基本块，既有测试未删除。
- 静态合同通过；迁移接线为 129 rows、374 representatives、470 candidates、
  144 legacy differentials、83 XLIR-only。接线计数不代表新执行的运行时数量。

### 本轮仍在验证与未完成的范围

v4 的 495/495 快速扫描已退出 0；五组运行回归（存储事务、条件存储、成员赋值、
条件索引、async 终态）也已整体退出 0，均包含各自严格 C 与 C-host JIT/AOT。
存储事务为 2,400 次执行、89,500 平衡分配；条件存储为 7,200 同步、3,600 async、
2,400 generator、137,600 平衡分配；成员赋值为 6,700 次执行、191,600 平衡分配；
条件索引为 7,300 次执行、490,400 平衡分配；async 终态含 349 拒绝变异。
两个较慢的 task API 生成均已退出 0，目前合计 497/497 生成通过；该数字只统计
生成，不等同于全量运行时验收。五组回归、源码与产物哈希保存在
build/tmp/xlir_frame_loan_v4_source_snapshot.json；具名函数值及共享全局用例的
严格 C 实际运行另已通过，stdlib_error_paths 本轮仅重新验证生成。
帧借用的非私有来源、所有捕获/
逃逸、CELL_NEW/closure 等完整构造失败、其余 callable/Future/generator/native
边界、DLL 安全卸载、正式发布与 legacy 清退仍需继续。外部 XRT 非末引用释放
失败仍保留且待确认修复范围，标准库继续只做最低必要基线。

## 2026-09-09：generator 借用边界与完整帧清理结果（frame-boundary-v5）

编译器源码 357 项，SHA-256：
`F149F04D1203C7FEAF52C35019DD9017C389EE1F37C66C1F343935CA6E87329A`。
相对 frame-loan-v4 只有两个编译器文件变化：统一 lifetime 规则增加 YIELD
观察边界，async/generator 共用的 activation cleanup 完整传播失败位。
Cgen 一个文件增加 247 字节，没有新增平行发射器、语义追溯或外部 XRT 修改。
上一 v4 的 77 个冻结 build 产物重新核对，0 差异；发布 CLI/DLL 仍未更新。

### 负对照与语义修正

- 原 lifetime full-module 测试第二项变异曾被接受。检查发现它线性扫描异常分支
  的 RELEASE，又把替换后的最新 LOAD 选为“旧借用”，在成功 CFG 路径上其实合法。
  当前改为同一基本块选取原始 owner、首次旧借用和已转移旧 owner，保留原有
  拒绝断言并增加第三项；没有通过改变合法程序或放宽 verifier 来消除失败。
- 独立旧源码探针在 loan graph 10 将跨 YIELD 的 global 借用判为有效，退出 3。
  原始二进制与日志保留于 xlir_frame_boundary_v2_probe.exe 和
  build/tmp/xlir_frame_boundary_v2_yield_negative.log。YIELD 现在进入统一观察边界，
  私有正例以及 global、extern-global、cell 反例通过。
- 新实际 generator 运行探针发现：kind=1、stop=0、无诊断析构失败时 result=3，
  预期 103，drops=1、catch=0、finally=1，退出 3。帧清理只恢复 error 指针而丢掉
  outcome.ok。负对照 xlir_frame_boundary_v4_generator_runtime.exe 及其同名前缀
  runtime.log 保留；当前共用 activation cleanup 将完整失败结果传递到包围 scope。

### 本轮当前源码实际通过

- 新 check_xlir_generator_frame_loan.ps1 接入 core 03 及静态合同，命令 7495
  退出 0：4 处合法 LOAD 复用、2 个受保护帧借用、20 项 C/header 输出前拒绝，
  其中 2 项共享借用跨 yield。基线和复用版各 3,600 次实际执行、39,701 次平衡
  分配；string/object 参数、共享更新、自然耗尽、三个 yield 后提前关闭、诊断
  与无诊断析构失败、精确析构/finally 次数通过。两版各自严格 C 与 C-host
  JIT/AOT 通过；复用版 C/header/JSON 确定性通过。不是新增通用 CSE pass。
- product-await 门禁 33781 退出 0：4 loans、2 await、51 IR 变异、2 Typed 构造
  效果反例；2,400 执行、2,800 真实挂起、113,903 平衡分配，严格 C/C-host JIT/AOT
  与重复输出均通过。
- 三组回归命令 32474 整体退出 0：owned activation 为 8 consuming captures、
  18 worker releases、52 变异、44,300 平衡分配与 1,100 真实分配失败；async
  terminal 为 11 await、40 resume entries、349 变异及运行/内存断言；value
  lifetime 为 14 full-module 变异、19 CFG 检查、2,200 真实挂起读写、600 求值
  成功/失败用例、178,200 平衡分配，模块 finalization 恢复初始基线。三门各自
  严格 C/C-host JIT/AOT 通过，不将 smoke 输出重复计为内存专项数量。
- 当前 source owner driver 的 no-args 契约已完成并进入扩大生成扫描：337
  RuntimeOps、1,114 consuming inputs、537 变异、46 CFG，与 v4 数量一致。
- 静态合同、290 source wiring、6,017 编码文件、329 markdown、129 semantic
  rows/374 representatives/471 candidates、144 legacy differentials/83 XLIR-only、
  Core 603 entry fixtures/11 packages 通过；这些不是本轮新增运行次数。

### 仍在运行与验收边界

当前 496/496 快速生成已完成，命令 90399 退出 0，失败 0；两项慢速 task API
命令 48718 与 61339 也均退出 0，合计 498/498 生成通过，仍不等于全量运行时
验收。逐项终态见 build/tmp/xlir_frame_boundary_v5_source_snapshot.json。
本轮没有证明所有非私有/投影/捕获借用、SSA owner 别名、CELL_NEW/closure 构造
失败或 Future 最终载荷；外部 XRT 非末引用释放、DLL 安全卸载、最终发布准入和
legacy 清退仍未完成，标准库仍只保持最低必要基线。

## 2026-09-09：owner-views 反例与新增阻断门禁（首次发现时的负对照）

frame-boundary-v5 的两项慢速生成现均已退出 0，498/498 生成已闭合；对应快照
包含 446 项哈希记录。随后在同一 F149F04D 源码上构造了一个合法的 Lowered
XLIR owner region：同一 owned string 的两个透明 VALUE 视图跨第一个 yield
存活，第二个 yield 经视图消费该 owner 一次。测试更新函数内 ValueId 范围并
重建机器，完整 module verifier 接受，C/header/JSON 生成也接受。

实际负对照 build/tmp/xlir_frame_alias_v3_runtime.log：提前关闭时
double-free=1、invalid-free=0、live=0、allocations=5、frees=5，命令退出 3。
这说明分配/释放总量平衡并不能替代重复释放断言。对应的 source-built probe、
生成 C/header/JSON、严格 C runtime EXE 均保留在 xlir_frame_alias_v3 前缀下。
生成 C 将同一指针写入两个 MOVE_OWNED 字段，各自拥有 init bit 与析构；两者的
takeInstruction 都是 SIZE_MAX，因为消费的是后续透明视图。

已建立永久 xlir_continuation_owner_views fixture/contract/runtime/host 与
check_xlir_continuation_owner_views.ps1，接入 core 03 和静态接线。契约用明确的
两次 yield IR 程序分别覆盖单视图和多视图，宿主入口根据验证后机器的 symbol
生成绑定，不硬编码 SymbolId。此测试不是通用 CSE，也不声称源级优化等价。
新门禁当前失败：原始自然耗尽探针 79206 因 generator next 报错而退出 1；
提前关闭的重复释放负例仍保留，不通过删断言、漏跑、额外 retain 或 Cgen 名称
补丁来消除失败。原有 498 项生成通过不能覆盖这一真实运行时缺口。

永久门禁随后按提前关闭优先执行，命令 3341 已退出 1：stop=1、double-free=1、
invalid-free=0，保存于 build/tmp/xlir_frame_alias_v5_gate_negative.log。严格 C
编译成功，运行断言失败后立即停止，没有运行不安全的 JIT/AOT。静态 IR 接线、
Core 607 entry fixtures/11 packages 和 329 markdown 链接检查通过，这些不代表
运行时通过。上一 v5 的 74 个冻结 build 文件重新核对，0 差异。

下一步为物理 owner、SSA 恢复投影与按 CFG 的消费动作分离建模；设计文档已
列出状态路径及交付失败约束。此处记录首次发现时的状态，后续基础修复见下节；
冻结负对照不能被覆盖或当作通过记录。

## 2026-09-09：owner-view 物理驻留与消费动作基础回归

当前源码为 358 个源文件，SHA256：
C0B96500B864A16DF13CE4B32C0841B3486436DD35C9FAD34F5EA0BD7F9F5774。
这不是 release DLL 的哈希；release CLI/DLL 与外部 XRT 均未修改。

- continuation frame 按 managed canonical owner 合并物理槽，live membership
  独立记录 SSA view，PROJECT 明确目的 ValueId。删除 frame.takeInstruction，
  改为完整 transfer action slice，明确指令、输入视图及 YIELD 提交时机。
- 每个实际到达的 suspension 都有 resident spill/install 动作，不再只在编号
  最小的 state 写入。Cgen 只执行动作，不追溯 VALUE 别名或源 AST/HIR。
- async/generator 重复的活跃性求解移入共同的指令/CFG 数据流实现；verifier
  重新计算 live-set，不能仅依赖已提供的 membership 和 action 相互自洽。
- owner-views 门禁 76979 退出 0：单视图与双视图均一个 MOVE_OWNED 槽；分别
  20/26 项非法 IR 在 C/header 输出前拒绝。双视图含“删除 alias membership
  和 project、同步修复所有切片”反例，专门要求 CFG liveness 诊断。两版各
  300 次运行、1,500 次平衡分配，double-free/invalid-free/use-after-free
  断言、自然耗尽及两处提前关闭、严格 C/C-host JIT/AOT、C/H/JSON 确定性通过。
- generator frame loan 门禁 70396、product await 门禁 52227 均退出 0；前者
  原始/复用版各 3,600 执行、39,701 平衡分配、20 变异，后者 2,400 执行、
  2,800 真挂起、113,903 平衡分配、51 IR 和 2 Typed 反例，与冻结前基线一致。
- owned activation、async terminal、value lifetime 三门全部退出 0：捕获门
  52 变异、1,100 真分配失败、44,300 平衡分配；异步终态 11 awaits、40 entries、
  367 变异（包含新 action/live identity 反例）；生命周期门 14 module 变异、
  19 CFG、2,200 挂起读写与 600 求值路径、178,200 平衡分配。各门严格 C、
  C-host JIT/AOT 与确定性断言通过，不将 smoke 重复计入运行数量。

### CFG 扩展、生成终态及代码收缩

- owner-cfg 门禁 95916 退出 0：四个有效 generator CFG 覆盖互斥消费、跳过
  首个 state、循环重新获取和恢复后的 STORE 所有权移交；五条消费边与五处
  resident install，15 项非法 IR 拒绝。3,500 次执行、20,200 次平衡分配，
  每个 yield 的提前关闭、严格 C/C-host JIT/AOT、C/H/JSON/binding 确定性通过。
- 测试共享 helper 提取后，owner-views 门禁 86119 重新退出 0；20/26 变异、
  两版各 300 执行和 1,500 平衡分配不变，并增加 binding 确定性比较。
- 当前 owner driver 4053 退出 0：337 RuntimeOps、1,114 consuming inputs、
  537 变异、46 CFG。快速生成命令 9883 为 497/497，新增 owner-cfg scaffold
  单独生成通过；两项大型 task API 命令 25179 和 98364 均退出 0，合计本轮
  500/500 生成通过。这不是 500 个完整运行时用例。
- 静态检查 87544 退出 0：291 source wiring、6,030 编码文件、329 markdown、
  129 semantic rows/374 representatives/473 candidates、144 legacy differentials/
  83 XLIR-only、Core 611 entry fixtures/11 packages。这些是接线而非运行次数。
- 相比 frame-boundary-v5，同一源码范围 Cgen 减少 1,991 字节，IR 现有文件
  减少 34,965 字节，新增共享活跃性实现 9,030 字节；IR 净减 25,935 字节，
  IR+Cgen 合计净减 27,926 字节。未重建 release，不能据此推算 DLL 体积变化。

本轮仍不证明全部分支/循环 owner-view、async 多别名和单指令多个 consuming
输入，也不证明 acquired borrow 重新激活与失败处理。其他构造/逃逸、Future
最终清理、外部 XRT 缺陷、DLL 安全卸载、完整发布准入与 legacy 清退仍未完成。
冻结记录为 build/tmp/xlir_owner_views_v4_source_snapshot.json；不覆盖历史负对照。

## 2026-09-09：async 多视图、单 CALL 多 owner 与显式 activation handoff

上一 owner-views-v4 检查点保持冻结。当前修正源码为 358 文件，SHA256：
7D9FD1B43ED4CECEF8AC6E7BA69DBC67ADBC62FD15E2EECC5C75FB08F3715E5A。
新增 owner-calls fixture/contract/runtime/host/gate 接入 core 03 与静态合同。
契约将源程序中 byval 参数的临时 STORE/LOAD/MOVE staging 改为明确的纯 SSA
owner/view 区域，保持已有 consuming CALL ABI，先通过完整 module verifier。
这不是源码优化等价性测试，也没有修改生产 lowering 来专门生成这些形状。

### 两项真实负对照

1. C0B96500 源码的 async 准入列表遗漏已经实现的消费动作。有效 IR 被 Cgen
   以 `XLIR async frame action is not admitted` 拒绝，退出 3。源构建探针与 JSON
   保留于 xlir_owner_calls_v1_negative 前缀和对应 negative_snapshot.json。
2. 补齐准入后的 4B048191 源码在两个 pending await 后返回 30，预期 35。
   生成 C 把同一个 f0 连续搬运两次，第二次将已清空的旧 f0 覆盖到新 activation。
   完整 IR 与严格 C 编译通过，运行失败；14 项负对照产物哈希保存在
   build/tmp/xlir_owner_calls_v4_negative_snapshot.json。没有运行失败产物的 JIT/AOT。

第一项由显式准入已有命令修正；第二项新增每 suspension 的 HANDOFF_RESIDENT
物理槽程序，构造/验证按唯一 frameSlot 枚举，Cgen 不再把活跃视图列表当作搬运
命令。动作、切片及遗漏均在 IR 验证阶段检查，不在 Cgen 做 SSA 追溯或去重。

### 当前已通过及待闭合项

- owner-calls 门禁 36252 退出 0：三个 owner 区域、六条消费动作、94 个非法 IR
  反例；包含删除单 CALL 的一个消费动作后同步修正索引、删除 handoff 并修正
  切片、错误提交阶段、错误 operand/view、双输入重复消费同一 owner 等。
- 一个 async 在两个 await 间保持同一 owner 的两个视图，并在互斥分支经不同
  视图消费；另外两组 generator/async 在单条 CALL 同时消费两个 owner。
  8,500 次执行、7,200 次真实挂起、116,502 次平衡分配、double-free/invalid-free/
  use-after-free 检查通过。ready/pending 与两次输入的四终态组合、消费调用
  失败、generator 提前关闭、严格 C/C-host JIT/AOT、C/H/JSON/binding 确定性通过。
- 7D9FD1B4 源码的其余七门已完成（49339 退出 0）：owner views/CFG、generator
  frame loan、product await、owned activation、async terminal 和 value lifetime。
  terminal 现拒绝 376 项非法变异；activation 包含 1,100 次实际分配失败。
- 独立 owner 合同 36548 退出 0：337 RuntimeOps、1,114 个消费输入、537 项拒绝、
  46 项 CFG 检查。静态门 13081 退出 0：291 源接线、6,035 文本、329 Markdown，
  129 语义行、374 代表、474 生成候选、144 差分接线、83 XLIR-only 接线，
  Core 614 C/H fixtures 和 11 packages；这些静态数量不作为运行通过数量。
- 扩大生成已完整结束：499 项快速检查（12085）、cgen_task_api（84890）和
  release/examples/task_api（43137）均退出 0，共 501 项生成通过；不是 501 项
  运行验收。旧 4B048191 源码生成记录不计入本次。冻结记录为
  build/tmp/xlir_owner_calls_v6_source_snapshot.json，保留新 gate 的精确测试副本。

仍未完成 acquired borrow 的重新激活/失败、非私有/投影/捕获借用、detached
LOAD/MOVE、构造/逃逸分配失败、Future 最终载荷、外部 XRT 非末引用释放、DLL
安全卸载、最终发布准入和 legacy 清退。release CLI/DLL 与外部 XRT 均未修改。

## 2026-09-09：挂起准备失败的 CFG 与物理槽回滚

owner-calls-v6 已冻结为 542 项哈希记录、501 项生成、八个专项，前代 114 项
build 记录和 14 项负对照未变化。后续分配故障扩展发现新的真实语言错误：
新帧和 continuation 注册失败直接关闭 Future，源码 catch 没有执行，内存错误
残留 TLS。40 组探针中 36 组实际触发错误终态，所有组内存平衡；探针也包含最终
返回载荷分配失败，该边界仍未修复。负对照保存在
build/tmp/xlir_owner_handoff_v1_negative_snapshot.json。

当前源码仍为 358 文件，SHA256：
139C514B5440326D788293B62B737C1B7DA67E5F8A2BBB6DE1F0867BE2671DD5。

- 每个 suspension 的 setupFailureEntry 必须指向它自己的 RESUME_FAILURE。
  emitter 遵循该边进入原有 cleanup/ERROR_TEST/catch CFG，不直接关闭结果。
- 新增反序 ROLLBACK_HANDOFF_RESIDENT 动作及精确切片；未受理的 next activation
  按该动作归还物理槽、初始化位和 outer Promise 所有权，然后在当前帧继续执行。
  不新增基于 AST、SSA 视图搜寻或运行时别名猜测的修补分支。
- owner-calls 扩展至三次 await，35245 退出 0：206 项非法 IR、8,500 次常规
  执行、7,200 次常规真实挂起、3,840 次真实分配故障；常规与故障矩阵合计
  205,462 次平衡分配。普通/嵌套 outer 的四个准备故障点、ready/pending、
  两个输入四终态和两个消费分支均检查恢复后的实际值与无残留 TLS 错误。
  严格 C/C-host JIT/AOT、C/H/JSON/binding 确定性通过。
- 其余七个专项 50895 退出 0：owner views、owner CFG、generator frame loan、
  product await、owned activation、async terminal、value lifetime；terminal 现拒绝
  385 项非法变异。独立 owner 合同 42487 退出 0：337 RuntimeOps、1,114 消费
  输入、537 项非法变异、46 CFG 检查。
- 静态检查 53074 退出 0：291 源接线、6,035 文本、329 Markdown，129 语义行、
  374 代表、474 生成候选、144 legacy 差分接线、83 XLIR-only 接线、614 Core
  C/H fixtures 和 11 packages；接线数量不作为运行数量。
- 同源完整生成检查使用 owner-handoff-v4 driver（23249 构建退出 0）。扩大
  501 项生成正在复验，不能沿用 owner-calls-v6 的结果充当本节结果。
  首次构建的 owner-handoff-v3 driver 仅支持 C/JSON，未用于完整生成统计。
- 运行证据先冻结于 build/tmp/xlir_owner_handoff_v4_partial_snapshot.json，
  明确保留 36178 / 58035 / 98295 三个生成会话待结束，不将其写为通过。
  相对 owner-calls-v6，Cgen 源码净增 101 bytes，IR 净增 2,372 bytes；
  release CLI/DLL 未重建。

仍须完成：最终返回载荷的错误终态、所有 acquired borrowed spill 失败和重新
激活、非私有/捕获/投影借用、detached LOAD/MOVE、构造/逃逸失败、无诊断失败
与更广 finally 组合、Future 最终载荷和 DLL/API 安全卸载、完整发布验收及
legacy 清退。外部 XRT 非末次引用释放缺陷未改动；标准库只保持最低必要范围。

## 2026-09-09：最终返回载荷失败与统一提交协议

前代 owner-handoff-v4 的三个生成会话已全部退出 0，共 501 项生成；完整记录为
build/tmp/xlir_owner_handoff_v4_source_snapshot.json（533 项记录），原 partial
记录保持不变。归档时核对其 117 项 frozen build 记录无变化。该记录只证明
139C514B 源码，不包含本节后续修改。

当前源码 358 文件，SHA256：
933225A729759F2751B79B4E99BA2CC03D88407A1809B6CE994A4D241ABFF5B6。
原始标量负对照 build/tmp/xlir_async_payload_v1_negative_snapshot.json 保留：
18 次分配释放平衡，但最终结果盒分配失败得到 CLOSED、FutureError=0、TLS=1。

- 新增 machine.resultCommitAbi 与独立 verifier 检查、文本/JSON 导出。
  Cgen 的所有 machine RETURN 共用提交/回滚程序，按 AsyncResultAbi 消费销毁
  规则，不再按 worker C 类型重复分支。结果盒/提交失败在已有 Promise 上发布
  原始错误；只有 Promise 尚未建立时才回滚并向调用方返回启动失败。
- 回滚隔离首次错误与生产者错误作用域。后续析构即使报告不同诊断或只有失败位，
  也不能替换 MemoryError；Future 失败不污染正在执行 callback 的外部调用。
- owner-calls 43892 退出 0：208 项非法 IR、8,500 次常规执行、7,200 次常规
  真实挂起、3,840 次准备分配故障、1,600 次结果盒分配故障，合计 234,262 次
  分配释放平衡；严格 C、C-host JIT/AOT 和确定性输出通过。
- 新 result-commit 专项初轮 36530 退出 0：五种合法载荷，2,265 次执行、2,250
  次实际分配失败、1,350 个失败 Future、900 次创建 Promise 前的启动失败，
  23,702 次分配释放平衡；early/ready/pending、raising/no-diagnostic destructor、
  生产者作用域隔离、严格 C、JIT/AOT 与确定性通过。随后增加未知 commit ABI、
  错误 payload ABI 拒绝和精确 3/1 次 post-arm 分配断言，79941 再次退出 0，
  拒绝 15 项非法 IR，已接入 Core 03。
- 其余七门 89430 退出 0，合计本节九个运行专项通过；terminal 保持 385 项
  非法变异。独立 owner 合同 57434 退出 0（337 RuntimeOps、1,114 消费输入、
  537 项非法变异、46 CFG 检查）。静态门 79854 退出 0：291 源接线、6,040
  文本、329 Markdown、129 语义行、374 代表、475 生成候选、144 差分接线、
  83 XLIR-only 接线，Core 617 C/H fixtures、11 packages。
- 相对 139C514B，Cgen 净减少 4,142 bytes，IR 增加 1,214 bytes，总计减少
  2,928 bytes。release CLI/DLL 和外部 XRT 未修改。

五种源码载荷是 int/string/class/closure/error。最初 test scaffold 的 extern
error 返回和直接 async future 返回被既有语言规则拒绝，未生成 C、未运行；
error 改为正常 catch 取得的语言值，直接 async future 返回保留既有
sem_async_return_future 负例。没有为了凑齐六种 ABI 而放宽语义规则。
嵌套 Future 的 task/callable worker 提交、无挂起 ready worker、返回完成后的帧
析构失败、完整 Future 生命周期、acquired borrow/构造/逃逸故障以及最终 CLI/DLL
发布和 legacy 清退仍未完成。新源码快速 500 项（19206）和 cgen_task_api
（43754）均退出 0，已通过 501/502；36576 task_api 仍运行，不能记为通过。
这些是新源码的回执，没有复用前代 501 项生成记录；已完成的
运行证据先冻结于 build/tmp/xlir_async_payload_v7_partial_snapshot.json。

### 下一项真实负对照：帧最后引用释放晚于 Future 发布

当前 933225A7 源码已另行生成并严格运行一个普通借用 class 参数的 async 函数。
调用方在挂起后释放自己的引用，帧持有最后引用。正常析构时 Future 为 RESOLVED
17，生产者作用域成功；析构报告诊断 93 或只有失败位时，Future 仍已成功为 17，
但生产者作用域失败。三个对照退出 0 / 1 / 1，分别 14 / 15 / 14 次分配释放平衡。
这与本节已修正的“未接管结果盒回滚”不同，证明还必须将 activation 清理结果
纳入 Future 最终发布屏障，不能以结果盒专项通过宣称完成所有 async 生命周期。
源码、生成 C/H/JSON、严格探针和回执的 10 项哈希记录保存在
build/tmp/xlir_async_frame_completion_v1_negative_snapshot.json，后续修正需保留负对照。

## 2026-09-09：Future 发布前的 activation 清理屏障

上一 async-payload-v7 的 task_api 会话 36576 已退出 0（回执 ff2bec），因此
933225A7 源码完整 502 项生成通过。完整冻结记录为
build/tmp/xlir_async_payload_v7_source_snapshot.json，共 557 项哈希记录；原
partial 与十项帧完成负对照保持不变，不把新源码结果混入旧检查点。

当前源码 358 文件，SHA256：
2EF33CA254D6C0EDF0A9E513087D16DBE9A19604D6E8026EE09966DFF2F9DEFF。

- 新增 machine.completionAbi=RETIRE_BEFORE_PUBLISH，构造、独立 verifier、
  文本/JSON 和 Cgen 准入共同固定。RETURN/THROW 先执行已验证 destroy-action
  程序并合并完整 (ok,error)，再发布 Future；frame/outer 的物理释放仍在之后。
  所有权初始化位在析构前清空，运行时晚到的 frame-free 不会重复析构。
- 原始错误与无诊断失败位优先于后续析构；帧析构使成功返回失败时，未提交的
  返回对象先回滚。XRT 不接受 NULL rejection error，因此最终传输边界用静态
  INTERNAL 表示无诊断失败；真正 OOM 仍为 MemoryError，不新增诊断分配依赖。
- 新 frame-completion 专项 73349 退出 0：9 项 C/header 输出前拒绝变异、
  4,800 次执行、1,440 次发布观察、1,200 次真实分配失败（其中 480 次建立
  Promise 前启动失败）、154,960 次分配释放平衡。两个帧参数与拥有型返回对象
  的精确逆序/回滚析构、诊断/无诊断首次失败、RETURN/THROW/cancel/closed、
  early/ready/pending、生产者隔离、严格 C、C-host JIT/AOT 与确定性通过。
- result-commit 71898 与 owner-calls 90978 均再次退出 0：前者 2,250 真分配
  失败、23,702 平衡分配；后者 3,840 准备故障、1,600 结果盒故障、8,500 常规
  执行、7,200 真挂起、234,262 平衡分配。七个回归门 7688 全部退出 0，合计
  当前十个运行专项通过，未借用前代编译结果。
- 相对 933225A7，Cgen 增加 2,348 bytes，IR 增加 1,400 bytes；本节新增的是
  显式完成语义及共享执行过程，不是 DLL 大小测量。release CLI/DLL 与外部 XRT
  未修改。503 项同源生成、独立 owner 合同与静态检查仍须收集完整终态回执。

初次 fixture 的未限定字段名修正为既有 self.id 语法；随后补齐测试宿主的
FUTURE_CONTINUE 模块声明并使用不接管数据的 Continue 观察者。两次失败均在
生成/严格编译门停止，没有执行不合格产物的 JIT/AOT，也没有放宽生产语义。

本节没有证明外部主动取消跳过 callback 的 teardown、无挂起/任务池 worker、
嵌套 Future 载荷、完整 Future/code-lease 生命周期；acquired borrow 重新激活、
投影/捕获借用、构造/逃逸失败、外部 XRT 非末次引用释放、完整 CLI/DLL 发布验收
与 legacy 清退仍未完成。标准库只保持语言验收所需最低基线。

### 静态与生成回执、主动取消的独立负对照

- 静态门旧断言要求 async_instruction 直接调用 future_complete_error；如今
  该调用已由共享 result_fail 统一执行。断言改为同时检查 caller→共享 helper
  和 helper→四终态映射，保留既有运行门与新增发布屏障门。77968 退出 0：
  291 源接线、6,045 文本、329 Markdown、129 语义行、374 代表、476 生成候选、
  144 legacy 差分与 83 XLIR-only 接线，Core 620 C/H fixtures、11 packages。
- 独立 owner 合同 7974 退出 0：337 RuntimeOps、1,114 消费输入、537 非法变异、
  46 CFG 检查。快速生成 99610 退出 0，501/501；30573 / 62664 两个大型生成
  会话待结束，当前只能记录 501/503，不将前代 502 项结果混入本节。
- 另以同一 2EF33CA2 源驱动生成主动取消探针：先请求取消输出，再完成输入。
  runtime 跳过 resume callback，随后执行 frame-free。成功析构的对照退出 0；
  析构诊断 93 / 无诊断失败两组退出 1，输出均为 CANCELLED，但生产者作用域
  被错误标为失败。三组 13 / 14 / 13 次分配释放均平衡，证明不是泄漏，也不是
  已修复 RETURN/THROW 的同一路径。11 项不可变哈希保存在
  build/tmp/xlir_async_cancel_completion_v1_negative_snapshot.json；未执行失败
  产物的 JIT/AOT。这是下一项应纳入 XLIR cancellation/teardown 合同的真实缺口。

## 2026-09-09：取消恢复 Watch 与拥有引用移交

前代 frame-completion-v5 的两个大型生成均已退出 0，完整 503 项生成记录冻结于
build/tmp/xlir_async_frame_completion_v5_source_snapshot.json（566 项哈希记录）。
原 partial 保持不变；这些回执属于 2EF33CA2，不是以下源码的验收。

当前源码 358 文件，SHA256：
A34B218A7316EACA8A1F23197642D28381B9D44FC43388E52DA876FE11CDB8FC。

- machine.transportAbi=FUTURE_WATCH 与 cancellationAbi=AWAIT_ONCE 由构造、
  verifier、文本/JSON 和 Cgen 准入共同固定。首次挂起建立逻辑输出 Promise，
  后续帧移交同一 Promise；内嵌 Watch 在 READY/PENDING 都调用规范 resume。
  取消作为一次 CANCELLED 输入进入既有 typed catch/rethrow/finally，不再由
  通用 Continue 适配器跳过语言回调。finally 再次 await 不会重复收到同一次
  取消；自动清理保护主异常，显式 finally 仍可覆盖它。
- 中间 2876FFB8 源码的取消矩阵揭示另一处真实缺口：读活跃性已消失但仍持有的
  owner 没被 handoff，旧帧在 finally 挂起时析构并以错误 92 污染生产者。
  生成/严格编译通过，严格运行失败；12 项源码、C/H/JSON、合同/运行二进制和
  日志副本冻结于 build/tmp/xlir_async_cancel_watch_v2_negative_snapshot.json。
  最初测试还错误地把“挂起时不得提前析构”当作参数读取寿命保证；移除该无依据
  的时序假设后，保留生产者隔离断言，真实错误仍复现。未将失败产物用于 JIT/AOT。
- 修正落在 IR 动作构造：所有 managed resident 按 init bit 移交其拥有责任，
  trivial 字段按 live membership 移交，反序动作精确回滚。没有在 Cgen 补充
  类型追溯、隐式保活或推导析构条件；RELEASE/消费/完成计划才可结束拥有责任。
- 新 cancel-watch 93614 退出 0：8 个不再读取但仍须移交的 owner、14 项
  C/header 输出前拒绝变异，包含一致删除该 owner 的 handoff 并修复切片。
  11,520 次执行、6,840 次取消请求、11,520 次发布观察、319,367 次分配释放
  平衡；前/后挂起取消、四种输入终态、finally 再次挂起、处理/重抛/覆盖取消、
  诊断和无诊断清理失败、生产者隔离、严格 C/C-host JIT/AOT、确定性通过。
- owner-calls 90660 退出 0：216 项拒绝变异、8,500 常规执行、7,200 真挂起、
  960 次真实准备分配失败、1,600 次结果盒分配失败。另 640 组空 Future 场景
  各触发两次真实注册拒绝/动作回滚，最终 catch 结果正确。合计 130,262 次
  分配释放平衡。960 不是削减故障点：Watch 复用 Promise 且不分配 node，
  每次 handoff 实际分配已由四次变为一次；不同矩阵的总分配数不能直接当作
  性能比较。公共 test helper 提取一致删除动作实现，不削弱反例。
- 相对 2EF33CA2，Cgen 增加 3,087 bytes，IR 增加 2,250 bytes。release CLI/DLL
  和外部 XRT 均未重建或修改。其余九个专项、504 项同源生成、独立 owner 合同
  与静态门正在复验；旧位置的 Continue/outer-ref 字符串断言已改为 Watch、
  唯一 Promise 与取消投递位的接线，不能据此宣称对应旧 CLI 套件运行通过。

当前协议是协作取消，不保证未完成输入立即唤醒，也不会取消共享输入生产者。
仍须压实：完整传输源 Future/载荷最终清理、borrow 重新激活/失败、复杂借用、
构造/逃逸、任务/无挂起 worker、跨线程取消竞争、外部 XRT 非末次引用释放、
DLL/API 安全卸载、最终发布验收和 legacy 清退；标准库仍只做最低必要基线。

### Watch 检查点的完整同源回执

A34B218A 源码未再修改。cancel-watch 最终增加生产者原本成功、已有诊断失败、
已有无诊断失败三种环境：73371 退出 0，34,560 次执行、20,520 次取消请求、
34,560 次发布观察、980,417 次分配释放平衡。14 项输出前拒绝变异以及严格 C、
C-host JIT/AOT、C/H/JSON/binding 确定性均通过；原 11,520 矩阵为被加强的旧记录，
不能与最终执行数叠加。原主动取消负对照在当前驱动下三组均退出 0，生产者不再
受帧析构失败污染，12/13/12 次分配释放平衡。

owner-calls 90660 与九个回归门 94551 均退出 0，合计十一项同源运行专项。
其中 frame-completion 仍有 4,800 次执行、1,200 次真实分配失败，当前 152,080
次平衡分配；result-commit 仍有 2,250 次真实分配失败，当前 22,792 次平衡分配。
独立 owner 18356 退出 0：337 RuntimeOps、1,114 消费输入、537 非法变异、46 CFG。
静态 30241 退出 0：291 源接线、6,050 文本、329 Markdown、129 语义行、374 代表、
477 生成候选、144 legacy 差分与 83 XLIR-only 接线、Core 623 fixtures、11 packages。
这些接线数字不是本轮端到端运行通过数。

快速生成 71397 完成 502/502；cgen_task_api 67633 与 task_api 90912 分别退出 0
（回执 66c180、3155c2）。因此完整同源生成是 504/504，不是 504 项运行验收。
完整记录冻结为 build/tmp/xlir_async_cancel_watch_v5_source_snapshot.json；前代
2EF33CA2 检查点及两个取消负对照保持原样。release CLI/DLL 未更新；本检查点
不证明上述未完成生命周期、全量 CLI/DLL 发布准入或 legacy 清退已完成。

## 2026-09-09：await 源租约收口与扩大验收发现的下一组缺口

A34B218A 完整检查点已实际冻结：656 项记录，155 项前代不可变 build 哈希与
23 项取消负对照哈希复核通过。随后唯一拥有的 ready Future/BYVAL 反例复现
载荷析构在成功发布后污染启动方：正常/诊断93/无诊断失败退出 0/1/1，均无泄漏。
10 项不可变记录为 build/tmp/xlir_async_source_retirement_v1_negative_snapshot.json。

sourceLeaseAbi=CAPTURE_THEN_RELEASE 修正该传输寿命；三组对照现在均退出 0，
成功值17/FAILED诊断93/FAILED静态INTERNAL，启动方均成功，7/7/6 次分配释放平衡。
中间源码09EB8C8F又暴露临时 Future 的裸 RELEASE 绕过完整失败事务；12 项负对照
冻结于 build/tmp/xlir_async_source_lease_v2_negative_snapshot.json。修正复用共用
evaluation cleanup lowering，并未给 Cgen 增加表达式追溯。

当前358文件源码 SHA256：
A2B804DBB1BDCC91983B8FBC37853ABA73E32D524AE05267076319C1BE8F4345。

- source-lease 常规矩阵78346退出0：8个machine、4条await操作数清理事务、19项
  C/header输出前拒绝变异；23,328次执行/发布观察、239,185次分配释放平衡。
  唯一/共享ready与pending、四种输入终态、字符串/对象/嵌套Future取得和回滚、
  body/finally/载荷析构失败、三种环境错误状态、严格C/C-host JIT/AOT/确定性通过。
- 其余11个运行专项61990退出0；独立owner30746退出0（337/1114/537/46）。
  静态62515退出0：291源、6055文本、329Markdown、129语义行、374代表、478候选、
  144差分/83XLIR-only接线、Core626 fixtures、11packages。接线不算运行通过数。
- 完整生成505项已全部结束：快速17881为502/503，cgen_task_api68220退出0，
  task_api74157退出1，因此当前实际是503/505。两项失败均为ERROR_RAISE的消息
  缺少拥有型ABI；最小7行throw-await反例也退出1。不能借用前代504项通过报告。
- 进一步真实分配故障专项96103退出1：字符串两种源形式各6次正常启动分配；
  首个反例是BYVAL字符串第4号分配失败，body/finally均执行，输出仍RESOLVED。
  严格探针回执2361de显示未检查的返回字符串复制失败；没有运行该失败产物的
  JIT/AOT，也未将此失败统计为分配故障门通过。新专项不是完整绿灯。
- 相对A34B218A，Cgen增加2482 bytes，IR增加2044 bytes，合计4526；这不是DLL
  体积测量。release CLI/DLL与XRT均未更新。部分验收记录保存在
  build/tmp/xlir_async_source_lease_v3_partial_snapshot.json。

下一步先压实显式存储取出所有权和可失败复制/retain的IR控制流，修复以上真实
失败后重跑受影响合同、运行专项与完整生成；不降低标准库以外的语言验收标准。

## 2026-09-09：明确取得所有权与字符串分配失败控制流（部分验收）

当前 358 文件源码 SHA256：
8FB639F2D59FD9240A5A37789A19F0ABD65DFEF39A85930DAD6EA4DA3975621F。

- STORAGE_TAKE 已用于 evaluation cleanup 提交；STRING_COPY/NULLABLE_COPY 已有
  独立物理结果与即时失败分支。返回提交前的失败计划、普通函数参数副本的部分
  安装回滚、字符串转换/插值的临时拥有者清理均落在 IR，不增加 Cgen 来源追溯。
- source-lease-v10 会话 67351 退出 0，回执 c3f549：8 个 async machine、4 个
  await 清理事务、80 项 C/header 输出前拒绝变异；23,328 次执行/发布观察，
  221,567 次分配释放平衡。900 次 async 真实分配失败和 300 次普通函数入口
  复制失败全部通过，严格 C、C-host JIT/AOT 与确定性通过。
- 故障测试原先即时分配析构诊断，导致注入击中测试自身；现预先创建诊断并引用。
  每种源形式/析构结果分别测量真实分配数，逐点注入包括最后结果盒；早期异常
  不再被要求触发不存在的后续分配。每次注入仍要求运行时报告实际触发，原来
  index=4 且无析构错误时的成功误报没有被跳过。旧测试与产物冻结于
  build/tmp/xlir_storage_take_v5_before_harness_snapshot.json。
- owner-v10 会话 22778 退出 0，回执 d1a258：337 RuntimeOps、1,152 消费输入、
  512 拒绝变异、47 CFG。合成图现在承认 STRING_COPY 的真实失败边，并保留
  用普通分支伪造失败免除拥有责任的负例；这些数字不与旧指令布局直接比较。
- 快速生成会话 18565 已退出 1，回执 5d51cb，502/503；唯一失败为
  test/cases/xlir_return_commit.xl 的命名适配器仍要求默认参数 no-raise。
  中间版 v6 的 13 项失败有 12 项在当前源码复测通过。两个大型 task API
  生成会话 17714、56902 尚在运行，不能将进行中的结果计入通过。
- 同源 cancel-watch 已通过；其余专项正在复验。owner-calls 的手工改图在
  消除旧调用/复制后必须同步处理结果分支和此前取得的 SSA owner 回滚，目前
  被验证器拒绝；owner-views 还依赖旧的十指令布局。这两项没有运行失败产物
  的 JIT/AOT，也没有登记为运行通过。详见各 xlir_storage_take_v9/v11 日志。

本检查点仍为部分验收，release CLI/DLL 与 XRT 未更新。下一步继续完善命名
适配器可失败默认参数的独立函数边界、更新精确的合成 SSA owner 图，再完成
当前同源回归；其后仍有复杂借用、构造逃逸、任务生命周期、取消竞争、DLL/API
安全卸载、最终发布准入与 legacy 清退。标准库仅维持最低基线。

### 该源码的追加回执

大型 cgen_task_api 会话 17714 已退出 0（680c62），因此已结束的生成是
503/504；task_api 会话 56902 仍实际运行，尚未登记结果。八个常规运行专项
已通过：source-lease、cancel-watch、frame-completion、result-commit、
generator-frame-loan、product-await-owner、owned-activation、terminal-outcome。
另四个专项仍失败：owner-calls 改图缺失新失败路径的 owner 回滚；owner-views
依赖旧十指令布局；owner-cfg 假定 COPY 紧邻 YIELD；value-lifetime 的单基本块
变异选择器未找到跨新 ERROR_TEST 的目标。后两项也没有被算作运行通过。

静态门 71390 已退出 0（7256ba）：291 源接线、6,055 文本、329 Markdown、129
语义行、374 代表、478 候选、144 差分/83 XLIR-only 接线、Core 626 fixtures、
11 packages。字符串转换的旧文本断言已更新为共用 owner 保护及完成事务；这
只是接线检查，不能替代上述失败专项的运行验证。部分记录将冻结在
build/tmp/xlir_storage_take_v13_partial_snapshot.json，后续完整回执须另行补充。

## 2026-09-09：命名默认适配、拼接分配失败与借用测试恢复（部分验收）

本轮 358 文件源码 SHA256：
FCE3664AE6610838C536D38D4F24B6B3275BE61A2C59EC42E202859A2EDB4889。

- 前代 8FB639F2 的 task_api 会话 56902 已退出 0（5ce4e9），其完整生成实际为
  504/505。此回执只属于前代，不将后续适配器修复混入该源码的成绩。
- 当前 driver v2 构建 8895 退出 0（d542b3）；原先 return_commit 生成限制解除。
  拼接负例从第 4 号分配失败仍 ok=true，变为 ok=false/MemoryError/NULL，五个
  分配点全通过（58a822）。返回提交另完成 1,000 轮、80,080 次平衡分配
  （351a61）；这是严格 C 定向运行，不冒充旧 release 驱动的完整返回专项。
- 新 string_allocation_outcome 永久门 2719 退出 0（ec7559）：10 处已检查拼接、
  40 项 C/header 输出前拒绝变异，六类场景共 580 次真实分配失败，包含 BYVAL
  回滚、inout 原子提交及默认参数诊断/无诊断失败。严格 C、C-host JIT/AOT、
  C/header/JSON/binding 确定性通过；已接入 core suite。
- value_lifetime 选择器改为沿函数入口的 ERROR_TEST 成功边和 JUMP 寻找替换，
  不再依赖单基本块，也不把失败分支中的 RELEASE 误认成成功路径失效。79368
  退出 0（582dfc）：14 个模块负例、19 个 CFG 检查、2,200 次真实挂起场景、
  600 次 RHS/address 场景、176,000 次平衡分配，严格 C/JIT/AOT/确定性通过。
- 其余八项运行门同源复验通过：source-lease、cancel-watch、frame-completion、
  result-commit、generator-frame-loan、product-await-owner、owned-activation、
  terminal-outcome。三组会话 47537/5241/31734 均退出 0，回执分别为
  13db46/eaa76f/7f008e。owner 门仍通过 337 RuntimeOps、1,152 消费输入、47 CFG；
  新基本块布局下枚举到 508 个拒绝变异，未放宽所有权验证规则。
- 503 项快速生成 10995 全通过（f8f3b0）；大型 cgen_task_spawn 52537 退出 0
  （79c8fe），task_api 40919 尚在运行。截至该条记录为已完成 504/504，另有
  一项未完成；新永久拼接 fixture 单独通过，不混入旧 505 项集合。
- 静态门 98467 退出 0（a0c576）：291 源接线、6,060 文本、329 Markdown、129
  语义行、374 代表、479 候选、144 差分/83 XLIR-only 接线、Core 629 fixtures、
  11 packages。接线统计不等于运行通过统计。

仍有三项 continuation 合成 owner 图专项未恢复：calls 缺少新增失败边的 owner
回滚，views/cfg 仍假定旧 COPY/YIELD 邻接关系。尚未运行这些失败产物的 JIT/AOT。

扩大故障探针 fae240 退出 3，确认三项真实语义反例：字面量 STRING_DUP 及
I64_TO_STRING 分配失败返回成功空值；插值前缀分配失败成功返回后半段。常规
生成及已通过的拼接门不覆盖这些失败，不能据此宣布字符串分配协议整体完成。
这些反例与当前源码将一并冻结到 build/tmp/xlir_adapter_outcome_v8_partial_snapshot.json。
release CLI/DLL 与 XRT 均未变更；标准库维持最低基线，语言问题继续作为必修项。

### 本轮最终生成与失败专项回执

task_api 会话 40919 已退出 0（06292e）。因此 FCE3664A 源码的既有 505 项
生成全部通过，且新永久拼接 fixture 另行通过。三项 continuation 合成图重新
实测仍失败：calls 34301（1059f6）缺失拥有者终端清理；views 51345（6613c6）
未找到旧十指令布局；cfg 79579（1ea718）不再满足 COPY 紧邻 YIELD。三项均退出
1，并在合同失败处停止，没有运行失败产物的 JIT/AOT。全量生成通过并未消除
上述运行验收缺口或三项新字符串分配语义反例。

## 2026-09-09：字符串分配全家族与部分构造回滚（部分验收）

当前 358 文件源码 SHA256：
A888D9E731AF1F8171C2568FD4AAF67FA7A96B6EB0207A4070753B4B54E1A610。

- 12 类字符串分配结果统一为即时检查的 NULL_RESULT/STATUS_OUTPUT。原先字面量、
  整数转字符串和插值前缀三个真实失败反例已修复；自定义 __toString__ 传播可失败
  调用结果，派生字符串释放部分结果并返回失败。Cgen 未增加 AST 来源追溯。
- v6 字符串永久门 83851 退出 0（e5aef6）：20 类场景、89 处已检查分配、366 项
  C/header 输出前拒绝变异、1,100 次逐点真实分配失败；包含 nullable null 成功、
  字面量、转换、派生/自定义字符串、默认值和 inout 原子提交。严格 C、C-host
  JIT/AOT、C/header/JSON/binding 确定性均通过。
- 新失败边暴露容器接收者和部分 enum 未保护的问题，已迁移到共享求值清理事务；
  enum 完成构造后用 STORAGE_TAKE 转交。callback LOAD 来源按定义块核验。
  快速生成由中间版 494/503 恢复为 503/503：83784 退出 0（5b649e）。v4 的
  503 个 exit=2 是驱动参数不匹配，未执行编译流程，不计入语言回归结果。
- 求值清理永久门扩展 8 个操作数形态。75805 退出 0（e11cb2）：原 1,000 次清理
  加 1,200 次操作数运行，含 400 次在接收者/部分 enum 已存在后定点注入的字符串
  分配失败；18,402 次分配释放平衡。179 个 Typed 效果、33 个清理事务、17 个
  待提交结果、742 项拒绝变异。严格 C/JIT/AOT/确定性通过；任务操作数部分另有
  50 次模块激活、2,201 次平衡分配、307 项拒绝变异。该定点专项不冒充容器自身
  所有分配点的全扫描。旧 v3 驱动对新 fixture 退出 1，仍能复现 enum owner 泄漏。
- 同源八项 async/generator 运行门及 value_lifetime 通过。回归组 22655、26336、
  87770 均退出 0（f1cd3d/8b1088/64c2bd）。owner 合同通过 337 RuntimeOps、
  1,154 消费输入、494 拒绝变异、47 CFG；不与旧布局的枚举数量直接比较。
  value_lifetime 仍通过 2,200 挂起、600 RHS/address、176,000 平衡分配。
- 三项 continuation 合成图仍失败，组 78987 退出 1（b45916）：calls 缺少第二次
  可失败复制前已取得 SSA owner 的回滚，views/cfg 依赖旧指令布局。均在合同处
  停止，未运行失败产物。不能将其归为无关失败或宣布生命周期验收完成。
- container_ops 当前源生成的严格 C 定向运行通过（9f7955）；raw C callback
  当前源生成/严格 C 运行通过（db2621）。未冒充仍由旧 release 驱动的完整专项。
- 大型 cgen_task_api 70423 已退出 0（26a2eb）。因此已结束的既有生成用例是
  504/504，release/examples/task_api.xl 会话 84029 仍在实际运行，不计为通过。
  当前冻结记录需保留这个待回执状态，后续结束后单独追加。
- 静态 XLIR 门更新为共享求值事务与完整字符串生产者约束，退出 0（7230ca）。
  相对 FCE3664A，IR 源码增加 2,509 bytes，Cgen 增加 261 bytes；这是源文件大小，
  不是 DLL 测量。release CLI/DLL 与 XRT 未重建，标准库继续维持最低基线。

本检查点仍不满足全目标完成条件。后续必须继续解决其他运行原语分配/析构结果、
适配器独立边界、三项合成 owner 图、复杂借用与构造逃逸、任务生命周期和取消
竞争、DLL/API 安全卸载、同源发布准入与 legacy 清退。记录计划冻结于
build/tmp/xlir_string_allocation_v8_partial_snapshot.json。

源码接线与编码检查通过（59353，87efc6）：291 源文件、6,060 文本文件。
文档索引检查退出 1（c5acc6）：六份已有扩展库文档未被 README/状态矩阵索引，
并匹配到历史验收记录中的“待确认”及 `1,251 个` 内部的 `251 个`。
后两种匹配已在 FCE 冻结文档中复核存在；没有删除真实未完成事项或改写历史
验收数来满足该文本检查。本轮不能报告文档索引门通过。

### A888D9E7 检查点的大型生成追加回执

task_api 会话 84029 已正常退出 0（ec9627），因此该源码的既有 505 项生成全部
通过。原部分检查点保留当时的运行中状态，不覆写历史快照；终态与产物哈希另记
build/tmp/xlir_string_allocation_v8_task_api_completion.json。完整生成仍不能替代
尚未恢复的 continuation 合成图或其余语言生命周期与发布准入工作。

## 2026-09-09：三项 continuation 所有权专项恢复（部分验收）

编译器源码仍是 A888D9E731AF1F8171C2568FD4AAF67FA7A96B6EB0207A4070753B4B54E1A610；
本轮修改测试建图、故障注入和验证接线，没有修改生产 IR 验证器或 Cgen 来接纳
不完整图，也没有重建 release CLI/DLL/XRT。505/505 生成终态已另存追加回执。

- views：80044 退出 0（d8fd32），aliased/single 两种图分别拒绝 29/23 项变异；
  每种 300 次正常/逐处提前关闭，另逐点注入 100 次真实分配失败，2,100 次平衡
  分配。一个物理 owner 的透明视图、终端转交和失败回滚均通过严格 C/JIT/AOT/
  确定性；失败注入仅计严格 C，JIT/AOT 仍为无注入 smoke。
- CFG：99734 退出 0（f5bc52），4 machines、5 条正常消费边、5 处驻留安装、
  20 项拒绝变异；3,500 次正常/提前关闭与 540 次真实分配失败，含 20 次在 owner
  已驻留并恢复后失败。25,400 次平衡分配；分支互斥消费、跳过首次挂起、循环
  重取得、转交存储和清理通过严格 C/JIT/AOT/确定性。
- calls：86741 退出 0（4391ec），3 个完整 SSA 区域、6 个正常消费动作、242 项
  拒绝变异。8,500 次基础运行、7,200 次真实挂起；已有 960 次 setup 故障、
  1,600 次最终结果盒故障、640 次 null-source/双 Watch 拒绝回滚通过。新增
  60 次 generator 与 240 次 async 启动故障，其中 160 次未捕获失败、80 次首次
  await 建立失败被源码 catch 恢复。132,602 次平衡分配，严格 C/JIT/AOT/确定性通过。
- 中间测试误用 FutureValue 检查失败 Future，实际主动将失败传播到 TLS；改为
  FutureResult 后继续保留错误隔离检查。启动末段故障的预期按源码 catch 修正，
  没有跳过任何实际触发的故障点。测试脚手架的失败图也曾被清理区规则和统计
  一致性拒绝；修复的是测试图的完整性，没有关闭验证规则。

下一项已建立运行反例：build/tmp/xlir_enum_allocation_v1.xl 的 tagged enum
构造基线严格 C 通过（d6ebe6），共 13 处分配。逐点注入 9da030 退出 1：
故障 0/1/2/3/4/6/7/9/10 导致进程 exit=3；5/8/11/12 正确返回 MemoryError。
这与 TAGGED_ENUM_NEW 的 abort、载荷装箱/插入的未闭合失败协议一致，不能以
此前“部分对象回滚通过”声称 enum 的分配结果完整。未运行失败产物的 JIT/AOT。

下一步处理 enum 创建、载荷装箱和插入的显式结果及统一构造事务，再继续其它
运行原语、适配器边界、复杂借用/构造逃逸、任务与取消竞争、DLL 安全卸载、
同源发布验收和 legacy 清退。仍保留上一检查点的文档索引失败记录。
本轮测试及 enum 负例计划冻结到 build/tmp/xlir_synthetic_owner_v7_partial_snapshot.json。

### continuation 证据冻结追加回执

上述 v7 三项最终重跑均退出 0：views 85951（a76c62）、CFG 27459（83b545）、
calls 36414（123348）。静态/编码检查 24520 退出 0（068004），6,062 个文本文件。
冻结脚本已执行，回执 5c6663：96 项当前记录，628 项前序产物哈希核验通过；
build/tmp/xlir_synthetic_owner_v7_partial_snapshot.json 已存在，不再只是计划。

## 2026-09-09：枚举构造和插入结果（部分验收）

当前 358 文件源码 SHA256：
B1FF950233B4EEDED75442689C1E0F385CF0C30F6004AB25D9C2AE4794FAAD9C。

- 枚举构造获得独立 MAY_RAISE 效果（包含零载荷变体）；NEW/SET_NEW 分别使用
  NULL_RESULT/FALSE_RESULT，验证器要求即时 ERROR_TEST。NEW 的局部对象/tag
  分配失败由固定 runtime helper 回收后返回 NULL；SET_NEW 保持两边均消费的
  载荷协议，由统一求值事务清理半成品，不再 TRAP_IF_FALSE。
- 原 v1 字符串枚举反例在 v2 修复版的 13 个故障点全部返回 MemoryError 且分配
  平衡（daf176），原 9 个进程异常终止点消除。v1 失败产物已保留，未覆写。
- 新永久门覆盖空/整数/混合标量/字符串/重复借用字符串/嵌套泛型枚举及 catch。
  v6 门 46583 退出 0（37d0f0）：7 个构造、11 个插入具有已检查结果；99 项
  C/header 输出前拒绝变异。240 次基线运行、2,640 次逐点真实分配失败、
  18,280 次平衡分配。严格 C、C-host JIT/AOT 各 12 次无注入 smoke，以及
  C/header/JSON 确定性通过；故障注入只计严格 C，不冒充 JIT/AOT 故障覆盖。
- 扩展 mixed fixture 暴露未使用平凡模式绑定的 STORE 已被规范化为 DISCARD、
  而验证器仍强制存储槽的冲突（v3 d08ce8 退出 2）。现在接受的
  DISCARD 仍须绑定精确载荷/来源/函数，受管理载荷不能走此分支；新增拒绝变异
  覆盖丢失该投影。不是删除 unused 参数来躲避编译失败。
- v5 门曾因测试对 C-host smoke 强制启用不支持的 memory-debug 而失败，未继续
  AOT。v6 将 smoke 与严格 C 分配计数分开，所有严格 C 故障点保持不变。
- 求值清理同源门 67535 退出 0（f85ee3）：179 effects、33 transactions、17 pending
  results、742 拒绝变异；任务操作数 50 次模块激活、2,201 次平衡分配、307 拒绝
  变异；严格 C、JIT/AOT 均通过。
- 中间 v2 驱动的既有快速生成 503/503 通过（84302，b53308）；它早于平凡模式
  绑定验证修复，不能冒充 B1FF 的全量结果。B1FF 使用 v5 驱动重跑快速 503 和
  两项大型 task API，实际终态需另行记录，不继承 A888 的 505/505。
- 新明确反例 build/tmp/xlir_enum_boxing_v1.xl：数值枚举装箱后执行 TYPE_BIND。
  B1FF 生成的 v2 负例基线为 7 处分配，故障 5 导致 exit=3，其余 6 点正确返回
  MemoryError（7e4584）；未执行失败产物的 JIT/AOT。普通装箱的延迟失败观察、
  数值枚举/union/nullable/callable 等完整生产者结果仍是未完成义务。
- 相对 A888，IR 源码增加 1,936 bytes，Cgen 增加 601 bytes；不是 DLL 体积测量。
  release CLI/DLL 和 XRT 未重建，标准库维持最低基线。前序文档索引失败未豁免。

后续继续完整装箱与其它分配/析构结果、独立适配器边界、复杂借用/构造逃逸、
任务生命周期与取消竞争、DLL 安全卸载、同源发布验收和 legacy 清退。当前只对
枚举 NEW/SET_NEW 及上述已执行场景部分验收，不能声明枚举或长期任务完成。

### B1FF 同源回归追加回执

- v6 快速生成 93169 退出 0（6a53cb），503/503；不包括仍需终态的两项大型生成。
- 字符串门 63804 退出 0（f4699a）：20 个形态、1,100 次真实分配失败，严格 C、
  JIT/AOT、确定性通过。
- owner/静态/编码检查 29858 退出 0（09f349）：337 RuntimeOps、1,154 消费输入、
  492 项拒绝变异、47 CFG；6,067 个文本文件。变异计数随当前生成图变化，不与
  A888 的 494 直接等价比较，未跳过不通过的变异。
- 七项既有 enum fixture 由 B1FF 驱动生成并执行严格 C，全部通过（770bd0）：
  match matrix、numeric、generic top-level、nested generic、recursive container、
  tagged lifecycle、async closure。没有执行会使用旧编译 DLL/common.bat 的完整
  enum ABI 脚本来冒充同源结果；本项不含故障注入，也不等于完整异步生命周期。
- 模式绑定清理门 99432 退出 0（ca6bd0）：同步/异步清理、捕获绑定、结果提交、
  CFG 拒绝变异均通过；异步部分包含 14 个明确绑定生命周期、614 项拒绝变异、
  1,600 次真实挂起、45,608 次平衡分配，严格 C/JIT/AOT/确定性通过。

大型 cgen_task_api 61432 已退出 0（5c8b96），当前已结束的既有生成为 504/504。
release/examples/task_api.xl 会话 60774 仍有实际活动进程，不计作通过。
本检查点冻结记录为 build/tmp/xlir_enum_allocation_v7_partial_snapshot.json；未完成
生成单独保留活动进程回执，不能冻结正在写入的产物。结束后需补充同源终态，
不能覆盖当时的部分检查点。

### B1FF 最后一项大型生成终态

60774 已退出 0（592146），同源既有生成现在为 505/505。追加回执是
build/tmp/xlir_enum_allocation_v7_task_api_completion.json（551fcc），不覆盖原先
记录仍在运行的部分检查点。这一结果不适用于随后修改的装箱编译器源码。

## 2026-09-09：装箱转换和泛型刷新（未通过完整专项验收）

当前 358 文件源码 SHA256：
C758000357367FE53E7C3C3783C4DC33F1DF1C32BAF16561932E42EA9130009D。

- 九类装箱具有独立 NULL_RESULT 及即时 ERROR_TEST；完整生产者元数据在分支前
  安装。初始化、调用/返回、集合元素与多赋值的 typed 转换效果同步补齐。
- v4 快速生成 502/503，唯一失败为泛型类复制/克隆。具体化后统一重算初始化及
  隐式转换，再重建 ExitPlan；原失败用例 v5 退出 0（6bfc62），v5 快速生成
  503/503（96378，e1ca35）。v5 早于后续多赋值修复，不作为 C758 全量证明。
- 新用例暴露多返回值到 var/动态索引的转换缺口；命名存储和元素写入现在共用
  已求值投影转换，保留源字段及未提交字段的所有权，不重复求值 RHS。
- 永久门 tools/check_xlir_boxing_allocation_outcome.ps1 已接入核心套件和静态
  合同检查。C758 结构检查覆盖 48 个受检查装箱、13 个上下文效果、12 个装箱
  初始化；233 项畸形 IR 在输出 C/header 前被拒绝（65e437）。完整脚本的重复
  C/header/JSON 比较及严格 C 编译通过，但运行失败，未进入 JIT/AOT：16357
  退出 1（ab1729），日志 build/tmp/xlir_boxing_outcome_v7_gate.log。
- 独立逐例检查的 24 种形态 × 未捕获/捕获共 48 个基线均通过。204 个真实分配
  故障场景中 161 个通过、43 个失败（96292，03160f）；全部结果保留在
  build/tmp/xlir_boxing_outcome_v7_fault_audit.json。成功 nullable-null 不分配，
  不伪造故障点；数值枚举在捕获/未捕获两种路径的全部 7 个点均通过。
- 失败集中在 union 数组、列表/字典字面量、泛型类复制/克隆及动态元素多赋值。
  证据指向容器创建后延迟观察失败、插入 TRAP_IF_FALSE，以及 clone 失败结果
  协议缺失；存在进程退出或错误种类不正确，不能以 catch 可运行宣布正确。
  这些负例没有被删除或转为预期拒绝，完整装箱门仍为红色。
- 同源求值清理门 60113 退出 0（3d1170）：179 effects、33 transactions、17 pending
  results、742 拒绝变异；task operands 50 次激活、2,201 次平衡分配、307 拒绝
  变异；严格 C/JIT/AOT/确定性通过。
- 同源 product owner 门 98567 退出 0（7511de）：337 RuntimeOps、1,154 消费输入、
  473 拒绝变异、47 CFG；1,490 次运行、60 次分配失败、41,080 次平衡分配；
  严格 C/JIT/AOT/确定性通过。计数反映当前图，不继承先前检查点的变异数量。
- 静态 XLIR 合同检查退出 0（9f849a）。当前 C758 的 503 快速扫描仍需另记终态；
  两项大型 task API 尚未在 C758 重跑。release CLI/DLL/XRT 未重建。

下一步先完成容器构造、插入与 clone 的规范失败结果，使上述完整运行门通过，
再继续其余分配/析构结果、适配器函数边界、复杂借用与构造逃逸、任务与取消
竞争、DLL 安全卸载、同源发布准入和 legacy 清退。标准库仅最低基线，旧文档
索引失败仍未豁免；整体目标保持未完成。

### C758 同源快速生成终态

10700 已退出 0（5fb30c），C758 既有快速生成 503/503；两项大型 task API
仍未在这一快照重跑，不继承 B1FF 的 505/505。编码检查 9717 退出 0（c85d08），
6,072 个文本文件；定向 git diff --check 退出 0（c1a854）。计划冻结到
build/tmp/xlir_boxing_outcome_v7_partial_snapshot.json，保留装箱完整运行门失败，
不以生成和已有所有权专项通过替代新发现的 43 项真实故障修复。

### C758 证据冻结追加回执

冻结已完成（95f596）：444 项记录、922 项前序记录核验，前序源码按其不可变
副本核验而非与当前工作树比较。build/tmp/xlir_boxing_outcome_v7_partial_snapshot.json
及 build/tmp/xlir_boxing_outcome_v7_sources/src 已存在。当前源码与发布产物边界、
503/503 快速生成终态、已通过的两项所有权门和仍失败的完整装箱门分别记录。

## 2026-09-09：装箱与容器完整专项通过（整体仍未完成）

当前 358 文件源码 SHA256：
06342732706FA2E8FE6DF4ED4AA290C8DB7A1A55037CDE72965C28B45A35036D。

- 新增八类容器结果协议；六类创建/复制使用 NULL_RESULT，两类插入使用
  SCOPED_BOOL_RESULT，同时观察原生布尔结果与独立回调错误。默认构造、空
  字面量、泛型复制和插入的错误效果显式进入 IR；Cgen 不推测源语义。
- C758 的 43 个真实故障反例已修复，旧失败快照不覆盖。永久专项 56574 退出 0
  （96d55b）：32 种形态、1,280 次基线、6,240 次真实分配失败、40,780 次平衡
  分配；额外 20 次 true-result 回调失败及 20 对插入失败/抛错析构均通过。
  预先分配的错误和实际析构计数保证次要错误确实发生，原始分配失败仍保留。
- 合同检查覆盖九类装箱和八类容器：66 个装箱、36 个容器操作、17 个上下文
  效果、16 个初始化、13 个构造、4 个复制；478 项畸形 IR 在 C/header 输出前
  被拒绝。完整门含严格 C、C-host JIT/AOT 冒烟和 C/header/JSON 确定性。
- 多返回值所有权 51184 退出 0（dfe6db）：337 RuntimeOps、1,154 消费输入、
  471 项拒绝变异、47 CFG；1,490 次运行、60 次分配失败、41,080 次平衡分配。
- continuation owner CFG 30587 退出 0（ca5aee）：4 状态机、20 拒绝变异、
  3,500 次运行、540 次分配失败（含 20 次 resident owner 挂起后失败）、25,400
  次平衡分配；分支、循环重获、存储转移和提前关闭的严格 C/JIT/AOT 均通过。
- 求值清理 71175 退出 0（a11923）：742 拒绝变异；task operands 307 拒绝
  变异、50 次模块激活、2,201 次平衡分配；严格 C/JIT/AOT/确定性通过。
- 静态 XLIR 门退出 0（62af1f）。同源驱动 50961 退出 0（d5ec8b）；快速扫描
  64110 退出 0（7a8770），503/503。大型 cgen_task_api 42131 退出 0（09d4ef），
  当前已完成 504/505；release/examples/task_api.xl 73378 仍有活动进程，未计为
  通过。部分检查点不冻结正在写入的产物，终态另行追加。

下一阶段仍须完成其余分配/析构结果、适配器独立函数边界、detached LOAD/MOVE、
复杂借用与构造逃逸、任务/Future 生命周期及跨线程取消、DLL/API 安全卸载、
同源发布准入和 legacy 清退。外部 XRT 非末引用释放问题与已有文档索引失败
仍未豁免。标准库仅最低必要基线；release CLI/DLL 和 XRT 本轮未重建。

### 06342732 最后一项生成终态

73378 已退出 0（8bd33c），最终大型 task API 的 C/header/JSON 均生成结束。
06342732 同源既有生成现为 505/505；这仍不等于全部语言运行验收。冻结脚本
第一次因任务刚结束而拒绝记录“仍在运行”（be2427），且尚未复制任何文件；
重新核验终态后将全部已结束产物纳入部分检查点，不保留虚假的活动进程声明。

冻结已完成（59fe0d）：build/tmp/xlir_container_outcome_v3_partial_snapshot.json
共 460 项记录，444 项前序记录核验通过；358 文件源码不可变副本位于
build/tmp/xlir_container_outcome_v3_sources/src。整体目标仍未完成。

## 2026-09-09：适配器默认析构与独立函数边界（部分验收）

当前 358 文件源码 SHA256：
FB983813BE0CDD170A7BCAE454FBA72EE7371F7EC02D8C0941AD10B9672F886C。

- 旧 06342732 源码已直接复现四项错误（bc1fd2，退出 3）：默认对象析构报告
  无诊断失败后，标量、对象、产品和 void 适配器仍返回 ok=true。原生成 C、
  JSON、探针与二进制保留于 build/tmp、build/test 的 xlir_adapter_cleanup_v2
  前缀；没有以外层 catch 的结果代替独立函数结果检查。
- 新增 FUNCTION_FAILURE 计划；适配器默认表达式与调用后清理都使用该独立
  边界，不继承创建方的词法 catch/清理路径。删除专属默认 RELEASE helper，
  复用共享调用完成、求值事务与 pending-result 的成功提交/失败回滚。
- 最终永久门 91153 退出 0（945404）：5 独立边界、5 清理事务、3 待提交结果，
  288 项 C/header 输出前拒绝变异。400 次执行、2,080 次逐点分配失败、30,960
  次平衡分配；严格 C、C-host JIT/AOT、C/header/JSON/binding 确定性通过。
  其中两个被适配目标 NO_RAISE，五个适配器均在创建方 try/catch 中取值。
  错误对象预先分配，分别检查有诊断/无诊断析构错误、逆序清理、无失败载荷
  外泄以及目标原始异常优先。永久门已接入 core suite 与静态合同检查。
- 中间 F5C62487 的 200 项拒绝检查和实际运行已通过（4731，488992）。扩大
  负例后发现删除 unwind.cleanupPlan 仍被接纳，29867 退出 1（77f2d4）。当前
  源码要求每个适配器 ERROR_UNWIND_BEGIN 均引用自身边界，不能缺省；新增负例
  保留并全部通过，不将其改为预期接纳。F5C62487 的 358 文件源码已冻结到
  build/tmp/xlir_adapter_cleanup_v7_sources/src（df7d6f）。
- 中间 F5C62487 生成 505/505：快速 39391 退出 0（1d0fdd），大型 83716/40816
  均退出 0（92254c/528c99）；这不是当前 FB983813 的全量生成成绩。
- FB983813 驱动 17256 退出 0（3ea4ad）。字符串门 53807 退出 0（bf691a）：
  89 个受检查分配、366 项拒绝变异、1,100 次真实分配失败；求值清理 85467
  退出 0（f19345），严格 C/JIT/AOT/确定性通过。静态门退出 0（ad88f1）。
  当前快速扫描、装箱/容器、多返回值所有权和两个大型 task API 另记最终回执。
- 相对 06342732，本轮只修改三个 IR 源文件，共增加 4,977 bytes；Cgen 源码
  字节未变。此数字是源码大小，不是 DLL。release CLI/DLL 与 XRT 均未重建。

本专项关闭已复现的具名适配器默认求值/析构及函数边界问题，不代表全部 callable
bridge、原生资源或异步边界已完成。其余分配/析构原语、detached LOAD/MOVE、
复杂借用和构造逃逸、任务与嵌套 Future 生命周期、无挂起 worker、跨线程取消、
DLL/API 安全卸载、外部 XRT 非末引用释放、同源发布准入和 legacy 清退仍有效。
标准库维持最低必要基线，既有文档索引失败不豁免，整体长期目标保持未完成。

### FB983813 同源回归终态

快速扫描 65174 退出 0（7fc828），503/503。装箱/容器 89993 退出 0（513fbc），
32 形态、478 拒绝变异、6,240 次真实分配失败；多返回值所有权 75316 退出 0
（479ea7），471 拒绝变异、47 CFG、1,490 次执行、60 次真实分配失败。
两项均包含严格 C、C-host JIT/AOT 与确定性。编码 37802 退出 0（97185c），
6,077 个文本文件；定向 diff 检查退出 0（b71273）。9070/65888 的两个大型
FB983813 task API 仍有活动进程，暂不计入通过；不可继承 F5C62487 的 505/505。

continuation owner CFG 71486 已退出 0（ca93fa）：4 状态机、20 拒绝变异、
3,500 次执行、540 次真实分配失败、25,400 次平衡分配，严格 C/JIT/AOT/确定性
通过。大型 cgen_task_api 9070 已退出 0（1cc0e6），当前既有生成 504/505；
65888 仍有实际活动进程，其正在写入的产物排除在部分检查点之外。

### FB983813 冻结与最终生成追加回执

部分冻结已完成（43166，4dfee1；记录输出 b0ee68）：
build/tmp/xlir_adapter_cleanup_v9_partial_snapshot.json 共 898 项记录，460 项前序
记录核验通过，源码副本位于 build/tmp/xlir_adapter_cleanup_v9_sources/src。
之后 65888 已退出 0（a3d358），最终同源既有生成达到 505/505。最后一项产物
单独冻结并追加到 build/tmp/xlir_adapter_cleanup_v9_task_api_completion.json，
不覆盖原先记录尚在运行的部分检查点。此结果仍不是语言全量运行或发布完成。

## 2026-09-09：闭包创建结果、创建方边界与可空捕获（部分验收）

当前 358 文件源码 SHA256：
C11F4AD5D652B733964801510FF9BD11F1F30DDCB4FFBA86B8A98BD148DC8F60。

- FB983813 的探针已复现具名/匿名/字符串捕获分配失败后 ok=true、value=NULL，
  以及 CELL_NEW 分配失败直接 abort。旧探针、生成 C 和相关日志保留。当前
  闭包创建改为 MAY_RAISE / NULL_RESULT，并立即进入经验证的 ERROR_TEST CFG；
  捕获槽问题尚未关闭。
- 匿名创建位于 try 内的新增反例先暴露了错误边界（v5 generate 1b0b84，退出 2）。
  现按 BEFORE_NODE 计划的创建方环境构造并核验退出路径，既保留 catch，也
  保留创建方活动局部的清理；函数体和参数边界不变。具名函数值的条件类型
  推断复用已有唯一候选解析器；两个既有重载负例仍在语义阶段拒绝（131ab8）。
- 中间 A553857E 源码已复制到 build/tmp/xlir_closure_allocation_v8_sources/src。
  其既有生成 503/503（77613，ff4ae8）及旧适配器专项通过，但扩大可空捕获门
  后 v9 17581 退出 1（a4af04）：NULL string? 被复制成空串，闭包错误返回 0。
  负例产物冻结在 xlir_closure_allocation_v9_nullable_negative 前缀，不覆盖。
- 新增 COPY_NULLABLE_STRING 捕获 ABI，普通/可空复制不可互换。最终永久闭包门
  15334 退出 0（88c093）：同步模块 12 个受检查闭包、8 个创建方边界、90 项
  C/header 输出前拒绝变异；suspension 模块另有 3 个可空捕获合同与 3 项拒绝。
  13 上下文、260 次基线执行、820 次真实分配失败（720 次创建、20 次调用、
  80 次被 catch 捕获）、2,940 次平衡分配；严格 C/JIT/AOT/确定性通过。
  async/generator 模块是挂起前后创建的运行冒烟，不冒充并发故障注入验收。
- v6 严格探针的早期失败 fedc53 属于测试将工厂和调用结果混写：字段恢复的
  callable 会在调用时装箱分配。已保留早期 host/runtime 并分开验证两份结果，
  未把该调用失败删除、豁免或改为预期成功。
- C11F4AD5 同源字符串门 16484 退出 0（c1e82e）：89 个受检查分配、366 项
  拒绝变异；装箱/容器 25103 退出 0（039369）：32 形态、478 项拒绝变异、
  6,240 次真实分配失败。两项均含严格 C/JIT/AOT 与确定性。
- 同源驱动 39177 退出 0（1b6e36），既有快速生成 51751 退出 0（29e145），
  503/503；两项大型 task API 86791/51185 另记终态，暂不计通过。静态合同门
  退出 0（131ab8）。其余同源所有权专项与编码检查随后追加最终回执。

下一步仍是 CELL_NEW 分配/初始值移交、参数及其他绑定入口的完整失败语义。
其余分配/析构原语、callable/native 独立边界、detached LOAD/MOVE、复杂借用和
构造逃逸、任务/Future 生命周期及取消竞争、DLL/API 安全卸载、同源发布准入
和 legacy 清退均未豁免。外部 XRT 非末引用释放与既有文档索引问题仍保留。
标准库仅最低必要基线；release CLI/DLL 与 XRT 未重建，整体目标未完成。

### C11F4AD5 同源所有权回归追加回执

求值清理 72136 退出 0（2ac9e1），742 项拒绝变异；task operands 307 项拒绝、
50 次模块激活、2,201 次平衡分配。多返回值所有权 53332 退出 0（448e96），
1,490 次运行、60 次分配失败、41,080 次平衡分配。continuation CFG 82340
退出 0（0aecfb），4 状态机、20 项拒绝变异、3,500 次运行、540 次真实分配失败、
25,400 次平衡分配。适配器 14720 退出 0（407187），288 项拒绝、400 次执行、
2,080 次真实分配失败。这些专项均含各自严格 C、C-host JIT/AOT 和确定性验证。

编码 16611 退出 0（0e2219），6,085 个文本文件；diff 检查退出 0（ececb2），
Git 的 LF/CRLF 提示不视为源文件错误。大型 cgen_task_api 86791 已退出 0
（9428de），当前既有生成 504/505；51185 的最终 task API 仍在运行，等待终态。
相对 FB983813，本专项生产源码修改 9 文件，共增加 4,993 bytes，其中 Cgen
五文件共增加 957 bytes，主要是显式可空捕获 ABI 的渲染与准入；不是 DLL 增量。

### C11F4AD5 冻结与最后一项生成终态

部分检查点已完成（45364，59acb7）：
build/tmp/xlir_closure_allocation_v13_partial_snapshot.json 共 936 项记录，902 项
前序记录核验通过；当前源码副本位于 xlir_closure_allocation_v13_sources/src。
51185 随后确认退出 0（261c25），同源既有生成达到 505/505。最后一项产物与
终态单独记录到 xlir_closure_allocation_v13_task_api_completion.json，不覆盖部分
检查点。本轮仍不包含 CELL_NEW 故障修复、全语言运行验收或发布产物更新。

## 2026-09-09：捕获槽入口的受检查分配与安装（部分验收）

当前 358 文件生产源码 SHA256：
8BF716A0DF66BD78BF95D1C60EAFBFCA140E0ED91FFF7F14A75BFA6D8154856C。

- CELL_NEW 改为零操作数、拥有型 cell 结果、MAY_RAISE / NULL_RESULT；CELL_INIT
  消费 cell 与初值并只在成功分支激活存储。共享 lowering 覆盖局部、参数提升
  和表达式模式绑定，Cgen 只渲染显式指令，不再为槽分配失败输出 abort。
- 新的 CFG 激活检查先拒绝局部字符串捕获：v2 794df3 退出 1；v4 诊断 c982c9
  明确指出 s24 在第 55 条 cell-release 时尚未激活。修复声明初始化的词法
  生命周期，并删除全存储扫描补发 cell-release 的旧逻辑，没有豁免验证器。
- v5 六形态探针 eccafc 已使整数槽 fault=0、字符串/动态值槽 fault=1 从 abort
  变为失败载荷与 MemoryError，内存断言通过。该旧探针对零捕获闭包过量注入
  fault=1 时两次触发“没有实际分配失败”断言，属于超过真实分配点的测试输入；
  新永久门从每次基线的分配数枚举故障点，未将未触发故障计入通过。
- 永久门 tools/check_xlir_cell_allocation_outcome.ps1 已接入核心函数 ABI suite
  与静态合同门。v10 10274 退出 0（df37af）：14 处受检查分配/安装、7 个参数槽、
  224 项 C/header 输出前拒绝变异；13 上下文、260 次基线执行、1,800 次真实
  分配失败（1,200 创建、540 调用、60 捕获）、9,700 次平衡分配。覆盖整数、
  字符串、var、tagged nullable、可空字符串参数、多参数已初始化前缀、表达式
  模式绑定、catch/finally。严格 C、C-host JIT/AOT 和 C/header/JSON/binding
  确定性通过；工厂结果和闭包调用结果独立检查。
- 同门的 suspension 模块另验证 3 个槽分配和 3 项拒绝变异，执行 await 前建槽、
  await 后创建闭包以及 yield 前后修改可空捕获。严格 C/JIT/AOT 冒烟通过；
  不冒充异步分配失败、跨线程 cell 引用计数或取消竞争验收。
- 旧 managed-storage 输入门的 cell-new/单操作数假设已更新为 cell-init 第二
  操作数，并保留绕过 copy/retain、缺失 MOVE 与原子替换的拒绝检查。其旧常量
  改写测试依赖已经消失的可变 VALUE merge，v8 16861e 暴露 constantWrites=0。
  现显式构造非法常量写入，未降低计数断言；v9 44625 退出 0（9130de），86 项
  拒绝，包括 2 个 cell-init、2 个 cell-exchange 和 2 个常量写入反例。
- 同源驱动 85037 退出 0（fe82f9），快速生成 77975 退出 0（efee2d），503/503。
  两项大型 task API 63407/4885 仍有实际进程，暂不计入通过。不能继承 C11F4AD5
  的 505/505。
- 同源字符串门 78577 退出 0（2266db），1,100 次真实分配失败；求值清理门
  69812 退出 0（5abc24），包括 task operands 的 307 项拒绝、50 次模块激活与
  2,201 次平衡分配。闭包门 79207 退出 0（7c597b），820 次真实分配失败；
  装箱/容器 80870 退出 0（fc8e48），6,240 次真实分配失败。
- continuation CFG 36908 退出 0（af5a23），3,500 次执行、540 次真实分配失败；
  多返回值/owner 门 62847 退出 0（7b8b3c），470 项拒绝、47 CFG、1,490 次运行、
  60 次真实分配失败；适配器门 32370 退出 0（b90964），288 项拒绝、2,080 次
  真实分配失败。这些门均包含各自严格 C、JIT/AOT 和确定性检查。静态合同门
  已退出 0（acdb9d）。

### 新暴露而未关闭的循环绑定缺口

test/cases/xlir_cell_loop_allocation_outcome.xl 是合法回归，不是预期拒绝用例。
同源诊断驱动 52eff2 返回 1：4:20 的 cell-retain 指向尚未提升的普通绑定 s10，
第 62 条指令没有 cell storage ABI。原始扩展探针及单独循环探针保留在
build/tmp/xlir_cell_allocation_v6_{expanded,loop}_negative.txt；循环回归从入口
专项拆出独立追踪，失败没有删除或转换为通过。下一步须统一循环绑定的物理
owner、初始化前缀、逐次重建和退出顺序，不能仅为一个数组样例添加 Cgen 补丁。

相对 C11F4AD5，本轮生产源码修改 13 文件，共增加 11,015 bytes：Cgen 八文件
合计增加 503 bytes，lowering 减少 358 bytes，主要增量是 IR 合同与独立验证。
这不是 DLL 增量。release CLI/DLL 与外部 XRT 未重建。捕获槽专项仍未包含完整
循环绑定及并发生命周期；其他可失败原语、callable/native 边界、复杂借用与
构造逃逸、Task/Future 生命周期、DLL 安全卸载、同源发布准入、legacy 清退和
既有文档索引问题均继续保留。标准库只维持最低必要基线，整体目标未完成。

### 8BF716A0 冻结与最后一项生成终态

部分检查点 40249 已退出 0（ddf2be）：
build/tmp/xlir_cell_allocation_v11_partial_snapshot.json 共 574 项记录，940 项前序
记录逐项哈希核验通过；358 文件源码副本位于 xlir_cell_allocation_v11_sources/src。
随后最后一项大型 task API 4885 已确认退出 0（a99e59），本轮同源既有生成达到
505/505，未再使用上一源码检查点的成绩。最终生成产物单独冻结并追加到
build/tmp/xlir_cell_allocation_v11_task_api_completion.json，不改写先前的部分记录。
编码检查 72348 退出 0（70b2e3），6,093 个文本文件；定向 diff 检查退出 0
（c6fd3e）。这些结果不关闭上节的合法循环捕获失败，也不代表全语言或发布完成。

## 2026-09-09：循环捕获绑定与分配失败（部分验收）

当前生产源码 SHA256：
BD52332A6E5BD9A39FE8BF84866F1B42852FEE5B777D32B6A6E51A58407BCCFD。

- 原合法失败 xlir_cell_loop_allocation_outcome.xl 已通过同源 IR、生成 C 与运行。
  共享循环入口按捕获元数据提升 cell，LoopOwner 使用物理类型，ExitStep 保存
  loopTargetCount；初始化失败按前缀逆序关闭，正常/异常退出使用同一生命周期。
  range 的用户变量不再承担内部推进状态；协议 payload 在成功绑定前由临时
  owner 保护，generator 的 ordinal 在取走 yield item 之前安装。
- 扩大故障注入曾暴露真实错误：v3 fd3122 在 string shape=2/fault=4 仍成功并
  返回 4105；v4 trace 9bffb9 在 list shape=3/fault=11 abort；v5 7b7367 的
  generator allocation failure 被映射为非 MEMORY；v6 776726 的显式 throw
  对象分配失败丢失诊断。均修复基础操作合同或共享错误构造，不放宽测试断言。
- 永久门 tools/check_xlir_cell_loop_binding.ps1 已接入核心函数 ABI suite 与
  静态合同。最终 v10 55022 退出 0（88f4f0）：17 物理 cell、93 个显式前缀、
  11 个受检查获取操作、427 项 C/header-gated 拒绝；11 上下文、220 次基线、
  5,480 次真实分配失败、94,060 次平衡分配。严格 C、C-host JIT/AOT 与
  C/header/JSON/binding 确定性通过。覆盖八类遍历和单目标字典，不依赖 set 顺序。
- 中间 EFB56D7B 源码的既有生成 503/503（e9b823）与两个大型任务均终态通过：
  cgen_task_api 83490 退出 0（b140e6），task_api 95603 退出 0（5ac30e）。
  该版源码保存在 build/tmp/xlir_loop_cell_v7_sources/src；这些成绩不能自动
  作为当前 BD52332A 的全套生成成绩。
- 当前同源驱动 6261 已退出 0（495f5c）。BD52332A 的快速生成 29951 与两个
  大型任务 28810/43447 已启动，未获终态之前不计入通过。
- 当前同源定向回归 35988 退出 0（4910bb）：循环清理 12,000 次运行、2,000
  次真实诊断分配失败、679,200 次平衡分配；Error 位置、原最小循环捕获、扩展
  循环绑定、双目标 generator、循环异常与协议迭代的严格 C/运行通过；owner
  consumption 470 项拒绝、47 CFG 检查通过。v7 定向脚本第一次误用 main 入口
  host 编译顶层语句用例而失败（86fbfa），改正测试入口后通过，未修改编译器
  来适配错误 host。

本轮不证明循环体跨 await/yield 的 cell 全生命周期、跨线程安全、全部可失败
原语或完整语言运行验收。复杂借用/构造逃逸、Task/Future、native/callable、
DLL 安全卸载、同源发布准入、legacy 清退、外部 XRT BUSY 问题及既有文档索引
问题继续保留。release CLI/DLL 与 lib/xrt/xrt.h 哈希未变，未修改外部 XRT 仓库。

### BD52332A 同源回归补充

快速生成 29951 已退出 0（1a245f），503/503。捕获槽入口 25465 退出 0
（354244），224 项拒绝、1,800 次真实故障；continuation CFG 94171 退出 0
（21e2ec），20 项拒绝、3,500 次运行、540 次真实故障。静态合同与定向 diff
检查退出 0（92398f）。两项当前源码大型生成仍待各自终态，不能继承 EFB56D7B
的 505/505。循环专项本次只对上述测试范围给出通过，不关闭整个长期目标。

### BD52332A 最终生成与循环体挂起冒烟

部分检查点 17415 退出 0（b9b191）：
build/tmp/xlir_loop_cell_v11_partial_snapshot.json 共 562 项记录，578 项前序记录
逐项哈希核验通过，源码副本位于 xlir_loop_cell_v11_sources/src。随后本轮
cgen_task_api 28810 退出 0（72d84b），task_api 43447 退出 0（2348d2），因此
当前 BD52332A 的既有生成终态为 505/505，不再继承中间版成绩。

新增循环体挂起合法回归通过同源生成（65d093）及严格 C、JIT/AOT（f6f25f），
并已接入永久循环门。最终 v13 53493 退出 0（e4ef97），保留 11 上下文、427
项拒绝与 5,480 次真实故障的完整入口专项，同时另验证两个跨挂起 cell 构造、
两项拒绝变异、await 后使用捕获槽、两次 yield 与提前关闭的严格 C/JIT/AOT
冒烟和确定性。挂起部分未做分配失败或跨线程取消注入，不计为完整并发验收。

最终生成和新增专项产物追加到 build/tmp/xlir_loop_cell_v14_completion.json，
不改写先前部分检查点。发布产物未更新；整个长期目标仍然 active。

## 2026-09-09：循环捕获跨挂起故障与受管析构验收

生产源码保持 BD52332A6E5BD9A39FE8BF84866F1B42852FEE5B777D32B6A6E51A58407BCCFD。
本轮增加源程序、IR 合同变异、C host/runtime 与永久门，未修改生产 Cgen 或
lowering。原 BD52332A 的 505/505 既有生成证据继续来自上一同源检查点，本轮
未重跑并冒充新的 505 项生成成绩。

- 新增 tools/check_xlir_cell_continuation_outcome.ps1，并接入核心函数 ABI suite
  及静态合同检查。完整源构建门 79794 退出 0（2992e6）；同源探针 v3 退出 0
  （0d65d6）。3 个 async cell resident、2 个 generator cell resident、5 个
  源级构造，25 项变异均在 C/header 输出前被拒绝。
- generator<string> 的循环同时捕获 index/item，每轮两次 yield。覆盖首次
  next、每次恢复及最终 next 的分配测量，并在所有挂起位置提前关闭；当前
  yield item 已取走/尚未取走两种情形都执行。270 次真实分配失败通过。
- 两个可独立控制的 future<int> 输入覆盖四种 ready/pending 组合。故障从
  await 后的源级标记开启，1,420 次真实分配失败通过，其中 710 次位于输入
  原本 pending 的恢复路径。失败结果必须为 MemoryError，Promise 生产者
  外层错误作用域仍成功。另验证每个输入的失败、取消、关闭，及循环后逃逸
  闭包继续读写同一迭代的字符串 cell。
- var 循环 cell 是对象的实际 owner，底层数组只持有 null；4,200 次受管
  析构组合覆盖第一个/第二个对象、带诊断/无诊断析构失败、输入终态与析构
  的优先级、每对象只析构一次，以及清理先于输出发布。不是对象数组保留
  引用直至最终销毁的弱化测试。
- 常规关闭/async 计数为 2,700 次，受管析构另计 4,200 次；整门累计 192,840
  次分配/释放平衡，live count/bytes、double/invalid free、UAF 检查均通过。
  严格 C、C-host JIT/AOT、两次 C/header/JSON/binding 的字节确定性均通过。

### 已保留的能力边界与未完成项

第一次受管扩展探针使用 generator<CellContinuationValue> 和名义类可变捕获，
前端明确拒绝（530292），原始源码保存在
build/tmp/xlir_cell_continuation_v2_rejected_source.xl，诊断在 v2_probe.log。
这属于当前前端能力限制，不能把改用受支持的 var cell 记为这两项能力已实现。
generator<var> 同样不在现有 generator 元素白名单内，本轮未放宽该白名单。

本轮关闭的是所列源程序的挂起后分配故障及受管 cell 析构组合，不是整个循环/
并发验收。跨线程引用计数与取消竞争、复杂借用/构造逃逸、其他可失败原语、
callable/native、Task/嵌套 Future、DLL 安全卸载及发布准入、legacy 清退继续
保留；标准库仍为最低必要基线。release CLI/DLL 与外部 XRT 未重建或修改。

## 2026-09-09：共享捕获身份与原子 cell 引用（同源回归进行中）

当前生产源码 SHA256：
560D7817C709C3781D29C519C631FCBA946A1E9E030BDEA9A0A51050D5DAABB6。

- 真实嵌套闭包源程序的初始生成失败（280aa2）：外层提升 cell，内层只读
  捕获仍发出普通 LOAD，验证器报 cell-backed storage must use explicit CELL
  operations。原始源码和诊断保存在 xlir_cell_thread_v1_negative_source.xl 与
  v1_probe.log。共享 sourceSymbol 的捕获现于注册结束、效果/CFG 生成前统一
  表示；只读兄弟和后代不能丢失已确定的 cell 身份。
- 增加独立验证约束及永久门 check_xlir_cell_thread_lifetime.ps1。覆盖先创建
  只读闭包再写入、跨工厂调用的参数 cell、三层闭包的对象生命周期；6 项共享
  捕获、3 项源级 cell 构造，9 项变异均在 C/header 输出前被拒绝。
- 修复前真实 8 线程 retain-only 反例（c99590）退出 2：期望 200,001，实际
  34,966。原始生成 C/header/JSON/binding 和非原子 prelude 保存在
  build/tmp/xlir_cell_thread_v4_negative。没有把未平衡的反例进程计为内存通过。
- cell 引用现使用 XRT 原子 retain/release，仍不对 payload 读写加锁。最终
  永久门 91267 退出 0（279095）：800,000 次原始引用操作、48,240 次源语言
  reader 创建/调用、30 轮最终 owner 释放竞争、97,238 次平衡分配。对象每轮
  恰好析构一次；严格 C、C-host JIT/AOT、C/header/JSON/binding 确定性通过。
- 最终同源通用驱动 22186 已退出 0（991799）。新的 503 快速生成及两个大型
  task API 已分别启动为 42339、76165、9462，未获终态前不计入通过；不能
  使用 BD52332A 的旧 505/505 代替本次成绩。入口/循环/continuation/挂起 cell
  与定向清理/owner 回归在 86737 依次运行，终态结果另行追加。

相对 BD52332A，四个生产文件增加 2,128 bytes：IR 合同/验证/lowering 合计
1,901 bytes，Cgen prelude 增加 227 bytes；不是 DLL 增量，也没有为嵌套闭包
添加 Cgen 存储猜测。发布 CLI/DLL、lib/xrt/xrt.h 与外部 XRT 仓库均未修改。
仍不证明 payload 并发读写、取消/恢复竞争、复杂借用/构造逃逸、其他可失败
原语、Task/嵌套 Future、native/callable、DLL 安全卸载或同源发布准入；标准库
维持最低必要基线，长期任务继续 active。

### 560D7817 同源回归终态补充（2026-09-10）

快速生成 42339 退出 0（f00516），503/503。核心串行回归 86737 退出 0
（9046b8）：入口 cell 224 项拒绝/1,800 次真实故障；循环 cell 427 项拒绝/
5,480 次真实故障；continuation CFG 20 项拒绝/3,500 次运行/540 次真实故障；
跨挂起 cell 保持 25 项拒绝、270 次 generator 和 1,420 次 async 分配故障、
4,200 次受管析构组合通过。循环清理另通过 12,000 次运行、2,000 次诊断分配
失败；owner consumption 470 项拒绝及 47 CFG 检查通过。各永久门均含严格 C、
C-host JIT/AOT 和确定性。静态/diff 检查退出 0（4fa368）；编码检查 27949
退出 0（736464），6,110 个文本文件。大型生成按各自真实终态继续追加。

两项大型生成现均确认终态：cgen_task_api 76165 退出 0（42d834），task_api
9462 退出 0（3384fa），本轮 560D7817 同源既有生成达到 505/505。源码、专项
生成物、反例及回归日志封存于 build/tmp/xlir_cell_thread_v8_snapshot；索引为
build/tmp/xlir_cell_thread_v8_snapshot.json，保留全部前序检查点，不改写旧证据。
此处的 505/505 仍是生成覆盖，不代表完整语言运行验收或发布准入完成。

## 2026-09-10：捕获槽取消/恢复与跨线程 frame 重叠

本轮生产源码仍为 560D7817C709C3781D29C519C631FCBA946A1E9E030BDEA9A0A51050D5DAABB6。
新增真实源程序、合同变异、并发 C host/runtime 和永久门，未修改生产 IR/Cgen。
既有 505/505 仍引用上一同源冻结检查点，不记为本轮重新执行的生成结果。

- tools/check_xlir_cell_cancel_race.ps1 已接入核心函数 ABI suite 和静态合同。
  最终门 58271 退出 0（3412e8）：4,032 次运行，含 1,080 次自由竞争（其中
  360 次两个输入由不同生产者线程完成）、360 次真实 Watch PENDING 后强制
  重叠 activation。该次有 3,599 次成功取消请求、175 次处理取消、2,149 次
  取消终态、308 次合法的晚取消成功返回，74,026 次分配/释放平衡。竞争分布
  随调度变化，不把某一分布硬编码成通过条件。
- 覆盖 pending 期间取消、恢复后/cleanup await 前取消、最后 await 后及析构
  中取消，catch 处理/重抛、finally 再次挂起、带诊断/无诊断析构失败、三类
  生产者原有错误上下文。对象析构、输出通知、Watch 释放各恰好一次，发布
  不得先于清理；输入生产者不被输出取消强杀。
- 两个源级 await 对应三个正常/异常 suspension state；1 个受管 cell，7 项
  C/header 输出前拒绝变异，包括一致删除每个 state 的 cell handoff 并修复
  arena 切片。严格 C、C-host JIT/AOT 与 C/header/JSON/binding 确定性通过。
- 20 轮独立进程复跑退出 0（ea719d）：80,640 次运行、21,600 次自由竞争、
  7,200 次强制 frame 重叠、1,480,428 次平衡分配。逐轮日志为
  build/tmp/xlir_cell_cancel_v8_stress_01.log 至 _20.log；聚合为 v8_stress.log。
- 同源既有取消及 frame 完成门 52771 退出 0（542339）：cancel-watch 的
  34,560 次执行、20,520 次取消、34,560 次发布观察、980,417 次平衡分配；
  frame-completion 的 4,800 次执行、1,200 次真实分配失败（含 480 次 Promise
  创建前失败）、1,440 次发布观察、152,080 次平衡分配。两门均含严格 C、
  C-host JIT/AOT 和确定性；这不是未运行的旧 CLI 套件成绩。

### 测试负例与边界修正记录

首个合同驱动误把源级 await 数等同于 lowered suspension 数，断言失败
（a09660）；检查实际 IR 后，改为同时断言 2 个来源和 3 个正常/异常 state，
并新增三处 cell handoff 删除变异，没有削减状态要求。

首个并发观察器误要求原生观察器永远看不到 ambient error（d47d1d）。跟踪
确认 drops=1，观察到的是原始生产者错误 31（b19068），不是清理错误 91。
原始生成物和测试代码保存在 build/tmp/xlir_cell_cancel_v2_negative。修正为
允许同一原始诊断对象，并同时加强生产者最终错误的身份检查；独立生产者各
retain 同一个不可变 sentinel，避免测试自身引入共享可变错误指针的数据竞争。
这些是测试预期修正，不宣称为生产编译器缺陷修复。

本轮验收限于上述捕获槽与取消/恢复组合。payload 并发读写、无 await worker、
Task/嵌套 Future、复杂借用/构造逃逸、其他可失败原语及 native/callable、DLL
安全卸载/发布准入、legacy 清退与外部 XRT BUSY 问题继续保留。标准库仍是
最低必要基线；release CLI/DLL 和外部 XRT 未修改，长期任务继续 active。
## 2026-09-10：无 await worker 的捕获清理结果

生产源码 SHA256：DB2219FDEACC6A667711DAA3885CA92E11036D5D4753693EE27A1CBA89749328。
本轮是真实语言完成语义修复，不是单独追加测试。原 560D7817 的 505/505 不再
视为当前源码成绩；DB2219FD 的同源生成及影响面回归终态另行追加。

- 原始反例 9f8d95 退出 3：state=RESOLVED、drops=2、error=0；两个借用捕获
  都由异步 frame 最后释放，逆序第一个析构错误应为 99。源码、生成物、宿主、
  运行器及原生产实现保存在 build/tmp/xlir_worker_completion_v1_negative。
- 目标级 asyncCompletionAbi 统一要求 retirement 先于 publication；机器从
  目标继承并验证相同合同。无挂起任务直接在 worker 完成边界退役捕获、选择
  完整 `(ok,error)` 并回滚未发布结果。物理释放回调只负责未执行/未交付的
  后备清理或幂等 frame 回收，不再决定已执行 worker 的语言结果。
- 修复后的同一原始反例 6cd90c 退出 0：state=FAILED、drops=2、error=99。
  中间驱动只用于反例定位；正式验证由最终源码重新构建。
- 永久门 check_xlir_async_worker_completion.ps1 已接入核心函数 ABI suite
  和静态合同。最终门 97792 退出 0（5a6a25）：6 个任务池 worker、0 个挂起、
  5 种结果 ABI（int/void/string/object/closure），64 项 C/header 输出前拒绝
  变异；3,006 次严格运行、90 次真实 worker 结果 box 分配失败、3,006 个发布
  观察者、972 次析构期间取消。受管对象在发布前或其最后结果 owner 释放时
  恰好析构一次；每个 case 等待全部测试回调，整个批次在模块关闭并 join 所有
  worker 后检查内存。第二个 1,503-case 测量批次有 40,383 次平衡分配，整个
  进程初末 live count/bytes、double/invalid free、UAF 也平衡；不能把此分配
  计数误写为两个批次的总分配量。C-host JIT/AOT 各另通过 972 次冒烟，重复
  C/header/JSON/binding 逐字节一致。
- 分配故障在实际 worker 线程使用 XRT thread-local fail-after 注入；测试
  包装真实 allocator 只用于入口屏障与注入，不伪造 NULL。普通借用参数和
  consuming 参数均由源语言实际拥有并释放，宿主在屏障前放弃自己的借用来源
  引用，不靠宿主持有对象掩盖析构时机。

测试预期修正单列：最初错误地调用仍开放池的 xrtTaskPoolWait（280302、
d30683）；XRT 要求先关闭池。修正为等待公开 Watch/对象测试回调，并在批次
结束走生成模块的关闭/join 路径后检查内存；不把 Running==0 误当成物理作业
已释放。未删除任何清理/终态断言。

### DB2219FD 扩展与回归终态

新增“调用者提前丢弃 Future”场景，不安装观察器、不保留宿主引用，在 worker
尚未越过入口屏障时释放公开 Future。语言参数和返回的对象/闭包仍由 transport
最终回收。最终永久门 37451 退出 0（f7bbad）：3,978 次执行、90 次实际结果 box
分配失败、3,006 个发布观察者、972 次析构期间取消、972 次提前丢弃 Future；
第二个 1,989-case 批次有 52,983 次平衡分配，整个进程初末内存检查通过。
JIT/AOT 各 1,296 次冒烟（含 324 次丢弃），64 项变异和确定性保持通过。
之前 3,006-case 的已通过测试代码及严格运行器保存在 v9_passed，不覆盖证据。
10 轮独立进程复跑退出 0（be6207）：39,780 次执行、900 次真实故障、9,720 次
提前丢弃、9,720 次析构期间取消，每轮分别验证内存，不把测量批次的分配数
误加为全进程总数。

影响面六门 71254 均退出 0（674b20）：owned-activation（52 变异、1,100 次
故障）、module-task-pool（108 变异、210 次卸载、60 次自卸载拒绝）、pool-outcome
（540 次运行、21 次提交前故障）、frame-completion（4,800 次运行、1,200 次
故障）、result-commit（2,265 次运行、2,250 次故障）、cell-cancel-race（4,032 次
运行、1,080 次自由竞争、360 次强制重叠）。六门均包含严格 C、JIT/AOT 和确定性。

最终同源通用驱动 49527 退出 0（2dacd1）；快速生成 90227 退出 0（1e88be），
503/503。大型 cgen_task_api 的 39865 已退出 0（6fc04d）；大型 task_api 的
61595 也已退出 0（6041ef），当前 DB2219FD 同源生成达 505/505。静态合同通过（bdc86a），
新增文件空白/脚本语法及定向 diff 检查通过（69b626）。

嵌套 Future 源探针仍被前端明确拒绝（ad935e），源码和日志保存在
build/tmp/xlir_worker_completion_v3_rejected；它仍未实现，未计为本门通过。
本门也不代表无挂起 worker 所有 callable/native/payload 并发边界已经完成。
复杂借用/构造逃逸、其他可失败原语、DLL 安全卸载与同源发布准入、legacy
清退等继续保留。标准库最低必要基线不变，release CLI/DLL 和外部 XRT 未修改。
本轮源码、反例、专项产物与终态日志封存于 build/tmp/xlir_worker_completion_v15_snapshot，
索引为 build/tmp/xlir_worker_completion_v15_snapshot.json；长期目标仍为 active。

## 2026-09-10：动态迭代分配合同与提前关闭反例（专项未通过）

当前生产源码 SHA256：
B9C093EE30BCE2AAA16875D8A74F779E350F6DBFE60D6445AFA7C6BA3E84D9AA。
相对 DB2219FD 仅三个生产文件变化：xl6_ir.h、xl6_ir.c、
xl6_ir_cgen_emit_xvalue_core.c。标准库最低必要基线、发布 CLI/DLL 和外部 XRT
均未扩展或重建；新的生成覆盖与专项结果分别登记，不能沿用旧源码成绩。

- 原始动态 dict 键分配故障使进程 abort（b9463b，退出 3），负例保存在
  build/tmp/xlir_dynamic_projection_v2_negative。只补 runtime 错误 ABI 的中间
  修复被验证器拒绝（070229）：缺少目标构造失败的规范语义退出计划，不把它
  当作成功。Typed construction 效果与 runtime 物理失败 ABI 现已同时表达。
- 修复后同一真实故障反例退出 0（7fb3de）：返回 -100、循环体执行 0 次、
  finally 执行 1 次；不是测试宿主伪造 NULL。
- 永久 fixture 包含动态 array/list/dict/set、单/双目标、两个可变捕获 cell、
  break/continue/return、实际 await、yield 及源语言消费者 break。合同驱动
  验证 4 个 key + 1 个 logical 投影、2 个 cell、6 个构造效果、37 个退出前缀、
  async/generator 各一个游标 frame 槽，95 项变异在 C/header 输出前被拒绝。
- 严格 C 已完成分配阶段 4,940 次运行、3,620 次真实投影/cell 分配故障，含
  pending await 恢复后的 480 次故障。每个完成用例检查分配/释放、live count/
  bytes、double/invalid free、UAF；源级对象恰好析构，MemoryError 不被清理
  错误取代。非 dict 的单目标 retain 分支实测零分配，不人为注入失败。
- 完整严格运行仍退出 3（8519a2）：源码消费者 break 后 visits=1、cursor=1/1、
  drops=1/1，但 finally=0。95 项变异及生成确定性通过不能覆盖该运行失败。
  首次永久门退出 1（6829f3），明确停在同一 mandatory finally 断言；C-host
  JIT 和 AOT 运行也各退出 3（36e951，AOT 构建退出 0）。两者此前仅完成各
  52 次无分配故障冒烟，不能记为完整 JIT/AOT 通过或内存检查通过。

源程序及原始生成物/日志保存在 xlir_dynamic_projection_v11_negative 与
v15_negative。测试早期的错误单列：int generator item 必须 copy 而非 take
（878c8b）；物理 frame 关闭时当前 item binding 可能晚于游标 backing 释放，
不应要求每次对象析构都发生在游标 destroy 的调用栈内（57a0c8）。修正这些
测试假设后仍保留且复现 finally 缺失，没有降低语言退出断言。早期 smoke 输出
误用了 memory-balanced 的描述，现已明确标为 memory checks disabled；只对
实际启用内存调试的严格用例记录内存成绩。

check_xlir_dynamic_iteration_allocation.ps1 已接入 03_core_function_abi 和静态
合同，其完整运行当前为失败；不得通过删除提前关闭断言把本门标绿。下一工作
是补齐 verifier 可见的 generator 关闭入口、退出计划与关闭路径活跃性，同时
保留 finally 内 yield 和已有 pending outcome 的语义问题，不降级为 frame-free。

### 同源回归接线修正与进行中记录

初次旧循环清理门因覆盖正在映射的同名 xl.dll 失败，未进入语言测试（53806d）；
该脚本还原本用 release/xl.exe 生成 IR/C。现在合同驱动可输出当前源码的
Typed/Lowered JSON、C 和 header，永久门先从源码编译驱动并运行全部变异，再
使用同一 IR 生成产物，重复验证确定性及严格 C/JIT/AOT。相同 DLL 哈希不再
重复覆盖。发布版 CLI 仅用作 C-host 工具，不用于冒充当前 XLIR 编译器。

本轮新生成驱动为 xlir_dynamic_projection_v4_driver.exe，最终源码编译终态
f00302。快速 503 样例已全部通过（b8bb67），大型 cgen_task_api 也已通过
（94dab6）；大型 task_api 尚待终态。循环 cell 与挂起 cell 两门在 53806d 中
先行通过，后续四门以新日志独立运行，未完成前不计为通过。

大型 task_api 现已获得退出 0 终态（576746），B9C093EE 的既有生成覆盖达到
505/505。最终新增门仍退出 1（ad1bc8），分配阶段 4,940 / 3,620 / 480 的结果
保持，源级提前关闭 finally=0 的失败保持。修正 smoke 日志描述后的 JIT/AOT
复跑仍各退出 3（753540），不是已通过成绩。静态合同通过（f56029），新增文件
空白与脚本语法检查通过（13223c），编码检查 6,125 文本通过（632908）。

剩余回归会话 3530 尚未获得终态，正在执行当前源码的循环清理变异合同；后续
owner/boxing/string 三门仍排队，不提前登记通过。拟封存脚本
build/tmp/xlir_dynamic_projection_v18_freeze.ps1 已准备，但 snapshot 目录和
JSON 清单在回归完成并核验前不成立。完整 generator 关闭语义未实现，长期目标
继续 active，不以当前分配阶段或 505 个生成样例重新定义完成标准。

### B9C093EE 回归终态

会话 3530 现已退出 0（00ab56），四门全部通过。源码构建的循环清理门两次分别
拒绝 2,526 个畸形 IR，36 个物理 owner、98 个事务、363 个释放点；运行通过
12,000 个 case、2,000 次真实诊断分配故障、679,200 次平衡分配，JIT/AOT 各
10,000 次冒烟。owner consumption 保持 470 项变异和 47 CFG 检查；boxing
通过 6,240 次实际分配故障，string allocation 通过 1,100 次故障。六项影响面
回归终态齐全，但新增关闭门仍失败，两者并不矛盾。

后续独立关闭路线探针（e059b4）19 次源语言消费运行中有 10 项语义失败，包括
普通提前关闭、暂停在 finally、嵌套 finally、待完成的带诊断/无诊断异常及
finally 中的 managed yield。整个探针内存检查平衡，进一步证明物理 owner
释放不能替代语言清理与结果传播。源码及负例在
build/tmp/xlir_generator_close_v2_negative，仍以 B9C093EE 驱动生成，未改为
预期拒绝或通过。本记录不声明关闭实现已完成。

### Generator close 首次正向验收（2026-09-10，v9）

生产源码新增显式关闭点/计划/指令及机器 ABI，复用已存在的非异常退出 lowering。
旧负例快照保持不变。首轮 C 编译后发现并修正新增计划统计遗漏、清理发射入口
仅接纳 BEFORE_NODE，以及 Cgen 指令接纳表缺项；未放宽原有 finally 断言。

- 关闭路线永久门：会话 19543 退出 0（c79633）。两次各 117 项 IR 变异在 C 和
  header 输出前拒绝，含反接分支且同步重建机器缓存。8 个显式关闭检查、10 个
  清理体 yield、9 个 typed close 计划。严格 C 的 19 次执行语义全部通过，逐例
  及整个进程内存平衡；JIT/AOT 各 19 次通过，其内存 debug 明确禁用。
- 原动态迭代永久门：会话 86677 退出 0（5b373e）。两次各 97 项变异，38 个
  退出前缀；5,180 次执行，3,620 次真实分配故障（480 次在 pending 恢复后）、
  160 次 generator 提前/耗尽关闭，135,370 次平衡分配。JIT/AOT 各 76 次
  冒烟（含 16 次关闭），不把 smoke 计入故障/内存 debug 统计。
- 源码静态合同退出 0（334757）。永久门已接入 03_core_function_abi，但未执行
  common.bat，未重建发布 CLI/DLL，也未修改独立 XRT 仓库。

本记录写入时，同源码 v9 通用驱动的 503 个快速生成、两个慢任务样例，以及
generator frame loan、cell loop/continuation、owner consumption 回归仍在运行；
不能提前登记 505/505 或完整影响面通过。后续终态应另行追加。

### 7BE15F68 终态与嵌套正常合流回归

v9 生产指纹为 7BE15F68F151FDE1F17211009B231EFDE783501D77C95936CBBD7E183581A17B。
快速 503 生成退出 0（832eef），两个 task 样例分别退出 0（d2be70、e68e68）；
四项影响面回归会话 37663 退出 0（e4a6ed）。generator frame loan 基线/优化各
3,600 次运行，cell loop 5,480 次真实分配故障，cell continuation 2,700 次
执行加 4,200 次析构场景，owner consumption 470 项变异均通过。另起的循环
清理会话 99894 仍在验证此 7BE15F68 源码，不能登记为后续修正源码的结果。

扩展源程序发现一个真实回归：旧 B9 驱动接受（2d6474），v9 驱动在
closeProtected 的 generator 状态机构建失败（d445a4）。隔离诊断副本证明
7 个发射 yield 中只有 6 个可达（da1e65、95e9bb）。原因不是 frame 类型不支持：
嵌套 finally 的 deferred-return 分支转交外层后，lowering 提前结束了整个
try 的处理，即使普通合流块仍有前驱。普通 post-try 代码因此被放进不可达块。
该版本相对 B9 改动的六个生产文件及扩展负例封存于
build/tmp/xlir_generator_close_v15_nested_negative。

三个 try/finally lowering 入口改为仅在普通合流没有前驱时采取原先的提前结束，
不减少 yield 计数、不忽略不可达指令，也不修改 Cgen。修正源码指纹为
CCBE50FFF7930157C7AA7096C11499F32166DB3EADC903F466DF7E130DAB902C。
v16 驱动重新接受扩展程序（13bce0），31 次严格 C 源级运行语义和内存检查
全部通过（431db9）。

### CCBE50FF 已完成专项门，完整重验仍待终态

- v17 关闭永久门会话 30913 退出 0（7a0daa）：基础 19 次与扩展 31 次严格 C
  运行全通过，各自逐例及整个进程内存平衡；JIT/AOT 各运行两组 50 次通过，
  smoke 不启用内存 debug。两组分别拒绝 117 / 127 项变异，两次生成结果一致。
  扩展覆盖未启动即关闭的 string 捕获、嵌套 protected try、catch 内关闭、
  清理自身失败及 false/NULL 失败；没有以禁止 finally 内 yield 规避问题。
- v17 动态迭代门会话 28966 退出 0（9f265c），保持 5,180 次执行、3,620 次
  真实分配故障、160 次关闭及 135,370 次平衡分配，JIT/AOT 通过。
- generator 的 var 参数仍被前端拒绝。原始源及诊断保存为
  build/tmp/xlir_generator_close_v10_var_rejected.xl/.log。未启动的正向回归使用
  已支持的 string；这一类型接纳限制仍是未完成项，不算测试通过或被豁免。

此记录写入时，CCBE50FF 的生成会话 68657 / 39135 / 10479 和四门影响面回归
51570 仍在运行。关闭事务的更多结果优先级、重复逻辑关闭、完整路径畸变拒绝、
复杂所有权/借用、native/callable、并发载荷、nested Future、DLL 安全卸载、
同源码发布准入与 legacy 退役仍须继续。长期目标保持 active。

### CCBE50FF 生成终态与必须保留的边界删除反例

v17 快速生成退出 0（fa38e8），两个慢样例退出 0（08f2d0、fd9030），同源码
既有生成覆盖为 505/505。四门影响面回归会话 51570 退出 0（29632e）。
较早 7BE15F68 的循环清理门会话 99894 也退出 0（93854f）：两轮 2,526 项
变异、12,000 次运行及 679,200 次平衡分配。它不是 CCBE50FF 的循环清理重验。

继续增加的独立反例把 AFTER_TRY 的 CLOSE_TEST 换成恒假 bool，清除关闭元数据，
并重建全部 generator machine 缓存。当前 verifier 与 C emitter 都接受
（eda027）。编译这份 C 并重跑基础 19 个场景，1 项语义失败（dc1ed6）：
暂停在 finally 后关闭，仍执行了后续普通语句 mark(30)。这不是内存泄漏；
反例的内存检查仍全部平衡。

上述变异已加入永久 close 合同，不把它记作预期接纳。更新后的质量门会话
60088 退出 1（2d0465），在 C/header 输出前的预期拒绝断言处失败。因此
当前状态是 FAILED_CLOSE_BOUNDARY_DELETION，而不是“244 项变异证明了完整关闭”。
需要独立验证从 cleanup suspension 到关闭边界/既有 pending terminal 的路径，
包括关闭检查不能被一致删除；只校验剩余指令字段与缓存一致性仍不充分。
还应继续审查 finally 中合法 break/continue 对关闭边界的影响，不能用某一种
畸变指令名称的黑名单替代控制流合同。完整长期目标继续 active，发布物不变。

### 2026-09-10：完整关闭路径、finally 跳转与处理器完成边界

CCBE50FF 的边界删除负例已由从每个挂起点出发的 closing=true CFG 验证拦截，
不是通过要求某个变异指令名称、只检查缓存或禁止 finally 内 yield 处理。
另外保留的 abrupt finally 源程序用旧驱动运行 8 次有 3 项语义失败（515ef7），
原始源、host、runtime、生成 C 和失败日志封存于
build/tmp/xlir_generator_close_v26_abrupt_negative。新 ABRUPT_UNWIND 计划令离开
活动清理区域的 break/continue 在关闭状态下走 RETURN 路线；内部循环跳转保持
正常清理控制流。共享 finally 剩余路线按退出计划种类区分。

首个实现的基础/扩展关闭门通过（c4dc02），30 项内外循环运行通过（f5687b），
但快速生成有两项新拒绝：catch 绑定 STORE 粗粒度归属整个 try。修正绑定归属后
两项恢复（080f1a）；独立 typed catch 探针又发现过滤 LOAD 的同类问题
（c596e9，旧驱动接纳）。最终统一修正单/多处理器的绑定、分派存储和类型过滤
归属，无需放宽验证器或修改 Cgen。该阶段源码 48173F012BD5D2D1B9226429EAF4A944B5469239319B73E5865EDA93D28CBEDE
通过 505 个既有生成样例、160 项关闭运行与 263 项变异（0b1e55），动态迭代和
owner consumption 也通过（facc68、a489f5）。这些不是后续源码的最终验收。

不含 yield 的 catch 正常结束探针再次被独立路径检查拒绝（a00a98）：清理失败
进入处理器后，正常合流仍可执行 mark(90)。Typed AFTER_TRY 因此补充有处理器
且含挂起点的 try，不能只依据 finally 自身是否 yield。原探针保留于
build/tmp/xlir_generator_close_v34_handler_completion_probe.xl 和 v34_handler.log，处理器正常
完成回归已扩充为单/多类型 catch、有无 finally、有无 handler yield 的 8 种源形态。

最新源码指纹为 5F71A07CE2530ADFD66C548A7DB351B869B169FDD6B16A950D9B523B04FC1EC4。
v35 驱动恢复上述源探针，处理器专项 160 次严格 C 运行通过（42cacb）。v36
关闭永久门会话 76131 退出 0（d5ebd4）：基础 19、扩展 31、内外循环 30、处理器
160，共 240 项源级运行全部通过，各组逐例和全进程内存平衡；JIT/AOT 各 240
次冒烟通过，smoke 明确禁用内存 debug。四组每轮拒绝 118/119/12/32 项畸形 IR，
合计 281 项，C/header 输出前拒绝且两次生成确定。新增变异包含删除同一个跳转
的所有 finally 副本、删除四个非挂起处理器边界，并重建全部机器缓存。

RuntimeError 对 false/NULL 失败的匹配遵守已有 error-kind ABI（无诊断归入
RuntimeError），不是新改动。处理器 host 的初始 oracle 错将其判为不匹配，导致
80 次中的 20 次预期错误（bdbb6a）；初始 host 与日志保留在 v31 文件中。核对
既有映射后修正 oracle（a65ec0），未变更生产错误分类，也未豁免失败位或处理器
执行顺序检查。其他无处理器的 false/NULL 保真仍由原关闭/动态迭代门覆盖。

此记录写入时，最新 v36 快速生成 503 项已退出 0（bae5ee）。v38 影响面序列
46582 中动态迭代（5,180 次运行、3,620 次真实分配故障、160 次关闭、135,370
次平衡分配）和所有权消费（470 项变异、47 CFG 检查）已通过，后续 frame/cell
三门尚在运行。v38 rollback/error-unwind 的严格 C、JIT/AOT 运行已通过，其中
rollback 14,800 次、4,000 次诊断分配故障；unwind 37,600 次、12,500 次故障。
其余变异检查会话 13985、循环清理完整重验 8625，以及两个慢生成 26505/55998
仍待终态。不能把旧 48173F01 的影响面终态或已有输出文件当成新版本完成证据。

下一次应先接收这些已在运行的句柄终态，再封存最终验收；不要重复启动同一组
进程。关闭事务的消费者既有失败、重复逻辑关闭和更复杂 handler/abrupt 组合，
以及 generator 类型接纳、完整所有权/借用、native/callable、并发载荷、嵌套
Future、DLL 安全卸载、同源码发布准入和 legacy 退役仍属长期目标。标准库保持
最低基线；未执行 common.bat、未重建发布 CLI/DLL、未改独立 XRT 仓库。

5F71A07C 后续终态：两个慢生成分别退出 0（3b362d、4b16f1），与 bae5ee 的
503 项合计 505/505。五门影响面序列 46582 全部退出 0（376e1f）：动态迭代、
所有权消费、generator frame loan、cell loop binding、cell continuation outcome。
Frame loan 基线/优化各 3,600 次；cell loop 5,480 次真实分配故障；continuation
2,700 次运行加 4,200 次析构场景，内存及 JIT/AOT 均通过。v39 检查点封存时
这些句柄尚待接收，因此保留原清单不改写；474 项源码/证据记录及源指纹已独立
复核（efb972）。源码编码检查退出 0（b09e78，6,141 个文本文件）。

下一 goal turn 复查时，剩余会话 13985/8625 已不可用，系统也没有对应编译器或
合同进程（66ac04）。v38 impact 日志只到运行成功，未留下最终 unwind 变异通过
标记；loop cleanup 尚无首条日志。因此这两项终态不能补记为通过，需在原进程
已停止的证据下补跑。新验证应将 stdout、stderr 和最终退出码独立持久化，避免
把句柄消失误当作成功；已完成的 505 样例与五门不重复启动。

### 2026-09-10：5F71A07C 长门终态与批次收敛切换

v40 补跑已真实完成。unwind 持久 receipt 退出零，源指纹前后一致为 5F71A07C：
465 个区域、690 个成员，4,007 项区域/身份/成员变异及 86 项立即结果变异拒绝。
loop receipt 也退出零：两轮各 2,526 项变异拒绝，12,000 次运行、2,000 次真实
诊断分配失败、679,200 次平衡分配，JIT/AOT 各 10,000 次通过。记录分别在
`build/tmp/xlir_generator_close_v40_unwind.receipt.json` 与
`build/tmp/xlir_generator_close_v40_loop.receipt.json`。不将失联的原句柄补写为成功。

同一 5F71A07C 的 v43 关闭门通过：已有 240 项运行之外，新增 900 次消费者/
清理错误优先级运行和 2,700 次重复逻辑关闭序列，各项及全进程内存平衡。
五组每轮合计 290 项畸形 IR 拒绝，严格 C、JIT/AOT、C/H/IR/binding 确定性通过。
详见 `build/tmp/xlir_generator_close_v43_gate.log`。v39 快照保留原封存时的 pending
状态，不改写历史证据。

按用户新的执行要求，后续采用 [XLIR 批次收敛流程](XLIR批次收敛流程.md)。
新增 `tools/run_xlir_batch.ps1` 支持显式清单、失败后继续收集、独立日志与退出码、
批次输入指纹、证据防覆盖和同工作区独占句柄。入口自测真实通过（27af22）；
一次原子报告写入因 PowerShell 将 null 转为空路径失败的反例已修正并保留日志。
5F71A07C 的新入口实际静态门也退出零（d9cf98），3,785 项输入指纹前后一致。

首轮 10 个新缺口探针在正式与隔离驱动上共执行 20 次，归并为 5 类根因。
随后生产修复结果值接纳和 yield 元素转换效果，源码变为
`356CBF6251BFD3C34B1235A75450D6BFF2379144839D01283C438553288A9D1B`。
同组正式源码生成失败由 9 项降为 5 项；恢复的对象、array、函数值和 var 四组
统一严格编译、各运行 100 次且内存平衡。该阶段证据和未覆盖范围详见批次文档。

356CBF62 仍有初始化执行上下文、var 捕获、Future 产出等真实缺口；全量生成和
重型影响面尚未重验，旧源码结果不可移用。完整所有权、并发、DLL 安全卸载、
同源码发布准入及 legacy 清退仍保留。未重建发布 CLI/DLL，未改独立 XRT，
未运行 common.bat；长期目标未完成。

### 2026-09-10：CC92C4BE 实例化/载荷批次

新增可验证的字段实例化 arena，分离声明诊断来源与实际执行来源，关闭 CFG
只使用已验证的执行上下文。generator 的 var 捕获复用语言结果值规则，并新增
Future 产出 MOVE_FUTURE 合同；Cgen 机械消费 yield ABI 选择释放方式。
同一组 10 个缺口探针的失败数从 9 经 5、2 降至 0；不是全仓问题已清零。

当前 358 文件源码指纹 CC92C4BE 的 505/505 既有生成已全部退出零（v20 三份
receipt）；v22 载荷/实例化两门通过且 3,793 项输入不变，合计 3,600 次运行
内存平衡，两轮各 264 项变异拒绝。另有 v23 载荷生命周期 2,400 次严格运行
通过，覆盖释放调用方引用后保活、未启动销毁、提前退出及析构错误传播。
完整源码哈希、驱动及证据链见 `build/tmp/xlir_generator_payload_v25_checkpoint.json`。

新增生命周期用例与一致伪造 Future 所有权反例已并入固定门，v24 十门影响面
批次正在统一验证；未结束项不计为通过。pending/cancel、真实分配失败和更多
语言类型组合尚需补齐。详见 [批次收敛流程](XLIR批次收敛流程.md)。
发布 CLI/DLL 与独立 XRT 未变；长期目标仍进行中。

CC92C4BE 后续：v24 九门已通过，loop cleanup 尚待终态；加强后的 payload 门
两轮各 34 项变异及 3,000 次运行通过。等待长门期间隔离发现新增三类问题：
error 和嵌套 generator 的产出 ABI 缺口，以及 generator 创建分配失败虽然
进入 catch 却因外层失败位已锁存而继续传播。后者在 int/var 两种产出都复现，
而 yield 装箱失败的捕获路径通过。详细证据与下一批修复边界见批次收敛文档。
这些新发现均未豁免，不能把原十探针零失败解释为所有语言问题解决。

### 2026-09-10：A97FAF35 launch outcome 与影响面复验

CC92C4BE 的 v24 十门现已全部终态通过。随后新增 error/嵌套 generator 的
显式产出转移/销毁 ABI，统一 async/generator 创建的 RESULT_CARRIER；创建
失败在局部 scope 收束，由调用方 ERROR_TEST 决定控制流，捕获成功不再污染
外层失败位。成功创建保留调用方原诊断也有固定回归断言。

当前 A97FAF35 源码完整快照见 `build/tmp/xlir_launch_v50_checkpoint.json`。
v47 四门（静态、payload、virtual activation、owned activation）全部通过，
3,810 项输入不变。payload 新十五 fixture 合计 3,910 次运行/模块生命周期，
380 次真实分配失败；virtual/owned activation 分别追加 900/1,100 次创建
分配失败，并通过严格 C/JIT/AOT。完整口径见批次收敛流程。

前一 BBAC0CBD 源码已通过 v43 的 505/505 生成；A97FAF35 尚未全量重验。
v49 十八门当前收集为 11 通过、6 失败、1 未终止，不计作整批通过。
未解决项包括共用测试头缺少 assert 声明、三门合成 SSA owner 关闭路径表达，
以及网络 native Future 布局错误（当前源码在三个网络/TLS fixture 复现）。
后两项不可通过放宽 IR verifier、只改测试期望或跳过标准库 fixture 消除。
发布物与独立 XRT 未变，完整语言语义和发布验收仍在进行。

### 2026-09-10：13F4FAC7 SSA owner 销毁与五门复验

v49 十八门终态为 12 通过、6 失败，所有门均执行且输入未变。新增
VALUE_RELEASE，显式消费单个拥有型 SSA 值并在销毁前解除物理拥有关系；
精确类型、抛错 scope、借用失效和 continuation 转移共同验证，不再在 generator
关闭路径拼接 STORE+RELEASE。测试头自包含缺陷同时修复。

v54 的 owner views/CFG/calls、terminal outcome、match cleanup 五门全部通过，
含严格 C、真实分配失败、内存检查及 C-host JIT/AOT。静态门的辅助器旧名称
检查在批次结束后修正并独立通过；保留原始 v54 的一项失败记录。当前 358 项
源码快照为 `build/tmp/xlir_launch_v56_checkpoint.json`，指纹 13F4FAC7…A5F4DD6。
原六个失败门已收敛至一个网络准入门，而不是全仓只剩一个问题。

剩余网络失败已定位为 NetBytes native destructor 的 MAY_RAISE/FOREIGN_SCOPE
合同与网络 wrapper 的 NO_RAISE 限制不一致。不能改变声明效果来掩盖差异。
VALUE_RELEASE 本轮运行主要证明 text SSA owner；其他类型、可能抛错析构、
重入/无诊断失败仍需专门质量门。505 生成与发布物未在此源码全量重验。
详见批次收敛流程的新节；长期目标未完成。

### 2026-09-10：0B2D5C48 native/network 销毁合同

native handle 与网络 Future 结果包装共用同一已验证析构 callback，保持声明的
MAY_RAISE/FOREIGN_SCOPE 合同、清理失败位和首错误，不再硬编码替代 native
destructor。当前源码快照见 `build/tmp/xlir_launch_v62_checkpoint.json`。

native TAKE 固定门已使用当前源码驱动，通过 2,000 次真实分配失败、重入替换
拥有权及新增 600 组合的正常/诊断失败/false-NULL/首错误检查，严格 C/JIT/AOT
通过。新 network wrapper 门三个 fixture 的确定性 C/H/JSON 与严格 C 通过，
报告 `build/tmp/xlir_launch_v61_network_wrapper.json`。这只关闭原生成错误。

真实运行准入仍不通过：net_fast_path 缺 resolver/UDP 链接组件，net_send_file
缺 string-repeat 桥接，TLS 在连接阶段报类型不兼容。完整记录及下一批根因组
见批次收敛流程。不能用编译通过代替运行，也不将发布 CLI 的结果当作当前源码。

### 2026-09-10：DB418173 四门批量复验与新调度缺口

补齐 string-repeat 的精确 IR bridge/组件选择；共享 TLS 便利层显式接收由
verified class layout 决定的 stream/bytes 借用投影，不再将类对象整体读作句柄。
同源码 v70 批次的静态、repeat、network wrapper、native TAKE 四门全部通过，
输入指纹前后一致。repeat 含 600 组/300 次真实分配失败；TLS 投影含 850 组/
350 次真实分配失败，内存平衡；native 门保持严格 C/JIT/AOT 与首错误检查。
完整 358 项源码快照为 `build/tmp/xlir_launch_v71_checkpoint.json`。

正式网络运行仍不通过：fast_path 缺 resolver/UDP 组件；send-file 与 TLS 各
三次实际运行均失败于等待。TCP 的八次隔离复现明确报告网络 worker 不可等待
自身操作；await 通知回调直接执行语言 resume，暴露了恢复执行位置合同缺口。
TLS 同组待查，未冒充已取得相同底层诊断。不能靠改断言或禁止合法同步调用收口。
下一批优先建立 IR 可验证的 continuation 派发/执行位置及其拥有权、错误、取消
和卸载边界。repeat 与类投影的原缺陷关闭，不代表网络整体准入通过；新增问题
独立记账，完整语言、并发、发布及其他遗留要求均保留。

### 2026-09-11：外部调用准备失败拥有权合同补齐

`4A38936E` 源码新增显式 preparationFailureAbi，统一直接调用和协议 witness
的输入退休：CALL 已消费但 native 未进入时，逆序释放 OWNED 输入、保留借用
及首失败（包括 false/NULL）。原始两条泄漏路径关闭，600 组固定检查通过；
协议严格 C/JIT/AOT、360 项执行/准备合同变异及受影响门复验通过。

v80 同输入八门七绿一红；保留的异步隔离红门仍是 200 个 pending 用例，不是
允许跳过项。完整报告 `build/tmp/xlir_launch_v80_prepare_batch.json`，源码
快照 `build/tmp/xlir_launch_v81_checkpoint.json`。执行器仅独立运行时基础层，
生产恢复/阻塞准入尚未接入，网络集成和全量发布准入不能借用这些局部结果。

### 2026-09-11：宿主退休及 continuation 代码引用局部准入

v87 源码增加宿主待退休所有权、checked 清理失败保持、入口/代码 lease 屏障、
可选 CodeOwner 绑定及 owned ticket 后继转交。最终 frame 释放返回之后，才由
常驻运行时归还可触发卸载的代码引用。v86 冻结批次七门六绿一红，原模块销毁
失败门已修复；异步隔离仍有 200 个 pending 线程违规，继续计入真实失败。

退休固定门覆盖 400 组事务、100 组代码引用与真实依赖卸载、200 组 owned ticket
生命周期；TCC 删除为记录器，不能冒充实际 JIT 页释放。源码 API 单独编译，
发布产品未重建。完整批次及源码分别见 `build/tmp/xlir_launch_v86_module_batch.json`
和 `build/tmp/xlir_launch_v87_checkpoint.json`。当前生成器没有绑定 CodeOwner
或接入这些派发函数；NW-04、阻塞准入、逃逸代码寿命和完整发布验收仍未关闭。

### 2026-09-11：生成 CodeOwner 绑定接入，生产派发仍未准入

v89（6F0F90C5）将模块代码能力的 bind-once-before-init 纳入 ModuleAbi，
lowering/verifier/dump/C/header 一致。生成绑定实际接受源码宿主能力，首次
初始化尝试后永久封闭更换；未绑定、失败初始化、重激活、扩展前缀等五种模式
均通过。10 个 IR 变异与 2 个保留符号冲突源文件在 C/header 发出前拒绝。

v88 同输入十二门十一绿一红，详见批次收敛流程。四模块严格 C 生命周期 fixture、
源码宿主/队列/交接、470 个 owner 拒绝变异及模块 task pool 源码合同和生成 C
严格/JIT/AOT 均通过。测试宿主主动以入口 lease 预留 owned ticket，两个后继
阶段实际调用语言 export，最后 release 返回后才请求代码删除；生产 async
尚未自动使用这一能力，TCC 删除亦为记录器。200 个 pending 隔离违规继续失败，
不得借绑定已实现关闭 NW-04、阻塞准入、逃逸代码寿命或发布验收。

### 2026-09-11：通用阻塞的根 activation 准入运行时证明

v91（D33B9E11）新增有界 Guaranteed 执行器策略：根发布前预留足够 worker，
后继复用，不给普通 foreign/NO_RAISE/析构临时增加可失败的准入。最后 frame
返回后的宿主退休 hook 也保留执行信用直到返回；旧信用提前归还的对照产生
五秒等待失败，新实现通过。策略最坏线程高水位为接收根数，必须保留显式资源
预算；不是无限并发或语言等待环的解决方案。

新固定门覆盖 100 组四根阻塞链、50 组四线程并发准入、100 次真实分配失败和
75 组边界检查，内存平衡。v90 同输入八门七绿一红，原宿主/生成绑定/队列等
门通过；生产异步仍有 200 个 pending 隔离违规。IR 的执行服务/根准入合同和
Cgen 的实际派发尚未接入，这一局部运行时证明不能关闭 NW-04 或完整发布准入。

### 2026-09-11：生产派发已接入，隔离转绿，失败载体仍开放

v93（4494EF34）通过 AsyncMachine.executionAbi 与 ModuleAbi.continuationAbi
接入常驻 Guaranteed 执行器；owned root、后继信用、提交、模块激活及 busy
退出进入实际生成 C。隔离门 pending/ready 各 200、共 400 用例全部通过。
新增派发门两容量十二 lane 中八个（200 次）通过，四个在无诊断注册失败
断言停止；共同根因为旧 setup-failure 补造 MemoryError，缺少独立失败载体。

v92 十四门全收集十二绿两红；静态门旧 Watch 位置检查修正并加强后，v94
仅重跑该门，核验其他输入不变后复用原结果，十三绿一红。没有减少测试分母、
跳过失败或按门数宣称整体完成率。新 lane 的失败不归为隔离回归。
详见批次收敛流程及恢复派发合同。NW-04、实际生成 async 联合宿主卸载、
原综合 terminal/cancellation/owner 门、TCP/TLS、逃逸代码及发布同源仍开放。

### 2026-09-11：AWAIT 状态合同收敛，当前十五门全绿

v97（875A92E1）建立 Instruction.awaitOutcomeAbi：payload/成功位/诊断独立，
setup failure 与 source lease 清理保持 false/NULL；阻塞、generator、machine
共用 typed acquisition，删除旧的各类型包装。14 个新 IR 变异在 C/header
前拒绝；派发 300 次、阻塞/generator 模块事务 300 次、原隔离 400 次通过。

上一轮 NULL 断言明确限定在发布前：XRT PromiseReject 不能接收 NULL，最终
Future 使用既有 INTERNAL 映射。新增清理前观察器和成功语句计数，旧 v92
驱动在同一加强测试中失败，新实现通过，不能把边界校正当成跳过缺陷。
v96 十五门完整同输入全绿，报告和快照详见批次收敛流程。相关 Cgen 源码净减
8,524 字节，非二进制测量。联合代码卸载、原综合 async 门、TCP/TLS、逃逸
生命周期及发布同源仍未验收，不以十五门全绿替代长期目标完成。

### 2026-09-11：联合生成 DLL 退休，逃逸结果仍为红门

v99（96A44564）补齐准备失败的空输出合同，并验证 200 次重复 CALL 事务。
新生成 DLL 联合源码宿主和常驻执行器，五场景共 125 次真实卸载及内存检查
通过；保持已完成 Future 载荷的第六场景，仍会在析构代码被引用时尝试退休。
测试在 FreeLibrary 前拒绝，不实际触发 UAF。这里使用测试专用规范机器入口，
不是受支持的语言 Future C 导出，也不是实际 TCC 重定位/删除的替代证明。

v98 十八门全收集十七绿一红，旧十五门保持全绿。新失败是逃逸结果代码寿命
的内部 ABI 准入缺口，未通过缩减分母消除。下一步要覆盖共享代码能力、closing
继承和模块自持结果形成的环；完整网络/其他 async/发布同源范围继续开放。

### 2026-09-11：共享驻留能力基础层通过，尚未切换生产

v101（0489633D）增加通用共享 CodeLease/CodeDrop，350 次真实 DLL 生命周期、
20 万对并发引用、100 次真实分配失败及独立 JIT/AOT smoke 通过。最后代码
释放在生成析构返回后的常驻代码中；closing 后只允许已有能力派生。

v100 六项本轮验证加十三项精确输入证明复用，账本十八绿一红；原生产 Future
逃逸模式仍失败，没有移除。IR/Cgen 尚未接入，module unit 自持值的环、从
Future 取出对象后的代码寿命及全局资源依赖仍须共同解决。基础组件通过不能
替代生产红门，也不证明完整 async、网络、逃逸或发布验收。

### 2026-09-11：普通结果载荷寿命已转绿，代码相关对象仍开放

v105（5E4523B7）将五组普通 Future box 的释放置于常驻运行时，由既有
AsyncResultAbi 统一选取。宿主 accessor 返回真实常驻地址，避免 Windows
导入跳板落在待卸载 DLL；没有给 Cgen 新增 AST 追溯或语义猜测。

v104 十五项相关门全部新跑且同输入通过。联合模式 3 现在在 DLL 卸载之后
读取和释放结果；六模式共 150 次真实卸载。五类型 payload 1,500 次精确
内存事务及各自独立 JIT/AOT、私有源裁剪门通过。另五项未重跑的 runtime
门只保留历史记录，不能计作本轮新通过项。报告见批次收敛流程。

closure/class/native finalizer、从 Future 取出对象后的代码引用、模块自持
引用环及全局资源依赖仍未验收；CodeLease/CodeDrop 尚未接入这些生产对象。
one-shot TCC 路径、其余综合 async/网络门、Linux 私有头接线及同源发布仍
开放。发布 CLI/DLL/VFS 没有重建，不以局部十五门全绿宣称长期任务完成。

### 2026-09-11：一次性运行不再绕过模块退休

v107（6AEB38B9）删除直接脚本运行的独立裸 TCC 删除路径，复用模块编译/
绑定、checked 主入口、入口引用及 resident retirement queue。存在打印
hook 的脚本错误现在也保存为宿主拥有文本，而非打印后丢失。

新 one-shot 门七场景 175 次真实 DLL 卸载通过，验证 pending/ready、初始化/
main 失败、取消、全局对象延迟清理、无 async 成功退出码及卸载后错误文本。
v106 七项新跑加九项精确输入证明复用，十六项账本全绿；不是十六项重跑。
另五项更早 runtime 门仍为历史记录。编译器/IR/Cgen 未变，继续复用 v104 驱动。

新门从已重定位状态起测，共用的是实际源码执行/退休逻辑；实际 TCC 全流程
与发布产物仍未验证。code-bearing 对象、模块自持引用环、其余 async/网络、
Linux 接线及同源发布依然开放，不把入口统一算作对象寿命设计全部完成。

### 2026-09-11：现用 TCC 快照的页退休已实测，Linux 私有头接线补齐

v109（生产源码仍为 6AEB38B9）新增真实 TCC one-shot/async 两门，共 325 个
执行事务验证 compile→relocate→checked execution→retirement→delete，
入口地址最终确认为 MEM_FREE；含卸载后保留 Future 载荷及全局清理顺序。
显式识别首次环境的 16 字节 TCC 路径记录，后续每事务 XRT 内存精确平衡。
原真实 DLL 两门也保持通过，没有替换测试分母。

Linux 两清单的 12 个私有头缺口已关闭，四份 manifest 共 24 个头映射及
20 处源码清单由新门覆盖。v108 六项实测、十三项经输入证明复用，十九项
账本全绿；其余五项更早 runtime 历史记录不冒充本轮验证。

这里使用现有发布 TCC/XRT 环境加当前源码 generator/dispatcher/host，
不是同源 release 重新发布，也不是从公开脚本 API 起始的完整前端验证。
Linux 构建/执行、对象代码身份、自持环及其他 async/网络覆盖仍开放。
