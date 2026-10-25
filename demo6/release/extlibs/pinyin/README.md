# pinyin

轻量 UTF-8 拼音转写扩展库。只收录约 400 个常用汉字的固定读音；不分词、不推断多音字、不标声调。未收录的字符（含生僻汉字与 emoji）保持原样，适合小型演示和固定词表场景，不适合姓名、地名或全文检索的准确读音判断。

```xlang
import pinyin
println(pinyin.convert("中国"))   // zhong guo
println(pinyin.initials("中国"))  // zg
```

`convert(text)` 在相邻已映射汉字间插入空格，也会在 ASCII 单词与已映射汉字之间分词；标点原样保留。`initials(text)` 仅将已映射汉字替换为首字母，不额外插入空格。两者遇到嵌入 NUL 或非法 UTF-8 均失败。

例如当前字表未收录“界”，因此 `convert("你好世界")` 为 `ni hao shi 界`。这是有意保留原字，不是完整拼音解析的结果。

C API 见 `include/pinyin_native.h`。`xpinyin_convert[_n]` 与 `xpinyin_initials[_n]` 返回独立 XRT 字符串，调用方须用 `xrtFree` 释放；`_n` 版本显式接收字节长度。每次调用先清理本线程上一次的 XRT 错误，成功返回后不会残留旧错误。

## 范例与验证

2026-10-25：C 边界测试补齐孤立续字节、截断的三/四字节字符、
非法续字节和超出 Unicode 范围的输入；两条显式长度入口均须报
`XERR_VALUE`，随后正常调用须恢复。Windows 严格 C 用例、Linux
C ASan/UBSan 与 XLang 包门禁通过；当前 Windows 发布版从 `release`
目录直跑 XLang 边界、分配故障和中文范例通过。本次未重跑 Windows
全量 AOT 门禁，转换规则和字表均未改动。

2026-10-24：更新后的 Windows 发布版执行默认 `test/check.ps1`，
严格 GCC、C/XLang 边界、两条入口的分配故障、双语固定中文输出，
以及无输出核心和带输出范例 AOT 均通过。旧的共享快照缺符号记录
属于先前发布物；本次未重跑 Linux sanitizer，字表覆盖范围也未扩大。

- `release/examples/extlibs/pinyin.xl` 与 `pinyin_c.c` 可直接 `xl run`，输出完全一致。
- `release/extlibs/pinyin/test/test_pinyin.c` 覆盖长输入、结果所有权、非法 UTF-8、嵌入 NUL 和未知字符。
- `release/extlibs/pinyin/test/boundaries.xl` 覆盖 XLang 桥接及异常；`demo6/test/cases/extlib_pinyin.xl` 是原有核心用例。
- `release/extlibs/pinyin/test/allocations.xl` 逐点检查两条 XLang 入口在分配失败时抛 `MemoryError`，解除故障后可正常转换。

Linux 可从 `demo6` 目录运行 `bash release/extlibs/pinyin/test/check.sh`。
脚本同次执行 C ASan/UBSan、中央与两组包内 XLang 用例，并核对 C/XLang 中文范例输出。
Windows 可运行 `powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/pinyin/test/check.ps1 -SkipAot`，一次执行严格 GCC 宏检查、C 边界、中央与两组包内 XLang 用例，并将包运行器 C、`xl run` 直跑 C、XLang 范例分别与包内固定中文输出比较；去掉 `-SkipAot` 还会执行完整 AOT 门禁。

2026-10-12：Windows 严格 GCC、Linux ASan/UBSan、C/XL 边界、双语同输出范例及完整无输出边界 AOT 均通过。带输出范例 AOT 仍因共享 `xl6ValueDataSnapshotV1` 未链接而失败，发布验收未完成，见 `demo6/dev/extlib_blockers/text_snapshot_link.md`。

2026-10-14：检查 395 项字表顺序，移除末尾不参与映射且破坏排序约定的哨兵；补 `convert` / `initials` 动态输出分配失败与恢复测试。Windows 严格 GCC、C/XLang 边界和双语范例、包内 Linux ASan/UBSan 门禁通过。映射范围与输出保持不变。

2026-10-14：补非法输入失败后立即成功的 C 错误状态回归，先红后绿；修复只涉及公开调用的线程错误边界，不改变字表或输出。Windows 严格 GCC、C/XLang 边界、双语同输出、Linux ASan/UBSan 及当前发布版的无输出边界 AOT 构建/运行均通过；带输出 AOT 阻断未变。

2026-10-15：按原生代码的 buffer 与 UTF-8 使用声明 `XRT_MODULE_BUFFER` / `XRT_MODULE_UNICODE`。同一核心 AOT 从 3,573,248 字节降至 582,144 字节，裁剪前后均运行通过；Windows C/XLang 边界、C/XLang 双语范例和 Linux ASan/UBSan 复跑通过。带输出 AOT 的共享快照符号阻断不变。
同日补严格 GCC 的 manifest 宏编译路径：原生源码条件定义所需模块，避免重复定义告警；Linux 包门禁已固化该检查。

2026-10-18：XLang 教学范例改为直接使用 UTF-8 中文字面量；Linux 包门禁
补齐中央/包内 XLang 用例和 C/XLang 输出对照。C ASan/UBSan 与双语门禁
同次通过。带输出 AOT 的共享主线阻断不变。

2026-10-19：`convert` / `initials` 将 UTF-8 与嵌入 NUL 检查并入逐字符解码，
去掉有效输入上的两次整段预扫描。C 边界补非法尾部、中途 NUL、两种模式
报错及失败后恢复；Windows C/XLang 定向用例、Linux C ASan/UBSan、
XLang 与双语范例门禁通过。输出及错误合同不变，完整 AOT 阻断未变。

2026-10-19：新增 Windows 单入口门禁与固定中文范例预期，当前整组通过；拼音算法未改，Linux ASan/UBSan 本轮未重跑。带输出 AOT 的共享缺符号阻断不变，不签收整包发布级。

2026-10-20：Windows 门禁增加 `-SkipAot` 快测，默认在三条范例
固定输出后构建/运行无输出核心和带输出范例 AOT。实测快测通过、
核心 AOT 通过；完整范例构建因共享 `xl6ValueDataSnapshotV1` 缺符号
失败，独占构建目录无残留。此前只测 C/JIT 的默认全绿不能作为
发布级签收；固定小字表的公开范围未扩大。
