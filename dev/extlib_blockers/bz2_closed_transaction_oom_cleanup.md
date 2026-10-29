# BZ2 已关闭事务的二次 OOM 回滚缺口

## 复现与范围

2026-10-29，在固定 `d64672903` 发布基线确认。与TAR、LZ4共享
RFS能力缺口，但本项实际调用BZ2的公开文件API，不由相似代码推断。
正文仍打开时的八组回调/回滚双故障已在包内修复；下面仅指成功
关闭后才发生提交失败的路径，不能用打开态通过来关闭此风险。

从完整 demo6 源码树执行：

```powershell
./release/xl.exe run release/extlibs/bz2/test/test_bz2_commit_cleanup.c
```

该开发测试共用 `test/support/extlib_codec_file_rollback.h`，TCC路径
带入正式的bzip2后端与BZ2核心/文件实现，调用
`xrtBz2CompressFile` / `xrtBz2DecompressFile`；不替换OS或产品函数。
无参数时生成独占目录，成功清理，失败报告并保留现场。

探针使用目录型目标，让完整写入、最终通知和关闭后的改名正常
失败；最终进度回调仍返回true，只对本线程注入一次性分配故障。
每种方向扫描预算0..7；故障在读取结果前清除，检查返回失败、错误
存在、原目录、临时文件及活动内存平衡。Windows C/TCC实际输出：

```text
bz2 commit cleanup 6: decode=0 budget=3 allocated=1 temporary=1 original=1 kind=10
bz2 commit cleanup 7: decode=1 budget=3 allocated=1 temporary=1 original=1 kind=10
```

其余预算无临时文件。最初C/TCC现场为
`C:\Users\ADMINI~1\AppData\Local\Temp\xlang-bz2-rollback-17646c1938bed69c`。
这是磁盘临时文件遗留，不是原目标被覆盖或不受控的堆泄漏。

## 原因与建议

文件层正确地在flush/close成功后才执行改名；此时已无可供
`xrtFileDeleteOpen`使用的原生文件。Windows按路径删除还需要UTF-8
到UTF-16的分配，提交失败后的该次分配遇OOM便无法完成回滚。
现有按路径删除没有无分配承诺，冻结XRT也没有可跨越Close的预备
事务凭据；不在普通包复制Win32接口、保留另一套文件所有者、重复
删除一次或取消成功关闭后再提交的约束。

独立XRT上下文应统一提供可准备的事务/回滚能力，明确所有权及
Flush、Close、Commit、Abort的首因、失败和重试规则，使正常Abort
不临时分配。稳定版本同步后，各C和XLang包装共同使用，再恢复
本包两平台的完整默认验收。

本项把BZ2由G转R；RFS实际涉及TAR、LZ4、BZ2、ZIP四包，不按
预算或编解码方向重复计数。其余压缩包相似代码仍只是待核对线索。

## 必须保留的验收

- 上述原始16组关闭态探针，不能删去或改为预期通过。
- `test_bz2_cleanup.c`的八组打开态双故障，检查首因、旧目标、零
  清理分配与零临时文件，C/TCC、严格GCC、独立EXE都须覆盖。
- 旧构造/流/文件单点OOM、取消/异常/重入、CRC/截断/串联成员、
  C与XLang的完整JIT/AOT、Python标准bz2互通及双语黄金输出。

默认入口为 `tools/check_extlib_bz2.ps1` 和
`tools/check_extlib_bz2.sh`；Windows默认门禁保留关闭态红项。
本轮正式门禁与平台现场结果补记在任务台账，不把TCC单例代替
整个发布验收。

## 本轮正式验证

Windows GCC16.2.0的默认 `-O2` 完整门禁77项，仅最后
commit-cleanup未达到预期；74项正例和两项预期私有访问拒绝通过，
输入无变化。正式GCC同样只在预算3的两个方向遗留临时文件，活动
内存平衡和原目标保持。报告、命令、原始两条输出流与现场在
`build/test/bz2_cleanup_c95e6c58a8e54038a5f4dbfc44546825/results.json`。

Linux GCC15.2.0原生文件系统快照
`/var/tmp/xlang-bz2-cleanup-src-9omFAV` 的默认完整门禁与外层输入
复核均退出0，成功后清理。包括C ASan/UBSan、24组双故障、56个旧
文件分配失败点、269项Python bz2互通、四种头组合、所有语言及
公开双语JIT/AOT；Linux正常路径删除没有Windows转码分配。

没有修改优化参数、C/XLang行为、bzip2上游、XRT或主线；Linux的
黄金文本仅规范化期望文件CRLF行尾，实际输出仍逐字比较。
