# 受管原生视图实现合同

此文记录正在实施的内部合同，不代表这些 API 已完成或可发布。现有 `@lifetime("caller")` 的显式 C 责任边界不变。

原生实例只创建一次控制记录；语言值保存精确类型的控制记录指针。方法准备得到已准入的原生指针，C 调用仍使用权威头中的物理类型。没有 xvalue 属性查找、动态装箱或操作码分发。首次登记、拥有引用、失效检查、代码驻留和跨 C 回调保护是实际成本，必须与相同安全合同的手写 C 分开核算，不能冒充裸 C 指针的零成本。

Root 控制记录唯一拥有 C payload，Child 控制记录借用其中的实例并强持有 Root。Root 的私有目录为已经暴露的实例复用控制记录，不在每次 getter 返回时新建包装。Root 对已登记 Child 的实际 native pin、Child 对 Root 的实际拥有引用、代码 lease 和捕获闭包均进入拥有图。只有不能独立共享、由同一 Root 唯一销毁的 C 内部内存属于该 payload；独立引用计数对象仍是独立的图节点，不能扁平化。

XUI 的 context 全实例登记、弱销毁通知和 `xuiCallScopeBegin/End` 已分别验证。弱通知立即清除控制记录的原生可访问状态。通知本身只做状态及待清理队列转换；不能在 XGE 的通知栈上丢掉最后的模块／DLL 代码引用。实际 native pin 和代码依赖要等完整原生调用返回后，由驻留运行时释放。回调入口／退出还要记录 foreign callback 活动，回调返回点本身不执行 drain；外部 C 驱动在整个原生调用返回后显式 drain。

调用作用域通过 Root 的原生 scope hook 保证物理存储。Cgen 提供权威类型的 scope 记录，不能保留任意容量的字节槽。作用域进入拒绝错误线程、关闭或退役状态；同线程回调重入允许嵌套。控制记录／代码的实际 pin 活到原生 CallEnd 返回之后，语义调用不跨越 XRT mutation 或全局锁。

语言 owning copy／drop 与原生 binding pin／collector pin 分开计账：都是真实引用，但只有语言 owning slots 影响默认关闭准入。没有语言拥有值后，Root 请求关闭；错误线程上只排入可检查、可在创建线程重试的清理记录。原生 Destroy 失败且未释放对象时保持可观察的原生拥有权，不能无条件丢弃 raw 或在错误线程析构。循环由完整拥有图处理，不能从估算引用数中减去所谓内部引用。

工厂先取得物理状态／输出，再登记受管结果，然后清理参数；登记失败时仍由现行原生 rollback 接管。参数清理可能关闭 parent／context，失效通知必须覆盖此时的结果。错误路径只回滚仍存活且仍归本次事务的输出；不能对已由 context 销毁的原始地址再次调用 destructor。成功准入、准备、结果登记和清理效果分别进入 XLIR。

捕获回调保存已有的类型化 Closure 和 userdata；原生入口直接调用匹配的类型化 entry。注册、替换、失效和活动调用分别拥有真实引用；替换不能释放仍在执行的旧记录。原生持有槽逐个追踪；错误按 checked 规则记录，C 栈不传播语言异常。不得把拥有闭包的记录登记为 native leaf。

后续顺序：完成控制记录／目录／待清理队列及图准入，接入捕获和存储 schema，然后把生命周期与调用准备降入 XLIR；再完成工厂结果保护、文本物化、动态连续 span、效果合同和包／SDK／IDE 的一次迁移。每组先批量发现和修复，再统一验证。最终产品和发布索引只在完整接入验收后更新。

内部运行时检查点：`xl6_foreign_view.c` 已实现创建线程约束、实例目录、引用和 native pin、失效与延迟清理、调用作用域、checked 关闭／重试、回调栈标记及独立拥有域。`Release` 返回独立的成功标志，保留无诊断分配失败；void drop 适配器必须传播失败位。这个组件暂未加入语言类型、捕获 schema、发布 SDK 或生成程序的运行时选择，不能把此检查点当成可用的受管声明语法。

Windows O0/O2 的 8 组独立测试及合并测试均通过；Linux 对这两份新运行时源码启用 ASan/UBSan/泄漏检查，也全部通过。共享 XRT／基础运行库未由这轮 sanitizer 重新编译，不能扩大该检查的覆盖声明。验证包括自引用与 Child→Root 循环、错误线程最后释放、native 通知栈上的代码依赖保留、空诊断首错、6 个分配失败位置和零分配重复曝光。

完整默认 XUI 引擎与发布 `xl.dll`／`xlc.dll` 分别验证通过，包含同一实例复用、native 侧销毁、64 次控件槽位复用、context 关闭后旧值拒绝以及全部分配恢复。该夹具只创建没有持有值模型／语言回调的 base widget；它的空 TraceRaw 不能作为完整 XUI context 的拥有策略。接入其余控件模型、捕获 schema 和所有 native→语言适配器后，才能启用语言和 SDK 接口。

捕获检查点：新增显式 native graph profile、Capture V3 和 Closure V4；沿用紧凑的现有物理 slot 布局，通过独立版本入口准入有拥有边的存储。graph profile 的较大、带 tag 前缀刻意不满足旧 leaf 合同，旧 capture／cell／Future 入口会拒绝，不能跳过追踪。通用捕获引擎不依赖 View／Map，具体视图 profile 单独发布，后续组件选择按需引入。

这组 Windows O0/O2 与 Linux 新源码 ASan/UBSan 共 18 个阶段分别通过，包含 mixed string/view 清理、稀疏 live flag、重叠元数据／错误 schema 拒绝、空诊断首错、2 个闭包创建分配失败位置，以及 native payload→closure→view 的完整循环回收。类型化 closure entry 经真实 Capture V3 环境调用，分配／释放／realloc 计数均不变化；该结果不是生成后的 C-native 捕获回调性能验收。共享 cell、Future、类字段及 native owned callback 槽仍在继续接入，语言语法尚未开放。

共享存储检查点：同一个 opaque Cell 核心增加显式 graph strategy，与 leaf strategy 共用一个指针槽；普通 Cell 的布局没有为此增大。新建和交换仍采用现有 consuming transfer 合同，拥有图逐项读取真正的 native owning slot，清理先移出 payload、退出 mutation，再执行 checked destructor。已有 Cell capture kind 可以捕获它，无须另加装箱层。Future 使用独立的 graph payload policy，验证精确 profile 身份，并在完成失败时保留生产者的 payload 拥有权。两个 checked 返回入口不把失败预先传播穿过上层 catch；void 清理适配器传播独立失败位。

`check_foreign_view_storage.py` 的 Windows、Linux 检查各 40 个阶段通过，包括 cell→closure→view 与 Future→view 循环、零分配的 cell exchange／Future Copy、两个 cell 与一个 payload 创建分配失败位置、复制失败回滚、重叠拒绝，以及空诊断首错不被后续清理错误覆盖。这是校验已有源码构建对象后替换修改单元的增量内部验收；修改单元分别用 O0/O2，Linux 对这些单元启用 ASan/UBSan/泄漏检查。复用的基础对象不由此变成 O2 或 sanitizer 构建，最终仍需完整源码验收。语言、类字段、原生 callback 槽和产品接线尚未完成。

执行上下文检查点：私有实现原先的 C TLS 已移除，统一使用 Windows FLS／POSIX key。Windows 回调标记与物理调用登记按 fiber 隔离；物理调用的 intrusive stack token 允许同一上下文内非 LIFO 结束，但拒绝复制 token、跨线程／fiber 结束及借用另一个 fiber 正在调用的根。drain 的防重入状态也按执行上下文保存，遍历 epoch 使用全局原子序列。新运行时没有依赖 TCC PE 不支持的 C TLS 重定位。

OS key 初始化和首次绑定是冷路径，不能称作绝对零成本；稳定路径有 OS 槽访问、token 连接和失效检查，没有语言堆分配。Windows GCC O0/O2、真实 TCC JIT 和链接实际宿主 DLL 的 TCC 独立程序均通过线程／fiber 专项；既有 view、capture、cell／Future 图和真实宿主 XUI 夹具一并通过。Linux 新单元 sanitizer 检查覆盖线程隔离和存储图；POSIX 测试不声称验证 Windows fiber。捕获回调和类字段仍需实际接入，不能由该底座检查推定已经完成。

驻留回调检查点：class finalizer 的执行／context 释放、boxed closure 的调用／释放及两个原生 closure 清理入口已登记 foreign callback 边界。finalizer context 先从自身 mutation 中移出，再执行任意用户清理；边界退出本身仍不 drain。Windows 与 Linux 修改单元 O0/O2（Linux sanitizer）各 42 个阶段通过，覆盖 native 通知栈期间保留代码、无诊断析构失败、关闭请求延迟至完整 native 返回以及 native 清理回调实际释放其捕获值。

十个私有组件已进入按需构建和精确符号表，通用 capture／cell／Future graph 能力不依赖 View 目录。Windows 1,407 单元源码构建完成；新组件进入独立程序时暴露了旧预编译 VFS 缺对象，现用 1,555 个当前 TCC 对象和隔离的新 VFS 提供方验收。编译器 driver／对象保持该完整源码构建的原字节；隔离 DLL 只运行 TinyCC／VFS，没有第二份正在执行的 xlang／XRT 实现，预编译目标对象仍以资源保存。发布目录没有因此更新。

更新资源后的声明式接入 11 组全部通过；旧 native cell、native Future 专项分别通过 510、391 个阶段，包含独立 O0/O2 提供方、故障前缀、错误实现负控、JIT／模块／独立程序。可卸载 finalizer 夹具在两个优化等级分别验证 250 个真实 DLL 生命周期。验证工具同时改走长命令响应文件、复用隔离 SDK，不在仓库根目录重建源码快照。上述是底座接入检查点；XUI retained callback 槽、类字段、完整受管声明与 IR、独立 SDK 头、工厂保护／文本／span／效果合同和产品迁移仍未完成。

retained userdata 检查点：驻留 `NativeCallback V1` 同时拥有类型化 Closure、独立适配器 CodeLease 和隐式 Root 控制 pin。隐式 pin 是实际图边，但不增加语言拥有值计数；普通绑定不会自行阻止 context 默认关闭。用户真正捕获的 context 仍是语言拥有值。原生发布方在调用前取得独立 snapshot 引用，在整个类型化 thunk 返回后才 Drop，避免 resident Leave 返回尾部的适配器 DLL 提前卸载。替换先发布新槽，再在 mutation 外清理旧引用；清理失败不能伪装为替换未发生。

Windows 和 Linux 修改单元 O0/O2 各 58 阶段通过，Linux 对这些修改单元启用 ASan/UBSan/泄漏检查，复用核心未重新插桩。每个优化等级分别跑过 50 对实际可卸载 body／adapter 共享库；回调内部删除原生槽后，两份库仍驻留到 thunk 完整返回。还覆盖 checked 空诊断失败、首错保留、错误线程释放后由创建线程 drain、循环图和两个创建 OOM 位置。重复嵌套调用没有 XRT 分配、释放或 realloc，但仍有 RC、代码活动和执行上下文登记，不能称整个绑定零成本。

View 的公开声明头移除了源树 XRT 依赖；拥有图 Count／Trace 和独立 Read 准入在检查 native payload 前拒绝错误线程。上述仅为私有组件和测试合同；XUI 原生 retained 槽、完整 payload 图、语言 schema／IR 及发布 SDK 仍需后续接入。

原生事件组合检查点：XGE `9ed440a4` 提供主事件／分类事件 owned 槽与完整返回尾部 snapshot，`b0533295` 将 64 位主事件拥有元数据由 1,568 字节降到 40 字节；48 项分类表仅在确实注册 owned handler 时另行分配。普通 C 借用入口不创建元数据；owned 分类表复用，主事件／分类事件稳定调用均无 XRT 堆分配。额外分类表意味着其首次绑定多一次冷分配，不能把此优化描述成所有绑定都更便宜。

View 描述符现在要求受观察 Root 提供独立 native Busy 探针，防止外部 C 事件循环活动尚未进入 resident active 计数时过早读取 payload。真实宿主 XUI base widget＋owned event 的组合已验证捕获视图、回调内解绑／关闭、空诊断失败及 Context→event→Closure→Widget→Context 循环。该 trace 只覆盖夹具实际创建的 base widget/event；完整控件模型与其他回调仍不能据此获得准入。

当前 1,408 单元源码构建及 1,556 个预编译对象完成；隔离 VFS 的驻留 API 连接已通过实际语言声明发起的 JIT／模块／独立程序，独立复制的头在 C/C++ 中通过语法检查。八项功能和产品迁移仍未全部完成，发布目录未由本批更新。

普通类存储检查点：`ForeignViewValue V1` 在真实 `xvalue` 字段边界分配一个 Handle 外壳，持有原有控制指针；没有第二份原生 payload 或控制记录。类型化局部、闭包捕获和 C 方法调用均不走此装箱入口。Box／Unbox 先认证实际 resident Ops／Trace，再检查声明的存储身份，不能由一个 TypeId 授权未知 Handle 的回调。字段复制、COW、深复制仍引用同一原生实例；关闭后的视图可复制，访问原生状态仍需准入。普通类→Handle→View→普通类的完整循环已回收，掉最后一个字段引用时的默认关闭由完整 native 调用之外的 checked drain 执行。

`BorrowRoot V1` 对 `Widget.context` 一类结果要求原生返回值就是当前调用的同一个存活 Root，直接取得已有 Root 控制记录的语言引用，不再创建一个 Child 记录来包装相同 context。正确路径在分配失败注入下仍零分配；错误线程／错误上下文及错误原生结果被拒绝。

本阶段 Windows 与 Linux 修改单元 O0/O2 各 46 阶段通过，Linux 修改单元有 ASan/UBSan/泄漏检查；包括真实 resident Class 字段、clone/COW/deep clone、关闭值、无诊断清理重试、OOM、未知 Ops 拒绝及输入诊断保留。随后 1,409 单元源码构建和 1,557 个 TCC 对象完成，当前 VFS 的 JIT／模块／独立程序与独立 SDK 头 7 阶段、声明式基础 11 组通过。这仍是私有物理存储能力；语言字段／容器 schema、受管类型和 XLIR 尚未开放，不能将本阶段视作产品已支持受管视图类字段。
