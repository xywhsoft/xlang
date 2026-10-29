# bytes 集合遍历合同

`array<bytes>`、`list<bytes>`、`set<bytes>`、`dict<bytes>` 及四种 `bytes?`
元素版本使用统一的拥有投影路径。此前分类器仅允许 SlotMap 使用已有的 bytes
遍历 ABI，普通容器也缺少该 ABI 的 lowering；本批补齐这两个入口。

迭代器仍持有原生快照。每次观察先借用游标元素，再按已解析 TypeId 取得精确
bytes／nullable bytes 拥有值；迭代出口复用现有反向释放合同。修改迭代值不会
修改原容器或 Set 的键。没有新增 Cgen 分支、RuntimeOp 或适配包装函数。

用例涵盖空 bytes、内嵌 NUL、0xFF、稀疏键、可空元素、单／双目标、弃用目标、
continue、break、return、throw／finally 以及复制后的独立修改。

Windows 和 Linux 分别从固定候选完整重编译 1,296／1,295 个源单元；两端均通过
10 个拥有遍历循环、50 个 C／头文件输出前的拒绝变异、确定性输出、JIT 和
实际 AOT 构建／执行。每端 GCC O0/O2 的严格 C 执行都穷举 0—219 的实际分配
失败，220 为首个未触发预算；四个通道共 880 次失败，内存账本均平衡。

候选基于 `cbb5f5ee8`，本批提交只重放已测试的遍历差异，保留同期进入主线的
其他 IR 更新；合并后的最终联合验收仍须执行。构造期明确赋值是随后批准的
独立开发项，其未完成修改没有纳入此候选。正常
发布产品、74 个工作器及长期任务的其余门禁仍需验收。本批证据见
`E:/xlang-validation/five-groups-20261027/p7-shipping/bytes-proof-r99/results.json`。
