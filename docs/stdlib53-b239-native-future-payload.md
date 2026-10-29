# 原生 Future 结果的发布和独立接收

future<SlotMapKey> 已接通直接原生结果槽。普通异步 worker 与跨 await
机器都能返回完整拥有键；重复 await 取得独立拥有值，接收后可保存到
已有原生键数组。返回时移动结果，接收时按精确类型复制，不把 Future
结果转为 xvalue。数组字面量仍使用其既有投影桥接，二者合同不混淆。

## 结果槽和所有权

NativeFuturePayloadRuntimeV1 复用 CaptureV2 的永久原生 leaf 发布能力。
一次分配包含不可变类型元数据与对齐的直接值槽；槽地址就是 XRT Future
payload。分配得到空槽，生产者移动值后使用同一个常驻 policy 发布。
发布失败不消费槽，常驻 Drop 先将类型槽清空，再释放拥有值和分配。

Copy 检查精确 profile 身份、实际大小和对齐，以及输出不能覆盖输入、
分配元数据或不可变类型信息。统一 IR 的 owned-native-value 合同要求
每次 await 获取前建立类型化空目标，成功才进入接收成功分支。复制的
false 状态独立于诊断；部分取得拥有值后失败，也先清空并释放目标。

普通 task worker 的新 policy 提交入口复用原有 executor admission、
稳定 producer、Watch 与结果 Drop 身份检查。常驻地址来自发布表，
不保留生成模块的回调或描述符。原生值本身没有语言图边；Future 仍
保留取消令牌、producer 和转发源等真实输送边，不能把整个 Future
视作 leaf。

原生结果组件按使用需要选入。结果和 await 的精确发布者由 IR 决定，
不依赖帧中恰好有同类型字段来间接保留。未使用原生结果的程序不选入
新组件。没有修改 XRT／xruntime 算法或另建 Future 状态机。

## 双平台验收

Windows 317、Linux 316 个完整源码编译单元通过。专项 Windows 423、
Linux 480 阶段通过；每端 284 次故障位置检测，8 组完整前缀跑至末端。
这些分配和复制前缀注入仅作用于当前线程，不代表 executor 线程注入。

普通 worker、跨 await 返回、两次 await relay、重复接收、原生数组
保存和抛错 finally 通过真实语言执行。每个优化等级执行 200 次模块
生命周期；故障后允许已接收工作继续退出，以现有 checked 清理的
busy 状态有限重试，再重初始化，并核对精确资源平衡。不把 busy
改成成功，也不丢弃执行引用或取消独立源。

原生五种场景每个优化等级各执行 200 次，含正常发布和独立复制、
取消后发布拒绝、无诊断复制失败、取得拥有值后仍返回 false，以及
返回 true 但发布诊断。402 次布局／身份／重叠拒绝检查通过。健康
释放在 armed OOM 下不分配。4 个原生结果生产者、7 个 await 与
29 项实际 IR 篡改通过验证。

三个实际坏实现分别遗漏部分复制清理、放宽 profile 身份、遗漏最终
拥有值释放，严格编译、链接后被运行断言拒绝。原生最小闭包选择
14 个供应方，不包含键装箱或 SlotMap 存储、task executor；语言
产品选择 35 个实际供应方。独立 C 的 O0／O2、JIT、module、AOT
通过；Linux ASan／UBSan 覆盖两个选定闭包的全部供应方。

旧原生异步帧双端各 387 阶段、240 次故障位置检测复测通过。
旧 generator、producer／取消合同及 Windows O0／O2 的拥有图、
交接回滚、三线程竞态、armed OOM 和生成器拥有环压力回归保持通过。
常驻目录双端核对 278 个组件、1435 个真实导出与 13 个拒绝对照。

## 后续范围

原生 callback／task 捕获、Future 元素遍历、WaitQueue、独立 borrowed
原生 SSA spill 的完整失败边界，以及 53 模块与正式 DLL／VFS 发布
总验收继续。对象池仍最后，CRT 0、XRT 19 组未决策范围没有扩大。
