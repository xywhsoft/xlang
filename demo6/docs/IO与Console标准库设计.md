# IO 与 Console 标准库

## 公开常量

- `console.stdoutStream = 1`
- `console.stderrStream = 2`
- `console.colorNone = 0`
- `console.colorBasic = 1`
- `console.color256 = 2`
- `console.colorTrue = 3`
- `console.eventText = 1`
- `console.eventKey = 2`
- `console.eventResize = 3`
- `console.eventPaste = 4`
- `console.eventMouse = 5`
- `console.eventClosed = 6`
- `console.modShift = 1`
- `console.modCtrl = 2`
- `console.modAlt = 4`
- `console.modMeta = 8`
- `console.styleBold = 1`
- `console.styleDim = 2`
- `console.styleItalic = 4`
- `console.styleUnderline = 8`
- `console.styleReverse = 16`
- `console.clearScreen = 0`
- `console.clearLine = 1`
- `console.clearToEnd = 2`
- `console.keyEscape = 1114112`
- `console.keyEnter = 1114113`
- `console.keyTab = 1114114`
- `console.keyBackspace = 1114115`
- `console.keyUp = 1114116`
- `console.keyDown = 1114117`
- `console.keyLeft = 1114118`
- `console.keyRight = 1114119`
- `console.keyHome = 1114120`
- `console.keyEnd = 1114121`
- `console.keyInsert = 1114122`
- `console.keyDelete = 1114123`
- `console.keyPageUp = 1114124`
- `console.keyPageDown = 1114125`
- `console.keyF1 = 1114126`
- `io.createNew = 0`
- `io.truncate = 1`
- `io.seekStart = 0`
- `io.seekCurrent = 1`
- `io.seekEnd = 2`
- `io.lineNone = 0`
- `io.lineLf = 1`
- `io.lineCrLf = 2`
- `io.append = 2`


## 范围

语言公开面只有 `io` 与 `console`；XRT 的 `io_buffer/io_file/io_line`
并入 `io`，C 层模块仍独立裁剪。`file` 保持独立，继续提供编码识别、
转换、路径和高级文件功能；基础读写允许重合。没有引入 TUI 框架。

`io` 处理原始字节，不自动转换编码、换行或 NUL。`console` 处理严格
UTF-8；重定向保持 UTF-8 字节，Windows 真实控制台转换为 UTF-16。

## IO API

| 入口 | 合同 |
|---|---|
| `io.openReader(path) -> IoReader` | 只读打开基础文件 |
| `io.openWriter(path, mode=io.truncate) -> IoWriter` | `createNew` 独占创建、`truncate` 覆盖、`append` 追加 |
| `io.fromBytes(data, size=-1)` / `io.fromBuffer(data, size=-1)` | 建立独立输入快照，不保留源的可修改引用 |
| `io.bufferWriter()` | 可增长内存输出；`toBytes()` 返回独立快照，关闭后仍可取快照 |
| `io.discardWriter()` | 丢弃数据并累计写入量 |
| `io.stdin()` / `io.stdout()` / `io.stderr()` | 受语言管理的标准流适配器，关闭不关闭进程原生流 |
| `io.readFile(path, maxBytes=67108864) -> bytes` | 有硬上限的完整读取，超限抛错，不返回悄悄截断的数据 |
| `io.writeFile(path, bytes|string|Buffer, mode=io.truncate, size=-1)` | 精确字节写入、刷新及关闭；参数验证在截断文件之前 |
| `io.copy(source, target) -> int` | 到 EOF，不设隐式总量上限、不自动关闭两端 |
| `io.copyExact(source, target, count) -> int` | 必须复制恰好 count，提前 EOF 抛 IOError |
| `io.copyUpTo(source, target, count) -> int` | 最多 count，提前 EOF 正常，绝不探测 count 之外的字节 |
| `io.lines(source, maxLineBytes=1048576) -> IoLineReader` | 转移 Reader 所有权；原 Reader 随即关闭，不暴露借用行 |

`size=-1` 使用实际字节长度，非负值截断到实际长度，低于 -1 抛
RangeError。所有读取数量及硬上限必须非负。数量、位置在 xlang 公开面
使用受检 int，转换到 size_t/uint64 前先检查。文件路径不可包含 NUL。

`IoReader`：`read(maxBytes)`、`readExact(count)`、
`readAll(maxBytes=67108864)`、`readInto(Buffer,maxBytes=16384)`（追加）、
`seek(offset,origin=io.seekStart)`、`tell/size/eof/canSeek/canSize/closed/close`。
`read` 空结果是 EOF（请求 0 也为空，不能据此判 EOF）；readExact 提前
EOF 抛错。硬上限探测保存多出的一个字节，后续读取与 Tell/相对 Seek
保持逻辑位置；错误并非整次操作回滚，已经消费/写出的前缀不会撤销。

`IoWriter`：`write(bytes|string|Buffer,size=-1) -> int`（允许短写）、
`writeAll(...)`（完整写入）、`flush/close/closed`、
`seek(offset,origin=io.seekStart)`、`tell/size/canSeek/canSize/toBytes`。
`toBytes` 仅对 bufferWriter 有效。Seek/Size 等不支持的操作报错，不能
伪造能力。close 幂等；关闭后的数据操作报错。析构释放资源，重要输出
应显式 flush/close 以观察失败；析构不能覆盖正在传播的语言异常。

`IoReadable.read(int)->bytes` 与 `IoWritable.writeAll(bytes,int)` 是结构化
协议，用户类型可参与三种 copy。原生 Reader/Writer 重载直接调用 XRT；
协议实现返回量不可超过请求量，空结果表示 EOF，不解释为“暂时无数据”。
本组是同步阻塞 IO，不提供隐含非阻塞状态或 async 包装。

`IoLineReader.next() -> (bool,bytes,int)` 返回是否存在、独立行值与
`lineNone/lineLf/lineCrLf`；LF/CRLF 被去掉，裸 CR 是内容。支持 `for line in`
单遍迭代；重复迭代不倒带。行大小上限不计 CRLF。错误可能已有读入，
发生行读取错误后应关闭，不承诺可恢复。标准输入只有一个活动消费者：
raw Reader 持有完整生命周期租约，阻止 Console/session 跳过其探测缓存。

## Console API

| 入口 | 合同 |
|---|---|
| `console.write/console.writeLine/console.error/console.errorLine(text)` | 严格 UTF-8，实际长度含 NUL；失败报 IOError，不每次强制 flush |
| `console.flush()` | 刷新 stdout 与 stderr |
| `console.readLine(maxBytes=1048576) -> string?` | EOF 前没有字符返回 null，空行返回空字符串，剥离 LF/CRLF |
| `console.readChar() -> char?` | Unicode scalar；EOF null，无效输入 UTF-8 抛 IOError |
| `console.isTerminal(stream=stdoutStream)` | 判断指定输出是否终端；重定向是正常 false |
| `console.size(stream=stdoutStream) -> (int,int)` | 列、行；非终端 `(0,0)`，查询不改模式 |
| `console.colorMode(stream=stdoutStream) -> int` | `colorNone/colorBasic/color256/colorTrue`；尊重 NO_COLOR/TERM=dumb |
| `console.writeStyled(text,ConsoleStyle)` / `console.errorStyled(...)` | 重定向不插入 ANSI 控制符；交互终端写样式并复原 |
| `console.rgb(r,g,b) -> int` | 每分量 0..255，生成真实色编码 |
| `console.openSession(ConsoleOptions? options=null)` | 独占交互 stdin/stdout，不适用于重定向；默认 raw 与 paste 打开 |

输出 stream 只有 `stdoutStream`、`stderrStream`；输入使用 readLine/readChar
或 io.stdin，不混用输出枚举。Console 输入不预读下一字符，但失败时已
消费的字符/行前缀不回滚。读取上限按 UTF-8 字节计，不把终止 CRLF 算入。

`ConsoleStyle`：`foreground/background=-1` 默认色，0..255 色板，
`console.rgb(...)` 真实色；`attributes` 可组合 bold/dim/italic/underline/reverse。
`ConsoleOptions`：`raw=true`、`mouse=false`、`paste=true`、`alternateScreen=false`。

`ConsoleSession`：`write/flush`、`moveTo(x,y)`（零起点）、
`clear(mode=clearScreen)`（clearLine/clearToEnd）、`showCursor`、
`setStyle/resetStyle`、`readEvent(timeoutMs=-1)`、`closed/pasteSupported/close`。
输出先入队，显式 flush 或 close 批量发射；队列 16 MiB 硬上限。写出错误
可能是部分成功，失败批次不会重放。close 幂等，输出失败仍继续恢复输入
和输出模式，释放 stdin 租约；恢复失败可由显式 close 观察。

`ConsoleEvent?`：超时 null，实际事件有 `kind/text/key/modifiers/repeat/down`
及 `x/y/wheel/columns/rows`。kind 是 Text/Key/Resize/Paste/Mouse/Closed。
text 为独立字符串，Paste 可包含 NUL；输入事件 1 MiB 硬上限。
Key 专用码从 0x110000 起，功能键 Fn 为 `console.keyF1 + n - 1`；
POSIX 常见终端序列支持 F1..F12，Windows 原生记录支持 F1..F24。
Text 的 key 是 Unicode scalar；修饰键
Shift/Ctrl/Alt/Meta 为 1/2/4/8。Mouse key 为按钮位集合（左1、右2、中4），
坐标零起点，wheel 为带符号步数。字段只在对应事件中有意义。

平台能力显式降级：Windows 原生输入记录不能识别粘贴边界，
pasteSupported 为 false，仍以 Text 收到字符；POSIX 支持 bracketed paste。
POSIX 普通终端不提供 key-up，down 为 true；窗口大小轮询间隔不超过
50 ms，独立 Escape 最多等待约 30 ms 以区分 Alt/序列。未知转义序列
不丢字节，退为 Escape 加剩余文本。UTF-16/UTF-8 均严格验证。

Session 普通操作有线程归属检查；不要共享可修改 Session/IO 对象跨线程
并发调用。最后资源析构允许跨线程进行恢复，但 C 层仍禁止并发析构与操作。
Windows 精确复原输入/输出模式和光标；POSIX 精确复原 termios、关闭本
Session 启用的 ANSI 模式，若隐藏过光标则显示（终端不能查询旧可见状态）。
多个互相竞争的原始终端库不在本合同内。强杀进程不保证恢复。

## 编译边界与验收

标准库声明与语言便利实现登记在 stdlib；IO/console 原生符号具有封闭
XLIR 签名，包含类型、参数借入、返回拥有权、native 工厂与析构身份、
错误传播合同。Cgen 仅选择薄适配器，算法、系统 IO、终端解析与恢复都
在 XRT。纯 screen 查询不链接 session，reader-only 不保留 writer 入口；
不使用完整 xrt/xruntime 实现头生成第二份运行时。

入口：`tools/check_stdlib_io_console.ps1`，覆盖严格独立 C、确定性、JIT/AOT、
错误 ABI 拒绝、二进制/NUL/size、协议、行值、192 个分配故障探针、输入 EOF/非法 UTF-8、
隐藏真实 Windows 终端、正常返回/异常/finally 模式恢复，以及原有 `file`
字节流/Buffer 的 JIT/AOT 回归。批量门禁只准备一次原生工具链。
XRT 九模块
批量测试覆盖模块化和单头，`tools/check_console_terminal_pty.py` 覆盖
POSIX PTY、分段 Unicode/转义、修饰键、数字溢出、NUL 粘贴、鼠标、
Resize/timeout、输入租约及 termios 恢复。

当前没有实现全局 `exit(code)`；其完整栈清理属于此前 CRT 已决待开发
任务，不能用 C exit 替代。本轮不宣称 exit、Linux xlang 全集成、
32 位/ARM、所有终端模拟器或与其他终端库并存已验收。
