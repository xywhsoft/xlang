# graph

2026-10-26 当前发布版复验：
`xl run dev/extlib_blockers/graph_var_array_projection.xl` 仍在合法
`var` 边数组进入原生 `isDag` 时报告
`ValueError: graph edges must be an array`。中央用例同形失败，
因此先修共享 foreign `var` 数组投影，再复跑完整包门禁；
旧 AOT 缺索引符号不是当前第一阻断。包本体不解释编译器私有句柄。

2026-10-24 当前 Windows 发布版复验：专项门禁的严格 C、分配故障与
四线程测试通过；首个 XLang 用例在合法图数组进入 C 桥时报告
`graph edges must be an array`。一条边的 JIT/独立 EXE 最小复现见
`../../../dev/extlib_blockers/graph_var_array_projection.xl`，属已记录的
foreign `var` 容器投影边界，不是图算法结果错误。本轮后续语言、DLL
宿主复用、生成原型及 AOT 门禁未跑到，不能以历史通过记录签收。

有向图的遍历、拓扑排序、连通分量和距离计算。使用 XRT map/array 存储实际节点和边，
不预留最大容量的全局数组；九项语言函数与九个 C 入口共享同一套算法。

## 输入

输入是数组，每行是以下一种形状：

| 行 | 含义 |
| --- | --- |
| `[name]` | 声明节点，允许没有边的孤立点 |
| `[from, to]` | 声明有向边，默认权重 1 |
| `[from, to, weight]` | 声明带权有向边 |

节点名必须为字符串，大小写敏感，空串和 UTF-8 名称合法。权重接受有符号/无符号整数或有限 float，
内部转换成 float64；bool、字符串、null、NaN、Infinity 不作为权重。
超出 float64 整数精确范围的大整数会损失精度，不能用于精确整数成本计算。

重复节点声明不增加节点数；重复边保留并计入边数，自环合法。所有输入先完整校验，
即使部分边不在所查询的可达子图中，也不能包含非法行。
最大 4096 个不同节点、32768 条边；超限报 RangeError。这不是名称总字节数或执行时间预算。

```xlang
import graph

var jobs = [["parse", "check", 2], ["check", "emit", 3], ["parse", "emit", 9], ["docs"]]
println(graph.isDag(jobs))                 // true
println(graph.shortestPath(jobs, "parse", "emit").length)  // 2，最少边数
println(int(graph.dijkstra(jobs, "parse")["emit"]))        // 5，最小权重
```

## 九项 API

| 函数 | 返回类型 | 行为 |
| --- | --- | --- |
| `isDag(edges)` | `bool` | 是否为有向无环图；有环正常返回 false，空图为 true |
| `topoSort(edges)` | `array` | 拓扑序；有环抛 ValueError，空图返回 [] |
| `bfs(edges, start)` | `array` | 沿出边广度优先遍历，包含起点，每个节点最多一次 |
| `shortestPath(edges, start, target)` | `array` | 最少边数路径，包含两端；**不使用权重**；不可达抛 ValueError |
| `dijkstra(edges, start)` | `dict` | 起点到所有可达节点的最小 float64 权重和；起点距离为 0，不可达节点没有键 |
| `components(edges)` | `array` | 弱连通分量，忽略边方向，每项是节点数组 |
| `scc(edges)` | `array` | 强连通分量，每项是节点数组，按有向可达性区分 |
| `nodes(edges)` | `array` | 去重后按原始 UTF-8 字节字典序排列的节点；不是区域化排序 |
| `neighbors(edges, node)` | `array` | 出邻居，按边出现顺序去重；自环可包含节点自己 |

查询的起点、终点或节点必须已声明；不存在时抛 ValueError。
起终点相同的 shortestPath 返回 `[start]`。
Dijkstra 要求**所有边**权重非负，包括起点不可达分量中的边；零权重和重复边允许。
溢出的候选路径不会遮蔽随后找到的有限最短路；若某个从起点可达的节点
最终没有可表示为有限 float64 的最短距离，则抛 RangeError。不可达分量里的
大权重路径不会被计算，也不会误报溢出。
其他算法不使用权重，但仍检查其类型和有限性，允许有限负权。

拓扑队列按节点首次出现顺序初始化，随后按出边顺序入队，不承诺字母序。
BFS 和最少边路径同长度时依据出边顺序选择。连通分量及其内部节点按输入节点首次出现
顺序排列，而不是底层 DFS 的发现顺序。输入顺序相同则这些输出顺序稳定。

```xlang
import graph

var edges = [["a", "b"], ["b", "a"], ["b", "c"], ["alone"]]
println(graph.components(edges).length)    // 2
println(graph.scc(edges).length)           // 3
try
    graph.topoSort(edges)
catch ValueError error
    println("cycle")
end
```

## 性能与生命周期

各调用独立解析输入并建立邻接表，不保留输入，也没有隐式全局缓存。多次查询同一大图
会重复建模。名称查找使用 XRT 字节键 map；建模成本还包括读取、散列和复制名称字节。

不计建模和结果输出：BFS、拓扑、弱连通分量和 SCC 为 O(V+E)；Dijkstra 使用索引最小堆，
每个节点最多占一个堆槽，O((V+E) log V)；节点排序 O(V log V) 次字符串比较。
SCC 使用显式栈 Kosaraju，不依赖深递归；算法工作空间按实际图规模分配。

结果独立拥有节点名和容器，更新输入、下一次调用或释放输入不影响已返回结果。
无共享可变图状态，独立调用可并发；调用期间不得并发修改输入 xvalue。
分配失败抛 MemoryError；不将其改写为普通 ValueError，也不返回截断结果。

## C API

公共头 `include/graph.h`。manifest 声明四个 C 编译单元，必须一起编译：
`graph_native.c`、`graph_traversal.c`、`graph_components.c`、`graph_distance.c`。
使用 XRT_MODULE_VALUE_CONTAINER、XRT_MODULE_ARRAY、XRT_MODULE_MAP。

语言到 C 的一一对应为：

| xlang | C |
| --- | --- |
| isDag / topoSort | xg_is_dag / xg_topo_sort |
| bfs / shortestPath / dijkstra | xg_bfs / xg_shortest_path / xg_dijkstra |
| components / scc | xg_components / xg_scc |
| nodes / neighbors | xg_nodes / xg_neighbors |

C 输入为借用的 `xvalue*`，字符串查询参数为 `const char*` 加 `size_t`；禁止用 strlen
替代已有视图长度。返回的 `xvalue*` 由调用方 `xrtValueRelease`。
失败返回 NULL 并设置 XRT error；isDag 的 false 也可能表示正常有环，必须检查
`xrtGetError()` 区分。每次调用清除旧错误。

C 名称按完整字节比较，支持内嵌 NUL。当前 `xl.exe` 的独立复现与
包内 XLang 边界测试均验证内嵌 NUL 名称可完整查询，且不会误匹配其前缀。

## 范例与门禁

2026-10-22：`uint64` 权重原被原生模型误拒绝为 `ValueError`，C/XLang 最小红例均复现；现与有符号整数一样转为 float64 参与 Dijkstra，布尔和非有限浮点仍拒绝。成对范例以无符号权重演示同一条边，输出保持不变。Windows 严格 C、XLang JIT 与生成原型通过；Linux C ASan/UBSan、XLang 与双语范例通过。Windows 综合门禁仍在既有 `xl.dll` 缺 `__xl_runtime/xl6_collection_count.h` 处停止，完整 AOT 未签收。

发布目录内：`xl run examples/extlibs/graph_basic.xl`。
完整九项 API 的 C 对照为 `extlibs/graph/examples/basic.c`，共同预期为
`examples/extlibs/graph_basic.expected.txt`。

开发仓库从 demo6 运行：

```powershell
./release/xl.exe run release/examples/extlibs/graph_basic.xl
./release/xl.exe run release/extlibs/graph/examples/basic.c
./tools/run_extlib_c.ps1 -Package graph -Source release/extlibs/graph/examples/basic.c
./release/extlibs/graph/test/check_c.ps1
./tools/check_extlib_graph.ps1 -SkipAot
```

C 范例显式包含四个算法单元，可以直接 `xl run`；正式扩展包仍按 manifest
分别编译这些源文件。`-SkipAot` 只跳过主线 AOT 构建，DLL 复用检查仍会执行。
`test/check_c.ps1` 只跑 Windows 严格 GCC 下的三组 C 测试，使用测试专用
XRT 内存调试模块和独占构建目录；不依赖后续的 DLL/AOT 主线门禁。
C 范例构图与结果读取使用显式检查，失败时释放已拥有的 `xvalue`；
两平台门禁均以 `-DNDEBUG` 构建执行，避免 Release 模式跳过构图操作。
Windows 门禁覆盖独立 C、128 组参照图、容量/Unicode/NUL/非法输入、218 个分配失败点、
独立线程、九个实际生成原型、JIT/AOT、DLL 上下文复用和双语黄金输出。
Linux 包级门禁为 `bash release/extlibs/graph/test/check.sh`：先运行原有的 C
ASan/UBSan、泄漏检查，再执行 XLang 边界测试，并将 XLang 范例与已核对的
C 范例黄金输出逐字节比较。包级脚本在本次运行专用的临时目录中保存 C
编译结果和测试日志，退出时清理；同时直接核对 C/XLang 范例输出。
独立 C 门禁仍可用 `bash tools/check_extlib_graph.sh`；
设置 GRAPH_DECLARATIONS 可核对生成原型的 LP64 合同。
独立 C 门禁与 Windows 完整门禁默认使用独占目录并在退出时清理；
需要保留 `results.json` 或编译产物时，分别显式设置 `GRAPH_WORK_DIR`
或传入 `-WorkDirectory`。旧的固定门禁目录不会被自动删除。

共享语言失败单独记录，不作为通过用例；以上范围不代表 32 位/ARM、TSan、
全部性能场景或同一可变输入并发已经验收。具体构建与结果见 docs 下的交付记录。

2026-10-16：补 C/XLang 对照边界：不可达分量中的负权仍拒绝，不可达的
极大权重链不触发距离溢出。当前 Windows 严格 GCC/C/XL JIT、Linux C
ASan/UBSan、128 组参照图和 218 个故障注入点通过；Windows 完整门禁
继续停在主线 `xl6ValueIndexReadCopyV1` AOT 缺符号，未签收发布级。

2026-10-17：修复 Dijkstra 遇到溢出候选便立即失败、遮蔽稍后有限路径的问题。C 负例先红后绿；无可表示最短路时仍保留 RangeError。Windows 严格 C 边界/故障/线程及 XLang JIT、Linux ASan/UBSan、128 组参照图和双语范例通过。完整 Windows 门禁仍停在已记录的 AOT 共享索引符号，未签收发布级。

2026-10-19：C 范例可直接 `xl run`，且与 GCC、包运行器的黄金输出一致；
Linux C ASan/UBSan、边界和双语门禁通过。Windows `-SkipAot` 门禁在
`xl.dll` 宿主复用处遇到主线发布 DLL 的运行时 VFS 缺头文件；同一最小脚本
通过 `xlc.dll` 宿主。复现和分析见 `dev/extlib_blockers/graph_dll_vfs_header.md`。
该阻断未在 graph 包内绕过，Windows 完整门禁尚未签收。

2026-10-18：Linux 包级脚本改用独立临时目录，避免并行测试写入固定
`build/test/graph_linux_guard`；C/XLang 范例另作直接逐字对照。128 组参照图、
218 个分配故障点、四线程、ASan/UBSan、XLang 边界和范例均通过。
包实现未改，完整 AOT 的共享索引缺符号阻断不变。

2026-10-20：C/XLang 边界补齐重复有向边的不同权重与最少边数/最小权重
语义对照。Windows 专属门禁通过 C 算法、独立参照、分配故障、线程、
语言 JIT 和范例，仍在已登记的 `xl.dll` VFS 缺头处停止；Linux C
ASan/UBSan 和独立的 Linux XLang 边界均通过。算法未改，两个主线阻断
仍不在包侧绕过，不签收完整发布级。

同日容量测试新增 map 多次扩容后的首/中/末节点名逐字校验，不只检查
BFS 结果数量；Windows `test/check_c.ps1` 与 Linux C ASan/UBSan、语言和
双语联合门禁通过。`test/check_c.ps1` 提供不经 DLL/AOT 的 C 聚焦验证，
复用现有测试运行时，不改变包算法或发布级阻断状态。

2026-10-21：三份 C 测试共用的建图辅助函数和关键操作原置于
`assert(...)` 中，`NDEBUG` 会使发布配置的测试近乎空跑。公共测试头现
显式保留断言，Windows 两条 C/综合门禁与 Linux ASan/UBSan 门禁都以
`-DNDEBUG` 编译 C 测试；预处理确认图查询仍执行。Windows 专项 C 测试、
Linux 128 组独立参照图、218 个分配故障点、四线程及 XLang 对照通过。
Windows 综合门禁仍在已登记的 `xl.dll` VFS 缺头处停止，未签收整包。

2026-10-23：新增九个语言入口的逐点分配故障测试，分别使用有效图与含
非法行的图，核对 `MemoryError`/`ValueError` 分类、正常结果和故障清除后
原图仍可查询。测试已登记到包清单并接入 Windows/Linux 联合门禁。
Linux C ASan/UBSan、XLang 与双语门禁全部通过；Windows 严格 C、
XLang JIT/代码生成、ABI 原型及 C 范例通过，综合门禁仍在既有的
`xl.dll` 运行时 VFS 缺头问题处停止，完整 AOT 亦未签收。
