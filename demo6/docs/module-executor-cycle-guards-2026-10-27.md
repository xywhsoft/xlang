# 模块执行器与循环图门禁

本批仅迁移测试及其驱动，不改产品运行时。

执行器门禁过去把一次非阻塞 freeze 准入拒绝误判为 preflight 回调已经
运行并返回 BUSY／FAILED。工作线程进入停泊时，这个假设在 O2 下会失败。
测试现在有界等待首次实际 callback，仍精确检查首次 BUSY／FAILED、
错误锁存、第二次 callback 成功，以及混合 await→task 场景中两个服务的
非破坏 preflight。没有把 BUSY 当作成功，没有减少重试或卸载人口。

循环图门禁改用生产正在使用的 V6 目录准入、发现和 completion 合同，
传递真实 module context，移除对已删除 resolver 函数的调用。精确图计数
改为 6 节点、7 边：`array<int>` 的 11、22 已在 typed storage 中内联保存，
不再产生两个 boxed xvalue 节点及对应两条拥有边。没有放宽成计数范围。

完整 Windows 源码 cohort 验证：

- 执行器 8 配置通过：TCC／DLL × O0/O2 及四个混合配置。
  主配置各 182 次真实卸载、112 次外部根检查、40 次 cleanup-await 根、
  30 次畸形合同拒绝、15 次 readiness retry、32 个预算及 13 次真实 OOM；
  混合配置各 50 次真实卸载、25 次开放 owner 完成、25 次关闭 owner 拒绝。
- 循环图全部 5 配置通过：三个原生闭包布局配置及模块 O0/O2。
  模块配置各保留 221 次原始 TCC 生命周期和额外 50 次 preparation snapshot
  分配失败，验证图保持、无语义错误锁存、成功重试、MEM_FREE 和完整内存平衡。

独立复核 8324 个输入、工具、产物与日志哈希：
`F:/xlang-validation/five-groups-20261027/p7-shipping/module-guards-r2.proof.json`。
普通发布产品、Linux 和其余参数化门禁仍按各自证据评估，不由本批推断通过。
