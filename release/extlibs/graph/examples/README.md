# graph 双语范例

从仓库根目录运行，Windows用 `release/xl.exe`，Linux用 `release/xl`：

```powershell
release/xl.exe run release/extlibs/graph/examples/basic.c
release/xl.exe run release/extlibs/graph/examples/from_json.xl
```

两者展示相同的依赖图、九项 API、孤立节点、带权路径和环拒绝。
完整十行输出见 `release/examples/extlibs/graph_basic.expected.txt`；
门禁在 GCC、直接TCC、独立程序及 xl/xlc 上下文顺序复用时逐字核对。

`shortestPath` 优先最少边数，因此选择 parse→emit；Dijkstra计算权重，
会选择 parse→check→emit，总成本5。拓扑序按声明和出边顺序入队，
不等于把节点名称排序；SCC与弱分量的内外层按首次声明顺序输出。
C 的每个结果都独立拥有，用完调用 `xrtValueRelease`；输入仅借用。

`from_json.xl` 展示标准JSON配置的正常输入来源，并未把语言数组
序列化来绕过错误。原 `release/examples/extlibs/graph.xl` 和
`graph_basic.xl` 仍是必须通过的语言字面量范例；当前 M1 容器投影
会使它们失败，不能根据这对JSON/C范例通过宣称整包已经可发布。

本轮新增 `test/ordered_records_c.c` 是聚焦测试，不再复制一份同功能
应用程序。它用空串、NUL、中文和反向声明完整检查结果寿命，双平台
门禁配合独立Python参照验证27776条有序结果；默认 C 输出不替代参照。
运行完整门禁：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/check_extlib_graph.ps1
```

Linux用 `bash release/extlibs/graph/test/check.sh`。当前两者都保留
最后M1正例的非零结果与失败现场；不提供跳过它以换取绿色发布的模式。
