# Executor 原子批量提交验收

日期：2026-10-20。范围仅为已批准五组长期任务中的 P1 Executor 批量公开面，
不是完整并发、网络、密码学、X.509、TLS 或发布完成声明。

## 本批实现

- 声明式 `Executor.submitBatch(array<func()>)`，支持静态／动态方法调用；
  参数和结果必须精确为零参数 void callable。
- 短期 pin 下验证并保留实际 callable box，结束来源 Array 操作后才原子准入；
  回调可清空源数组，活动的同一闭包可再次提交。
- 一次 native SubmitOwnedBatch，整批接受或拒绝；容量不足不切批，无已执行前缀。
  开放执行器的空批次为 no-op，关闭执行器仍 checked 拒绝。
- OOM／类型拒绝逆序回滚；取消不执行 body，清理失败进入同一首错／计数账本；
  不引入第二套队列、Future 或默认线程池回退。
- XRT 新增 immutable callable policy 身份探针：不调用用户函数，不分配，允许
  活动调用；它不替代完整图准入／静止检查。xlang 只同步必要 runtime_call 片段，
  不升级固定快照的期限 ABI，也不导入另一个工作区的文件／内存改动。
- 旧式不具备协调生命周期的 opaque closure box、同名外来 callable、相同内容的
  policy 副本不能获得批量能力。修正公共 callback 诊断装饰顺序，先记录 bool／原始
  首错，再为裸 C 调用者补诊断；NULL 首错不会被后续已分配错误替换。

XLIR 的闭合合同携带精确 Array／callable identity，独立校验器重算存储配方与
物理调用合同。Cgen 只增加两个 identity 的 context literal 发射，不生成批量算法。
仅借用已有 Executor 的模块不创建本地 Execution Source；未使用声明仍按需裁剪。

## 冻结后的验收

证据目录：`F:/xlang-validation/five-groups-20261019/p1-executor-batch/`。
只认下列 final 目录（语言验收使用 `language-win-final2`／`language-linux-final2`）；
discovery 和首次 final 的报告保留，不追溯改写为通过或替代最终报告。

| 检查 | Windows | Linux | 范围 |
| --- | ---: | ---: | --- |
| native-final | 4 组／8 实际二进制 | 4 组／8 实际二进制 | GCC／TCC O0／O2，模块化和单头 |
| jobs-final | 35 阶段／6 预期拒绝 | 37 阶段／6 预期拒绝 | 原生作业与拥有图、故障注入 |
| resource-final | 74 阶段／8 预期拒绝 | 76 阶段／8 预期拒绝 | checked 退休、真实 worker-exit 回归 |
| language-final2 | 115 阶段／20 预期拒绝 | 115 阶段／20 预期拒绝 | JIT、模块、严格 C、独立 AOT、XLIR 拒绝 |
| source-final | 615 翻译单元 | 614 翻译单元 | 当前源码的编译器／驻留实现／SDK 驱动 |

首次 final 的执行已通过，但旧报告的 BatchSurfaceAccepted 标记仍为 false；修正
报告范围和完成标记后，用相同冻结源码驱动重跑完整语言门，正式结果使用 final2。
没有因为该报告修正改变生产实现或把旧报告就地改写。

正式完整性条件：每份 `results.json` 均须 Complete、InputsUnchanged、
LogsUnchanged 为 true，Failures 为空；提交封印再次核对输入与日志实际哈希。
源码驱动仅复用真实 TCC 核心；Windows 匹配缓存项也核对源与实际对象哈希。
不使用既有 release DLL 替代本批驻留实现。

原生夹具保留既有 raw closure／租约／多工作线程测试，增加 boxed body／Drop
结果组合、NULL／已分配首错、非 void 结果拒绝、取消、准备分配前缀、外来／旧式
box 拒绝，以及 8 个真实七节点／七边／三个外部根的 queued 图。六种坏实现均须
成功编译后被断言拒绝，不能把编译错误算作负控制成功。

语言夹具实际运行普通批量、容量不足无前缀、清空来源数组、活动闭包再次提交、
动态方法与原始异常重复报告；仅借用调用方资源的模块通过严格 C／模块／AOT。
真实 XLIR 在旧 15 项以外另拒绝 17 种批量合同变异，C／头输出均不得提前交付。
既有 ThreadLocal 跨作业和真实线程退出、未报告错误与 Channel 服务组合仍回归。

Linux ASan／UBSan／泄漏检查仅覆盖选定原生实现、provider 和 native 夹具闭包，
不声称全部语言运行时已 instrument。没有网络通信／安全专项、系统安全设置变化、
release 产物提升、推送或无关工作区清理。

## 后续仍需完成

P1 的公开 TaskPool 拥有型泛型结果、真实 worker-exit 和其余并发／TaskGroup 接入；
随后按依赖完成网络基础与全部传输、受管理非对称密钥／ASN.1、X.509、TLS，
最后完成新 shipping DLL／SO／VFS 和两平台常规联合验收。整项长期任务保持 active。
