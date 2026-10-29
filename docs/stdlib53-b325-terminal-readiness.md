# 启动期 resize 的应用就绪合同

本批按用户批准的策略保留 Windows 系统 ConPTY：启动尺寸在配置中指定；
启动后的动态 resize 先等待应用就绪。没有修改 XRT 后端、Cgen 或运行时算法，
没有隐式重发、任意 sleep、新依赖或对 opaque HPCON 的内部访问。

## 问题归因与支持边界

本机 Windows 10 19045、conhost 10.0.19041.4522 的直接 Win32 对照可以复现：
`ResizePseudoConsole` 返回成功，但子程序原生查询仍为 80×24，直至五秒期限
结束也没有变成请求的 96×32。该程序不链接 XRT 或 xlang。

探索矩阵保留原始失败状态：432 次中有 96 次故意省去必要标准句柄设置，
属于无效配置对照，不是 96 项生产缺陷；其余 336 次有效配置中两次启动期
resize 丢失。`original-35-1` 在创建后、恢复子进程前调整失败；
`trace-57-3` 在创建子进程前调整失败，观察记录最终缓冲区和窗口仍为 80×24。
后者仅包装真实查询以记录变化，没有改变尺寸、事件、超时或返回语义。

此前原语言夹具在两端各 256 次全绿，也不构成根因修复。这次不以“多跑几轮
终于成功”关闭历史失败，而是明确承认系统启动期限制，按用户选择修改支持合同。

初始尺寸使用 `terminalSize`／原生 `Columns`、`Rows`。`start`／`Spawn` 成功
不是应用就绪确认。Windows 10 上首次动态 resize 前，需要应用协议确认其已
附着并准备好处理终端；没有该协议的第三方程序不享有启动期调整交付保证。
运行中 resize 成功也不是应用已经观察到新尺寸的屏障；尺寸敏感操作仍需应用
处理尺寸事件，并在协议中按需确认。库不会猜测任意第三方程序的就绪状态。

[Microsoft API 文档](https://learn.microsoft.com/en-us/windows/console/resizepseudoconsole)
规定 resize 请求和 HRESULT 返回；
[当前上游实现](https://github.com/microsoft/terminal/blob/main/src/winconpty/winconpty.cpp)
通过控制通道提交 resize 消息，没有等待应用确认。当前上游代码不是本机
旧版系统实现已修复的证据，平台归因以本批独立原生对照为准。

## 夹具与产品验收

私有 child 在原生终端查询成功、输入模式／尺寸事件准备完成后发送
`terminal-ready`。普通会话先读到该消息，再发送一次 96×32 resize 和输入；
输入先行夹具仍等待完整 `line-ready` 后才发送一次 resize。
两者继续断言真实 TTY、stderr 合流、实际 96×32、`reply:hello` 和退出码 0。
没有删除、放宽或改写目标尺寸断言，也没有把控制请求成功当作应用确认。

新增 `check_stdlib_terminal_shipping.py`，使用 B324 普通构建的真实 CLI、
DLL／SO 及嵌入 VFS，在无源码和头文件回退的独立目录执行。
仅新测试工具、私有 child 和两份语言夹具来自当前任务目录；生产实现、SDK
声明与提供方来自指纹匹配的构建。生成 C 两次完全一致，分别严格编译 O0／O2。
独立 AOT 移到另一个目录执行，并检查没有依赖 xlang 共享库或 Linux RPATH。

Windows、Linux 每端均为 280 阶段通过：基础构建及两夹具各九阶段、四项
拒绝对照、256 次双工作线程压力。压力精确覆盖两份夹具 × JIT／模块，每格
64 次。跳过 resize 和改为 95×31 都必须在两个编译器 API 模式中产生
`AssertionError`，不能以超时替代拒绝证据。输入、产物和日志均记录 SHA-256。

产品总门另纳入此专项，并显式使用更新后的 child 和终端夹具，避免使用冻结
构建目录中的旧测试而误报新合同通过。其他夹具与产品来源不变。

跨组产品总门：Windows 628 阶段、9 个子门通过；Linux 629 阶段、10 个子门
通过。每端清单均为 311 个候选：303 普通通过、1 预期拒绝、6 需专用宿主、
1 平台条件；专用对象图、clear、Array 入口等另由对应子门验证。Windows 的
IANA DST 条件、Linux 的旧 cmd/findstr/more 条件没有改写成普通通过。

XRT 原生 API 文档同步提交为 `f132faaf`，仅更新 `docs/api/process.md`，
包括就绪前置条件、返回成功的含义及已存在的 1..32767 尺寸范围；没有生产
源码或单头产物变更。xlang 文档同步使用同一合同。

## 证据与未完成范围

- 原 Win32 探索：`D:/xlang-validation/stdlib53-b325-win32-matrix/results.json`，
  保留 `Complete=false`、两项有效配置失败与 96 项无效配置对照。
- 原基线：`stdlib53-b325-terminal-baseline-win/results.json` 及 Linux
  `/var/tmp/xlang-stdlib53-b325-terminal-baseline-linux/results.json`。
- 新合同 Windows：`D:/xlang-validation/stdlib53-b325-terminal-ready2-win/results.json`。
- 新合同 Linux：`/var/tmp/xlang-stdlib53-b325-terminal-ready2-linux/results.json`。
- 跨组产品：`D:/xlang-validation/stdlib53-b325-products-win/results.json`、
  `/var/tmp/xlang-stdlib53-b325-products-linux/results.json`。
- 输入与子报告复核：`D:/xlang-validation/stdlib53-b325-dev/certificate-win.json`、
  `certificate-linux.json`；选定提交边界见同目录 `commit-selection.json`。

本专项不是 Windows 系统 bug 修复，也不是全平台零句柄增长承诺。此前记录的
系统 ConPTY 句柄保留现象仍保留，未解读或关闭其不透明内部句柄。

额外重编旧 `xlir_process_terminal_host.c` 时发现，它的源码拼接依赖落后于当前
typed Array 和 xruntime 声明闭包。失败记录保留在两端第一轮
`stdlib53-b325-terminal-ready-*/native-ledger-build.log`；本轮没有替换生产提供方
来迁就它，也不将旧源码账本记为重新通过。B325 报告明确记录
`NativeSourceLedgerAcceptance=false`。随后已完成共享产品观察器迁移和双端
完整账本验收，当前状态见
[终端共享产品的完整资源账本](stdlib53-b326-terminal-product-ledger.md)；
原 B325 报告仍保持原值，不将后续通过回写为历史通过。

完整函数／分支覆盖、工作线程故障注入及发布总验收继续，53 模块不结项。
CRT 未决策为 0；19 个 XRT 未决策组不变，对象池仍最后。
