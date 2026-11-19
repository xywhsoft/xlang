# ListView 的行绘制和激活

`ListView.setItemRenderer(callback)` 提供行索引、行矩形、绘制上下文和状态位。返回 `0` 使用默认绘制，返回正值表示回调已完成整行绘制。绘制上下文和回调内的控件视图在本次回调结束后失效。回调异常由 `Context.render()` 交付。

`ListView.setActivate(callback)` 接收控件、行索引和按钮。Enter、左键双击和无障碍激活使用同一原生入口。激活回调可以关闭控件。两个回调均可用 `null` 解绑，原生会保留正在执行的回调直到它返回。

`ListView.setEnabledItems(array<c_int> enabled, int size=-1)` 采用和已有 Buffer 入口相同的原生规则。`0` 禁用对应行，其他值启用。未覆盖的后续行默认启用，空前缀恢复全部启用。XUI 在调用内复制状态数组，之后修改输入数组不会影响列表。

数组入口在调用前按 C 互操合同投影为整数数组。回调注册和调用保留闭包及其代码。回调要求使用默认绘制时，渲染读取最新字体，以防回调刚换掉字体。这些声明直接调用原生接口。

Windows 和 Linux 上各重复设置两行状态 1,000 次。语言数组入口与手写 C 各记录到 1,000 次 XRT 堆分配和释放，循环结束时的存活块数及字节数均与循环前一致。语言入口仍会转换数组元素。见[验收记录](../dev/release-materials/provenance/xui-list-view-events-20261120.json)。

[list_row_events.xl](../lib/bindings/xui/examples/list_row_events.xl) 展示默认行、自绘色块和禁用行。选中可用行后按 Enter 或双击，可以在列表下方看到激活结果。
