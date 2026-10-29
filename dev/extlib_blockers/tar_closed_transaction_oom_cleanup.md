# TAR 已关闭事务的低内存回滚缺口

## 状态与范围

2026-10-29，在冻结的 d64672903 发布基线与当前 TAR 包上确认。
这是 RFS 的补充风险，不是名称数组投影 M1，也不是覆盖提取改写旧
文件的问题。Windows 可能遗留临时文件，原目标保持不变；Linux 同一
探针通过。不能把它描述为 `xrtFileDelete` 违反契约：按路径删除允许
分配失败，缺的是跨平台、已准备好资源的事务回滚能力。

Reader/Writer 的错误分类和**仍打开的事务**已在包内修复：复用现有
`xrtFileDeleteOpen`，先按句柄删除，再关闭持有它的 Writer；只有
一个借用指针，不复制系统实现、不增设兼容层。该方法不能解决已经
关闭原生文件后才失败的提交。

## 最小复现

源码：`release/extlibs/tar/test/closed_transaction_oom.c`。
从 demo6 根目录构建，使用 GCC、冻结的单头 XRT 和实际 TAR 核心：

```powershell
gcc -std=c11 -O2 -Wall -Wextra -Werror -DXRT_MODULE_MEMORY_DEBUG `
  -Ilib/xrt -Irelease/extlibs/tar `
  release/extlibs/tar/test/closed_transaction_oom.c `
  release/extlibs/tar/xrt_tar_common.c `
  release/extlibs/tar/xrt_tar_reader.c `
  release/extlibs/tar/xrt_tar_writer.c `
  release/extlibs/tar/test/runtime.c `
  -o build/test/tar_closed_transaction_oom.exe -lws2_32 -lbcrypt -lshlwapi
$work = Join-Path $pwd ('build/test/tar_closed_' + [guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($work)
& ./build/test/tar_closed_transaction_oom.exe $work
```

探针在独占目录中创建目录型提交目标，写完事务后让原子改名正常
失败；此时文件已关闭、临时归档仍待处理。然后在 Destroy 入口注入
**下一次分配的一次性故障**，最后枚举临时文件并检查原目录未变。
注入不是持续拒绝所有分配。

- Windows：退出 1，`closed TAR rollback: allocated=1 temporary=1 original=1`。
- Linux C ASan/UBSan：退出 0，`allocated=0 temporary=0 original=1`。
- Windows 完整 Writer 门禁在此保持失败；它前面的十组 C 分配矩阵、
  活动分配快照、原 C/TAR 套件、XLang JIT/AOT 与清理检查均通过。

失败现场保留于 `build/test/tar_writer_65bce5966bf24aab8b2a012ac389e22b`，
含 commands.log、inputs.sha256、原目录和遗留临时归档。

## 原因与建议

TAR 在刷新、关闭成功后才调用 `xrtPathRename`，以免把未成功关闭的
归档提交为目标。改名失败后需要再按路径删除临时文件；Windows 的
`xrtFileDelete` 需把 UTF-8 路径转为 UTF-16，此处的分配失败导致
清理无法完成。Linux 的 unlink 路径不需要该分配。

建议在独立 XRT 上下文提供可准备的事务文件或提交/回滚凭据：在创建
阶段保存所需的原生路径/句柄信息，显式规定 Flush、Close、Commit、
Abort 的顺序、错误归属与可重试状态，让 Abort 的正常清理路径不再
临时分配。应由 XRT 同时供 C 和 XLang 使用；不能在 TAR 内复制 Win32
删除代码，也不能为逃避问题取消“成功关闭后提交”的约束。

## 恢复验收

同步稳定 XRT 后，先复验上面的原始探针，再运行两平台默认
`test/check_writer.ps1` / `test/check_writer.sh`，保留所有语言/AOT 检查。
同时补提交失败后重试、放弃提交的 OOM、原目标保留和无遗留临时文件。
根内提取失败原子性仍按 `tar_overwrite_failed_entry.md` 单独验收；
关闭此风险不能自动关闭 M1、回调或其他文件系统边界。
