> 历史工作树记录：以下结果对应文内冻结版本，不自动代表主目录当前 0.6.0；需在当前发布物上复验。

# XML 公共类尚不能安全导出

状态：blocked-upstream。普通扩展库任务不修改 XLang 主线。

当前发布版从仓库根目录运行以下三个最小文件的 `xl check`，均非零退出，
报告 `[import] import package has no such exported type`：

- `dev/extlib_blockers/xml_sax_reader_type.xl`：`xml.sax.Reader`
- `dev/extlib_blockers/xml_dom_node_type.xl`：`xml.dom.Node`
- `dev/extlib_blockers/xml_etree_element_type.xl`：`xml.etree.Element`

三包目前通过工厂返回对象，调用方可用 `auto` 接收并操作；但不能在
包外声明这些类型的变量、参数或返回值。这不是解析器无法处理对象，
而是包源码的类未导出。直接加 `export` 不是安全修复：DOM 与
ElementTree 有接管原生 `point` 的构造路径，SAX 的 Reader 也由
原生句柄构造；导出后必须阻止调用方传入任意指针。已登记的
`sharedmem_private_native_constructor.md` 表明当前主线对导出类
`private extern create(point)` 的访问控制尚不可靠。

主线应先修复导出类私有原生构造器的可见性与重载解析，并提供
“包内可从原生句柄构造、包外不可调用”的稳定合同。随后在三个包
分别导出 Reader/Node/Element，保留公开工厂，确保以上三个文件
`xl check` 通过；另增加包外构造任意 `point` 的编译负例，只检查
诊断，不能运行伪造句柄。最后重跑双平台 `check_trees` 默认门禁。

2026-10-26：现行 Windows `check_trees.ps1` 与 Linux
`check_trees.sh` 已通过严格 C、Linux ASan/UBSan、语言 JIT、
边界/分配/中央/双语范例 AOT 和 DOM/ElementTree 往返；这些
运行证据不覆盖上述类型声明与构造器安全性，因此三包仍待签收。

2026-10-30：八个冻结发布物/单头库仍未改变。三份最小源在 Windows/
Linux 原生文件系统均再次返回 1，仍为同一缺少导出类型诊断。联合
默认门禁最后保留 SAX 这个正确的公开类型正例，因此当前默认门禁
为红，不再只凭工厂运行通过而退出 0。之前的私有方法负例照常验证，
它们与原生构造器访问控制不是同一能力，不能混同签收。
本轮修复仅是包内冷启动错误状态，详见
[共同审查记录](../../release/extlibs/xml.sax/test/BRIDGE_STATE_AUDIT_2026-10-30.md)。
