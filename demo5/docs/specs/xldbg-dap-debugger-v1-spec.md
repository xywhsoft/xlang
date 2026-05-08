# xldbg DAP 调试器 V1 可跟踪规格

状态说明：

- `[ ]` 未开始
- `[~]` 进行中
- `[x]` 完成
- `[!]` 阻塞或需要重新设计

## 0. 范围

V1 实现语言层源码调试器：

- `xldbg` 新增 DAP adapter 模式。
- 生成 C 插入 XL 调试 probe。
- IDE 通过 Debug Adapter Protocol 控制调试。
- 调试状态以 XL 源码、XL 调用栈、XL 局部变量为核心。
- 所有调试器实现严格隔离在 `xldbg` 中，不能有任何可执行代码进入 `xl.exe`。

V1 明确不做：

- OS 级 attach/debug。
- DWARF/PDB 输出。
- 完整表达式求值。
- 修改变量。
- 条件断点。
- logpoint。
- data breakpoint。
- 多 debuggee 线程。
- async/generator 完整逻辑栈重建。

## 1. 文档和设计冻结

- [x] 新增 DAP 调试器设计文档
- [x] 新增 V1 可跟踪规格文档
- [x] 在 README 或开发文档中链接设计文档和 spec
- [x] 记录 xldbg-only 隔离硬性约束
- [x] 确认 V1 命令名称：`dbg` / `dap`
- [x] 确认 V1 使用 stdio DAP，`--listen` 只预留 TCP 参数并返回 unsupported
- [x] 确认断点不可执行行的策略：V1 不移动断点；不可执行行返回 `verified=false`

验收：

- [x] 新贡献者能从文档理解整体架构；设计文档覆盖架构、协议、线程模型、隔离规则和命令入口
- [x] spec 中每个实现任务都能独立跟踪状态
- [x] 非目标明确，避免 V1 范围膨胀

## 2. CLI 命令

- [x] `XL_Command` 增加 `XL_CMD_DBG`
- [x] `XL_Command` 增加 `XL_CMD_DAP`
- [x] `xlCliParse(...)` 支持 `dbg <file>`
- [x] `xlCliParse(...)` 支持 `dap`
- [x] `dap` 默认不要求 input file
- [x] `dap --listen <host:port>` 可解析但 V1 可先报 unsupported
- [x] `xlCliPrintHelp(...)` 输出新命令说明
- [x] release build 不暴露 debug-only 命令，或明确只在 `xldbg` 暴露
- [x] 公共 CLI 改动全部处于 `XLANG_DEBUGMODE_ENABLED` 宏保护内

## 2.1 xldbg-only 隔离

- [x] `XL_CMD_DBG` / `XL_CMD_DAP` 只在 `XLANG_DEBUGMODE_ENABLED` 下定义
- [x] `dbg` / `dap` 命令表项只在 `XLANG_DEBUGMODE_ENABLED` 下编译
- [x] `dap` input-file 例外只在 `XLANG_DEBUGMODE_ENABLED` 下编译
- [x] `--listen` 解析只在 `xl_cli_debug.c` 中实现
- [x] DAP adapter 实现只放入 xldbg 专用 debug 文件
- [x] Debug controller 实现只放入 xldbg 专用 debug 文件，并从 `xl.exe` / `xlang.dll` source list 排除
- [x] Probe runtime 实现只放入 xldbg 专用 debug 文件，并从 `xl.exe` / `xlang.dll` source list 排除
- [x] `xl.exe` help 输出不包含 `dbg` / `dap`
- [x] `xl.exe` 二进制不包含 DAP/debugger/probe 字符串和符号

验收：

- [x] `release\xldbg.exe --help` 显示 `dbg` 和 `dap`
- [x] `release\xldbg.exe dbg examples\full\run_basic.xl` 能进入调试启动路径
- [x] `release\xldbg.exe dap` 能等待 DAP initialize 消息
- [x] `release\xl.exe --help` 不显示 debug-only 命令

## 3. Debug controller 核心

- [x] 新增 `src/debug/xl_debugger_debug.h`
- [x] 新增 `src/debug/xl_debugger_debug.c`
- [x] 定义 controller 状态：`starting` / `running` / `stopped` / `terminated`
- [x] 定义 stop reason：`entry` / `breakpoint` / `step` / `exception` / `pause`
- [x] 定义 step mode：`none` / `stepIn` / `next` / `stepOut`
- [x] 定义断点结构：source path、line、column、verified、actual line、message
- [x] 定义 frame 结构：function name、source path、line、column、func index、argument/local range，scope id 由 DAP reference 编码生成
- [x] 定义 variable 结构：name、type、value、children reference；已按 frame 区分 Arguments / Locals，`var` 中的 array/list 可展开子项
- [x] controller 支持 init/free
- [x] controller 支持 set/clear breakpoints，支持按 source 替换、launch 前缓存、line table 存在时绑定 verified
- [x] controller 支持 enter/leave frame，维护 frame stack 和 frame depth
- [x] controller 支持 update current location，由 `xl_dbg_stmt` 更新 source/line/column
- [x] controller 支持 stop/wait/resume，可阻塞/恢复 debuggee 线程，并支持 `stepIn` / `next` / `stepOut` / `pause`
- [x] controller 支持 snapshot stack frames，stopped 时复制 frame stack
- [x] controller 支持 snapshot scopes，DAP 按 frameId 返回 Arguments / Locals
- [x] controller 支持 snapshot variables，stopped 时复制 frame-specific primitive arguments / locals 快照
- [x] frame variable 池 V1 容量为 128 entries，避免 debug controller 在 Windows 默认栈上过大；需要更高容量时应改为堆分配 controller 或变量池

验收：

- [x] controller 单元测试覆盖 breakpoint 命中；`xl5_test debug` 覆盖 `debug.controller_breakpoint`
- [x] controller 单元测试覆盖 stepIn；`xl5_test debug` 覆盖 `debug.controller_step_in`
- [x] controller 单元测试覆盖 next 不进入更深 frame；`xl5_test debug` 覆盖 `debug.controller_next`
- [x] controller 单元测试覆盖 stepOut；`xl5_test debug` 覆盖 `debug.controller_step_out`
- [x] controller 单元测试覆盖 terminated 状态；`xl5_test debug` 覆盖 `debug.controller_terminated_state`

## 4. Probe runtime

- [x] 新增宿主 probe API：`xl_dbg_stmt(...)`
- [x] 新增宿主 probe API：`xl_dbg_enter_func(...)`
- [x] 新增宿主 probe API：`xl_dbg_leave_func(...)`
- [x] 新增宿主 probe API：`xl_dbg_local_int(...)`
- [x] 新增宿主 probe API：`xl_dbg_local_bool(...)`
- [x] 新增宿主 probe API：`xl_dbg_local_float(...)`
- [x] 新增宿主 probe API：`xl_dbg_local_string(...)`
- [x] 新增宿主 probe API：`xl_dbg_local_value(...)`
- [x] probe API 在非调试模式下不可见
- [x] probe API 通过 debug-only `xlDebugRegisterHostSymbols(...)` 注册给 TCC
- [x] Windows 下注册 `_name` / `__imp_name` 兼容别名
- [x] probe 调用时能访问当前 controller，支持当前位置、当前函数名、frame depth、probe count 和 primitive arguments / locals

验收：

- [x] 手写 C smoke 能成功链接 `xl_dbg_stmt`
- [x] 命中 probe 后 debuggee 线程可阻塞
- [x] DAP/controller resume 后 debuggee 可继续执行
- [x] local probe 能记录 `int` / `bool` / `float` / `string` 文本值
- [x] argument probe 能记录 `int` / `bool` / `float` / `string` 文本值
- [x] 无断点且无 step 时 probe 开销只做轻量检查；`xl_dbg_stmt` 更新当前位置后在 zero-breakpoint / no-step 路径直接早退

## 5. Codegen 插桩

- [x] C codegen context 增加 debug instrumentation flag
- [x] 新增 debug-only `xlCgenMirDebug(...)` 入口，普通 `xlCgenMir(...)` 默认不插桩
- [x] 生成 C 头部声明 probe 函数原型
- [x] 函数入口插入 `xl_dbg_enter_func`
- [x] 普通函数出口插入 `xl_dbg_leave_func`，已覆盖显式 return、尾部默认 return、sync/async finally return escape dispatch、async 非 finally 直接返回和 async linear 外层返回；generator 逻辑栈是 V1 非目标
- [x] 普通函数 return 前保证 leave frame 或使用统一 cleanup label；generator return 由 executor done cleanup 收束，但 V1 不维护 generator 逻辑栈 frame
- [x] `XL_MIR_BIND` 前插入 `xl_dbg_stmt`，V1 为语句入口级插桩
- [x] `XL_MIR_BRANCH` 前插入 `xl_dbg_stmt`，V1 为语句入口级插桩
- [x] `XL_MIR_TRY` 前插入 `xl_dbg_stmt`，V1 为语句入口级插桩
- [x] `XL_MIR_WHILE` 插入 `xl_dbg_stmt`，V1 为 loop 语句入口级插桩
- [x] `XL_MIR_FORIN` 插入 `xl_dbg_stmt`，V1 为 loop 语句入口级插桩
- [x] `XL_MIR_RETURN` 前插入 `xl_dbg_stmt`，V1 为语句入口级插桩
- [x] `XL_MIR_BREAK` 前插入 `xl_dbg_stmt`，V1 为语句入口级插桩
- [x] `XL_MIR_CONTINUE` 前插入 `xl_dbg_stmt`，V1 为语句入口级插桩
- [x] `XL_MIR_THROW` 前插入 `xl_dbg_stmt`，V1 为语句入口级插桩
- [x] `XL_MIR_ASSERT` 前插入 `xl_dbg_stmt`，V1 为语句入口级插桩
- [x] `XL_MIR_YIELD` 前插入 `xl_dbg_stmt`，V1 为语句入口级插桩
- [x] 局部变量声明后注册变量，目前覆盖普通 `XL_MIR_BIND` 的 `int` / `bool` / `float` / `string` / `var`
- [x] 参数进入函数后注册 arguments，目前覆盖普通参数的 `int` / `bool` / `float` / `string` / `var`
- [x] 对不可映射或 synthetic 节点避免插入用户可见停点；codegen 和 executable line table 均要求用户源码 line/column 大于 0，并继续解包赋值语句外层 `Value @ 0:0 text=""` wrapper

验收：

- [x] `xldbg cgen --dump-c` 能看到 probe 调用
- [x] 未启用 debugger 时普通 `xldbg cgen` 不输出 probe 调用
- [x] `xldbg cgen --dump-c` 能看到普通局部变量后的 `xl_dbg_local_*` 调用
- [x] `xldbg cgen --dump-c` 能看到普通函数参数后的 `xl_dbg_arg_*` 调用
- [x] 启用 debugger 时可命中函数第一行；smoke 覆盖 `bit_ops_basic.xl:1`
- [x] 循环内断点每次迭代都可命中；smoke 覆盖 loop body assignment 连续 3 次停在 line 4
- [x] return 前断点可命中；smoke 覆盖 `bit_ops_basic.xl:9`

## 6. Executable line table

- [x] 从 MIR 收集 executable line table
- [x] 记录 source path、line、column、MIR kind、function；`xldbg/executableLines` 返回 `kind` / `function` 元数据
- [x] 去重同一行多个停点
- [x] 支持查询 requested line 是否可执行
- [x] 支持返回 actual line，V1 当前不移动断点，因此 actual line 等于 requested line
- [x] 支持 dump line table 便于测试；DAP 自定义请求 `xldbg/executableLines` 返回 `source` 和 `lines[{line,column,kind,function}]`

验收：

- [x] 空行/不可执行行断点返回 `verified=false`；smoke 覆盖 `bit_ops_basic.xl:10`
- [x] 注释行断点返回 `verified=false`；smoke 覆盖 `//` 注释行
- [x] 函数声明行可按策略停在函数入口；line table 包含 `XL_MIR_FUNC`
- [x] 循环体语句行可验证为 executable；smoke 覆盖赋值语句 wrapper 解析到真实 source line
- [x] `xldbg/executableLines` 可 dump 当前 launch 的 line table；smoke 覆盖 `bit_ops_basic.xl` 包含 1/2/9/11 且不包含 10，并验证 kind/function 元数据

## 7. DAP transport

- [x] 新增 DAP message reader
- [x] 支持 `Content-Length` header framing
- [x] 支持 UTF-8 JSON body
- [x] 支持从 stdin 读取 DAP request
- [x] 支持向 stdout 写 DAP response/event
- [x] debuggee 运行期间短轮询 stdin 并处理嵌套 DAP request
- [x] 支持 xldbg-only 自定义诊断 request：`xldbg/executableLines`
- [x] 所有非 DAP 日志写 stderr 或 trace 文件，不能污染 stdout
- [x] 请求 seq 和 response request_seq 正确关联
- [x] 未识别命令返回 error response
- [x] JSON parse 错误返回协议错误并终止 session
- [x] TCP listen 预留接口，V1 不启用并返回 not implemented

验收：

- [x] 可用脚本发送 `initialize` 并收到合法 response
- [x] stdout 中没有非 DAP 文本
- [x] 连续多条 DAP 消息可正确 framing
- [x] response body 符合 DAP 基本格式
- [x] debuggee 运行期间可处理 `pause` request 并返回 response
- [x] malformed JSON 返回 error response 后 clean exit；smoke 覆盖 `invalid JSON`
- [x] `dap --listen 127.0.0.1:0` 返回非 0，stderr 报 reserved TCP mode，stdout 不输出 DAP 内容

## 8. DAP initialize 和 launch

- [x] 支持 `initialize`
- [x] 返回 capabilities
- [x] capabilities 声明支持 configurationDoneRequest
- [x] capabilities 声明支持 conditional breakpoints 为 false
- [x] capabilities 声明支持 evaluate 为 false 或 repl-only false
- [x] 发送 `initialized` event
- [x] 支持 `launch`，可启动 debuggee worker thread，可入口暂停/断点暂停，无断点时发送 `terminated`
- [x] `launch` 参数包含 `program`
- [x] `launch` 可选 `stopOnEntry`
- [x] `launch` 编译 XL 并启动独立 debuggee thread
- [x] 支持 `configurationDone`
- [x] `launch` 运行期间 suppress debuggee stdout，避免污染 DAP stdout
- [x] `launch` 启动 debuggee thread 前执行编译 preflight，并绑定 executable line table / breakpoint verified 状态

验收：

- [x] VS Code 能完成 initialize/configurationDone/launch，DAP adapter 支持入口暂停、断点暂停和正常结束
- [x] `stopOnEntry=true` 时在入口暂停；手工验收：`bit_ops_basic.xl` 停在 `main:1`，reason=`entry`
- [x] launch 编译失败时返回 error response；手工验收：临时语义错误程序返回 `launch success=false`
- [x] launch 后程序结束能发送 `terminated` event

## 9. DAP breakpoints

- [x] 支持 `setBreakpoints`
- [x] 支持 `source.path`
- [x] 支持 line breakpoint
- [x] 返回 `Breakpoint.verified`
- [x] 返回 `Breakpoint.line`
- [x] 不支持条件断点时对 condition 返回 unsupported message，且不会在运行时命中
- [x] 支持清空文件断点，`setBreakpoints` 只替换当前 `source.path` 的断点
- [x] launch 前设置的断点可缓存
- [x] launch 后设置的断点立即生效
- [x] 支持 `setExceptionBreakpoints` 空配置
- [x] `setBreakpoints` 在 line table 尚未构建时返回 unverified breakpoint 列表；line table 存在时立即绑定 verified

验收：

- [x] IDE 设置断点后可获得 verified breakpoint；smoke 覆盖 launch 后 `breakpoint(reason=changed)` 事件把 line 2 更新为 `verified=true`
- [x] launch 后设置断点可立即绑定并在后续 probe 命中；smoke 覆盖运行中设置 `while` 行断点
- [x] 断点命中后发送 `stopped` reason=`breakpoint`，并阻塞 debuggee 等待 `continue`
- [x] 清除断点后不再暂停，smoke 覆盖：清空当前 source 后 launch 正常结束且无 stopped event
- [x] 同一文件多断点可同时生效；手工验收：同一 source 返回 2 个 breakpoint
- [x] 多文件断点设置互不覆盖；smoke 覆盖：清空文件 A 后文件 B 断点仍保留
- [x] 条件断点返回 `verified=false` 和 unsupported message；手工验收：只有条件断点时程序直接 `terminated`

## 10. DAP execution control

- [x] 支持 `threads`
- [x] V1 固定返回 thread id `1`
- [x] 支持 `continue`
- [x] 支持 `next`
- [x] 支持 `stepIn`
- [x] 支持 `stepOut`
- [x] 支持 `pause`
- [x] 支持 `disconnect`
- [x] continue 后返回 `allThreadsContinued=true`
- [x] debuggee 暂停时才能接受 step/continue
- [x] debuggee 未暂停或已结束时收到 step/continue 返回 error response
- [x] debuggee 已暂停时收到 pause 返回 error response

验收：

- [x] continue 后运行到下一个断点或程序结束；smoke 覆盖 3->4 连续断点、loop body repeated breakpoint 和正常退出
- [x] `next` 不进入被调用函数；smoke 覆盖：`bit_ops_basic.xl:2` next 后停在 `main:3`
- [x] `stepIn` 能进入被调用函数；smoke 覆盖：`anonymous_function_basic.xl:5` stepIn 后停在 lambda line 2
- [x] `stepOut` 能从当前函数返回到调用者后暂停；smoke 覆盖：`class_stage1_base_destroy_runtime.xl:29` stepOut 后停在 `main:35`
- [x] pause 能在下一个 probe 位置暂停；手工验收：临时长循环程序运行中收到 `pause` response 后发送 `stopped(reason=pause)`
- [x] pause 期间 debuggee stdout 不污染 DAP stdout；手工验收：临时打印循环程序运行中 pause，DAP stream 未出现 debuggee 输出
- [x] debuggee 已暂停时 pause 返回 error response；手工验收：`stopOnEntry` 停住后 `pause` 返回 `debuggee is already stopped`

## 11. DAP stack, scopes, variables

- [x] 支持 `stackTrace`，当前返回 probe frame stack
- [x] frame 名称使用 XL 函数名；smoke 覆盖 lambda 当前 frame 到 `main` 调用者 frame
- [x] frame source 使用 XL source path
- [x] frame line/column 使用当前 XL 位置
- [x] 支持 `scopes`，当前按 `frameId` 返回 Arguments / Locals 容器
- [x] V1 返回 `Arguments` scope
- [x] V1 返回 `Locals` scope
- [x] 支持 `variables`，当前 frame-specific Arguments / Locals 返回已采集的 primitive values
- [x] int 显示十进制值
- [x] bool 显示 `true` / `false`
- [x] float 显示稳定文本
- [x] string 显示文本值
- [x] var 显示 `xl_value_to_string` 文本，DAP smoke 覆盖 `var dyn = 41`
- [x] variablesReference 在每次 stopped 后重新生成，frame-specific ref 编码为 `base + stopSequence * 1000 + frameId`
- [x] continue 后旧 variablesReference 失效，stale ref 返回 error response

验收：

- [x] 断点暂停后 IDE 能显示调用栈；smoke 覆盖单帧和 lambda -> main 多帧 stackTrace
- [x] 断点暂停后 IDE 能显示当前/调用者 frame 的函数参数 scope；smoke 覆盖 lambda frame `a = 20` / `b = 22`
- [x] 断点暂停后 IDE 能显示当前/调用者 frame 的局部变量 scope；smoke 覆盖 primitive `mask = 10`、var `dyn = 41` 和 caller frame 独立 locals reference
- [x] 多层函数调用栈顺序正确，当前由 stepIn smoke 覆盖当前 frame 进入 lambda，手工验收覆盖 `__lambda_2_13 -> main`
- [x] 继续运行后旧变量引用不会崩溃；smoke 覆盖旧 locals ref 在下一次 stop 后返回 `stale variablesReference`

## 12. 异常和结束

- [x] uncaught exception 转换为 DAP stopped event reason=`exception`
- [x] exception frame line/column 使用现有 exception location
- [x] 用户 continue 后发送 terminated event
- [x] 正常 exit 发送 exited event
- [x] 正常 exit 发送 terminated event
- [x] exit code 使用 host run result
- [x] 编译错误返回 launch error response
- [x] TCC link/runtime failure 返回带 host trace tail 的 output event 和 terminated event；uncaught exception 返回 readable `stopped(reason=exception)`

验收：

- [x] throw 未捕获时 IDE 停在 throw 源码行；smoke 覆盖 `throw_basic.xl:2`
- [x] 正常程序结束 IDE debug session 关闭；手工验收：无断点运行发送 `exited(exitCode=42)` 后发送 `terminated`
- [x] 编译错误不会启动 debuggee thread；手工验收：launch preflight 失败后没有 `terminated` event
- [x] runtime error 有可读错误信息；smoke 覆盖 `stopped.text` 包含 `uncaught exception: boom`

## 13. REPL 调试入口

- [x] `xldbg dbg <file>` 使用同一 debug controller
- [x] 支持 `break <line>`
- [x] 支持 `run`
- [x] 支持 `continue`
- [x] 支持 `next`
- [x] 支持 `step`
- [x] 支持 `out`
- [x] 支持 `bt`
- [x] 支持 `locals`
- [x] 支持 `quit`

验收：

- [x] 不依赖 IDE 即可验证 controller；smoke 覆盖 `xldbg dbg` scripted REPL
- [x] REPL 和 DAP 的断点/单步语义一致，共用 controller、debuggee thread、breakpoint bind、step mode
- [x] REPL 输出不影响 DAP stdout；REPL 只在 `dbg` 命令路径写普通文本，DAP 路径仍保持 framed stdout

## 14. 测试

- [x] 新增 DAP framing smoke 测试：`test/dap/xldbg_dap_smoke.js`
- [x] 新增 DAP initialize smoke 测试
- [x] 新增 DAP threads smoke 测试
- [x] 新增 DAP setExceptionBreakpoints smoke 测试
- [x] 新增 DAP unsupported request smoke 测试
- [x] 新增 DAP malformed JSON smoke 测试
- [x] 新增 DAP TCP listen unsupported smoke 测试
- [x] 新增 DAP request validation error smoke 测试
- [x] 新增 DAP setBreakpoints golden 测试；当前 smoke 覆盖 setBreakpoints + verified changed event + launch 后断点立即生效 + non-executable/comment unverified + function entry breakpoint + return breakpoint + loop body repeated breakpoint + breakpoint hit + clear + conditional unsupported response + multi-file isolation
- [x] 新增 DAP launch stopOnEntry 测试
- [x] 新增 DAP breakpoint hit 测试
- [x] 新增 DAP continue 测试
- [x] 新增 DAP stepIn/next/stepOut 测试
- [x] 新增 DAP launch 编译失败测试
- [x] 新增 DAP pause 测试
- [x] 新增 DAP pause error 测试
- [x] 新增 DAP disconnect stopped/running 测试
- [x] 新增 stackTrace/scopes/variables 测试
- [x] 新增 multi-frame stackTrace/scopes/variables 测试
- [x] 新增 var variable display 测试
- [x] 新增 unknown variablesReference 测试
- [x] 新增 exception stop 测试
- [x] 新增 executable line table dump 测试
- [x] 新增普通 `xldbg run` 回归，确保未启用 debugger 时行为不变
- [x] 新增 `xldbg dbg` scripted REPL 回归，覆盖 breakpoint、run、bt、locals、continue、quit
- [x] 新增 `xl5_test debug` controller 单测组，覆盖 breakpoint、stepIn、next、stepOut、frame variables、terminated state
- [x] 新增 VS Code extension headless smoke：`test/dap/vscode_extension_smoke.js`，验证 package contribution、configuration provider 和 `DebugAdapterExecutable(xldbg, ["dap"])`

验收：

- [x] `build_gcc.bat` 通过
- [x] `regress.bat` 通过，包含新增 `debug` selector baseline
- [x] 新增 DAP 测试在 CI 可运行：`node test\dap\xldbg_dap_smoke.js`
- [x] 新增 VS Code extension smoke 在 CI 可运行：`node test\dap\vscode_extension_smoke.js`
- [x] 测试不依赖 VS Code

手动验收：

- [x] `release\xldbg.exe --help` 显示 `dbg` 和 `dap`
- [x] `release\xl.exe --help` 不显示 `dbg` 和 `dap`
- [x] `release\xl.exe` / `release\xlang.dll` 二进制未发现 `Debug Adapter Protocol` / `interactive debugger` / `xl_dbg_stmt` / DAP launch stub 字符串
- [x] stdio DAP `initialize` + `setBreakpoints` + `disconnect` 连续请求返回合法 framed responses/events
- [x] stdio DAP `initialize` + `setBreakpoints` + `launch` 可运行程序并返回 `terminated` event
- [x] stdio DAP 无断点正常退出时先返回 `exited(exitCode=42)`，再返回 `terminated`
- [x] stdio DAP `initialize` + `setBreakpoints` + `launch` + `stackTrace` + `continue` 可真实阻塞 debuggee、返回 `stopped`、单帧 line 2、恢复后 `terminated`
- [x] stdio DAP stopped 后 `scopes` 返回 `Arguments` / `Locals`
- [x] stdio DAP stopped 后 `stackTrace` 返回当前 XL 函数名，例如 `__lambda_2_13`
- [x] stdio DAP stopped 后 `stackTrace` 可返回多帧，例如 `__lambda_2_13 -> main`
- [x] stdio DAP stopped 后 `variablesReference=1001` 可返回已采集 primitive argument，例如 `a = 20`、`b = 22`
- [x] stdio DAP stopped 后 `variablesReference=1002` 可返回已采集 primitive local，例如 `mask = 10`
- [x] stdio DAP `scopes(frameId=1)` / `scopes(frameId=2)` 返回不同 frame-specific references，例如 `101001` / `101002`
- [x] stdio DAP `variables(201001)` 可返回当前 frame local，例如 `mask = 10`
- [x] stdio DAP stopped 后 `disconnect` 会清断点、恢复 debuggee，并发送 `terminated`
- [x] DAP `launch` 运行期间未发现 `RunResult` 或 debuggee stdout 污染 DAP stdout
- [x] stdio DAP 运行中 `pause` 可通过短轮询请求通道处理，并在后续 probe 发送 `stopped(reason=pause)`
- [x] `node test\dap\xldbg_dap_smoke.js` 覆盖 normal exit、`xldbg run` regression、`xldbg dbg` scripted REPL、compile failure、host runtime failure output、exception stop、threads、setExceptionBreakpoints、unsupported request、malformed JSON、TCP listen unsupported、request validation errors、breakpoint continue、verified breakpoint after launch、unverified non-executable/comment breakpoint、executable line table dump、function entry breakpoint、return breakpoint、loop body repeated breakpoint、set breakpoint while running、conditional breakpoint response、multi-file breakpoint isolation、cleared breakpoints、next、stepIn、stepOut、stackTrace/scopes/variables、multi-frame stackTrace/scopes/variables、var variable display、var array children reference、stale/unknown variablesReference、disconnect stopped/running、stopOnEntry、pause、pause errors
- [x] `release\tmp\gcc_xl_sources.rsp` / `release\tmp\gcc_xlang_sources.rsp` 未包含 `src/debug/`、`xl_cli_debug.c`、`xl_driver_debug.c`

## 15. VS Code 插件最小接入

- [x] 定义 debug type，例如 `xlang`；文档见 `docs/design/xldbg-vscode-dap-adapter.md`
- [x] launch configuration 支持 `program`
- [x] launch configuration 支持 `stopOnEntry`
- [x] adapter command 指向 `release\xldbg.exe`
- [x] adapter args 使用 `dap`
- [x] 插件不实现调试语义，只转发 DAP
- [x] 新增最小 VS Code 扩展骨架：`tools/vscode-xlang-debug`
- [x] README 提供 launch.json 示例；当前放在 VS Code DAP 接入文档和扩展 README，并由仓库 README 链接

验收：

- [x] VS Code 扩展注册 `xlang` debug type 和 launch configuration，headless smoke 覆盖 package contribution
- [x] VS Code configuration provider 可解析 `${file}` / `${workspaceFolder}` 并默认填充 `type=request=name=xldbg=stopOnEntry`
- [x] VS Code adapter descriptor 使用 `DebugAdapterExecutable(xldbg, ["dap"])`，不经 shell 包装 stdout
- [x] 编辑器断点、Call Stack、Variables、Debug Console 所需协议语义由 DAP smoke 覆盖；最终 VS Code 图形界面验收作为发布前外部检查项，不阻塞 V1 自动化完成状态

发布前 VS Code UI 外部检查：

- VS Code 中 F5 可启动 XL 调试。
- 编辑器断点可设置并命中。
- Debug Console 不出现 raw DAP frame 或 debuggee stdout 污染。
- Variables / Call Stack 面板有内容。

## 16. 2026-05-06 initial design capture

- [x] 确认现有 `xldbg` 是 debug-mode driver
- [x] 确认当前运行链路是 XL -> C -> libtcc memory execution
- [x] 确认 AST/HIR/MIR 已携带源码行列
- [x] 确认 host symbols 可作为 generated C 调用宿主 probe 的入口
- [x] 确认 V1 采用编译期插桩，不采用 OS 级调试
- [x] 确认 IDE 通信采用 DAP
