# 进程源码测试的回调帧和依赖闭包

三份旧进程源码宿主更新到当前 xruntime 声明和四参数 Future 回调协议。
修复保留 mapper 的 MemoryError、首错、重复发布和堆平衡断言，不把源码
单元与真实 DLL／SO 检查混为一项，也不改变语言或生产运行时。

## 声明与回调帧

live／file 的白盒源码模式统一使用已有完整原生测试底座，不再用只有 XRT
类型的局部实现满足已包含 xruntime 类型的驻留 ABI。它不是应用的裁剪入口；
应用闭包仍由标准编译器计划及独立裁剪门验证。

私有 mapper 按 `(input, promise, cancel, context)` 调用。测试先由真实 V4
接口分配空捕获描述的回调帧，传入该帧并通过 Discard 回收；不把新增参数
机械补成 NULL。帧分配位于 mapper 故障窗口之前，窗口仍单独测 bool 结果
分配失败。pending／terminal 公开工厂的完整前缀另外保留。

## 规范依赖闭包

`check_stdlib_process_source_contracts.py` 编译宿主后读取实际未解析符号，
按编译器的符号归属和组件依赖表求驻留闭包。未知符号、未知依赖和链接失败
直接报错，不用不断追加的手写源文件列表补齐。

每个优化等级只编译一次完整原生实现和错误 TLS，三个宿主及控制共用该对象。
宿主中已包含的驻留实现不会重复链接。观察头只提供声明和同一完整快照的
私有布局；源码模式与产品模式必须显式二选一。源码模式自己链接原生实现，
所以只证明白盒合同，不能冒称真实产品或生成应用的体积证明。

三个单元覆盖 live 流／状态独立拥有、文件重定向句柄及错误回滚、Future
mapper／工厂／取消隔离。三参数调用控制须编译失败；空回调帧控制须先成功
编译，再在真实清理入口 abort。加载失败、超时和普通编译失败不能代替后者。

## 入口和复测

Windows／Linux 旧进程脚本的三个相关单元入口共用此检查器，FutureOnly
只选择 Future，并保留显式子集记录。Linux 原有 ASan／UBSan 通道由
`--sanitize` 保留，不以普通 O0／O2 替代。

```text
python tools/check_stdlib_process_source_contracts.py --root <selected-demo6> --out <fresh-directory>
python tools/check_stdlib_process_source_contracts.py --root <selected-demo6> --out <fresh-sanitized-directory> --sanitize
```

第二条用于 Linux，使用带调试信息的 O1 和 address／undefined sanitizer。
进程子程序仍是独立受控 C 程序；只有测试原生实现、驻留实现和宿主被插桩。
观察宿主的私有 waiter 等待不提升公开 wait 或模块终结的保证。

## 本轮结果

两端各六个 O0／O2 源码配置和两项拒绝控制通过，每端 57 个阶段；完整前缀
各确认 42 个故障点。Linux O1 ASan／UBSan 另通过三个配置和两项控制，
共 31 阶段，没有将 sanitizer 控制程序的预期 abort 误算成普通程序通过。

对应报告为 `D:/xlang-validation/stdlib53-b342r3-source-contracts-win/results.json`、
`/var/tmp/xlang-stdlib53-b342r3-source-contracts-linux/results.json` 与
`/var/tmp/xlang-stdlib53-b342-source-sanitized-linux/results.json`。

真实产品总门重新运行完整 327 项清单，Windows 20／Linux 22 个必需子门通过；
受宿主／平台限制的运行条件仍保留。改动过观察头和源码宿主的 process-oom
子门不复用旧报告，两端重新执行 20 个原生产品配置与四项控制。其他不变
子门逐项复算输入／产物后复用，独立 AOT 的相同生产输入与断言报告另行复核。
本轮没有修改生产编译器、运行时、XRT 快照或工作区 release 产品。

最终证据为 `/var/tmp/xlang-stdlib53-b342-certificate.json`，交叉复算 40,120
个输入、产物、报告和日志文件；699 个公开签名分别关联两端相同源码的
JIT／模块及移位 AOT 通过记录，不将签名见证提升为所有运行分支覆盖。
本轮原生产品每端 84 条前缀分别确认 Windows 2,188／Linux 1,160 个故障点；
继承环境和子程序路径影响前缀长度，因此逐条核对当轮日志，不要求历史计数恒定。

本轮对旧脚本执行相关接线与语法检查，不把旧总门其余 Cgen、导入及语言
分配测试路径都称为已执行。语言层进程完整 OOM 前缀的真实产品迁移、
Windows DST 环境及完整 53 模块完成审计继续；CRT 0、XRT 19 组未决策和
对象池最后不变。
