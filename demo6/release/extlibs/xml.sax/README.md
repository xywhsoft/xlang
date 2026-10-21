# xml.sax

基于 XRT XML/Expat 的流式 XML 解析。`import xml.sax` 后可用 `reader`、
`open`、`parse`、`parseFile`；读取结果是含事件类型、名称、文本、属性和
位置信息的 `var` 对象。`Reader` 可在 `for` 中迭代，也可调用 `next()`。

默认拒绝 DTD 和外部实体，且有输入、输出、项目、节点与嵌套深度限制。
文本长度单次超过 `maxOutput`、或单个元素属性数超过 `maxEntries` 时也会
报资源限额错误，不会因无符号减法回绕而绕过限制。
遇到格式错误可读取 `errorCode/errorLine/errorColumn/errorOffset`。
这些错误位置按调用线程保存；原生错误通常映射为 `ValueError`。
C 接口和事件所有权见 `include/xrt_xml.h`。
公共 C 辅助函数 `xrtXmlNameValid` 按 XML 1.0 `Name` 规则校验 UTF-8
元素/属性名，供 DOM 与 ElementTree 的创建和属性入口共用。
XLang 显式资源限额必须为正，深度不能超过 C `unsigned int` 的范围；C 的 `xrtxmloptions.tLimits` 则沿用 XRT 公共语义，0 表示对应项目不限制。此 XML 包不解释 `tLimits.iFlags`；DTD 与外部实体由独立选项控制，非零资源策略标志返回参数错误。文件路径
含嵌入 NUL 时抛出 `ValueError`。`reader(text)` 使用带长度的缓冲区，
XML 正文由解析器按实际字节长度处理。已知长度的内存输入在分配快照
之前校验 `maxInput`；语言层也会在创建临时 `Buffer` 前比较 UTF-8 字节
长度。原生 pull 读取器持有独立快照，调用方可以随后释放或修改输入。
公共 C push 入口 `xrtXmlSaxParse` 接管 Reader 时，会把解析成功后的关闭失败
按底层错误报告为 `XRT_XML_ERROR_MEMORY` 或 `XRT_XML_ERROR_IO`；
借用 Reader 则由调用方关闭。若正文已发生
解析错误，关闭失败不覆盖原始错误。`xrtXmlSaxParseFile` 沿用该规则。
pull 入口正常读取到 EOF 时，拥有型 Reader 会在返回 `END` 前关闭；
关闭失败让最后一次 `xrtXmlSaxNext` 返回 `ERROR` 和相应的内存/I/O 错误。
借用 Reader 不会被自动关闭。提前调用返回类型为 `void` 的
`xrtXmlSaxDestroy` 仍无法报告关闭失败。提前停止时调用
`xrtXmlSaxClose` / `Reader.close()`：关闭拥有型流并使未交付事件失效，
借用流保持开放；重复 close 安全。C 返回值和 `xrtxmlerror` 可报告关闭失败，
已有解析错误优先保留，之后仍须 Destroy 释放解析器。XLang 失败沿现有
XML 错误通道抛出；正常读到 EOF 后也可调用 close。
自定义 Reader 的读取回调也遵循同一分类：报告内存故障时保留
`XRT_XML_ERROR_MEMORY`，其他流读取失败报告 `XRT_XML_ERROR_IO`。

验证：

- `xl run examples/extlibs/xml_sax.xl` 与
  `xl run examples/extlibs/xml_sax_c.c`，输出一致。
- `xl run extlibs/xml.sax/test/boundaries.xl`：UTF-8、回调、EOF、
  第二行格式错误位置与 DTD 拒绝。
- `xl run extlibs/xml.sax/test/allocations.xl`：注入公开读取器创建及
  首事件读取中的分配失败，验证 `MemoryError` 与完整结果。
- `xl run extlibs/xml.sax/test/test_xml_sax.c`
- Windows 独立 GCC `-Wall -Wextra -Werror` 与 Linux ASan/UBSan 通过。
- `bash release/extlibs/xml.sax/test/check_trees.sh` 对 SAX、DOM 和
  ElementTree 执行 Linux ASan/UBSan 回归，并需要 `release/xl` 运行三包
  XLang 边界及 C/XLang 范例逐字对照。
- Windows 可从 `demo6` 运行 `powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/xml.sax/test/check_trees.ps1`，联合检查三包严格 GCC/C 边界、中央与包内 XLang 用例、分配故障、私有入口负例及双语范例。

三个 XML 包及私有 `_expat` 依赖均在清单中声明直接 XRT 模块，内部头不再
强制启用整个 XRT。共用 C 测试宿主另外启用内存调试；同一 Windows GCC
配置下，完整 XRT 对象文件为 3,362,091 字节，按需模块对象为 428,333 字节。
此数据是测试宿主对象文件大小，不代表最终 `xl build` 产物大小。
仅含包所需模块、不含测试内存调试的 XRT 对象为 409,155 字节；
三个单文件 C 范例已在 Windows 严格 GCC 下与该对象独立链接并运行通过。

XL AOT 目前被主线缺失的 `xl6ValueDataSnapshotV1` 符号阻断，见
`demo6/dev/extlib_blockers/text_snapshot_link.md`；JIT/C 验证不代表
发布验收完成。

2026-10-15：中央 `test/cases/extlib_xml_sax.xl` 的畸形 XML 与 DTD
负例改为捕获当前原生错误映射的 `ValueError`，完整中央 JIT 与无输出
AOT 均构建运行通过。私有 `_expat` 的 Windows 随机源模块宏改为条件
定义，消除与包清单重复声明时的警告；Windows C/XL 边界及三包 Linux
ASan/UBSan 门禁复跑通过。带输出范例 AOT 仍缺上述快照符号。

2026-10-15：修正 C 内存入口先复制超限输入、随后才报限额的问题；
pull 与一次性 parse 均在创建 Reader 前校验长度、配置与平台范围。
XLang `reader(text)` 在临时缓冲区分配前检查输入字节数；输入限额及
内嵌 NUL 负例通过。Windows C/XLang 边界、严格 GCC 与
SAX/DOM/ElementTree 的 Linux ASan/UBSan 门禁通过。完整 AOT 阻断未变。
直接将 `string` 作为该原生入口的参数会暴露内部 helper 的符号可见性
问题，尚需判定是主线依赖收集缺陷还是包对私有入口的错误依赖；
故本轮保留已验证的 `Buffer` ABI，见 `dev/extlib_blockers/xml_sax_string_abi.md`。
2026-10-15：修复 C 选项中 0 限额被拒绝，与 XRT `xrtresourcelimits` 的公开语义统一；内部一次性规范化，SAX、DOM、ElementTree 共用。三个 C 入口的非空 XML 红绿回归、Windows 严格 GCC/JIT 与 Linux ASan/UBSan 通过；`plist` 的局部限额适配已移除且复验通过。完整 AOT 阻断不变。

2026-10-19：C 配置的 `tLimits.iReserved` 必须为 0。SAX push 解析与 pull 打开的 C 负例先红后绿，共用选项入口拒绝非零保留字段，仍保留限额为 0 表示不限制的合同。Windows 三包联合严格 C/XLang/双语门禁与 Linux C ASan/UBSan、XLang 联合门禁通过；完整 AOT 共享阻断不变。
同日：`tLimits.iFlags` 原被 SAX 忽略，即使传入允许外部实体的标志也不会安装实体解析器。非零标志的 C 负例先红后绿，现由共用配置入口拒绝；DOM 和 ElementTree 沿用同一校验。Windows 三包联合严格 C/XLang/双语与 Linux C ASan/UBSan、XLang 门禁复跑通过；完整 AOT 共享阻断不变。

2026-10-16：共享 Linux 门禁补 SAX/DOM/ElementTree 的 XLang 包内边界
与三组 C/XLang 范例逐字对照；三包 C ASan/UBSan 和新增语言路径
在同一脚本中通过。完整 AOT 的共享缺符号阻断不变。

2026-10-17：桥接层原将 SAX 的结构化内存错误一律发布为
`XERR_VALUE`；现按错误码及线程局部错误状态可用性保留 `XERR_MEMORY`。
公开 XLang 故障用例先红后绿，Windows 目录 smoke 和 Linux 三包
ASan/UBSan 联合门禁通过。完整 AOT 主线阻断未变。

2026-10-18：修复 C push 解析成功但拥有型 Reader 关闭失败时误报成功。
失败流 C 负例先红后绿，覆盖借用流不提前关闭、正文格式错误保留首因；
Windows C/SAX/DOM/ElementTree 边界与 Linux 三包严格 GCC、ASan/UBSan、
XLang 双语对照联合门禁通过。pull 析构仍为 `void`，不报告关闭结果；
完整 AOT 的共享阻断未变。

2026-10-18：C pull 读取原在拥有型 Reader 关闭前返回成功 `END`，随后
`void Destroy` 吞掉关闭失败。失败流先红后绿，EOF 现在关闭并把失败
报告为 `ERROR/IO`；借用流保持外部所有权，成功关闭只执行一次。
Windows C 与 XML 三包 Linux 严格 GCC/ASan/UBSan/XLang 对照联合门禁
通过。提前销毁的关闭错误仍不可由 `Destroy` 报告；完整 AOT 阻断未变。

2026-10-18：新增显式提前关闭入口。拥有型失败流、借用流、重复调用、
关闭后无后续事件及解析错误优先级由 C 测试覆盖；XLang `Reader.close()`
和 C/XLang 对照范例覆盖正常提前停止。析构仍是无返回值兜底清理；
需要获知关闭结果时须显式 close。

2026-10-18：新增 Windows XML 三包联合门禁，C/XL 边界、分配故障、私有入口负例与双语范例同次通过；Linux 严格 GCC/ASan/UBSan 联合门禁复跑通过。完整 AOT 的共享缺符号阻断不变。

2026-10-19：SAX、DOM 与 ElementTree 的 C/XLang 边界均补外部
`DOCTYPE SYSTEM file://`，以及内部子集声明并引用外部实体的负例；
默认设置下两者均明确报 DTD 错误，不把外部引用当正常 XML。
Windows 三包联合门禁及 Linux C ASan/UBSan、
XLang、双语范例联合门禁通过；这条测试不代替全部 XML 安全组合验收，
完整 AOT 的共享缺符号阻断仍在。

2026-10-20：三包共用 Windows 门禁改为每次独占构建目录，始终重编
XRT 测试对象，并在退出时只清理本次目录；旧固定目录保持不动。SAX、
DOM、ElementTree 的 C/XLang 范例除双语对照外还分别核对固定 UTF-8
黄金输出；Linux ASan/UBSan 联合门禁也使用同一组黄金文件。
Windows `check_trees.ps1 -SkipAot` 与 Linux `check_trees.sh` 全部通过。
Windows 默认门禁现在逐包构建/运行完整范例 AOT，实测在首个 SAX
构建处因共享 `xl6ValueDataSnapshotV1` 缺符号失败；DOM、ElementTree
的新增 AOT 阶段尚未执行到，不据此签收三包发布级。

2026-10-21：自定义 Reader 的读取和关闭内存故障原均被 SAX 固定报为
I/O。C 回调故障夹具先红后绿，覆盖 push、pull 和提前 close；现仅在
底层失败时读取本次 XRT 错误，保留 MEMORY，普通设备错误仍为 IO，
正文解析错误继续优先。Windows `check_trees.ps1 -SkipAot` 的三包严格
C/XLang 与双语对照、Linux `check_trees.sh` 的三包 C ASan/UBSan、
XLang 和黄金输出均通过。完整带输出 AOT 尚未重跑，不签收发布级。
