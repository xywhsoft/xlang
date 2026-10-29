# 进程原生接口的真实产品故障验收

进程的原生错误和分配故障检查增加真实 DLL／SO 入口，覆盖 Command／Run、
Pipeline、live Process、Future 工厂和文件重定向。此前这些接口的完整前缀
主要由源码宿主证明；普通语言夹具虽然在当前产品中运行，却没有注入故障。
本次补齐的是原生接口证据，语言进程的完整分配前缀仍须单独完成产品迁移。

## 五组检查

| 组别 | 检查范围 |
| --- | --- |
| Command 和 Run | 构造、环境配置、事务更新、13 个所属线程拒绝、实际运行完整前缀、独立二进制结果、取消及绝对期限 |
| Pipeline | 创建、阶段快照、扩容、实际运行及失败启动前缀，六种拥有结果投影、所属线程拒绝、取消和期限 |
| live Process | 完整 Spawn 前缀、Reader／Writer 工厂前缀、状态分配失败、唯一流租约、父对象释放后流仍有效 |
| Future | pending／terminal 两条公开工厂前缀、独立 Future、取消不影响共享源、父对象释放后的结果和精确 bool payload 策略 |
| 文件重定向 | 独立且不可继承的句柄副本、替换与回滚、关闭原文件后的真实子程序、VFS 能力拒绝、OS 错误分配前缀及堆／句柄账本 |

五组分别链接运行库和编译库，原生观察宿主各使用 O0／O2。优化等级属于
宿主，不是 TCC JIT。每端 20 个配置、84 条完整前缀通过；这些前缀在 Windows
确认 2,168 个分配失败点，在 Linux 确认 1,076 个，每条另确认首个未触发点。
两端差异来自真实平台路径，不能要求故障点数量相同。

## 产品与观察边界

宿主只编译测试代码与同一构建的声明头，不编译或替换 XRT、驻留实现或
编译器对象。导入表必须指向选定的一个 DLL／SO，宿主不能自行定义 Process、
Pipeline 或内存故障接口。实际产品及其生产输入核对构建指纹，测试输入另外
固定指纹。

堆账本检查 live 数量和字节、分配与释放差、double free、invalid free 和
use-after-free。必要时保留真实 native 引用，并依据该完整产品快照提取的
私有布局等待 waiter，之后再比较账本。产品全程保持加载；这不是公开
Process.wait、模块终结或卸载的线程 join 保证，也不适用于裁剪 AOT 的私有布局。

故障只注入调用线程，不冒称子进程、waiter 或整个调度器同时 OOM。旧宿主的
私有 POSIX 启动计划和内部 Future mapper 单元仍走原源码模式，不把无法从
公开产品调用的静态函数换成假提供方，也不把它们列入本门通过数量。

源码模式的额外语法审计当时发现三份旧宿主已过时：live／file 的 XRT-only
实现缺少当前驻留声明所需的 xruntime 类型，Future mapper 仍按三参数调用
现行四参数回调。修改前 HEAD 和本次工作区在两端均复现相同的三项失败。
这部分随后由[回调帧和依赖闭包修复](stdlib53-b342-process-source-contracts.md)
更新，重新通过双端源码 O0／O2 和 Linux ASan／UBSan；不再以早期记录作为
当前证明。源码单元、真实 DLL／SO 和语言层故障迁移仍按各自范围验收。

## 反例与集中复测

两个反例分别删除 Run 测试的故障注入，及故意不释放 Pipeline 阶段投影。
它们在两种实际产品中均先成功编译，再因故障计数或 live 账本断言被拒绝，
共四项控制；编译失败、载入失败和超时不计作控制通过。每端总计 89 个阶段。

扩大检查时，Linux 文件宿主曾依赖嵌入实现间接引入 fcntl 定义；改为显式
包含 POSIX 头后，五组及所有反例统一复测通过。没有改变生产实现或降低断言。
首次四组通过和扩大后失败的报告均保留，最终报告为：

- Windows：`D:/xlang-validation/stdlib53-b341r3-process-oom-win/results.json`。
- Linux：`/var/tmp/xlang-stdlib53-b341r3-process-oom-linux/results.json`。

复测命令：

```text
python tools/check_stdlib_process_oom_shipping.py --root <frozen-demo6> --build-report <ordinary-build.json> --out <fresh-directory>
```

该门作为 `process-oom` 纳入 `check_stdlib_current_products.py` 的必需子门。
两端完整 327 项清单已重新运行 JIT／模块；Windows 20、Linux 22 个必需
子门通过，其中 19／21 个按源码、产品、日志和输出指纹复算后复用，私有
头闭包重新执行。独立 AOT 没有源码或产品变化，保留并复核此前完整报告，
没有把原生宿主 O0／O2 当成新的语言 AOT 执行。

总报告为 `D:/xlang-validation/stdlib53-b341-products-win/results.json` 与
`/var/tmp/xlang-stdlib53-b341-products-linux/results.json`。
最终只读证书 `/var/tmp/xlang-stdlib53-b341-certificate.json` 复算 39,842 项
关联文件指纹，699 项公开签名仍有同源码的双端 JIT／模块和独立 AOT 佐证。
严格 C 与原生对象证明按相同产品及断言字节复核复用，没有重跑或改写旧报告。
完整清单、专用宿主、独立 AOT、原生裁剪与 Windows DST 环境验收仍各自保留
完成条件；本次不新增 API、不升级 XRT 时间模型、不宣告 53 模块整体结项。
