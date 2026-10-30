> 历史工作树记录：以下结果对应文内冻结版本，不自动代表主目录当前 0.6.0；需在当前发布物上复验。

# Snappy 关闭态事务回滚 OOM

2026-10-31，冻结发布物的真实本包文件 ABI 实测。打开态 96 组已在
包内修复，不覆盖关闭后提交失败：按 UTF-8 路径删除仍需 UTF-16 分配。

## 最小复验

从仓库根目录运行，只使用测试自己的独占目录：

```powershell
./release/xl.exe run release/extlibs/snappy/test/file_rollback_c.c release/extlibs/snappy/xrt_snappy.c release/extlibs/snappy/snappy_native.c release/extlibs/snappy/third_party/xrt_snappy.c --inc-path lib/xrt --inc-path lib/xruntime -- --closed
```

两方向各 8 预算共 16 组，正常处理流，用目录目标使重命名自然失败；
在最后一次真实 XRT callable 进度调用安排一次分配失败。不替换 IO、
不伪造 FINAL 标志，也不操作测试目录之外的目标。

Windows GCC/TCC 新旧实现均退出1：

```text
snappy rollback mode=0 budget=3 closed=1 result=0 armed=1 faults=1 temporary=1 first=1 original=1 balanced=1
snappy rollback mode=1 budget=3 closed=1 result=0 armed=1 faults=1 temporary=1 first=1 original=1 balanced=1
snappy closed rollback: 16 cases, 8 real OOM, 2 failures
```

临时 `.xrt-snappy-*.tmp` 残留，原目录、首错和七项内存快照保持。
Linux 原生 ASan/UBSan 新旧均 16 组、2 次真实 OOM、0 失败；不把
未出现的 Windows 转码分配归为 Linux 已验证故障点。

## 修复责任

与现有 RFS 同类，稳定 XRT 需创建时准备好事务凭据和根锚点，明确
Commit/Abort、关闭、错误首因与所有权；关闭后 Abort 无临时分配。
包侧不复制 Windows 文件 API、不重试吞错、不提前提交、不更换开发中
XRT。稳定修复同步后再跑本正例、UTF-8/旧目标和双平台完整门禁。

M2 顶层 AOT 收尾仍独立开放。当前完整原始现场、成本和 IO 首因
回归见包内 `docs/io-file-audit-2026-10-31.md`，不能只按正文正确签收。
