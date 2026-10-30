> 历史工作树记录：以下结果对应文内冻结版本，不自动代表主目录当前 0.6.0；需在当前发布物上复验。

# Zstd 关闭态事务回滚 OOM

2026-10-31，冻结发布物的真实 native 文件 ABI 实测。打开态96组已
在包内修复，但关闭后 rename 失败，按 UTF-8 路径删除仍需新分配。

## 最小复验

仓库根目录，仅操作夹具自己的独占临时目录：

```powershell
./release/xl.exe run release/extlibs/zstd/test/file_rollback_c.c release/extlibs/zstd/xrt_zstd.c release/extlibs/zstd/zstd_native.c release/extlibs/zstd/third_party/zstd.c --inc-path lib/xrt --inc-path lib/xruntime -- --closed
```

两方向各8预算，共16组。正式 XRT callable 的最后一次成功进度
回调安排一次分配失败；用目录目标使 rename 自然失败。不替换 IO，
不伪造 FINAL 标志。期望失败后旧目标不变、首错不变、零临时文件、
七项内存快照平衡。

Windows GCC/TCC新旧均退出1：

```text
zstd rollback mode=0 budget=3 closed=1 result=0 armed=1 faults=1 temporary=1 first=1 original=1 balanced=1
zstd rollback mode=1 budget=3 closed=1 result=0 armed=1 faults=1 temporary=1 first=1 original=1 balanced=1
zstd closed rollback: 16 cases, 8 real OOM, 2 failures
```

每方向遗留 `.xrt-zstd-*.tmp`；目标目录、首错与内存平衡保持。
Linux ASan/UBSan新旧同16组、2次真实 OOM、0失败；不把 Windows
专属转码分配算作 Linux 已验故障点。

## 修复方向与恢复

与 RFS 同类，由稳定 XRT 提供创建时预备的事务凭据与根锚点，明确
Commit/Abort、关闭、首因和所有权，保证关闭后 Abort无需临时分配。
包内不重写平台文件操作、不重试吞错、不提前提交、不替换开发中XRT。
同步稳定修复后重跑本正例、打开态、UTF-8/旧目标及双平台完整门禁。

包内 `docs/io-file-audit-2026-10-31.md` 记录旧红/新绿、普通17条AOT、
原始现场及成本；两平台 M2全局AOT退出问题仍独立开放，不因正文
正确或错误分类正确而关闭。普通队列任务不修改主线。
