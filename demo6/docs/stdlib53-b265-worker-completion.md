# 工作线程完成与分配失败清理验收

2026-10-14。当前统一 IR 的六类真实 task worker 完成、取消、析构报错和
Future 丢弃场景，在 Windows O0/O2、Linux O0/O2 及所选闭包的
ASan/UBSan 下通过。此次更新测试与验收工具，不改变生产语义或公开 API。
完整调度器和对象图的跨线程 OOM、生成模块组合及发布总门仍需继续。

## 当前所有权与字符串合同

六个 worker 的两个 class 参数均采用 `RETAIN_XVALUE`，共十二项捕获。
class 保持共享身份，但 worker 独立拥有引用；调用者在进入屏障前释放
自己的两个引用。移除旧测试对单个消费参数的假设，没有恢复参数修饰语法。

字符串结果改为 `"worker\0result"`，以 `xstrview` 检查数据指针、13 字节
长度、全部内容和尾部哨兵，不再只检查结构体的首个指针。完成协议仍要求
输入退役后再发布结果；IR 篡改继续由 C 与声明头生成入口共同拒绝。

## 原有完整场景与新增故障前缀

以下数字均为每个配置的实际执行量，不是多个配置的混合计数。

| 检查 | 每个配置 |
| --- | --- |
| 原有场景 | 3978 次 |
| 真实 worker 结果分配失败 | 90 次 |
| 完成观察者 | 3006 次 |
| 析构时取消 | 972 次 |
| 丢弃 Future | 972 次 |
| 1989 场景计量周期 | 63578 次分配，完整平衡 |
| 新增五类结果故障前缀 | 35 次尝试，其中 30 次真实失败 |

新增前缀在真实 worker 首次生成 payload 分配处设置 XRT 线程本地预算，
持续覆盖函数体、对象或闭包构造、输入退役及完成发布。完成观察者在同一
worker 线程读取触发状态并清除预算，再执行宿主记账。每次尝试独立完成
模块终结，加入真实 worker 后核对内存；不以开放任务池的等待代替终结。

整数、字符串、对象、闭包和另一整数 worker 的首个不触发预算分别为
1、2、12、14、1，逐个执行此前全部失败预算。成功结果精确检查，输入
只析构一次；失败构造允许尚未形成结果所有者，部分构造由完整内存账核验。
失败诊断分配也可能失败，此时保留现有异步无诊断 InternalError 合同。

此故障范围从生成 payload 的首次分配开始，不包含此前任务池初始化、
任务入队或任意其他线程分配；不将它称为完整调度器 OOM 验收。void
worker 保留原有场景覆盖，不计入新增五类非 void 结果前缀。

## 构建来源与复现

编译器完整核心重新构建，Windows 327 个、Linux 326 个翻译单元。
生成 C、声明头、绑定头和 IR 连续生成两次，内容一致；原生供应方使用
实际组件计划，不启用 ALL 模块或替换供应方。测试调用处的分配入口只
设置执行屏障及真实 XRT 故障预算，始终转发实际分配器。

验收入口为 `tools/check_stdlib_worker_completion.py`，参数为
`--root`、`--public-directory`、全新 `--out`；Linux 可加 `--sanitize`。
工具绑定完整核心构建输入、生成产物、实际原生闭包及执行日志。

- Windows 核心：`E:/xlang-validation/stdlib53-b265-build-win1/build.json`。
- Linux 核心：`/var/tmp/xlang-stdlib53-b265-build-linux1/build.json`。
- Windows 执行：`E:/xlang-validation/stdlib53-b265-worker-win-dev1/results.json`，43 阶段。
- Linux 执行：`/var/tmp/xlang-stdlib53-b265-worker-linux-dev1/results.json`，62 阶段。
- 最终提交快照：`E:/xlang-validation/stdlib53-b265-source-final1/frozen/demo6`。

此批生产源码与上一批相同，实际 CLI、共享库及 VFS 产品证明仍见
[当前 IR 产品重建与三组标准库批量验收](stdlib53-b264-current-products.md)。
公开命名空间的函数名出现检查不能证明各重载和 class 方法已实际执行，
完整函数覆盖继续独立验收。CRT 无未决策，XRT 19 组、WaitQueue MPMC
与对象池最后的顺序不变。
