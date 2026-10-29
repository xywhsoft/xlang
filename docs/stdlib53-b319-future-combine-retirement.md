# FutureCombine 的终结后物理退役

原生 ALL、ANY、RACE 聚合器已有 Clear 后完成协议，可与 Future V5、
TaskGroup、取消观察者共同退役。它保留重复源的独立强引用，并按
实际 producer、映射 Data 和完成注册计数，不把嵌入式 item 当作新节点。

## 完成职责

Clear 禁止新的聚合和 mapper 语义，保留端点、源、上下文和取消注册。
Step 先尝试一个短独占 freeze，统一预检所有源注册；遇到在途回调、
注册交接或冻结忙时返回 BUSY，不修改拥有边。预检通过后才摘除稳定
注册并消费取消注册权。映射 Data 和端点在 freeze 外归还。

BUSY、FAILED、READY 使用规范 XRT 完成描述符。全部实际计划引用
保留到所有职责完成；Combine 可以 READY，而仍在派发中的 Watch
继续 BUSY。重复 READY 不重复释放。mapper Data 的语义终结必须由
自己的节点负责，不能在 Drop 中补跑，也不能以丢弃输入代替语言清理。

## 验证范围

`check_future_combine_retirement.py` 验证实际消费的 XRT 与 xruntime
单头，并可核对完整源码对象后执行同一原生测试。用例包含144种
ALL／ANY／RACE、owned／raw 映射、重复源、真实外部根、TaskGroup
混合图、正反完成顺序和中止恢复，以及8次冻结 BUSY 和2个真实派发尾部。
独立检查器在计划加引用之前核对节点、强边和根；完成路径零分配并
平衡最终账本。规范源码另有 GCC、TCC、Linux 与 ASan／UBSan 批测。
完整源码通道还复验 array 与 variadic 聚合调用的确定性严格 C、JIT、
模块和移动 AOT，保留内嵌 NUL、UTF-8 字符串与重复输入的语言合同。

同步仅包含 future 公共声明和 future_combine 实现增量，Future 执行
与相对超时单位保持不变。旧 V1 准备协议及语言侧全局目录没有切换。
现有语言回调和 continuation 仍要求正常执行 catch／finally 的准备
职责；这些职责与新物理完成协议协调前，不能开启 TaskGroup 的全局
循环准入。53模块和正式产品发布验收仍继续，对象池最后处理。
