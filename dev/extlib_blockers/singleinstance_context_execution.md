# singleinstance 跨 XL_Context 同时执行的验收阻断

## 当前复验：2026-11-02

Windows 与 Linux 当前发布 DLL 均复现 M8。新的跨平台公开 `xl.h` 宿主
使用两个真实上下文和两个自有线程：第一个持锁并写就绪文件；第二个
执行争锁脚本，十秒内不能完成；宿主再写完成文件，使前者释放并退出。
随后第二个才继续且争锁断言失败，宿主报告 blocked-upstream 并退出 2。
它先 join 两线程再销毁上下文；门禁进程外另有 120 秒超时，不以串行
完成当作并发验收。没有修改主线的执行锁或把双上下文改成静态模拟。

原 Windows 门禁未执行真实上下文，本次补入为最后必过正例；Windows
43 阶段前 42、Linux 45 阶段前 44 符合，最终门禁都真实退出 1。普通
C 子进程、两个独立静态包实例和各八份普通 AOT 通过不替代这一要求。
测试就绪通知不再使用一线程写 CRT 环境表、另一线程并发 getenv；
只在启动线程前设环境，两脚本用门禁独占目录内的就绪/完成文件交接。

在仓库根目录运行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/singleinstance/test/check.ps1
```

```sh
bash release/extlibs/singleinstance/test/check.sh
sh release/extlibs/singleinstance/test/check_contexts.sh
```

前两条是本轮实际完整执行的入口；独立 wrapper 的参数已同步，但本轮
没有另一次重复 wrapper 验收。证据与诊断见
`docs/reports/extlib-singleinstance-cleanup-20261102.json`。下面旧日期和
旧工作目录留作历史，不是当前命令或退出码来源。

2026-10-26 当前 Linux 发布版复验：独立 C ASan/UBSan、双模块
争锁、中央与顶层语言边界、C/XLang 双语 AOT 和固定输出先全部
通过；单独 `test/check_contexts.sh` 仍在约 10 秒有界观察后输出
`blocked-upstream: second XL_Context did not execute during first context's script`
并退出 1。该脚本现位于 Linux 默认 `test/check.sh` 的最后一步，
防止只凭包实例/子进程通过而误签收真实双上下文合同。

## 观察

`singleinstance` 原先在 POSIX 使用 XRT 的 `fcntl(F_SETLK)` 文件锁。
该锁按进程计：同一进程的两个独立 JIT 模块会同时报告取得同名锁。
包内改用按打开文件句柄计的 `flock` 后，
`test/test_multiple_modules.c` 的红色复现已转绿。

真实 `XL_Context` 验收仍有独立边界：一个上下文运行脚本并等待哨兵，
另一个上下文同时调用 `xlRunScriptFile` 时，后者在 10 秒观察窗口内不返回。
宿主写入哨兵让前者退出后，第二个调用才继续；由于锁已释放，
它得到的结果不能证明两个上下文同时执行时的锁语义。
这可能是当前 DLL 的预期串行合同，也可能是上下文/编译执行锁粒度过大；
需要主线确定合同并处理。本任务不修改编译器、运行时或 DLL。

## 最小复现

在 `demo6` 目录的 Linux/WSL 环境运行：

```sh
sh release/extlibs/singleinstance/test/check_contexts.sh
```

该脚本构建只调用公开 `xl.h` API 的 `test/test_contexts.c`，
运行 `context_hold.xl`、`context_probe.xl` 和 `context_after.xl`。
宿主在第二个调用开始后至多等待 10 秒，随后释放第一个脚本，
因此复现不会无限挂起。当前输出为：

```text
blocked-upstream: second XL_Context did not execute during first context's script
```

包级独立模块门禁另用 `sh release/extlibs/singleinstance/test/check.sh`，
验证两个独立静态包实例的锁互斥、释放隔离和真实子进程，不依赖 DLL 同时执行。

## 建议

先明确 `XL_Context` 是否承诺不同实例可并发执行。如果承诺，
审计 `xlRunScriptFile` 的全局锁与 TCC 编译/重定位锁：只在共享状态操作时持锁，
不要跨入用户脚本执行与等待。随后将本复现纳入 DLL 门禁，
重新验证两个上下文并行时同名锁互斥、释放后可重新获取。
