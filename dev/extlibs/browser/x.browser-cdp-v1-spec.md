# x.browser CDP V1 设计规格

状态：设计定稿草案  
目录：`dev/extlibs/browser`  
目标包：`x.cdp`、`x.browser`、`x.browser.assert`、`x.browser.trace`  
目标平台：Windows 优先，后续扩展 Linux / macOS  
浏览器范围：Chromium 内核浏览器，优先 Chrome / Edge

## 1. 背景与目标

xlang 的自动化能力需要一个稳定、可脚本化、可诊断的浏览器控制层。Playwright 的体验证明了几个关键方向：浏览器上下文隔离、Locator 惰性解析、动作前自动等待、网络拦截、可观测 trace。xlang 不应直接依赖 Playwright 运行时，而应基于 Chrome DevTools Protocol 建立自己的浏览器自动化扩展。

CDP 是 JSON over WebSocket 协议，按 `Page`、`Runtime`、`Network`、`Fetch`、`Input`、`Target`、`Browser` 等 domain 组织命令和事件。协议官方说明 tip-of-tree 版本会频繁变化，因此实现必须做协议版本探测、能力探测和降级。

V1 的目标不是 100% API 兼容 Playwright，而是实现 Playwright 自动化体验中最关键的能力：

- launch / connect / close
- browser context 隔离
- page navigation / evaluate / screenshot / pdf
- Locator 和自动等待
- 点击、输入、键盘、鼠标、选择、文件上传
- 网络事件、等待、拦截、mock
- cookie / storage / emulation
- trace、失败截图、DOM 快照、网络日志
- 低层 CDP 原始命令出口

## 2. 非目标

- 不支持 Firefox / WebKit。CDP 路线天然以 Chromium 为主。
- 不实现 Playwright Test runner。V1 是自动化库，不是测试框架。
- 不以内置反检测、绕过风控为目标。
- 不把 Node.js、npm、Playwright 作为运行时依赖。
- 不要求 V1 直接返回复杂 native 结构体。复杂数据优先使用 JSON 文本，xlang 门面层再解析成 `dict` / `list` / `var`。
- 不在 V1 依赖 native callback ABI。事件和 route 先采用轮询/等待模型，callback 留到 V2。

## 3. 包边界

### 3.1 `x.cdp`

低层协议包，只面向高级用户和 `x.browser` 内部。

职责：

- Chrome / Edge 远程调试端点连接。
- HTTP bootstrap：`/json/version`、`/json/list`、`/json/protocol`。
- WebSocket JSON-RPC transport。
- request id、response、error、event 分发。
- target / session 生命周期。
- 原始 CDP 命令出口。

原则：

- 不做 Locator。
- 不做自动等待。
- 不包装浏览器语义。
- 所有复杂参数和返回值使用 JSON。

### 3.2 `x.browser`

高层自动化包，用户主要使用它。

职责：

- `Browser` / `BrowserContext` / `Page` / `Frame` / `Locator` 抽象。
- 默认超时、自动等待、动作稳定性检查。
- 常用 navigation、evaluate、screenshot、input、network、storage、emulation。
- 将 CDP 错误转成 xlang 可读错误。

### 3.3 `x.browser.assert`

自动重试断言包。

职责：

- `visible`、`hidden`、`text`、`contains_text`、`count`、`url`、`title`、`ok` 等断言。
- 断言失败时附带 selector、实际值、超时和 trace 附件路径。

### 3.4 `x.browser.trace`

调试追踪包。

职责：

- action log。
- 失败截图。
- DOM snapshot。
- console log。
- network log / HAR。
- trace zip 导出。

## 4. 交付形态

推荐按官方 native 包发布：

```text
libs/
  packages/
    x.browser/
      package.json
      src/
        browser_facade.xl
        browser_assert.xl
        browser_trace.xl
      bin/
        x_browser.dll
```

`x_browser.dll` 负责 CDP 协议状态机、基于 xrt WebSocket 的 transport 编排、事件循环和浏览器生命周期。`src/*.xl` 负责更友好的 xlang 门面、错误转换、JSON 辅助解析和断言 API。

后续如果拆包：

```text
x.cdp       只暴露低层协议
x.browser   依赖 x.cdp
```

V1 可先合并到一个 `x.browser` 包内，但代码内部必须保持 `cdp_*` 与 `browser_*` 模块边界。

当前第一阶段以 `c-source` 方式先交付 `x.cdp` 内核：

```text
dev/extlibs/browser/libs/x.cdp/
  package.json
  src/x_cdp.c
```

后续当 API 稳定、需要二进制发布或更复杂生命周期管理时，再迁移到官方 native DLL 包形态。

## 5. 依赖策略

### 5.0 xrt-first 强制约束

`x.browser` / `x.cdp` 是 xlang 官方自动化扩展，必须和 xrt、xlang 一起成长。xrt 是 xlang 的标准运行时来源，browser 扩展不得在 xrt 已有能力之外重复造轮子。

强制要求：

- CDP WebSocket transport 必须使用 xrt WebSocket API。
- CDP JSON parse/stringify 必须使用 xrt JSON API 或 xlang 标准 JSON 封装。
- 浏览器进程启动、等待、终止必须使用 xrt process API。
- HTTP bootstrap，包括 `/json/version`、`/json/list`、`/json/protocol`，必须优先使用 xrt HTTP / net API。
- path、fs、log、time、thread、lock、queue 等基础设施必须使用 xrt / xlang 标准能力。
- 如果 xrt 实现存在 BUG 或能力缺口，先修复 xrt，重新编译 xlang，再继续 browser 扩展开发。
- 不允许在 browser 扩展内引入私有 WebSocket、JSON、HTTP、process 实现作为默认路径。

本文件的实现分期直接使用 `[ ]`、`[X]`、`[~]` 标记跟踪，不再通过独立 tracking spec 跟踪项目状态。

### 5.1 运行时依赖

V1 不捆绑浏览器。

查找顺序：

1. `XLANG_BROWSER_CHROME` 环境变量。
2. `XLANG_BROWSER_EDGE` 环境变量。
3. Windows 注册表或常见安装路径。
4. `PATH` 中的 `chrome.exe` / `msedge.exe` / `chromium`。

用户也可显式传入 `executablePath`。

### 5.2 不依赖 Playwright

不需要：

- `node.exe`
- `playwright` npm 包
- Playwright 下载的浏览器

### 5.3 Chrome 136 之后的安全要求

Chrome 136 起，`--remote-debugging-port` / `--remote-debugging-pipe` 调试默认用户数据目录会受限制。V1 必须：

- 默认创建独立临时 `userDataDir`，source facade 通过 `LaunchPolicy` / `launch_policy` 承载默认 profile、独立 profile、localhost 绑定和自动端口策略。
- 禁止默认连接真实用户默认 profile，除非用户显式传入 `allowDefaultProfile: true`。
- 远程调试地址固定绑定 `127.0.0.1`。
- 关闭时默认删除临时 profile。

## 6. CDP 内核设计

### 6.1 启动流程

`browser.launch(headless)` 的内部步骤：

1. 解析 launch options。
2. 创建 user data dir。
3. 选择空闲 remote debugging port，或使用 `--remote-debugging-port=0`。
4. 启动浏览器进程：

```text
chrome.exe
  --remote-debugging-address=127.0.0.1
  --remote-debugging-port=0
  --user-data-dir=<isolated-profile>
  --no-first-run
  --no-default-browser-check
  --disable-background-networking
  --disable-popup-blocking
  --headless=new          可选
```

5. 读取 `<userDataDir>/DevToolsActivePort`。
6. 请求 `http://127.0.0.1:<port>/json/version`。
7. 取得 `webSocketDebuggerUrl`。
8. 建立 Browser WebSocket。
9. 调用 `Target.setDiscoverTargets` 和 `Target.setAutoAttach`。
10. 初始化 Browser 对象。

### 6.2 连接已有浏览器

`browser.connect(cdpUrl)` 支持：

- `http://127.0.0.1:9222`
- `ws://127.0.0.1:9222/devtools/browser/...`

如果传入 HTTP URL，内部先读取 `/json/version`。

### 6.3 WebSocket 消息模型

请求：

```json
{
  "id": 1,
  "method": "Page.navigate",
  "params": {
    "url": "https://example.com"
  },
  "sessionId": "..."
}
```

响应：

```json
{
  "id": 1,
  "result": {}
}
```

事件：

```json
{
  "method": "Network.requestWillBeSent",
  "params": {},
  "sessionId": "..."
}
```

内核职责：

- 单调递增 request id。
- pending request 表。
- session event queue。
- page event queue。
- route paused request queue。
- 超时和取消。
- 断线后标记所有 handle 失效。

### 6.4 线程模型

V1 推荐 native 内部使用一个 I/O 线程：

- 主调用线程发命令并等待条件变量。
- I/O 线程维护 WebSocket 读取、event dispatch、request resolve。
- 对 xlang 暴露同步 API。

这样可以先避开 xlang native callback ABI 不稳定的问题。V2 再增加 async/callback。

### 6.5 CDP domain 映射

| 能力 | CDP domain |
| --- | --- |
| 浏览器版本、关闭、窗口 | `Browser` |
| target、page、iframe session | `Target` |
| 导航、生命周期、截图、PDF | `Page` |
| JS 执行、对象句柄 | `Runtime` |
| DOM 查询、节点信息 | `DOM` |
| 鼠标、键盘、触摸 | `Input` |
| 请求、响应、cookie、body | `Network` |
| 请求拦截、mock、abort | `Fetch` |
| viewport、UA、时区、地理位置 | `Emulation` |
| cookie、localStorage、IndexedDB | `Storage` |
| role locator、可访问性快照 | `Accessibility` |
| console、异常、日志 | `Console` / `Log` / `Runtime` |
| trace、性能记录 | `Tracing` / `Performance` |

## 7. 对象与生命周期

V1 公开对象都使用 `custom` 包装 native `point` payload。

```xl
custom Browser
  extern "x_browser_browser_release" destroy()
end

custom BrowserContext
  extern "x_browser_context_release" destroy()
end

custom Page
  extern "x_browser_page_release" destroy()
end

custom Locator
  extern "x_browser_locator_release" destroy()
end
```

规则：

- `Browser.close()` 会关闭其创建的所有 context/page。
- `BrowserContext.close()` 会关闭其 page。
- `Page.close()` 后 locator 失效。
- `Locator` 是惰性描述，不持有 DOM 节点；每次 action 重新解析。
- `ElementHandle` V1 不公开，避免远端对象泄漏。后续如需要，作为高级 API 加入。

## 8. 错误模型

native 层统一返回状态码：

```c
#define X_BROWSER_OK 0
#define X_BROWSER_ERROR 1
#define X_BROWSER_TIMEOUT 2
#define X_BROWSER_NOT_FOUND 3
#define X_BROWSER_DETACHED 4
#define X_BROWSER_PROTOCOL_ERROR 5
```

xlang 门面层规则：

- 返回 handle 的 API 失败时抛 `BrowserError`。
- 等待超时抛 `TimeoutError`。
- selector 匹配 0 个元素抛 `LocatorNotFoundError`。
- strict locator 匹配多个元素抛 `LocatorStrictError`。
- 目标关闭抛 `TargetClosedError`。
- 网络 route 未处理超时抛 `RouteTimeoutError`。

错误信息必须包含：

- API 名称。
- URL 或 page id。
- selector 或 CDP method。
- timeout。
- CDP error code/message，如果有。
- trace 附件路径，如果启用 trace。

## 9. 默认超时

默认值：

- action timeout：30000 ms。
- navigation timeout：30000 ms。
- expect timeout：5000 ms。
- route timeout：30000 ms。
- connect timeout：10000 ms。

配置 API：

```xl
browser.set_default_timeout(int ms)
browser.set_default_navigation_timeout(int ms)

func BrowserContext.set_default_timeout(int ms) as void
func BrowserContext.set_default_navigation_timeout(int ms) as void

func Page.set_default_timeout(int ms) as void
func Page.set_default_navigation_timeout(int ms) as void
```

优先级：

1. 单次调用 options。
2. Page 默认值。
3. BrowserContext 默认值。
4. 全局默认值。

## 10. xlang API 定稿

### 10.1 API 设计规则

V1 同时提供简单重载和 JSON options 版本：

```xl
import x.browser as browser

page.goto("https://example.com")
page.goto_ex("https://example.com", "{\"waitUntil\":\"networkidle\",\"timeout\":60000}")
```

复杂返回值优先用 JSON：

```xl
auto headers = response.headers_json()
```

门面层后续可以增加 `dict` 返回重载，但 native ABI 不依赖复杂 xvalue 结构。

### 10.2 模块函数

```xl
import x.browser as browser

func browser.launch(bool headless) as Browser
func browser.launch_ex(string optionsJson) as Browser
func browser.connect(string cdpUrl) as Browser
func browser.connect_ex(string cdpUrl, string optionsJson) as Browser
func browser.find_chrome() as string
func browser.version() as string
func browser.last_error() as string
func browser.set_default_timeout(int ms) as void
func browser.set_default_navigation_timeout(int ms) as void
```

`launch_ex` options：

```json
{
  "browser": "chrome",
  "channel": "stable",
  "executablePath": "",
  "headless": true,
  "userDataDir": "",
  "keepUserDataDir": false,
  "args": [],
  "env": {},
  "viewport": { "width": 1280, "height": 720 },
  "proxy": { "server": "", "username": "", "password": "" },
  "downloadsPath": "",
  "slowMo": 0,
  "timeout": 10000,
  "allowDefaultProfile": false
}
```

### 10.3 Browser

```xl
func Browser.version() as string
func Browser.browser_name() as string
func Browser.cdp_url() as string
func Browser.cdp() as CdpBrowser
func Browser.is_connected() as bool
func Browser.new_context() as BrowserContext
func Browser.new_context_ex(string optionsJson) as BrowserContext
func Browser.new_page() as Page
func Browser.close() as void
```

`Browser.cdp()` 返回共享底层连接的低层 CDP handle，不拥有浏览器生命周期。`Browser.close()` 后该 handle 立即失效。

`new_context_ex` options：

```json
{
  "viewport": { "width": 1280, "height": 720 },
  "deviceScaleFactor": 1,
  "isMobile": false,
  "hasTouch": false,
  "userAgent": "",
  "locale": "zh-CN",
  "timezoneId": "Asia/Shanghai",
  "geolocation": { "latitude": 0, "longitude": 0, "accuracy": 0 },
  "permissions": [],
  "colorScheme": "light",
  "reducedMotion": "no-preference",
  "acceptDownloads": true,
  "ignoreHTTPSErrors": false,
  "extraHTTPHeaders": {},
  "storageState": ""
}
```

### 10.4 BrowserContext

```xl
func BrowserContext.new_page() as Page
func BrowserContext.pages_json() as string
func BrowserContext.close() as void

func BrowserContext.set_default_timeout(int ms) as void
func BrowserContext.set_default_navigation_timeout(int ms) as void

func BrowserContext.cookies_json() as string
func BrowserContext.set_cookies_json(string cookiesJson) as void
func BrowserContext.clear_cookies() as void
func BrowserContext.storage_state() as string
func BrowserContext.load_storage_state(string stateJson) as void

func BrowserContext.set_extra_http_headers(string headersJson) as void
func BrowserContext.set_offline(bool offline) as void
func BrowserContext.grant_permissions(string permissionsJson) as void
func BrowserContext.clear_permissions() as void

func BrowserContext.route(string urlPattern, string actionJson) as void
func BrowserContext.unroute(string urlPattern) as void

func BrowserContext.tracing_start(string optionsJson) as void
func BrowserContext.tracing_stop(string path) as void
```

V1 route 的 `actionJson` 先支持静态动作：

```json
{ "action": "abort", "errorReason": "Failed" }
{ "action": "continue", "headers": { "X-Test": "1" } }
{ "action": "fulfill", "status": 200, "headers": {}, "body": "ok" }
```

动态 route 留到 V1.5：

```xl
func Page.wait_route(string urlPattern, int timeoutMs) as Route
func Route.request_json() as string
func Route.continue_ex(string optionsJson) as void
func Route.abort(string reason) as void
func Route.fulfill(string responseJson) as void
```

### 10.5 Page

导航与页面信息：

```xl
func Page.goto(string url) as Response
func Page.goto_ex(string url, string optionsJson) as Response
func Page.reload() as Response
func Page.reload_ex(string optionsJson) as Response
func Page.back() as Response
func Page.forward() as Response
func Page.close() as void

func Page.url() as string
func Page.title() as string
func Page.content() as string
func Page.set_content(string html) as void
func Page.set_content_ex(string html, string optionsJson) as void
```

等待：

```xl
func Page.wait_load_state(string state) as void
func Page.wait_load_state_ex(string state, int timeoutMs) as void
func Page.wait_url(string urlPattern) as void
func Page.wait_url_ex(string urlPattern, int timeoutMs) as void
func Page.wait_event(string eventName, int timeoutMs) as string
func Page.next_event(int timeoutMs) as string
func Page.wait_request(string urlPattern, int timeoutMs) as Request
func Page.wait_response(string urlPattern, int timeoutMs) as Response
```

执行脚本：

```xl
func Page.evaluate(string expression) as string
func Page.evaluate_json(string expression, string argJson) as string
func Page.evaluate_bool(string expression) as bool
func Page.evaluate_int(string expression) as int
func Page.evaluate_float(string expression) as float
```

截图与导出：

```xl
func Page.screenshot(string path) as void
func Page.screenshot_ex(string path, string optionsJson) as void
func Page.pdf(string path) as void
func Page.pdf_ex(string path, string optionsJson) as void
```

Locator：

```xl
func Page.locator(string selector) as Locator
func Page.get_by_text(string text) as Locator
func Page.get_by_text_ex(string text, string optionsJson) as Locator
func Page.get_by_role(string role, string name) as Locator
func Page.get_by_label(string text) as Locator
func Page.get_by_placeholder(string text) as Locator
func Page.get_by_alt(string text) as Locator
func Page.get_by_title(string text) as Locator
func Page.get_by_testid(string testid) as Locator
```

输入与页面级操作：

```xl
func Page.keyboard_press(string key) as void
func Page.keyboard_type(string text) as void
func Page.mouse_click(int x, int y) as void
func Page.mouse_move(int x, int y) as void
func Page.mouse_wheel(int dx, int dy) as void
func Page.bring_to_front() as void
```

网络与日志：

```xl
func Page.requests_json() as string
func Page.responses_json() as string
func Page.console_json() as string
func Page.errors_json() as string
func Page.response_body(Response response) as string
func Page.request_body(Request request) as string
```

### 10.6 Locator

组合：

```xl
func Locator.locator(string selector) as Locator
func Locator.get_by_text(string text) as Locator
func Locator.get_by_role(string role, string name) as Locator
func Locator.filter(string optionsJson) as Locator
func Locator.first() as Locator
func Locator.last() as Locator
func Locator.nth(int index) as Locator
```

查询：

```xl
func Locator.count() as int
func Locator.text() as string
func Locator.all_text_json() as string
func Locator.attr(string name) as string
func Locator.input_value() as string
func Locator.is_visible() as bool
func Locator.is_enabled() as bool
func Locator.is_checked() as bool
func Locator.bounding_box_json() as string
```

等待：

```xl
func Locator.wait() as void
func Locator.wait_ex(string state, int timeoutMs) as void
```

`state`：

- `attached`
- `detached`
- `visible`
- `hidden`
- `enabled`
- `editable`
- `stable`

动作：

```xl
func Locator.click() as void
func Locator.click_ex(string optionsJson) as void
func Locator.dblclick() as void
func Locator.hover() as void
func Locator.fill(string value) as void
func Locator.clear() as void
func Locator.press(string key) as void
func Locator.type(string text) as void
func Locator.check() as void
func Locator.uncheck() as void
func Locator.select_option(string value) as void
func Locator.set_input_files(string filesJson) as void
func Locator.scroll_into_view() as void
func Locator.screenshot(string path) as void
```

`click_ex` options：

```json
{
  "button": "left",
  "clickCount": 1,
  "delay": 0,
  "position": { "x": 0, "y": 0 },
  "timeout": 30000,
  "force": false,
  "trial": false,
  "noWaitAfter": false
}
```

### 10.7 Request / Response / Route

V1 可以先将 `Request`、`Response`、`Route` 作为 handle，也可以只返回 JSON。推荐 handle + JSON：

```xl
func Request.url() as string
func Request.method() as string
func Request.headers_json() as string
func Request.post_data() as string

func Response.url() as string
func Response.status() as int
func Response.ok() as bool
func Response.headers_json() as string
func Response.text() as string
func Response.body_base64() as string
```

### 10.8 `x.cdp` 原始协议 API

```xl
import x.cdp as cdp

func cdp.connect(string cdpUrl) as CdpBrowser
func cdp.close(CdpBrowser browser) as void
func cdp.send(CdpBrowser browser, string method, string paramsJson) as string
func cdp.send_session(CdpBrowser browser, string sessionId, string method, string paramsJson) as string
func cdp.next_event(CdpBrowser browser, int timeoutMs) as string
func cdp.targets_json(CdpBrowser browser) as string
func cdp.protocol_json(CdpBrowser browser) as string
```

第一阶段 `c-source` 内核 API：

```xl
func cdp.version() as string
func cdp.last_error() as string
func cdp.error(point conn) as string

func cdp.version_url(string endpoint) as string
func cdp.list_url(string endpoint) as string
func cdp.protocol_url(string endpoint) as string
func cdp.http_get(string url, int timeout_ms) as string

func cdp.connect(string websocket_url, int timeout_ms) as point
func cdp.connect_http(string endpoint, int timeout_ms) as point
func cdp.is_open(point conn) as bool
func cdp.send(point conn, string method, string params_json, int timeout_ms) as string
func cdp.send_session(point conn, string session_id, string method, string params_json, int timeout_ms) as string
func cdp.next_event(point conn, int timeout_ms) as string
func cdp.close(point conn) as bool
func cdp.dispose(point conn) as bool
```

示例：

```xl
auto c = cdp.connect("http://127.0.0.1:9222")
auto version = cdp.send(c, "Browser.getVersion", "{}")
print(version)
cdp.close(c)
```

第一阶段示例：

```xl
point c = cdp.connect_http("http://127.0.0.1:9222", 5000)
string version = cdp.send(c, "Browser.getVersion", "{}", 5000)
print(version)
cdp.close(c)
cdp.dispose(c)
```

## 11. Selector 与 Locator 设计

### 11.1 Selector 语法

V1 支持：

- `css=button.save`
- `xpath=//button`
- `text=保存`
- `role=button[name="保存"]`
- `label=用户名`
- `placeholder=请输入用户名`
- `alt=logo`
- `title=详情`
- `testid=submit`

无前缀规则：

- 以 `//` 或 `..` 开头按 XPath。
- 其他按 CSS。

### 11.2 Locator 是惰性的

`Locator` 保存 selector AST，不保存 DOM node。每次 `click`、`text`、`count` 都重新解析。

优点：

- DOM 替换后 locator 仍可用。
- 动作前可以自动等待。
- 避免长期持有 remote object。

### 11.3 strict mode

默认 strict：

- action 类 API 必须匹配 exactly one element。
- 匹配 0 个，等待直到超时。
- 匹配多个，立即抛 `LocatorStrictError`，除非用户调用 `first()` / `nth()`。

查询类 API：

- `count()` 允许多个。
- `all_text_json()` 允许多个。

### 11.4 Frame 支持

V1 先支持：

```xl
func Page.frame_locator(string selector) as FrameLocator
func FrameLocator.locator(string selector) as Locator
```

实现上通过 injected script 遍历 iframe，并结合 CDP `Target` / `Runtime` execution context。

## 12. 自动等待与 actionability

每个 action 都走统一 pipeline：

1. 计算 deadline。
2. 解析 locator。
3. 检查 strict。
4. 检查 attached。
5. 按 action 类型检查 visible / stable / receives events / enabled / editable。
6. `scrollIntoViewIfNeeded`。
7. 再次计算 bounding box。
8. 通过 `Input` domain 执行动作。
9. 如果动作可能触发导航，等待目标 load state。

### 12.1 visible

可见条件：

- 节点连接在 document。
- computed style 非 `visibility:hidden`。
- bounding box 宽高大于 0。
- `display:none` 不可见。
- `opacity:0` 仍视为可见，与 Playwright 行为保持一致。

### 12.2 stable

稳定条件：

- 连续两个 `requestAnimationFrame` 中 bounding box 相同。
- 没有 CSS transition / animation 导致 bbox 变化。

### 12.3 receives events

取点击点，通常为元素可见区域中心。通过 `document.elementFromPoint(x, y)` 检查命中元素是否为目标或目标子节点。需要穿透 shadow DOM。

### 12.4 enabled

不可用条件：

- 表单元素有 `disabled`。
- 位于 disabled fieldset 中。
- 元素或祖先有 `aria-disabled=true`。

### 12.5 editable

可编辑条件：

- enabled。
- input / textarea / select 非 readonly。
- contenteditable 为 true。
- ARIA readonly 不成立。

### 12.6 force

`force=true` 只跳过非必要检查：

- 可跳过 receives events。
- 不跳过 attached。
- 不跳过 strict。
- 不跳过基础坐标计算。

## 13. 注入脚本

`x_browser.dll` 内置一段 injected script，在每个 frame 的 isolated world 中安装。

职责：

- selector 解析。
- role / accessible name 简化计算。
- label / placeholder / text / testid 查询。
- actionability 检查。
- bounding box。
- scroll into view。
- text normalization。

注入策略：

- `Page.addScriptToEvaluateOnNewDocument`。
- 新 frame 出现时补注入。
- 脚本版本号和页面 world 绑定，防止重复注入。

V1 role locator 不要求完全实现 ARIA 规范，但必须覆盖常用标签：

- `button`
- `link`
- `textbox`
- `checkbox`
- `radio`
- `combobox`
- `listbox`
- `option`
- `heading`
- `img`
- `table`
- `row`
- `cell`

## 14. 导航与 load state

支持 load state：

- `commit`：CDP `Page.frameNavigated` 或 loader 改变。
- `domcontentloaded`：`Page.lifecycleEvent`。
- `load`：`Page.lifecycleEvent`。
- `networkidle`：500 ms 内无 inflight network request。

`goto` options：

```json
{
  "timeout": 30000,
  "waitUntil": "load",
  "referer": "",
  "headers": {}
}
```

导航失败分类：

- DNS / connect / TLS 失败：`NavigationError`。
- 超时：`TimeoutError`。
- 下载触发：返回 `Response`，并产生 `download` event。
- same-document navigation：允许成功。

## 15. 网络设计

### 15.1 事件

内核监听并缓存：

- `request`
- `response`
- `requestfinished`
- `requestfailed`
- `websocket`

公开：

```xl
func Page.wait_request(string urlPattern, int timeoutMs) as Request
func Page.wait_response(string urlPattern, int timeoutMs) as Response
func Page.requests_json() as string
func Page.responses_json() as string
```

### 15.2 Route 拦截

V1 使用 `Fetch.enable` 实现。

匹配：

- glob：`**/*.png`
- exact：`https://example.com/api`
- regex：`regex:^https://`

动作：

- `abort`
- `continue`
- `fulfill`
- `modify headers`
- `modify method`
- `modify postData`

### 15.3 Service Worker

因为 service worker 可能吞掉页面网络事件，context options 提供：

```json
{ "serviceWorkers": "allow" | "block" }
```

V1 默认 `allow`，需要强可观测或 mock 时建议 `block`。

### 15.4 HAR

V1 trace 中记录 HAR-like JSON，不承诺完全 HAR 1.2 兼容。V2 再补完整 HAR 导入/回放。

## 16. 上下文隔离与仿真

BrowserContext 对应 CDP 中的 browser context。

实现：

- `Target.createBrowserContext`
- `Target.createTarget`
- `Target.disposeBrowserContext`

每个 context 独立：

- cookies
- localStorage
- sessionStorage
- cache
- permissions
- extra headers
- network emulation

Emulation API：

```xl
func BrowserContext.set_viewport(int width, int height) as void
func BrowserContext.set_user_agent(string userAgent) as void
func BrowserContext.set_locale(string locale) as void
func BrowserContext.set_timezone(string timezoneId) as void
func BrowserContext.set_geolocation(float lat, float lon) as void
func BrowserContext.set_color_scheme(string scheme) as void
func BrowserContext.set_reduced_motion(string value) as void
```

## 17. 输入设计

键盘：

- `press("Enter")`
- `press("Control+A")`
- `type("text")`

鼠标：

- `click`
- `dblclick`
- `move`
- `down`
- `up`
- `wheel`
- `drag_to`

触摸：

- V1 仅在 mobile emulation 下提供 `tap`。

文件上传：

- 使用 DOM file input 设置。
- API：`Locator.set_input_files(filesJson)`。

## 18. 下载与对话框

下载：

```xl
func Page.wait_download(int timeoutMs) as Download
func Download.suggested_filename() as string
func Download.path() as string
func Download.save_as(string path) as void
```

Dialog：

```xl
func Page.wait_dialog(int timeoutMs) as Dialog
func Dialog.message() as string
func Dialog.accept() as void
func Dialog.dismiss() as void
func Dialog.accept_with_text(string text) as void
```

V1 可先支持 alert / confirm / prompt。

## 19. Trace 设计

trace zip 内容：

```text
trace.zip
  trace.jsonl
  network.jsonl
  console.jsonl
  errors.jsonl
  snapshots/
    000001.html
  screenshots/
    000001.png
  attachments/
```

每个 action 记录：

```json
{
  "time": "2026-05-12T10:00:00Z",
  "type": "action",
  "api": "Locator.click",
  "page": "page-1",
  "url": "https://example.com",
  "selector": "text=More information",
  "startMs": 123,
  "endMs": 456,
  "status": "ok",
  "screenshot": "screenshots/000001.png",
  "snapshot": "snapshots/000001.html"
}
```

失败时必须自动记录：

- 当前 URL。
- 最后 50 条 action。
- 最后 100 条 network。
- console error。
- page screenshot。
- DOM snapshot。

## 20. 断言 API

```xl
import x.browser.assert as expect

func expect.visible(Locator locator) as void
func expect.hidden(Locator locator) as void
func expect.enabled(Locator locator) as void
func expect.disabled(Locator locator) as void
func expect.text(Locator locator, string expected) as void
func expect.contains_text(Locator locator, string expected) as void
func expect.count(Locator locator, int expected) as void
func expect.value(Locator locator, string expected) as void
func expect.url(Page page, string pattern) as void
func expect.title(Page page, string expected) as void
func expect.ok(Response response) as void
```

所有断言自动重试直到 timeout。

扩展版本：

```xl
func expect.visible_ex(Locator locator, int timeoutMs) as void
func expect.text_ex(Locator locator, string expected, string optionsJson) as void
```

## 21. 示例

### 21.1 最小导航

```xl
import x.browser as browser

func main() as int
  auto b = browser.launch(true)
  auto p = b.new_page()
  p.goto("https://example.com")
  p.get_by_text("More information").click()
  p.screenshot("out.png")
  b.close()
  return 0
end
```

### 21.2 表单自动化

```xl
import x.browser as browser
import x.browser.assert as expect

func main() as int
  auto b = browser.launch(true)
  auto ctx = b.new_context()
  auto p = ctx.new_page()

  p.goto("http://localhost:8080/login")
  p.get_by_label("用户名").fill("admin")
  p.get_by_label("密码").fill("123456")
  p.get_by_role("button", "登录").click()

  expect.url(p, "**/dashboard")
  expect.visible(p.get_by_text("控制台"))

  b.close()
  return 0
end
```

### 21.3 网络 mock

```xl
import x.browser as browser

func main() as int
  auto b = browser.launch(true)
  auto ctx = b.new_context()
  ctx.route("**/api/profile", "{\"action\":\"fulfill\",\"status\":200,\"body\":\"{\\\"name\\\":\\\"xl\\\"}\"}")

  auto p = ctx.new_page()
  p.goto("http://localhost:8080")
  print(p.get_by_text("xl").text())

  b.close()
  return 0
end
```

### 21.4 原始 CDP

```xl
import x.browser as browser
import x.cdp as cdp

func main() as int
  auto b = browser.launch(true)
  auto result = cdp.send(b.cdp(), "Browser.getVersion", "{}")
  print(result)
  b.close()
  return 0
end
```

## 22. Native C ABI 草案

内部 C ABI 使用 opaque handle。

```c
typedef void* xbrowser_handle;

int x_browser_launch(const char* options_json, xbrowser_handle* out_browser);
int x_browser_connect(const char* cdp_url, const char* options_json, xbrowser_handle* out_browser);
void x_browser_release(xbrowser_handle browser);

int x_browser_new_context(xbrowser_handle browser, const char* options_json, xbrowser_handle* out_context);
void x_browser_context_release(xbrowser_handle context);

int x_browser_new_page(xbrowser_handle context, xbrowser_handle* out_page);
void x_browser_page_release(xbrowser_handle page);

int x_browser_page_goto(xbrowser_handle page, const char* url, const char* options_json, xbrowser_handle* out_response);
int x_browser_page_screenshot(xbrowser_handle page, const char* path, const char* options_json);
const char* x_browser_page_title(xbrowser_handle page);
const char* x_browser_page_url(xbrowser_handle page);
const char* x_browser_page_evaluate(xbrowser_handle page, const char* expression, const char* arg_json);

int x_browser_page_locator(xbrowser_handle page, const char* selector, xbrowser_handle* out_locator);
int x_browser_locator_click(xbrowser_handle locator, const char* options_json);
int x_browser_locator_fill(xbrowser_handle locator, const char* value, const char* options_json);
const char* x_browser_locator_text(xbrowser_handle locator, const char* options_json);
void x_browser_locator_release(xbrowser_handle locator);

const char* x_browser_last_error(void);
const char* x_browser_handle_last_error(xbrowser_handle handle);
void x_browser_free_string(const char* value);
```

字符串返回值规则：

- native 返回的 `const char*` 必须由 x.browser 门面复制或通过 `x_browser_free_string` 释放。
- 对 xlang 暴露时优先复制成 xlang string，避免悬挂指针。

## 23. 实现分期

### M0：协议内核

- [X] `c-source` 包骨架：`dev/extlibs/browser/libs/x.cdp`。
- [X] 基于 xrt WebSocket 的 CDP client。
- [X] 基于 xrt HTTP 的 bootstrap：`http_get`。
- [X] `/json/version` / `/json/list` / `/json/protocol` URL helper。
- [X] HTTP endpoint 到 Browser WebSocket 的连接：`connect_http`。
- [X] Browser WebSocket 直连：`connect`。
- [~] request/response/event 分发：已支持同步 command response、event queue、pending response 保留；source facade 已新增 `EventDispatchPlan` / `event_dispatch_plan`，承载 session、队列容量、queued/dispatched/dropped/canceled 和最近事件状态，用于表达 session/page 分队列设计意图；尚未实现完整取消和真实 session/page 分队列。
- [~] Target attach/session 管理：已提供 `send_session` 原始出口和 `create_target` / `create_context` / `dispose_context` / `create_target_context` / `attach_target` / `close_target` 最小封装；source facade 已提供轻量 `Target` 对象、`Browser.new_target`、`wrap_target` 和 `attach_target_object`，封装 targetId/attach/attach_page/close；已新增 `TargetLifecyclePlan` / `target_lifecycle_plan`，承载 create/attach/page wrap/close intent、flatten 和 timeout 状态；已新增 `ResourceCascadeClosePlan` / `resource_cascade_close_plan`，承载 pages -> contexts -> targets -> browser 的级联关闭完成/失败状态；完整 target 事件生命周期和真实父子资源级联释放尚未封装。
- [X] `cdp.send` / `cdp.send_session` / `cdp.next_event`。
- [X] 基础 smoke：`dev/extlibs/browser/xcdp_smoke.xl`。

验收：

- [~] 能连接手工启动的 Chrome：已提供 `x.cdp.connect_http` 并在 `xcdp_chrome_smoke.xl` 中覆盖手工 Chrome 连接路径；当前真实验证仍受 xrt HTTP `/json/version` 超时问题和并行 release 二进制状态影响。
- [~] 能调用 `Browser.getVersion`：已提供低层 `x.cdp.browser_get_version(conn, timeout_ms)`，返回 CDP result JSON；真实 Chrome 验证仍受当前 xrt HTTP `/json/version` 超时问题和 `xl.exe` 缺失状态阻塞。
- [~] 能创建 target 并收到 page 事件：已提供 `x.cdp.target_set_discover` / `wait_target_created`，并在 `xcdp_chrome_smoke.xl` 中用命名 API 覆盖 target 创建与事件接收；尚未完成稳定的真实 Chrome 回归。

### M1：Browser / Page 基础

- [~] `x.browser` 包入口：已新增 `dev/extlibs/browser/libs/x.browser` source facade，依赖 `x.cdp` 并提供 `version` / `last_error` / `error` / `error_info` / `find_chrome` / launch 生命周期 / `connect` / `connect_http` / `close` / context/page / storage / emulation / network / route / dialog / download / pdf / accessibility / screencast / `locator_*` / `expect_*` 等第一批高层入口；已新增 `x.browser.assert` 和 `x.browser.trace` source facade，分别承载断言与 trace API；已验证 `Browser` / `BrowserContext` / `Page` / `Locator` / `Dialog` / `Download` / `Route` / `Request` / `Response` / `BrowserError` source 包装类、`wrap_browser` / `connect_browser` / `wrap_context` / `wrap_page` / `wrap_target_page` / `wrap_locator` / `wrap_dialog` / `wrap_download`，以及 launch、context、page、storage、emulation、network、route、dialog、download、pdf/accessibility 和 locator 的对象式入口；已提供并验证 `Browser` / `BrowserContext` / `Page` 的默认 timeout 状态读写，且对象方法在 `timeout_ms <= 0` 时会使用对象默认 timeout，`Page.locator` / `Page.wait_dialog_object` / `Browser.wait_download_object` 会继承父对象当前默认 timeout；已提供 `new_page_object` / `new_context_page_object` / `attach_page_object` / `close_page` 和 `Page.close` 的初步对象生命周期包装；已新增 `LaunchPolicy` / `launch_policy` 和 `ContextApplyPlan` / `context_apply_plan`，用于承载启动安全策略与 context defaults 应用计划；JSON 字段解析和完整父子资源释放仍待继续封装。
- [~] `browser.launch`：已提供低层 `x.cdp.find_chrome()` / `launch_chrome()` / `read_devtools_active_port()` / `wait_devtools_active_port()` / `connect_user_data_dir()` / `process_*()`，覆盖 executable 发现、独立 profile 启动、DevToolsActivePort 轮询读取、连接和显式进程释放；这些生命周期能力已在 `x.browser.find_chrome` / `create_user_data_dir` / `launch_chrome` / `launch_process` / `wrap_process` / `wait_devtools_active_port` / `connect_user_data_dir` / `process_*` 暴露；已提供轻量 `ChromeProcess` 对象包装 `process` / `is_running` / `kill_tree` / `dispose`；source facade 已提供 `LaunchOptions` / `LaunchPolicy` / `LaunchResult`、`launch_options_full` / `launch_policy` 和 `launch_with_options`，用于承载 executable/userDataDir/port/headless/keepUserDataDir/timeout、默认 profile/独立 profile/localhost/自动端口策略及启动结果；已新增轻量 `LaunchCleanupPlan` / `launch_cleanup_plan` / `LaunchResult.cleanup_plan()`，只记录 close/dispose/cleanup intent 和 `keepUserDataDir` 策略，不执行目录删除；已新增 `LaunchPreparePlan` / `launch_prepare_plan`，承载 executable/userDataDir/port、autoPort 决策、独立 profile 需求和 prepared 状态；已新增 `LaunchConnectPlan` / `launch_connect_plan`，承载启动后连接、connected/failed 和失败清理意图；自动端口选择、启动后连接为 `Browser` 对象和异常路径真实自动清理尚未封装。
- [~] 独立 userDataDir：已提供低层 `x.cdp.create_user_data_dir(base_dir)` 创建独立 profile 目录，并在 `x.browser.create_user_data_dir` 暴露；source facade 已提供 `LaunchPolicy` / `launch_policy` 承载默认禁止真实用户 profile、要求独立 userDataDir、localhost 绑定和自动端口策略；已新增 `LaunchPreparePlan` / `launch_prepare_plan`，用于显式记录 launch 前是否仍需要独立 userDataDir；已新增 `UserDataDirLifecyclePlan` / `user_data_dir_lifecycle_plan`，承载 created/release/cleanup request 和 keep 策略，不执行目录删除；尚未接入 launch 默认策略执行和真实 release 后清理。
- [~] `Browser.new_context`：已提供低层 `x.cdp.create_context` / `x.cdp.dispose_context`，并在 `x.browser.new_context` / `new_context_object` / `close_context`、`Browser.new_context_object` 和 `BrowserContext.close` 中暴露；`Browser.new_context_object` 会让 `BrowserContext` 继承 `Browser` 当前默认 timeout；`BrowserContext` 已封装 viewport/userAgent/locale/timezone/geolocation/offline/cacheDisabled/permissions 默认状态、`apply_defaults(session_id, timeout_ms)` 显式应用入口，并会在 `BrowserContext.new_page_object` 成功 attach 后自动应用这些默认项；source facade 已提供轻量 `ContextOptions` / `wrap_context_options`、`ContextApplyPlan` / `context_apply_plan` / `ContextOptions.apply_plan` 和 `ContextLifecyclePlan` / `context_lifecycle_plan`，用于承载 context defaults、storage state 应用计划以及 context/page/target/close 生命周期状态；已新增 `ContextOptionsParsePlan` / `context_options_parse_plan`，承载 raw options JSON、option count、storageState presence 和 parsed 状态；已新增 `ContextStorageStatePlan` / `context_storage_state_plan`，承载 options 中 storageState 的 raw/apply/parsed/failed 状态；context options JSON 真实解析尚未封装。
- [~] `Context.new_page`：已提供低层 `x.cdp.create_target_context`，并在 `x.browser.new_context_page` 和 `BrowserContext.new_page` 中暴露；已新增 `BrowserContext.new_page_object` 和 `x.browser.new_context_page_object`，会创建 target、attach session 并返回携带 `target_id` / `session_id` 的 `Page`；`BrowserContext.new_page_object` 已在成功 attach 后自动应用 context 默认项；source facade 已新增 `ContextLifecyclePlan` / `context_lifecycle_plan`，用于记录 page/target 数量、defaults/permissions/storage 应用状态和 close 状态；已新增 `ContextDefaultApplyPlan` / `context_default_apply_plan`，承载 session、默认项数量、timeout、applied/completed/failed 状态；已新增 `ContextPageCreatePlan` / `context_page_create_plan`，承载 context page 创建后 defaults 请求、应用和失败状态；顶层 `new_context_page_object` 的 options 自动应用和父子资源自动释放尚未完成。
- [~] `Page.goto`：已提供低层 `x.cdp.page_enable` / `x.cdp.page_navigate` / `x.cdp.page_goto` / `x.cdp.page_goto_wait`，支持 `load` / `domcontentloaded` / `none` 等待状态；已提供 `x.cdp.page_wait_load_state` 和 `x.cdp.page_reload` 作为 `waitUntil` / reload 的低层基础；已在 `x.browser.Page.goto/reload/wait_load_state` 对象方法中暴露；source facade 已提供轻量 `NavigationOptions` 和 `Page.goto_ex` / `Page.reload_ex` 承载 waitUntil/timeout；已新增 `NavigationWaitPlan` / `navigation_wait_plan`，承载 URL、waitUntil、timeout、navigation started、load state 和 network idle 状态；已新增 `NavigationNetworkIdlePlan` / `navigation_network_idle_plan`，承载 networkidle 的 inflight、quiet window、idle reached 和 timeout 状态；`bringToFront` 已有 C 侧内部实现，但受当前共享桥接 128 符号上限限制尚未公开导出；真实 `networkidle` 事件聚合语义尚未封装。
- [~] `Page.title/url/content`：已提供低层 `x.cdp.page_title` / `x.cdp.page_url` / `x.cdp.page_content`，并在 `x.browser.Page.title/url/content` 对象方法中暴露。
- [~] `Page.evaluate`：已提供低层 `x.cdp.runtime_evaluate`，并在 `x.browser.Page.evaluate` 对象方法中暴露；source facade 已提供轻量 `EvaluateResult` 和 `Page.evaluate_result`，用于承载 raw CDP result、类型和值文本；已新增轻量 `TypedEvaluateResult` / `typed_evaluate_result`，用于承载调用方已解析出的 string/int/bool/float-e6 结果；已新增 `EvaluateParsePlan` / `evaluate_parse_plan`，承载 raw result、expected type、has value、parsed/failed 状态；已新增 `EvaluateTypedParsePlan` / `evaluate_typed_parse_plan`，承载 EvaluateResult 到 typed result 的 expected/actual type、value、parsed/failed 状态；自动从 CDP JSON 解析 typed evaluate 尚未封装。
- [~] `Page.screenshot`：已提供低层 `x.cdp.page_screenshot` PNG 保存，并在 `x.browser.Page.screenshot` 对象方法中暴露；已提供轻量 `ScreenshotOptions` 和 `Page.screenshot_ex` source 层入口，当前仅复用 path 调用底层截图；`ScreenshotOptions` 已能承载 format/fullPage 和 clip 矩形，并提供 `screenshot_options_clip` builder；已新增 `ScreenshotRenderPlan` / `screenshot_render_plan`，承载 path、format、fullPage、clip、render requested/completed 状态；已新增 `ScreenshotOptionsApplyPlan` / `screenshot_options_apply_plan`，承载截图 option 数量、clip/fullPage、applied/failed 状态；clip/fullPage/type 等真实截图 options 尚未接入底层。
- [~] `Page.close` / `Browser.close`：已提供低层 `x.cdp.close_target` 与连接 `close/dispose`；`Page.close` / `x.browser.close_page` 已封装 target 关闭，`Browser.close` / `x.browser.close` 已封装连接关闭与释放；source facade 已提供轻量 `BrowserSession`，用于记录 Browser 下 contexts/pages/targets 和 close 状态；已新增轻量 `ResourceClosePlan` / `resource_close_plan` / `BrowserSession.close_plan()`，承载 pages -> contexts -> browser 的关闭顺序和资源数量；已新增 `ResourceCascadeClosePlan` / `resource_cascade_close_plan`，承载 pages/contexts/targets/browser 级联关闭完成度和失败状态；已新增 `TargetLifecyclePlan` / `target_lifecycle_plan`，承载 target close intent 与 attach/page wrap 状态；真实对象树级联释放尚未完成。

验收：

- 打开 example.com，读取标题，截图保存。
- headless/headful 均可运行。
- 失败时输出 last_error。

### M2：Locator 和动作

- [~] injected script：已提供低层 `x.cdp.page_add_init_script` / `page_remove_init_script`，映射 CDP `Page.addScriptToEvaluateOnNewDocument` / `Page.removeScriptToEvaluateOnNewDocument`；已在 `x.browser.page_add_init_script` / `page_remove_init_script` 和 `Page.add_init_script` / `remove_init_script` 暴露；source facade 已新增 `FrameBindingPlan` / `frame_binding_plan`，承载 frame 绑定时的 execution context、init script 计数和 bound 状态；尚未实现 x_browser 内置注入脚本和 frame 级自动补注入。
- [~] css/xpath/text/testid selector：已提供低层 CSS selector `x.cdp.locator_click` / `locator_fill` / `locator_text`，以及既有 `expect_visible/text/count`；source facade 已提供轻量 `Selector` 描述对象和 `make_selector` / `css` / `text` / `testid` / `xpath` 构造入口，`Page.locator_selector` 可将 selector 描述转换为 locator 查询字符串；已新增轻量 `SelectorGroup` / `selector_group` 承载多个 selector 查询和 strict intent；已新增 `SelectorParsePlan` / `selector_parse_plan`，承载 kind/value/normalized query、supported/parsed/unsupported 状态；xpath/text/testid selector 引擎尚未接入底层执行。
- [~] strict mode：已提供低层 `x.cdp.locator_count`，可用于高层 strict mode 的 0/1/>1 判定；已提供 `Locator.count` / `Locator.strict`、`Page.locator_count` / `Page.locator_strict` 和顶层 `x.browser.locator_count` / `locator_strict`；source facade 已提供轻量 `StrictResult`、`Locator.strict_result` 和 `Page.locator_strict_result` 承载 selector/count/message；完整 Playwright 风格 strict 错误详情尚未封装。
- [~] visible/stable/enabled/editable/receives-events：已提供低层 `x.cdp.locator_state` 返回 CSS locator 的 `count/visible/enabled/editable/receivesEvents` JSON；已在 `x.browser.locator_state` / `Page.locator_state` / `Locator.state` 暴露 raw state；source facade 已提供轻量 `ActionabilityState`、`Page.locator_actionability` 和 `Locator.actionability` 承载 raw/visible/enabled/editable/receivesEvents/stable 字段；已新增 `ActionabilityState.ready()` 和轻量 `ActionabilityPolicy` / `actionability_policy`，用于描述 click/fill 等动作的检查需求并做 source 层显式判定；已新增 `ActionabilityCheckPlan` / `actionability_check_plan`，显式承载 selector/action/timeout/force 及检查请求、成功、失败状态；stable 轮询、自动 JSON 字段解析和高层 actionability pipeline 尚未封装。
- [~] click/fill/press/type/hover/check/select：已提供低层 CSS `locator_click` / `locator_fill` / `locator_hover` / `locator_check` / `locator_select` / `locator_press` / `locator_type`，并提供 `input_dispatch_key_event` / `input_insert_text` 作为 CDP Input 域真实输入基础；已提供 `page_mouse_move` / `page_mouse_click` / `page_mouse_wheel` 作为页面级 mouse 低层 API；`x.browser` source facade 已暴露顶层 `locator_*` / `page_mouse_*` / `input_*` 包装和 `Locator.click/fill/hover/check/select/press/type/text/count`、`Page.locator_*`、`Page.mouse_*`、`Page.key_event/insert_text` 对象入口；已提供轻量 `ActionOptions` 和 `click_ex/fill_ex/hover_ex/check_ex/select_ex` source 层入口；已新增轻量 `KeyboardActionPlan` / `keyboard_action_plan`、`MouseActionPlan` / `mouse_action_plan`、`FileUploadPlan` / `file_upload_plan` 和 `DragPlan` / `drag_plan`，承载完整键盘/鼠标/上传/拖拽参数；高层 actionability pipeline、真实 drag 和 file upload 底层执行尚未封装。
- [~] timeout 和 force：已提供低层 expect/action 的 `timeout_ms` 参数和 `locator_state` 查询基础；`x.browser` 对象式 facade 已封装默认 timeout 状态和 `timeout_ms <= 0` 的默认值解析；`ActionOptions` 已能承载 timeout/force，`ActionabilityPolicy` 已能描述 actionability 检查需求；`ActionabilityCheckPlan` 已在 source 层表达 `force` 对 actionability 检查调度的影响；当前 `*_ex` 先复用 timeout，force 跳过 actionability 的统一执行语义尚未接入。

验收：

- 动态插入按钮后 click 能自动等待。
- disabled 按钮会等到 enabled。
- 遮挡按钮会超时或 force 点击。
- 多元素匹配触发 strict error。

### M3：Context 与仿真

- [~] cookie/storage state：已提供低层 `x.cdp.storage_get_cookies(conn, browser_context_id, timeout_ms)` / `storage_set_cookies(...)` / `storage_clear_cookies(...)`，映射 CDP `Storage.getCookies` / `Storage.setCookies` / `Storage.clearCookies`；已提供页面级 `storage_get_local_storage` / `storage_set_local_storage` / `storage_get_session_storage` / `storage_set_session_storage`，通过 `Runtime.evaluate` 导入导出当前页面 storage；这些能力已在 `x.browser.storage_*` facade 暴露；source facade 已提供轻量 `StorageState` 对象、`Page.storage_state` 和 `BrowserContext.storage_state`，用于承载 cookies/localStorage/sessionStorage raw JSON；已新增 `StorageOriginState` / `StorageStateBuilder` / `storage_origin_state` / `storage_state_builder`，用于承载跨 origin 聚合状态和最终 `StorageState` 生成；已新增 `StorageApplyPlan` / `storage_apply_plan` 承载 cookies/origins/localStorage/sessionStorage 的导入计划；已新增 `StorageSerializePlan` / `storage_serialize_plan`，承载 cookies/origins/localStorage/sessionStorage 的导出序列化状态；已新增 `ContextApplyPlan` / `context_apply_plan` 和 `ContextOptions.apply_plan`，用于统计 context defaults 与 storage state 应用计划；已新增 `ContextStorageStatePlan` / `context_storage_state_plan`，用于承载 context options 中 storageState 的解析和应用状态；统一 storage_state JSON 序列化、真实多 origin 遍历和 context options 默认应用尚未封装。
- [~] viewport：已提供低层 `x.cdp.page_set_viewport`，映射 `Emulation.setDeviceMetricsOverride`，并在 `x.browser.page_set_viewport` / `Page.set_viewport` 中暴露；`BrowserContext.set_viewport` 已保存默认 viewport，并可通过 `BrowserContext.apply_defaults` 显式应用到 session；`ContextDefaultApplyPlan` 已能表达新 session 上默认项应用进度；创建页面时完整 options 自动应用尚未封装。
- [~] user agent：已提供低层 `x.cdp.page_set_user_agent`，映射 `Emulation.setUserAgentOverride`，并在 `x.browser.page_set_user_agent` / `Page.set_user_agent` 中暴露；`BrowserContext.set_user_agent` 已保存默认 userAgent，并可通过 `BrowserContext.apply_defaults` 显式应用到 session；`ContextDefaultApplyPlan` 已能表达新 session 上默认项应用进度；创建页面时完整 options 自动应用尚未封装。
- [~] locale/timezone：已提供低层 `x.cdp.page_set_locale` / `x.cdp.page_set_timezone`，映射 `Emulation.setLocaleOverride` / `Emulation.setTimezoneOverride`，并在 `x.browser.page_set_locale` / `page_set_timezone` 和 `Page` 对象方法中暴露；`BrowserContext.set_locale` / `set_timezone` 已保存默认值，并可通过 `BrowserContext.apply_defaults` 显式应用到 session。
- [~] geolocation/permissions：已提供低层 `x.cdp.page_set_geolocation` / `browser_grant_permissions` / `browser_clear_permissions`，映射 `Emulation.setGeolocationOverride` / `Browser.grantPermissions` / `Browser.resetPermissions`，并在 `x.browser.page_set_geolocation` / `grant_permissions` / `clear_permissions` 和 `Page` / `BrowserContext` 对象方法中暴露；`BrowserContext.set_geolocation` 已保存默认 geolocation，并可通过 `BrowserContext.apply_defaults` 显式应用到 session；`BrowserContext.set_permissions` / `apply_permissions` 已保存并显式应用默认 permissions；`BrowserContext.new_page_object` 成功 attach 后会自动应用 geolocation/permissions。
- [~] offline/cache：已提供低层 `x.cdp.page_set_offline` / `network_set_cache_disabled`，映射 `Network.emulateNetworkConditions` 和 `Network.setCacheDisabled`；`page_set_offline` / `network_set_cache_disabled` 已在 `x.browser` facade 和 `Page` 对象方法暴露，`BrowserContext.set_offline` / `set_cache_disabled` 已保存默认值，并可通过 `BrowserContext.apply_defaults` 显式应用到 session；`ContextDefaultApplyPlan` 已能表达新 session 上默认项应用进度；创建页面时完整 options 自动应用尚未封装。

验收：

- 两个 context cookie 隔离。
- storage state 可导出再导入。
- viewport 生效并影响 screenshot。

### M4：网络与 route

- [~] request/response 事件：已提供低层 `x.cdp.network_enable` / `wait_network_event`，可打开 `Network` domain 并让底层事件队列接收 request/response 事件，且已在 `x.browser.network_enable` / `wait_network_event` 和 `Page.network_enable` / `wait_network_event` 暴露；已提供轻量 `Request` / `Response` 对象包装保存 raw event；已新增 `RequestResponsePairPlan` / `request_response_pair_plan`，承载 requestId、request/response raw event、body、paired/bodyReceived/failed 状态；page 级事件列表和真实 request/response 聚合尚未封装。
- [~] wait_request / wait_response：已提供低层 `x.cdp.wait_request` / `x.cdp.wait_response`，支持按 URL 子串等待 `Network.requestWillBeSent` / `Network.responseReceived`，且已在 `x.browser.wait_request` / `wait_response` 暴露；已提供 `Page.wrap_request` / `Page.wrap_response` / `x.browser.wrap_request` / `x.browser.wrap_response` 轻量对象入口；source facade 已提供轻量 `NetworkPattern` 和 `glob` / `regex` / `network_pattern`，`Page.wait_request_pattern` / `wait_response_pattern` 当前复用底层 substring 等待；已新增 `NetworkWaitPlan` / `network_wait_plan`，承载 wait kind、pattern、timeout、最近匹配、timeout 和 cancel 状态；真实 glob/regex 匹配和自动从 event JSON 解析 requestId 尚未封装。
- [~] response body：已提供低层 `x.cdp.network_get_response_body`，封装 `Network.getResponseBody` 并返回包含 `body` / `base64Encoded` 的 result JSON，且已在 `x.browser.response_body` 暴露；已提供 `Response.body()` 对象方法，使用显式 `request_id` 查询 body；source facade 已提供轻量 `RequestId` / `make_request_id` 和 `Page.response_body_id` / `route_apply_id`；已新增 `RequestIdParsePlan` / `request_id_parse_plan`，承载 response/request event JSON、requestId、parsed/failed 状态；已新增 `RequestResponsePairPlan` / `request_response_pair_plan`，显式承载 response body 归属和完成状态；自动从 response event 提取 requestId 尚未封装。
- [~] Fetch route：已提供低层 `x.cdp.fetch_enable` / `x.cdp.fetch_disable`，封装 `Fetch.enable` / `Fetch.disable` 单 pattern 拦截，且已在 `x.browser.route_enable` / `route_disable` 暴露；source facade 已提供轻量 `RoutePlan`，保存 pattern/action JSON 并支持显式 enable/apply；已新增轻量 `RouteRegistry` / `route_registry`，用于记录 route 数量、最后 pattern/action 和启用状态；已新增 `RouteMatchPlan` / `route_match_plan` / `route_match_pattern`，承载 glob/regex/substr kind、pattern、action、priority 和 enabled 状态；已新增 `RouteHandlerRegistryPlan` / `route_handler_registry_plan`，承载 handler 数量、active 数量、最高优先级、最后 pattern/action 和 cancelled 状态；真实 registry 匹配、glob/regex matcher 和回调调度尚未封装。
- [~] abort/continue/fulfill：已提供低层 `x.cdp.fetch_abort` / `x.cdp.fetch_continue` / `x.cdp.fetch_continue_ex` / `x.cdp.fetch_fulfill` / `x.cdp.fetch_fulfill_text` / `x.cdp.fetch_fulfill_ex`，分别封装 `Fetch.failRequest` / `Fetch.continueRequest` / `Fetch.fulfillRequest`，其中 `fetch_fulfill_text` 使用 xrt Base64 编码文本 body 并可附加 `Content-Type`；已在 `x.browser.route_abort` / `route_continue` / `route_fulfill_text` / `route_apply` 暴露静态 route 操作；已提供轻量 `Route` 对象包装，承载显式 `request_id` / raw event 并提供 `apply` / `abort` / `continue_request` / `fulfill_text`；source facade 已提供轻量 `RouteAction` 和 `route_abort_action` / `route_continue_action` / `route_fulfill_text_action` builder；已新增 `RouteActionPlan` / `route_action_plan`，承载 requestId、action name/json、prepared/applied 状态；自动从 event JSON 解析 requestId、route registry 和回调调度尚未封装。
- [~] headers/method/postData 修改：已提供低层 `x.cdp.fetch_continue_ex` / `fetch_fulfill_ex` / `network_set_extra_http_headers`，允许传入 CDP 原生 `url` / `method` / `postData` / request `headers` / `responseHeaders` / `body` JSON，并由实现强制覆盖 `requestId`；已提供 `x.cdp.route_apply` 解析静态 `actionJson` 并分派到 continue/abort/fulfill；`network_set_extra_http_headers` 已在 `x.browser` facade 和 `Page.network_set_extra_http_headers` 暴露；source facade 已提供轻量 `HeaderSet` / `headers` 和 `Page.network_set_extra_headers`；已新增轻量 `HeaderBuilder` / `header_builder`，承载 header 数量和最后一项并可生成 `HeaderSet`；已新增 `HeaderApplyPlan` / `header_apply_plan`，承载 request/response header target、count、JSON 和 applied 状态；`RouteAction` 已提供基础 action JSON builder；真实 route registry 匹配和完整 header JSON 组装尚未封装。

验收：

- mock API 返回固定 JSON。
- abort 图片请求。
- wait_response 可按 glob 等待目标请求。

### M5：trace 和 assert

- [~] action log：已提供低层 `x.cdp.trace_write_action` / `x.cdp.trace_write_event`，会校验 JSON 为对象、重新序列化为单行 JSONL 并追加写入调用方指定的 `trace.jsonl` / `network.jsonl` / `console.jsonl` / `errors.jsonl`；`x.browser.trace` 已提供轻量 `TraceSession`，封装 `trace.jsonl` / `network.jsonl` / `console.jsonl` / `errors.jsonl` / `manifest.json` / `failure.*` 的目录路径和写入方法；`TraceSession` 已维护最近一条 action 和 action 计数；已新增轻量 `TraceActionWindow` / `action_window`，承载容量、计数、丢弃数、最近 action 和前一条 action；完整最后 50 条 action 环形列表尚未封装。
- [~] 失败截图：已提供低层 `x.cdp.page_failure_artifacts` / `x.cdp.trace_record_failure`，可按调用方路径一次性记录失败 action、采集失败 screenshot 和 DOM snapshot、写入 manifest；`TraceSession.record_failure` 已封装默认 failure artifact 路径；`x.browser.assert` 已提供 `*_trace` 断言便捷入口，在断言失败时同步触发 `trace_record_failure`；`x.browser.trace` 已新增 `FailureArtifactPlan` / `failure_artifact_plan`，承载 action、screenshot、snapshot、manifest 路径和 recorded 状态；trace 目录命名和更完整的错误对象尚未封装。
- [~] DOM snapshot：已提供低层 `x.cdp.page_snapshot`，复用 `page_content` 获取 `documentElement.outerHTML` 并保存为 HTML 附件；`FailureArtifactPlan` 已承载默认 snapshot 路径；已新增 `DomSnapshotPlan` / `dom_snapshot_plan`，承载 trace 目录、snapshot 路径、HTML byte size 和 recorded 状态；尚未接入 trace 目录和失败断言自动采集。
- [~] network log：已提供低层 `x.cdp.wait_network_event`，可按 session 消费下一条 `Network.*` 事件，并可用 `x.cdp.trace_write_event` 追加写入 `network.jsonl`；已在 `x.browser.wait_network_event` 和 `Page.wait_network_event` 暴露；source facade 已提供轻量 `PageEventLog` 最近事件承载对象，并记录 network/console/error 分类计数和总计数；已新增轻量 `PageEventWindow` / `event_window`，承载 page 级事件窗口容量、事件总数、丢弃数、分类计数、最近 kind 和最近事件；已新增 `EventDispatchPlan` / `event_dispatch_plan`，承载 session 级事件入队、分发、丢弃和取消状态；已新增轻量 `HarEntry` / `har_entry` 和 `HarLog` / `har_log`，承载 HAR-like entry 数量、最后 request/status 和 raw request/response；已新增 `HarSerializePlan` / `har_serialize_plan`，承载 HAR entry 数量、输出路径、content 包含策略和 serialized/failed 状态；`x.browser.trace` 已新增 `TraceEventPlan` / `event_plan`，承载 trace 目录、network/console/error 分类计数和最近事件；真实 page 级环形列表和完整 HAR JSON 写入尚未封装。
- [~] console/error log：已提供低层 `x.cdp.runtime_enable` / `x.cdp.log_enable`，以及 `x.cdp.wait_console` / `x.cdp.wait_exception` / `x.cdp.wait_log_entry` 事件等待，并可用 `x.cdp.trace_write_event` 追加写入 `console.jsonl` / `errors.jsonl`；已在 `x.browser` facade 和 `Page.runtime_enable/log_enable/wait_console/wait_exception/wait_log_entry` 暴露；source facade 已提供轻量 `PageEventLog` 最近 console/error 事件承载对象，并记录分类计数；已新增 `PageEventWindow` / `event_window` 承载 console/error 事件窗口计数和容量/丢弃状态；`x.browser.trace` 已新增 `TraceEventPlan` / `event_plan`，承载 console/error trace 计数和最近事件；真实 page 级 console/error 环形列表尚未封装。
- [~] trace zip：已提供低层 `x.cdp.trace_default_manifest` / `x.cdp.trace_write_manifest`，可生成并写入 trace 目录中的 `trace.jsonl` / `network.jsonl` / `console.jsonl` / `errors.jsonl` / snapshots / screenshots 清单 manifest JSON；`TraceSession.default_manifest` / `write_manifest` 已封装默认 manifest 位置；source facade 已提供轻量 `TraceArchive` / `archive`，承载 trace 目录、manifest 和目标 zip 路径；已新增轻量 `TraceViewerPlan` / `viewer_plan` 和 `TraceArchive.viewer` 字段，承载 viewer 输出计划；已新增 `TraceZipPlan` / `zip_plan`，承载 traceDir、manifest、zip、viewer、entry count 和 packed 状态；真实 zip 打包和 trace viewer 生成尚未实现。
- [~] expect visible/text/count/url/title/ok：已提供低层 `x.cdp.expect_visible` / `x.cdp.expect_text` / `x.cdp.expect_count` / `x.cdp.expect_url` / `x.cdp.expect_title` / `x.cdp.expect_ok`，其中页面断言会在 timeout 内轮询 bool 判断；`x.browser.assert` 已提供基础断言和 `title_trace` / `url_trace` / `visible_trace` / `text_trace` / `count_trace` 失败采集入口；source facade 已提供轻量 `AssertResult` / `result` 承载 passed/message/expected/actual/traceDir；已新增轻量 `AssertionErrorInfo` / `error_info`，承载 kind/selector/expected/actual/traceDir/timeout；已新增 `AssertionRetryPlan` / `retry_plan`，承载 expect 轮询 timeout、interval、attempt 和最近失败信息；已新增 `AssertionTracePlan` / `trace_plan`，承载断言 kind、selector、expected/actual、traceDir 和 artifact recorded 状态；自动从断言失败路径生成完整结构化错误对象尚未封装。

验收：

- 断言失败时生成 trace。
- trace 中能定位失败 action、selector、URL 和截图。

### M6：高级能力

- [~] frame locator：已提供低层 `x.cdp.page_frame_tree(conn, session_id, timeout_ms)`，返回 CDP `Page.getFrameTree` 的 result JSON，作为 frame 定位和 iframe 绑定的基础；已在 `x.browser.page_frame_tree` 和 `Page.frame_tree` 暴露；已提供轻量 `Frame` / `FrameLocator` 对象包装，承载 raw frame tree / selector 并可创建继承默认 timeout 的 `Locator`；已新增 `FrameBindingPlan` / `frame_binding_plan`，承载 frameId、selector、executionContext、initScriptCount 和 bound 状态；已新增 `FrameLocatorPlan` / `frame_locator_plan`，承载 frame selector、内部 locator selector、timeout、resolved/bound/failed 状态；跨 frame locator 查询、frame 级 execution context 真实绑定和自动补注入尚未实现。
- [~] dialog：已提供低层 `x.cdp.wait_dialog` / `dialog_accept` / `dialog_dismiss` / `dialog_accept_text`，分别映射 `Page.javascriptDialogOpening` 和 `Page.handleJavaScriptDialog`，且已在 `x.browser` facade 暴露；已提供轻量 `Dialog` 对象包装，保存 raw event 并提供 `accept` / `dismiss` / `accept_with_text`；source facade 已提供 `wrap_dialog_info` 和 `type/message/defaultValue` 显式字段访问器，用于承载调用方已解析事件字段；已新增轻量 `DialogInfo` / `dialog_info`，承载 type/message/defaultValue 以及 accepted/dismissed 状态；已新增 `DialogActionPlan` / `dialog_action_plan`，承载 accept/dismiss/prompt text/timeout/handled 意图；自动 JSON 字段解析和 page 级回调尚未封装。
- [~] downloads：已提供低层 `x.cdp.browser_set_download_behavior` / `wait_download` / `wait_download_progress`，映射 `Browser.setDownloadBehavior` 和 Browser download 事件，且已在 `x.browser` facade 暴露；已提供轻量 `Download` 对象包装，保存 raw event 并提供 `progress`；source facade 已提供 `wrap_download_info` 和 suggested filename/path/save_as/state 显式字段访问器；已新增轻量 `DownloadInfo` / `download_info`，承载 filename/path/save_as/state/received/total 并提供 completed 判定；已新增 `DownloadSavePlan` / `download_save_plan`，承载 suggested filename、save-as path、progress、completed/failed 状态；自动 JSON 字段解析和真实下载完成状态聚合尚未封装。
- [~] pdf：已提供低层 `x.cdp.page_pdf(conn, session_id, path, timeout_ms)`，通过 CDP `Page.printToPDF` 保存 PDF，且已在 `x.browser.page_pdf` 暴露；已提供轻量 `PdfOptions` 和 `Page.pdf_ex` source 层入口，当前仅复用 path 调用底层 PDF；`PdfOptions` 已能承载 landscape/printBackground、paper size 和 margins，并提供 `pdf_options_paper` builder；已新增 `PdfRenderPlan` / `pdf_render_plan`，承载 path、已启用 option 数、render requested/completed 状态；已新增 `PdfOptionsApplyPlan` / `pdf_options_apply_plan`，承载 paper/margin/landscape/printBackground options 应用计划和 applied/failed 状态；这些真实 PDF options 尚未接入底层。
- [~] accessibility snapshot：已提供低层 `x.cdp.accessibility_snapshot(conn, session_id, timeout_ms)`，返回 CDP `Accessibility.getFullAXTree` 的 result JSON，且已在 `x.browser.accessibility_snapshot` / `Page.accessibility_snapshot` 暴露；已提供轻量 `AccessibilityOptions` 和 `Page.accessibility_snapshot_ex` source 层入口，当前仍复用底层完整 AX tree；已新增 `AccessibilityOptionsApplyPlan` / `accessibility_options_apply_plan`，承载 interestingOnly/root filter options 应用计划和 snapshot requested/applied/failed 状态；已新增轻量 `AccessibilitySnapshotPlan` / `accessibility_snapshot_plan`，承载 interestingOnly/root/includeIgnored/maxDepth 过滤计划；已新增 `AccessibilitySerializePlan` / `accessibility_serialize_plan`，承载 root filter、node count、maxDepth、includeIgnored 和 serialized 状态；这些过滤和序列化选项尚未接入底层。
- [~] video/screencast：已提供低层 `x.cdp.page_start_screencast` / `page_stop_screencast` / `wait_screencast_frame` / `screencast_frame_ack`，映射 CDP screencast 帧流，且已在 `x.browser` facade 暴露；已提供 `Page.start_screencast` / `stop_screencast` / `wait_screencast_frame` / `screencast_frame_ack` 和轻量 `ScreencastFrame` 对象包装；source facade 已提供轻量 `ScreencastSession`，承载录制参数、最近帧和帧计数；已新增 `ScreencastLifecyclePlan` / `screencast_lifecycle_plan`，承载 start/stop/frame/failed 生命周期状态；已新增轻量 `VideoRecordPlan` / `video_record_plan`，承载输出路径、帧参数和 start/stop 状态；已新增 `VideoTracePlan` / `video_trace_plan`，承载 traceDir、输出路径、帧数、manifest 写入和 trace 附加状态；视频文件编码和完整高层录制生命周期尚未封装。
- [~] dynamic route handle：已提供低层 `x.cdp.wait_route(conn, session_id, url_part, timeout_ms)`，可等待 `Fetch.requestPaused` 并复用既有 `fetch_continue(_ex)` / `fetch_abort` / `fetch_fulfill(_ex)` 处理 requestId，且已在 `x.browser.wait_route` 暴露；已提供 `Page.wrap_route` / `x.browser.wrap_route` 轻量对象入口，`RoutePlan.apply` 可对显式 `Route` 执行静态 action JSON；source facade 已提供轻量 `RouteRegistry` 状态承载对象，并新增 `RouteMatchPlan` 承载 handler 匹配计划和优先级，新增 `RouteHandlerRegistryPlan` / `route_handler_registry_plan` 承载 handler registry 的 source 层状态，新增 `RouteDispatchPlan` / `route_dispatch_plan` 承载轮询式 pending/handled/cancelled 派发状态；尚未封装真实 route handler registry、自动 requestId 提取、回调调度和并发取消语义。
- [~] callback/async 事件模型：V1 已通过 `x.cdp.next_event` / `wait_event` / `wait_*` 提供轮询和阻塞等待式事件消费，避免引入 native callback ABI 和额外竞态；source facade 已新增 `RouteHandlerRegistryPlan` 和 `RouteDispatchPlan`，用于表达高层 route 的 handler registry 与显式轮询派发状态而不启动 native callback、线程或锁；已新增 `CallbackRegistryPlan` / `callback_registry_plan`，承载 event kind/handler 注册、显式 begin dispatch、active/canceled 状态；尚未封装真实异步回调执行器。

## 24. 测试计划

### 24.1 单元测试

- xrt JSON 集成封装。
- xrt WebSocket transport 集成。
- CDP request id 分发。
- session event routing。
- glob/regex URL matching。
- selector AST parse。
- timeout deadline。

### 24.2 本地集成测试

已提供 `dev/extlibs/browser/testsite`：

- `index.html`：基础导航和标题。
- `dynamic.html`：延迟插入元素。
- `form.html`：label/placeholder/fill/check/select。
- `overlay.html`：遮挡和 receives-events。
- `network.html`：fetch API。
- `frames.html`：iframe。
- `download.html`：下载。
- `dialog.html`：alert/confirm/prompt。

### 24.3 回归用例命名

建议加入：

- `xcdp_smoke`
- `xcdp_page_smoke`
- `xbrowser_smoke`
- `xbrowser_assert_trace_smoke`
- `x_browser_launch_basic`
- `x_browser_page_goto_title`
- `x_browser_page_screenshot`
- `x_browser_locator_css_click`
- `x_browser_locator_text_click`
- `x_browser_locator_auto_wait_visible`
- `x_browser_locator_strict_error`
- `x_browser_context_cookie_isolation`
- `x_browser_network_wait_response`
- `x_browser_route_fulfill`
- `x_browser_trace_on_failure`

## 25. 风险与约束

### 25.1 协议变化

CDP tip-of-tree 不保证向后兼容。应：

- 启动时读取 `/json/protocol`。
- 保存 Browser / Protocol-Version。
- 对实验 domain 做 capability check。
- 把失败映射成明确错误。

### 25.2 WebSocket 与 TLS

本地 CDP 默认是 ws/http，无 TLS。远程连接若支持 wss/https，V1 可暂不承诺，V2 补。

### 25.3 事件回调

xlang native callback ABI 不是本扩展 V1 的前置条件。V1 用：

- `wait_event`
- `next_event`
- 静态 route action
- `wait_route` 动态轮询

### 25.4 Role locator 完整性

完整 ARIA accessible name 计算复杂。V1 覆盖常用元素和属性，V2 再对齐更多 WAI-ARIA 规则。

### 25.5 进程清理

异常退出时可能残留 Chrome 进程和临时 profile。需要：

- Browser 对象 destroy 时关闭子进程。
- 记录 pid。
- release 后清理临时目录。
- 提供 `keepUserDataDir` 调试开关。

## 26. API 稳定性规则

- `x.browser` 模块名稳定。
- `Browser` / `BrowserContext` / `Page` / `Locator` 类型名稳定。
- 简单 API 参数顺序稳定。
- `*_ex(optionsJson)` 可新增 option，不破坏旧字段。
- JSON 返回结构只能新增字段，不能重命名或改变含义。
- 错误 kind 名稳定。
- 低层 `x.cdp.send` 永久保留，方便覆盖高层未支持 CDP 能力。

## 27. V1 最小完成定义

V1 完成必须满足：

- 不依赖 Node.js / Playwright。
- 能 launch Chrome / Edge。
- 能 connect 已有 CDP endpoint。
- 能 new context / new page。
- 能 goto / evaluate / screenshot。
- 能用 css/text/testid locator click/fill。
- 有 visible/stable/enabled/receives-events 自动等待。
- 有基本 network request/response 监听。
- 有 route fulfill/abort。
- 有失败截图和 trace log。
- 有至少 10 个自动化回归用例。

满足这些后，xlang 就具备第一版可用于真实自动化脚本的浏览器控制能力。
