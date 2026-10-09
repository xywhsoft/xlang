# 动态绑定方法保活与模块读取证书

日期：2026-10-10。动态绑定方法现在使用常驻 closure 和 callable 的
统一生命协议，分别保留 receiver 与生成适配器的代码生命。模块输出
独立读取证书，真实宿主可在当前线程同步暂停的边界读取 owning slots，
但拒绝外线程执行、损坏证书和未知原生策略。JSON/XLON 的新恢复入口
尚未切换；本批不关闭完整 53 模块任务。

## 绑定方法的代码生命

旧 `xlir_bound_method` 只拥有 receiver。保存 callable 后，receiver
可以来自模块 B，而调用和参数适配代码属于模块 A。B 的对象生命不能
替代 A 的代码生命；旧生成 entry 和 drop 也没有独立执行登记。

生成环境现在使用 V2 的关闭捕获布局：receiver 是 XVALUE slot，
dispatcher 是不拥有资源的函数地址。`xl6ClosureBoxErasedV1` 复用
既有 resident box 算法，不另外实现引用计数、执行栈或图回收。闭包与
box 分别持有代码依赖，resident invoke 登记 adapter 和 closure 两个
实际 scope，resident drop 在生成调用返回后才释放最后代码引用。
没有最后释放后的生成代码返回尾部。

erased box 不绑定名义 callable identity；原 V1 typed Box 仍拒绝
identity 0。两者共用构造、回滚、invoke、trace 和 drop，只在身份绑定
处区别。失败不消费输入 closure。生成环境创建失败时由 V2 回滚；
成功发布后不再由 Cgen 的自定义 trace/drop 管理。

`ModuleAbi.boundMethodStorageAbi` 将物理存储与语义 member binding
分开。typed 为 NONE，lowered 为 resident closure layout，验证器
独立检查，文本与 JSON dump 都记录。真实可达绑定才选择三个 resident
符号和对应 closure provider；只有直接动态调用时不选择 erased box。
无普通 closure target 的独立夹具验证了 artifact 计划选择，并确认
没有选择日志、网络服务或语言 Thread provider。

## 模块读取边界

`ModuleAbi.readExecutionAbi` 在 typed 为 NONE，在具有初始化生命周期
的 lowered 模块为 tracked slots。其验证不替代已有的 host、export、
raw callback、class、closure、绑定方法、async、task、generator 与
slot 合同检查。

生成的 `xl_script_module_ownership_read_v1` 返回不可变 descriptor，
包含精确 size、ABI tag 和实际 `xl_script_module_ownership_trace`
地址。getter 不登记执行、不分配、不进入 mutation，可以在 Freeze
内调用。独立 ABI 声明头不包含完整 XRT，实现包的该头单独通过 C11
与 C++17 的 Wall/Wextra/Werror/pedantic 语法检查。

真实宿主验收在 Begin 后进入 Freeze，当前同步线程可读取实际模块
Count/Trace，保留所有真实入口及代码引用。外线程执行 checked 拒绝；
复制 read scope、错误 size/tag/trace 或缺失证书也拒绝。证书不认证
任意 native owning resource，不授权不受合同约束的原生写入。

## 双端验收

Windows 和 Linux 的完整当前源码编译器与运行时分别构建 251 和
250 个编译单元，无对象替换。链接人口为 O0；O0/O2 指宿主和生成 C
测试单元。动态 TCC 核仍是工具输入，不等于重建 shipping libraries。

六项门每端各 145 阶段，共 290 阶段，其中四次为预期基线断言。
native 宿主使用 C11 Wall/Wextra/Werror，无豁免；旧生成 C 和编译器
夹具的 warning 豁免逐项列入报告，不宣称全源零警告。

- 绑定方法门每端 37 阶段。两个真实 TCC 模块分别拥有适配器和 receiver；
  export 返回后直接原生调用、外线程同步停车、关闭两个模块后的调用
  和最后 resident 释放通过，没有独立 caller lease 掩盖缺陷。每执行
  覆盖 6 个连续 factory 分配失败前缀及 1 个 invocation 失败前缀，
  失败覆盖诊断分配并逐次核对状态与账本。旧提交在实际 A scope 缺失处
  断言，新实现通过。V1 identity 0 拒绝也保留。
- 既有绑定语义覆盖 118 个输出前合同变异、继承/重载、参数默认值、
  关键字和变参、NUL 字符串、nullable、结果传递回滚，以及每个 rich
  执行的 100 次模块循环和 300 次真实异步挂起。rich 使用 smoke 方式，
  不把旧 3 点故障注入或该执行的堆账本算作新 OOM 证明。
- raw 读取门每端 22 阶段，包括 49 个 module/read/storage/export/raw
  合同变异及既有 24 个 raw 合同变异。真实 getter 在 Freeze 内调用，
  真实计数/追踪、畸形证书及外线程拒绝通过。
- 宿主活动、导出事务和旧恢复语言回归每端分别 29、20、33 阶段；
  读取证书前后对照另各 4 阶段，旧产物因无证书拒绝。

两端保存 callable 的真实 TCC 测试最终都只保留工具缓存的一个分配，
核对其精确地址和大小，其他增量分配全部释放；无 double/invalid free
或 use-after-free 账本事件。这不是 sanitizer 证明，本批没有新增
ASan/UBSan 执行。

## 证据与未完成项

源码快照为 Windows
`E:/xlang-validation/stdlib53-b164-source-dev2/frozen/demo6` 和 Linux
`/var/tmp/xlang-stdlib53-b164-source-dev2/demo6`；完整构建报告分别在
`stdlib53-b164-build-win-dev2/build.json` 和
`xlang-stdlib53-b164-build-linux-dev2/build.json`。编译源码不改变；
修正测试入口声明和非法 var export 后的测试输入使用显式路径覆盖并
记录 SHA，最终 Git index 与报告再次逐项核验。

最终门报告位于对应平台的 `stdlib53-b164-bound-*-dev4`、
`stdlib53-b164-{raw,activity,export,language}-*-final1` 以及
`stdlib53-b164-red-green-*-final2`；Linux 目录加 `xlang-` 前缀。
早期候选和失败报告保留，不计入 290 阶段。

独立 AOT 的源码驱动尝试在 Windows 因缺少当前内嵌 compiler-source
VFS 停止，在 Linux 因驱动没有 exe 模式停止，保留于 bound dev3。
最终依赖计划测试仅证明选择与裁剪元数据，不冒充打包 AOT 执行。
发布库/VFS/独立 JIT 与 AOT、完整产品裁剪及 53 模块总门仍须验收。

下一步仍是生成声明 clone thunk，接通 native restore 节点/边/精确
类型及实际 JSON/XLON 入口；外部共享默认子对象只按声明 clone 隔离，
否则 checked 拒绝。未知资源族不放行。其他语言容器载体、开放/泛型
类、SlotMap/WaitQueue/Channel/OrderedMap 与冷路径性能继续，19 个
未决策组和对象池最后不变。本批不修改 canonical XRT。
