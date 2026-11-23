# TreeView 节点配置与快照

`TreeView.setNodes(states, texts, size=-1)` 使用 `TreeNodeState` 和文本数组一次设置整棵树。节点 ID 必须为唯一的非负整数，父节点排在子节点前，根节点的 `iParent` 为负值。`bEnabled` 为 `1` 时节点可交互。展开状态、图标位、复选框位和勾选状态都可设置，`iDepth` 和 `bHasChildren` 由原生树根据层级计算。文本为 `null` 时显示为空串。

`getNode(index)` 按输入顺序读取，`getNodeById(id)` 按 ID 读取，`getVisibleNode(index)` 按当前展开后的行顺序读取。它们返回 `(state, text)`，节点状态和文本可在替换树或关闭控件后继续使用。找不到节点时会产生 checked 错误。折叠节点的深度依然可通过 ID 查询。

批量设置使用编译器的 C 记录数组投影。原生层先暂存节点，复制显示文本，计算可见行并校验布局，再发布整棵树。无效层级或构建阶段内存不足时，当前树和选中项保持原值。拥有式适配器也使用同一个提交过程，直接移交暂存节点、索引和文本。回调提供的文本经正常构造分配一次，发布时沿用这份原有存储。

[tree_nodes.xl](../lib/bindings/xui/examples/tree_nodes.xl) 展示文件名、节点深度和实际路径，直接在终端运行。

## 节点附加数据

`setNodesWithData(states, texts, size=-1, values=null)` 同时提交整棵树和每行的数据。`values` 是一组按输入顺序排列的值，长度须等于实际提交行数。整组传 `null` 时，各节点的数据从空值开始。节点和数据准备成功后一起发布。准备阶段遇到层级错误或内存不足时保留现有树及数据。

`setNodeData(id, value)` 和 `getNodeData(id)` 按节点 ID 读写独立的数据快照。`null` 清除该节点的数据。折叠或移动可见行时，数据留在对应节点上。清空或替换整棵树会释放原数据，复用 ID 时使用新提交的数据。先前读出的快照可以独立使用。

`setDataAdapter(count, read)` 的行回调分别接收节点输出和数据输出。用 `node.setText()` 写入显示文本，用 `value.set()` 提交数据快照。两个输出视图都只在回调内有效。刷新会在旧树可读期间构建新树。如果行回调出错，绑定继续有效，可修正数据后再次调用 `refreshAdapter()`。原生拥有图跟踪已发布的数据，析构回调在整棵旧树解除拥有关系后运行。

原生节点记录布局保持兼容。首次附加数据时按节点容量分配指针数组。已分配槽位中的原生读写直接保留或释放值；语言复合数据快照会有遍历和分配成本。

预热后的 4,000 对整数读写在 Windows 和 Linux 的 C、xlang 样本中均无 XRT 分配。双字段对象的 xlang 样本每对读写平均分配 13 次，含结果取字段和校验。C 对照循环保留和读取已有值，实测零分配。

## 自绘行和图标

`TreeView.setItemRenderer(callback)` 为每个可见行提供节点、行号、绘制区域和状态。返回 `1` 表示已经绘制整行，返回 `0` 使用默认行绘制。`setIconRenderer(callback)` 只绘制预留图标的区域，文本、展开箭头和点击命中由 TreeView 处理。传入 `null` 会解绑对应回调。

`CallbackTreeNode`、`CallbackWidget` 和 `CallbackDrawContext` 仅在当前回调有效。读出的节点文本可以独立保存。回调清空或替换节点时，当前节点快照仍可在回调内读取。XGE 保留这次读取所用的原有文本存储到回调结束。原生保存闭包的引用，渲染中的回调在调用尾部释放临时引用，回调抛出的错误由调用 `render()` 的一侧收到。

节点的标量字段直接读取，读取 `node.text` 会复制字符串。自绘仍需执行回调调用和生命周期检查。[tree_painters.xl](../lib/bindings/xui/examples/tree_painters.xl) 演示同一棵树的两种自定义图标。
