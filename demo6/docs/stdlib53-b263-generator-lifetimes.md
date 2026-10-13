# Generator 跨线程冻结与回调库卸载验收

生成器存储、循环图退役与独立 control/frame 生命周期三组宿主已
完成双端 O0/O2 执行，Linux 所选原生闭包和真实回调库另通过
ASan/UBSan。当前 V3 描述符的动态库驻留与最后引用释放已有专项
证明；任意生成模块卸载、工作线程组合与完整发布验收仍未完成。
此次没有修改生产实现或新增公开标准库能力。

## 线程冻结与真实图退役

平台测试层使用 Windows 原生线程或 Linux pthread，保留原有同步
点、超时与 join。跨线程标记为真正的原子读写；线程必须在资源
退役前结束，没有模拟线程、用计数代替线程或提前放行冻结。

每个配置执行 200 次生成创建／推进冻结、200 次运行及物理退役中
control/frame 拒绝检查、200 次真实 OOM 回滚和 100 次已置 OOM
的无分配 item 转移。该程序仍计量 3400 次分配、零存活增量。

生成的 V3 control/frame 使用当前统一 IR V4 合同。140 次六节点、
七边循环图事务通过生产共享 OwnershipPlan 执行，涵盖 finally
先于对象 finalizer、真实外部根、缺少 frame 适配器拒绝、准备错误、
无诊断失败、finalizer 错误、复活与独立 frame 引用。每配置保留
560 次跨线程冻结、3360 个创建 OOM 预算和 980 次实际拒绝；这些
创建预算不等于工作线程／清理各阶段的完整 OOM 前缀验收。

## 动态库和描述符驻留

真实回调库包含 resume、frame destructor、trace 及布局描述符。
旧 V1 控制协议每配置保留 100 次卸载，检查普通 owner 已关闭后
回调取得的子 lease 仍保活代码，返回尾部完成且子 lease 释放后才
解除映射。

旧 V2 managed 组仍执行 150 次最后 owner 重入关闭、150 次稀疏
payload 析构、100 次独立 control/frame 描述符卸载、400 次跨线程
冻结、100 次真实 OOM 和 250 次 ABI 拒绝，原有全部断言和计数保留。
新增当前 V3 组另执行 100 次描述符卸载与 100 次跨线程冻结，分别
让 control 和 frame 成为最后拥有者。V2 capture 布局真实位于库内，
不是驻留复制；独立 frame 的布局访问与清理也必须持有 code lease。

Windows 在最后 lease 释放后检查 `VirtualQuery` 的 `MEM_FREE`。
Linux 在 `dlclose` 后立即检查回调地址所在页的 `mincore` 返回
`ENOMEM`，再核对 `dladdr` 已无注册映射。只从 loader 名单移除
不足以判定代码页已解除映射。Linux 这里测量的是实际共享库，
存量输出中的 DLL 字样是历史测试名称。

本轮每配置共 300 次实际回调库卸载。它们使用真实 resident runtime
与代码 lease，但没有覆盖编译器发布的任意生成模块、所有 payload
原生类型或多个模块相互引用，因此不能授予一般卸载许可。

## 批量门禁和剩余工作

三组完整人口在 Windows 两个、Linux 三个配置执行。C、绑定头与
IR 两次生成必须完全一致；生成契约链接逐项比对过的完整源码核心，
被测生成程序及按需原生供应方重新编译。共享 OwnershipPlan 是
手写退役宿主的明确依赖，不是 Cgen 算法或供应方替换。独立宿主
失败会集中记录，随后继续其他配置，最后统一判断本轮是否通过。

最终证据位于 `E:/xlang-validation/stdlib53-b263-lifetimes-win-dev2`
和 `/var/tmp/xlang-stdlib53-b263-lifetimes-linux-dev2`；最终选择性
源码快照为 `E:/xlang-validation/stdlib53-b263-source-final1/frozen/demo6`。
认证将已执行的输入逐项与最终快照比较，保留日志和产物哈希。

下一步继续实际生成模块与 worker/graph OOM 组合、完整 53 模块
函数覆盖及新 IR 发布库／内嵌 VFS 验收。WaitQueue 保持 MPMC 与
拥有接收，CRT 无未决策、XRT 19 个未决策组不变，对象池仍最后。
