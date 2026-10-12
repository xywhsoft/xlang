# 原生 Dict 命名访问与拥有值

本批修复原生 Dict 的点访问，让 `d.key` 与 `d["key"]` 共用精确键长度、
拥有结果及 checked 失败规则。没有新增公开标准库函数，也没有修改 XRT
供应方实现。三组标准库长期任务仍进行中，联合类型容器与发布总验收不在本批完成范围内。

## 访问与生命周期

命名读取借用接收者和键的字节片段，返回一个独立拥有值。缺失键或空接收者
返回拥有的 Value null；真正失败返回 C NULL，因此失败不会被当成缺失。
读取的字符串、bytes 或子对象在源 Dict 清空、替换、释放后仍有效。

命名写入借用接收者，成功或失败都消费输入值。原生 Dict 保持既有的不可变
类型准入合同；类对象继续使用已认证的声明字段与开放字段路由。首次失败，
包括没有诊断对象的失败，不会被后续输入值析构覆盖。

`xl6_value_index_read.c` 和 `xl6_value_index_write.c` 的字符串键入口复用
两个命名访问组件。数字索引继续使用 Array／List 路径，空键、UTF-8 与
内嵌 NUL 仍按完整字节片段处理，不使用 `strlen`。

## XLIR 与生成代码

XLIR 新增封闭的命名读取和消费写入操作，明确接收者、输入值、规范键字面量、
结果拥有权及错误 ABI。无属性或方法候选的动态成员直接降为这些操作。
有候选时，类属性和方法分派保持优先；没有匹配的原生 Dict 接收者使用
同一个命名访问实现。只读属性和不支持的适配器不能被原始字段写入绕过。

命名读取的核心生成代码是一次指针与精确长度调用，例如：

```c
value = xl6ValueNameReadCopyV1(receiver, "answer", sizeof("answer") - 1, true);
```

没有临时键装箱、TLS 结果缓存或伪借用。必要的错误边界和拥有值清理仍由
已验证的 XLIR 控制流提供。已知类字段保留直接访问路径。

两个新组件分别按需选择。命名键专用程序不带入通用索引、Array／List／Set；
只访问已知类字段或只做数值计算的程序不选择新组件与 Dict。
类动态字段路径的既有私有导出也补齐了精确目录登记。

## 验收入口

- `tools/check_stdlib_container_contract_batch.py`：九个真实语言用例，严格 C O0／O2、
  JIT、独立 AOT、完整同步分配失败前缀与 import 检查；双端各 156 个阶段。
- `test/cases/xlir_dict_named_route_contract.c`：388 项损坏 IR 变异拒绝，C 与头文件
  均不输出；拥有结果、消费输入和类／数值／命名键裁剪均有独立检查。
- `tools/check_stdlib_dict_index.py`：精确字节键、拥有值、类型拒绝、无诊断失败、
  输入析构及五个真实坏实现；直接索引和命名访问均覆盖完整故障前缀。
- `tools/check_stdlib_array_scalar_read.py --boundaries` 与
  `tools/check_stdlib_array_primitive_admission.py --reads`：相邻读写及准入闭包回归，
  Linux 使用真实闭包的 ASan／UBSan。
- `tools/check_xlir_resident_catalog.py`：267 个组件、1413 个精确导出、13 个
  错误登记检测和独立依赖闭包检查。
- `tools/check_xlir_runtime_operand_catalog.py --catalog-only`：新增命名操作进入
  独立的编译后所有权目录检查。此项不等于完整线程验收。
- `tools/check_stdlib_persistence_batch.py`：四格式全部已登记持久化用例，
  双端各 505 个阶段、40 份计划与 27 个独立程序；完整失败前缀和恢复图合同保留。

源码驱动由 Windows 306 个、Linux 305 个真实翻译单元构建。
旧 shipping compiler 不参与语言验收；共享库候选仅提供明确记录的 TCC 核心。
Windows 已提交的 DLL 源文件清单和工作区的新发布构建模板均补齐两个组件；
既有未提交的发布脚本重构没有并入本批。发布 DLL／SDK 本体尚不宣称验收完成。

## 剩余范围

普通 `union` 的 Array 元素缺少完整原生存储配方，相关两个真实用例仍待修复。
容器成对迭代、WaitQueue 拓扑与后端、压力测试以及 shipping／53 模块总门继续。
Thread 与 Stack 的 O2 分配失败测试在双端出现间歇性的回收观察失败；
该用例生成的 C 与上一批逐字节相同，单独复测也能通过。线程结束与
资源回收的同步观察边界仍需压实，失败报告保留，完整线程门不计为通过。
CRT 未决策 0，XRT 19 个未决策组不变；对象池留在最后。
