# 动态方法依赖筛选的重复扫描

完整 222 项常规产品批次中，Windows 有 1 项、Linux 有 2 项超时。
实际产品采样以及原样解包 SDK 的性能对照显示，不能把问题仅归因于 VFS：
Cgen 反复检查动态方法依赖，其中大量方法根本没有匹配的动态调用点。
TLS ticket 用例含 1,188 个方法、2 个动态调用点和 21,774 条指令。

现在先检查规范 IR 的动态调用点集合，再检查原有目标合同与真实可达指令。
在指令循环中也先比较操作和调用点，再查找所属函数。没有缓存验证结果、
新增可变全局状态或堆分配，也没有改变 IR、ABI、裁剪规则与生成代码。
完整 IR 校验及 C/H 写出前的拒绝仍然执行。

在同一份完整源码对象集合上，仅替换这个函数进行两端 A/B 对照。
两端各 15 个真实用例，包括动态重载、继承、绑定寿命、泛型、
cryptoAdvanced 和两个大型 TLS 用例，逐方法选择及全部生成 C/H 均相同。
独立计时把旧算法等价检查移出 Cgen 区间，ticket 用例的 Cgen `clock()`
区间从 Linux 75.834 秒降到 5.579 秒，Windows 93.049 秒降到 6.139 秒。
Linux 计时为进程 CPU 时间；Windows 使用该 CRT 的 `clock()` 计时口径。
这是一份明确替换 Cgen 对象的受控诊断，不代表新常规 DLL/SO 的端到端性能。

两端现行拒绝观察器各执行 732 次真实 IR 变异；错误 IR 不能写出任何 C/H。
动态 getter 夹具的第六次访问已是拥有型 named-read，测试现在精确检查
5 次 erased getter 与 1 次 named-read，而不是继续要求旧的第六次动态派发。
绑定方法的异步帧测试分别检查 receiver 和 Future 的启动持有、释放、追踪与
初始化位，不再把 Future 误当作 xvalue。原有错误变异均保留并增加覆盖。

证据根：`F:/xlang-validation/five-groups-20261027/p7-shipping/`。
对照位于 `dynamic-selection-*-r2`，最终变异观察器为
`dynamic-mutants-*-r6`，独立计时为 `profile-cgen-ab-*-r1`；
Windows 热目录位于同名 E 盘目录，Linux 目录位于
`/home/ubuntu/xlang-validation/20261027-p7-shipping/`。
旧失败回执继续保留。下一步仍须全新常规构建与批量功能复测，
并重新执行完整 74-worker 总门；此批证据不替代这些验收项。
