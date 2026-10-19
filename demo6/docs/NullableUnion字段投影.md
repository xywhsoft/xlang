# Nullable union 的非空字段投影

修复 `if value != null` 后直接读取或修改 union 字段的共用 XLIR 缺口。
旧源码编译器报“dynamic member expression has no resolved object result ABI”，
因为字段绑定只查询 declared storage TypeId，忽略 NAME 已有的非空表达式 TypeId。

`xl6IrUnionReceiverType` 统一验证两者关系：直接 union 保留原合同；可空存储
只有在 NAME 的表达式类型精确等于 canonical nullable union 基础类型时才许可。
不接受同大小的其他名义类型、任意 nullable 接收者或临时 CALL 冒充 flow NAME。
字段仍绑定完整 layout／field identity；类型阶段与 lowering 使用同一合同。

lowering 保留原有 NAME load 与显式 NULLABLE_UNWRAP；UNION_GET／UNION_SET
只接收精确的 union SSA 值。写回先显式 NULLABLE_WRAP，再通过普通 storage
replacement 写入原声明类型；捕获变量继续使用已有 cell store。没有改变赋值／
传参的值语义，没有新增别名、字段猜测、Cgen 特判或运行时算法。

验收入口为 `tools/check_xlir_nullable_union_member.py`，真实完整源码编译器，
JIT／module／独立 AOT、生成 C 的严格 O0/O2、七组旧 union／容器／Channel
回归及独立 C 合同。每种优化的固定合同为 14 个 narrowed member、16 次读、
7 次写、8 次 wrap、102 个无效 IR 变体，以及 204 次 C/header 零输出拒绝。
变体包含不匹配的表达式类型／接收者／nullable 基础类型及同字段类型的其他
名义 union owner；不能以相同宽度绕过精确类型证明。

证据位于 `F:/xlang-validation/five-groups-20261020/p0-nullable-union-member/`。
Windows／Linux 完整源码分别为 624／623 编译单元，两端最终语言报告各
26 阶段，共 52 阶段通过；逐项复核输入、对象、日志和产物哈希。
本批只是 P0/P1 共用值层修复；不是 Case／Future 编码、整个 P1 或新 shipping
产品验收。不运行网络或安全性质专项，不修改 XRT 和无关工作区内容。
