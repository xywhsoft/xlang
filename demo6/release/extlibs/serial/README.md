# serial

同步串口 I/O，提供 xlang `Serial` 对象和同一套独立 C 接口。
句柄、UTF-8 路径、分配、缓冲、锁与线程错误由 XRT 管理，Windows 串口参数通过系统 API 配置。

> 2026-10-13 复验当前 `release/xl.exe`：严格 GCC、回环 C 测试、故障注入、
> C/XLang JIT 通过；首个 XLang AOT 构建因共享运行时缺少
> `xl6ValueDataSnapshotV1` 而停止。下文的旧快照 AOT 结果不代表当前发布物
> 已通过；主线修复后需重跑完整门禁。

## 平台与验收范围

| 能力 | 范围 |
| --- | --- |
| 真实串口 | Windows `COM3` / `\\.\COM3`；Linux `/dev/tty*`、`/dev/serial/by-id/*` 等 POSIX TTY 路径 |
| 内存回环 | `LOOPBACK` / `SIM`；Windows、Linux，不访问设备 |
| 本地已验 | 回环文本/二进制、短读短写、C/xlang ABI、拥有权、并发、分配失败；Linux PTY 的实际 FD 双向 I/O、配置、独占状态、超时、断线与 C/XLang 对照 |
| 尚未验收 | 实际串口硬件、热拔插、驱动差异、串口线与流控；macOS/BSD 未测试 |

回环测试通过不代表真实设备达到生产级验收。
目前没有异步串口、硬件流控或串口枚举 API。

## xlang API

```xlang
import serial
from serial import Serial

func main(...) as int
    Serial port = serial.openLoopback(115200)
    assert port.write("你好") == 6
    println(port.read(6))

    Buffer data = Buffer()
    assert data.appendByte(0) and data.appendByte(255)
    assert port.writeBytes(data) == 2
    Buffer result = port.readBytes(2)
    assert result.byteAt(0) == 0 and result.byteAt(1) == 255
    return 0
end
```

| API | 约定 |
| --- | --- |
| `serial.open(string port, int baudrate = 9600) as Serial` | 默认 8 数据位、1 停止位、无校验；参数无效或端口名含 NUL 抛 ValueError，设备失败抛 IOError |
| `serial.openLoopback(int baudrate = 9600) as Serial` | 创建独立的有界内存回环对象 |
| `Serial(port, baudrate, databits, stopbits, parity)` | 完整配置构造；dataBits 为 5..8，stopBits 为 1/2，parity 为 0 无/1 奇/2 偶；端口名含 NUL 抛 ValueError，打开设备失败抛相应异常 |
| `valid as bool` / `isOpen as bool` | 当前对象是否有句柄；不是设备健康探针，不检测拔线 |
| `port as string` | 创建时使用的端口名，语言侧返回独立字符串 |
| `baudrate as int` | 创建时配置的波特率 |
| `available as int` | 当时可读的字节数；并发下是快照，不构成下一次读取量保证 |
| `write(string data) as int` | UTF-8 文本按完整字节长度写入，包括内嵌 NUL；返回实际字节数，不是字符数 |
| `writeBytes(Buffer data) as int` | 二进制写入，不修改或接管输入 |
| `read(int maxLen = 1024) as string` | 将本次读取转为文本；maxLen 按字节计数 |
| `readBytes(int maxLen = 1024) as Buffer` | 返回独立拥有的 Buffer；负长度抛 ValueError，0 返回空缓冲且不消费数据 |
| `setTimeouts(int readMs, int writeMs)` | 设置设备读写等待上限，范围 0..60000 毫秒；0 请求不等待，Windows 写入映射为最短 1ms |
| `flush()` | 丢弃接收/发送队列；不是等待数据发送完毕 |

所有 I/O 失败均应按异常处理；成功的零字节结果不是异常。
直接构造和工厂函数都会报告打开失败；工厂函数另外提供默认参数。
类中的 `*Native` 方法仅供包装层内部使用。XLang 应用使用上表的检查型
方法；C 应用仍可直接使用下方公开的 C API。

## 字节、短读与短写

- 输入长度、`maxLen`、`available` 和所有 I/O 返回值统一为字节数。
- `Serial(...)` 直接构造与 `serial.open(...)` 都拒绝端口名中的内嵌 NUL；文本写入不会在 NUL 处截断。
- `readBytes` 不返回借用缓冲；下一次读取、flush 或端口销毁不会影响已有结果。
- 回环队列容量 4096 字节。写不下时返回实际写入量，满队列返回 0，不丢弃未计入返回值的输入。
- 单次真实设备 I/O 最多 65536 字节；更大的请求允许短读/短写，不进行无界分配。
- 真实设备采用同步 I/O：默认首字节读取等待约 50ms、写入等待约 1000ms；可在打开后调用 `setTimeouts` 修改。Windows 读取设为 0 时立即返回；Win32 将写入超时字段全设为 0 解释为无限等待，所以写入设为 0 时使用最短的 1ms 驱动超时，不承诺严格非阻塞，实际时长仍受驱动约束。POSIX 的 0 使用非阻塞描述符和 `poll` 立即检查就绪；超时返回成功的零字节结果。回环口接受该配置，但收发始终立即完成。
- Linux 使用 `O_NOCTTY` 打开 TTY，以 `termios` 配置原始字节模式、数据位/停止位/奇偶校验和支持的标准波特率，关闭前由包持有原生 FD；支持时启用 `TIOCEXCL`，特权进程可能绕过独占限制。非 TTY 路径明确失败，不当作普通文件写入。
- `read` 不是消息帧解析器。UTF-8 字符可能跨读取边界；有 framing、二进制或分块 UTF-8 要求时，先用 `readBytes` 收齐再解码。
- `read(maxLen)` 读到半个 UTF-8 字符时，文本转换抛 `ValueError`，不会返回非法字符串；已经读取的字节不会放回队列。需要保留分块内容时始终用 `readBytes`，收齐后再调用 `Buffer.text()`。
- 每次操作内部加锁，但 `available` 后紧接 `read`、或 `write` 后紧接 `read` 并非原子事务。请求/响应协议应由调用方协调。

## 生命周期与 C API

xlang 对象负责释放 native 句柄；赋值或传参保留对象引用，不复制串口资源。
C 侧负责恰好一次 `xserial_close`，并在关闭前结束所有使用该句柄的操作。
`close(NULL)` 安全；关闭后的指针、重复关闭及并发关闭不在有效调用合同内。

公开头文件：`include/serial.h`。链接 `serial_native.c` 和一份 XRT 即可，不依赖 xlang 编译器。

| C API | 成功 | 失败/拥有权 |
| --- | --- | --- |
| `xserial_open(name, baudrate, databits, stopbits, parity)` | 拥有型 `void*` | NULL；name 借用到调用结束 |
| `xserial_close(handle)` | 消费句柄 | 不再使用或再次关闭 |
| `xserial_is_open(handle)` | `bool` | NULL 为 false；不是存活指针检测器 |
| `xserial_write(handle, const void* data, size_t size)` | `int64_t` 实际字节数 | -1；输入只读借用到返回 |
| `xserial_write_text_len(handle, const char* text, size_t size)` | 与 write 相同，允许内嵌 NUL | -1；输入只读借用到返回 |
| `xserial_write_text(handle, const char* text)` | `int64_t` 实际 UTF-8 字节数 | -1；文本按 C 字符串读取 |
| `xserial_set_timeouts(handle, readMs, writeMs)` | true，后续设备 I/O 使用新上限 | false；参数范围 0..60000 毫秒，失败时保留原配置 |
| `xserial_write_bytes(handle, void* data, size_t size)` | 与 write 相同 | xlang `point` 的精确 ABI 入口，不接管或修改 data |
| `xserial_read(handle, int64_t maxBytes)` | 拥有型 `void*`，实际为 `xbuffer*` | NULL 只表示失败；结果须 `xrtBufferDestroy` |
| `xserial_available(handle)` | `int64_t` 当前字节数 | -1 |
| `xserial_flush(handle)` | true | false |
| `xserial_get_baudrate(handle)` | `int64_t` 配置值 | NULL 为 0 |
| `xserial_get_port(handle)` | 借用 `const char*` | NULL 为 `""`；非空句柄的字符串借用到 close |
| `xserial_error()` | 当前线程的借用错误文本 | 无错误为 `""`；错误槽更新后借用失效 |

`xrtGetError()` 可取得结构化错误。C 调用方按返回值判定本次操作，不应把已有 last-error
直接解释成新操作失败；记录或处理错误后使用 XRT 清理接口。不同线程不共享错误槽。

## 范例与测试

发行目录下可直接运行，两份范例实现相同行为且输出逐字一致：

```text
xl run examples/extlibs/serial.xl
xl run examples/extlibs/serial_c.c
```

包内 `examples/device_c.c` 也可直接用 `xl build` 构建；运行时需传入真实串口路径。
Linux 门禁会给直接构建和 `xl run` 的 C 范例传入独立 PTY 并核对请求和响应；Windows 无设备门禁只检查构建，
不能据此宣称物理串口已验收。

C 范例中的写入、读取和清空操作不依赖 `assert` 的副作用；Linux 包内门禁
在 `-DNDEBUG` 下编译运行，并与 XLang 范例逐字核对输出。两份范例还分别
校验剩余 2 字节、容量 4096 字节、满队列零写入及最终零剩余，避免共同
打印错误值时仅凭输出一致而误判通过。

仓库 `demo6` 下：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/check_extlib_serial.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tools/run_platform_extlibs.ps1 -Package serial
```

共享 AOT 链接符号尚未修复时，可用 `tools/check_extlib_serial.ps1 -SkipAot`
完整执行包内 C/JIT、ABI 与双语范例门禁；这不等于发布级验收。

专用门禁用独立进程验证严格 GCC、生成 C 原型、C/xlang JIT、xlang AOT、双语范例。
分配故障测试使用独立 C 运行库，不在已经运行的 xl.dll 宿主里切换分配器。

- `test/test_serial.c`：二进制边界、空/负/极值长度、超长设备名、独立结果、并发写入和 64KiB 连续传输。
- `test/test_serial_alloc.c`：open/read/write 各 32 个分配失败位置，共 96 组；检查队列不被失败操作消费、存活内存回归以及越界/双重释放计数。
- `test/serial_runtime.c`：仅用于独立测试，不能加入包的 cSources，避免编译第二份宿主 XRT。
- `test/cases/extlib_serial.xl`：仓库级语言边界、异常、别名、跨函数返回读取结果和重复生命周期测试。

2026-10-18：`writeNative(text,length)` 与 `writeBytesNative(point,length)`
原可由导入方直接调用，绕过实际字符串/Buffer 长度及所有权边界。
两条只编译负例先确认可达，八个 `*Native` 方法现均为类内私有；
`xl check extlibs/serial/test/private_text_negative.xl` 和
`private_bytes_negative.xl` 应报告访问错误，禁止运行这些负例。
Windows `check_extlib_serial.ps1 -SkipAot` 的严格 GCC、生成原型、
C/XLang JIT 与双语范例，以及 Linux C ASan/UBSan、PTY 真设备路径和
C/XLang PTY 对照全部通过；完整 AOT 与物理串口验收仍未完成。

Linux/WSL 聚焦门禁：

```sh
sh release/extlibs/serial/test/check.sh
```

该脚本对回环、故障注入和 PTY 设备路径在 `-DNDEBUG` 下运行严格 GCC 与 ASan/UBSan；三个 C 测试单元会保持断言生效，使断言中的读写操作不会被发布构建删除。可用 `release/xl` 和 Python 时，还将 C/XLang 回环范例对照，并把独立 GCC、直接 `xl build`、直接 `xl run` 和 XLang 的 `examples/device.*` 分别接到独立 PTY，核对 `PING`/`PONG` 与相同输出。PTY 是真实内核 TTY/FD 路径，但不等于 USB/物理串口、拔线和驱动兼容性验收。完整 AOT 仍由共享缺符号问题阻断。

2026-10-22：新增 C/XLang 共用的 `setTimeouts(readMs, writeMs)`，保留原 50/1000 毫秒默认值；仅接受 0..60000 毫秒，失败不改变原配置。Windows 严格 GCC、生成 C 原型、C/XLang 回环及双语范例门禁通过；Linux C ASan/UBSan、PTY 实测 150 毫秒读取等待、零等待写入背压及双语 PTY 范例通过。物理串口驱动差异尚未测试；默认 Windows AOT 门禁仍缺共享 `xl6ValueDataSnapshotV1`，不签收整包发布级。

同日补正 Windows 写入零超时：`COMMTIMEOUTS` 的两个写入字段为 0 会禁用超时，而非立即返回；现在映射为 1ms，并由不依赖设备的 Windows C 用例核对实际配置。Windows 聚焦门禁通过；Linux 仍使用零等待 `poll`。1ms 是驱动超时配置，不保证所有串口硬件严格在 1ms 内返回。
