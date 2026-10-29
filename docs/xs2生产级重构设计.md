# xs 2.0 生产级重构设计

本文档是 xs 2.0 的当前设计与验收依据。实现只参考 demo6 当前设计文档、XRT 当前接口和本文档，不继承旧版 xserver 的历史兼容路径。

## 一、目标与边界

xs 2.0 是由 xlang 编写、建立在 XRT 网络基座和 xlang 动态运行时之上的生产级多协议服务宿主。

本期必须实现：

- HTTP/HTTPS 服务。
- TCP/UDP 服务。
- HTTP Host 与 TLS SNI 虚拟主机。
- C 和 xlang 动态处理器。
- 严格 JSON 配置、配置检查、原子 reload 和失败回滚。
- 日志、指标、健康状态、优雅停止和资源上限。
- C 与 xlang 双语言 SDK、范例和测试。
- 一个完整博客系统，覆盖公开站点和管理后台。

本期不实现 WebSocket、XTP 和进程内 bus。它们不进入空实现、兼容分支或预留语法。

production `xs` 是薄宿主，不内置固定管理 URI。博客管理后台属于博客应用，不属于 xs 核心。

## 二、分层

### 2.1 XRT

XRT 负责网络、TLS、HTTP、路由、静态文件、文件系统、线程同步、日志、时间、进程和通用数据结构。能够在 XRT 内合理解释的能力必须在 XRT 实现，xs 不重复制造网络设施。

### 2.2 xlang 运行时

xlang 运行时负责：

- 编译并重定位 C/xlang 模块。
- 查询模块导出符号。
- 向动态上下文注入 XRT 与宿主符号。
- 管理模块调用期间的生命周期。

### 2.3 xs 核心

xs 核心负责：

- 配置解析、校验和规范化。
- 服务、虚拟主机和 route 装配。
- handler generation 的构建、切换和回收。
- 生命周期、reload、日志、指标和健康状态。
- 把 XRT request/response/stream/dgram 对象映射到统一 handler frame。

### 2.4 应用

应用只实现业务处理器和资源。博客系统是第一套完整应用，也是 xs 2.0 的综合验收项目。

## 三、目录结构

```text
release/projects/xs/
    xs.xl                       启动入口
    xs.json                     默认配置
    README.md
    src/
        config.xl               配置读取、校验、规范化
        runtime.xl              根运行时与状态
        handler.xl              handler generation 与 lease
        http_service.xl         HTTP/HTTPS 装配
        stream_service.xl       TCP 服务
        dgram_service.xl        UDP 服务
        reload.xl               配置事务与 generation 切换
        lifecycle.xl            信号、drain、停止与回收
        metrics.xl              指标快照
    include/
        xs_handler.h            C handler ABI
    handlers/
        c/                      C 范例
        xlang/                  xlang 范例
    tests/
        config/
        integration/
        stress/
    blog/
        app.xl
        config/
        db/
        handlers/
        templates/
        static/
        tests/
```

源文件按职责拆分。禁止重新形成一个包含配置、网络、脚本、博客和测试的超大 `xs.xl`。

## 四、统一 handler ABI

### 4.1 固定入口

C 和 xlang 使用同一个导出入口：

```c
int xs_handler(xs_handler_frame* frame);
```

xlang 写法：

```xlang
export "xs_handler" func handle(point frame) as int
    // 通过 xs.handler API 访问 frame。
    return 0
end
```

不再把协议字段编码到 `argv`，也不再通过临时文件回传响应。

### 4.2 frame

`xs_handler_frame` 是版本化 C ABI 结构：

```c
typedef struct xs_handler_frame {
    uint32_t struct_size;
    uint32_t abi_version;
    uint32_t kind;
    uint32_t flags;
    uint64_t request_id;
    uint64_t deadline_ms;
    const char* service_name;
    const char* route_name;
    void* request;
    void* response;
    xvalue input;
    xvalue output;
    const xs_handler_host_api* api;
    void* host;
} xs_handler_frame;
```

规则：

- `struct_size` 用于结构向后扩展，读取方不能访问超出该长度的字段。
- `abi_version` 首版固定为 `1`，不匹配时拒绝加载。
- `kind` 区分 HTTP、TCP、UDP 和生命周期事件。
- HTTP 的 `request/response` 是当前回调生命周期内的 XRT 视图。
- TCP/UDP 的二进制数据通过 `input` 或只读 Buffer 视图传递。
- handler 可设置 `output`，所有权由 frame API 明确转移。
- `api` 是同样带 `struct_size/abi_version` 的宿主 helper 表，供 C handler 记录日志、检查取消和设置输出。
- `host` 只允许传给 xs SDK，不允许业务代码猜测内部结构。
- handler 返回 `0` 表示成功；非零值映射为稳定的 xs 错误类别。

所有 frame 字段都必须同时提供 C helper 和 xlang 薄封装。xlang 层不能复制另一套请求模型。

## 五、handler generation 与池

每个动态处理器由不可变 generation 表示：

- generation 记录源码指纹、ABI 版本、构建时间、实例池、活动调用数和最后错误。
- 每个实例拥有独立的 `TCCState` 或 `XL_Context + XL_Module`。
- 同一实例同一时刻只处理一个请求，不并发进入同一动态上下文。
- 池使用有界等待；池耗尽时按配置等待或快速返回 overload，不能无限创建实例。
- 新 generation 必须全部编译、重定位、解析入口并完成 init 后，才允许原子发布。
- 发布后新请求只进入新 generation；旧 generation 等活动调用归零后执行 unit 并释放。
- 新 generation 构建失败时保留旧 generation，reload 返回失败详情。

### 5.1 超时约束

不能通过强杀线程实现超时，这会破坏 XRT、TCC、锁和内存状态。

- embedded 模式只运行可信处理器，`deadline_ms` 提供协作取消和耗时统计。
- 网络读写和池等待使用硬超时。
- 需要强制终止不可信脚本时使用独立 worker 进程；进程隔离属于明确配置，不伪装成线程超时。
- 当前发布候选只实现 embedded 处理器；配置若声明未实现的进程隔离模式会被严格拒绝，而不是静默回退。

## 六、配置模型

配置根对象必须包含 `version: 2`。配置读取执行四个阶段：读取、结构校验、语义校验、规范化。

```json
{
  "version": 2,
  "runtime": {
    "workDir": ".",
    "shutdownTimeoutMs": 10000,
    "reloadDebounceMs": 300,
    "handlerPoolSize": 4,
    "handlerQueueLimit": 1024
  },
  "logging": {
    "level": "info",
    "console": true,
    "file": "run/xs.log",
    "rollingBytes": 10485760,
    "rollingFiles": 5
  },
  "services": []
}
```

配置规则：

- 未知字段默认报错，避免拼写错误被静默忽略。
- 数值范围、路径、证书、端口、Host、route 和 handler 必须在启动前验证。
- service `name` 全局唯一，route `name` 在 service 内唯一。
- 相对路径统一以配置文件目录解析，不依赖进程当前目录。
- 密钥、口令和 session secret 支持从环境变量读取，不要求明文进入配置。
- `check` 只验证，不启动服务、不执行 handler init、不修改运行目录。

## 七、服务模型

### 7.1 HTTP/HTTPS

- 支持多个 HTTP/HTTPS service。
- 每个 service 可配置默认 app、多个 Host app 和多个 SNI 证书。
- 支持路由、静态目录、错误处理器、body streaming、Cookie、form、multipart 和响应 streaming。
- 暴露 header/body/idle/write timeout，header/body/multipart limit，worker 数和 backlog。
- 关闭时先停止接收新连接，再 drain，超时后才执行强制 stop。

### 7.2 TCP

- 支持多个 listener 和并发 session。
- 支持 `text`、`delimiter`、`length-prefix` 和 `raw` 四种 framing。
- `raw` 把每次网络读取的数据作为一帧，不解释内容；它适合调用方自行维护协议状态。
- `text` 以 LF 分帧，输入兼容 CRLF 且移除行尾，输入必须是严格 UTF-8；响应统一追加 LF。
- `delimiter` 使用 1 至 256 字节的 UTF-8 分隔符，分隔符不进入 handler 输入；响应自动追加同一分隔符。
- `length-prefix` 使用 4 字节无符号大端长度前缀，输入与响应采用同一格式。
- `frameLimit` 限制单帧载荷，必须小于或等于 `recvLimit`；长度前缀在分配载荷前校验，累计缓冲超过上限立即拒绝。
- 每个 session 有接收上限、idle timeout、write timeout 和 backpressure 上限。
- handler 接收 open/data/close 生命周期事件。

### 7.3 UDP

- 支持多个 socket、二进制 datagram、队列上限和丢包统计。
- 每个 datagram 携带远端地址、端口和本地 service 信息。
- handler 通过 frame 输出可选响应，不复制字符串协议。

## 八、reload 事务

reload 流程：

1. 读取并严格校验新配置。
2. 计算规范化配置差异。
3. 构建所有新增或变化的 handler generation、app、证书和未绑定的 service candidate。TCP/UDP candidate 在 prepare 阶段不占用端口。
4. 任一步失败则销毁 shadow，原运行时不变。
5. 纯 handler 变化原子切换 generation；需要重绑的 HTTP/TCP/UDP service 先停止旧 listener、释放端口，再启动已完成校验的 candidate。若新 listener 启动失败，事务报告失败并执行可恢复路径，不把“同端口双绑定”伪装成原子能力。
6. 旧对象进入 drain 队列，活动调用归零后释放。
7. 输出结构化 reload 结果和稳定错误码。

同一时刻只允许一个 reload 事务。并发请求返回 busy，不排队堆积。

## 九、生命周期、日志和指标

- 捕获平台支持的退出信号，信号处理器只设置停止状态，不直接释放资源。
- 根运行时按 `RUNNING -> DRAINING -> STOPPING -> STOPPED` 转换。
- 所有线程、listener、handler generation、数据库和日志对象都有唯一所有者。
- 生产路径禁止 `thread.stop()` 作为正常回收方式。
- 日志提供 request id、service、host、route、handler generation、耗时和错误码。
- 指标至少包括请求数、活动连接、状态码、收发字节、handler 等待/执行耗时、reload 次数和失败数。
- production xs 不固定暴露 HTTP 管理路径；应用可按需读取指标快照并自行注册路由。

## 十、博客系统

博客系统使用 SQLite，启用 WAL、foreign keys 和 busy timeout。所有写操作使用事务和参数绑定。

### 10.1 数据表

- `admins`：管理员、口令摘要、状态和最后登录时间。
- `sessions`：会话摘要、过期时间、CSRF token 和客户端信息。
- `categories`：栏目名称、slug、排序、状态和描述。
- `articles`：栏目、标题、slug、摘要、Markdown 正文、渲染结果、状态、发布时间和版本。
- `comments`：文章留言和独立留言本内容、昵称、联系方式摘要、审核状态和时间。
- `settings`：站点名称、描述、主题、分页、留言策略和其他键值配置。

数据库升级使用单调递增 migration，不在启动代码里散落 `CREATE TABLE IF NOT EXISTS`。

### 10.2 公开功能

- 首页、栏目页、文章页、归档、搜索和分页。
- Markdown 渲染和严格 HTML 转义。
- 文章留言与独立留言本。
- RSS/Atom、sitemap、robots 和标准 404 页面。
- 静态资源缓存、ETag、Range 和安全路径规则。

### 10.3 管理功能

- 登录、退出、会话过期和 CSRF 防护。
- 栏目增删改查、排序与启停。
- 文章草稿、发布、编辑、删除、预览和按栏目筛选。
- 留言审核、回复、隐藏和删除。
- 网站配置读取与修改。
- 管理操作审计日志。

初始管理员凭据和 session secret 不写死在源码中。部署工具负责初始化，运行时只读取摘要或环境变量。

## 十一、安全规则

- 所有 SQL 使用 prepared statement 和参数绑定。
- 所有 HTML 输出按上下文转义；Markdown 使用经过验证的实现，不维护第二套不完整解析器。
- 管理写操作要求有效 session 和 CSRF token。
- Cookie 默认 HttpOnly、SameSite=Lax；HTTPS 下启用 Secure。
- 静态文件禁止目录穿越、点文件和反斜杠路径。
- request/body/header/multipart/连接/队列均有明确上限。
- 错误响应不泄露文件绝对路径、SQL、密钥或脚本内部栈信息。

## 十二、验收矩阵

### 功能

- HTTP、HTTPS、SNI、vhost、静态文件和完整请求/响应 API 通过。
- TCP/UDP 的 C/xlang handler 都能处理文本和二进制数据。
- C/xlang generation 均支持加载、复用、reload、失败回滚和释放。
- 多 service 可同时运行，端口和 Host 分发正确。
- 博客公开端和管理端功能全部通过端到端测试。

### 稳定性

- 正常停止不使用强杀线程，不遗留活动连接、线程、模块和数据库句柄。
- reload 压力下请求不进入已释放 generation。
- handler 构建失败、证书错误、端口冲突和坏配置均保持旧服务可用。
- HTTP/TCP/UDP 分别完成并发、长连接、慢客户端和异常输入测试。
- 内存检测与重复进程压力不得出现堆损坏、双重释放或持续增长。

### 工程

- C 与 xlang 范例覆盖同一 handler ABI。
- 配置、核心、协议、handler、reload、博客和测试分文件维护。
- 所有发布路径可从 `release` 目录直接运行，不依赖仓库上层路径。
- 自动化测试按 fast、integration、stress、soak、memcheck 分类。

## 十三、实施顺序

1. 收口 TCC/XRT 符号清单和运行态 smoke。
2. 实现 handler frame、C/xlang module symbol 调用和 generation 池。
3. 重写配置、服务装配和生命周期。
4. 实现 reload 事务、日志和指标。
5. 补齐 C/xlang SDK 与协议测试。
6. 实现博客数据库、公开端和管理端。
7. 完成压力、故障注入、内存与发布守门。
