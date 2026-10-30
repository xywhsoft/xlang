> 历史工作树记录：以下结果对应文内冻结版本，不自动代表主目录当前 0.6.0；需在当前发布物上复验。

# miniz 关闭态事务回滚 OOM

2026-10-30，冻结发布物实测。打开态回滚已在包内修复并通过，不覆盖
关闭后的提交失败：Windows 删除 UTF-8 路径还需 UTF-16 临时分配。

## 复现

仓库根目录运行（仅测试自己的独占临时目录）：

```powershell
./release/xl.exe run release/extlibs/miniz/test/file_rollback_c.c release/extlibs/miniz/xrt_miniz.c release/extlibs/miniz/miniz_native.c --inc-path lib/xrt --inc-path lib/xruntime -- --closed
```

真实 miniz native 文件入口、XRT callable 和 XRT 编解码流处理，使用目录
作为目标使关闭后重命名自然失败；在末次实际回调安排一次 XRT OOM，
不替换 IO 或伪造 FINAL 标志。两方向各 8 预算，共 16 组。

Windows GCC/TCC 新旧 native 都退出 1：

```text
miniz rollback mode=0 budget=3 closed=1 result=0 armed=1 faults=1 temporary=1 first=1 original=1 balanced=1
miniz rollback mode=1 budget=3 closed=1 result=0 armed=1 faults=1 temporary=1 first=1 original=1 balanced=1
miniz closed rollback: 16 cases, 8 real OOM, 2 failures
```

原目录、首错、七项内存快照保持，问题是磁盘临时残留，不是堆泄漏。
Linux 原生 ASan/UBSan 同例 16 组、2 次真实 OOM、0 失败，退出 0。
现场与新旧对象对照见包内 `docs/file-transaction-audit-2026-10-30.md`。

## 修复责任

与其他已实测 RFS 同类，建议独立 XRT 上下文提供创建时准备的事务
凭据和根锚点，明确 Commit/Abort、关闭、所有权和首错；关闭后
Abort 无临时分配。包侧不提前提交、不吞错/重复删除、不复制系统 IO，
也不更换冻结 XRT。稳定来源同步后重跑本正例、UTF-8 路径、旧目标
与完整双平台门禁。M2 顶层 AOT 退役是另一条边界，不能一并关闭。
