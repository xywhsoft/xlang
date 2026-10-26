# 对象逃逸测试的现行数组和拥有目录合同

保留原有四类完整工作负载：guarded 10×25、automatic 10×25，以及
retirement outcome 的 O0/O2 各 4×25，总计 28 配置、700 次真实 TCC 生命周期。
独立对象、子对象、全局别名、克隆容器、闭包调用、析构错误、代码页实际释放
与内存账本断言均未删减。Cgen 和运行时实现没有为测试新增兼容行为。

旧测试有两类实际接口漂移：

- 旧拥有权解析函数已删除；现在通过 V3 和实际 module 的拥有目录解析，
  不使用 NULL 上下文或恢复旧解析器。
- 语言数组已经采用 typed native storage。旧测试却直接使用 wire ARRAY 的
  borrowed get 和通用 opaque-handle clone，无法取出子对象，也没有真正复制
  数组。现在使用已有的 `xl6ArrayValueReadCopyV1` 和 `xl6ValueCloneV1`，分别
  取得拥有值和独立存储，并保留非同一 shell、存活和析构计数断言。

四个宿主均链接完整的同一 source cohort；实际 host API/registrar 的完整
依赖不再用手工驻留源码列表维护，也不使用 `-fwhole-program` 隐藏未解析
依赖。两个旧源内嵌 host 编译单元不重复链接，正式源 cohort 中其他对象
均由响应文件传给 GCC。无 release-provider 或临时源对象补洞。

证据根：`F:/xlang-validation/five-groups-20261027/p7-shipping/`。
`object-escape-r1` 保留旧 wire 假设导致的三种失败；`object-escape-r2` 的
28 配置全部记录为真实退出 0，700 回合通过，生成 C/H/IR 重复输出一致，
输入、日志、对象和程序由独立 reader 核对。外层 exec 会话结束后其控制器
退出码未保留，因此只对这批实际场景及独立完成检查作结论，不假称已取得
完整参数化总门的退出回执。后续 74-worker 总门仍须重新运行并保留回执。
工作区 DLL 后来被其他构建替换；实际执行的相邻归档 DLL 与最初输入、报告
末尾记录的哈希完全一致。独立核对绑定该实际提供者，不拿新 DLL 代替它，
也不把这项历史场景证明写成新 DLL 验收。
