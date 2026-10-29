# Future 退役门迁移到现行拥有与调度合同

旧门使用已移除的 `xlModuleResolveOwnershipV2` 和私有阻塞池函数，并手工
列举不再完整的驻留依赖。修复测试控制面，不恢复旧 API，不修改生产调度
或退役语义，也不减少原测试工作负载。

- 用 V3 resolver 和实际 module 的不可变拥有目录准入原生节点，同时取得
  completion 协议；不能用 NULL 上下文绕开现行目录。
- 物理图测试从原来的直接组件根查询正式 artifact registry，得到 14 个
  逻辑单元。错误 TLS 由已审计原生组合单元提供，另编译 13 个单元；补齐
  数组迭代器和 V2 capture 的真实传递依赖，不引入替代 provider。
- TCC 夹具在提交全部根工作后，显式、有 5 秒期限地等待固定执行器 idle，
  然后调用当前生成模块的 checked quiesce。该观察包括物理释放尾部；不是
  重新承诺生产 teardown 会阻塞任意未完成的逻辑等待，也不是删掉等待。

Windows 原门实际通过 15 个配置：GCC O0/O2/TCC 三份物理图测试，以及
GCC O0/O2 宿主各六种实际 TCC 退役场景。每个物理配置保留 225 图、289 次
终结、189 producer close、50 次观察拒绝和 64 个故障预算；12 个 TCC 配置
各 25 次生命周期，总共 300 次，保留对象、闭包、嵌套结果与外部别名存活
断言和内存平衡检查。三轮日志全部保留，最终为 `future-retirement-r3`。
源码 cohort 和实际使用的相邻 TCC provider DLL 均保留；工作区 release DLL
后来被其他构建替换时，只以归档副本与原始输入的完全相同哈希绑定本次结果，
不会把旧副本的通过冒称为新 DLL 的验收。

同批修正 string_join 的绝对 WorkDirectory 计算。原有路径 AST 检查增加
这一种实际赋值形式，PS5、PS7 各 11 个 worker／77 项均通过，包含跨盘、
空格、Unicode、UNC 纯路径计算和 NUL 拒绝。没有访问 UNC，也不代表完整
string_join worker 已通过。

证据根为 `F:/xlang-validation/five-groups-20261027/p7-shipping/`。
这是一个原有 Windows 源码退役门及路径前置的修复；不能替代普通产品、
Linux、74-worker 总门、package 依赖分支或五组联合封板。未完成项仍开放。
