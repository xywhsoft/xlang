# 旧拥有权宿主改用真实 IR 依赖计划

完整参数化批次中的一些旧 C 宿主仍手工列出早期运行时源。
新增声明头路径缺失，随后又漏掉当前生成代码实际调用的错误、数组和动态值
组件。现在合并两种明确来源：宿主自己的显式根，以及同一完整源码编译器
对夹具生成的已验证、已 lowering 的 `XL6_ArtifactRuntimePlan`。
合并后的驻留源再经原有 artifact registry 关闭依赖；不扫描生成 C、按名称
猜测功能，或用一份越来越长的源文件补丁表追赶 Cgen。

`xlir_fixture_runtime_plan.c` 使用实际源码对象，仅替换驱动入口，重复构建计划
并比较根、组件、源和原生模块。helper 重校验整个源码对象集合，拒绝
runtime-only 编译器角色。任一夹具失败均不发布部分驻留闭包。
组件原生依赖保留在 `NativeModules`；每个完整夹具的原生计划同时保留于
`FixturePlans`，本组宿主仍使用原来全量 source-XRT 对象，不冒充最小原生裁剪门。
编译改走已有 response/log 助手，保留真实退出码和完整 stderr。

## 同步更新的测试语义

- 捕获图：原生 `Array<int>` 使用未装箱存储，不再包含旧 wire Array 的独立
  整数 Value 节点；类实例还拥有真实的 runtime-type 字段。保留逐模式精确
  节点/边数。原始 XRT handle clone 的同一载体语义与语言 Array 的独立
  shallow clone 分别验证；后者保留两个 callable 元素的实际引用，仍检查
  原容器不被外部 clone 保活、共享捕获修改可见和每轮内存精确平衡。
- Future 图：使用统一驻留 frame launch 的当前终结图，保留 700 个生成图、
  真正 opaque Watch 拒绝、别名、调用、原始错误与取消结果；字符串结果按
  `xstrview.Data/Size` 检查，去掉测试中的旧 `char**` 假设。
- 生成器图：预处理调度拦截移到实际调用 Reserve 的驻留 frame 编译单元，
  直接 include 原实现，保持原队列控制范围。初始 activation 现在也入队，
  测试先显式交付它，再检查原先的首个 await 图。增加 frame/producer 两个
  真实节点之间各一条强边的结构检查，不把它们藏成外部根或旧帧链。
- 聚合 Future：恢复完整真实依赖，其余原有合同、3500 个生成事务和公开
  literal/array/void-All 检查保持。

## 本批结果与边界

GCC O0/O2 与所选源码 TCC 共 15 个编译/运行配置全部通过：捕获 3、Future 6、
聚合 Future 3、生成器 3。生成器每配置仍执行 2400 次生命周期、1000 次
异步事务、2000 次真实挂起及 16000 次检查探针，不降低原有故障数量断言。
两端普通产品尚未因本次测试宿主修改而重建；这些是 Windows 冻结 r5 对象
及明确测试宿主/调度边界迁移的源码门，不是最终发布或 74 项总门完成证明。

独立复核 7084 个输入、日志、源码对象、SDK 文件与产物；回执为
`F:/xlang-validation/five-groups-20261027/p7-shipping/resident-workers.proof.json`。
实际证据在同名 E 盘根：捕获/Future 为 `resident-workers-fix-r4`，
聚合为 `resident-workers-fix-r2`，生成器为 `resident-workers-fix-r6`。
原有失败和只用于发现布局的外部诊断均保留，诊断中临时抑制的断言不属于验收。
PowerShell 7 的无效第二夹具/runtime-only 角色、PowerShell 5 的真实正例与
语义拒绝也分别通过，均不发布部分闭包。
