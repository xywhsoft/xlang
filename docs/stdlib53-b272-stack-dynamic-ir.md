# Stack 核心动态调用接入统一 IR

2026-10-14。Stack 到动态值的转换现在是显式、可验证的 IR 操作，核心
动态方法复用驻留类型描述、接纳规则和 XRT typed storage。原
`xlir_stdlib_dynamic_native.pending.xl` 不改内容，已通过双端 C、JIT、
module 和独立 AOT；文件名保留原证据路径，不再代表该场景仍失败。
完整 Stack 动态公开面与 53 模块总门仍在进行。

## 转换与分派合同

`stack-erase` 记录具体 Stack TypeId 和元素 TypeId，消费一个拥有源，
成功返回一个动态拥有值，失败也消费源。借用源先由独立的 retain 操作
取得拥有权，不能因相同指针表示而直接转移调用者对象。生成 C 调用
`xl6StackEraseTakeV1`，随后清空已消费的 SSA 见证，并立即观察失败边。
类型擦除保持容器身份，不复制元素。

验证器拒绝错误角色、丢失的身份／元素、错误结果类型、错误实参数量，
以及用普通 VALUE 复制绕开 checked 擦除。可空 Stack 的 absent 输入
仍是语言 null 数据，不是原生调用失败的 NULL；实际存在的 Stack 则
使用同一个具体布局。临时值、参数、返回值和可空来源都在所选夹具中。

动态分派认证具体 Stack 角色后，再执行 push、pop、peek、peekAt、
clear、reserve、trim、count、capacity 和 clone。Array 入口没有放宽，
Stack 的 Array-only 操作仍报 TypeError。实际原生算法不进入 Cgen，
生成器只输出已验证的物理调用。动态集合的 null 字面量输入复用既有
规范化判定；不以“C ABI 不支持”允许任意伪造输入。

擦除提供方和元素转换器由显式 IR 操作按需选择。纯 typed Stack 的
实际 AOT 符号不含 Stack 擦除、擦除读取或动态集合分派入口；保持构造
阶段不绑定动态投影。内建资源动态分派使用具体角色，不把 HANDLE
统一解释为 Array 或裸指针。

## 验收结果

Windows 从 328 个源码翻译单元构建，Linux 从 327 个构建，无对象覆盖。
每端 72 项验收包括六份语言夹具、严格 C11 O0/O2、确定性 C 输出、
JIT、module、独立 AOT 和实际符号检查。54 个 IR 变异在 C/header 输出
之前被拒绝。六份夹具覆盖原 pending、核心方法、错误边界、纯 typed
裁剪、现有 Stack 值语义和相邻 Native Buffer／Regex 接入。

拥有场景包含精确 NUL／UTF-8、char 的动态保存与比较、bytes 写时值
隔离、可空元素、类对象身份、克隆独立存储及可空 Stack 擦除。O0/O2
双端四条扫描各执行 85 次分配预算尝试，前 84 次触发故障，直到首次
未触发；每次完成模块退役并检查完整内存账本。原生投影专项继续通过。

语言证据为 `E:/xlang-validation/stdlib53-b272-language-win-final1/results.json`
与 `/var/tmp/xlang-stdlib53-b272-language-linux-final1/results.json`。源码构建为
`stdlib53-b272-build-win4` 和 `xlang-stdlib53-b272-build-linux4`。各报告须保持
Complete 和 InputsUnchanged；复用旧 TCC 核心不等于发布动态库重新认证。

## 仍需补齐

Stack 动态 try、批量和 equals 尚未接通；其拥有产品、结果容器身份和
同类型参数合同必须在 IR 中完整表示后再选择物理入口，不能从运行时
值形状猜测。其他内建容器的完整动态公开面也须继续核对。

本轮另外确认 `char(var)` 和 `var as char` 都被现有转换合同拒绝，
分别为 checked integer intrinsic 和 closed Cast ABI 缺口。char 动态
值的保存与比较已经通过，但这两种转换不能据此标为已开发；应优先
补齐统一 IR 的 checked 字符转换。源码失败记录保留在专项开发目录。

其余 Native factory／callback 编码、工作线程和对象图 OOM、生成模块
组合、平台专项与发布总门继续。19 个未决策分组、WaitQueue MPMC 和
对象池最后不变，长期目标保持进行中。
