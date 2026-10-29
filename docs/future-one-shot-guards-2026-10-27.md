# Future 图与一次性执行验收修正

本批修正验收的调度前提、源码依赖和私有产物路径，不修改运行时或语言合同。

固定工作线程发布 Future 后，工作线程未必已经进入 await。Future 图用例现在先
通过原生 debug snapshot 等待真实 Watch 注册，再在冻结边界检查拥有图。活动尾部
只允许原子、可检查的状态拒绝并限时重试；首次成功的图仍必须严格匹配原有
6 个节点、9 条边和 3 个外部根。真实 opaque Watch 的拒绝及输出不变检查仍保留。

一次性执行的布尔结果遵循现有 `xlRunModuleMainUnlocked`：入口失败或实际清理
失败返回失败；BUSY 将清理交给受管退役队列，不把正常延迟退役改成入口错误。
测试现在严格检查正常入口的 42 退出码，以及初始化／main 的原始 AssertionError。
失去调用方句柄后仍要求模块留存，两个源完成后才允许全局析构和物理卸载。

两份 one-shot guard 使用同一份已核验的完整源码 cohort，去除旧运行时文件清单和
`-fwhole-program` 的符号删除。DLL 路径、生成头文件及实际 TCC 输入拥有独立目录。
TCC 的生成产物搜索根与 runtime 头文件根显式分开，避免读到旧 build/test 产物。
async-module 的 TCC guard 同步采用这一目录规则。

Windows 本批验收：

| 用例 | 实际范围 | 结果 |
| --- | --- | --- |
| Future 拥有图 | GCC O0、O2、TCC；每配置 700 个生成图与 1500 个 payload 用例 | 全通过 |
| one-shot DLL | 原有 7 种模式，各 25 次实际 DLL 卸载 | 175 次通过 |
| one-shot TCC | 原有 7 种模式，各 25 次 compile/relocate/MEM_FREE | 175 次通过 |
| async-module TCC | 原有 6 种模式，各 25 次 compile/relocate/MEM_FREE | 150 次通过 |

上述均保留逐事务的内存平衡、代码租约、调用与 finalizer 顺序检查。证据位于
`E:/xlang-validation/five-groups-20261027/p7-shipping/` 的
`future-graph-workers-r2`、`one-shot-workers-r2` 和
`one-shot-workers-r1/check_xlir_async_module_tcc`。第一轮 one-shot 的旧断言失败
仍保留；本批只接受明确列出的成功配置。独立重算见
`F:/xlang-validation/five-groups-20261027/p7-shipping/future-one-shot.proof.json`。

这些是完整保留原有用例规模的定点验收，不代表全部参数化 guard 或发布验收通过。
