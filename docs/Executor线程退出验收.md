# Executor 真实线程退出与 checked 失败账本

本批落实已批准的“首个错误＋失败计数，显式 checked 报告”，补齐受管理
Executor 的真实工作线程退出路径。不等同于并发或五组长期任务全部完成。

## 实现边界

- 作业 body 与捕获清理合为一个作业失败；每个失败的真实线程退出另计一次。
  首错不被后续错误替换，无已分配诊断的失败仍保留失败位；计数饱和不丢通知。
- ThreadLocal 跨作业保留。关闭后排空，真实工作线程执行驻留 checked 退出入口，
  完成 hook 后才发布退出。wait 包含这个尾部，错误由 wait／throwIfFailed 报告；
  未观察的失败由普通退休或图清理交付，不静默忽略、不因用户析构抛错而 abort。
- XRT 持有一个真实的共享退出上下文引用，各 Worker 只借用；全部 join 后只释放
  一次。创建失败包括部分线程回滚不接管输入，也不调用 Exit／Drop。
- 退出上下文就是同一失败账本：Resource 引用与 native owning slot 分别追踪。
  封存的 typed ExecutorExit 能力必须先匹配真实合同地址，才准入 NativeExecutor；
  合同副本不授权，活动退出回调为 BUSY，图 Prepare 先完成退出和 join 再 Clear。
- 清理 Source 的 checked 作用域延续至真实 registry/resource/ledger 的最后释放；
  active 覆盖这段尾部。独立检查 pin 延迟的最后释放仍是持 pin 方自己的清理职责。
- 可选分派不拉入 ThreadLocal 实现。类型化函数指针的短发布／读取使用 XRT 原子
  门，避免依赖部分 TinyCC 安装缺少的 stdatomic.h，也不做函数／对象指针强转。
- ThreadLocal 包装类改为声明时显式构造原生值字段；不放宽 IR 原生资源初始化
  检查，不修改 Cgen。适配仍在驻留 runtime，生成 C 仅调用现有声明／拥有清理。

## 验收范围与证据

证据根目录：`F:/xlang-validation/five-groups-20261019/p1-executor-worker-exit/`。
每轮使用新目录保留失败与修复记录，最终轮检查输入、日志和实际产物摘要。

| 范围 | Windows | Linux |
|---|---|---|
| 原生 XRT 模块化＋单头，GCC／TCC、O0／O2 | native-win-r2，16 个测试产物 | native-linux-r1，16 个测试产物 |
| 实际资源、拥有图、退休、错误与裁剪 | resource-win-final3，74 阶段 | resource-linux-final3，76 阶段 |
| 存量 detached 闭包 producer | jobs-win-final，29 阶段 | jobs-linux-final，31 阶段 |
| 语言、模块、严格 C、AOT、拒绝与 ThreadLocal 回归 | language-win-final2，89 阶段 | language-linux-final，89 阶段 |

资源测试新增每个运行产物 48 组真实线程退出：正常、无诊断失败、有诊断失败，
显式重复报告与未观察退休；精确检查分配／释放、存活字节和原错误代码。
故意删除退出错误登记的反例必须在运行时断言失败，不能把编译失败算作有效反例。
既有图恢复、任意 Finish 次序、OOM、最后拥有者在 worker 释放、创建／关闭竞争
及错误通知反例继续验证。Linux 资源和 producer 的实际选中原生闭包通过
ASan／UBSan／leak；不据此称整个语言运行库已经 sanitizer 验收。

实际语言测试包含 ThreadLocal 跨两个作业保留、真实退出析构、原 ValueError、
显式失败计数、模块未观察退出错误，以及三个既有 ThreadLocal 用例的运行／模块／
AOT 回归。未观察错误可能在运行收尾或模块退休时交付；要求 checked 失败码 1，
异常终止不算通过。最小 Source／Executor／Pool 符号表不包含 ThreadLocal provider。

Windows 源驱动验证 614 个编译单元及匹配缓存摘要；Linux 从实际源码构建 613 个
编译单元。仅复用 genuine TCC 核心，独立驱动不冒充新构建的 shipping DLL／SO／VFS。
验证完全离线，无网络、扫描、攻击模拟或网络安全性质专项。

XRT 原生提交为 `0dd252e2efd5b1b4c42fb710a347fc7660aab5ef`。xlang 只移植
Executor 的精确生成段，保留既有微秒 deadline ABI，不整包升级其它 XRT 模块。
XRT 工作区的文件／内存／扩展库修改不纳入提交；生成单头只提交干净 HEAD 加本批
增量，用户的其它生成段留在工作区。xlang 的原有 IDE／扩展／文档及 IR 空白保留。

## 尚未完成

公开原子批量载体、公开 TaskPool 的拥有结果与真实 worker 退出、其它剩余并发，
完整 shipping DLL／SO／VFS 验收，以及后续网络、受管理非对称密钥、X.509、TLS
和最终双平台联合验收仍在长期任务中。本批不改变未决策分类。
