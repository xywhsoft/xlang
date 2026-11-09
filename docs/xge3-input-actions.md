# Input Actions 直接接入

`xge.createActions(actionCount,bindingCapacity)` 创建受语言生命周期管理的动作表。
容量和动作数必须显式提供；范围、键码、设备号、deadzone、scale、绑定容量等
继续由 XGE 原生校验，错误通过 checked 状态交付。

| 语言入口 | 原生入口 | 合同 |
| --- | --- | --- |
| createActions | xgeActionsCreate | 成功后接管原生对象；失败回滚 |
| actions.bind | xgeActionsBind | ActionBinding 按址作为只读输入，原生复制绑定 |
| clearBindings | xgeActionsClearBindings | 删除指定动作的绑定与状态 |
| setDeadzone | xgeActionsSetDeadzone | 原生有限数和 `[0,1)` 校验 |
| setEnabled | xgeActionsSetEnabled | 原生 `0/1`，禁用时清状态 |
| update | xgeActionsUpdate | 同步 ActionReader，省略/传 null 使用原生设备轮询 |
| state | xgeActionsGet | 按值 ActionState，保留 value/down/pressed/released |
| reset | xgeActionsResetChecked | 焦点丢失时清状态，不生成 release 边沿 |
| close / 自动清理 | xgeActionsFreeChecked | 原生拥有外壳，语言不再次 free |

`ActionReader = func(InputBinding) as c_float`。InputBinding 是本次回调期的只读视图，
保存后访问会 checked 拒绝；长期需要的数据应在回调内复制字段。读数必须为有限
`[-1,1]`。语言回调抛错时用越界值 `-16` 返回原生，确保原生拒绝提交新状态，随后
由通用同步回调边界交付原始语言错误。这里不需要新的专用回调包装层。

回调中允许查询上一帧状态；绑定修改、清空、reset、嵌套 update、原生销毁在本次
update 完成前返回 BUSY，避免使迭代地址失效。原生修复提交为
`09cf77bb130e71b96d8e647b79ddbfa710d507f9`。语言回调中关闭拥有者时，已有调用期
保护使物理销毁延迟到采样返回，之后的方法拒绝已关闭对象。线程合同仍是创建线程
串行使用，不把重入防护解释为可跨线程并发访问。

默认轮询直接调用 XGE；200 次 update/state 的专项实测没有 XRT 分配。自定义回调
仍有语言闭包、回调期视图与 checked 边界成本，本次没有宣称回调每次零分配或零开销。
绑定复制、对象创建和一次状态返回也各自保持原生/记录投影的实际成本。

专项夹具 `test/cases/foreign_xge3_input_actions.xl` 包含：浮点采样与 deadzone、
按下/释放边沿、回调视图到期、错误回滚、重入拒绝、回调内关闭、容量/索引错误、
32 个构造分配故障前缀、live count/bytes 与错误计数平衡、无分配轮询。
原生 Windows O0/O2 及 Linux O0/O2 ASan/UBSan 检查通过；语言 JIT/静态 AOT
通过，公开头通过 C11/C++17 严格编译。设备硬件/窗口输入仍需实际后端验收。

新手示例是 `lib/bindings/xge/examples/input_actions.xl`，使用确定读数，不要求真实
输入设备。小验收索引在 `dev/release-materials/provenance/xge-input-actions-20261109.json`。
当前还是隔离候选包，没有替换正式 release 或 IDE。完整编译器配套重建正在进行。
