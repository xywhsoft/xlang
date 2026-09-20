# XLIR 原生缓冲拥有链

2026-09-19，v214。原生池、字节结果和嵌入缓冲的实际拥有关系已实现，聚焦
GCC O0/O2、TCC 各 68/68，通过原样 XLIR 规划器的组合回收验证。它还不是
完整 transport／生成模块拥有图验收；DEMO6 生产集成仍为 v209。

## 实际拥有合同

- 缓冲链是唯一嵌入存储，不增加伪造 RC 节点。宿主通过明确的 Trace 投影
  allocator、每个 live block 的 pool 和每个认证 external context 的真实引用。
- 池是计数叶子：creator、owned allocator、live block 和 collector hold 各为
  实际引用。池不反向拥有 live buffer，cached block 不反向引用池。
- 跨池 Move 保留块原来的 pool；目标、源缓冲各自保留 allocator。旧 borrowed
  allocator/data、自定义无身份 Release、file extent、reservation 明确拒绝。
- 新 AppendOwned 只在成功时消费一个 context 引用；未知策略必须在访问任何
  子节点之前拒绝。xnetbytes 直接作为不可变计数叶子，不伪造借用 span 节点。
- Engine 真实拥有并投影 worker pool 的 creator。Prepare 可先关门并 join，
  不要求宿主析构之前销毁 live buffer；Finish 退休池 creator。独立外部缓冲
  只使池可达，不把 Engine／其他宿主虚假保活。普通非图 Destroy 仍保留 busy。

## 用户回调与物理释放

结构修改在 mutation 中完成，外部 Release 按原 FIFO 顺序在 mutation 之外
执行；原 block 的真实池引用保护整个回调尾部。缓冲活动状态是嵌套计数，不是
重入锁：旧接口允许的释放回调追加仍然有效，嵌套操作完成不能提前放行外层图。

Pullup 在回调后重新取得当前首部视图；前缀被回调清除时返回失败和空视图，
不返回悬空地址，也不泄露额外尾部容量。Clear 保留 allocator 和回调追加；
Unit 先关闭写入，再释放回调和 allocator，重复调用无副作用；完整 Unit 后
可重新 Init，活动回调中不能重初始化存储。宿主必须存活到 API 返回，所有
buffer 操作仍按原合同由拥有者串行化，并未变成跨线程并发容器。

用户可见 Release 必须在宿主语义 Finalize／Prepare 中完成并重新检查复活，
不能挪到提交后的物理 Finish。本次没有修改 Cgen 去猜测这种职责，也没有修改
生产规划器 `src/runtime/xl6_ownership_plan.c`。

## 失败与批量验证

首次原生发现批次三配置各 65/68；3 个失败程序归为两类：

1. 新 bool activity 错误拒绝了旧接口释放回调中的 Append；改为嵌套活动计数，
   保留原 `test_net_buffer.c` 的正例，新增测试也改为验证追加后的真实内容。
2. Engine 新增真实 pool 子节点后，测试漏算两个真实 collector internal slot；
   正确的剩余边数为 3，不是 1，仍检查精确节点／边／外部根计数。

修复后一次聚焦批次三配置各 68/68。每个普通／单头新增程序验证 54 个清理场景、
6 次跨池移动、72 次拒绝、8 个分配预算（1 次真实失败）、6 个字节叶子、6 次
Pullup 视图失效和 6 次终结 Unit；每个回调在嵌套操作前后都由另一线程取得
freeze 并确认活动 buffer／pool 拒绝图检查，各场景精确内存平衡。

真实规划器入口 `tools/check_xlir_native_buffer_plan.cjs` 复用 Engine runtime
实现，链接未改的 XLIR planner。三配置各 96 轮，包括：

- 96 个实际宿主／Engine／pool 图，精确检查 3 个节点、7 条边；
- 96 次错误 policy 拒绝、96 次语义析构与实际释放、96 次回调跨线程 freeze；
- 48 次回调复活：旧计划 ROOTED，撤销后用真实 escape 引用取得新计划 pin，
  再实际释放 escape、重新验证并回收，不伪造内部根，不重放语义析构；
- 48 次独立外部缓冲保活池：Engine 和宿主释放后仍能读取字节，新分配 CLOSED，
  最后 Unit 释放余下的两个真实池引用；两种物理释放顺序均覆盖。

新规划器测试首次因遗漏 `<string.h>` 三配置编译失败；修复后发现测试错误地
将停机过渡期间的 STEP_ERROR 准入拒绝视为语义 FAILED。按既有公开合同，在
限时内保留同一 plan/pins 重试准入，语义 FAILED 仍立即失败；全部计数和精确
内存断言保留。两批失败现场都保存在独立目录，没有被覆盖。

在同一候选上，原 Engine 规划器三配置各 96 轮、网络任务规划器各 80 轮通过。
稳定后只执行一次完整原生回归：GCC **198/198**、TCC **198/198**，零失败、
零未运行，保留上一轮全部 162 个程序。GCC 编译 167.712 秒、运行 41.283 秒；
TCC 编译 16.424 秒、运行 29.084 秒。8085 个当前工作区来源文件前后指纹一致。
聚焦、组合验证与完整回归均保存输入指纹；最终候选须逐字节匹配已测单头才可
复用专项证据。最终统一回归记录为 `build/tmp/xlir_launch_v214_native_verified/results.json`，
总检查点为 `build/tmp/xlir_launch_v214_checkpoint/results.json`。

## 尚未完成

本项不自动认证原生 port／completion、嵌入命令、transport 的拥有关系，也不
完成生成模块 host 的缓冲投影与语义回调职责。原 18 个模块服务图门、混合对象／
DLL 卸载、完整语言／FFI、同源码发布／部署隔离、平台与旧路径退役仍须完成。
v211 ResolverFuture 的 TCC 隔离缺口继续保留，不能用本项 TCC 通过抵消。
本轮没有替换正式嵌入／声明头和发布二进制，也没有改变默认 Cgen。
