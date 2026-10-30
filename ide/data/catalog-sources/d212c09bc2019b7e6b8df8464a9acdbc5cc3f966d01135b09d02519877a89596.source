# xml.etree

2026-10-26 当前 Windows/Linux 三包联合默认门禁均退出 0：严格 C、
Linux ASan/UBSan、中央/边界/分配 JIT 与 AOT、公开 C/XLang 范例
独立 EXE 同输出、DOM/ElementTree 往返均通过。旧 VFS 头路径与
共享快照缺符号不再复现。`Element` 仍不能在包外命名；导出前须
收口原生指针构造器的访问控制，见
`../../../dev/extlib_blockers/xml_public_class_visibility.md`。
这项公开类型缺口修复前不签收完整包。

2026-10-25 当前发布物入口复验：中央 ElementTree 用例从
`demo6` 根目录运行时，在包代码执行前因共享 TCC VFS 缺少
`include/xruntime_decl.h` 退出；从 `release` 目录运行通过。
本轮未重跑三包联合默认门禁及 AOT，下方 2026-10-24 的通过记录
只证明当时发布物。见
`../../../dev/extlib_blockers/tcc_vfs_runtime_decl_include.md`；
不在 ElementTree 包内增加头文件路径兼容层。
同轮 WSL 三包联合门禁通过 C ASan/UBSan、Linux XLang JIT 边界与
分配故障及双语黄金输出；使用现有 `release/xl`，未验新版 Linux AOT。

2026-10-24 当前发布物复验：Windows 三包联合默认门禁通过严格 C、
XLang 边界/分配、私有入口负例、DOM/ElementTree 往返与固定双语输出，
以及三包各自的边界、分配和范例 AOT；Linux 联合门禁通过 C ASan/UBSan、
XLang JIT 与双语对照。旧共享缺符号不再阻断 Windows AOT；Linux 独立
EXE 本轮未验收。下文较早的 AOT 阻断均指当时的发布物。

`stringify()` 通过 `xetree_stringify_text` 接管 XRT 写出的 UTF-8 缓冲，
避免再由 `Buffer.text()` 复制整份结果。C 侧使用该文本桥接函数后
以 `xrtFree` 释放返回值；原有 `xetree_stringify` 仍返回独立的 `xbuffer*`。
补齐终止字节时，缓冲容量不足仍可能扩容。

基于 XRT XML/Expat 的轻量 ElementTree。`import xml.etree` 后可用
`create`、`parse`、`load` 和 `Element` 的属性、子节点、路径查询、序列化接口。
路径查询支持直接子元素和 `//name` 后代匹配；不是完整 XPath。

`Element` 持有原生引用；`child`、`find`、`findAll` 返回的句柄由各自的
`Element` 实例释放。`parse` 默认限制输入、输出、元素及嵌套深度，DTD 与
外部实体被拒绝。解析错误通过 XRT 原生错误通道呈现为 `ValueError`；
创建/写入失败可能呈现为 `XmlError`。C 接口见 `include/xrt_xml_etree.h`。
C 侧 `xrtXmlElementFindAll` 返回的数组装有非拥有型指针值，但每个指针
对应一份已保留的元素引用；调用方须逐个 `xrtXmlElementRelease`，再
`xrtValueRelease` 数组。包内 C 用例展示了该释放顺序。
已知长度的内存输入在创建 Reader 和树节点前验证平台范围与输入限额；
XLang `parse(text)` 先检查 UTF-8 字节长度，再将只读字符串及显式长度
直接传给同步解析入口，不创建整份输入的临时 `Buffer`；解析后的树独立持有节点内容。
解析限额必须为正，深度不能超过 C `unsigned int` 的范围；写出缩进
仅允许 0..16。名称、属性、文本、查询路径及文件路径通过 C 字符串
传递，含嵌入 NUL 时会抛出 `ValueError`，不会静默截断。
元素/属性名按 XML 1.0 `Name` 检查，`create` 对非法名称抛 `XmlError`，
不会返回空句柄对象。写出拒绝无效 UTF-8、非法 XML 字符及不合法的
注释/处理指令；属性空白与文本回车用字符引用保留往返值。
`save` 在序列化成功后才原子替换文件，失败时旧文件保持不变。
保存及流写出遇到底层内存失败时，C 层报告 `XRT_XML_ERROR_MEMORY`，
XLang `save()` 抛 `MemoryError`；真实写入和关闭失败仍按 IO 报告。
C 的 `xrtXmlElementWrite` 空 Writer、`xrtXmlElementSave` 空路径在序列化前
报告 `XRT_XML_ERROR_ARGUMENT`，不会把无效参数误报为 I/O 故障。

验证：

- `xl run examples/extlibs/xml_etree.xl` 与
  `xl run examples/extlibs/xml_etree_c.c`，两者输出一致。
- `xl run extlibs/xml.etree/test/boundaries.xl`
- `xl run extlibs/xml.etree/test/allocations.xl`：逐点注入属性分配失败，
  验证 `MemoryError`、树状态不变及清除故障后重试。
- `xl run extlibs/xml.etree/test/test_xml_etree.c`
- `xl check extlibs/xml.etree/test/constructor_access_negative.xl`、
  `xl check extlibs/xml.etree/test/private_native_negative.xl` 和
  `xl check extlibs/xml.etree/test/private_pointer_negative.xl` 均应失败，
  验证原生构造器、C 字符串方法和返回裸指针的原生方法不能由导入方绕过。
- Windows 独立 GCC `-Wall -Wextra -Werror` 与 Linux ASan/UBSan 包级测试通过。
- Linux 可运行 `bash release/extlibs/xml.sax/test/check_trees.sh`，
  同时验证 DOM 与 ElementTree 的 C 写出边界、三包 XLang 边界及
  C/XLang 范例逐字对照；需要 `release/xl`。
- Windows 可从 `demo6` 运行 `powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/xml.sax/test/check_trees.ps1`，联合验证三包 C/XL 边界、分配故障、私有入口负例和双语范例。
- 联合门禁另运行 `test/cases/extlib_xml_tree_interop.xl`，双向检查 DOM 与
  ElementTree 的序列化/解析结果。混合内容中的前导文本和子节点尾文本
  在 ElementTree 分别位于 `text`、`tail`，DOM 则保留独立文本子节点。

过去的 XL AOT 缺符号复现及当时建议见
`dev/extlib_blockers/text_snapshot_link.md`。当前 Windows 包级
AOT 结果以上述现行门禁为准，不能外推为 Linux 独立 EXE 验收。
本包和 `xml.sax`/`_expat` 依赖现声明实际 XRT 模块，内部实现头不再
强制启用 `XRT_MODULE_ALL`；Windows 裁剪 C 宿主和 Linux sanitizer 回归已通过。

2026-10-21：保存故障红测复现原子写入第 8 个分配点误报 IO。
现于保存、流写入及关闭边界保留内存错误，成功序列化后清除陈旧错误，
关闭不覆盖先前的写入失败。C 保存逐点扫描、内存 writer/真实关闭 IO、
XLang `Element.save()` 故障和恢复通过；Windows XML 三包联合门禁及
Linux ASan/UBSan、XLang/双语联合门禁均通过。完整 AOT 共享阻断未变。

2026-10-15：修复 `ParseMemory(uint64 size)` 先窄化长度并分配 Reader/树、
再判断输入限额的问题。C 分配故障与极大长度负例先红后绿；Windows C/XL
边界、双语范例、严格 GCC 和 XML 三包 Linux ASan/UBSan 门禁通过。
完整 AOT 的共享缺符号阻断未变。
2026-10-15：C `xrtxmloptions.tLimits` 的 0 限额沿用 XRT 公共语义，表示不限制；XLang 显式限额仍要求正数。非空 XML 解析回归与 SAX/DOM 共用入口同步通过，Windows C/XL 边界和 Linux ASan/UBSan 通过；完整 AOT 阻断不变。

2026-10-17：新增属性在多个字符串分配中途失败时，原有部分分配未计入
属性数，释放树也不会回收，导致泄漏。C 故障注入先复现活动分配增长，
现改为完整构造后一次提交，失败则释放暂存内容；C/XL 用例验证原树不变、
`MemoryError` 分类和可重试性。Windows C/XL 与 Linux 三包 ASan/UBSan
联合门禁通过；完整 AOT 的共享缺符号阻断未变。
同日补解析入口的分配故障回归：桥接层原把原生内存错误发布为
`XERR_VALUE`，现保留 `XERR_MEMORY`；`parse` 在 XLang 中抛
`MemoryError`，正常格式错误仍按原合同处理。

2026-10-18：C 解析入口接管 Reader 时，SAX 层的关闭失败现在向上传递为
错误，不会返回未确认输入关闭成功的元素树。该轮失败关闭流报
`XRT_XML_ERROR_IO`；底层内存故障现按 `XRT_XML_ERROR_MEMORY` 保留。失败关闭流
负例、Windows C 边界和三包 Linux 严格 GCC/ASan/UBSan/XLang 对照通过；
完整 AOT 共享阻断未变。

2026-10-18：复审发现 `nameNative()` 等九个原生查询方法仍可由导入方
直接访问，其中裸 `point` 结果会绕过字符串所有权包装，`childNative()`
还会绕过索引检查。编译负例先确认可以访问，再将九个方法设为私有；
原有两条负例和新指针负例均接入 XML 三包联合门禁。Linux C 严格 GCC、
ASan/UBSan、XLang 边界与双语输出对照通过；完整 AOT 主线阻断未变。

2026-10-18：解析深度可由调用方提高，原树释放、后代查询、`findAll` 和
写出均递归消耗 C 栈。现用无分配后序遍历释放树，查询和写出使用显式
遍历栈；8192 层解析、查询、序列化、释放先异常退出后通过。
`findAll` 中途分配失败会撤销已保留的全部引用，多匹配逐点故障注入
检查无泄漏。写出缓冲/栈分配失败报告 `XRT_XML_ERROR_MEMORY`；漂亮
打印低输出限额和深树写出逐点故障也进入 C 回归。Windows C 与 XML
三包 Linux GCC/ASan/UBSan/XLang 联合门禁通过；完整 AOT 共享阻断未变。

2026-10-18：Windows 三包联合门禁与 Linux ASan/UBSan 联合门禁均通过；ElementTree 的 C/XL 边界、分配故障、未导出构造器/私有入口负例及双语同输出在同次 Windows 门禁核对。完整 AOT 仍待主线缺符号修复。

2026-10-19：C/XLang 新增外部 `DOCTYPE SYSTEM file://` 和内部子集
声明并引用外部实体两类负例，默认配置必须报 DTD 错误；SAX、DOM
同步固定此合同。
Windows 三包严格 C/XLang 和 Linux C ASan/UBSan、XLang、双语范例
联合门禁通过。完整 AOT 的共享缺符号阻断未解除。

2026-10-20：三包共用门禁为 ElementTree 范例加入固定 UTF-8 黄金
输出，Windows 快速模式与 Linux ASan/UBSan 联合门禁通过。Windows
构建目录改为每次独占并清理；默认门禁还要执行三包完整范例 AOT，
但当前先在 SAX 范例链接处缺共享 `xl6ValueDataSnapshotV1`，
ElementTree AOT 尚未执行到，不据此签收发布级。

2026-10-21：`Load(path)` 的 Reader 打开阶段发生 XRT 分配失败时原
固定报 XML IO。复用已有文件夹具，C 注入用例先红后绿；现保留
`XRT_XML_ERROR_MEMORY`，清除故障后同一文件可正常加载。
Windows 三包 `check_trees.ps1 -SkipAot` 与 Linux 三包 C ASan/UBSan、
XLang 和黄金输出门禁通过。完整带输出 AOT 未签收。
