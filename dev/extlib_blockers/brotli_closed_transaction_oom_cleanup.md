# Brotli 已关闭事务的二次 OOM 回滚缺口

## 状态与复现

2026-10-29，新增普通 C 文件接口时，用 Brotli 自己的压缩/解压入口
确认 RFS 第五个实测包，不从 LZ4/bz2 的相似代码推断。Windows 留下
临时文件，原目录型目标和活动内存保持；同组 Linux 16 场景全通过。
冻结 XRT SHA256 为
`049C180A43597D5476F5357CC9A7D1603B439765A3E3550437F7D49B5B3E55F4`，
编译器、XRT/xruntime 和三件套均未改。

从 demo6 运行默认门禁，或单独复验：

```powershell
powershell -NoProfile -File tools/run_extlib_c.ps1 -Package brotli -Source release/extlibs/brotli/test/file_rollback_closed.c
```

测试复用 `test/support/extlib_codec_file_rollback.h` 的断言，调用真实
文件 API，不替换系统函数。在独占目录创建目录型目标，让提交改名
自然失败；FINAL 回调返回 true，仅安排一次后续分配失败，预算 0..7，
压缩/解压各一轮。故障在验证前清除，失败现场保留。

Windows GCC16.2 `-O2 -DNDEBUG -Wall -Wextra -Werror` 结果：

```text
brotli commit cleanup 6: decode=0 budget=3 allocated=1 temporary=1 original=1 kind=10
brotli commit cleanup 7: decode=1 budget=3 allocated=1 temporary=1 original=1 kind=10
```

其余预算无残留。日志和两份临时文件在
`build/test/brotli_guard/run_229980438e324b5e93f483776cf64e29/closed-files`。
旁边 `results.json` 的完整 99 阶段为 95 正例成功、两项预期私有访问
拒绝符合要求、本项及 M2 两项失败，152 项指纹变动为零。脚本在本项
失败后仍检查 M2，不让一个红项掩盖另一个。

Linux 原生快照 `/var/tmp/xlang-brotli-cfiles-src-2QWlTU`，日志
`tmp/xlang-brotli-YaxnsU/rollback-closed.log` 的 16 组通过。144 份源件
复制/执行后与原位置一致，门禁另含 GCC 共 145 项指纹亦不变。
完整门禁最后在已有 M2 全局 AOT 退役退出 1，而不是本项。

## 原因与修复责任

新增 `xrt_brotli_file.c` 在正文/回调失败、文件仍打开时调用
`xrtFileDeleteOpen`，仅借用 Writer 所拥有的句柄；首因通过
`xrtSetErrorTake` 移交，不复制错误。八组打开态双故障在两平台均保持
原目标、首因和内存，安排故障后不再分配，临时文件为零。

这不代表关闭态已经修复。提交必须先完成刷新及 Reader/Writer 关闭，
以支持原地变换并报告关闭错误。改名失败后文件已经关闭，后续
`xrtFileDelete` 在 Windows 还需为 UTF-8 转 UTF-16 分配，此次失败便
无法删除临时文件。不能提前改名、重复删除一次、继续使用失效句柄，
也不在每个包复制系统路径实现。

由独立 XRT 上下文提供预备事务/回滚凭据：创建时准备必要原生状态，
跨 Flush/Close 后仍能无临时分配 Abort，明确 Commit/Abort 的所有权、
首因及失败重试。稳定来源受控同步后，包层接入同一能力；本任务不
更新开发中 XRT、不临时扩张共享运行库。

恢复时先验 16 组关闭态与八组打开态，再跑两平台完整门禁，保留普通
C 回调、最终取消、首因、目标竞态、UTF-8、原地变换、空文件、OOM、
双语及 JIT/AOT。M2 必须独立修复，不能因此自动重新签收整个包。
