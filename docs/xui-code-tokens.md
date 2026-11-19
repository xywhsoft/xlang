# Independent versioned token cache

`createCodeTokenBuffer()` owns the native numeric cache. It retains no document,
UI context, native child or callback. Generic creator-thread admission, active
call deferral and expired receiver checks provide the same independent leaf
lifetime as `CodeFoldState`. Close in `finally`.

| Language method | Native entry |
| --- | --- |
| `set(tokens,size,version)` | `xuiCodeTokenBufferSet` |
| `setRange(tokens,size,version,start,end)` | `xuiCodeTokenBufferSetRange` |
| `getVersion()` | `xuiCodeTokenBufferGetVersion` |
| `getRange()` | `xuiCodeTokenBufferGetRange` |
| `getCount()` | `xuiCodeTokenBufferGetCount` |
| `clear()` | `xuiCodeTokenBufferClear` |

The factory and owner destruction call native Create/Destroy. Native C still
offers the caller-buffer operations `GetTokens` and `GetTokensInRange`. The
owned captures below expose the same selections in xlang.

Write parameters preserve native array/count/version/range order. The existing
array projection contract requires count immediately after its column. Use
`size=-1` for all records; a nonnegative size selects at most the available
prefix, including zero, and another negative value throws before C entry.
There is no new argument reordering rule, forwarding function or dispatcher.
Each `CodeToken` has five C integers. Byte start/end, style, kind and flags are
copied to one contiguous temporary native array; the native cache keeps its own
copy. Changing the input afterwards does not change the cache.

Token ranges use UTF-8 byte offsets, not character indices. End must exceed
start; tokens must be fully inside the declared cache interval. Native ordering
requires nondecreasing starts and ends. Version is an unsigned 32-bit value,
including UINT32_MAX. A full set records interval [0,INT_MAX); a range set records
its explicit interval. Clear resets count, version and interval to zero and
retains allocated token capacity for reuse. Validation/OOM preserves previously
published records and metadata. The checked tuple return contains version,
start and end; its fields keep their exact C scalar types.

The [lesson](../lib/bindings/xui/examples/code_tokens.xl) records one keyword,
checks and prints cache count/version/byte range, then closes the owner. It
requires no window, font, document, asset or service. Separate fixtures exercise
invalid prefixes/order/ranges, full unsigned versions, copied storage, native
payload retrieval, allocation prefixes, callable values, close during an active
foreign call, wrong-thread use and expiry. Native O0/O2 clients link the actual
hosted/static SDK; they do not include another copy of token-buffer source.

Prepared count/version/range reads are compared with the same SDK's native C.
The measurement separately reports language/native create-close and warmed
input publication. Array projection retains O(n) validation/copy and one
20-byte-per-record temporary allocation, plus native reserve/copy work. Managed
owner allocation and CodeLease bookkeeping, checked scopes, thread admission, native
calls, tuple transport and callable creation/indirection still have costs.
Cold compiler/package work and CPU-time equality are not measured.

Exact runner, sources, SDK reuse, heap counters and unreferenced-declaration
size comparison are indexed in
`dev/release-materials/provenance/xui-code-tokens-20261114.json` for the
original standalone-cache batch.

Measured prepared metadata reads make 3000 native calls per language/C sample
with zero XRT alloc/free/realloc; the language loop succeeds under allocation
budget zero. One create/close pair is 3/3/0 versus native 1/1/0, and one warmed
two-record set is 1/1/0 versus native 0/0/0. These are heap observations at the
same native revision and workload, not a zero-overhead or CPU-time claim.

Windows and Linux pass 60 combined native/language/lesson build, run, header
and import steps, plus 16 static reachability comparison steps. The comparison
uses the same SDK/compiler and CodeModels lesson; only the unused token-cache
manifest entry is removed. File, text and BSS deltas are zero on both platforms.
The native SDK and compiler bytes are reused unchanged; this is not a rebuild
or a whole-ecosystem completion claim.

## CodeEdit 的 token 前景样式

`CodeEdit.setTokenStyleResolver()` 安装可见 token 的前景样式回调，传 `null`
清除。`hasTokenStyleResolver()` 查询当前是否装有处理器。原生回调地址和 userdata
可继续通过 C 的 `xuiCodeEditGetTokenStyleResolver()` 查询。

处理器可以读取文档和 token 属性，并通过 `choice.value` 写入编辑器主题中的样式 ID。
返回 `XUI_OK` 且目标样式有可见前景时使用该颜色。返回其他状态、样式不存在或
前景透明时，使用词法主题和 CSS 的颜色。`startOffset` 和 `endOffset` 是整份文档的
UTF-8 字节偏移，换行、软折行和裁剪均保持完整 token 的偏移。

文档、token 和输出槽在回调结束后失效，访问会抛出 `StateError`。可保留按值读取的
偏移、类别、样式、标志和字符串。回调的原生调用作用域只准入一次。回调体再次请求同一
Context 的调用作用域会报告 Busy，包括编辑文档、关闭编辑器和重新渲染。回调抛出的异常由 `Context.render()`
交付。移除或替换处理器后可继续绘制。

原生缓存渲染对处理器只持有一次活动引用，再逐个解析可见 token。C 回归确认设置
颜色不会重建字形或改写词法缓存。xlang 每次回调仍建立有效期检查，读取
`document.getText()` 会复制整份源码，适合在回调外准备需反复使用的文档数据。

同一静态 AOT 程序采用 CPU 测试后端，交替执行手写 C 和 xlang 两个处理器。
每种处理器取七轮中位数，每轮重绘 60 帧并调用处理器 7,200 次。按调用次数折算
整轮的渲染耗时，Windows 为 C 0.91 µs、xlang 5.81 µs，Linux 为 C 0.91 µs、
xlang 2.34 µs。每轮两条路径均有 180 次分配、180 次释放和 0 次重分配。
测量包含缓存重绘和回调，创建及注册发生在计时区间之前。

运行 [CodeEdit 前景样式范例](../lib/bindings/xui/examples/code_token_style.xl)，
可以看到 `42` 和 `3` 使用关键字的前景色。
[验收记录](../dev/release-materials/provenance/xui-codeedit-token-style-20261119.json)。

## 实时缓存与只读快照

`editor.tokenBuffer()` 取得编辑器实际使用的缓存。用
`editor.documentVersion()` 获取发布所需的当前版本。版本过期或范围超出
文档字节长度时，发布会抛出 checked 错误并保留旧缓存。编辑器优先使用覆盖
当前渲染范围的同版本缓存，缺少覆盖时按已选语言重新生成。关闭缓存视图会
保留编辑器里的缓存，关闭编辑器会使视图失效。

`capture(version)` 读取已缓存的全部 token。
`captureInRange(version, start, endOffset)` 选择与半开字节范围相交的
token，保留原始位置。请求的版本和范围必须已缓存。空区间返回空序列。
快照的 `getRange()` 返回版本和此次选取的字节范围。

在快照上调用 `tokens()` 取得 `NativeSequence<CodeToken>`。序列保留
快照的原生引用，关闭快照、清空缓存或关闭编辑器后仍可遍历。捕获和切片
共享同一块只读记录。后续非空发布如遇已共享存储，只将本次输入写入新块，
原快照保留旧块。缓存独占且容量足够时直接复用。新的原生快照分配一个小
描述符，xlang 获取序列时还会登记拥有关系。

Windows 和 Linux 的原生测试均确认，预热后 1,000 次同容量发布产生
0 次堆分配，1,000 次区间捕获产生 1,000 次描述符分配和释放。xlang 在
零分配预算下完成 1,000 轮已取得序列的遍历。快照、发布和遍历分开计量。

[编辑器 token 范例](../lib/bindings/xui/examples/code_editor_tokens.xl) ·
[快照验收记录](../dev/release-materials/provenance/xui-codeedit-token-snapshot-20261120.json)
