# singleinstance 跨 XL_Context 同时执行的验收阻断

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
