# 泛型枚举容器与 Stack 返回值所有权

本批恢复 `array<GenericEnum<T>?>` 的精确原生容器配方，并修复局部
`Stack<T>` 返回为 `var` 时错误选择 MOVE 的问题。没有新增标准库名称，
没有修改 XRT、IR 结构布局或 Cgen，也没有放宽验证器。

## 精确枚举配方

带负载枚举已有受管理 xvalue 表示和精确名义类型标识，但 Sequence 配方
只列入 class、protocol 和 generic instance，遗漏了 TAGGED_ENUM。
现在将其纳入现有 NOMINAL_V2 配方；构造仍必须取得模块中经过验证的
完整名义类型集合，不能用 ANY、类型名或底层整数表示替代。

`Choice<string>` 与 `Choice<int>` 的类型标识保持独立；可空槽允许 null，
不可空槽不能通过 resize 虚构默认枚举。已有 List／Dict／Set、嵌套容器
和 Stack 使用同一套精确类型信息与拥有约定。Set 保留已有基础值按内容、
对象与可变容器按引用身份的规则；本批没有定义新的枚举内容哈希，也没有
开放缺少活动成员／相等性合同的 C union Set 键。

运行时继续复用现有名义 admission 和 retain/release，不新增枚举容器
适配器、逐项动态装箱或 Cgen 类型追溯。

## 转换先决定所有权

旧 lowering 的布尔“允许 xvalue rebind”同时覆盖了两种不同操作：

- 原物理句柄不变，只改变静态类型，例如普通容器／类到 `var`。
- Stack 到 `var`，需要建立已有的动态 Stack 包装对象。

局部返回优化先根据这个宽泛布尔结果选择转移源存储，后续才创建包装。
严格 MOVE 验证发现结果不再是源存储的 LOAD/VALUE 链，因此正确拒绝编译。

现在由一个私有转换分类统一区分 NONE、REBIND 和 STACK_ERASE。只有
REBIND 能进入局部透明转移。STACK_ERASE 保留原局部的清理责任，按既有
Take 接口交付一个独立拥有的包装结果；finally 仍能观察、修改源 Stack，
已捕获的 cell 和 nullable Stack 同样不被提前清空。

普通赋值、动态项和返回转换共用这个分类。校验器仍拒绝把运行时调用
伪装成透明 MOVE，不跳过调用、不穿透包装对象追溯 LOAD。生成代码中的
Stack 包装仍有其既有分配和引用计数成本，本批不宣称该转换零开销。

## 验证

Windows／Linux 各 39 项全部通过，包括三个新增程序的 JIT、AOT、
GCC O0／O2，以及既有递归枚举、枚举闭包／异步和动态 Stack 回归。
每个原生 host 预热后运行三次完整模块生命周期，核对 LiveCount、
LiveBytes、分配／释放差额和 double-free／invalid-free／use-after-free。

枚举容器覆盖内嵌 NUL／UTF-8、泛型隔离、可空默认、错误写入和投影、
递归树、嵌套容器、受管理负载析构及六个容器族的 576 个分配失败前缀。
外部 C 回调只借入值，核对真实泛型 ID；在预存错误与零分配预算下，
匹配、拒绝与 null admission 均不分配、不改变错误、不保留指针。

严格 IR 合同观察 11 个精确配方（4 个可空）、5 个独立 Stack 擦除，
81 个错误变异全部在 C／头文件输出前拒绝，每端 162 次空输出检查。
另有两端各 60 项属性选择／事务／await 生命周期回归通过；Windows
完整字符串专项 364 项通过。

固定的 536 份旧源码保持内容哈希不变：504 → 505 成功，恢复
`hir_flow_value_ownership_matrix.xl`；原有 504 份成功产物 C 逐字节一致，
没有回退。剩余 31 个拒绝项包含真实核心缺口、负例和过时示例；514
接纳下限不变，完整门仍未通过。

## 证据与边界

在既有一致的 Windows 1295／Linux 1294 单元源码编译器上，只重编 IR
及 lowering 两个单元，相邻编译器源码包同步相同内容；TCC 核心仍为
固定的真实依赖。首次新增测试的非法 `export var` 已改为受支持的
extern 借入参数；测试暴露的 Stack 返回问题修复后，保留原 576 前缀
人口，未通过删除 Stack 覆盖使其通过。首次失败日志保留。

Windows 证据根目录为
`E:/xlang-validation/five-groups-20261027/p7-shipping/`：

| 目录／文件 | 内容 |
|---|---|
| enum-sequence-r1、enum-sequence-r3 | 两个生产单元的来源、对象及一致源码包 |
| enum-sources-r2 | 首次测试设计错误和真实 Stack 返回拒绝 |
| enum-check-r6/results.json | 39 项、生命周期、OOM 和严格 IR 合同 |
| enum-properties-r7/results.json | 60 项属性回归 |
| enum-corpus-r4/results.json | 固定 536 份源码对照 |
| enum-strings-r5/results.json | 完整字符串专项控制器 |
| enum-proof-r8.json | 独立 reader 的哈希和结果核对 |

Linux 对应根为 `/home/ubuntu/xlang-validation/20261027-p7-shipping/`，
同名 r3／r6／r7／r8 保留原件。持久检查工具为
`tools/check_xlir_enum_sequences.py`，要求显式提供完整 source profile、
其 root、case 目录和新的工作目录，不能用旧 DLL 或 runtime-only 代替。

本批不是新发布 DLL／VFS，也不是全部 74 项参数化 guard 或五组联合
验收通过。没有攻击、扫描、公网探测或密码学安全性质专项；普通产品
整合与剩余核心／测试阻塞继续，长期任务保持 active。
