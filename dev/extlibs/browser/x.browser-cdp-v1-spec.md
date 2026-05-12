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

- 默认创建独立临时 `userDataDir`。
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
- [~] request/response/event 分发：已支持同步 command response、event queue、pending response 保留；尚未实现完整取消和 session/page 分队列。
- [~] Target attach/session 管理：已提供 `send_session` 原始出口；尚未封装 target 生命周期。
- [X] `cdp.send` / `cdp.send_session` / `cdp.next_event`。
- [X] 基础 smoke：`dev/extlibs/browser/xcdp_smoke.xl`。

验收：

- [ ] 能连接手工启动的 Chrome。
- [ ] 能调用 `Browser.getVersion`。
- [ ] 能创建 target 并收到 page 事件。

### M1：Browser / Page 基础

- [ ] `browser.launch`。
- [ ] 独立 userDataDir。
- [ ] `Browser.new_context`。
- [ ] `Context.new_page`。
- [ ] `Page.goto`。
- [ ] `Page.title/url/content`。
- [ ] `Page.evaluate`。
- [ ] `Page.screenshot`。
- [ ] `Page.close` / `Browser.close`。

验收：

- 打开 example.com，读取标题，截图保存。
- headless/headful 均可运行。
- 失败时输出 last_error。

### M2：Locator 和动作

- [ ] injected script。
- [ ] css/xpath/text/testid selector。
- [ ] strict mode。
- [ ] visible/stable/enabled/editable/receives-events。
- [ ] click/fill/press/type/hover/check/select。
- [ ] timeout 和 force。

验收：

- 动态插入按钮后 click 能自动等待。
- disabled 按钮会等到 enabled。
- 遮挡按钮会超时或 force 点击。
- 多元素匹配触发 strict error。

### M3：Context 与仿真

- [ ] cookie/storage state。
- [ ] viewport。
- [ ] user agent。
- [ ] locale/timezone。
- [ ] geolocation/permissions。
- [ ] offline/cache。

验收：

- 两个 context cookie 隔离。
- storage state 可导出再导入。
- viewport 生效并影响 screenshot。

### M4：网络与 route

- [ ] request/response 事件。
- [ ] wait_request / wait_response。
- [ ] response body。
- [ ] Fetch route。
- [ ] abort/continue/fulfill。
- [ ] headers/method/postData 修改。

验收：

- mock API 返回固定 JSON。
- abort 图片请求。
- wait_response 可按 glob 等待目标请求。

### M5：trace 和 assert

- [ ] action log。
- [ ] 失败截图。
- [ ] DOM snapshot。
- [ ] network log。
- [ ] console/error log。
- [ ] trace zip。
- [ ] expect visible/text/count/url/title/ok。

验收：

- 断言失败时生成 trace。
- trace 中能定位失败 action、selector、URL 和截图。

### M6：高级能力

- [ ] frame locator。
- [ ] dialog。
- [ ] downloads。
- [ ] pdf。
- [ ] accessibility snapshot。
- [ ] video/screencast。
- [ ] dynamic route handle。
- [ ] callback/async 事件模型。

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

提供 `dev/extlibs/browser/testsite` 或 `demo5/release/examples/browser_site`：

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
