# 驻留源码的 TCC 结构体数组初始化兼容性

嵌套 AOT 需要重新编译驻留 C 源码。一处现有局部 `xstrview` 数组用结构体
形参作初始化元素，Windows TCC 报 `init_assert: initializer overflow`，
Linux 系统 TCC 报该行 `index too large`。两端均在编译阶段稳定复现，无需
执行 ACME、访问文件服务或发起网络请求。

修复只是将该初始化改为五次显式结构体赋值。所有参数顺序、长度校验、
存储操作和 checked 错误路径保持原样，不修改 Cgen，不引入转接层，也不
改变 ACME 的功能、接口或拥有语义。

`check_xacme_c_initialization.py` 保留旧源码作为失败对照，实际新源码在
两端 TCC 均可编译。GCC O0/O2/Os 分别编译旧源码、新源码和单独标明的
诊断行号对照：新增注释会改变后续 `__LINE__`，因此不冒称原始新、旧对象
完全相同。对照仅对未修改后缀使用显式 `#line` 对齐诊断行号，其对象逐字节
相等；实际新源码对象也完整保留，没有过滤或修改对象、反汇编中的差异。

随后以本轮普通构建的实际 TCC 和对应 SDK，在两端各批量编译全部 1,287
个正式编译单元。其余源码来自不可变构建副本，仅显式覆盖这一份候选源码，
2,574 次编译全部通过。该批为 compile-only 兼容性检查，没有链接替代
provider 或执行任何功能，不能据此宣称嵌套 AOT 或新普通产品已经验收。

证据根：`F:/xlang-validation/five-groups-20261027/p7-shipping/`：
`tcc-local-aggregate-win-r5`、Linux `tcc-local-aggregate-linux-r5`，以及
两端 `resident-tcc-*-r2`。首轮系统 TCC／默认 SDK 的 BOM、原子头和 Windows
常量差异另行保留，不误判为当前嵌入式编译器的生产缺陷。

`build-*-r2` 是修复前已经冻结并开始构建的真实基线，不向其补丁式替换源码
或资源。包含该修复的普通产品仍需从新快照构建，再执行原嵌套门；五组目标
保持开放。
