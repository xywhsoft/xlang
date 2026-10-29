# xlang demo6 XLIR C 发射器边界

本目录只记录当前 `AST -> XLIR -> C` 管线的 C 发射边界，不再维护按源码语法分支编写的第二套代码生成规则。语言语义、类型、调用绑定、控制流、所有权、异常和挂起恢复必须先进入已验证的 Typed/Lowered XLIR；C emitter 只把这些事实机械映射为 C。

## 当前源码入口

- `src/cgen/xl6_ir_cgen.c`：统一 XLIR C emitter 的 unity translation unit；其 `xl6_ir_cgen_*` 分片按验证、需求分析、函数/指令发射、异步与 generator、header 和模块装配划分职责。
- `src/cgen/xl6_ir_cgen.h`：C/header 发射的公开编译器内接口。
- `src/cgen/xl6_cgen_shared_runtime.c`：生成程序使用的共享运行时组件 emitter；buffer、file、network、TLS、WebSocket、JSON/XLON 等组件只输出已由 XLIR artifact plan 选择的运行时桥，不解析 AST，也不决定语言语义。

`src/cgen` 中保留 `xl6_cgen_*` 名称的文件只属于上述共享运行时组件体系，不是旧 AST 后端。它们由 `xl6_cgen_shared_runtime.c` 统一装配，供 XLIR emitter 复用。

## 不变量

1. C emitter 只能读取 `XL6_IrModule` 及其已验证的 TypeId、ABI、CFG、effect、所有权和 artifact plan。
2. 不读取 AST/HIR，不按源码名称或表达式形状重新做解析、类型推断、重载选择、派发选择或 cleanup 规划。
3. 不支持的语义必须在 semantic、Typed XLIR、lowering 或 verifier 边界给出明确诊断；不存在替代后端或静默回退。
4. C 与 header 必须由同一份 Lowered XLIR export/global ABI 生成。
5. 同一 XLIR 的重复发射必须逐字节确定；生成结果必须通过严格 C、JIT/AOT 和相应宿主运行门。
6. 标准库和原生组件的需求来自已验证的 runtime/native dependency plan，不扫描源码或生成后的 C 文本猜测功能。

具体类的 fixed typed 方法绑定由 Lowered XLIR 的
`XL6_IR_CLOSURE_TARGET_BOUND_METHOD_ADAPTER` 指定唯一目标和一个接收者捕获。
重载选择、泛型闭合、默认实参、native 参数投影与虚方法槽位均在 IR 中决定。
C emitter 直接发射 bool/typed-out 的闭包入口函数体，不再套转发 thunk；
参数/结果不经过动态 Value，owned 字符串沿用普通接管路径。动态擦除调用
仍使用其原有 ABI，不能用这个 role 绕过来源及签名校验。

## IO 与 Console

`src/ir/xl6_ir_io_bridge.c` 封闭登记参数/结果类型、借入、错误 ABI、
native 工厂/析构身份；`xl6_ir_cgen_emit_io.c` 与
`xl6_ir_cgen_emit_console.c` 只输出已选中的薄边界适配器。
系统 IO、短操作、行扫描、终端解析与模式恢复在 XRT 实现，不搬入 emitter。
text write/writeln/print 返回 bool 状态，语言层转 IOError，不隐含每次 flush。
Screen 查询、Console 输入和 Session 分别选择不同 XRT 模块；
生成 C 仅引入声明头，不嵌入第二份 XRT/xruntime 实现。
`check_stdlib_io_console.ps1` 检查确定性、依赖隔离、严格 C、JIT/AOT 与错误 ABI。

## Process 配置与同步捕获

Command、Options、Result、Status 的真实参数/结果、所有权、错误和效果
由声明式 native/text-value 合同固定。typed argv 复用语义与 IR 共用的
`xl6_native_collection_contracts.inc`，不在 emitter 中恢复数组项类型。
`exec/success/exitCode` 是普通标准库源码组合；不再保留专用
`PROCESS_EXEC/SUCCESS/EXIT_CODE` operation 或 `xl6_cgen_process_run.*`。
运行、双流排空、取消和回收使用 XRT；四个 resident 适配组件按声明选择。
`check_stdlib_process_run.ps1/.sh` 验证冻结输入、严格 C、JIT/AOT、实际
原生装配、OOM 账本与错误类型；live/pipeline/Future/PTY 是后续范围。

## 权威设计与验收

- `../docs/XLIR统一中间表示设计.md`：统一 IR、lowering、verifier 和 C emitter 的完整设计合同。
- `../docs/XLIR迁移验收矩阵.md`：当前语义分区、所有权/错误路径和运行时专项门的验收账本。
- `../tools/check_xlir_contract.ps1`：XLIR 数据与发射边界的核心静态合同。
- `../tools/check_xlir_current_source_admission.ps1`：当前源码、退役 selector、provenance 和批量准入合同。
- `../tools/check_xlir_default_backend_boundary.ps1`：默认与显式 XLIR 的确定性、非法 selector 拒绝和 context 恢复合同。
- `../tools/check_cgenrule_docs.ps1`：本目录的退役文档与现役源码边界守门。

旧 AST Cgen 的编号规则表、验证矩阵和函数索引已随实现一起删除；需要追溯迁移前行为时使用 Git 历史。新的语言功能不得在本目录建立绕开 XLIR 的平行规则体系。

模块多返回值声明与普通声明使用同一作用域分类；闭包分析通过 XLIR
declaration-binding 身份读取字段，不按名称或初始化源码恢复类型。
全局字段保持全局存储；可写局部字段在统一 lowering 中构造共享 cell，
产品其他字段在构造失败期间继续受保护。`print/println` 的私有桥在
封闭 direct 描述表登记 borrowed 动态输入和 may-raise、非 pure、
may-block 合同；Cgen 使用通用 checked 外部调用，不为它另加错误补丁。
`check_xlir_module_product_globals.ps1`/`.sh` 验证这些边界与实际产物。

类中的 `static extern func` 在 frontend/XLIR 解析为没有隐式 self 的
直接 C 目标。封闭 direct 描述表登记 owned native 结果时，无论源
声明是 create、static extern 或普通 extern，都从声明返回类取得
canonical field/destructor TAKE ABI；不在 Cgen 识别函数名或补造
Buffer 包装。未登记的普通 extern class 返回值不擅自改为原生工厂。
`check_stdlib_buffer_codecs.ps1`/`.sh` 检查这种工厂、错 owner/borrow/
result/effect 拒绝、实际按需闭包和分配失败清理。旧 raw Hex/Base64
解码生成实现已删除。B42进一步删除旧 Buffer core 的名称匹配、
verifier profile及生成实现；公开工厂、字节操作和查询只消费封闭
direct合同与resident plan。`check_stdlib_buffer_core.ps1`/`.sh` 验证
owner/field/u8宽度、按需查询裁剪、错误与分配失败事务。旧file/network
转换帮助函数仍是独立待迁移范围，不得重新用于实现公共Buffer核心。
