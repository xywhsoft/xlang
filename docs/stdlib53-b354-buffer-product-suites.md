# Buffer 原生专项的实际产品验收

2026-10-17。三份既有 Buffer C 专项已在 Windows、Linux 的实际运行库与
编译器 DLL／SO 上通过 O0／O2 验收，各十二个配置、十一项拒绝控制。
补齐的是当前产品的故障、线程和重入证据，没有修改 Buffer 或 XRT 实现。

## 原有断言与产品链接

`check_stdlib_buffer_suites_shipping.py` 保留 `stdlib_buffer_core_host.c`、
`stdlib_buffer_value_host.c`、`stdlib_buffer_codec_host.c` 原有断言。
消费者只使用与构建一致的声明头，链接实际产品；对象符号检查拒绝强符号、
弱符号和数据形式的 xrt／xl6 提供方。导入检查必须指向选定产品，Windows
另在可执行程序旁放置对应 DLL，避免 PATH 导入旧文件。

核心专项覆盖容量、范围、任意字节、自引用扩容、空隙填零和错误保持；
值专项覆盖独立副本、内嵌 NUL、快照迭代、错误线程拒绝、跨线程最终销毁及
拥有句柄事务；codec 专项覆盖无哨兵视图、自定义字母表、长度、严格格式和
错误码。三个源文件与冻结构建中的原件必须一致。

## 故障前缀与资源检查

| 测试 | 前缀族数 | 分配失败方式 |
| --- | --- | --- |
| 核心 | 8 | 调用线程的一次性逻辑故障注入，从零开始 |
| 编解码 | 4 | 调用线程的一次性逻辑故障注入，从零开始 |
| 值与迭代器 | 15 | 原测试分配器的持续物理拒绝，从一开始 |

每个前缀运行至首次不命中，再确认下一个不命中点。每端四个产品／优化组合
累计 120 个实际失败点。self-assign 是有意的零分配操作，四个组合均确认
分配次数为零，不为了制造故障而改变实现。失败断言继续检查原对象的数据、
长度、容量和内容不变。

消费者在真实 `xrtMemDebugReset` 清零前核对完整堆账本，结束时再次要求
live count、live bytes 为零，分配释放相等，且无非法释放、重复释放、UAF
或残留调用线程错误。每端十一项反例分别确认两种产品上的末尾泄漏、清零前
泄漏、关闭调试、禁用物理注入、禁用逻辑注入都不能通过；另一项确认伪造
提供方及 CRLF 符号输出被正确识别。

值专项另有四个分配器回调重入配置。它们关闭调试隔离后检查 Close 重入时
状态已脱离及无重复释放，单独记录为断言通过，不作为零堆账本证明。

## 完成矩阵与延期项

产品总门新增必需的 `buffer-native` 子门，C10／C11／C12 行为合同要求该门。
完成审计重新核对三份原件、实际镜像、导入、逐点日志及输入摘要，再将
Buffer、buffer_hex、buffer_base64 的 `buffer-native-suites` 未完成项清零。
六项新增审计反例拒绝缺失配置、缺失前缀、替代提供方、缺失拒绝控制、零故障
及把重入冒作堆账本通过。

最终矩阵仍为 53 个原生模块、49 项行为合同、84 份行为夹具、166 处检查片段；
十三项审计反例通过。批准范围的 `ApprovedScopeAcceptance` 为 true。
Windows DST gap／fold 实测按用户决定延期，`Full53Acceptance` 保持 false，
不把本轮结项扩展成全仓 RC0 或未执行环境的验收通过。

本批沿用同一冻结构建，不重复未变的 328 项正例、独立 AOT、严格 C 和裁剪
矩阵。完成审计组合旧父报告与后来独立运行的文档、声明拒绝和 Buffer 子门，
共 Windows 三十项、Linux 三十一项；这不表示旧父报告曾运行这些新门。
旧说明和测试脚本仅按原摘要及来源恢复历史字节，生产输入不允许历史回退。

## 运行记录

每端原生 Buffer 运行各 442 个子进程阶段，结果位于：

- `D:/xlang-validation/stdlib53-b354-buffer-win/results.json`
- `/var/tmp/xlang-stdlib53-b354-buffer-linux/results.json`

最终完成审计位于：

- `D:/xlang-validation/stdlib53-b354-audit-win-r3/results.json`
- `/var/tmp/xlang-stdlib53-b354-audit-linux-r3/results.json`

```text
python tools/check_stdlib_buffer_suites_shipping.py --root <frozen-demo6> --build-report <same-build.shipping.json> --out <new-directory>
python tools/check_stdlib_completion_audit.py --root <frozen-demo6> --product-certificate <B350-certificate> --guide-certificate <B351-certificate> --abi-certificate <B352-certificate> --buffer-report <Buffer-results.json> --history-root <exact-historical-inputs> --out <new-directory>
```

CRT 未决策为零，XRT 其余十九组和对象池最后的决策顺序不变。
