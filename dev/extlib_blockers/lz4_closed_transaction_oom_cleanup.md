# LZ4 已关闭事务的二次 OOM 回滚缺口

## 状态

2026-10-29，在固定的 `d64672903` 发布版和当前包上确认。与
`tar_closed_transaction_oom_cleanup.md` 属于同一 RFS 能力缺口，但这里
使用 LZ4 自己的同步文件入口复现，不从 TAR 结果推算受影响包。

正文仍打开时的八组双故障已在包内修复：复用 `xrtFileDeleteOpen`，
先回滚再关闭 Writer，只借用文件指针。以下是**成功关闭后**的提交
失败，需要已经准备好的回滚信息；不能把仍打开时的通过当作本项修复。

## 最小复现

从 demo6 目录运行：

```powershell
./release/xl.exe run release/extlibs/lz4/test/test_lz4_commit_cleanup.c
```

该 C 文件在 TCC 单文件路径带入真实 LZ4 包源码，调用公开的
`xrtLz4CompressFile` / `xrtLz4DecompressFile`，不是替换过的系统函数。
也可与正式包对象分离链接；Windows 默认包门禁以 `-O2 -Wall -Wextra
-Werror` 编译该源并运行，不依赖 TCC 结果作唯一证据。

探针在独占目录内创建目录型目标，让提交改名正常失败；最终进度
回调返回 true，只注入本线程下一次/后续分配的一次性失败。每种方向
扫描预算0..7，记录故障是否触发、临时文件数、原目标目录和活动内存。
故障在验证前清除，不持续拒绝所有分配。测试不访问其他目录；成功
清理，失败打印并保留实际现场。

Windows C/TCC 结果：

```text
lz4 commit cleanup 6: decode=0 budget=3 allocated=1 temporary=1 original=1
lz4 commit cleanup 7: decode=1 budget=3 allocated=1 temporary=1 original=1
```

其余预算未留下临时文件；目标目录保持不变。原始最小 C/TCC 现场为
`C:\Users\ADMINI~1\AppData\Local\Temp\xlang-lz4-commit-7605dac7354cdada`。
这证明回滚资源泄漏，不是输出污染、无界内存泄漏或原目标被覆盖。
正式 GCC、Linux 及完整门禁结果以任务台账的本轮记录为准。

本轮 Windows GCC16.2.0 `-O2` 正式门禁再次在相同预算复现，76项中
73项正例和两项预期拒绝通过，仅最后的 commit-cleanup 失败；输入
无变化，现场为 `build/test/lz4_cleanup_05b8ea3979aa4c22bf4db037c3895167`。
Linux GCC15.2.0 ASan/UBSan 及 C/TCC/AOT 的八组打开态测试通过，
16组关闭态测试同样全部通过，原目标和活动内存保持；完整门禁、
514项系统库互通及外层输入复核退出0，成功快照
`/var/tmp/xlang-lz4-cleanup-src-Tx8cok` 已清理。首次Linux完整复验
只在黄金文件CRLF与实际LF的对照处失败，现场
`/var/tmp/xlang-lz4-cleanup-src-AMgFLW` 保留；两组C新矩阵此前已过。

## 原因与修复方向

`xrt_lz4_file.c` 在流处理、最终回调、flush/close 成功后才执行
`xrtPathRename`；这是正确顺序，不能为了方便按句柄删除而提前提交。
改名失败后原文件已关闭，后续 `xrtFileDelete` 在 Windows 需要将
UTF-8 路径转为 UTF-16，这次分配失败便无法删除临时文件。

已核对冻结单头的公开文件 API，没有可准备并跨越 Close 的文件事务
Commit/Abort 凭据；`xrtFileDelete` 也没有承诺无分配。应由独立 XRT
上下文提供统一的事务文件或回滚凭据，在创建阶段准备必要原生状态，
明确 Flush、Close、Commit、Abort 的所有权、首因、失败和重试规则，
使正常 Abort 不再临时分配。C 与 XLang 共同复用，不在每个包复制
系统路径实现、增加另一套所有权、重试一次删除来掩盖持续低内存，
也不更新本工作树到开发中的 XRT。

默认 Windows 完整门禁保留本项失败；此前所有单点 OOM/正常事务
通过记录不覆盖这个双故障。该发现把 LZ4 从 G 转 R；RFS 涉及包数
由 TAR/ZIP 两包增加为三包，不按压缩/解压或预算重复计包。其他压缩
包有相似清理代码只是待审查线索，尚未逐包证明，不能直接计为同根。

## 恢复验收

稳定 XRT 同步后先重跑上述16组原始探针，再运行两平台完整门禁：

```powershell
./tools/check_extlib_lz4.ps1 -Interop
```

```sh
bash tools/check_extlib_lz4.sh
```

同时保留 `test_lz4_cleanup.c` 的八组回调/回滚双故障、原有文件单点
OOM（本轮Windows78、Linux60）、流状态/取消/首因、线程、系统liblz4互通、语言 JIT/AOT
和双语范例。跨平台均须检查原目标不变、无临时文件、活动内存平衡，
不删除或跳过失败探针以恢复绿灯。
