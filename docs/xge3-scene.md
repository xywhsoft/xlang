# Scene：原生场景栈与拥有式回调

此批补齐 Scene 的直接声明、真实栈引用、八类回调、模块代码寿命和 checked
退役。它是 XGE 全量接入中的一个批次，不代表异步加载、全部输入载荷、3D、
XUI/Document 或正式发布已经完成。最小学习示例见
[scene_stack.xl](../lib/bindings/xge/examples/scene_stack.xl)。

## 公开入口

| xlang | 原生入口 | 合同 |
| --- | --- | --- |
| createScene() | 冷路径 xl6Xge3SceneCreateOwned → xgeSceneCreateOwnedWithCode | 独立拥有的 Scene 与代码能力 |
| currentScene() | xgeSceneCurrentOwned | 空栈返回 null；成功返回真实共享引用 |
| Scene.share() | xgeSceneShareOwned | 创建另一个拥有视图 |
| Scene.entered / paused | xgeSceneEnteredGet / PausedGet | 创建线程 checked 输出 |
| Scene.activate / push / replace | xgeSceneSetOwned / PushOwned / ReplaceOwned | 栈持有独立的原生引用 |
| Scene.onEnter / onLeave / onPause / onResume | 对应 CallbackSetOwned | 可空、精确类型回调 |
| Scene.onDraw / onFree / onEvent / onUpdate | 对应 CallbackSetOwned | 可空、精确类型回调 |
| Scene.close() | 通用 ForeignView checked close | 关闭本视图，不隐式移除场景栈 |
| clearScenes / popScene / sceneCount | 对应原生栈操作 | 引擎创建线程调用 |
| setSceneUpdateStrategy / sceneUpdateStrategy | 原生调度设置/输出 | mode、正的有限 step、正的 maximum |
| dispatchSceneEvent(event) | xgeSceneDispatchEvent | 仅传递当前有效的 SceneEvent |

返回当前场景时，遇到显式 C 互操创建的 caller-owned 场景，原生返回 Unsupported，
不会把它伪装成拥有对象。Scene 的 close 与 onFree 是不同事件：onFree 表示一次
激活从栈中移除；最终对象析构只清理回调，不额外调用 onFree。错误线程的方法、
属性、close 和全局查询均由 checked 边界拒绝。

## 状态与错误

Push/Replace 在破坏原栈之前取得新场景的真实引用。失败的 Leave 保留原激活，
调用者修正回调后可以重试。移除项先从栈中摘下，再执行 Free；Free 报错也不会
复活已经摘除的记录。同一场景不能同时占两个栈槽，重复 Set 唯一当前项和
Replace 当前项为无操作。

更新/事件回调允许切换场景；栈、调度和回调的版本变化会停止本帧的旧调度，
避免继续访问已移除对象或执行旧 draw。生命周期回调不能递归切换栈，但可以
设置调度或分发事件；递归 frame 和活动回调内 Unit 均返回 Busy。Unit 的场景
清理失败时保留可重试的初始化状态。GPU 资源未释放时的 Busy 规则继续有效。

SceneEvent 是回调期只读视图，当前公开 type、param1/2、x/y、dx/dy、pointerId、
codepoint、time 十个标量字段。保存它不会延长事件存储寿命，回调返回后读取会
checked 报错。需要保存数据时复制相应基础值。原生 pData 的触摸/手柄等类型化
载荷仍属于后续 Input 批次，没有导出任意 void* 来绕开寿命检查。

## 拥有图与代码退役

原生栈持有 Scene；Scene 持有不可变回调节点；执行时另持回调快照和活动计数，
覆盖完整返回及清理尾部。通用拥有域暴露真实 Count/Trace、静止、准备、分步
finalize 和 consuming checked release，可以识别闭包 → Scene 的循环。

即使没有回调，Scene 也独立持有创建模块的代码能力。已用实际动态模块复现：
若只依赖回调租约，空 Scene 仍在原生栈中时模块就可能卸载。创建入口现在传递
独立代码 owner，并通过驻留的 xl6CodeLeaseHoldNativeV1/DropNativeV1 维护它；
最后 Drop 的原生返回尾部由既有退役标记保护。该能力保守地保留创建模块及其
依赖，并非只保留一个函数地址。

宿主必须先停止原生发布者：clearScenes、unit 后再执行 checked retirement drain。
Close module 不等同于强制销毁仍被原生引用的对象。不能在发布者仍需继续调用
期间用无限 drain 等待其自行消失。原始 C 的无 code-owner 工厂仍由 C 宿主保证
代码驻留；语言安全工厂不采用该省略合同。

跨线程最后释放由既有 ForeignView 创建线程队列交付，原始 C 调用则应在 Busy
且未 consumed 时保留释放义务并重试。创建线程退出/OS 线程 ID 回收的恢复方案
尚未在此批验收，不能据此宣称任意跨线程资源操作安全。

## 成本与证据

普通操作直接调用原生函数；Cgen 只投影参数、结果和通用生命周期。冷路径存在
一个代码能力准入 adapter，公开头内另有通用 policy/domain 的生命周期元数据。
没有逐方法 C 包装、操作码分发或动态值装箱。

Windows/Linux 完整 O2 候选下，暖机后创建一个空 Scene 的增量是 4 次堆分配、936 字节
请求量，包含原生对象、语言视图控制和代码能力记录；离开局部作用域后回到基线。
其中原生 owning 对象为 312 字节加 32 字节生命周期记录，共一次 344 字节分配；
原始 caller-owned C record 是 80 字节。这些数值不含分配器元数据、冷启动和回调
注册成本。原生带回调的 1,000 帧暖循环无新增堆分配，仍有引用、活动计数、版本
检查和 checked 边界；没有宣称相对手写 C 的零总开销或硬件性能。

专项包括原生状态机/真实 OpenGL 回归、八类回调与自清空、错误重试、OOM 前缀、
过期事件、错误线程、JIT/静态 AOT、实际模块 Close 后存活及停止发布后的循环回收
和 DLL 卸载。索引见
`dev/release-materials/provenance/xge-scene-20261110.json`。完整候选、配套 SDK 与
大日志保存在 F:/xlang-validation/xge-full-20261110/，正式 release/IDE 尚未替换。
