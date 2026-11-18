# XGE 有序输入事件

在引擎创建线程调用 `xge.readInputEvent(buffer)` 获取 `(status, info)`。`InputEventInfo` 是独立的值记录，包含事件序号、时间、按键、指针位置、修饰键、码点和 IME 选区与替换范围。文本按字节写入调用方的 `Buffer`，支持内嵌 NUL。指针位置使用 framebuffer 像素，IME 文本偏移使用 UTF-8 字节。

| 状态 | 含义 |
| --- | --- |
| `XGE_INPUT_READ_READY` | 消费队首事件，写入恰好 `info.iTextSize` 字节。缓冲区长度和其余字节保持原值。 |
| `XGE_INPUT_READ_NEED_BUFFER` | 返回当前队首元数据，`info.iTextSize` 是所需长度。队列和缓冲区均保持原值，扩容后可以重试。 |
| `XGE_INPUT_READ_EMPTY` | 队列为空，返回已清空的事件元数据。 |

这些入口与原生 `xgeInputEventGet` 共享同一个队列。若 XUI 或另一个消费者在两次读取间取走了队首，重试会读取新的队首。`iSequence` 可用于确认两次读到的是同一事件。传入长度为零的 `Buffer` 可以直接消费无文本事件。

`xge.postInputEvent(info, text, size=-1)` 支持 `bytes`、`string` 和 `Buffer`，按实际字节数提交。省略文本会提交零字节。`size` 为 `-1` 时使用全部内容，非负值截到实际长度，其他负值报错。原生队列分配序号、补充合成标记并复制文本，`info.iSequence` 和 `info.iTextSize` 不影响投递。无效事件、错误线程和分配失败走 checked 错误。投递失败不会将半个事件放进队列。

两个入口均直接声明原生 API。元数据按 C 值复制。读取把文本复制到既有 `Buffer`，没有文本扫描、结果对象分配或追加 NUL。长文本仍由队列分配和释放，队列扩容和语言调用的类型、状态及 Buffer 检查也有成本。

运行 [input_events.xl](../lib/bindings/xge/examples/input_events.xl) 可观察缓冲不足、重试和先后顺序。示例使用合成事件，不需要窗口或输入设备。
