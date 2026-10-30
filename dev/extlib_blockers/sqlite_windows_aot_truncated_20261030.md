# SQLite Windows AOT 产物被截断（待定位）

## 范围与现场

2026-10-30，根目录布局的 codex/extlibs-release-sync 工作树，编译器源码
冻结基线 d64672903；没有合并最新主线、修改 XRT 或 SQLite vendor。

本次两个借用型列出口的首错修复是独立的确定性包错误，不作为下述
产物截断的根因解释。Linux 历史无诊断退出也不能直接认定为同一问题。

Windows 默认完整门禁在 file-jit、file-build 均退出0后，file-aot
无法启动，系统报告不是有效的应用程序。该文件并未开始执行，不能
归因于 SQL、中文路径读写或对象析构。原始报告与产物保留在：

- build/test/sqlite_guard/results_dbaaafb10af74f5ca2081161f3900baa.json
- build/test/sqlite_guard/run_dbaaafb10af74f5ca2081161f3900baa/file.exe
- build/test/sqlite_audit_1793325761094/windows-full.json

失败输入和工具前后指纹均未变化，编译器为冻结的 release/xl.exe
及 release/xlc.dll。门禁没有隐式重试，也没有将启动失败视为成功。

## 已确认的事实

1. 输入为原有 release/extlibs/sqlite/test/file.xl，JIT输出
   sqlite file/locking boundaries ok，退出0。
2. xl build 返回0，双流为空，但 file.exe 只有1,232,896字节。
   SHA256为 E5F0B5425CF19AC836D4329BB36FF32DA1BE3FD75C64DCBF8E119EDDC0BAC403。
3. PE头为有效的x86-64格式，.text声明大小0x289ec8，
   .rdata原始偏移0x28a400，.pdata原始偏移0x2b1a00。
   这些区段超出实际文件长度，objdump明确报告区段无法读取；
   不是“机器不支持64位”。
4. 随后使用同一输入、发布物、工作目录，另一个独占文件名构建，
   输出2,907,136字节，启动0并打印同一成功文本。
   此次成功只能说明并非稳定的输入必现，不能解释先前为何截断。
5. 观察时C盘剩余约1.15GB，无法回溯失败瞬间的磁盘空间或外部写入。
   不据此认定磁盘满、杀毒软件、并发或TCC本身为根因。

## 复现与下一步

从仓库根目录运行默认门禁即可保持原始边界：

    powershell -ExecutionPolicy Bypass -File tools/check_extlib_sqlite.ps1

若只定位产物发布链路，在新的独占输出目录执行：

    release/xl.exe build release/extlibs/sqlite/test/file.xl -o build/test/独占目录/file.exe

每次都记录原始状态、双流、文件长度/哈希与PE区段范围，再启动产物；
不能覆盖上次失败文件后再推定其内容。不要添加自动重试掩盖失败。

主线侧建议审核：输出文件的逐次write/flush/close错误是否向上传播，
短写是否被完整处理，失败时是否仍返回0/保留半成品；如涉及复制或
VFS物化，整个出口也应纳入相同检查，成功才发布完整产物。
这些是检查方向，并非已经确认的根因。本扩展库任务不修改编译器、
运行时或系统文件API，不在SQLite包内添加PE补丁。

## 验收口径

本记录是待定位R风险，不扩大成所有AOT构建必然失败，也不凭一次
隔离重建成功关闭。与[历史Linux门禁退出](sqlite_linux_silent_gate_20261028.md)
一并保留，后续只有取得可解释的根因/环境边界后再签收SQLite完整发布。
