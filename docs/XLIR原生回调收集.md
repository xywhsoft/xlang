# XLIR 原生回调收集参与者（v230）

回调绑定现在提供 `xrtNetCallbackOwnershipAdapterV1`。它消费调用方明确承认的
不可变策略身份，不按函数名、析构函数地址相似性或任意 Data 猜测拥有语义。

## 生命周期协议

- Hold/Drop 对应实际引用；多个 native 槽及每次执行中的回调分别持有引用。
- 活动回调拒绝图读取及适配器准入；已声明参与的节点在活动期间 Prepare 返回
  BUSY，不等待其他参与者、不触发取消、不提前释放 Context。
- Claim/Restore 保持对象字段完整，收集被外部持有或终结器复活打断时可恢复。
- Clear 不运行用户代码；Finish 先摘除 Context，再在冻结区和 mutation 区外
  调用驻留 Drop，重复 Finish 不重复释放。
- 完成 Clear 后禁止新增引用和再次进入该回调绑定。

## 可重复的验证入口

`tools/check_xlir_native_callback_plan.cjs` 接受 `--runtime-directory` 指向原生
XRT 仓库，以及新的 `--directory` 输出目录。它冻结输入、生成候选头，并运行
Windows/Linux 的 GCC、GCC O2、TCC，以及 Linux ASan/UBSan/泄漏检查。

`test/cases/xlir_native_callback_plan.c` 复用现有真实 TCP/UDP 回调、替换、共享
Listener 绑定、双 Worker 活动和 OOM 用例；另外将原生绑定直接接入未修改的
XLIR 规划器，验证双节点环、重复拥有槽、外部根、放弃收集、复活和精确释放。
Context Drop 会等待另一线程获取图冻结，用来检查释放路径没有继承冻结锁。

双节点环是有意构造的绑定参与者白盒测试，不是完整网络传输对象环。
完整 Engine、transport、服务、TLS、语言模块、FFI 和 DLL 卸载验收仍未完成。
生产头文件、release 和默认 Cgen 未切换；未知父图仍必须拒绝收集。
