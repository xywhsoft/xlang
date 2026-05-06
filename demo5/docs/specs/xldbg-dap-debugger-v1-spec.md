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
- [ ] 确认断点不可执行行的策略：拒绝或移动到下一可执行行

验收：

- [ ] 新贡献者能从文档理解整体架构
- [ ] spec 中每个实现任务都能独立跟踪状态
- [ ] 非目标明确，避免 V1 范围膨胀

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
- [ ] 定义 controller 状态：`starting` / `running` / `stopped` / `terminated`
- [ ] 定义 stop reason：`entry` / `breakpoint` / `step` / `exception` / `pause`
- [ ] 定义 step mode：`none` / `stepIn` / `next` / `stepOut`
- [x] 定义断点结构：source path、line、column、verified、actual line、message
- [ ] 定义 frame 结构：id、function name、source path、line、column、scope id、func index
- [ ] 定义 variable 结构：name、type、address/value、children reference
- [x] controller 支持 init/free
- [~] controller 支持 set/clear breakpoints，目前为 launch 前全局缓存表
- [~] controller 支持 enter/leave frame，目前只维护 frame depth
- [~] controller 支持 update current location，目前由 `xl_dbg_stmt` 更新 source/line/column
- [~] controller 支持 stop/wait/resume，目前可阻塞/恢复 debuggee 线程，尚未支持 step mode
- [~] controller 支持 snapshot stack frames，目前只有最近 stopped/current location 的单帧
- [ ] controller 支持 snapshot scopes
- [ ] controller 支持 snapshot variables

验收：

- [ ] controller 单元测试覆盖 breakpoint 命中
- [ ] controller 单元测试覆盖 stepIn
- [ ] controller 单元测试覆盖 next 不进入更深 frame
- [ ] controller 单元测试覆盖 stepOut
- [ ] controller 单元测试覆盖 terminated 状态

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
- [~] probe 调用时能访问当前 controller，目前支持当前位置、frame depth 和 probe count

验收：

- [x] 手写 C smoke 能成功链接 `xl_dbg_stmt`
- [x] 命中 probe 后 debuggee 线程可阻塞
- [x] DAP/controller resume 后 debuggee 可继续执行
- [ ] 无断点且无 step 时 probe 开销只做轻量检查

## 5. Codegen 插桩

- [x] C codegen context 增加 debug instrumentation flag
- [x] 新增 debug-only `xlCgenMirDebug(...)` 入口，普通 `xlCgenMir(...)` 默认不插桩
- [x] 生成 C 头部声明 probe 函数原型
- [x] 函数入口插入 `xl_dbg_enter_func`
- [~] 普通函数出口插入 `xl_dbg_leave_func`，已覆盖显式 return 和尾部默认 return，finally/async/generator 仍需统一 cleanup 策略
- [~] return 前保证 leave frame 或使用统一 cleanup label，目前非 finally return 已覆盖
- [~] `XL_MIR_BIND` 前插入 `xl_dbg_stmt`，当前为语句入口级插桩
- [~] `XL_MIR_BRANCH` 前插入 `xl_dbg_stmt`，当前为语句入口级插桩
- [~] `XL_MIR_TRY` 前插入 `xl_dbg_stmt`，当前为语句入口级插桩
- [~] `XL_MIR_WHILE` 插入 `xl_dbg_stmt`，当前为 loop 语句入口，尚不是条件检查前
- [~] `XL_MIR_FORIN` 插入 `xl_dbg_stmt`，当前为 loop 语句入口，尚不是每次迭代前
- [~] `XL_MIR_RETURN` 前插入 `xl_dbg_stmt`，当前为语句入口级插桩
- [~] `XL_MIR_BREAK` 前插入 `xl_dbg_stmt`，当前为语句入口级插桩
- [~] `XL_MIR_CONTINUE` 前插入 `xl_dbg_stmt`，当前为语句入口级插桩
- [~] `XL_MIR_THROW` 前插入 `xl_dbg_stmt`，当前为语句入口级插桩
- [~] `XL_MIR_ASSERT` 前插入 `xl_dbg_stmt`，当前为语句入口级插桩
- [~] `XL_MIR_YIELD` 前插入 `xl_dbg_stmt`，当前为语句入口级插桩
- [ ] 局部变量声明后注册基础类型变量
- [ ] 参数进入函数后注册 arguments
- [ ] 对不可映射或 synthetic 节点避免插入用户可见停点

验收：

- [x] `xldbg cgen --dump-c` 能看到 probe 调用
- [x] 未启用 debugger 时普通 `xldbg cgen` 不输出 probe 调用
- [ ] 启用 debugger 时可命中函数第一行
- [ ] 循环内断点每次迭代都可命中
- [ ] return 前断点可命中

## 6. Executable line table

- [ ] 从 MIR 收集 executable line table
- [ ] 记录 source path、line、column、kind、function
- [ ] 去重同一行多个停点
- [ ] 支持查询 requested line 是否可执行
- [ ] 支持返回 actual line
- [ ] 支持 dump line table 便于测试

验收：

- [ ] 空行断点返回 `verified=false` 或移动到实际行
- [ ] 注释行断点返回 `verified=false` 或移动到实际行
- [ ] 函数声明行可按策略停在函数入口
- [ ] 循环体语句行可验证为 executable

## 7. DAP transport

- [x] 新增 DAP message reader
- [x] 支持 `Content-Length` header framing
- [x] 支持 UTF-8 JSON body
- [x] 支持从 stdin 读取 DAP request
- [x] 支持向 stdout 写 DAP response/event
- [x] 所有非 DAP 日志写 stderr 或 trace 文件，不能污染 stdout
- [x] 请求 seq 和 response request_seq 正确关联
- [x] 未识别命令返回 error response
- [ ] JSON parse 错误返回协议错误并终止 session
- [ ] TCP listen 预留接口，V1 可不启用

验收：

- [x] 可用脚本发送 `initialize` 并收到合法 response
- [x] stdout 中没有非 DAP 文本
- [x] 连续多条 DAP 消息可正确 framing
- [x] response body 符合 DAP 基本格式

## 8. DAP initialize 和 launch

- [x] 支持 `initialize`
- [x] 返回 capabilities
- [x] capabilities 声明支持 configurationDoneRequest
- [x] capabilities 声明支持 conditional breakpoints 为 false
- [x] capabilities 声明支持 evaluate 为 false 或 repl-only false
- [x] 发送 `initialized` event
- [~] 支持 `launch`，当前为同步运行到结束并发送 `terminated`
- [x] `launch` 参数包含 `program`
- [~] `launch` 可选 `stopOnEntry`，当前接受但尚未暂停在入口
- [~] `launch` 编译 XL 并启动 debuggee，当前尚未拆到独立 debuggee thread
- [x] 支持 `configurationDone`
- [x] `launch` 运行期间 suppress debuggee stdout，避免污染 DAP stdout

验收：

- [~] VS Code 能完成 initialize/configurationDone/launch，当前 launch 后直接运行到结束
- [ ] `stopOnEntry=true` 时在入口暂停
- [ ] launch 编译失败时返回 error response
- [x] launch 后程序结束能发送 `terminated` event

## 9. DAP breakpoints

- [x] 支持 `setBreakpoints`
- [x] 支持 `source.path`
- [x] 支持 line breakpoint
- [x] 返回 `Breakpoint.verified`
- [x] 返回 `Breakpoint.line`
- [ ] 不支持条件断点时对 condition 返回 unsupported message
- [~] 支持清空文件断点，目前清空全局缓存表
- [x] launch 前设置的断点可缓存
- [ ] launch 后设置的断点立即生效
- [x] 支持 `setExceptionBreakpoints` 空配置
- [x] `setBreakpoints` 在未接入 executable line table 前返回 unverified breakpoint 列表

验收：

- [ ] IDE 设置断点后返回 verified breakpoint
- [~] 断点命中后发送 `stopped` reason=`breakpoint`，当前为运行完成后保留 stopped 状态的过渡实现
- [ ] 清除断点后不再暂停
- [ ] 同一文件多断点可同时生效

## 10. DAP execution control

- [x] 支持 `threads`
- [x] V1 固定返回 thread id `1`
- [x] 支持 `continue`
- [ ] 支持 `next`
- [ ] 支持 `stepIn`
- [ ] 支持 `stepOut`
- [ ] 支持 `pause`
- [x] 支持 `disconnect`
- [x] continue 后返回 `allThreadsContinued=true`
- [x] debuggee 暂停时才能接受 step/continue
- [ ] debuggee 运行中收到 step/continue 返回合理错误

验收：

- [~] continue 后运行到下一个断点或程序结束，多个后续断点仍需更多用例覆盖
- [ ] next 不进入被调用函数
- [ ] stepIn 能进入被调用函数
- [ ] stepOut 能从当前函数返回到调用者后暂停
- [ ] pause 能在下一个 probe 位置暂停

## 11. DAP stack, scopes, variables

- [~] 支持 `stackTrace`，当前返回单帧
- [~] frame 名称使用 XL 函数名，目前固定为 `main`
- [x] frame source 使用 XL source path
- [x] frame line/column 使用当前 XL 位置
- [~] 支持 `scopes`，当前返回空容器
- [x] V1 返回 `Arguments` scope
- [x] V1 返回 `Locals` scope
- [~] 支持 `variables`，当前返回空变量列表
- [ ] int 显示十进制值
- [ ] bool 显示 `true` / `false`
- [ ] float 显示稳定文本
- [ ] string 显示文本值
- [ ] var 显示 `xl_value_to_string` 或等价格式
- [ ] variablesReference 在每次 stopped 后重新生成
- [ ] continue 后旧 variablesReference 失效

验收：

- [~] 断点暂停后 IDE 能显示调用栈，目前为单帧过渡实现
- [~] 断点暂停后 IDE 能显示函数参数 scope，参数值采集未完成
- [~] 断点暂停后 IDE 能显示局部变量 scope，局部变量值采集未完成
- [ ] 多层函数调用栈顺序正确
- [ ] 继续运行后旧变量引用不会崩溃

## 12. 异常和结束

- [ ] uncaught exception 转换为 DAP stopped event reason=`exception`
- [ ] exception frame line/column 使用现有 exception location
- [ ] 用户 continue 后发送 terminated event
- [ ] 正常 exit 发送 exited event
- [ ] 正常 exit 发送 terminated event
- [ ] exit code 使用 host run result
- [ ] 编译错误返回 launch error response
- [ ] TCC link/runtime failure 返回 output event 和 terminated event

验收：

- [ ] throw 未捕获时 IDE 停在 throw 源码行
- [ ] 正常程序结束 IDE debug session 关闭
- [ ] 编译错误不会启动 debuggee thread
- [ ] runtime error 有可读错误信息

## 13. REPL 调试入口

- [ ] `xldbg dbg <file>` 使用同一 debug controller
- [ ] 支持 `break <line>`
- [ ] 支持 `run`
- [ ] 支持 `continue`
- [ ] 支持 `next`
- [ ] 支持 `step`
- [ ] 支持 `out`
- [ ] 支持 `bt`
- [ ] 支持 `locals`
- [ ] 支持 `quit`

验收：

- [ ] 不依赖 IDE 即可验证 controller
- [ ] REPL 和 DAP 的断点/单步语义一致
- [ ] REPL 输出不影响 DAP stdout

## 14. 测试

- [ ] 新增 DAP framing 单元测试
- [ ] 新增 DAP initialize golden 测试
- [ ] 新增 DAP setBreakpoints golden 测试
- [ ] 新增 DAP launch stopOnEntry 测试
- [ ] 新增 DAP breakpoint hit 测试
- [ ] 新增 DAP continue 测试
- [ ] 新增 DAP stepIn/next/stepOut 测试
- [ ] 新增 stackTrace/scopes/variables 测试
- [ ] 新增 exception stop 测试
- [ ] 新增普通 `xldbg run` 回归，确保未启用 debugger 时行为不变

验收：

- [x] `build_gcc.bat` 通过
- [ ] `regress.bat` 通过
- [ ] 新增 DAP 测试在 CI 可运行
- [ ] 测试不依赖 VS Code

手动验收：

- [x] `release\xldbg.exe --help` 显示 `dbg` 和 `dap`
- [x] `release\xl.exe --help` 不显示 `dbg` 和 `dap`
- [x] `release\xl.exe` / `release\xlang.dll` 二进制未发现 `Debug Adapter Protocol` / `interactive debugger` / `xl_dbg_stmt` / DAP launch stub 字符串
- [x] stdio DAP `initialize` + `setBreakpoints` + `disconnect` 连续请求返回合法 framed responses/events
- [x] stdio DAP `initialize` + `setBreakpoints` + `launch` 可运行程序并返回 `terminated` event
- [x] stdio DAP `initialize` + `setBreakpoints` + `launch` + `stackTrace` + `continue` 可真实阻塞 debuggee、返回 `stopped`、单帧 line 2、恢复后 `terminated`
- [x] stdio DAP stopped 后 `scopes` 返回 `Arguments` / `Locals`，`variables` 返回空列表
- [x] DAP `launch` 运行期间未发现 `RunResult` 或 debuggee stdout 污染 DAP stdout
- [x] `release\tmp\gcc_xl_sources.rsp` / `release\tmp\gcc_xlang_sources.rsp` 未包含 `src/debug/`、`xl_cli_debug.c`、`xl_driver_debug.c`

## 15. VS Code 插件最小接入

- [ ] 定义 debug type，例如 `xlang`
- [ ] launch configuration 支持 `program`
- [ ] launch configuration 支持 `stopOnEntry`
- [ ] adapter command 指向 `release\xldbg.exe`
- [ ] adapter args 使用 `dap`
- [ ] 插件不实现调试语义，只转发 DAP
- [ ] README 提供 launch.json 示例

验收：

- [ ] VS Code 中 F5 可启动 XL 调试
- [ ] 编辑器断点可设置并命中
- [ ] Debug Console 不出现协议污染
- [ ] Variables / Call Stack 面板有内容

## 16. 2026-05-06 initial design capture

- [x] 确认现有 `xldbg` 是 debug-mode driver
- [x] 确认当前运行链路是 XL -> C -> libtcc memory execution
- [x] 确认 AST/HIR/MIR 已携带源码行列
- [x] 确认 host symbols 可作为 generated C 调用宿主 probe 的入口
- [x] 确认 V1 采用编译期插桩，不采用 OS 级调试
- [x] 确认 IDE 通信采用 DAP
