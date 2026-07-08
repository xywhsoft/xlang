# portscan - TCP 端口扫描器

用 `tcp.connect` 探测主机端口范围，验证 xlang 的 TCP 网络与并发（async + `task.all`）能力。

## 用法

```
portscan <host> [start=1] [end=1024] [timeout=200] [options]
```

| 选项 | 说明 |
|---|---|
| `-v`, `--verbose` | 打印每个端口（OPEN 和 CLOSED）；默认只打印 OPEN |
| `--async` | 并发扫描（每批 8 个端口，`task.all` 聚合） |
| `--allow-remote` | 允许非本地主机（默认拒绝，安全护栏） |
| `-h`, `--help` | 显示帮助 |

## 示例

```
xl run projects\dev\portscan\portscan.xl -- 127.0.0.1 1 100 200
xl run projects\dev\portscan\portscan.xl -- 127.0.0.1 80 90 150 -v
xl run projects\dev\portscan\portscan.xl -- 127.0.0.1 1 1024 200 --async
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

**陷阱**：`p is null` **恒为 false**（即便零句柄也报 "not null"），绝不能用 `is null` 判定连接成败。详见 `BUGS.md` BUG-22。

`TcpStream(p).send("")` 也能当探针（失败句柄返回 false），但 point 直接比较更干净、无副作用。

### 2. 并发扫描：async + task.all

`async func probe(host, port, timeout) as int` 返回 `future<int>`（1=OPEN, 0=CLOSED）。并发模式按 BATCH=8 一批发射 8 个 probe，用 `task.all` 聚合：

```xlang
future<int> f0 = probe(host, p0, timeout)
... f1..f7
array<future<int> > jobs = array[f0, f1, f2, f3, f4, f5, f6, f7]
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
    future<int> done = scanAll(host, startPort, endPort, timeout, useAsync)
    done.wait()      // 阻塞到 async 扫描完成，无死锁
    return 0
end
```

`future.wait()` 跨函数阻塞可用且不死锁（详见 `BUGS.md` BUG-24 / NOTE-22）。

### 4. 为什么 BATCH 是编译期常量

`future<T>` **无法存入 `list<T>`**（codegen 报 `当前 C 生成暂不支持将该表达式打包为 var`），而定长 `array` 无法运行期改变大小。因此并发批只能用**定长 array 字面量** `array[f0,...,f7]`，BATCH 固定为 8。末批不足 8 个时，越界 slot 探测 port 1（无害），统计时按 `basePort+s <= endPort` 跳过。详见 `BUGS.md` BUG-23。

### 5. 计时用 `time.timer()`（float 秒，不是 int 毫秒）

`time.timer()` 返回 **float 秒**（高精度墙钟）。直接 `float t0 = time.timer()`，`elapsed = t1 - t0`。**不要** `int(time.timer())`——截断到整秒后精度丢失，差值几乎全为 0。

### 6. 安全护栏

默认只接受 `127.0.0.1` / `localhost` / `::1`；其他主机需显式 `--allow-remote`。

### 7. `localhost` vs `127.0.0.1` 差异（网络层细节，非 BUG）

XRT 解析 `localhost` 时在本机解析到 IPv6 `::1`。若服务只绑了 IPv4 `127.0.0.1`，则 `localhost:port` 探测为 CLOSED 而 `127.0.0.1:port` 为 OPEN。用 `127.0.0.1` 测试最可靠。

## 验证状态

- `xl check`：通过。
- `xl run`：sync + async + verbose + 边界（单端口、整 8 倍数、非整批、越界端口、非本地拒绝、start>end）全部正确。
- `xl build` + exe：通过，输出与 `xl run` 一致。

> 注：开发期间曾因环境里残留的外部头 `release\include\xl_runtime.h`（外部文件系统优先于静态 VFS）同时阻断 `xl run`（`redefinition of ...`）与 `xl build`（`unresolved xl6_command_argv_to_arg`）。删除该残留头后两条路径均恢复。详见 `BUGS.md` NOTE-22 对 BUG-19 的细化。
