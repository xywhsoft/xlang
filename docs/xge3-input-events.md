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

## 小程序触摸

在宿主事件线程调用 `xge.miniProgramTouch(phase, array<MiniprogramTouch>, size=-1)`。
`size` 按触点记录计数。默认使用整个数组，非负值限制处理的前缀，其他负数抛出可捕获的范围错误。单批最多 `XGE_TOUCH_MAX` 个触点。空批次沿用原生的静止点更新规则。无效阶段在修改触点、投递输入或通知场景前返回 `XGE_ERROR_INVALID_ARGUMENT`。

`xge.touchFind(id)` 按 64 位触点 ID 返回 `(status, TouchPoint)`。触点已结束或不存在时返回 `XGE_ERROR_FILE_NOT_FOUND`，其余情况下按原生状态码判断结果。成功的 `TouchPoint` 是独立值，位置与位移使用 framebuffer 像素。

批量输入将语言数组转换为临时连续 C 记录，只调用一次原生函数。非空批次需要一次 XRT 分配，并逐项复制记录。分配失败发生在原生调用前。查询返回值直接来自原生输出，暖态无 XRT 堆分配。

运行 [miniprogram_touch.xl](../lib/bindings/xge/examples/miniprogram_touch.xl) 可在桌面无窗环境验证两个触点和 DPR 坐标换算。

## 小程序宿主回调

`xge.setMiniProgramBridge(requestFrame, loadResource, audioCommand)` 将三个可选回调作为同一组注册。省略全部参数即可清除。帧调度和音频调用保留宿主的非负结果。音频入口接受 `bytes` 或 `Buffer`，以实际字节数调用原生接口。回调通过 `MiniProgramAudioData.size` 读取长度，通过 `bytes` 取得可独立保存的字节副本。回调结束后，视图会失效。

资源回调接收完整的 `res://` URI，使用 `ResourceOutput.set()` 提交数据。输出会复制实际字节，支持内嵌 NUL。成功交付的资源可以在替换回调或关闭桥接后继续使用。分配或注册失败会保留旧桥接。旧回调释放出错时，已发布的新桥接仍然生效。

受管回调在注册线程运行。帧调度和音频回调期间，底层固定该次调用的回调组。注册时保存拥有引用，查找时使用短锁，锁外调用宿主。音频只读长度时无需复制数据。资源输出和显式读取音频 `bytes` 会分配并复制。使用 `miniProgramUnitChecked()` 收尾，`xge.unit()` 也会清理预初始化注册。

使用预先创建的三字节载荷连续调用 1,000 次，Windows 和 Linux 上的原生 C 与 xlang 循环均未触发 XRT 堆操作。该测量的 xlang 回调只读取长度，不包含首次注册或音频数据复制。

运行 [miniprogram_bridge.xl](../lib/bindings/xge/examples/miniprogram_bridge.xl) 可用本地回调验证帧请求、资源读取和音频指令。

XGE 的小程序 JavaScript 适配器使用同一原生回调组。单线程 wasm32 构建可显式启用 `XGE_ENABLE_MINIPROGRAM_JS` 并链接配套的 Emscripten JavaScript 库。回调通过固定的 WASM 导入函数执行，原生最后一个引用释放时回收 JavaScript 注册。资源回调向原生输出复制实际字节。JS 主动提交音频时将实际字节写入 WASM 内存，音频处理函数在本次调用期间借用该视图。

Node.js 的实际 WASM 回归覆盖帧调度、资源加载、音频状态、分配失败、内存扩充及桥接替换。它使用测试 canvas，微信客户端的图形与音频设备验收仍待进行。Windows 和 Linux 的匹配扩展包已通过 xlang JIT、动态 AOT 和静态 AOT 回归。详见 [验收记录](../dev/release-materials/provenance/xge-miniprogram-js-20261119.json)。
