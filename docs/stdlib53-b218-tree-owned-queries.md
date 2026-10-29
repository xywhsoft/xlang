# OrderedMap 拥有结果查询

更新日期：2026-10-12

OrderedMap 的查询现在直接返回拥有值或多返回值，不再写回调用方的输出
变量。十二个语言入口共用一份声明表和精确 XLIR 结果合同，Cgen 只将
结果字段接到 `xl6TreeQueryV2` 的私有空槽，不实现树查询或重新推导语义。
底层仍使用 XRT typed Tree 的排序、复制、移动与对象图生命周期。

## 公开接口

对 `OrderedMap<K,V>`，严格查询在没有结果时抛出 RangeError；对应的
try 查询正常返回 found=false 和 null。复制、分配、准入或资源错误
仍走 checked 失败路径，不得伪装成缺失。

| 方法 | 参数 | 返回值 |
| --- | --- | --- |
| get | K key | V |
| take | K key | V |
| first | 无 | K, V |
| last | 无 | K, V |
| lowerBound | K key | K, V |
| upperBound | K key | K, V |
| tryGet | K key | bool found, V? |
| tryTake | K key | bool found, V? |
| tryFirst | 无 | bool found, K?, V? |
| tryLast | 无 | bool found, K?, V? |
| tryLowerBound | K key | bool found, K?, V? |
| tryUpperBound | K key | bool found, K?, V? |

lowerBound 返回第一个不小于搜索键的条目，upperBound 返回第一个大于
搜索键的条目。take／tryTake 移出并删除条目，其他方法取得按值域合同
独立拥有的副本。string 与 bytes 保持基础值的复制语义；可变对象取得
独立强引用，不深克隆对象。V 已 nullable 时不增加第二层 nullable，
独立的 found 可区分“存在的 null”和“缺失”。

```xlang
auto values = OrderedMap::<int, string?>()
values.set(1, null)
auto found, item = values.tryGet(1)
assert(found and item == null)
auto missing, absent = values.tryGet(2)
assert(not missing and absent == null)
auto key, value = values.first()
assert(key == 1 and value == null)
```

六个 getInto／takeInto／firstInto／lastInto／lowerBoundInto／upperBoundInto
语言方法已经清退，旧 source/runtime IR 操作和 Cgen 写回分支一并删除。
相关夹具改用赋值、多返回值及 try 查询。原生 V1 C ABI 暂保留给原生宿主
与底座回归，不构成语言兼容层；生成查询不引用旧 GetV1／EndpointV1。

## 声明与编译合同

`xl6_tree_query_ops.inc` 声明名字、搜索键、成对结果、required 和原生
操作身份。Sem 由同一表构造结果类型；XLIR 的 TreeQueryPlan 验证接收者、
键和值 schema、nullable 表示、拥有权、键顺序、参数数量和错误物理 ABI。
lowering、独立 verifier 与 Cgen 共用该合同。

所有结果先处于无拥有内容的空态，不先构造默认 T 或空字符串。Cgen 将
精确字段地址及 sizeof 交给原生查询，操作成功状态和 found 保持独立。
失败由显式状态进入错误出口和作用域清理，包括没有错误诊断对象的失败。
没有新增查询实现、按函数名选择的转接实现或成对临时堆缓冲。

泛型声明只接纳精确逻辑签名，不猜测开放 K／V 的 codec。实例化时重绑定
键类型、键顺序和载荷 ABI，再验证封闭的物理结果合同；未实例化的合同
不能进入可执行 C。仅通过 get 使用 class 元素的 Tree 也会根据 IR 声明
的准入依赖选择类身份元数据，不依赖 values 等其他调用偶然带入。

## 验收范围

验收入口是 `check_stdlib_tree_owned_queries.py --root <frozen-demo6>
--public-directory <fresh-source-build> --out <fresh-output>`。Linux 的
`--sanitize` 对所选独立程序的宿主、生成代码及原生闭包使用 ASan／UBSan，
不等于完整编译器或发布运行时全量插桩。

专项覆盖十二个入口、七种严格缺失、空 try 结果、存在的 null、精确 NUL
和 UTF-8 文本、引用共享与移动、泛型键和值、线程与同步资源、嵌套容器、
多返回值、异步、函数值、生成器和 finally。实际 parsed／typed／lowered
IR 的结果、参数、schema 和身份合同变异必须被 verifier 与 Cgen 拒绝，
且已有输出不能被破坏。九个坏源码合同包括全部六个退役 Into 方法。

完整宿主与实际 artifact plan 裁剪程序均检查 O0／O2 的 1000 次生命周期，
并逐点推进 query 无诊断失败和 OOM，直到完整首个未命中点。每个失败点
检查 checked 状态、同一模块成功重试、释放平衡及无非法释放。真实 JIT、
module 和 AOT 产物都参与运行，独立程序检查未选中队列、SlotMap、Regex
提供方符号。

原生复制事务和四种实际坏实现继续由
[原生查询专项](stdlib53-b217-tree-query-native.md)及更新后的
`check_stdlib_tree_query.py` 复测；组件目录仍逐个编译验证精确导出与闭包。

双端分别使用本批 302／301 翻译单元构建的源码编译器。每端公开查询专项
728 阶段：18 个 JIT 夹具、15 份实际 IR 合同、82 个查询和 2197 项损坏
合同拒绝；9 个坏源码合同拒绝。8 组完整失败前缀共 604 次运行，query 的
首个未命中点为 22、OOM 为 127；完整和裁剪 O0／O2 共 4000 次正常生命
周期均平衡。真实 module／AOT 和 27 组件的独立程序通过。Linux 两个
优化级的所选独立闭包及其故障前缀同时通过 ASan／UBSan。

原生查询专项每端 189 阶段、20 组失败前缀、92 次运行、四个实际坏实现
拒绝及 747 项相邻 IR 合同变异通过。逐个编译的组件目录保持 263 组件、
1399 精确导出，69169 个两根依赖组合和 13 个坏目录控制通过。Windows
原生专项在并行重型编译中一次因编译器内存不足中断，串行重跑以完成
报告为准；中断未计成通过。文档守卫保留相同 14 条历史诊断，不算全仓
守卫通过。

证据在 Windows `E:/xlang-validation/stdlib53-b218-*` 和 Linux
`/var/tmp/xlang-stdlib53-b218-*`，提交绑定测试输入与选择索引，不混入
无关工作区改动。供应方 XRT 和消费方生成单头不变。

本批不扩展 Tree 的 Future／拥有型原生记录值域，不代表 WaitQueue／Channel、
SlotMap 成对迭代、所有 frame／cell 边界、队列压力或发布 DLL／SO／VFS
总门完成。三组长期任务继续，19 个未决策组及对象池最后的排期不变。
