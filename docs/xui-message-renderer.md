# 消息气泡自绘

`MessageList.setNodeRenderer(renderer)` 注册受管自绘回调，传入发送控件、节点索引、只读结点、绘制上下文、矩形和状态。返回 0 使用默认绘制，正值表示已经绘制，负值通过当前渲染调用报告。传入 `null` 恢复默认绘制。

`CallbackConstMessageNode` 的数值字段直接读取原生记录。读取文本属性会复制字符串，该字符串可以在回调后继续使用。发送控件、结点和绘制上下文的回调视图均在返回时失效，过期访问产生 checked 错误。

回调可通过 `Proxy.fillRect()` 等已有绘制接口使用当前目标。渲染中修改消息正文、替换节点或清空列表受原生 Busy 检查保护。回调注册可更换，当前调用持有正在执行的回调。

[气泡自绘范例](../lib/bindings/xui/examples/message_renderer.xl) 把系统消息显示为色块，普通消息仍由控件绘制。

## 事件节点

`MessageList.setEvent(handler)` 的 `event.node` 返回当次事件的 `CallbackConstMessageNode?`。光标离开节点等没有目标的事件返回 `null`。

回调可以修改正文、替换节点或清空列表，`event.node` 在本次回调内继续读取事件开始时的字段。原生层延后释放被回调引用的原有文本缓冲区，事件分派无需复制正文或分配缓冲区。访问 `node.text` 等属性时才复制为独立字符串。回调结束后，读取保留下来的节点视图会产生 `StateError`。

[消息列表范例](../lib/bindings/xui/examples/messagelist.xl) 在点击消息时显示发送者和正文。
