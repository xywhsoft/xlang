# CodeEdit 输入处理器

`CodeEdit.setInputHandler()` 安装由编辑器持有的处理器，传 `null` 移除。
键盘、IME 提交、粘贴及补全使用同一套输入阶段。事件提供输入来源、原始文本、
文档版本、选择区域和 UTF-8 字节范围。

- **预览**通过 `XUI_CODE_INPUT_PREVIEW` 表示。可以准备替换文本、修改选择，
  或加上 `XUI_CODE_INPUT_ACTION_CONSUME` 跳过原始输入。
- **提交**通过 `XUI_CODE_INPUT_COMMITTED` 表示。此时可以观察已写入的文档、
  写入后的选择和最近一次写入范围，还可以添加后续编辑和补全、签名提示动作。
  一次输入产生的编辑归入同一个撤销组。

`response.setEdits(ranges, replacements)` 接受按起点升序且互不重叠的范围。
范围用 UTF-8 字节偏移，包含起点并排除终点。`setEdits()` 会保活替换文本并设置
编辑标志，传空列表清除已准备的编辑。校验或分配失败会保留上一次成功准备的动作。
`setSelection(anchor, caret)` 设置选择标志。`setFlags()` 替换标志集合，若要保留
编辑或选择标志，先取 `response.flags()` 再用按位或组合。

事件、文档、控件和响应是回调期视图。回调结束后再访问会抛出 `StateError`。
文本按实际字节长度读取，返回的字符串和按值读取的范围可以保存。回调体再次请求
同一 Context 的调用作用域会收到 Busy。预览阶段的异常会阻止输入。提交阶段的异常
会交给调用边界，已经写入的内容可以通过该次输入的撤销组撤销。

输入处理器直接映射到 XUI 的原生回调。无替换动作时无需创建替换文本存储。
有替换动作时，语言层投影参数记录，原生层将记录和文本一起放入一块拥有内存，
待编辑器消费后释放。批量写入沿用原生文档的撤销事务。

在同一静态 AOT 程序中比较手写 C 与 xlang 的预览处理器。两者都消耗输入，文档保持
原样。每种处理器取七轮中位数，每轮派发 2,000 次键盘事件。Windows 每次派发耗时
为 C 4.91 µs、xlang 12.01 µs，Linux 为 C 1.87 µs、xlang 3.79 µs。两条路径
在计时区间内的 XRT 分配、释放和重分配均为零。创建、注册和预热在计时前完成。

[括号配对范例](../lib/bindings/xui/examples/code_input_handler.xl) 在预览阶段用一对
括号替换输入，并将光标留在中间。
[验收记录](../dev/release-materials/provenance/xui-codeedit-input-20261119.json)。
