# xml.dom

基于 Expat 的 XML 文档树。XLang 使用 `import xml.dom`，C 使用
`include/xrt_xml_dom.h`；解析、节点树和写出共用同一套实现。

```xlang
import xml.dom

auto document = xml.dom.parse("<root id='one'><child>你好</child></root>")
auto root = document.documentElement
assert root.getAttribute("id") == "one"
assert root.child(0).textContent == "你好"
print(document.stringify(true, 2, true))
```

`Node` 提供节点类型、名称、命名空间、属性、父节点、子节点、文本内容、
`appendChild`、`removeChild` 和创建元素/文本/注释/处理指令；文档可解析、
创建、序列化、加载或保存。缺失属性可指定默认值。解析与写出接口都有
输入/输出、单项、条目、节点及深度上限，默认值见 `src/dom.xl`。
已知长度的内存输入在创建 Reader 和文档树前验证平台范围与 `maxInput`；
XLang `parse(text)` 先检查 UTF-8 字节长度，再将只读字符串及显式长度
直接传给同步解析入口，不创建整份输入的临时 `Buffer`；解析后的树独立持有节点内容。
这些限额必须为正；深度不能超过 C `unsigned int` 的范围。序列化
缩进仅允许 0..16，输出限额必须为正。名称、属性、文本及文件路径通过
C 字符串传递，含嵌入 NUL 时抛出 `ValueError`，不会静默截断。
元素/属性名按 XML 1.0 `Name` 检查；序列化拒绝无效 UTF-8、非法 XML
字符与注释/处理指令的非法写法。属性空白和文本回车使用字符引用，
写出再解析保持原值。C 层格式错误返回 `XRT_XML_ERROR_FORMAT`，输出
限额返回 `XRT_XML_ERROR_LIMIT`。`save` 先序列化，再用 XRT 原子写入；
格式错误或超限不会覆盖已有文件。
保存与流写出遇到底层内存失败时，C 层报告 `XRT_XML_ERROR_MEMORY`，
XLang `save()` 抛出 `MemoryError`；真实写入/关闭失败仍报告 IO。
C 的 `xrtXmlDomWrite` 空 Writer、`xrtXmlDomSave` 空路径在序列化前
返回 `XRT_XML_ERROR_ARGUMENT`，不会把无效参数误报为 I/O 故障。

C API 的节点引用由调用方 `xrtXmlDomNodeRelease`；`Child`、`Parent`、
`DocumentElement` 返回保留过的引用。`xrtXmlDomStringify` 的结果用
`xrtBufferDestroy` 释放。XLang `Node` 负责对应的生命周期。

安全边界：SAX 基座禁止 DTD 与外部实体，DOM 继承该策略；包级 C/XL
测试均覆盖 DTD 拒绝。XML 输入不应作为拥有文件或网络访问权限的实体解析器。

## 定点验证

在 `demo6/release` 目录运行：

```powershell
.\xl.exe run extlibs/xml.dom/test/test_xml_dom.c
.\xl.exe run extlibs/xml.dom/test/boundaries.xl
.\xl.exe run extlibs/xml.dom/test/allocations.xl
.\xl.exe run examples/extlibs/xml_dom.xl
.\xl.exe run examples/extlibs/xml_dom_c.c
```

`xl check extlibs/xml.dom/test/private_native_negative.xl` 和
`xl check extlibs/xml.dom/test/private_pointer_negative.xl` 均应失败：前者
验证文本入口不可绕过校验，后者验证不能把任意 `point` 传给节点操作。
`Node` 的其余 `*Native` 成员也仅供类内部使用，原生结果由公开方法接管。
Linux 可从 `demo6` 运行
`bash release/extlibs/xml.sax/test/check_trees.sh`，对 DOM/ElementTree
共用的 XRT/Expat 实现运行严格 GCC 与 ASan/UBSan；脚本还需要
`release/xl` 运行三包 XLang 边界与 C/XLang 范例逐字对照。
Windows 对应入口为 `powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/xml.sax/test/check_trees.ps1`（从 `demo6` 目录运行）；同时检查三包的 C/XL 边界、分配故障、私有入口负例及双语范例。

两个示例的 XML 内容在归一化换行后相同；Windows C 标准输出使用 CRLF，XLang 使用 LF。Windows 严格 GCC、Linux ASan/UBSan、XL JIT 与
上述包级测试通过。**AOT 发布验收仍被主线缺少
`xl6ValueDataSnapshotV1` 阻断**，需在主线线程修复后重跑。
本包和 `xml.sax`/`_expat` 依赖现声明实际 XRT 模块，内部实现头不再
强制启用 `XRT_MODULE_ALL`；Windows 裁剪 C 宿主和 Linux sanitizer 回归已通过。

2026-10-21：定点故障注入先复现 `save` 原子写入第 4 个分配点把内存失败
误报 IO。现于写出边界保留 XRT 内存错误，并在成功序列化后清除陈旧错误；
流写入与关闭同样区分内存和 IO，关闭不覆盖先前的写入失败。
C 保存逐点扫描、流故障/陈旧错误和 XLang `document.save()` 分配故障
均通过；Windows 三包联合门禁及 Linux ASan/UBSan、XLang/双语联合门禁通过。
完整 AOT 共享缺符号阻断未变。

2026-10-15：修复 `ParseMemory(uint64 size)` 先窄化长度并分配 Reader/文档树、
再判断输入限额的问题。C 分配故障与极大长度负例先红后绿；Windows C/XL
边界、双语范例、严格 GCC 和 XML 三包 Linux ASan/UBSan 门禁通过。
完整 AOT 的共享缺符号阻断未变。

2026-10-18：再次审计发现 `appendChildNative(point)` 等十五个原生方法
仍公开，包外可绕过 `Node` 类型约束或直接拿到待接管的指针；编译负例先
确认可达，现统一设为私有。Windows DOM C/XL 与 Linux XML 三包
ASan/UBSan、XLang/双语门禁通过；完整 AOT 阻断不变。
2026-10-17：C 解析入口新增逐分配点故障扫描，覆盖命名空间、属性、
文本与注释，确认失败时整棵未完成的树释放且随后可重试。包侧桥接原将
`XRT_XML_ERROR_MEMORY` 一律发布为 `XERR_VALUE`，现保留 `XERR_MEMORY`；
XLang 故障用例核对 `MemoryError` 和完整结果。Windows C/XL 与 Linux
三包 ASan/UBSan 联合门禁通过；完整 AOT 共享阻断未变。
2026-10-15：C `xrtxmloptions.tLimits` 的 0 限额沿用 XRT 公共语义，表示不限制；XLang 显式限额仍要求正数。非空 XML 解析回归与 SAX/ElementTree 共用入口同步通过，Windows C/XL 边界和 Linux ASan/UBSan 通过；完整 AOT 阻断不变。

2026-10-18：C 解析入口接管 Reader 时，SAX 层的关闭失败现在向上传递为
错误，不会返回未确认输入关闭成功的文档树。该轮失败关闭流报
`XRT_XML_ERROR_IO`；底层内存故障现按 `XRT_XML_ERROR_MEMORY` 保留。失败关闭流
负例、Windows C 边界和三包 Linux 严格 GCC/ASan/UBSan/XLang 对照通过；
完整 AOT 共享阻断未变。

2026-10-18：解析深度可由调用方提高，`textContent` 和序列化因此改用
显式遍历栈，不再随树深消耗 C 调用栈。8192 层 XML 的解析、文本读取和
逐字节序列化先红后绿；漂亮打印的低输出限额、逐分配点故障与无泄漏也
进入 C 回归。写出缓冲或遍历栈分配失败明确报告 `XRT_XML_ERROR_MEMORY`。
Windows C 测试和三包 Linux GCC/ASan/UBSan/XLang 联合门禁通过；完整
AOT 共享阻断未变。

2026-10-18：Windows 三包联合门禁和 Linux ASan/UBSan 联合门禁均复跑通过；DOM 的 C/XL 边界、分配故障、私有原生入口负例和 C/XL 范例同输出已在同次 Windows 门禁核对。完整 AOT 仍待共享符号修复。

2026-10-19：C/XLang 新增外部 `DOCTYPE SYSTEM file://` 和内部子集
声明并引用外部实体两类负例，默认配置必须报 DTD 错误；SAX、
ElementTree 同步固定此合同。
Windows 三包严格 C/XLang 和 Linux C ASan/UBSan、XLang、双语范例
联合门禁通过。完整 AOT 的共享缺符号阻断未解除。

2026-10-20：三包共用门禁为 DOM 范例加入固定 UTF-8 黄金输出，
Windows 快速模式与 Linux ASan/UBSan 联合门禁通过。Windows 构建目录
改为每次独占并清理；默认门禁还要执行三包完整范例 AOT，但当前先在
SAX 范例链接处缺共享 `xl6ValueDataSnapshotV1`，DOM AOT 尚未执行到。
这不是 DOM 的发布级签收。

2026-10-21：`Load(path)` 的 Reader 打开阶段发生 XRT 分配失败时原
固定报 XML IO。复用已有文件夹具，C 注入用例先红后绿；现保留
`XRT_XML_ERROR_MEMORY`，清除故障后同一文件可正常加载。
Windows 三包 `check_trees.ps1 -SkipAot` 与 Linux 三包 C ASan/UBSan、
XLang 和黄金输出门禁通过。完整带输出 AOT 未签收。
