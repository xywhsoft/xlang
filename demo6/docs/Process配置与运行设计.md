# Process 配置与同步运行

本页记录三组长期任务 B21 配置/同步运行、B23 live Process 和 B27
生成程序分配失败验收的公开合同，B28b 增补真正的 typed pipeline。
typed Future 见 B26 专页；文件重定向和 PTY 会话仍待后续开发，
不是七个 process 模块或 53 模块的完整总验收。
旧裸 point live 门面已由 typed Process 与 owned Reader/Writer 替换，
不再用裸 point 转换绕过所有权合同。

B28a 先修复下一批 pipeline 的原生底座：初始化失败不关闭无关句柄，
首个错误穿过 cleanup 保留，同步 collector 归还各 Process 前退役私有
waiter。正常执行/末段启动失败的调用线程完整分配失败前缀和双端存量
回归通过；原生底座与语言接入的不同证据分别见三组任务 B28a/B28b。

## Pipeline：配置、结果与执行合同

`ProcessPipeline()` 创建空构建器；`add(ProcessCommand)` 保存全部已验证
OS 文本/argv/env/stdio 标量的独立快照，成功后才发布新阶段。原 command
修改或释放不会影响快照。`clear()` 释放阶段内容并保留容器容量；`count`
是配置数。配置/计数/run 必须在创建线程调用；不是跨线程共享的可变容器。
add 不接受 terminal 或借用 HANDLE，文件/PTY 有独立 capability 合同。

`run(bytes input=b"", int timeoutMs=-1, int size=-1)` 与
`run(ProcessRunOptions options, bytes input=b"", int size=-1)` 返回
`PipelineResult`。`process.pipeline(array<ProcessCommand>, …)` 有相同两个
重载，使用普通语言循环向构建器 add，再 run；不将 array<class> 当 xvalue
动态数组强行解包，也不在 Cgen 增加 pipeline 算法。空配置报 ArgumentError。

所有阶段并发启动，前段 stdout 通过真实 OS 管道接后段 stdin；不是依次
捕获、复制并重放。提供的 bytes 写入首段，结果 stdout 是末段输出，各段
stderr 独立排空。stderr 的 merge 配置被保留：中间段 merge 内容流入后段，
末段 merge 内容进入结果 stdout；该段独立 stderr 为空，不虚构副本。
其他 stdout/stdin 配置由连接器/捕获规则覆盖，与原生 pipeline 合同一致。

timeout 在 run 时转为同一个绝对单调期限，不逐段重置；使用同一 cancel
source、stopGrace、overflow 策略和输出限额。stderrLimit 对每段单独适用。
启动/停止和资源退役可能越过期限，不承诺硬实时返回。error 溢出是异常；
keep-first/keep-last 仍排空全部流并标记截断，避免管道背压死锁。
基础设施/启动/捕获失败保持原生错误，不制造退出码或部分成功结果；
非零退出、期限与取消是实际结果。一个阶段非零退出不阻止其余已启动阶段。

PipelineResult 的只读字段为 success、waitResult、timedOut、cancelled、
count（原生返回的状态数）、completedCount（已观察到非 NONE 终态数）、
failedIndex（首个非 code=0 阶段；没有则 -1）、exitCode（末段 code）、
inputWritten、duration（微秒）、durationMs（截断毫秒）、stdoutTruncated。
预取消可能没有启动阶段，count=0、failedIndex=-1，但 success=false；
此时 exitCode 报 StateError，不伪造 0。count 与配置数不是始终相等。

`readStdout(size=-1)`/`stdout` 返回独立 bytes，`stdoutText(size=-1)` 严格
验证 UTF-8，完整保留 NUL。size 低于 -1 拒绝、超过实际长度夹紧；bytes
允许在任意字节边界截断，text 不允许截断 UTF-8 scalar。

`stage(index)` 返回独立 owned `ProcessStageResult`，负索引/越界拒绝。
stage 有 success、exitCode、stderrTruncated、readStderr(size=-1)、stderr、
stderrText(size=-1)、status。status 是同一 ProcessExitStatus 的独立快照。
释放/替换整个 PipelineResult 后，stage/status/bytes/string 仍可使用；
投影 OOM 不修改原始结果，允许随后重试。不提供丢失归属的聚合 stderr，
也不伪造中间阶段 stdout、按阶段 duration 或 inputWritten。

28 个 resident 入口的宽度、bytes/string carrier、参数/结果 owner 和效果
由 XLIR 闭合；除保持 ambient error 的析构外保守 checked，不虚报 pure/
no-raise/nonblocking。两个按需组件区分配置与执行：配置-only 不选择执行
组件/native PROCESS_PIPELINE；执行不带 resident live/Future/stream/open/
terminal/run。原生的 PROCESS_PIPELINE → PROCESS_RUN 依赖仍保留，不把
“不选 resident run”冒称“不需要 XRT run 底座”。没有新增 Cgen 算法。

## 构造与输入

`process.cmd(program, args: array<string> = [], size = -1, argSize = -1)`
直接执行程序，参数逐项传递，不拼接 shell，也不隐式转换数组项。
`process.shell(command, size = -1)` 明确请求 shell。
`process.exec/success/exitCode/run(string)` 是 shell 便利入口，不能代替
安全的直接 argv；`process.runFile(program, args)` 直接执行。

ProcessCommand 自有复制 program/command/argv/arg0/workDir/environment，
不持有语言 string 的地址。配置修改和 run 必须在创建线程调用；错误
报告为 checked RuntimeError。引用托管线程身份，不把可复用 OS ID 当
托管身份；外部宿主线程在配置对象使用期间必须存活。

所有 OS 文本先按 size 选取字节前缀（-1 全部、过大截断、低于 -1 错误），
再验证 UTF-8 和禁止内嵌 NUL。截断落在字符中间报错，不静默修复编码。
程序和 shell 命令非空；参数可空。环境名还禁止 `=`。env 的新增/覆盖
在分配成功前不改变旧配置；removeEnv 是子进程环境覆盖，不修改宿主
环境；clearEnv 清除覆盖项。默认继承环境、创建独立进程组、隐藏窗口。

`workDir/arg0/env/removeEnv/clearEnv/inheritEnv/processGroup/newConsole/
hideWindow/terminal/terminalSize/stdio` 修改配置。terminalSize 必须为正
uint32 范围。stdio 接受 inherit=0、pipe=1、null=2；merge=4 仅 stderr，
不把 raw HANDLE=3 导出为整数。捕获式 run 强制 stdout 为 PIPE，保留
stderr 的 merge，否则也改为 PIPE；提供输入或 stdin=pipe 时使用输入
管道。默认构造设置三条 PIPE。它不是文件重定向入口；terminal 的启动
能力待下一批验收。

## 选项、执行与错误

`ProcessRunOptions(timeoutMs=-1, stdoutLimit=16777216,
stderrLimit=16777216, overflow=0, stopGraceMs=250, deadline=0)` 是不可变
设置和自有取消令牌。timeout -1 无限、0 立即到期、正数为每次调用起算
的毫秒；deadline=0 使用相对设置，非零为 monotonic 微秒绝对期限且
要求 timeout=-1。两种时间均有溢出检查。stopGraceMs 非负。
overflow ERROR=0、KEEP_FIRST=1、KEEP_LAST=2；limit 非负。

`options.cancel()` 可跨线程；requested 可跨线程读取，取消不可复位。
调用者必须保持 options 存活；不会将普通可变配置声明成并发安全。
`command.run(input: bytes=b"", timeoutMs=-1, size=-1)` 或
`command.run(options, input: bytes=b"", size=-1)` 借用输入前缀至调用结束。
输出双流并发排空、输入/取消/超时/停止由 XRT 实现，Cgen 不生成该算法。

启动失败、捕获溢出 ERROR、分配失败等基础设施错误抛异常，不返回伪造
退出码。普通非零退出、超时和取消返回真实 ProcessResult。错误与结果
不能混为 `success=false`，不再公开掩盖错误的 spawnFailed 等假状态。

## 结果与生命周期

stdout/stderr 是值类型 bytes；readStdout/readStderr(size=-1) 返回独立
前缀副本。stdoutText/stderrText(size=-1) 是显式严格 UTF-8 复制，保留
内嵌 NUL，无自动替换字符。原结果释放后输出值继续有效。

success、timedOut、cancelled、waitResult、stdoutTruncated、stderrTruncated
描述实际结果。inputWritten/duration 使用 uint64；duration 为微秒，
durationMs 是除以 1000 的普通语言属性。

status 返回独立只读 ProcessExitStatus，具有 kind/code/signal/stop/
coreDumped；kind 是 NONE=0、CODE=1、SIGNAL=2、LOST=3（平台等待失败）。
仅 kind=CODE 时 result.exitCode 可读，其他情况抛 State/RuntimeError。
status.code 是原生状态字段，应与 kind 一起解释。预先取消或立即到期
可没有启动进程，不伪造正常退出。

构造/方法由精确声明进入 XLIR，41 个 resident 导出按 Command、Options、
Run、Status 四个组件分开；typed argv 复用语义与 IR 共用的封闭集合
注册表。IR 持有 owner、参数、结果、错误与效果合同，Cgen 仅消费声明
和通用生命周期机制。exec/success/exitCode 是普通语言组合，不增加
专用 Cgen operation 或逐函数生成算法。checked/conservative 合同不
冒充 pure/no-raise/nonblocking，析构为 no-raise。

## 验证入口

`tools/check_stdlib_process_run.ps1` 要求冻结的全源码 compiler 目录与新
输出目录；Linux 使用 `tools/check_stdlib_process_run.sh`。覆盖精确
文本/bytes、字面 argv、并发双流、取消/期限、独立结果、分配失败账本、
ABI/owner/效果负例、严格 C、JIT/AOT、实际 native/resident/program
装配与 Linux ASan/UBSan/leaks。共享 CLI/JIT 不声称被 sanitizer 插桩。
示例为 `release/examples/stdlib_process_run.xl`。

POSIX launch plan 的参数复制必须遇到首个失败立即停止，保持连续已拥有
前缀；否则零终止数组回收会漏掉失败槽之后的成功分配。XRT 原库与两份
固定快照同时修复；专项对直接执行/shell 的每个 plan 分配点逐项故障
注入，不以第一个对象分配失败代替整个启动路径测试。

Windows 文本准备、内部 waiter 创建、并发 I/O 与失败回收均保存首个
真实错误，不能把 MemoryError 覆盖为“程序不存在”或 ArgumentError。
同步 run 在发布结果前还等待内部 waiter 退役；进程已退出并不等于
该线程的进程引用和上下文已释放。故障注入按完整原生路径和语言路径
扫描到首个未触发点，检查错误类型及活跃分配账本，而不是延时后回收。

## 系统关联程序与终端能力

`process.open(target: string, size: int = -1) as bool` 是显式副作用入口。
按字节选择前缀，超出实际长度截断，`size < -1` 抛 RangeError；选择后
必须为非空、完整 UTF-8、无内嵌 NUL，否则抛 ValueError。有效前缀可
排除 NUL 后缀。适配器只创建一次拥有的 NUL 终止文本并调用
`xrtProcessOpen`，不插入 shell、不解析或改写 URI、不复制库算法到 Cgen。
操作失败通过原生结构化错误抛出；成功只代表 OS/launcher 已接受，
不代表关联应用已经执行完或读取了文件。程序名、URI 和目标仍须由调用者
决定信任边界；此 API 不是沙箱，也不返回一个可等待的 Process。

`process.terminalSupported() as bool` 直接查询真实 XRT 平台能力；当前
无法创建返回 false。其原生实现不建立 XRT 错误，登记为 no-raise，
但不标记 pure/nonblocking，不把运行时资源可用性宣称为永久平台保证。
查询和关联启动各自是独立 resident 组件，不依赖 Command/Run/Status。

专项 `stdlib_process_support`、`stdlib_process_open_only`、
`stdlib_process_terminal_only` 和示例 `stdlib_process_support.xl` 不会
启动宿主关联程序。原生夹具使用真实 XRT 分配器/Unicode/终端查询，
只替换 association launcher，验证精确前缀、截断、七类输入拒绝、原生
错误传递及分配失败账本。这不是 Windows ShellExecute / Linux
xdg-open / macOS open 的系统集成成功验收；不要将受控 launcher 算为
真实关联应用验证。live Process 已由下方 B23 接通；PTY 会话与 resize
仍属后续批次。

## Live Process 与标准流（B23）

已接通的入口为 `process.start(ProcessCommand)`、
`process.start(string command, int size = -1)`（显式 shell）、
`process.startFile(string program, array<string> args = [], int size = -1,
int argSize = -1)`，以及 `ProcessCommand.start()`。`os.start/startFile`
仍转入相同路径，不维护第二份实现。程序与参数沿用配置的真实长度、
UTF-8、NUL 和所属线程检查；传入数组修改不影响已经复制的 argv。

`Process.id` 是 uint64 PID，`state/running` 查询 native 快照。
`wait(timeoutMs = -1)` 返回 `ProcessExitStatus?`：-1 无限等待，0 轮询，
正值为有溢出检查的毫秒期限；超时返回 null，基础设施失败抛原生异常。
`status` 相当于 `wait(0)`。普通非零退出仍有真实状态，不等于异常；
每次返回的退出快照独立拥有，父进程销毁后仍有效。访问 nullable 状态
先使用 `if status != null` 缩窄，不能仅依靠 assert 完成静态缩窄。
退出 kind 决定应读取 code 还是 signal；wait 不伪造退出状态，也不隐式
关闭输入、读取输出或 join 尚在退役的 native waiter。

`stdinWriter()/stdoutReader()/stderrReader()` 返回现有 IoWriter/IoReader。
同步读写复用 XRT IO 的短操作、完整操作、bytes、string 精确字节前缀、
EOF 与错误合同，不创建输出缓存或按 offset 回读历史输出；无 seek/size，
writer 不缓冲，flush 为成功的空操作。Terminal 合并 stderr 时没有独立
stderr reader；非 PIPE 标准流也不能获取管道适配器。

每条标准流最多一个活动适配器；重复获取抛 StateError。可以共享同一个
IoReader/IoWriter 的语言对象引用，但操作必须由调用方串行化，不能并发
读取/写入/销毁同一适配器。不同流独立。每个适配器自有 resident 引用，
父 Process 释放后仍有效，完全不依赖裸 OS handle 快照或语言对象图回链。
原生构造 OOM 回滚引用与租约而不关闭管道，可以安全重试。

`close(stream)` 使用 0/1/2 标识 stdin/stdout/stderr，`closeStdin()` 为
便捷入口。重复关闭成功，已退役的流不能重新获取适配器；显式关闭会
使现有适配器后续非零 I/O 失败，不保证取消已经开始的阻塞 I/O。
IoReader/IoWriter.close 同时关闭对应父管道并释放资源引用；自动析构
为 no-raise 并保留环境错误，显式 close 的原生失败仍会抛异常。

`interrupt()/terminate()/kill()/killTree()` 直接使用 XRT 的停止强度与
进程组边界，不等待子进程，也不扩张成扫描全系统进程树。terminate
会先关闭 stdin。当前析构沿用 XRT：释放引用/管道，不暗中杀进程；
native waiter 最终回收子进程。默认自动终止还是保持显式停止的可选
策略尚未由用户确认，不能以该待选项阻塞已经明确的接入工作。

必须先处理 stdin 和输出再等待（或并发排空输出）；任意子进程都可能
因输出管道写满而阻塞。需要完整收集输出时优先使用同步 run；本 API
不提供自动后台缓存，不把用户选择的等待误称为并发排空保证。

```xlang
auto child = process.startFile(program, ["--mode", "echo"])
auto input = child.stdinWriter()
auto output = child.stdoutReader()
input.writeAll(b"a\0b")
input.close()
bytes data = output.readAll()
auto status = child.wait()
if status != null then
    assert status.kind == 1
    print status.code
end
```

上例只适用于 stderr 不会写满的简单协议；一般情况必须并发排空两个
输出流或使用 run。本批新增 14 个固定 ABI resident 桥接、两个独立
组件；Cgen 只共用 IO 投影布局，没有增加 Process 算法或语义补丁。
typed Future 的新增 B26 合同见《进程标准库异步等待.md》；其完整语言
OOM/模块卸载与跨组验收仍待完成。pipeline 已由上方 B28b 接通；文件
重定向与真实 PTY/resize 会话仍待后续开发和验收，不将这些专项写成
process 整组完成。

## 生成程序分配失败验收（B27）

等待/状态、完整 owned 流、跨函数返回流三条真实生成程序路径，从模块
初始化开始逐点注入当前线程的逻辑分配失败，直到每条首个未触发点。
其中包括原生工厂成功后的 managed handle/class 装箱与部分构造回滚；
触发失败必须传播 MemoryError，不能用成功、伪造状态或捕获所有异常
绕过失败。正常路径保留 bytes/NUL/UTF-8、非零退出与独立状态断言。

测试仅通过原生 Spawn 观察者保留真实 child 引用，在 module fini 后
关闭未完成的输入并显式 join 私有 waiter 尾部，再核对所有分配与错误
账本。没有 mock 原生结果、sleep 重试或隐式 kill；这不是 public wait
的线程退役保证。别的线程未被注入失败，Future/卸载与跨组完整验收
仍待完成；双端冻结证据见三组长期任务 B27。
