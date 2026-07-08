# crawl - 异步 HTTP 爬虫

并发请求多个 URL，汇总每个 URL 的状态码、响应字节、耗时，验证 xlang 的
`async` + `http` client 并发能力（`task.all` 聚合）。

## 用法

```
crawl <url1> [url2] [url3] ...        并发请求所有 URL
crawl -f <urls.txt>                    从文件读取 URL 列表（每行一个）
crawl demo                             内置示例（启动本地 server 再爬取）
crawl -t <ms> <url1> [url2] ...        设置单请求超时（默认 8000ms）
crawl -h | --help                      帮助
```

`urls.txt` 格式：每行一个 URL；空行和 `#` 开头的注释行跳过；最多 16 条。

## 示例

```
xl run projects\dev\crawl\crawl.xl -- demo
xl run projects\dev\crawl\crawl.xl -- http://127.0.0.1:8080/a http://127.0.0.1:8080/b
xl run projects\dev\crawl\crawl.xl -- -f urls.txt
xl build projects\dev\crawl\crawl.xl -o projects\dev\crawl\crawl.exe
crawl.exe demo
```

## 输出示例（demo）

```
demo: local server listening on 127.0.0.1:14993
demo: crawling 6 endpoints concurrently...
------------------------------------------------------------
[1/6] 200        27B       6ms  http://127.0.0.1:14993/hello
[4/6] 200        44B      15ms  http://127.0.0.1:14993/json
[6/6] 404         9B      30ms  http://127.0.0.1:14993/missing
[2/6] 200      2048B      30ms  http://127.0.0.1:14993/big
[5/6] 500        19B      30ms  http://127.0.0.1:14993/error
[3/6] 200        25B     345ms  http://127.0.0.1:14993/slow
------------------------------------------------------------
urls:      6
ok(2xx/3xx): 4   http-error(4xx/5xx): 2   failed(net): 0
wall time: 345 ms (concurrent via task.all, max 16)
```

注意结果行**按完成顺序**而非输入顺序打印——`/slow` 故意 sleep 300ms，
总是最后完成；其余 5 个请求在它睡眠期间并发跑完。wall time ≈ 最慢单请求
（≈330ms），不是 6 个请求耗时之和，证明是真并发。

`/slow` 路由在 server 侧 sleep 300ms，demo server 的 `workerCount=4`，因此
`/slow` 不会阻塞其它请求的处理。

## 设计要点（xlang 约束绕过，见 dev/README.md）

### 1. 为什么并发编排放进 `async func runUrls` 而不是 `func main`

`await` 不能出现在普通 `func main` 里（`[sem] await can only be used at top
level, inside async func, or inside async generator`）。解法：

```xlang
async func runUrls(float startSec) as int
    ...
    array<int> codes = await task.all(jobs)
    ...
end

func main(...) as int
    // 解析 argv 后
    future<int> f = runUrls(time.timer())
    bool ok = f.wait()        // 阻塞到并发爬取完成
    return g_rc
end
```

`future.wait()` 跨函数阻塞可用且不死锁（同 portscan）。

### 2. `task.all` 要求 typed 定长 array，并发上限是编译期常量

`task.all` 接收 `array<future<T>>`（定长 array 字面量）；`future<T>` 不能
存进 `list<T>`（codegen `cannot pack into var`），定长 array 不能运行期改
变大小。因此并发上限固定为 `MAX_URLS=16`，URL 列表存进 `g_url0..g_url15`
一组全局标量（参考 psearch 的 `g_dir0..g_dir15`），用三元短路的 array 字面
量填充，未用 slot 喂一个 `noop()` future：

```xlang
array<future<int> > jobs = array[
    c > 0  ? fetchOne(0, g_url0, g_timeout) : noop(),
    ...
    c > 15 ? fetchOne(15, g_url15, g_timeout) : noop()
]
array<int> codes = await task.all(jobs)
```

### 3. HttpClientResponse 是 native-backed class，操作必须内联

`HttpClientResponse`（以及 `HttpClientRequest`）带 destroy，是不可按值复制
的资源句柄，**不能跨函数传参**。所以每个请求的 `status()` / `bodySize()` /
计时读取全部内联在 `async func fetchOne` 内，只把 status code 作为 `int` 返回：

```xlang
async func fetchOne(int slot, string url, int timeout) as int
    float t0 = time.timer()
    HttpClientResponse resp = http.get(url, timeout)
    int elapsedMs = int((time.timer() - t0) * 1000.0)
    int code = resp.status()
    int bodyLen = code != 0 ? resp.bodySize() : 0
    println(...)         // 该 URL 的结果行
    return code
end
```

### 4. 阻塞 http.get 在 async 帧里获得并发

`http.get` 当前是同步阻塞 client（DEMO6功能清单 第 14 节：异步 HTTP client
归入后续统一异步网络运行时）。但 `async func` 在 xlang 运行时里跑在 worker
线程池上（参考 portscan 的 `tcp.connect` 并发验证），把多个阻塞 `http.get`
分别放进多个 `fetchOne` async 任务、用 `task.all` 聚合，即可实现真并发——
demo 里 6 个请求（含一个 300ms sleep 的 `/slow`）的 wall time ≈ 330ms，远小
于串行耗时。

### 5. 退出码怎么从 async 任务回传给 main

`future<int>` 的结果值只能通过 `await` 取出，而 `main` 不能 `await`。所以
`runUrls` / `runDemo` 在 return 前把退出码写进全局 `g_rc`，`main` 在
`f.wait()` 之后 `return g_rc`。

### 6. `crawl demo` 本地 server

`runDemo` 用 `http.server.create(port=0, ..., workerCount=4)` 起一个本地
server，注册 `/hello`、`/big`、`/slow`、`/json`、`/error`、`/missing` 六个
路由（覆盖 200/404/500、大小 body、延迟响应），填充 `g_url*` 后调用
`runUrls`，最后 `stop` + `destroy`。`workerCount=4` 保证 `/slow` 的 sleep
不会串行化其它请求。

## 发现的限制（详见 dev/BUGS.md）

- **BUG-C1**：`file.readText` 不剥离 UTF-8 BOM。BOM 会污染首行，使 `#` 注释
  判断失效并把 BOM 前缀的注释行当成无效 URL。本工具用 `string.byteAt` 检测
  `0xEF 0xBB 0xBF`，命中则 `string.slice(text, 1)` 去掉那 1 个 BOM 字符。
- **BUG-C2**：`file.readText` 对不存在的文件静默返回 `""` 而不抛异常，无法用
  try/catch 区分「文件不存在」与「文件为空」。本工具在读取前先 `file.exists`
  判定，区分「不存在 / 读失败 / 无有效 URL」三种错误返回。
- **手感-C1**：`string.slice(text, start, count)` 的 `start` 是**字符下标**
  而非字节下标（与 `string.byteAt` 的字节下标不一致）。BOM（3 字节 / 1 字符）
  必须用 `slice(text, 1)` 去掉，用 `slice(text, 3)` 会多删 2 个字符。
- **手感-C2**：`string.trim` 不剥离 U+FEFF BOM 字符。
- **手感-C3**：`catch e` 的异常变量 `e` 不能传给 `print`/`println` 的可变参数
  （codegen `cannot pack as var`）；catch 内只用 `e` 做标志位即可。
- 并发上限 16 来源于 `task.all` 定长 array 限制（同 psearch），非本工具特有。

## 验证状态

- `xl check`：通过。
- `xl run demo`：EXIT=0（6 路由并发，wall time ≈ 330ms）。
- `xl build` + `crawl.exe demo`：EXIT=0，输出与 `xl run` 一致。
- 直连 URL 模式（3 URL 并发）：EXIT=0。
- `-f` 文件模式（含注释/空行/404）：EXIT=0。
- `-f` BOM 文件：BOM 正确剥离，注释行跳过（BUG-C1 绕过有效）。
- `-f` 不存在文件 / 空文件：精确错误提示，EXIT=1（BUG-C2 绕过有效）。
- `-t` 超时 + 不可达端口：FAIL 正确报告，EXIT=1。
- 无参数 / 非 URL 参数：帮助或错误提示，EXIT=1。
