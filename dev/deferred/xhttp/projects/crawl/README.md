# crawl - 异步 HTTP 爬虫

并发请求任意数量的 URL，输出状态码、响应字节和耗时，并汇总成功、HTTP 错误与网络失败数量。

## 用法

```text
crawl [options] <url1> [url2] ...
crawl [options] -f <urls.txt>
crawl demo

-t, --timeout <ms>       单请求超时，默认 8000
-c, --concurrency <n>    最大并发数，默认 16，范围 1-256
-f, --file <path>        从文件读取 URL，每行一个
-h, --help               显示帮助
```

URL 文件会忽略空行和以 `#` 开头的注释行，URL 数量不设固定上限。

```bat
xl run projects\dev\crawl\crawl.xl -- demo
xl run projects\dev\crawl\crawl.xl -- -c 32 https://example.com https://example.org
xl run projects\dev\crawl\crawl.xl -- -t 3000 -f urls.txt
xl build projects\dev\crawl\crawl.xl -o projects\dev\crawl\crawl.exe
```

## 实现要点

### 动态并发任务

URL 和任务分别使用 `array<string>` 与 `array<future<int>>`。`runUrls` 按运行期并发度分批创建任务，并用 `task.all` 等待当前批次：

```xlang
array<future<int>> jobs = []
while i < stop
    jobs.push(fetchOne(i, urls.length, urls[i], timeout))
    i += 1
end
array<int> codes = await task.all(jobs)
```

批处理只限制同时占用的连接和 worker 数量，不限制输入 URL 总数，也不创建虚假的填充任务。

### 清晰的所有权边界

每个 `fetchOne` 独占自己的 `HttpClientResponse`，在同一异步函数中读取状态、长度和耗时，最后只返回整数状态码。响应句柄不会进入共享容器或跨任务传递。

### 同步入口与异步编排

`await` 位于 `async func runUrls` 内；`main` 只负责参数解析并通过 `future.wait()` 等待。当前 future 的 `wait()` 返回等待是否成功，不直接返回 `future<int>` 的完成值，因此退出码通过整数 `g_rc` 桥接。

### 内置验证服务器

`crawl demo` 启动本地 HTTP 服务，覆盖 200、404、500、大响应和延迟响应。多个请求并发执行，总耗时应接近最慢请求，而不是各请求耗时之和。

## 验证

- `xl check` 和 `xl build` 均通过。
- 内置六路本地 HTTP 演示通过，成功、HTTP 错误与网络失败分类正确。
- 文件不存在、空文件、非法超时和非法并发度返回非零退出码。
- `xl run` 不依赖临时可执行文件或固定任务槽。
