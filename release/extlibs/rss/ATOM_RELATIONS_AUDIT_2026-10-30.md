# Atom 链接关系与资源边界审查

## 修复

`rss_parse.c` 的 `rss_atom_link` 原只把缺省 `rel` 与短名称 `alternate`
作为网页链接，遗漏了规范等价的完整 IANA 关系 IRI。若 `self` 在前，
完整 IRI 的 alternate 在后，会返回订阅自身地址；若后续还有短名称
alternate，又会错误跳过前一个本应同等优先的链接。

依据 [RFC 4287 §4.2.7.2](https://www.rfc-editor.org/rfc/rfc4287#section-4.2.7.2)，
增加完整常量字符串的精确比较。feed/entry 共用同一路径，不新增 API、
ABI、复制、分配或链接缓存；没有测试吞吐，不能声称性能成本为零。
不做任意 IRI 归一化、不把 HTTPS、尾斜杠、不同路径大小写或空白猜成
同一关系。仍按既有约定取第一个 alternate，无此项则取第一个 href。
空 href 仍作为存在的 IRI 引用保留，不改变宽容解析边界。

额外核对了 DOM 属性的借用期限与 `xrtValueNull`：属性借用期间文档仍
存活，输出字符串会复制到拥有型 XRT 值；空值是不可变进程期单例，
不会另行分配并覆盖前一条错误。这两点未发现需修改的问题。

## 测试与范例

- `test/test_link_relations.c`：20 组数据 × 默认/前缀命名空间 ×
  parse/parseAtom，共 80 份解析，每份同时验证 feed/entry 链接。
  覆盖完整/短名称的顺序、实体解码、缺省 rel、缺少/空 href、外来元素、
  非直接子节点、限定属性，以及不应误认的近似 IRI。
- 同时验证实体文本/CDATA，释放源输入后再读结果。逐次比较活动分配、
  活动字节、非法释放、重复释放和 UAF 计数；两模式扫描至成功，
  各平台共 668 个真实 OOM，保留 `XERR_MEMORY`，清理后恢复成功。
- `test/link_relations.xl`：前缀/外来命名空间、feed/entry、源字符串
  替换后的结果寿命及逐预算 MemoryError/恢复；JIT 和独立程序通过。
- `examples/atom_links.xl` / `atom_links_c.c` 及固定三行黄金输出；
  C 的 GCC、TCC JIT/AOT 与 XLang JIT/AOT 相同，原作者继承例也保留。
- 六条真实生成的 C 声明与公开头文件联合严格编译；错改 parse 返回
  类型必须以类型冲突拒绝，不把任意编译失败算作正确 ABI 验证。

## 实测结果

| 平台 | 本轮结果 |
| --- | --- |
| Windows | 默认 PS5 门禁 56 项，前 55 项符合预期（含一个必须拒绝的错误 ABI）；第 56 项原中央 XLang 字典生成失败。122 个输入/工具指纹不变。 |
| Linux 原生文件系统 | ASan/UBSan C、相同 JIT/AOT、七份独立程序与双语黄金输出通过；随后原中央语言字典生成失败。120 个复制源和三项工具前后相等。 |
| 旧源对照 | 同一最终 C 测试在修复前 Windows TCC 和 Linux sanitizer 各 18 条断言失败，修复后为 0；这是一处选择错误，不是 18 个独立 BUG。 |
| 门禁失败控制 | Windows 真实静默 GCC 退出 37，报告保留 raw37、外层失败 1；Linux 静默工具 37 和不存在工具 127 都保留首因和现场。 |
| M1 独立复现 | 两平台 JIT/AOT 均先完成原生解析对象的生成往返，再在同形语言字典/数组处 ValueError、退出 1；不改写原正例。 |

Windows 现场：`build/test/rss_relations_audit_1793360315/`；默认门禁报告：
`build/test/rss_guard_5e3420148cd44dd6ace7f5d839581b05.report.json`。
Linux：`/var/tmp/xlang-rss-relations-audit-eJt8zzQ8`，包含完整命令、
原始输出、复制源/工具哈希和失败现场；不是另一份活动开发目录。

运行与编译使用冻结的现有编译器，未构建、合并或修改 xlang 主线、
TCC、XML 依赖及 XRT。完整门禁依然失败，RSS 仍为 M1，不能签收整包。
主线修复共享 foreign 容器投影后，必须重跑原有生成例和默认门禁。
