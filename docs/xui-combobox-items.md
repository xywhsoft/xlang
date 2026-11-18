# ComboBox 选项配置与快照

`setItemData(texts, values, enabled, separators, icons, size=-1)` 批量设置文本、业务值、启用状态、分隔项和图标 ID。原生最多接受 64 项。五个数组以相同索引对应同一选项。默认要求数组长度一致。指定非负 `size` 时，写入项数取该值与各数组长度的最小值。`null` 文本显示为空串，内嵌 NUL 会产生 checked 错误，分隔项始终不可选。

`getItem(index)` 返回 `(text, value, enabled, separator, icon)`。越界产生 checked 错误。返回的文本是独立副本，替换选项或关闭控件后仍可使用。原生 `pUser` 仍供 C 调用方管理。xlang 可以用业务值查询自己的应用数据。

写入时编译器构造临时 C 记录数组，原生复制所需文本。读取直接调用一次原生描述接口，复制标量和显示文本。

[combobox_items.xl](../lib/bindings/xui/examples/combobox_items.xl) 无需打开窗口，按业务值选中一项并输出 `Selected Office 20`。
