# OrderedMap 原生查询与独立结果

更新日期：2026-10-12

OrderedMap 的原生查询底座已提供 get、take、first、last、lower、upper 六种
操作，共用精确结果槽和 checked 失败合同。这一步为语言多返回值消除
调用方变量写回与默认值构造；语言目前仍使用原有 Into 方法，尚未切换。
XRT 树算法、对象图策略和既有公开类型不变，Cgen 没有增加查询实现。

## 结果与缺失

`xl6TreeQueryV2` 的返回值只表示操作成功；`found` 单独表示是否有结果。
因此存在的 nullable null 与缺失不同，复制、分配、状态或准入失败也
不能伪装为缺失。required 查询在缺失时产生 `XERR_RANGE`，可选查询在
缺失时成功且 found 为 false。

GET／TAKE 只产生值；FIRST／LAST 不接受搜索键；LOWER／UPPER 同时
接受搜索键并产生键和值。TAKE 复用 XRT owning move 并删除对应条目，
其他查询获得按值域合同独立拥有的副本。引用元素取得新的强引用，
不意味着对象深克隆。

结果槽是调用尚未发布的私有存储，不是调用方已有变量。槽必须具有
精确物理大小和对齐，不能相互重叠，也不能与搜索键、布局或 found
重叠；托管槽必须为空。输入被拒绝时不写回，已经开始获取的结果在
失败后统一回到 codec 的空结果态。

空结果态不是默认 T。字符串槽初始 Data 为 NULL、Size 为 0，不先
分配空字符串；只有命中后才复制实际内容。平凡值查询零分配，文本
成对查询只分配两个实际副本，没有成对临时堆存储。NUL 与 UTF-8
始终按精确字节长度传递。

## 所有权与错误边界

查询先认证实际私有 Tree ops、完整驻留布局及准入策略，再访问存储。
复用原生操作 pin、busy 和拥有图清理协议；不保留生成模块的回调或
元数据，不增加树遍历或平衡算法。

私有键槽和值槽本身承担复制暂存。第二份复制失败时清理第一份，
包括“先产生拥有内容、随后返回失败”的情况。释放期间的后续错误
不能覆盖首次有诊断或无诊断失败。失败清理后可用同一树成功重试。

原生 symbol 归属既有 TREE 组件，不新建组件或选择所有模块。旧 V1
输出接口仍服务当前语言；新 V2 并存是迁移阶段，不是语言兼容承诺。

## 双端验收

Windows／Linux 本批源码编译器分别构建 302／301 个翻译单元。
新的查询体分别以严格 C O0／O2 重新编译，未使用旧发布编译器替代。

| 专项 | 每端结果 |
| --- | --- |
| 六种查询 | 空树、required／可选、查找、取出、端点、上下界及零分配标量通过 |
| 结果拥有权 | 精确文本、nullable 值、强引用、源树销毁后结果可用、已占用槽拒绝通过 |
| 完整分配失败前缀 | 新查询及旧 create／clone／endpoint／projection 共 20 组、92 次运行，成功重试与内存平衡通过 |
| 逻辑复制失败 | 每优化级 8 种：首份／次份、有／无诊断、部分拥有内容；后续 drop 错误不覆盖首错 |
| 四个坏实现 | 漏 drop、误报命中、复制替代取出、覆盖已有拥有槽；均实际编译执行并被拒绝 |
| 相邻语言回归 | 5 份既有接口 JIT 通过；18 个工厂、114 个操作、628 项损坏 IR 合同拒绝通过 |
| 按需独立程序 | 真实 artifact plan 的 24 组件闭包，O0／O2 执行新原生查询，不含队列、SlotMap、Regex 提供方符号 |
| 生命周期 | 完整宿主和按需独立程序各优化级 1000 次，共每端 4000 次，内存平衡通过 |
| 组件目录 | 263 组件、1399 个精确导出，69169 个两根依赖组合及 13 个错误目录控制通过 |

每端查询专项 189 阶段。Linux 两份按需独立程序的宿主、查询体与
选中原生闭包还以 ASan／UBSan 检查；完整源码编译器、发布运行时
不是全量插桩产品。

证据位于 Windows `E:/xlang-validation/stdlib53-b217-*` 和 Linux
`/var/tmp/xlang-stdlib53-b217-*`。验收入口：
`check_stdlib_tree_query.py --root <frozen-demo6> --public-directory
<fresh-source-build> --out <fresh-output>`；Linux 可加 `--sanitize`。
提交前复核编译输入、头文件、产品和验收输入与选择索引一致。
文档守卫的 14 条历史诊断不增加，本批不算全仓守卫通过。

## 后续工作

语言 getInto／takeInto 与边界输出方法仍待改为多返回值，须先在
统一 IR 声明精确输出类型、拥有权、缺失与失败合同，再由 Cgen 机械
投影至此底座。Future 和拥有型原生记录的 Tree 值域不在本批范围。
WaitQueue／Channel、SlotMap 成对迭代、其他原生值消费边界、队列压力、
发布 VFS／DLL／SO 及三组总门继续。

`NativeFreshTreeQuery=true`、`PrunedSourceNativeQuery=true`；
`LanguageOwnedTreeQueries=false`、`PrunedShippingArtifact=false`、
`Full53Acceptance=false`。Set 的可变对象／容器按引用身份、基础值按
内容规则不变；CRT 0、XRT 19 组未决策和对象池最后不变，见
[标准库决策清单](标准库决策清单.md)。
