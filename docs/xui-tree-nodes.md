# TreeView 节点配置与快照

`TreeView.setNodes(states, texts, size=-1)` 使用 `TreeNodeState` 和文本数组一次设置整棵树。节点 ID 必须为唯一的非负整数，父节点排在子节点前，根节点的 `iParent` 为负值。`bEnabled` 为 `1` 时节点可交互。展开状态、图标位、复选框位和勾选状态都可设置，`iDepth` 和 `bHasChildren` 由原生树根据层级计算。文本为 `null` 时显示为空串。

`getNode(index)` 按输入顺序读取，`getNodeById(id)` 按 ID 读取，`getVisibleNode(index)` 按当前展开后的行顺序读取。它们返回 `(state, text)`，节点状态和文本可在替换树或关闭控件后继续使用。找不到节点时会产生 checked 错误。折叠节点的深度依然可通过 ID 查询。

批量设置使用编译器的 C 记录数组投影。原生层先暂存节点，复制显示文本，计算可见行并校验布局，再发布整棵树。无效层级或构建阶段内存不足时，当前树和选中项保持原值。拥有式适配器也使用同一个提交过程，直接移交暂存节点、索引和文本。回调提供的文本经正常构造分配一次，发布时沿用这份原有存储。

[tree_nodes.xl](../lib/bindings/xui/examples/tree_nodes.xl) 会输出 `main.xl depth 1`，无需打开窗口。

## 自绘行和图标

`TreeView.setItemRenderer(callback)` 为每个可见行提供节点、行号、绘制区域和状态。返回 `1` 表示已经绘制整行，返回 `0` 使用默认行绘制。`setIconRenderer(callback)` 只绘制预留图标的区域，文本、展开箭头和点击命中由 TreeView 处理。传入 `null` 会解绑对应回调。

`CallbackTreeNode`、`CallbackWidget` 和 `CallbackDrawContext` 仅在当前回调有效。读出的节点文本可以独立保存。回调清空或替换节点时，当前节点快照仍可在回调内读取。XGE 保留这次读取所用的原有文本存储到回调结束。原生保存闭包的引用，渲染中的回调在调用尾部释放临时引用，回调抛出的错误由调用 `render()` 的一侧收到。

节点的标量字段直接读取，读取 `node.text` 会复制字符串。自绘仍需执行回调调用和生命周期检查。[tree_painters.xl](../lib/bindings/xui/examples/tree_painters.xl) 演示同一棵树的两种自定义图标。
