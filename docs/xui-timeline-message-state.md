# 时间轴与消息列表的数据接口

`TimeLineView.getLayer(index)` 返回层 ID、名称、可见、锁定、选中、行高和颜色。层 ID 在调序后保持稳定，索引按当前顺序计算。`getFrame(layer, frame)` 返回层索引、帧号和类型，合法位置没有帧记录时类型为 `XUI_TIMELINE_FRAME_EMPTY`。`setFrame(layer, frame, type)` 设置关键帧或其他帧类型，沿用原生回调和校验。`getSpan(id)` 返回层索引、ID、起止帧、类型、自定义类型名、标签和颜色。

`MessageList.setNodes()` 按下标组合 ID、正文、发送者、时间、父 ID、标题、类型、标志和辅助类型，再一次性替换原生节点。默认要求各列长度一致。指定非负 `size` 时，根据各列实际长度截取相应前缀。传入空数组可以清空节点。这里沿用原生替换语义，原有节点附带的 Document 和状态也随原节点退役。

`MessageList.getNode(index)` 按相同顺序返回上述字段。可选文本缺省时读取为空串。无效索引、区间 ID 或帧位置产生 checked 错误。输出快照的字符串可在控件修改或关闭后继续使用。原生应用指针仍由 C 调用方管理。

每个方法生成一次直接原生调用。写入消息前构造临时记录数组，检查文本中的 NUL，原生复制文本并建立节点索引。快照复制读取所需的字符串，标量按值返回。

[时间轴范例](../lib/bindings/xui/examples/timeline_state.xl) 输出 `Move 4 12`。[消息列表范例](../lib/bindings/xui/examples/message_nodes.xl) 输出 `review Note`。两者均无需打开窗口。
