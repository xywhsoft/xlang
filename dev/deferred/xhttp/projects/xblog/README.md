# xblog - xlang 博客引擎

一个用 xlang 编写的综合博客引擎，覆盖 HTTP server + 模板 + 手写 Markdown 渲染 +
JSON 持久化 + REST API + 静态文件 全链路。

## 用法

```bat
cd /d D:\GIT\x-lang\release

:: self-test：启动 server、自发自收 17 个 HTTP 测试用例 + 24 项 Markdown 单元测试，
:: 跑完恢复 posts.json 后退出。两条验证路径都必须 EXIT=0。
xl run projects\dev\xblog\xblog.xl
xl build projects\dev\xblog\xblog.xl -o projects\dev\xblog\xblog.exe

:: 常驻 serve 模式（默认 18200 端口）
xl run projects\dev\xblog\xblog.xl -- serve
xl run projects\dev\xblog\xblog.xl -- serve 8080
```

## 路由

| 方法   | 路径                    | 说明                                   |
|--------|-------------------------|----------------------------------------|
| GET    | `/`                     | 首页（文章列表，HTML）                 |
| GET    | `/post/{slug}`          | 文章详情（模板 + Markdown 渲染）       |
| GET    | `/archive`              | 归档页                                 |
| GET    | `/search?q=xxx`         | 搜索（标题 + 正文）                    |
| GET    | `/api/posts`            | JSON API：所有文章                     |
| GET    | `/api/posts/{slug}`     | JSON API：单篇文章                     |
| POST   | `/api/posts`            | 新建文章（接收 JSON body）→ 201        |
| PUT    | `/api/posts/{slug}`     | 编辑文章                               |
| DELETE | `/api/posts/{slug}`     | 删除文章                               |
| GET    | `/static/*`             | 静态文件（CSS 等，xweb 自动 ETag/Range）|

## 文章数据格式

`posts.json`，compact JSON 保序：

```json
{"posts":[
  {"slug":"hello-world","title":"Hello World","date":"2026-07-01",
   "tags":["intro","welcome"],"content":"# Hello World\n\n..."}
]}
```

## Markdown 渲染器（手写）

`renderMarkdown(md) -> html` 实现以下块级和行内元素：

- 标题 `# / ## / ###` ... `######` → `<h1>`..`<h6>`
- 段落（连续非空行合并）→ `<p>...</p>`
- 粗体 `**bold**` → `<strong>`
- 斜体 `*italic*` → `<em>`
- 行内代码 `` `code` `` → `<code>`
- 代码块 ` ``` ... ``` ` → `<pre><code>`（内部不做行内变换，只 HTML 转义）
- 链接 `\[text\]\(url\)` → `<a href="url">text</a>`
- 图片 `!\[alt\]\(url\)` → `<img src="url" alt="alt">`
- 无序列表 `- item` / `* item` → `<ul><li>`
- 有序列表 `1. item` → `<ol><li>`
- 分隔线 `---` / `***` / `___` → `<hr>`
- 所有非代码文本先做 HTML 转义（`&` `<` `>`），防止 XSS

## 目录结构

```
xblog/
    xblog.xl            主程序（~870 行）
    posts.json          文章数据（2 篇种子文章）
    README.md           本文件
    templates/          HTML 模板（{$var} / {#if} / {#foreach}）
        list.html
        post.html
        archive.html
        search.html
    static/
        style.css
```

## 验证结果

self-test 输出（`xl run` 与 `xl build` 后的 exe 均一致）：

```
================ markdown unit tests ================
[ PASS ] md.h1 / md.h2 / md.bold / md.italic
[ PASS ] md.inline-code / md.code-block-open / md.code-block-content-preserved / md.code-block-close
[ PASS ] md.ul-open / md.ul-item1 / md.ul-item3 / md.ul-close
[ PASS ] md.ol-open / md.ol-item1 / md.ol-close
[ PASS ] md.link / md.image
[ PASS ] md.para1 / md.para2 / md.hr / md.para-after-hr
[ PASS ] md.escape-lt-gt / md.escape-amp / md.no-raw-script

================ xblog http self-test ================
[ PASS ] 1..17 共 44 项 HTTP 行为校验

------------------- summary -------------------
passed: 68 / 68
failed: 0
```

## 状态与持久化

- `gPosts` 在启动时从磁盘加载一次，不在每个请求中重复读取和解析文件。
- `rwlock` 保护文章缓存；读请求创建浅快照后释放读锁，模板渲染和搜索在锁外完成。
- 新建、编辑和删除在写锁内同步更新缓存与 `posts.json`，避免多个 worker 互相覆盖。
- 持久化 JSON 使用统一转义函数，API 错误响应也会正确转义 slug、路径和错误文本。
