# CSV 关闭态事务的 OOM 回滚缺口

2026-10-30，CSV 自己的真实文件入口确认 RFS：Windows 目标改名失败后，
Writer 所有的文件句柄已关闭，析构删除临时文件需再次分配 UTF-16 路径；
此处 OOM 留下 `.xrt-csv-*.tmp`。原目录及标记文件保持，首错和活动内存保持。
不从其他压缩包的类似实现推断本包结果，也不修改冻结 XRT。

## 复现

从仓库根目录运行：

```powershell
powershell -NoProfile -File tools/run_extlib_c.ps1 -Package csv -Source release/extlibs/csv/test/file_rollback_closed.c
```

测试只创建自己的独占目录和带标记文件的目录型目标；使用 CSV Writer 正常
写入，再让 Close 的改名自然失败。验证内部流已释放后，只对随后 Destroy
安排一次分配失败。它不替换系统调用，不改变正常数据或虚构 IO 返回值。
故障在验证前清除；无残留要求返回 0，失败返回 1 并保留私有现场，配置/内存
检查错误返回 2。默认 Windows/Linux 综合门禁都独立执行，仍继续后续语言边界。

Windows 最小探针已经得到：

```text
closed transaction: first-oom=1 second-oom=1 temporary-remains=1 original=1
```

这份探针先使改名路径 OOM，再使关闭态回滚 OOM；正式回归使用自然改名失败，
缩小为 IO 首错加清理分配失败。正式平台、退出码和现场位置见发布验收矩阵及台账。
冻结 XRT SHA256：`049C180A43597D5476F5357CC9A7D1603B439765A3E3550437F7D49B5B3E55F4`。

## 修复责任

与 TAR/LZ4/bz2/Brotli 的 RFS 同类：由独立 XRT 上下文提供创建时准备的
事务/回滚凭据，使 Abort 在 Flush/Close 后也无需临时分配。需明确 Commit、
Abort、失败重试、句柄与原生路径所有权，并同时保持 UTF-8 路径与首错。
本任务不更新开发中 XRT，不复制 Windows 文件 API，不通过重复删除重试
或提前提交来掩盖缺口。稳定来源受控同步后，CSV 再接入同一能力并复跑双平台。

## 本轮验证

正式 Windows GCC16.2 回归返回 1：

```text
csv closed rollback: cleanup-oom=1 temporary=1 original=1
```

现场在 `build/test/csv_audit_1793320836319/windows-final/csv-closed-4da09f92b16a4745`。
同次默认门禁 88 阶段继续完成其他边界，56 项指纹在执行前后变化为零。
Linux 原生快照 `/var/tmp/xlang-csv-final-src-uLgSv6` 的同一真实回归在
ASan/UBSan/泄漏检查下返回 0，`cleanup-oom=0 temporary=0 original=1`；
无路径转换分配，不宣称 Linux 也触发了 OOM。59 项门禁输入、60 份复制源及
原件在运行前后不变，现场 `tmp/xlang-csv-qIwZjg`。
内存、非法释放计数与 IO 首错身份的检查均通过，不将临时残留误报成堆泄漏。

M2、M3、M5 是独立语言阻断，不能因 RFS 修复而一并关闭。
