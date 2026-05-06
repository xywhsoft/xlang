# xldbg DAP 调试器设计

## 目标

`xldbg` 当前是 debug mode compiler driver，已经能执行 `lex` / `parse` / `sem` / `lower` / `cgen` / `run` 并输出 dump/trace。下一阶段目标是在此基础上实现语言层调试器，让 IDE 能通过标准协议调试 XL 源码。

设计目标：

- IDE 通过标准 Debug Adapter Protocol 连接 `xldbg`，不绑定私有 REPL。
- 调试视角停留在 XL 语言层，而不是生成 C 或 TCC 内部。
- 断点、单步、调用栈、局部变量都基于 AST/HIR/MIR 的源码位置和语义信息。
- 第一版优先可用和可验证，暂不追求 OS 级进程调试能力。
- debugger / DAP 实现必须严格隔离在 `xldbg` 中，不能有任何可执行代码进入 `xl.exe`。

非目标：

- 不实现 DWARF/PDB 调试信息生成。
- 不通过操作系统调试 API 注入硬件/软件断点。
- 不把 IDE 暴露到 TCC 生成 C 的细节上。
- V1 不实现完整表达式求值、修改变量、数据断点、时间旅行调试。

## 当前基础

现有主链路：

```text
XL source -> lexer -> parser -> AST -> semantic -> HIR/MIR -> C codegen -> libtcc -> entry function
```

关键事实：

- `xldbg` 通过 `-DXLANG_DEBUGMODE=1` 构建。
- debug 入口是 `main_debug.c`，最终调用 `xlDriverRunDebug(...)`。
- CLI 命令枚举在 `src/driver/xl_cli.h`，debug 命令当前只覆盖 dump/trace。
- 运行路径在 `xlDriverRunProgram(...)` 中生成 C 后调用 `xlExecRunCode(...)`。
- `xlExecRunCode(...)` 通过 host/TCC 层把生成 C 编译到内存并执行入口符号。
- AST/HIR/MIR 已携带 `iLine` / `iColumn`，可以作为调试源位置基础。
- Host symbol 注册集中在 `src/host/xl_host_symbols.c`，生成 C 可以调用宿主函数。

这些条件适合做编译期插桩调试器：在生成 C 时注入调试 probe，probe 回调 `xldbg` 宿主调试运行时。

## xldbg-only 隔离约束

调试器实现必须遵守硬性隔离规则：

- 所有 DAP adapter、debug controller、probe runtime、REPL debugger 代码必须由 `XLANG_DEBUGMODE_ENABLED` 宏保护。
- 新增调试器 `.c` 文件必须放入 `xldbg` 专用构建路径，或显式从 `xl.exe` / `xlang.dll` 的 source list 中排除。
- 公共头文件只允许暴露宏保护下的 debug-only 类型、枚举和声明。
- `xl.exe` 的 release 构建中不应出现 `dap` / `dbg` 命令、debugger 符号、probe 符号或 DAP transport 代码。
- 每个实现阶段都要用 `release\xl.exe --help` 和二进制符号/字符串检查验证隔离。

## 架构

推荐结构：

```text
IDE
  <DAP JSON over stdio/TCP>
xldbg dap adapter
  <debug controller API>
xldbg debug runtime
  <probe callbacks>
generated C debuggee
  <libtcc in-memory execution>
XL program
```

`xldbg` 需要分成三个逻辑层：

| 层 | 职责 |
| --- | --- |
| DAP adapter | 解析和发送 DAP 消息，把 IDE 请求转换为调试控制命令 |
| Debug controller | 管理断点、step 模式、暂停状态、线程同步、frame/variable 引用 |
| Probe runtime | 被生成 C 调用，记录当前位置、调用栈、局部变量，并在需要时阻塞 debuggee |

## 为什么不用 OS 级调试

OS 级调试器适合 C/C++ 这类直接生成机器码并维护 DWARF/PDB 的语言。x-lang 当前通过 C codegen 和 TCC 内存执行，若从 OS 层调试，只能看到：

- TCC 生成的机器码或中间 C 源。
- C 层函数名和临时变量。
- 不完整的 XL 语义栈。
- async/generator lowering 后的状态机细节。

而语言用户需要的是：

- XL 源码行。
- XL 函数调用栈。
- XL 局部变量。
- XL 异常和 async/generator 状态。

因此 V1 采用源码级 probe，比 OS 级调试更直接，也更适合当前工程。

## 可暂停点

可暂停点来自 MIR 中有源码位置的语句节点。V1 建议覆盖：

| MIR kind | 是否插桩 | 说明 |
| --- | --- | --- |
| `XL_MIR_BIND` | 是 | 变量绑定、赋值前停 |
| `XL_MIR_BRANCH` | 是 | `if` 条件或分支语句前停 |
| `XL_MIR_TRY` | 是 | try 入口停 |
| `XL_MIR_WHILE` | 是 | 每次条件检查前停 |
| `XL_MIR_FORIN` | 是 | 每次循环迭代前停 |
| `XL_MIR_RETURN` | 是 | return 前停 |
| `XL_MIR_BREAK` | 是 | break 前停 |
| `XL_MIR_CONTINUE` | 是 | continue 前停 |
| `XL_MIR_THROW` | 是 | throw 前停 |
| `XL_MIR_ASSERT` | 是 | assert 前停 |
| `XL_MIR_YIELD` | 是 | yield 前停 |
| `XL_MIR_CALL` | 否，V1 可选 | 表达式级 step-in 后续再细化 |

生成 C 示例：

```c
xl_dbg_stmt("D:\\git\\x-lang\\demo5\\release\\examples\\full\\main.xl", 12, 4, "main");
```

probe 执行逻辑：

```text
更新当前 source location
检查断点表
检查 step mode
如需暂停：
  设置状态为 stopped
  发送 stopped event 给 DAP 层
  阻塞 debuggee 线程
等待 continue/next/stepIn/stepOut 唤醒
```

## 调用栈

在函数入口和退出处插入 frame probe：

```c
xl_dbg_enter_func("main", "D:\\path\\main.xl", 1, 6);
...
xl_dbg_leave_func();
```

controller 维护 XL 语言层栈：

```text
frameId
functionName
sourcePath
line
column
scopeId
funcIndex
locals
arguments
```

`stepOver` / `stepOut` 基于 frame depth 实现：

- `stepIn`：下一次到达任意可暂停点即停。
- `next`：只在栈深度小于等于当前深度并且源码位置变化时停。
- `stepOut`：只在栈深度小于当前深度后停。
- `continue`：只在断点、异常或 pause 请求时停。

## 局部变量

局部变量需要 codegen 主动注册。外部不能可靠访问 C 栈上的局部变量地址，所以 probe runtime 应由生成 C 告知变量名、类型和地址。

示例：

```c
xl_dbg_local_int("i", &xl_i);
xl_dbg_local_bool("ok", &xl_ok);
xl_dbg_local_float("total", &xl_total);
xl_dbg_local_string("name", &xl_name);
xl_dbg_local_value("item", &xl_item);
```

V1 变量范围：

- arguments
- current function locals
- primitive values: `int` / `bool` / `float` / `string`
- dynamic value: `var`

后续扩展：

- array/list/dict/set 子项展开。
- object 字段展开。
- captured variables。
- async/generator 状态机保存字段。
- setVariable。

## 断点

IDE 发送 source path + line。`xldbg` 内部按规范化路径存储断点。

```text
breakpoints[canonicalSourcePath][line] = breakpoint
```

为了返回 verified breakpoint，需要在编译后从 MIR 构建 executable line table：

```text
source path -> executable lines
source path + requested line -> actual executable line
```

策略：

- 若请求行可执行，返回 `verified=true`。
- 若请求行不可执行但下一行有可暂停点，V1 可选择移动到下一条可执行行并返回实际行。
- 若无法映射，返回 `verified=false` 和 message。

## DAP 通信

DAP 使用带 `Content-Length` header 的 JSON 消息。

```text
Content-Length: <byte-length>\r\n
\r\n
{...json...}
```

推荐支持两种启动方式：

```bat
xldbg dap
xldbg dap --listen 127.0.0.1:4711
```

V1 优先实现 stdio 模式，因为 VS Code 可直接启动 adapter 进程并通过 stdin/stdout 通信。

基本会话：

```text
IDE -> initialize
xldbg -> initialize response
xldbg -> initialized event
IDE -> setBreakpoints
IDE -> setExceptionBreakpoints
IDE -> configurationDone
IDE -> launch
xldbg -> stopped event
IDE -> threads
IDE -> stackTrace
IDE -> scopes
IDE -> variables
IDE -> continue / next / stepIn / stepOut
```

DAP 官方参考：

- https://microsoft.github.io/debug-adapter-protocol/
- https://microsoft.github.io/debug-adapter-protocol/overview.html

## 线程模型

推荐最小线程模型：

```text
DAP thread:
  read requests
  write responses/events
  mutate controller state under lock

debuggee thread:
  call xlExecRunCode
  execute generated C entry
  block inside probe when stopped
```

共享状态：

```text
state: starting | running | stopped | terminated
breakpoints
step mode
current frame stack
variables reference table
pending stop reason
```

同步规则：

- debuggee 只在 probe 边界暂停。
- DAP thread 不直接读未加锁的 frame/variable 状态。
- `continue` / `next` / `stepIn` / `stepOut` 修改 step mode 后唤醒 debuggee。
- `disconnect` 应能请求终止，V1 可以先做到停止接收并让 debuggee 自然结束。

## 命令设计

建议新增命令：

```bat
xldbg dbg <file> [options]
xldbg dap [--listen host:port]
```

`dbg` 是本地交互 REPL，便于调试 adapter 之前验证 controller。

`dap` 是 IDE 接入入口，适配 DAP 消息。

两者共用同一套 debug controller 和 probe runtime，不应各自维护断点/单步逻辑。

## 错误和异常

语言异常应该作为 DAP stopped event：

```json
{
  "event": "stopped",
  "body": {
    "reason": "exception",
    "threadId": 1
  }
}
```

当前 host/TCC 层已有 `xl_generated_exception_*` 符号读取逻辑。V1 可以先复用现有异常信息，在 uncaught exception 后生成 exception stop 或 terminated event。

## V1 边界

V1 完成后应能：

- 在 IDE 中启动 XL 程序。
- 设置源码行断点。
- 命中断点后显示源码位置。
- 执行 continue、next、stepIn、stepOut。
- 展示 XL 调用栈。
- 展示当前 frame 的基础局部变量。
- 正确结束程序并返回 exit code。

V1 不要求：

- attach 到已运行进程。
- 多线程 debuggee。
- 条件断点。
- logpoint。
- data breakpoint。
- hover/evaluate 完整表达式。
- 修改变量。
- async/generator 的完整逻辑栈恢复。
