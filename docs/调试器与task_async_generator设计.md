# 调试器、task API 与 async generator 设计

本文固定 DEMO6 后续三项高级能力的设计口径：

- 完整调试器体系：`xl.exe dbg` / `xl.exe dap`、断点、单步、变量查看和 DAP。
- demo5 风格的高层 `task` API：`task.spawn` / `task.all` / `task.race` / `task.cancel`。
- `async generator<T>`：允许 `await` 与 `yield` 在同一函数中稳定组合。

这三项能力允许参考 demo5 的用户语义和测试资产，但不照搬 demo5 的内部实现。demo6 必须建立在当前类型系统、XRT、ABI、VFS、DLL、静态 libtcc 和代码生成基座上重新实现。

## 当前发布状态

- 高层 `task` API 与 async generator 已进入当前实现和回归。
- 完整调试器仍是后续设计，本次首发不提供 `xl dbg`、`xl dap` 或独立 `xldbg.exe`。
- 曾经存在的 probe/DAP smoke 只返回合成栈帧和占位变量，没有控制真实被调试程序，已从正式 CLI 和构建中删除。
- 本文后续调试器章节只描述重新实现时必须满足的完整边界，不表示当前命令可用。

## 总原则

- demo5 只作为迁移参考，不作为 demo6 的权威设计来源。
- 先补高层 `task` API，再实现 `async generator<T>`，最后落地完整调试器体系。
- 能放入 XRT 的并发、任务、future、取消和同步基础能力，应优先进入 XRT。
- xlang 侧只做语言语义、类型检查、代码生成和少量必要 glue。
- `xl.dll` 运行版不包含调试器、DAP adapter、交互式 dbg 或 dump 类能力。
- 调试器视角必须停留在 xlang 源码层，不暴露 TCC、生成 C 或 OS 级调试细节。
- 第一版优先完整、稳定、可验证，不追求所有高级 IDE 能力一次到位。

## 开发顺序

### 第一阶段：高层 task API

原因：

- demo6 已有 `async`、`await`、future/task、thread、channel、generator 基础。
- `task` API 是 async generator 和调试器的运行时前置能力。
- 该阶段对语法侵入较小，适合作为本轮高级能力的第一步。

### 第二阶段：async generator

原因：

- `async generator<T>` 同时依赖 async suspend point 和 generator suspend point。
- 它会影响后续调试器的逻辑栈、暂停点、异常传播和变量快照。
- 先实现它，可以避免调试器在状态机模型变化后返工。

### 第三阶段：完整调试器体系

原因：

- 调试器需要理解普通函数、async 函数、generator、async generator、异常和 task。
- 等 task 与 async generator 稳定后，再固定 probe、frame、变量和 DAP 映射更可靠。

## 高层 task API

### 公开 API

第一版固定以下主路径：

```xlang
auto t = task.spawn(work, 1, 2)
auto v = await t

auto allResult = await task.all([t1, t2, t3])
auto firstResult = await task.race([t1, t2, t3])

task.cancel(t)
```

暂不在本设计中新增更多 task 标准库函数。已有 async/thread/channel/sync 能力继续按当前标准库口径使用。

### task.spawn

`task.spawn(fn, ...args)` 创建一个异步任务并立即调度执行。

规则：

- `fn` 必须是可调用值，包括普通函数、闭包、成员函数适配后的函数值。
- `...args` 按现有函数调用规则做重载解析、默认参数补全和类型检查。
- 返回值为可 `await` 的 task/future 对象。
- 若 `fn` 返回 `T`，则 `task.spawn(fn, args...)` 的 await 结果为 `T`。
- 若 `fn` 无返回值，则 await 只等待完成，不产生值。
- 若任务体抛出异常，异常存入 task/future，并在 await 时重新抛出。

实现要求：

- 底层优先使用 XRT task/future 能力。
- 如果 XRT 缺少取消、聚合或 race 所需基础设施，先补 XRT。
- xlang 生成代码负责捕获参数、构造调用帧、注册释放函数和桥接异常。
- 捕获参数必须遵守当前 class/function/xvalue retain 和 destroy 规则。

### task.all

`task.all(array<future<T>> tasks)` 等待全部任务完成。

规则：

- 输入必须是 array/list 形态的 future/task 集合；第一版优先支持 `array<future<T>>` 与 `array<var>`。
- 所有任务成功时，返回 `array<T>`，顺序与输入顺序一致。
- 任一任务失败或取消时，`task.all` 记录第一个非成功 source。
- 如果多个 source 同时处于失败/取消等非成功状态，`task.all` 不设置“失败优先”或“取消优先”的额外优先级；聚合 future 传播 group 首先观察到的非成功结果。
- 第一版采用 wait-all 语义：所有 source 都完成后，聚合 future 再传播第一个失败或取消结果；未失败的 source 不会被主动取消，后续仍可单独 `await`。
- `task.all([])` 是运行时错误；第一版要求输入集合非空，避免空集合结果类型和失败传播语义不清晰。

取消规则：

- 对 `task.all` 返回的聚合任务调用 `task.cancel`，应向所有尚未完成的子任务发出协作式取消请求。
- 取消不是线程强杀；任务体需要在可取消点观察取消状态。

### task.race

`task.race(array<future<T>> tasks)` 等待第一个完成的任务。

规则：

- 第一个成功完成的任务使 race 成功，结果为该任务结果。
- 第一个失败完成的任务使 race 失败，并在 await race 时重新抛出对应异常。
- 第一个取消完成的任务使 race 进入取消态，并在 await race 时抛出取消错误。
- 如果多个 source 在 race 创建前已经完成，`task.race` 不设置“成功优先”“失败优先”或“取消优先”的额外优先级；聚合 future 传播 group 首先观察到的完成结果。
- `task.race([])` 是语义错误或运行时错误；第一版建议在语义可判定时诊断，否则运行时抛出错误。
- race 完成后，会向尚未完成的 loser source 发出协作式取消请求。取消不是线程强杀，worker 晚到完成不能覆盖已经进入取消态的 loser。

取消规则：

- 对 race 结果调用 `task.cancel`，应向尚未完成的子任务发出协作式取消请求。

### task.cancel

`task.cancel(t)` 发出协作式取消请求。

规则：

- `t` 必须是 task/future 或聚合 task。
- 只有 pending task 首次接受取消请求时返回 `true`；已取消或已完成的 task 返回 `false`，且不报错、不改写原状态。
- 取消不保证任务立刻停止。
- 如果任务在取消后到达可取消点，应进入 canceled 状态。
- await canceled task 时抛出取消错误，错误模型复用当前异常系统。

第一版暂不设计强制终止线程。

### 类型与泛型

建议将 task/future 作为明确的一等类型继续完善：

```xlang
future<int> a = task.spawn(add, 1, 2)
auto b = await a
```

后续可根据实现收敛为 `task<T>` 或 `future<T>` 的单一公开类型名，但第一版必须保持与当前 demo6 async 返回值口径一致。

### 标准库命名

采用短命名空间：

```xlang
task.spawn
task.all
task.race
task.cancel
```

不增加 demo5 风格的额外大命名空间层级。

## async generator

### 语法

采用以下两种声明形式：

```xlang
async func stream() as generator<int>
    auto v = await loadValue()
    yield v
end

func stream() as async generator<int>
    auto v = await loadValue()
    yield v
end
```

消费语法：

```xlang
for await item in stream()
    print(item)
end
```

### 核心语义

- DEMO6 RC0 的公开句柄统一落到 `generator<T>`；`async` 是函数/返回类型上的语义标记，表示该生成器 worker 内允许 `await`。
- `async generator<T>` 不等同于 `future<generator<T>>`，调用后直接得到可迭代的 generator 句柄，不需要先 `await` 得到 generator。
- `async func name(...) as generator<T>` 与 `func name(...) as async generator<T>` 语义一致，前者强调函数体异步能力，后者强调返回类型是异步生成器。
- 函数体内允许同时出现 `await` 和 `yield`。
- `yield expr` 产生一个 `T`。
- `await expr` 等待 future/task，并在恢复后继续执行生成器。
- `return` 只结束生成器，不返回额外值。
- async generator 不支持多返回值。
- async generator 不支持 `return value`。
- 异常在生成器内部捕获并存储，消费端推进时重新抛出。
- 消费端提前退出时必须关闭 async generator，并执行清理逻辑。

### for await

`for await item in expr` 要求 `expr` 是 `generator<T>`。

第一版固定支持：

- `async generator<T>` 返回值。
- 普通 `generator<T>`；此时 `for await` 与 `for in` 等价，便于调用方在静态类型被擦成 `generator<T>` 后仍能统一消费。
- 后续可扩展到实现异步迭代协议的 class。

生成规则：

```text
gen = expr
while gen.next()
    item = gen.value
    body
end
close gen
```

真实实现不要求暴露 `next/value/close` 名称给用户；它们可以是内部 runtime API。

### 状态机模型

async generator 应使用统一 suspend point 模型：

```text
suspend point:
    kind = await | yield
    source location
    cleanup scope
    resume state
```

不要照搬 demo5 的 asyncState/generatorState 投影实现，但可以吸收它的测试思路。

RC0 实现口径：

- 复用现有 `XL6_Generator` / XRT coroutine 运行时。
- 生成器 worker 内遇到 `await future<T>` 时阻塞等待该 future 完成，再继续 resume/yield。
- 该方案不额外创建 async iterator 句柄，避免在发布候选前引入第二套生成器生命周期。
- 后续如需真正非阻塞 async iterator，可在保持 `for await` 语法不变的前提下升级内部 runtime。

实现建议：

- 在语义检查后建立 coroutine plan。
- plan 记录所有 `await`、`yield`、`return`、`throw`、cleanup 和循环控制点。
- cgen 根据 plan 输出状态结构体、worker 函数、next/close/await 回调。
- 每个 suspend point 都保留源位置，供调试器使用。

### 异常与取消

- async generator 内部 `throw "message"` 存入生成器错误槽。
- 消费端 `for await` 推进时，如果发现错误槽，则重新抛出。
- 如果消费端 break/return/throw 提前离开循环，必须 close 生成器。
- 如果 task cancel 作用于 async generator，应在下一次 await 或 yield 边界观察取消。

### 暂缓能力

第一版不实现：

- async generator 多返回值。
- `yield from`。
- 用户自定义 async iterable 协议。
- 复杂背压策略。
- 多线程并行推进同一个 async generator。

## 后续完整调试器体系（首发暂缓）

### 命令行入口

调试器完整实现并重新评审后，计划仍使用 `xl.exe` 作为统一入口：

```bat
xl.exe dbg <file> [--break line] [--stop-on-entry] [--commands file] [--save-temps] [-- <args>...]
xl.exe dap
```

说明：

- `dbg` 是本地交互式调试入口。
- `dap` 是 Debug Adapter Protocol stdio 入口。
- 调试器能力不进入 `xl.dll` 运行版。
- dump 类开发辅助功能仍按 CLI 工具口径存在，不进入 DLL 运行版。

首发边界：

- 上述命令当前不存在，调用时走普通未知命令诊断。
- 不保留只打印 probe 摘要、合成栈帧或占位变量的简化实现。
- 重新开放命令前，必须一次满足本节后续的调试器模型和验收要求。

### 调试器模型

采用源码级 probe 调试器，而不是 OS 级 DWARF/PDB 调试器。

原因：

- xlang 当前通过 C codegen 和 TCC 执行，OS 级调试看见的是生成 C 和机器码。
- 用户需要的是 xlang 源码行、函数栈、局部变量、异常和 coroutine 状态。
- 编译期插桩可以直接保存 xlang 语义信息，更适合 demo6 当前架构。

### 架构分层

```text
IDE / VS Code
    |
    | DAP JSON over stdio
    v
xl.exe dap adapter
    |
    v
debug controller
    |
    v
debug probe runtime
    |
    v
generated C debuggee
```

职责：

- DAP adapter：解析 DAP 消息，发送 response/event。
- Debug controller：管理断点、单步、暂停状态、frame、scope 和变量引用。
- Probe runtime：由生成 C 调用，记录当前位置、变量快照和调用栈，并在需要时暂停。
- Codegen debug mode：在函数、语句、变量、await/yield/throw 等位置插入 probe。

### 可暂停点

第一版必须覆盖：

- 函数入口。
- 变量声明和赋值后。
- `if` / `elseif` / `else` 分支入口。
- `for` / `while` / `do` 循环入口或迭代点。
- `return`。
- `break` / `continue`。
- `throw`。
- `assert` 失败前或失败时。
- `await` 前后。
- `yield` 前后。

### 断点

断点以 source path + line 为核心。

规则：

- launch 前可先接收断点，返回 pending 状态。
- 编译 debug 版本时建立 executable line table。
- 能绑定到可暂停点的断点标记为 verified。
- 不能绑定的断点保持 unverified，并给出原因。
- 第一版不支持条件断点、logpoint、data breakpoint。

### 单步

必须支持：

- continue。
- stepIn。
- next。
- stepOut。
- pause。
- stopOnEntry。

语义：

- stepIn：下一个可暂停点停。
- next：同栈深或更浅栈、源位置变化时停。
- stepOut：当前函数返回到上一层后停。
- pause：设置 pending pause，在下一个 probe 边界停。

### 调用栈

生成 C 在函数入口/退出插入 frame probe。

frame 至少包含：

- function name。
- source path。
- line / column。
- frame id。
- scope id。
- arguments snapshot。
- locals snapshot。
- coroutine state，可选。

async、generator 和 async generator 的逻辑栈必须以 xlang 视角呈现。第一版可以先展示当前恢复点和当前函数 frame，不要求完整还原跨 await 的历史调用链。

### 变量查看

第一版变量采用快照模型。

支持：

- 参数。
- 当前 frame 局部变量。
- `bool`、`int`、`float`、`string`、`time`。
- `var` 的简要显示。
- array/list/set/dict 的长度和浅层元素展示。
- class/object 的字段展示，优先使用现有 runtime type/persistence 能力。

暂缓：

- setVariable。
- 完整表达式 evaluate。
- 任意深度对象展开。
- 捕获变量的完整 live 修改。

### DAP 第一版能力

必须支持以下请求或事件：

- `initialize`。
- `launch`。
- `setBreakpoints`。
- `setExceptionBreakpoints`。
- `configurationDone`。
- `threads`。
- `stackTrace`。
- `scopes`。
- `variables`。
- `continue`。
- `next`。
- `stepIn`。
- `stepOut`。
- `pause`。
- `disconnect`。
- `stopped` event。
- `terminated` event。
- `output` event。

第一版只承诺 stdio DAP。

`--listen` TCP 模式暂缓；如果保留参数，应返回 unsupported，不进入半实现状态。

### dbg 本地交互入口

`xl.exe dbg <file>` 与 DAP 共用 debug controller 和 probe runtime。

第一版命令：

```text
b file:line
run
c
n
s
out
bt
locals
p name
q
```

当前 CLI 批处理入口用 `--commands` 文件承载上述命令子集；交互式 REPL 可以在不改变 probe runtime 的前提下继续扩展。本地 dbg 只作为调试器开发和人工验证入口，不单独维护另一套断点/单步逻辑。

## 与 XRT 的关系

以下能力优先进入 XRT 或基于 XRT 扩展：

- task/future 聚合。
- task cancellation token。
- thread-safe wait。
- race/all 等待原语。
- coroutine/generator 所需底层同步。
- 调试器 controller 所需 mutex/cond/event 基础能力。

xlang 侧负责：

- 语法和语义检查。
- 类型推断。
- runtime API 选择。
- 参数捕获和释放。
- 生成 probe。
- 生成 coroutine 状态机。
- DAP 协议文本处理。

## 与 C ABI 的关系

task、future、generator、async generator 都应有明确 C 可见表示。

第一版至少保证：

- C 可以持有 task/future/generator 的 opaque handle。
- C 可以释放 handle。
- C 可以等待 future/task。
- C 可以通过 xvalue 或 typed pointer 取得结果。

暂不要求 C 直接实现 async generator 状态机。

## 测试与验收

### task API 验收

必须包含：

- `task.spawn` 普通函数。
- `task.spawn` 闭包。
- `task.spawn` 带参数和返回值。
- `task.spawn` 抛异常并由 await 重新抛出。
- `task.all` 全成功。
- `task.all([])` 负例。
- `task.all` 子任务失败。
- `task.race` 成功优先。
- `task.race` 失败优先。
- `task.race([])` 负例。
- `task.cancel` 已完成任务。
- `task.cancel` 运行中任务。
- 参数捕获中 class/function/xvalue 的 retain/destroy 边界。

### async generator 验收

必须包含：

- 基础 `yield`。
- `await` 后 `yield`。
- 多个 `await` 和多个 `yield` 混合。
- `for await` 正常消费。
- `for await` 中 `break` 触发 close。
- generator 内 `throw` 在消费端重新抛出。
- 循环、if、try/finally 与 `await/yield` 混合。
- 禁止 `return value`。
- 禁止多返回值。
- 禁止 `for await` 消费非 `generator<T>` 表达式。

### 调试器验收

必须包含：

- `xl.exe dbg` 基础命令 smoke。
- `xl.exe dap` stdio smoke。
- initialize/launch/setBreakpoints/continue/terminated。
- stopOnEntry。
- breakpoint hit。
- stepIn/next/stepOut。
- stackTrace。
- scopes/variables。
- 异常 stopped。
- await/yield 暂停点。
- async generator 恢复点。
- unsupported request 返回稳定错误。
- DAP stdout 不被普通日志污染。

### 守门要求

- 新增测试必须进入 `test.bat`。
- 新增范例必须进入 `release/examples/README.md`。
- 新增代码生成规则必须进入 `demo6/cgenrule`。
- 调试器代码不得进入 `xl.dll` 运行版。
- `xl.dll` 体积和导出 API 不应因调试器增加非必要内容。

## 当前边界

本设计固定方向，不表示当前代码已经实现以上三项完整能力。

实现前必须继续遵守：

- 不为兼容 demo5 保留旧语法。
- 不引入临时双路线。
- 不为了调试器污染 release DLL。
- 不绕过 XRT 重复制造任务/同步底层设施。
- 不把 OS 级调试、DWARF/PDB 作为第一版目标。
