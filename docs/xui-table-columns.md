# TableView 和 TableGrid 列配置

`setColumnsDetailed(settings, ids, titles, size=-1)` 适用于 `TableView` 和 `TableGrid`。每个 `TableColumnSettings` 可指定数值 ID、宽度与限制、可见性、是否可调整宽度、对齐、单元格类型和样式。`ids` 和 `titles` 可在同一位置传 `null` 或空串，原生表格复制有值的文本。`setColumns(titles, widths)` 适合只需标题和宽度的表格。

`bVisibleSet` 或 `bResizableSet` 为零时，原生表格使用默认行为。需要隐藏或禁止调整宽度时，先将对应的 `Set` 字段设为 `1`，再将值设为 `0`。设置宽度超过上下限时，原生表格会取允许范围内的宽度。替换 Grid 列配置沿用原生编辑结束规则。

`getColumn(index)` 返回 `(settings, id, title, rect)`。配置和矩形是值，ID 和标题是独立字符串，可在替换列或关闭控件后继续使用。矩形反映原生布局，尚未布局的列可以返回零矩形。无效索引产生 checked 错误。`TableView.getSortColumn()` 返回当前列号和降序标记，列号为 `-1` 表示尚未排序。

详细设置使用编译器已有的 C 记录数组投影和原生拥有式文本复制。投影随列数线性增长，原生再使用最多 64 项的临时栈记录接入同一套列校验和替换。读取在一次原生调用内取得借用记录，随后复制有效的文本字段。任一字段转换失败都会回滚整组结果。

[table_columns.xl](../lib/bindings/xui/examples/table_columns.xl) 把 `Price` 列请求宽度设为 160，上限设为 128。运行示例会输出 `Price 128.0`。

## 同时设置列和回调

`setColumnsWithCallbacks(settings, ids, titles, configure=...)` 在 `TableView` 或 `TableGrid` 上一次提交列定义、格式化器和绘制器。`configure` 只调用一次。通过它收到的 `CallbackTableColumnSetup` 按新列表的零基索引调用 `setFormatter` 和 `setRenderer`，再返回 `0` 提交。`size` 和 `setColumnsDetailed` 使用相同的规则。省略 `configure` 时只设置列定义。

配置回调运行时仍可读取原有列和回调。同一表格在此期间的列替换和回调注册返回 Busy。配置视图在回调结束后失效。提交前的错误保留旧列和旧回调。提交后，原生才释放旧回调。如果旧回调的清理报告错误，新列和回调已经生效。`TableGrid` 在配置前依照常规的列替换规则结束正在进行的单元格编辑。

每个已配置的回调通过表格的原生拥有目录保留。单元格格式化和绘制继续使用现有的直接调用路径。配置时会投影列数组、复制列文字、为新回调登记拥有引用，并在提交时转移已经持有的引用。

Windows 和 Linux 分别重复配置一列和一个受管格式化器 1,000 次。xlang 每次记录 5 次 XRT 分配，手写 C 为 3 次。两条路径在每轮回收上一份配置，循环内均无重分配。该样本度量列配置开销。 [验收记录](../dev/release-materials/provenance/xui-table-column-setup-20261123.json)

[table_column_callbacks.xl](../lib/bindings/xui/examples/table_column_callbacks.xl) 把库存列和格式化器一起安装，读取单元格后输出 `有货`。
