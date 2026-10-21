# slugify

`slugify` 把标题转成适合 URL 路径段的人类可读文本。ASCII 字母转小写，常见分隔符合并为 `-`，`&` / `@` 分别作为独立词转成 `and` / `at`；例如 `a&b` 得到 `a-and-b`。包内保留有限的拉丁预组字符转写表（例如 `é→e`、`ü→u`），忽略组合重音符；汉字、假名和韩文原样保留。全角 ASCII 字母和数字转半角。**这不是完整的 Unicode 规范化、通用转写或 URL 编码器**；未覆盖的其他文字和 emoji 当前丢弃。不同原文可能产生同一 slug，持久链接需要由应用追加稳定 ID 等判重信息。

## API

- `slugify.slugify(text)`：返回独立结果；空输入或只有符号的输入返回空串。
- `slugify.isAscii(text)`：仅判断原始文本是否所有字节均小于 128；并不判断它是否是合法 slug。

输入必须是有效 UTF-8，最多 1 MiB，不能含内嵌 NUL。非法输入抛 `ValueError`，不再静默截断或输出畸形 UTF-8。普通 ASCII 标点中引号被丢弃，空格、连字符、下划线、点、斜线及常见标点作为词边界。该函数用于生成展示性标识，**不是文件路径净化或安全校验**；用于 HTTP URL 时仍应由 URL 层做正确编码。

生成结果再次交给 `slugify` 不会改变文本；分隔符会合并，结果没有首尾 `-`。这项幂等性不解决不同标题碰撞的问题。

## C 接口与所有权

公开声明在 `include/slugify_native.h`。C API 接受显式字节长度，`xsl_slugify` 返回 XRT 独立分配的 NUL 结尾文本，调用方用 `xrtFree`；失败返回 `NULL`。`xsl_is_ascii` 返回 `1`/`0`/`-1`（参数错误），与 XLang `c_int` 桥接一致。结果无共享缓冲，不同线程可独立调用。UTF-8 解码与输出缓冲复用 XRT。

两个公开 C 入口在调用开始时清理上一次的 XRT 线程错误；成功返回时 `xrtGetError()` 为 `NULL`，失败后可直接重试或调用另一入口。

## 范例与验证

2026-10-22：有限拉丁转写表的 287 项映射保持不变，按码点升序排列并改用
二分查找，避免长段重音文本逐字符扫描整张表。C/XLang 新增表头、表中和
表尾映射的固定回归；Windows 严格 C、线程、XLang 与双语快测，以及 Linux
C ASan/UBSan、线程、XLang 联合门禁通过。完整带输出 AOT 的共享缺符号
阻断仍在，不签收整包发布级。

- XLang：`release/examples/extlibs/slugify.xl`
- 同输出 C：`release/extlibs/slugify/examples/slugify_c.c`

在 `demo6/release` 可直接运行 `xl run extlibs/slugify/examples/slugify_c.c`；
普通 C 编译仍按包清单单独链接 `slugify_native.c`。
- 边界：包内 `test/test_slugify.c`、`test/test_threads.c`、`test/boundaries.xl`，以及 `demo6/test/cases/extlib_slugify.xl`

在 `demo6` 运行 `powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/slugify/test/check.ps1`；Linux 运行 `bash release/extlibs/slugify/test/check.sh`。两份脚本均覆盖 C 边界、线程、中央与包内 XLang 用例及 C/XLang 范例逐字节同输出；Windows 默认还构建运行核心与完整范例 AOT，`-SkipAot` 只做 C/JIT 快测。Windows 另做严格 GCC 语法检查，Linux 的 C 测试启用 ASan/UBSan。测试覆盖长 ASCII/汉字输入、畸形 UTF-8、内嵌 NUL、独立结果和并发。

2026-10-12 已通过 Windows 严格 GCC、Linux ASan/UBSan、C/XL 边界与线程测试、双语同输出范例及 `test/cases/extlib_slugify.xl` 无输出核心 AOT。`xl build extlibs/slugify/test/boundaries.xl -o ../build/test/slugify_boundaries.exe` 当前因共享链接缺少 `xl6ValueDataSnapshotV1` 而失败，完整 AOT 尚未验收；见 `dev/extlib_blockers/text_snapshot_link.md`。

2026-10-14 补输入恰好 1 MiB 成功、超 1 字节拒绝的 C/XLang 边界，以及 C 动态输出缓冲分配失败后恢复测试。C/XLang 边界、C 并发与双语范例通过；上述完整 AOT 缺符号已用当前 `xl.exe` 复现。

2026-10-14 补失败→成功跨入口的错误槽红绿回归，新增包内 `test/check.sh` Linux ASan/UBSan 门禁；Windows 严格 GCC、C/XL 边界、线程与双语范例、Linux sanitizer 均通过。完整 AOT 阻断不变。
2026-10-15 包内门禁增加按 manifest 传入 buffer/Unicode 模块宏的严格 GCC 编译检查；原生源码条件定义这两个模块，避免重定义错误。输出与公开 API 未变。
2026-10-18 补 Windows 包级联合门禁，并将既有线程、中央/包内 XLang 和 C/XLang 同输出用例纳入 Linux 包级脚本；两平台均实跑通过。此门禁不包含当前受共享文本快照缺符号阻断的完整 AOT，不据此签收发布形态。

2026-10-19 新增 ASCII、重音字符、CJK、全角标点、emoji 与组合重音的 81 组 C 输入组合，验证二次转写幂等及分隔符规整；XLang 增加混合文本幂等断言。Windows 包级严格 C/XLang、Linux C ASan/UBSan、线程与双语门禁通过。实现未改，完整 AOT 共享阻断不变。

2026-10-20：Windows 默认包级门禁加入核心与完整带输出范例 AOT，范例结果与同次 C 输出对照；AOT 产物仅保存在本次独占测试目录。`-SkipAot` 保留 C/JIT 快测，快测通过不表示发布级 AOT 已签收。当前构建的共享缺符号阻断仍需在主线修复后复验。

2026-10-21：`isAscii` 原遇到畸形的非 ASCII 字节时直接返回 `false`，
与本包要求有效 UTF-8 的输入合同不一致。C 负例先红后绿；现在纯 ASCII
输入仍单次扫描，非 ASCII 输入经 XRT 校验，非法序列返回 `-1` 并设置
`XERR_VALUE`。Windows `test/check.ps1 -SkipAot` 的严格 C、C/XLang
与双语门禁，以及 Linux C ASan/UBSan、线程、XLang 联合门禁通过。
本轮未重跑完整 AOT，共享链接阻断仍未解除。

2026-10-21：`&` / `@` 原先把替换词直接拼在相邻文本上，
`a&b` 变成 `aandb`。C 负例先复现，再将两个符号作为独立词输出；
现在 `a&b` 为 `a-and-b`，`user@example.com` 为
`user-at-example-com`。中央与包内 XLang 断言已同步；Windows 严格
C/线程/双语快测和 Linux C ASan/UBSan、线程、XLang 联合门禁通过。
完整 AOT 仍受共享 `xl6ValueDataSnapshotV1` 缺符号阻断。
