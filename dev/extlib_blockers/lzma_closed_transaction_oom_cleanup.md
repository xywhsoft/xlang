# LZMA 关闭态事务回滚 OOM

2026-10-30，根目录布局的固定发布物实测。打开态回调失败的回滚已经
在包内改用 `xrtFileDeleteOpen` 并通过，不能据此关闭本项 RFS。

## 复现

从仓库根目录，通过 manifest 的全部 24 个 cSources 运行
`release/extlibs/lzma/test/file_rollback_c.c --closed`：

```powershell
$m = Get-Content release/extlibs/lzma/xlang.package.json -Raw | ConvertFrom-Json
$sources = @($m.artifact.cSources | ForEach-Object { "release/extlibs/lzma/$_" })
./release/xl.exe run release/extlibs/lzma/test/file_rollback_c.c @sources --inc-path lib/xrt --inc-path lib/xruntime -- --closed
```

这是实际 native 文件 API、XRT callable 与分配注入。临时输出已处理
并关闭，用本测试的目录作为目标让重命名自然失败；末次回调安排
一次真实 OOM。无伪造 IO、外部目录或主线补丁。

Windows GCC/TCC（新旧 native 均同）：

```text
mode=0 budget=3 closed=1 result=0 armed=1 faults=1 temporary=1 first=1 original=1 balanced=1
mode=1 budget=3 closed=1 result=0 armed=1 faults=1 temporary=1 first=1 original=1 balanced=1
lzma closed rollback: 16 cases, 8 real OOM, 2 failures
```

回归进程退出 1，保留其私有现场。原目录与首错保持，七项内存快照
平衡；问题是磁盘临时文件残留，不是堆泄漏。Linux 原生 ASan/UBSan
同例为 16 组、2 次真实 OOM、0 失败，退出 0。故障点数量因平台分配
路径不同，不宣称 Linux 也触发了 UTF-16 删除故障。

## 责任与验收

冻结 XRT 的 Windows 关闭态按路径删除需要重新分配 UTF-16 缓冲。
建议由独立 XRT 线程提供创建时准备好路径/根锚点的事务接口，明确
Commit/Abort、关闭、错误首因和凭据所有权；Abort 不再依赖临时分配。
包侧继续保持处理/flush/close 全成功后才提交，不做重复重试、不吞错、
不复制 Windows IO 或同步开发中的 XRT。

稳定来源同步后，重跑本 16 组正例，要求新旧目标策略、首错和内存
检查不变且零残留；再跑完整包门禁。M2 顶层对象退役是另一条主线
问题，不随本项关闭。范围与现场见包内
`docs/file-transaction-audit-2026-10-30.md`。
