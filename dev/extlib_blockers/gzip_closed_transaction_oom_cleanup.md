> 历史工作树记录：以下结果对应文内冻结版本，不自动代表主目录当前 0.6.0；需在当前发布物上复验。

# Gzip 关闭态事务回滚 OOM

2026-10-31，固定发布物的正式native文件ABI实测。本包打开态回滚
96组已修，但关闭后提交失败仍依赖UTF-8路径删除的新分配。

## 最小复现

从仓库根目录运行，只操作夹具自己的独占目录：

```powershell
./release/xl.exe run release/extlibs/gzip/test/file_rollback_c.c release/extlibs/gzip/xrt_gzip.c release/extlibs/gzip/gzip_native.c --inc-path lib/xrt --inc-path lib/xruntime -- --closed
```

真实XRT callable末次成功回调安排一次分配失败，用目录目标使提交
自然失败；不替换文件IO、不伪造FINAL。不允许遗留临时文件、替换
原目标或产生内存违规。

Windows GCC/TCC旧新均退出1：

```text
gzip rollback mode=0 budget=3 closed=1 result=0 armed=1 faults=1 temporary=1 first=1 original=1 balanced=1
gzip rollback mode=1 budget=3 closed=1 result=0 armed=1 faults=1 temporary=1 first=1 original=1 balanced=1
gzip closed rollback: 16 cases, 8 real OOM, 2 failures
```

每方向留一个 `.xrt-gzip-*.tmp`，仍有错误且原目录/七项内存快照保持。
这里 `first=1` 仅证明错误仍存在，不宣称验证了关闭态首错指针身份。
Linux ASan/UBSan旧新同16组、2次实际OOM、0失败，不把Windows
专属转码分配算作Linux故障点覆盖。

## 修复方向

属于已登记RFS边界。由稳定XRT预备事务凭据和根锚点，明确
Commit/Abort、关闭、首因与所有权，关闭后Abort无需新分配。
普通包侧不重写平台IO、不重试吞错、不提前提交、不更新开发中XRT。
稳定修复同步后，先重跑本正例与打开态，再跑UTF-8/旧目标和完整门禁。

包内 `docs/io-file-audit-2026-10-31.md` 有旧红/新绿与完整现场。
M2全局类对象AOT退役仍独立开放，不能按正文或局部例通过签收。
