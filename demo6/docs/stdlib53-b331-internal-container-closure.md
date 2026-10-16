# 内部容器依赖与原生裁剪验收

覆盖表中六项纯内部模块的定位和裁剪已有双平台实际产物证据。语言 Stack
并不使用旧 XRT stack 系列；FixedStack 是对同一 typed Stack 的逻辑容量限制，
不是另一套固定栈实现。本批只增加测试和同步文档，不修改生产实现或公开 API。

## 实际依赖

| 模块 | 当前用途与验收结论 |
| --- | --- |
| ptr_array | XRT value_container 的内部数组支撑，随值容器进入语言产物；不重复公开 PtrArray |
| int_map | xruntime typed List 的稀疏整数索引存储，也是 value_container 的依赖；不重复公开 IntMap |
| avl_tree | typed OrderedMap 和 int_map 的原生有序树存储，复用既有节点、分配池和比较合同 |
| avl | avl_tree 的节点与平衡算法，不在 Cgen 复制旋转或遍历实现 |
| ptr_fixed_stack | 不是语言 FixedStack 的实现依赖；所测语言原生闭包排除它，保留显式 C 互操能力 |
| block_stack | 不是语言 Stack 的实现依赖；所测闭包排除它，不重复公开实现类 |
| ptr_stack | 不是语言 Stack 的实现依赖；所测闭包排除它，保留显式 C 互操能力 |

xruntime typed Stack 的直接存储依赖是 typed Array。FixedStack 的声明包含一个
私有 `Stack<T>` 并检查逻辑容量；因此旧 XRT `stack`、`fixed_stack` 也不应仅因
这两个语言类型而保留。上述关系同时由仓内模块声明、真实编译器保存的原生
实现输入、预处理后的 feature 集合及严格编译对象符号核对。

即使字面量基线，现有语言入口和值容器闭包仍包含 ptr_array、int_map、avl_tree
和 avl。不能把它们描述成仅在 List／OrderedMap 中出现，也不能据此宣称函数级
最小化；这里验收的是既定模块依赖和未使用的旧栈系列不被保留。

## 双平台实际产品测试

`check_stdlib_internal_container_closure.py` 调用实际 compiler 的公开 API，
在没有 src、lib、res 或 include 兜底的目录中，分别完成六个程序的 JIT、模块、
AOT 构建和迁移后执行。程序覆盖字面量基线、Array 创建、稀疏 List 方法、
OrderedMap 拥有查询、Stack 拥有型批量操作和 FixedStack 满／空行为。

每个 AOT 构建通过正式 saveTemps 功能留下真实 `module.c` 和
`xl6_xrt_impl.c`。测试不是另写原生 provider 来替代这两个输入；编译器仍只使用
同次构建的内嵌 VFS。独立程序不得导入 xl／xlc，Linux 不得带 RPATH／RUNPATH。

每端另以 GCC O0／O2 严格编译真实生成 C 和保存的原生实现，检查预处理 feature
及对象符号：需要的内部模块有具体函数定义，五个旧 raw stack family 均不出现。
对象符号检查不等于已剥离 TCC AOT 文件的完整符号检查。这里重编原生对象只用于
独立检查，JIT／模块／AOT 的行为证明来自实际共享产品，不来自这些检查对象。

三个已编译对照分别强制加入 ptr_stack、block_stack、ptr_fixed_stack。其原生
对象必须真实包含相应符号，并被同一“未使用模块不得保留”的判定拒绝；不把
编译失败或源码字符串出现当作有效反例。

Windows／Linux 每端 82 阶段、六条完整产品路径、12 个原生 O0／O2 对象和
12 个生成 C 对象、三个保留代码拒绝对照全部通过，输入指纹保持不变。
没有新增分配故障或资源寿命覆盖声明；这些仍由既有专项和产品总门承担。

## 证据与剩余验收

- Windows：`D:/xlang-validation/stdlib53-b331-internal-win/results.json`。
- Linux：`/var/tmp/xlang-stdlib53-b331-internal-linux/results.json`。
- 使用的普通产品来自 B330：
  `D:/xlang-validation/stdlib53-b330-build-win/build.shipping.json`、
  `/var/tmp/xlang-stdlib53-b330-build-linux/build.shipping.json`。
- 既有整批产品与独立 AOT 验收见
  [任务线程阻塞终结与真实产品验收](stdlib53-b330-task-worker-quiescence.md)。

本批关闭 ptr_array、avl、int_map、ptr_fixed_stack、block_stack、ptr_stack 的
纯内部定位／所测裁剪待验收项，并补充 avl_tree 的内部依赖证据。它不代替
全部公开方法、泛型元素域、边界分支和最终 53 模块完成审计。CRT 未决策 0、
XRT 未决策 19 组及对象池最后的顺序保持不变。
