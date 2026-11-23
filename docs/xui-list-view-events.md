# ListView 的行绘制和激活

`ListView.setItemRenderer(callback)` 提供行索引、行矩形、绘制上下文和状态位。返回 `0` 使用默认绘制，返回正值表示回调已完成整行绘制。绘制上下文和回调内的控件视图在本次回调结束后失效。回调异常由 `Context.render()` 交付。

`ListView.setActivate(callback)` 接收控件、行索引和按钮。Enter、左键双击和无障碍激活使用同一原生入口。激活回调可以关闭控件。两个回调均可用 `null` 解绑，原生会保留正在执行的回调直到它返回。

`ListView.setEnabledItems(array<c_int> enabled, int size=-1)` 采用和已有 Buffer 入口相同的原生规则。`0` 禁用对应行，其他值启用。未覆盖的后续行默认启用，空前缀恢复全部启用。XUI 在调用内复制状态数组，之后修改输入数组不会影响列表。

数组入口在调用前按 C 互操合同投影为整数数组。回调注册和调用保留闭包及其代码。回调要求使用默认绘制时，渲染读取最新字体，以防回调刚换掉字体。这些声明直接调用原生接口。

Windows 和 Linux 上各重复设置两行状态 1,000 次。语言数组入口与手写 C 各记录到 1,000 次 XRT 堆分配和释放，循环结束时的存活块数及字节数均与循环前一致。语言入口仍会转换数组元素。见[验收记录](../dev/release-materials/provenance/xui-list-view-events-20261120.json)。

[list_row_events.xl](../lib/bindings/xui/examples/list_row_events.xl) 展示默认行、自绘色块和禁用行。选中可用行后按 Enter 或双击，可以在列表下方看到激活结果。

## 虚拟行文本

`setVirtualItems(count, provider)` 按索引生成行文本。绘制只读取视口和边缘缓冲内的行，无障碍和显式读取会查询指定行。`provider` 接收一个回调期控件视图和零基索引。省略提供器时使用空标签，可与自绘行配合。提供器捕获的状态变化后，可重新调用 `setVirtualItems` 让列表更新。`setItems` 会恢复普通条目并解除提供器。

`readItemText(index)` 返回独立的字符串，保留内嵌 NUL，索引无效时抛出 checked 错误。旧的 `getItemText(index)` 保持 C 字符串语义和越界返回 `null`。无障碍名称仍是受长度限制的 C 字符串。提供器可以替换数据源或关闭列表，当前读取会放弃过期结果。回调中递归读取同一个正在执行的提供器会收到 Busy 错误。

注册通过原生拥有目录保活闭包。每次查询都经过回调和拥有引用检查。绘制直接使用文本指针和字节长度。`readItemText` 直接接管提供器交回的 XRT 缓冲区，并执行运行时的 UTF-8 校验。普通条目或原生借用文本需要复制一次。

预热后分别用 xlang 和手写 C 读取五字节标签 1,000 次。两条路径各记录到 1,000 次 XRT 堆分配和释放，无重分配。见[验收记录](../dev/release-materials/provenance/xui-list-view-virtual-text-20261120.json)。

用 `xl run list_virtual_rows.xl` 运行[虚拟行示例](../lib/bindings/xui/examples/list_virtual_rows.xl)，可以从最后一行往回滚动查看。

## 共享选择缓冲区

`xui.createListSelectionBuffer(capacity)` 创建固定容量的缓冲区。`list.setSelectionBuffer(buffer)` 将当前选择写入缓冲区，随后列表直接在这块存储中更新选择。缓冲区一次只能绑定一个 ListView。重复绑定到同一列表会直接返回。选择仍通过列表的 `setItemSelected`、`setSelected` 或界面交互修改。

`buffer.get(index)` 在创建线程返回 `0` 或 `1`。超出容量会抛错，容量内暂未使用的索引返回 `0`。列表缩短后再增长，新出现的行从未选中开始。超过缓冲区容量的条目更新会被拒绝。

`list.selectionBuffer()` 取得一份独立的受管引用。关闭任意一个引用后，列表和其他引用仍可使用缓冲区。`list.setSelectionBuffer(null)` 将当前选择复制回内部存储。解绑、换绑或销毁列表后，仍持有缓冲区的调用方可以读取最后一次绑定的选择。

原生创建使用一次固定容量分配。换绑复制当前行的选择，解绑在需要时分配内部数组。逐项读取是一次原生调用，xlang 同时执行存活性和错误检查。预热后的 10,000 轮选择切换在两端均观察到零 XRT 堆分配和重分配。计时和手写 C 对照见[验收记录](../dev/release-materials/provenance/xui-list-selection-buffer-20261123.json)。

用 `xl run list_selection_buffer.xl` 运行[共享选择缓冲区示例](../lib/bindings/xui/examples/list_selection_buffer.xl)。
