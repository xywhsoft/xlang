# 进程语言故障前缀的真实产品验收

已有十一份语言进程故障夹具改用真实编译器输出和实际 DLL／SO 运行时，
覆盖原生入口之后的句柄装箱、类创建和语言清理。两端各 44 个配置通过，
保留原夹具断言，不修改生产编译器、标准库或 XRT 快照。

## 语言夹具与产品

Run、Pipeline、File 各一份，live Process 三份，terminal 一份，Future 四份，
合计十一份。每份由真实 CLI 生成 C，原样编译为 O0／O2 消费对象，再分别
链接运行库和编译库。消费对象和测试观察对象不链接 XRT 或驻留实现源文件；
私有声明和 waiter 布局来自同一次完整产品构建。

文件夹具的生成 C 含两个既有适配函数 `xl6_xrt_fs_file_close` 和
`xl6_xrt_fs_file_temp`。检查器按规范文件操作登记表核对这两个定义，
保留其真实编译输出，不把它们误认为测试替换的驻留入口。其他产品入口
不得由消费对象重新定义。符号检查针对链接前对象，避免混淆 PE 导入跳板，
并用 CRLF 对照确认行尾不会绕过检查。

## 启动和清理观察

`xl6ProcessSpawnV1` 返回后，语言装箱仍可能失败。只在语言 `start()` 返回
之后保留进程，会遗漏该区间，因此测试在消费者链接阶段透明观察这个入口：
调用产品原函数一次，保留其真实 native 引用，再原样返回原句柄和错误。
没有替换进程、分配器、流、Future 或调度实现。

额外测试引用可能延迟最后用户引用的 EOF。初始化成功或失败后，观察器
显式关闭真实 stdin、等待真实进程和私有 waiter，再释放引用。Future 夹具
还等待实际执行器空闲，随后完成模块终结并再次确认其幂等性。堆账本核对
live 数量和字节、分配释放差、double free、invalid free 和 use after free。
等待有明确期限；超时或加载失败不计作通过。

这层 GNU 链接观察只作用于生成 C 的调用点，不拦截 DLL／SO 内部调用。
它证明的是带透明观察的生成 C 消费程序，不是未插入观察器的 TCC JIT
故障前缀，也不提升公开 wait、模块终结或产品卸载的线程 join 保证。
普通 JIT／模块及独立 AOT 仍由完整产品清单分别验证。

## 取消诊断分配失败

取消结果先于可选诊断对象。测试另外透明观察真实
`xl6PayloadObserveOutcome`，只记录输入为 CANCELLED、返回失败、该次调用
首次触发分配故障且最终错误种类仍为 CANCELLED 的事件。只有取消夹具
允许一次这样的诊断回退；其语言断言仍要求捕获 CancelledError、Future
保持 cancelled 而不是 failed，并完成其他等待。

其他触发点必须以 MemoryError 失败，不能笼统放行“触发 OOM 后仍成功”。
每个配置遍历全部当前线程前缀，确认首个未触发点及紧接的第二个未触发点；
两次都必须执行成功、具有准确子进程数量并保持账本平衡。其他线程没有
注入故障，前缀计数也不是所有调度分支覆盖率。

## 批量结果与拒绝控制

Windows 各配置合计确认 5,872 个故障点，Linux 确认 5,652 个；每端
44 条前缀、44 次首个未触发确认及 44 次后续确认通过。阶段数分别为
6,178／5,958。取消诊断回退每端准确四次，其余故障保持 MemoryError。

每端七项控制通过：两种产品分别删除故障注入、泄漏观察引用、漏记取消
诊断，共六个程序均先成功编译再因对应断言拒绝；另一个伪造驻留入口
控制先成功编译，再由消费对象定义检查拒绝，包含 CRLF 对照。

复测命令：

```text
python tools/check_stdlib_process_language_oom_shipping.py --root <frozen-demo6> --build-report <ordinary-build.json> --out <fresh-directory> --jobs 2
```

最终专项报告为 `D:/xlang-validation/stdlib53-b343r2-language-oom-win/results.json`
和 `/var/tmp/xlang-stdlib53-b343r2-language-oom-linux/results.json`。
初次原型揭示的取消诊断回退、首轮符号分类误拒绝及对应失败日志均保留。

专项作为 `process-language-oom` 纳入产品必需门，按输入和产物指纹复核后
复用独立报告。两端重新运行完整 327 项 JIT／模块清单，各 318 项执行通过、
一项预期拒绝，七项宿主条件和一项平台条件保持；Windows 21／Linux 23 个
必需子门通过。总报告为 `D:/xlang-validation/stdlib53-b343-products-win/results.json`
与 `/var/tmp/xlang-stdlib53-b343-products-linux/results.json`。

最终只读证书 `/var/tmp/xlang-stdlib53-b343-certificate.json` 复核 52,694 项
关联文件指纹，并重新核对每个故障点、反例和原清单的十一份夹具。699 项
公开签名仍关联相同源码的双端 JIT／模块及独立 AOT 通过记录；严格 C 与
原生裁剪证明按相同生产输入和断言字节复核复用，不改写或冒称重新执行。

Windows DST 专项环境和完整 53 模块逐项完成审计继续；
CRT 0、XRT 19 组未决策和对象池最后不变。
