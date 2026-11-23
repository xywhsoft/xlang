# CodeEdit 外部语言服务

`getCompletionPosition()` 在 UI 线程一次读出文档版本和 UTF-8 光标字节偏移。需要单个折叠选区。语言服务完成计算后，把这两个值交给 `applyCompletionItems()`。文档版本或光标不再匹配时，原生编辑器拒绝该结果。

`applyCompletionItems()` 接收标签和插入文本，也支持显示详情、文档、过滤和排序文本、提交字符及原生标志。原生编辑器复制它需要保留的字符串，并继续负责过滤、排序和弹层。空列表沿用原生规则关闭补全并返回 checked 错误。

`showSignatureHelp()` 支持可选参数标签、说明和签名中的字节区间。活跃参数索引从 0 开始，`-1` 表示无高亮。`showHint()` 接收文档中的字节区间和提示文本。这两项使用原有的帮助弹层。

`setPlaceholders()` 可直接接受 `array<CodePlaceholder>`。`applyTextEdits()` 接受按字节偏移排序的非重叠区间和对应的替换文本，并保留原生状态及单次撤销分组。范围检查先于文本修改。批量修改中途遇到资源错误时，先前成功的替换可能留在当前编辑组。

每项操作直接调用一次原生 API。补全、参数和编辑列在调用前各创建一份临时 C 记录数组并保活输入，逐条写入字段，检查 C 文本中的内嵌 NUL。签名和悬浮描述本身在栈上构造。版本和光标读取只返回两个标量。

[运行示例](../lib/bindings/xui/examples/code_assist.xl) 会输出当前补全请求的版本和光标位置，并验证签名弹层可以打开。

## 同步提供器

`setCompletionProvider`、`setHoverProvider` 和 `setSignatureProvider` 把闭包
注册到编辑器上。传 `null` 清除对应提供器。编辑器在 UI 线程调用闭包，
传入回调期 Widget、UTF-8 字节偏移和回复对象。

补全回复的 `prefix()` 按需返回前缀文本，`remaining()` 返回剩余候选数量。
用 `add` 添加一项，或用 `appendItems` 一次添加多项。悬停及签名回复使用
`set`。编辑器通过 `showCompletion()`、`requestHoverHint(offset=-1)` 和
`requestSignatureHelp()` 消费回复。省略悬停偏移时使用光标位置。

原生回复在发布前复制临时字符串和签名参数，失败时保留上一次成功的回复。
作用域结束时释放回复数据。关闭编辑器会释放已注册闭包。回调期回复对象
离开回调后失效。回调期间修改文档会使弹层拒绝过期结果。

每次添加一个补全批次、设置一次提示或设置一次签名会分配一块原生存储。
批量补全把候选字段投影为临时 C 记录。弹层沿用现有的原生消费路径，
会再次复制它需持久保存的内容。

同一悬停弹层的 CPU 测试每次使用两段文本。旧式 C 借用回复无堆分配，C
拥有式回复分配一次，xlang 拥有式回复分配三次，其中两次用于准备 C
字符串。三条路径都在每次请求内释放各自的临时分配。
[验收记录](../dev/release-materials/provenance/xui-codeedit-provider-replies-20261120.json)

[同步补全示例](../lib/bindings/xui/examples/code_providers.xl) 注册两个候选，
可用键盘或鼠标选择。

## 鼠标停留提示

`setHoverOptions(true)` 开启自动提示，默认等待 0.35 秒。延时可以配置，
0 表示下一次更新时查询。提供器收到鼠标下文本簇的 UTF-8 字节偏移，
可以返回 `XUI_ERROR_NOT_FOUND` 跳过。已有诊断、补全和签名提示优先显示。
移动、离开、滚动、编辑或开始输入法组合会取消当前请求。提供器可以
在回调中关闭编辑器或禁用悬停，已失效的结果会被丢弃。
提供器仍由界面更新在 UI 线程同步调用。

[悬停示例](../lib/bindings/xui/examples/code_pointer_hover.xl) 演示把说明
显示在 `length` 下方。
计时测试在停留阈值前交替派发鼠标移动并更新界面。开启与关闭时的 XRT
分配、释放和重分配计数均为零。[验收记录](../dev/release-materials/provenance/xui-codeedit-pointer-hover-20261120.json)

## 自定义命令

`editor.commandMap()` 返回编辑器正在使用的键位表。可直接绑定、解除绑定、
查询键位或恢复默认键位。关闭视图会解绑观察者。关闭编辑器会使视图失效。
自定义命令 ID 从 `XUI_CODE_COMMAND_USER_BASE` 开始。

`setCommandProvider()` 注册执行回调。用 `handled.value = 1` 表示已处理，
返回 `XUI_ERROR_UNSUPPORTED` 则交给原生编辑器。`setCommandEnabledProvider()`
注册启用检查。用 `enabled.value = 0` 禁用命令，返回
`XUI_ERROR_UNSUPPORTED` 沿用默认状态。启用检查适用于快捷键、命令事件、
直接执行和内置菜单。对编辑器执行同一启用检查期间的重入会收到 Busy。

`queryCommandEnabled()` 返回 `(hasDecision, enabled)`。第一项为 false
时由原生编辑器决定。`executeCommand()` 返回是否处理，回调异常会交给调用者。
`Context.dispatchCommand()` 将命令放入事件队列，事件循环会分发它。
现有查找、补全和粘贴等专用路径继续交给编辑器处理。xlang 命令回调接收命令
ID 和回调期输出，旧的任意指针参数由 C 互操保留。

[快捷键范例](../lib/bindings/xui/examples/code_editor_commands.xl) 将 Ctrl+F2
绑定到自定义命令。

静态 AOT 基准中，启用检查和执行回调都使用 xlang 时，每次命令的中位耗时为
Windows 10.45 μs、Linux 4.52 μs。相同的 C 回调分别为 3.53 μs、2.03 μs。
预热后的计时区间内三组均未发生 XRT 堆操作。见[验收记录](../dev/release-materials/provenance/xui-codeedit-command-provider-20261120.json)。

## 编辑器折叠

`editor.foldState()` 返回编辑器正在使用的折叠模型。调用
`setRanges()` 发布范围，`toggleLine()` 切换折叠，`foldAll()` 和
`unfoldAll()` 批量操作。行号和可见行号从 0 开始，范围的起始行保持
可见，结束行包含在折叠范围内。可通过 `getRange()` 取得单个范围的值。

脚本和折叠栏操作使用同一个模型，并触发布局与绘制刷新。文档在块内
增删行时更新折叠范围。编辑触及首尾行时丢弃该范围，使陈旧的语法范围
及时失效。重复设置相同范围会保留换行缓存。编辑器用折叠版本号判断
缓存是否有效。

关闭折叠视图会解绑观察者，折叠状态继续由编辑器持有。可以重新取得
视图。关闭编辑器后，已有视图会抛出 `StateError`。批量发布使用临时
C 记录，原生端保存自己的副本。范围无效或内存分配失败时保留原状态。

[编辑器折叠范例](../lib/bindings/xui/examples/code_editor_folding.xl)

## 语言注册表与语法高亮

`editor.languageRegistry()` 返回当前编辑器的语言注册表。调用
`registerRegex()` 可注册语言 ID、扩展名、注释语法和高亮规则，再用
`editor.setLanguage(id)` 启用。扩展名可以带前导点，ID 和扩展名查找
不区分大小写。规则在同一位置命中时使用数组中靠前的规则。替换当前语言的
定义会刷新高亮缓存。

扩展名和规则分别使用数组。各数组组的数量参数为 `-1` 时使用全部元素，
且同组各列长度必须相等。非负数量选择同组各列的可用前缀。规则按原生正则
词法器的合同在词法分析时编译，正则语法错误会从该调用返回。

`get()`、`find()` 和 `findByExtension()` 返回定义的元数据。可按索引
用 `getExtension()` 和 `getRegexRule()` 获取具体值。返回的字符串是独立副本，
可在规则替换后继续使用。xlang 在注册时投影两组临时 C 记录，原生注册表
复制字符串并保留自己的定义。注册失败会保留原定义。

`createCodeLanguageRegistry()` 创建空的独立注册表，可通过 `loadDefaults()`
装入内建 C 定义。关闭编辑器注册表的视图只解绑观察者，定义继续生效。
跨线程使用与关闭编辑器后访问视图会报 checked 错误。

独立注册表和编辑器注册表都可用 `lex(id, document)` 和 `fold(id, document)`
对独立文档或编辑器文档执行已注册的语言定义。`lex` 的行号从零开始，结束行包含在内，
`-1` 表示直到文档尾。省略输出缓冲区时只返回总数。缓冲区的字节长度必须是对应
`CodeToken` 或 `CodeFoldRange` 原生记录大小的整数倍。只写入容得下的记录，
返回值仍为总数。`lex` 可选的诊断
缓冲区接收原生错误文本。失败时抛出 checked 错误，调用方应丢弃已写入的记录。
回调执行期间，清除、关闭或再次执行同一注册表会报 Busy。

`xui.tokenizeC`、`xui.tokenizeCRange` 和 `xui.tokenizeRegex` 直接处理字符串，
适合没有编辑器文档的程序。`document.tokenizeCRange` 对现有文档处理 C 源码，
保留区间起点处的词法状态，区间以 UTF-8 字节计，结束位置不包含在内。

`registerCallbacks` 可为自定义语言注册词法和折叠回调，两者可以单独使用。
注册表持有闭包，替换、清空或销毁时释放。编辑器注册表的语言定义随编辑器存活。
词法输入为文档和行区间，输出为文档内的 UTF-8 字节位置。折叠输出使用逻辑行号。
回调通过有界输出追加记录。一次调用会统计全部结果，只写入缓冲区容得下的前缀。
文档视图和输出视图只在当前调用中有效。文档被修改或关闭时，调用会拒绝结果。

[自定义词法回调范例](../lib/bindings/xui/examples/code_language_callbacks.xl) ·
[无窗口词法与折叠范例](../lib/bindings/xui/examples/code_language_tools.xl) ·
[高亮范例](../lib/bindings/xui/examples/code_editor_language.xl) ·
[验收记录](../dev/release-materials/provenance/xui-codeedit-language-registry-20261120.json)
