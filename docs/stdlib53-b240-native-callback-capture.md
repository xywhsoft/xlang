# 原生值在 task 和 Future 回调中的捕获

SlotMapKey 已接通 task.spawn 的参数、Future.then 的结果参数，以及
不含 await 的 async 函数参数。捕获产生独立拥有值，成功、异常、取消
和提交前失败都使用同一套类型化清理，不经过 xvalue 装箱。

## 帧布局与所有权

TaskRuntimeV3 和 FutureCallbackRuntimeV4 接收 CaptureLayoutV2。
它们复用已有 task／Future 回调的物理帧、引用计数、代码租约、Watch、
稳定 producer、调度和退役流程；旧入口仍只接收原来的精确布局，
不把 V1 描述符强转为 V2，也不另建状态机。

新布局中的原生字段由统一 IR 的原生值计划指定永久发布能力。
Cgen 只生成类型化字段、静态布局和复制／提交调用，共用字段渲染和
已有捕获复制逻辑。常驻清理接收实际分配长度，先将拥有槽清空，再
释放资源；重复清理看到空槽，健康清理在 armed OOM 下不分配。
清理的 false 状态独立于是否有诊断，并传入现有 checked 错误作用域。

原生键没有语言图边，输送帧仍保留真实 Future、Promise、取消与代码
租约边。已退役但仍被真实 pin 保留的回调帧没有残留 payload 边。
原生复制和发布者由 IR 显式选择；最小原生闭包不包含键装箱或
SlotMap 容器存储。含原生字段的程序才选择其发布者。

## 类型推导顺序

auto 变量先以未赋值名字登记，检查初始化表达式后再提交完整类型。
Future.then 在检查回调后确定的 future<SlotMapKey> 不再被提前保存的
裸 future 类型覆盖。该顺序保留自引用拒绝、整数字面量范围检查和
泛型／nullable 合同，未引入回调专属推导补丁。

## 双平台验收

Windows 317、Linux 316 个完整源码编译单元通过。捕获专项 Windows
323、Linux 358 阶段通过；分别有 202／189 次故障位置检测，8 组复制／分配
前缀跑至末端。故障注入限当前线程，不代表 executor 线程分配注入。

每个优化等级执行 1000 次原生场景，覆盖 pending／ready／cancelled
回调、真实 task executor 和提交前释放；600 次回调调用核对清理与
退役 pin，另有两项无效元数据拒绝。语言用例每个优化等级执行
200 次模块生命周期，验证普通 async 参数、task.spawn、then、重复
await 和抛错 finally。故障后使用现有 checked busy 清理的有限重试。

18 项实际 IR 篡改被验证器、C／header 输出门和原生计划拒绝。
三个遗漏 task 清理、遗漏回调清理、错误清理长度的实际坏实现经
编译、链接后被运行断言拒绝。最小原生闭包选择 17 个供应方，
语言产品选择 23 个供应方。O0／O2 独立 C、JIT、module、AOT
通过；Linux ASan／UBSan 覆盖两个选定闭包的全部供应方。

旧原生 Future 结果与异步帧双端复测通过。相邻回归 Windows 71、
Linux 47 阶段通过，包含泛型 auto、nullable、旧 task／callback IR
合同和三个错误程序拒绝。Windows O0／O2 的旧 V1 捕获布局、生成
六字段 async 捕获、十个 Future continuation、producer／取消竞态
及生成器拥有环压力均保持资源平衡。旧布局测试拒绝 325 项无效布局，
task 合同拒绝 652 项 IR 篡改，回调合同拒绝 175 项存储 ABI 篡改。
常驻目录双端核对 278 个组件、1437 个真实导出与 13 个拒绝对照。

## 后续范围

Future 元素遍历、WaitQueue、独立 borrowed 原生 SSA spill 的完整
失败边界，以及 53 模块与正式 DLL／VFS 发布总验收继续。对象池
仍最后，CRT 0、XRT 19 组未决策范围不变。本批未修改 XRT／xruntime
算法，也未纳入工作区中独立的 AST／IDE 功能改动。
