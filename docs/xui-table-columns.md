# TableView 和 TableGrid 列配置

`setColumnsDetailed(settings, ids, titles, size=-1)` 适用于 `TableView` 和 `TableGrid`。每个 `TableColumnSettings` 可指定数值 ID、宽度与限制、可见性、是否可调整宽度、对齐、单元格类型和样式。`ids` 和 `titles` 可在同一位置传 `null` 或空串，原生表格复制有值的文本。`setColumns(titles, widths)` 适合只需标题和宽度的表格。

`bVisibleSet` 或 `bResizableSet` 为零时，原生表格使用默认行为。需要隐藏或禁止调整宽度时，先将对应的 `Set` 字段设为 `1`，再将值设为 `0`。设置宽度超过上下限时，原生表格会取允许范围内的宽度。替换 Grid 列配置沿用原生编辑结束规则。

`getColumn(index)` 返回 `(settings, id, title, rect)`。配置和矩形是值，ID 和标题是独立字符串，可在替换列或关闭控件后继续使用。矩形反映原生布局，尚未布局的列可以返回零矩形。无效索引产生 checked 错误。`TableView.getSortColumn()` 返回当前列号和降序标记，列号为 `-1` 表示尚未排序。

详细设置使用编译器已有的 C 记录数组投影和原生拥有式文本复制。投影随列数线性增长，原生再使用最多 64 项的临时栈记录接入同一套列校验和替换。读取在一次原生调用内取得借用记录，随后复制有效的文本字段。任一字段转换失败都会回滚整组结果。

[table_columns.xl](../lib/bindings/xui/examples/table_columns.xl) 把 `Price` 列请求宽度设为 160，上限设为 128。运行示例会输出 `Price 128.0`。
