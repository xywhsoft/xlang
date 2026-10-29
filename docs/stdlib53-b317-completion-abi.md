# 拥有图完成协议的统一 ABI

语言运行时的 `XL6_OwnershipFinishV1` 直接使用 XRT 的
`xrtownershipfinishv1` 类型。原生与语言节点共享同一个描述符和
回调签名，不再维护布局相似但 C 类型不兼容的两份定义。

原生完成回调返回 `XRT_OWNERSHIP_FINISH_FAILED`、`BUSY` 或
`READY`。拥有图计划明确转换为自身的状态；计划的 `STEP_ERROR`
只表示计划使用或快照错误，不是完成回调的第四种结果。未知回调
结果仍按内部错误拒绝，不能因为数值相似而默认为成功。

## 物理拥有与重试

BUSY 保留全部计划引用，READY 的职责不会重复执行。FAILED 在
诊断对象无法分配时仍是显式失败，调用方可以重试未完成职责。
模块、静态模块与 Process 共享句柄都使用规范原生回调类型。

`xlir_ownership_native_finish_runtime.c` 将 XRT 实际返回的取消
Token V3 和 Watch V2 描述符直接交给语言计划，不经布局或函数
指针强制转换。测试中的拥有者、注册、取消令牌和可选外部父令牌
组成真实物理图；独立检查器在计划加引用之前检查节点、强边和根。
Clear 保留注册，真实拥有者在完成阶段才消费 Unwatch 权利。

测试覆盖 200 个循环，其中 100 个中止后恢复注册，另 100 个
包含 800 次 BUSY 和 100 次无诊断对象的失败重试。完成及 BUSY
通道在 fail-after-zero 下不分配，最终分配和释放账本平衡。

## 验证范围

`check_ownership_completion.py` 校验完整源码构建输入，再批量执行
原生描述符集成、既有完成、准备、物理计划、Value COW 与代码租约
尾部六个程序。支持严格 C 的 O0 和 O2、Windows TCC，以及 Linux
ASan 和 UBSan；插桩模式同时编译测试使用的实际 XRT 实现。
Windows 原有 `check_xlir_ownership_plans.ps1` 同样纳入新集成测试。

本批只统一 ABI，不切换全局 Future／Cancel 准入版本。旧协议的
准备职责以及 FutureCombine、continuation 的语义终结顺序保持
不变。TaskGroup 的语言桥接真实引用、owned 父观察者和统一目录
接入仍按前批任务继续；这一项不代表 TaskGroup 语言循环退役或
53 模块发布总验收完成。
