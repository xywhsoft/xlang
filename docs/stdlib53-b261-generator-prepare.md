# Generator 准备失败与拥有值提交

generator 的独立原生 SSA spill 已接通。暂停前的复制和 item 交付使用
checked 准备步骤，失败执行统一 IR 已有的异常清理与 finally，成功后
才提交 item 所有权并暂停。保护借入仍复用活跃拥有帧，不增加复制。
本批不新增公开 API、byref 或其他语言参数修饰。

## 统一 IR 合同

`generator-prepare-yield` 观察一个待交付 item，并返回明确的 bool
状态，紧邻的 `ERROR_TEST` 分离成功与失败 CFG。managed item 先进入
显式临时拥有槽；失败按原始错误回滚，成功 `STORAGE_TAKE` 后交给
`YIELD`。后者只提交所有权与状态，不再承担可失败的取得操作。

IR 验证器核对准备指令、item 类型、待交付槽、状态来源和唯一提交，
禁止未检查的边进入成功提交块。派生暂停记录携带准备指令位置，Cgen
直接消费这份记录和 transfer ABI，不按类型名猜测清理或追溯 AST。
scalar 与 managed yield 都明确包含可失败的准备效果；否则 catch 中
的 yield 可能生成 THROW，却仍被错误标为 no-raise worker。

generator 的编译器 storage ABI 升为 V4，拒绝旧 V3。驻留 generator
runtime V3 接口没有变更。内部精确 storage address 的 C 输出与 async
一致；这是闭合 IR 能力，不是新增语言引用功能。

## 执行与回滚验证

最终专项为 Windows 415 阶段、28 条完整故障前缀，Linux 598 阶段、
42 条完整故障前缀；Linux 的组合数量包含附加 sanitizer 验证。

双端完整源码分别编译 326 和 325 个单元。最终专项覆盖保护借入与
独立复制、O0/O2、完整运行和提前关闭；Linux 另插桩所选生成程序及
原生闭包，执行 ASan/UBSan。每个正常组合计量 20 次完整模块生命周期。
完整运行的复制数为 13 与 14，提前关闭为 9 与 10，独立取得均恰好多
一次复制。未将这个计数当作吞吐量或时延基准。

故障前缀覆盖 false 无诊断、false 带错误、部分取得后 false、true 带
错误、item 发布拒绝、发布拒绝带错误及 OOM。原生 item 的待交付拥有
值、已取得的 spill 和已启动的生成器都必须清理；每个已进入的生成器
执行一次 finally，未启动的生成器不执行 body。清理后重试成功，核对
存活分配、字节、分配释放、非法释放和重复释放，带错误的路径保留
首个 RangeError。发布拒绝由明确标注的测试调用点拦截，其他操作仍
调用实际驻留函数，没有替换生产原生实现。

六项既有 IR 合同通过，包括 storage、managed frame、native frame、
frame loan、close 路由和首错保留。准备失败使额外的 exceptional
finally 路径成为可达路径；managed 夹具增加真正的死分支，并将其
常量 false 边等价折叠为 JUMP 后重建机器，继续验证不可达暂停不得
入机。保留全部拒绝测试，不将旧的可达性数量强行套用到新合同。

三组 301 项严格 C 双端全通过。292 项可执行夹具的 JIT/module 回归
双端零失败；六项专用宿主和两项平台／产品要求保留为独立覆盖要求，
不算运行通过。专项中的八个相邻生成器夹具通过严格 O0/O2 C，其中
两个普通 SlotMap 夹具另通过 JIT/module/独立 AOT。

## 后续验收

更广的旧 generator 专用宿主需按新合同复跑，包括拥有图、DLL 卸载、
跨线程冻结和详细关闭压力。本批的旧合同拒绝验证与严格 C 不能替代
这些完整宿主执行。工作线程／对象图 OOM、完整 53 模块函数覆盖、
发布库与内嵌 VFS 重建仍继续；Source 驱动使用固定的真实 TCC 核心，
不能据此宣告 shipping 通过。对象池仍最后，19 个 XRT 未决策组不变。

正式证据为 `E:/xlang-validation/stdlib53-b261-generator-win-final2`、
`/var/tmp/xlang-stdlib53-b261-generator-linux-final2`，以及双端
`stdlib53-b261-{discovery,execute}-*-final1`。生产输入逐项与所选源码
比对；三对象快速重链接仅用于前期定位，不计入验收证据。
