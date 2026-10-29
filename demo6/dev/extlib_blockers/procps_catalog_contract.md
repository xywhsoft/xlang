# procps 中央用例的旧接口漂移（已修复）

2026-10-17，在 `demo6` 目录执行：

```powershell
release/xl.exe run dev/extlib_blockers/procps_catalog_contract.xl
release/xl.exe run test/cases/extlib_procps.xl
```

当时 `test/cases/extlib_procps.xl` 调用 `procps.listPids()`；两行最小
复现也在导入阶段报 `import package has no such exported symbol`。公开包接口已统一为
`procps.pids() as array<int>`，包内 `test/boundaries.xl` 和双语范例均
按此接口验证当前进程出现在快照中。旧字符串接口不应恢复。

2026-10-18 已将中央用例改为对 `pids()` 整数数组的当前 PID 成员断言，
并纳入 Linux 包级门禁。旧字符串接口不恢复。目录 smoke 当前只登记
包内 `boundaries.xl`，因此其 1/1 不代表中央用例由目录清单运行；
中央用例由包级脚本另行执行。
