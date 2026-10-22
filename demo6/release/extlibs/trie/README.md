# trie

前缀字符串集合。`Trie` 提供插入、精确包含、前缀存在、前缀词数、删除、清空和总词数；重复插入不增加计数。空字符串可作为一个键，空前缀始终存在，`countPrefix("")` 返回总词数。

```xlang
import trie

trie.Trie words = trie.create()
words.insert("你好")
words.insert("你们")
println(words.countPrefix("你"))  // 2
```

XLang 使用字符串的完整字节长度，内嵌 NUL 不会截断。底层按 UTF-8 原始字节建立树，不做 Unicode 归一化、大小写折叠或词形处理；同一对象的并发写入需要调用方同步。查询和删除不分配新节点，插入失败时不改变已有键和计数。查找约为 O(键字节数 × log 分支数)，每节点最多 256 个分支。长键的删除/销毁不依赖递归 C 栈。

`trie.create()` 在底层分配失败时抛出 `MemoryError`，不会返回带空原生句柄的实例。
`insert` 的原生分配失败同样抛 `MemoryError`，旧键、前缀词数与总数保持不变。
`Trie` 是公开导出类型，可用 `trie.Trie` 显式声明变量；推荐通过
`trie.create()` 构造，以保留上述失败检查。

`insertNative` 等原生入口仅供类内部使用。包外通过公开方法传入字符串，包装层计算真实字节长度；`test/private_native.xl` 是预期编译失败的负例，不执行其中长度不匹配的调用。

## C 接口

`include/trie_native.h` 的所有查询和修改函数接收明确的 `size_t` 字节长度，可存储任意字节串，包括 NUL。`xtrie_create` 返回的句柄由调用方用 `xtrie_destroy` 释放；`xtrie_clear` 保留句柄供复用。插入失败返回 `false` 并设置 XRT 错误。输入缓冲在调用返回后即可释放，树保存自己的节点结构。
`countPrefix` 与 `count` 的 C 返回值为 `int64_t`，与 XLang `int` 的 LP64 ABI 一致。

公开 C 操作会清除本线程上一次的 XRT 错误；无效句柄或输入缓冲设置本次错误，合法键未命中则返回 `false`/`0` 且不留错误。`xtrie_count(NULL)` 保持返回 `0`，但会设置参数错误，便于区分空树计数。`xtrie_destroy` 仅释放资源，不覆盖调用方已有诊断。

## 验证

Windows 聚焦快测：`powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/trie/test/check.ps1 -SkipAot`。它同次检查 C 独立键集合模型、包外 `insertNative` 访问拒绝、XLang 边界及 C/XLang 固定范例输出。去掉 `-SkipAot` 后，默认还构建运行无输出核心和公开范例 AOT，后者须匹配同一黄金输出；构建使用单次独占目录并在失败后清理。当前快测和核心 AOT 通过，完整范例仍缺共享 `xl6ValueDataSnapshotV1`，整包不签收发布级。

从 `demo6` 目录运行：

```powershell
.\release\xl.exe run test\cases\extlib_trie.xl
.\release\xl.exe run release\extlibs\trie\test\boundaries.xl
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_extlib_c.ps1 -Package trie -Source release\extlibs\trie\test\test_trie.c
WSL bash release/extlibs/trie/test/check.sh
.\release\xl.exe build test\cases\extlib_trie.xl -o build\test\trie_guard.exe
.\build\test\trie_guard.exe
.\release\xl.exe run release\examples\extlibs\trie.xl
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_extlib_c.ps1 -Package trie -Source release\examples\extlibs\trie_c.c
```

C 测试覆盖全部 256 个首字节、30,000 字节深键、内嵌 NUL、UTF-8、删除/清空，以及分配故障注入后的原树不变；创建的首次和第二次分配失败均返回空句柄，清除故障后可正常创建。另以确定性 800 步交错插入、删除和清空，对照独立键集合逐项核验精确包含、所有前缀的存在性和词数。包内 Linux 门禁用 ASan/UBSan 运行同一测试。XLang 常规用例、双语范例及无输出 AOT 用例通过。带输出范例的 AOT 构建仍受共享 `xl6ValueDataSnapshotV1` 缺符号阻断，见 `demo6/dev/extlib_blockers/text_snapshot_link.md`。

2026-10-15：补全 C API 失败后成功的连续调用回归，修复合法未命中仍保留旧错误的问题；清单显式声明无可选 XRT 模块。中央无输出 AOT 用例构建运行通过，产物由 3,759,616 字节降至 768,000 字节；C/XLang 用例、双语同输出及 Linux ASan/UBSan 门禁通过。带输出完整 AOT 仍缺共享快照符号，不据此签收发布级验收。

2026-10-16：修复文档已使用但未导出的 `trie.Trie` 公开类型；包内新增
空键、共享前缀逐步删除、内嵌 NUL/UTF-8 键及清空复用边界测试，
并登记到 `coverage.tests`。当前发布物的中央与包内 XLang 用例、
C 测试和双语同输出范例通过；完整 AOT 阻断仍按原记录。

同日补 LP64 ABI：Linux XLang 原在 `countPrefix` 的 `long long` 声明处导入失败；两个计数入口统一为 `int64_t`，C 范例使用标准格式化宏并加入头文件契约检查。完整 AOT 阻断不变。

2026-10-17：新增包内 XLang 创建和插入逐点分配故障测试，并登记到
`coverage.tests`；Windows/Linux 均验证 `MemoryError`、旧键与计数保持、
故障解除后可继续使用。Linux `test/check.sh` 合并 C ASan/UBSan、
中央及包内 XLang 用例和双语范例逐字输出对照，全部通过。
完整带输出 AOT 仍受共享主线缺符号阻断。

2026-10-19：新增 Windows 单入口 `powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/trie/test/check.ps1`，整合严格 GCC/ABI、800 步 C 独立键集合模型、中央与包内 XLang 边界/分配故障，以及包运行器 C、单文件 C、XLang 三条范例固定输出对照。Linux `test/check.sh` 同步加入固定输出检查；两平台门禁通过，Linux 含 ASan/UBSan。Trie 实现未改，完整带输出 AOT 的共享缺符号阻断不变，不签收整包发布级。
