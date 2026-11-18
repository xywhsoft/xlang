# TreeView 节点配置与快照

`TreeView.setNodes(states, texts, size=-1)` 使用 `TreeNodeState` 和文本数组一次设置整棵树。节点 ID 必须为唯一的非负整数，父节点排在子节点前，根节点的 `iParent` 为负值。`bEnabled` 为 `1` 时节点可交互。展开状态、图标位、复选框位和勾选状态都可设置，`iDepth` 和 `bHasChildren` 由原生树根据层级计算。文本为 `null` 时显示为空串。

`getNode(index)` 按输入顺序读取，`getNodeById(id)` 按 ID 读取，`getVisibleNode(index)` 按当前展开后的行顺序读取。它们返回 `(state, text)`，节点状态和文本可在替换树或关闭控件后继续使用。找不到节点时会产生 checked 错误。折叠节点的深度依然可通过 ID 查询。

批量设置使用编译器的 C 记录数组投影。原生层先暂存节点，复制显示文本，计算可见行并校验布局，再发布整棵树。无效层级或构建阶段内存不足时，当前树和选中项保持原值。拥有式适配器也使用同一个提交过程，直接移交暂存节点、索引和文本。回调提供的文本经正常构造分配一次，发布时沿用这份原有存储。

[tree_nodes.xl](../lib/bindings/xui/examples/tree_nodes.xl) 会输出 `main.xl depth 1`，无需打开窗口。
