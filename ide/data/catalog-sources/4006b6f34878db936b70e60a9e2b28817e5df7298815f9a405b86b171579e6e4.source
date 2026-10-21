# serial

同步串口 I/O，提供 xlang `Serial` 对象和同一套独立 C 接口。
句柄、UTF-8 路径、分配、缓冲、锁与线程错误由 XRT 管理，Windows 串口参数通过系统 API 配置。

## 平台与验收范围

| 能力 | 范围 |
| --- | --- |
| 真实串口 | Windows；例如 `COM3` 或 `\\.\COM3`，独占打开 |
| 内存回环 | `LOOPBACK` / `SIM`；Windows、Linux，不访问设备 |
| 本地已验 | 回环文本/二进制、短读短写、C/xlang ABI、拥有权、并发、分配失败 |
| 尚未验收 | 实际设备、热拔插、驱动差异、串口线与流控；Linux 真实设备尚无实现 |

回环测试通过不代表真实设备达到生产级验收。
目前没有异步串口、可配置超时、硬件流控或串口枚举 API。

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
| `serial.open(string port, int baudrate = 9600) as Serial` | 默认 8 数据位、1 停止位、无校验；参数无效抛 ValueError，设备失败抛 IOError |
| `serial.openLoopback(int baudrate = 9600) as Serial` | 创建独立的有界内存回环对象 |
| `Serial(port, baudrate, databits, stopbits, parity)` | 完整配置构造；dataBits 为 5..8，stopBits 为 1/2，parity 为 0 无/1 奇/2 偶；失败句柄为空，必须先检查 `valid` |
| `valid as bool` / `isOpen as bool` | 当前对象是否有句柄；不是设备健康探针，不检测拔线 |
| `port as string` | 创建时使用的端口名，语言侧返回独立字符串 |
| `baudrate as int` | 创建时配置的波特率 |
| `available as int` | 当时可读的字节数；并发下是快照，不构成下一次读取量保证 |
| `write(string data) as int` | UTF-8 文本写入，返回实际字节数，不是字符数 |
| `writeBytes(Buffer data) as int` | 二进制写入，不修改或接管输入 |
| `read(int maxLen = 1024) as string` | 将本次读取转为文本；maxLen 按字节计数 |
| `readBytes(int maxLen = 1024) as Buffer` | 返回独立拥有的 Buffer；负长度抛 ValueError，0 返回空缓冲且不消费数据 |
| `flush()` | 丢弃接收/发送队列；不是等待数据发送完毕 |

所有 I/O 失败均应按异常处理；成功的零字节结果不是异常。
直接 native 构造的失败状态通过 `valid` 检查，通常优先使用会抛出异常的工厂函数。
类中的 `*Native` 方法是声明层入口，原始返回码遵循下方 C 约定；应用优先使用上表的检查型方法。

## 字节、短读与短写

- 输入长度、`maxLen`、`available` 和所有 I/O 返回值统一为字节数。
- `readBytes` 不返回借用缓冲；下一次读取、flush 或端口销毁不会影响已有结果。
- 回环队列容量 4096 字节。写不下时返回实际写入量，满队列返回 0，不丢弃未计入返回值的输入。
- 单次真实设备 I/O 最多 65536 字节；更大的请求允许短读/短写，不进行无界分配。
- 真实设备采用同步 I/O：当前固定首字节读取等待约 50ms、写入超时 1000ms，最终行为仍受 Windows 驱动约束。
- `read` 不是消息帧解析器。UTF-8 字符可能跨读取边界；有 framing、二进制或分块 UTF-8 要求时，先用 `readBytes` 收齐再解码。
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
| `xserial_write_text(handle, const char* text)` | `int64_t` 实际 UTF-8 字节数 | -1；文本按 C 字符串读取 |
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

发行目录下可直接运行，两份范例实现相同行为且 12 行输出逐字一致：

```text
xl run examples/extlibs/serial.xl
xl run examples/extlibs/serial_c.c
```

仓库 `demo6` 下：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/check_extlib_serial.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tools/run_platform_extlibs.ps1 -Package serial
```

专用门禁用独立进程验证严格 GCC、生成 C 原型、C/xlang JIT、xlang AOT、双语范例。
分配故障测试使用独立 C 运行库，不在已经运行的 xl.dll 宿主里切换分配器。

- `test/test_serial.c`：二进制边界、空/负/极值长度、超长设备名、独立结果、并发写入和 64KiB 连续传输。
- `test/test_serial_alloc.c`：open/read/write 各 32 个分配失败位置，共 96 组；检查队列不被失败操作消费、存活内存回归以及越界/双重释放计数。
- `test/serial_runtime.c`：仅用于独立测试，不能加入包的 cSources，避免编译第二份宿主 XRT。
- `test/cases/extlib_serial.xl`：仓库级语言边界、异常、别名、跨函数返回读取结果和重复生命周期测试。

Linux 可独立用 GCC 编译同一批 C 测试，追加 `-pthread -lm -ldl`。
其结果仅覆盖回环；真实端口会返回 XRT 的 Unsupported 错误。
