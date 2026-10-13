# 六项专用宿主夹具的真实产品验收

原三组批量清单中需要专用宿主的六项夹具，现已在 Windows 和 Linux
获得完整执行证据。每端通过 309 个阶段、3364 次计量语言生命周期，
保留全部源断言、优化级别和退役次数，没有把需要宿主的条目直接
改为普通运行通过。完整 53 模块函数覆盖和发布总门仍继续。

## 已覆盖的夹具

| 夹具 | 每端组合 | 每端计量生命周期 | 验证内容 |
| --- | ---: | ---: | --- |
| `stdlib_stack_ownership` | 16 | 400 | 自环、外部对象／容器根、深复制根、字段完整的析构和退役 |
| `stdlib_tree_ownership` | 64 | 768 | 五类对象图、外部根、一次性析构和内存平衡 |
| `stdlib_concurrent_queue_ownership` | 76 | 1900 | 三种并发队列及 Future、数组、闭包图，外部根和退役 |
| `stdlib_concurrent_queue_continuation` | 48 | 96 | 三次 await，成功／拒绝／取消／关闭，ready／pending Watch／pending Commit |
| `stdlib_stack_clear_error` | 4 | 100 | 完成全部 Drop，保留首个 RangeError，finally 和幂等清理 |
| `stdlib_stack_clear_error_continuation` | 4 | 100 | async 和 generator 的捕获、错误传播与清理 |

所有组合覆盖两个真实共享库和 GCC O0/O2。生命周期在预热后计量，
核对存活分配、字节数、分配与释放次数、非法释放、重复释放；相关
宿主还核对 use-after-free 计数。表格不将预热或额外 CLI/AOT 运行
计入生命周期总数。

## 宿主接线和标准库合同

Stack 宿主新增通过运行版 API 使用共享库内嵌 VFS 的路径，与既有
Tree／Queue 宿主一致；原有编译器 API 路径保留。新 clear 宿主通过
one-shot 模块入口执行初始化、main 和终结，不再先手动激活同一
模块。已有 clear 回调测试实现同时支持产品保留声明 SDK 和原有
源码测试路径，没有替换容器、所有权适配器、错误桥或生成的 C。

四种图／续体门禁共 279 阶段、204 种完整组合。Promise 宿主完成真实
Promise，并在实际 Watch 或 Commit 之后交付结果；仅调用点的测试
时序拦截有明确标记，不以假的 Future 通知或 provider 替换为证据。

clear 门禁另有 30 阶段，覆盖 lowered IR、严格 C、两个共享库宿主、
真实 CLI JIT 和两份单独移走后执行的 AOT 程序。AOT/JIT 的附加 C
源仅负责注入测试回调，其声明来自产品自己的 VFS SDK。标准库的
原生实现不依赖外部源码或声明目录回退。

本批没有修改标准库生产实现，也不新增公开 API。生产源码与上一批
发布候选逐项保持一致，复用已固定的真实构建产物，不借用旧 TCC
单对象替换或未验证的 DLL。

## 覆盖汇总和拒绝门禁

汇总工具验证完整模式矩阵、重复／缺失组合、夹具和产品哈希、实际
生命周期次数、所有执行阶段及输入不变性。五类损坏报告分别被拒绝：
缺失 pending Commit、夹具被替换、少计生命周期、产品被替换以及
失败阶段伪装。定向 `--family` 子集只用于发现问题，不能代替正式
完整报告；六项夹具必须同时满足要求才能生成覆盖证据。

正式证据为 `E:/xlang-validation/stdlib53-b259-graph-win-final1`、
`clear-win-final1`、`hosted-{win,linux}-final1.json`，及 Linux 原生
目录 `/var/tmp/xlang-stdlib53-b259-{graph,clear}-linux-final1`。
早期发现运行、定向子集和失败宿主接线不计为正式通过证据。

## 剩余验收

这六项不再缺少宿主执行证据，但不据此宣称整个旧 301 项清单或完整
53 模块全函数已通过本批。原清单另外两项平台／产品要求仍需专门
验证，更广的真实产品回归也需汇总。

独立 borrowed 原生 SSA、工作线程和对象图 OOM、模块真实卸载与
错误压力、全函数覆盖及更广的发布总门继续。共享运行时未做完整
sanitizer 插桩，图退役门禁不代表独立 AOT 图退役或模块取消映射。
CRT 未决策为零，XRT 其余 19 组未决策保持，对象池最后。
