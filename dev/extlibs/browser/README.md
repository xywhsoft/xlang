# x.browser CDP 浏览器自动化扩展

文档状态（2026-05-12）：
本目录是官方扩展库设计区，当前记录的是 `x.browser` 的 V1 设计与验收口径，不代表 demo5 核心语言已经内置浏览器自动化能力。

本目录记录 xlang 官方浏览器自动化扩展的设计定稿。

目标是基于 Chrome DevTools Protocol 实现 xlang 自己的 Chromium / Edge 浏览器控制能力，形成接近 Playwright 使用体验的自动化 API，但不依赖 Node.js、npm 或 Playwright 运行时。

文档：

- [x.browser-cdp-v1-spec.md](x.browser-cdp-v1-spec.md)：V1 设计规格、API、实现分期和验收标准。
任务跟踪直接写在 `x.browser-cdp-v1-spec.md` 的实现分期条目中。

强制规则：

- CDP WebSocket、JSON、HTTP bootstrap、浏览器进程启动必须使用 xrt / xlang 标准库能力。
- 禁止在 browser 扩展中重复实现 xrt 已有能力。
- 如果 xrt 存在 BUG 或能力缺口，先修复 xrt，重新编译 xlang，再继续 browser 扩展开发。

外部协议参考：

- Chrome DevTools Protocol: https://chromedevtools.github.io/devtools-protocol/
- Playwright auto-waiting: https://playwright.dev/docs/actionability
- Playwright locators: https://playwright.dev/docs/locators
- Playwright network: https://playwright.dev/docs/network
- Playwright browser contexts: https://playwright.dev/docs/browser-contexts
- Chrome remote debugging 安全变更: https://developer.chrome.com/blog/remote-debugging-port
