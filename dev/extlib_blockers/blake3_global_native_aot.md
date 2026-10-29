# 顶层原生 hasher 的 AOT 退出状态错误

2026-10-27 复验：`xl.exe` SHA-256 为
`CCF98FF05BAAA71C1827EAA0C8B7A011A9A17EAB1AECD593CD89848D227C5A2B`，
`xl.dll` 为 `7D07E84B3FD99C74F226D5A221CD94BE402AFC1091F15E9FDEE4FD019F2B11D9`，
`xlc.dll` 为 `2F3DAA80FF68FA0E6C42CEEA39F7205573559CDDF3BCF4F5A7F96FD55A09F655`。
最小样例 `xl run` 退出 0；`xl build` 成功，独立 EXE 输出相同摘要后
仍报 `RuntimeError: operation is not valid in the current state` 并退出 1。
阻断未解除，未修改扩展包绕行。

状态：blocked-upstream。普通扩展库任务不修改 XLang 主线。

## 最小复现

在 `demo6` 目录运行：

```text
release/xl.exe run release/extlibs/blake3/test/aot_global_hasher_repro.xl
release/xl.exe build release/extlibs/blake3/test/aot_global_hasher_repro.xl -o build/blake3_global_repro.exe
build/blake3_global_repro.exe
```

源程序只在顶层创建 hasher、追加 `abc` 并打印摘要。2026-10-24 的发布版
`release/xl.exe` SHA-256 为
`CCF98FF05BAAA71C1827EAA0C8B7A011A9A17EAB1AECD593CD89848D227C5A2B`。
`xl run` 输出正确摘要并退出 0；`xl build` 成功，但生成的程序输出相同摘要后
又报 `uncaught xlang error: RuntimeError: operation is not valid in the current state`，
退出 1。该错误在用户代码完成后出现，不是摘要计算失败。

对照：把 hasher 放在显式 `func main(...) as int` 的三组中央用例和两份
教学范例中，完整 Windows 包门禁的 JIT/AOT 都通过。本次只证明顶层原生
对象触发差异，尚未证明所有 class 或所有扩展包均受影响。

## 修复与验收

请主线检查 AOT 模块顶层执行、原生对象析构和运行时错误状态交接，
确定是否在退出清理时留下/传播了状态错误。先以本文件的最小复现要求
JIT/AOT 输出相同、退出码均为 0，再复跑 BLAKE3 完整门禁。
教学范例使用显式 `main` 是可执行程序的正常写法，不是对此缺陷的修复；
不能删除复现或据此宣称顶层写法已稳定。

2026-10-24 BLAKE2 独立复测：顶层使用 `Blake2bHasher`/`Blake2sHasher`
的完整教学范例也在 AOT 输出完成后报相同状态错误；改为显式 `main`
后，BLAKE2 默认门禁 48 项通过。这表明问题不只影响 BLAKE3，
但仍不能据此推断所有 native-backed class 均受影响。
