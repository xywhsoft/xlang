# 封闭类 JSON/XLON 恢复切换

本批将封闭类、标量、可空字段和原生 Array 的 JSON/XLON parse/read
切换到驻留恢复事务。外部共享默认子对象只按声明的 clone 能力隔离；
没有能力、返回自身或仍共享的结果均 checked 拒绝，不修改原对象。
这不是 Open/List/Set/Dict 或完整标准库迁移完成的声明。

## 所有权与生成合同

Lowered XLIR 直接转移声明默认工厂返回的独立拥有值，恢复操作的目标
操作数为 CONSUME；文本仍借用。成功和失败都消费目标，不能先制造
borrowed LOAD 再 retain 成第二个语言拥有引用。验证器和 Cgen ABI
检查要求目标的真实生产者为匹配类型的 CLASS_DEFAULT_INVOKE。
STATUS_OUTPUT 的输出先置空，避免把未初始化 SSA 存储交给事务。

Cgen 只输出验证后的局部节点/字段表、声明默认工厂和精确 clone 调用
适配器，以及格式解析/文件路径适配。没有字段赋值算法、类型名注册表
或回溯源码推断；已切换根不再生成旧 assign/restore 类型辅助函数。
内部方法按已确定的 checked/no-raise ABI 调用，派生 copy/clone 复用
驻留 shallow/deep clone，不以结构相似猜测可复制性。

驻留事务记录真实拥有槽，在独立宿主读取边界和 Freeze 下认证全图。
clone、默认工厂、析构和错误清理不在 Freeze 内执行。隔离后的结果
再次认证，字段整批发布；失败撤销未发布结果，但不能撤销普通语言
初始化器/clone/析构已经产生的外部副作用。缺失字段保持声明默认值，
不为没有写入的共享对象执行多余 clone。

解析错误与语言错误分别保留独立结果位。只有确定格式域/代码的
语法、数字、XLON tag 拒绝及普通文件访问失败成为 nullable null；
OOM、预算、供应者、clone 和清理失败保持 checked 错误，诊断分配
失败也不能变成成功。源、种子和目标的退休参与同一 first-error 边界。
仍待迁移的族使用独立的消费目标适配器，保留原有赋值路径。

## 完整模块的迁移边界

目标结构封闭不代表其代码拥有图封闭。真实 JIT 的类租约会追踪模块
全局变量；Open 对象的 DynamicFields 仍含生成模块内的旧原生句柄
策略，不能作为无边叶节点接受，也不能在 Freeze 内调用该策略。

XLIR 新增 OPAQUE_MODULE_OWNER pending 位（128），从可达的拥有句柄
操作和显式原生策略重新推导。存在旧 opaque 策略的模块保留旧恢复
路径，包括其中结构封闭的根。验证器独立重算这个屏障；删除标志会
在 IR/C/头/产物计划输出前被拒绝。Cgen 不按类名或测试名决定回退。
这是明确的未迁移范围，不是其共享默认对象已经安全隔离的证明。

封闭类恢复的准入器与私有 List 图准入器拆成不同组件。普通类/Array
恢复不带入 List/Future；完整 List 组件则声明它实际使用的 Future
准入依赖。拒绝未知资源的规则没有放宽。

## 验收与证据

最终编译源码快照为 Windows
`E:/xlang-validation/stdlib53-b166-source-final3/frozen/demo6` 和 Linux
`/var/tmp/xlang-stdlib53-b166-source-final3/demo6`。完整构建分别为
254、253 个翻译单元，报告为 `stdlib53-b166-build-*-final3/build.json`。
编译器/runtime 对象为 O0；生成 C 与原生测试宿主分别执行 O0/O2。
仅沿用 TCC 工具核心，不以旧发布库替代本批实现。

语言最终报告使用 `stdlib53-b166-language-*-final4/results.json`，
显式 test-root 只补充封闭继承类用例，记录当前测试 SHA；编译源码
快照不修改。五个语言用例覆盖宽字段、独立字段实例、共享自定义
clone、派生 copy/clone、拒绝/原异常/恢复、嵌套 Array、NUL/Unicode、
可空值、文件读写，以及保留旧路径的混合构造模块。

语言门每端 573 阶段，其中 514 次为两条完整分配失败前缀，双端共
1,028 次尝试。分配账本平衡。两端各 31 阶段检查真实 API 自动绑定、
读取能力、拒绝顺序、零分配绑定以及独立 C11/C++17 头。
六项目标拥有权/模块屏障变异拒绝发生在输出前。

原生读取/声明 clone 集中回归每端 12 阶段，8 条完整失败前缀共
332 次；所选 Linux 原生闭包 O0/O2 ASan/UBSan 通过，不代表整个
runtime 或 JIT 代码都插桩。恢复 IR 专项每端 24 阶段，选定五个
恢复用例及包含字段的拒绝测试；明确不是整个 projection fixture 矩阵。
这四项双端门共 1,280 阶段，完整前缀共 1,360 次尝试。
对应报告为 `stdlib53-b166-{binding,native-read,plans}-*-final3/results.json`。

Windows `stdlib53-b166-artifact-win-final3/results.json` 中三个格式裁剪
用例的 Cgen、真实 JIT、AOT 构建及独立执行均成功。选定原生组件的
编译和生成 C 的编译无漏符号，实际原生程序集与计划一致，另一格式
及 List/Future 不带出。215 个驻留组件的 46,225 对依赖关系及实际
目标文件闭包检查通过。

但该报告整体 Complete=false：完整导出审计有 60 项登记差异，涉及
内部辅助符号（含新选入的私有 List 接口）和四项宿主清理接口。
保留原报告和失败，不计入已通过总门或最终通过报告的索引证明。
三个产物成功不代表发布 DLL/SO、全平台、VFS 或完整标准库验收。
开发候选失败及测试修改导致输入哈希失配的旧报告也不算最终验收。

## 剩余工作

Open/DynamicFields 的驻留拥有边描述、List/Set/Dict/通用载体恢复、
未知资源策略、冷路径性能与完整导出/发布总门继续。没有修改
canonical XRT，没有新增公开标准库范围；19 个 XRT 未决策组保持
不变，CRT 无未决策，对象池最后。无关 IDE、扩展库及源码覆盖层的
工作区改动未纳入提交。
