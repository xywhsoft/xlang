# 模块 preparation 门禁收尾

模块准备测试曾依赖“async 自动创建阻塞任务池”和“当前 Cgen 仍导出 V3”
两项已退出的合同。本批让语言 fixture 实际提交并等待一个普通 task，显式
创建任务池；原有 async 继续使用独立就绪执行器。原先的阻塞任务、自调用、
外部 Future alias、析构和错误测试人口均保持，不凭空制造服务句柄。

测试 artifact 对当前 V4 输出作显式 V2／V3 适配：V2 仍执行真实任务池
drain，V3 转交实际 V4 的非破坏 preflight。Cgen 没有增加旧协议输出、兼容
分支或运行时兜底。运行脚本固定实际 artifact 路径，避免私有验收误用旧文件。

缺失高版本协议时，有效 V1 fallback 仍可能发现有外部代码 guard 的真实图。
观察到的暂态为 phase 5、一个 snapshot pin、未清空图、无语义 callback；
工作线程停泊可能令撤销 freeze 暂时不可得。测试现有界等待该快照真正撤销，
然后仍精确要求零 preparation、零 forbidden callback、零计划，再执行原有
显式 legacy teardown。不把非阻塞延期当作成功或语义失败。

Windows `module-prepare-r4` 全部 72 配置通过：两种协议 × O0/O2 × 18 模式，
每配置保留 25 次真实 TCC 生命周期和 75 次内部 finalizer，共 1800 次
生命周期、5400 次 finalizer。BUSY／诊断独立失败重试、环境错误优先级、
跨线程锁／freeze、自调用、缺失／畸形协议、scope owner、MEM_FREE 与精确
内存平衡均通过；C／头文件／IR 重复产物一致。

独立重新计算 9179 项输入、工具、产物和日志哈希：
`F:/xlang-validation/five-groups-20261027/p7-shipping/module-prepare-r1.proof.json`。
本批修改只有测试及其驱动；不据此宣称 Linux 对应人口或全体参数化门禁完成。
