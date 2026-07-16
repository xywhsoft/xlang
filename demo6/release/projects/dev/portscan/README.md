# portscan - TCP 端口扫描器

用 `tcp.connect` 探测主机端口范围，验证 xlang 的 TCP 网络与并发（async + `task.all`）能力。

## 用法

```
portscan <host> [start=1] [end=1024] [timeout=200] [options]
```

| 选项 | 说明 |
|---|---|
| `-v`, `--verbose` | 打印每个端口（OPEN 和 CLOSED）；默认只打印 OPEN |
| `--async` | 并发扫描（默认并发数为 8，使用 `task.all` 聚合） |
| `-c`, `--concurrency` | 设置并发探针数（1-256，同时启用并发模式） |
| `--allow-remote` | 允许非本地主机（默认拒绝，安全护栏） |
| `-h`, `--help` | 显示帮助 |

## 示例

```
xl run projects\dev\portscan\portscan.xl -- 127.0.0.1 1 100 200
xl run projects\dev\portscan\portscan.xl -- 127.0.0.1 80 90 150 -v
xl run projects\dev\portscan\portscan.xl -- 127.0.0.1 1 1024 200 -c 32
xl build projects\dev\portscan\portscan.xl -o projects\dev\portscan\portscan.exe
portscan.exe 127.0.0.1 1 200 150 --async
```

输出（示例，本机起了 `python -m http.server --bind 127.0.0.1 8085`）：

```
Scanning 127.0.0.1 ports 8080-8090... [async x8]
Port  8085: OPEN
Scan complete: 1 open, 10 closed, elapsed 0.44s
```

## 设计要点与关键发现（开发前必读）

### 1. 如何判定 OPEN / CLOSED（最重要）

`tcp.connect(host, port, timeout)` 失败时返回的是 **零句柄 point**，不是 "null"：

- 成功 → 非零 point（`!= 默认 point`）
- 失败 → 零句柄 point（`== 默认初始化的 point`）

```xlang
point ZERO              // 默认零句柄
point p = tcp.connect(host, port, timeout)
if p == ZERO then       // CLOSED（失败）
    ...
else                    // OPEN（成功）
```

`point` 是不透明句柄值，不是 `null` 引用。连接失败必须与默认初始化的零句柄比较。

`TcpStream(p).send("")` 也能当探针（失败句柄返回 false），但 point 直接比较更干净、无副作用。

### 2. 并发扫描：async + task.all

`async func probe(host, port, timeout) as int` 返回 `future<int>`（1=OPEN, 0=CLOSED）。扫描器按运行期并发数构造 `array<future<int>>`，每批用 `task.all` 聚合：

```xlang
array<int> ports = array<int>[]
array<future<int>> jobs = array<future<int>>[]
while port <= endPort and jobs.length < concurrency do
    ports.push(port)
    jobs.push(probe(host, port, timeout))
    port += 1
end
array<int> results = await task.all(jobs)
```

实测 8 个并发 probe 耗时 ~1ms（真并行，非协作交替）；1-200 端口扫描 sync 2.2s → async 0.34s（约 6.5x 提速，本机 closed 端口命中超时而非 RST，故提速明显）。

### 3. 为什么扫描逻辑放进 `async func scanAll`（而不是 main）

`await` **不能**出现在普通 `func main` 里（`[sem] await can only be used at top level, inside async func, or inside async generator`），而 `arg` 只在 `func main` 里可用。解法：

```xlang
async func scanAll(...) as int
    // 这里能用 await（async 路径）和 tcp.connect（sync 路径）
end

func main(...) as int
    // 解析 arg、校验
    future<int> done = scanAll(host, startPort, endPort, timeout, useAsync, concurrency)
    done.wait()      // 阻塞到 async 扫描完成，无死锁
    return 0
end
```

`future.wait()` 在同步入口等待异步任务完成；异步实现内部仍使用 `await`，两层职责保持清晰。

### 4. 运行期控制并发度

`array<future<int>>` 可以动态追加任务，因此并发度无需写死。`-c` 会同时启用并发模式，范围限制为 1-256，避免一次创建过多套接字和任务。末批只包含真实端口，不会发出填充探针。

### 5. 计时用 `time.timer()`（float 秒，不是 int 毫秒）

`time.timer()` 返回 **float 秒**（高精度墙钟）。直接 `float t0 = time.timer()`，`elapsed = t1 - t0`。**不要** `int(time.timer())`——截断到整秒后精度丢失，差值几乎全为 0。

### 6. 安全护栏

默认只接受 `127.0.0.1` / `localhost` / `::1`；其他主机需显式 `--allow-remote`。

### 7. `localhost` vs `127.0.0.1` 差异（网络层细节，非 BUG）

XRT 解析 `localhost` 时在本机解析到 IPv6 `::1`。若服务只绑了 IPv4 `127.0.0.1`，则 `localhost:port` 探测为 CLOSED 而 `127.0.0.1:port` 为 OPEN。用 `127.0.0.1` 测试最可靠。

## 验证状态

- `xl check`：通过。
- `xl run`：sync + async + 自定义并发度 + verbose + 边界（单端口、非整批、越界端口、非本地拒绝、start>end）全部正确。
- `xl build` + exe：通过，输出与 `xl run` 一致。
