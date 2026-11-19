# CodeEdit 诊断

在编辑器线程读取 `documentVersion()` 和待检查的源码。分析结果通过
`applyDiagnostics(version, ranges, severities, codes, messages, sources, userData)`
提交。位置以 UTF-8 字节计数，区间包含起点并排除终点。一次提交会替换整个诊断集，
空批次会清空诊断。文档版本不符时抛出可捕获的错误，现有诊断保持原样。
诊断存储和重绘调度需要的分配均在发布前完成，失败时保留旧诊断。重复提交相同的诊断集
会复用现有存储与绘制缓存。

`diagnosticCount()` 和 `getDiagnostic(index)` 用于枚举当前诊断。
`hitTestDiagnostic(x, y)` 接收 Context 坐标，按已布局的视觉行查找。
返回顺序为索引、矩形、字节范围、等级、代码、消息、来源和业务 ID。
未命中时索引为 `-1`，文本字段为空值。多个诊断重叠时，先选更高等级，
同级选范围更短的项。读取的文本是独立快照，原生空字符串和 NULL
分别映射为 `""` 和 `null`。

语言方法直接调用 XUI 入口。提交会投影数组记录，原生注释存储保留文本副本。
命中查询先准备布局，再读取注释。退出布局作用域后复核控件和版本，然后取得返回字段，
避免交付失效的原生字符串。语言层复制非空值字符串并在转换失败时回滚整组输出。
预热后，空诊断集连续查找 1,000 次，XRT 无堆操作。静态 AOT 的同进程对照测试中，
Windows 每次空查询的中位耗时为原生 C 73 ns、xlang 3.57 µs，Linux 为 73 ns、1.62 µs。
每个中位数来自七轮各 100,000 次调用，范围是空诊断集和结果投影。xlang 路径仍执行接收对象
检查、错误检查及多返回值交付。

运行 [CodeEdit 诊断范例](../lib/bindings/xui/examples/code_diagnostics.xl)
可看到未知名称下的诊断标记。程序同时读取并打印 `E01` 和 `Unknown name`。
范例使用 XUI 的共用窗口入口，字体和图形准备同其他窗口范例。
[验收记录](../dev/release-materials/provenance/xui-codeedit-diagnostics-20261119.json)。
