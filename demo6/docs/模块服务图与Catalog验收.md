# 模块服务图、typed catalog 与 TaskGroup/TaskPool 组合

日期：2026-10-20。本批刷新 P0/P1 的源码合同与一项跨组件语言验收，没有修改
生产 IR、Cgen、驻留 runtime 或 XRT，不代表 P0/P1 或五组目标完成。

## 现行合同

- 普通值、显式 task、async、task＋async 四个程序对应 Pool/Executor 的
  00、10、01、11 组合。async 不隐式使用 TaskPool。有服务的当前模块使用
  retryable V4；错误拥有字段及旧 V3 必须在 C/header 输出前被拒绝。
- 宿主调用 typed catalog，Task domain 登记真实 producer/data/watch；Pool
  domain 使用带退出策略的 V2 adapter。不要求把这些实现复制回旧 API 文件。
- 101 个 domain 编译单元提供 129 个登记入口。完整 C 合同引用全部 getter 并
  真实构建 catalog；声明 SDK 与所有 domain 源另作严格语法检查，不能按文件数
  推断登记完整性。仅用 opaque policy 指针的 loader 不必引入完整 native 声明，
  但不得猜测布局或定义按值 ownership query。当前该纯声明单元为 TaskPoolLanguage。
- 独立效果预期补齐 Stack erasure 和 checked container projection：前者消耗
  typed Stack owner，后者消耗 witness、借用 source，失败也消耗该输入。这与
  现有 resident Take ABI 一致，不修改生产效果去适配旧测试。

本批同步七组生命周期静态守卫的现行位置／版本。仪器化副本只用于批量发现，
不是通过证据；总守卫仍有 28 组失败待逐项判定，不能假定全是生产缺陷或全是
无害工具债务。没有删掉这些守卫或宣布总门通过。

## 实际检查

`tools/check_stdlib_ownership_catalog.py --native-only`：完整登记及 17 个 typed
policy 组、关键执行策略的精确身份、封存后拒绝添加、未知回调不被调用、冲突
拒绝。cursor 在完整和空 catalog 中都可独立准入。O0/O2 各执行完整构造的 34 个
分配前缀、吞掉 load 失败的 4 个前缀与精确内存账本；七个错误实现先成功编译再
运行拒绝。此模式不冒称重新执行默认的 78 条 source-JIT graph 矩阵。

`tools/check_xlir_module_service_graph.py`：O0/O2 各验证四个模块分区、32 个 IR
变体及 64 次 C/header 零输出拒绝；实际生成的 trace、prepare、退休与 text/JSON。
全表检查 467 个 RuntimeOp、3,736 个输入位置、52 个 consume 位置和 40 个非法
操作／边界位置。每个原生 Pool 产物完成 280 次平衡生命周期、240 个排队 raw／
language 图、520 次真实准备及两种 Finish 次序。另删除实际 catalog 的 cursor
准入分支：编译成功后必须在真实 cursor Resolve 断言处拒绝，只替换该反例的一个
catalog 对象，不修改项目源码。

测试单元使用 O0/O2；完整生产源码对象经当前输入／头／对象／driver 摘要复核后
复用，保留其记录的选项，不冒充全产品 O2 重建。

## 语言组合

新增 `stdlib_task_group_pool_composition.xl`，通过已批准的
`group.start(func() as future<T> ... return pool.submit(work) ... end)` 组合
覆盖 `task_group_pool` 场景，不另加 group 专用调度器，也不提交后再 add。
验证整数、含 NUL/UTF-8 的 string、无 payload 结果，以及关闭的 pool 拒绝提交
时的组容量回滚和成功重试。组观察完成与 pool 真实退出分别等待，四个任务只
统计一次。用例同时加入默认 TaskPool 完整回归。

`check_stdlib_task_pool_language.py --composition-only` 运行该用例及原有 47 项
TaskPool IR 拒绝合同，不冒称重新执行 ThreadLocal worker-exit 矩阵。两端真实
源码 JIT、module、严格 C O0/O2、独立 AOT 均通过；无隐式 ready Executor、无默认
内部 task pool、无 MODULE_ALL。Windows AOT 仅导入 msvcrt/kernel32；Linux 仅
导入系统 C/math/loader，不依赖 xl/xlc 动态库。

## 最终证据

证据根目录：`F:/xlang-validation/five-groups-20261020/p0-catalog-service-graph/`。

| 报告 | Windows | Linux |
|---|---|---|
| catalog | `catalog-win-r3/results.json`，27 阶段 | `catalog-linux-r3/results.json`，27 阶段 |
| 服务图／效果 | `services-win-r2/results.json`，16 阶段 | `services-linux-r2/results.json`，16 阶段 |
| 语言组合 | `composition-win-r1/results.json`，11 阶段 | `composition-linux-r1/results.json`，11 阶段 |

共 108 阶段，含 18 项预期运行拒绝。提交前复核输入、日志、响应文件及产物摘要；
失败轮次保留但不进入最终通过计数。没有覆盖 release 或推送，没有网络通信、外部
DNS、扫描、攻击模拟、安全设置修改或网络安全专项。

本批不是新 shipping DLL/SO/VFS、所有旧夹具或 P1 整体验收。继续审核其余核心／
公开能力，再按原范围完成网络 40、非对称密钥／ASN.1、X.509、TLS 和双平台联合验收。
