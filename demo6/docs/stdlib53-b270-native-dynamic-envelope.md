# Native 动态方法的物理调用合同

2026-10-14。Native 类的动态方法现在可以使用已封闭的直接结果调用
合同，按顺序检查接收者及其他原生类参数，全部投影成功后才进入 C。
Buffer、Regex、Pattern 的所选场景已通过双端执行验证。这不是所有
Native 编码或内建资源动态分派的完成证明；综合 pending 用例仍在
`Stack.count()` 被拒绝，完整长期目标继续。

## 调用合同

统一 IR 的动态注册、Lowering、验证器和 C 输出共用同一物理调用
接纳规则。Native 源声明的返回值列表不再被当作泛型参数列表，
因此没有函数体的 `len()` 等声明能够获得正确的适配身份。

每个原生参数使用 CallTarget 登记的精确 native field。生成代码在
进入 C 前，依次执行 checked 字段读取；不是把 getter 嵌套在 C 的
实参表达式里，也不会把投影失败当成可以调用的空句柄。名义参数和
bytes 的重载选择复用上一批纯类型匹配，不按相同指针形状选择函数。

动态调用记录独立成功状态。准备失败不转移输入拥有权，真正进入 C
后才清空已转移的 OWNED 见证。原生失败先保存首个错误，再释放未
提交的拥有结果；原生字符串的借用结果转换在参数退役前完成。
无诊断失败保持为失败，不能为了补充 TypeError 触发新的诊断分配。
通常的静态直接调用沿用原 ABI，未增加库专用生成分支或转接函数体。

闭合整数默认值增加一元正负号和 signed minimum 的规范化。`-1`
这样的默认长度参数在统一 IR 中变为可移植 C 字面量。动态方法和
具名 callable 共用有范围检查的整数规则；验证器同时核对默认值
内容与声明相符，不能用另一个格式正确的数字替换它。

## 验收结果

Windows 完整源码构建 327 个翻译单元，Linux 构建 326 个。每端
104 项检查覆盖九份执行夹具和一份故障夹具：确定性 C 输出、严格
C11 O0/O2、JIT、module 和独立 AOT。故障宿主使用真实生成的导出
函数与实际原生提供方，检查九种模式及模块最终退役的内存平衡。

测试覆盖内嵌 NUL、UTF-8、文本／bytes／Buffer 重载、超长 size 的
截断、多原生对象参数、别名自追加、Regex 匹配与替换、Pattern 标签、
相邻 Array/Dict 动态操作，以及方法／绑定方法／xcall 的负默认值。
原有重载、参数、继承、泛型和绑定接收者生命周期继续通过。

21 个物理字段、物理错误 ABI 和整数默认值变异在 C/header 输出前
被拒绝。实际编译的错误实现删除无诊断失败的防护，零分配预算测试
立即击中。无诊断信号直接使用显式 `(false, NULL)` 的作用域合同，
不把会创建诊断的 callback 便利入口冒充无分配失败。

Native 正向程序的四个配置各执行 130 次分配预算尝试，其中 129 次
触发故障，直到首次未触发；每次完成模块退役并核对完整内存账本。
这不覆盖工作线程、跨模块或任意对象图的全部故障空间。

最终证据为 `E:/xlang-validation/stdlib53-b270-native-win-final3/results.json`
和 `/var/tmp/xlang-stdlib53-b270-native-linux-final3/results.json`，两份均
Complete 且 InputsUnchanged。源码构建为 `stdlib53-b270-build-win4` 和
`xlang-stdlib53-b270-build-linux4`。复用的旧 TCC 核心只负责 C 编译，
发布 CLI、动态库和嵌入 VFS 尚未据此重新认证。

## 剩余工作

`xlir_stdlib_dynamic_native.pending.xl` 保留不变。独立 Native、Array
和 Dict 场景已有成功证据；综合用例仍在 Stack 的内建资源动态分派
处失败，不能把它改成通过清单。后续须复用 resident 的具体角色、
类型描述和拥有值投影，不能把所有 HANDLE 当成 Array 或裸指针。

Native factory 结果、native array 结果及未接纳的 callback、原生
标量等编码仍须逐项补齐；无法完整表示的行保持 unsupported。内建
容器与字段的完整覆盖、工作线程和对象图 OOM、生成模块组合及发布
总门继续。WaitQueue MPMC、19 个未决策 XRT 分组和对象池最后不变。
