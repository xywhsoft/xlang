# 动态方法的声明类型匹配

2026-10-14。动态方法的重载选择现在区分普通 `var`、基础值、名义
类型和精确容器身份，不再把所有 `xvalue*` 参数当作任意值。动态
`bytes` 参数和返回值也已接通，包括 nullable、默认 null、内嵌 NUL
和修改后原值不变。这是 Native 动态调用的类型安全前置工作；原生
字段投影尚未接通，`xlir_stdlib_dynamic_native.pending.xl` 仍待完成。

## 修复范围

旧匹配规则把类和容器都标为 `any-xvalue`；`bytes` 的独立物理 ABI
则未被动态适配器接纳。真实旧 Windows/Linux 产品在基础重载反例
上报出歧义。修复前日志和产物哈希保存在
`E:/xlang-validation/stdlib53-b269-dev/before.json`。

统一 IR 增加三项闭合匹配合同：基础值域使用已验证类型的 admission
规则，名义参数使用已封闭的身份集合，容器使用完整具体类型身份。
只有源 `var` 保留任意值语义。不支持的类型维持 unsupported，不
从指针载体推导类型许可。验证器拒绝把精确规则改成 ANY 或未知枚举；
匹配和实参投影共同消费同一合同，不读取 AST 或识别库函数名称。

C 输出调用已有 resident 纯判定，不生成另一套身份算法。`bytes`
沿用拥有引用和写入时产生新值的规则；nullable 的语言空指针与动态
null 值在调用边界转换。已有字符串、标量、默认值、可变参数、继承、
泛型及绑定接收者生命周期按原合同执行。

独立 AOT 曾暴露新增判定未进入 resident 依赖清单的问题，已修复。
输出与依赖选择共用可达动态行闭包。无参数且名称独立的动态方法
反例确认没有引入基础值或名义判定符号；同名方法属于开放动态调度
候选，不能把它们当成未使用声明直接裁掉。

## 双端验收

`check_xlir_dynamic_method_value_domains.py` 使用隔离源码的完整编译器，
Windows 构建 327 个、Linux 构建 326 个翻译单元。复用的旧 TCC 核心
只负责 C 编译，不作为新语言编译器或发布动态库的验收替代。

八份语言夹具在每端完成 93 项检查，包括确定性 C 输出、严格 C11
O0/O2、JIT、module 和独立 AOT。十个 IR 匹配规则变异在 C/header
输出前被拒绝；三个实际编译的错误实现分别把 bytes、名义或容器
判定放宽为 ANY，均被语言反例击中。

正向拥有语义程序的四个配置各覆盖 27 次分配预算尝试，其中 26 次
触发故障，直到首次未触发；模块退役后核对分配、释放和错误状态。
这个范围不包含线程调度、跨模块或任意对象图的完整故障空间。

最终证据为 `E:/xlang-validation/stdlib53-b269-domains-win-final4/results.json`
与 `/var/tmp/xlang-stdlib53-b269-domains-linux-final4/results.json`，两份均
Complete 且 InputsUnchanged。完整源码构建记录为 `stdlib53-b269-build-win4`
及 `xlang-stdlib53-b269-build-linux4`。失败的中间记录保留，不覆盖证据。

## 后续任务

接下来仍需统一 Native receiver 和各实参的 checked 原生字段投影、
外部错误作用域、准备失败的拥有退役及结果提交，不得只放宽 INTERNAL
限制。内建容器与字段的完整执行覆盖、工作线程和对象图 OOM、生成
模块组合及发布总门继续。长期目标保持进行中；WaitQueue MPMC、
CRT 0 和 XRT 19 组未决策、对象池最后的顺序不变。
