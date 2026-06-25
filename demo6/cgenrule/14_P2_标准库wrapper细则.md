# P2 标准库 wrapper 细则

本文细化当前 cgen 中标准库 wrapper 的 C 生成规则。这里的 xlang 写法按用户可见标准库写法描述；实际触发点是标准库声明层生成或引用的 `__xl_std_runtime_*` 内部调用名，needs 扫描命中后由 `xl6CgenEmit*Runtime` 注入对应 helper。

## CGEN-STD-001 标准库 wrapper 注入总规则

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `crypto.sha256(s)`、`url.host(s)`、`process.exec(cmd)` 等 |
| 生成规则描述 | 标准库 wrapper 不总是输出，只有 needs 扫描发现对应 `__xl_std_runtime_*` 调用时才注入。每个 wrapper 文件用 `#ifndef XL6_*_RUNTIME` 包裹，避免同一 C 输出中重复定义。 |
| 生成器文件名和行号 | `xl6_cgen_module.c:44`、`xl6_cgen_module.c:104`、`xl6_cgen_crypto.c:7`、`xl6_cgen_url.c:7` |
| 对应生成的 C 代码 | `#ifndef XL6_CRYPTO_RUNTIME ... void* xl6_xrt_sha256_hex(...); #endif` |
| 中间形态伪代码 | `if hasCryptoRuntime: emitCryptoRuntime()` |
| 实现同等功能手写的 C 代码 | 手写 C 需要自己包含 XRT 原型并实现空值、分配和错误路径。 |
| 额外依赖的封装代码 | 各命名空间 runtime helper。 |

## CGEN-STD-002 crypto SHA HEX wrapper

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `crypto.sha1(text)` / `crypto.sha256(text)` / `crypto.sha384(text)` / `crypto.sha512(text)` |
| 生成规则描述 | crypto wrapper 声明 XRT SHA 函数和 `xrtHexEncode`，对 null 文本转为空字符串，按摘要位数在栈上分配 digest，再返回 XRT 分配的 HEX 字符串。 |
| 生成器文件名和行号 | `xl6_cgen_crypto.c:9`、`xl6_cgen_crypto.c:20`、`xl6_cgen_crypto.c:27`、`xl6_cgen_crypto.c:34`、`xl6_cgen_crypto.c:41` |
| 对应生成的 C 代码 | `xrtSHA256(textOrEmpty, size, digest); return xrtHexEncode(digest, sizeof(digest));` |
| 中间形态伪代码 | `shaN(text) -> digest[N/8] -> hex string` |
| 实现同等功能手写的 C 代码 | `unsigned char d[32]; xrtSHA256(s,n,d); return xrtHexEncode(d,32);` |
| 额外依赖的封装代码 | `xl6_crypto_text_or_empty`。 |

## CGEN-STD-003 url part wrapper

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `url.scheme(text)`、`url.host(text)`、`url.path(text)`、`url.query(text)` |
| 生成规则描述 | URL part wrapper 调用 `xrtUrlParseViewN` 得到 `xrturlview`，再按 part 编号选择 scheme/authority/userInfo/host/path/query/fragment/target，最后复制成 XRT 分配的 C 字符串。解析失败返回空字符串。 |
| 生成器文件名和行号 | `xl6_cgen_url.c:13`、`xl6_cgen_url.c:47`、`xl6_cgen_url.c:57` |
| 对应生成的 C 代码 | `view = url.tHost; return xl6_url_copy_text(view.sPtr, view.iLen);` |
| 中间形态伪代码 | `parse(url); switch(part); copy(view)` |
| 实现同等功能手写的 C 代码 | 手写 C 需要调用 `xrtUrlParseViewN` 并复制 view。 |
| 额外依赖的封装代码 | `xrturlview` 本地 typedef、`xl6_url_empty_string`、`xl6_url_copy_text`。 |

## CGEN-STD-004 url flag 与 port wrapper

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `url.valid(text)` / `url.isAbsolute(text)` / `url.isSecure(text)` / `url.port(text)` |
| 生成规则描述 | `url_flag` 解析 URL 后按 flag 编号返回 valid/absolute/secure；flag 为 0 表示只检查解析是否成功。`url_port` 解析失败返回 0，成功返回 `iPort`。 |
| 生成器文件名和行号 | `xl6_cgen_url.c:78`、`xl6_cgen_url.c:88` |
| 对应生成的 C 代码 | `return (url.iFlags & XL6_URL_F_SECURE) != 0u;` / `return (int)url.iPort;` |
| 中间形态伪代码 | `parse(url); flag switch -> bool/int` |
| 实现同等功能手写的 C 代码 | 手写 C 直接读取 `xrturlview.iFlags/iPort`。 |
| 额外依赖的封装代码 | `XL6_URL_F_ABSOLUTE`、`XL6_URL_F_SECURE` 常量。 |

## CGEN-STD-005 url resolve 与 normalizePath wrapper

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `url.resolve(base, ref)` / `url.normalizePath(path)` |
| 生成规则描述 | resolve 先解析 base，再调用 `xrtUrlResolveTo` 写入 8192 字节临时缓冲区；normalizePath 调用 `xrtUrlNormalizePathTo`。失败统一返回空字符串，成功复制输出缓冲区。 |
| 生成器文件名和行号 | `xl6_cgen_url.c:95`、`xl6_cgen_url.c:106` |
| 对应生成的 C 代码 | `xrtUrlResolveTo(&baseUrl, ref, refSize, buffer, sizeof(buffer), &outLen)` |
| 中间形态伪代码 | `fixedBuffer[8192]; if xrtOk -> copy(buffer,outLen) else ""` |
| 实现同等功能手写的 C 代码 | 手写 C 需要准备缓冲区和失败路径。 |
| 额外依赖的封装代码 | URL copy/empty helper。 |

## CGEN-STD-006 query encode/decode/valid

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `query.encode(text)` / `query.decode(text)` / `query.valid(text)` |
| 生成规则描述 | encode 按最坏情况 `size * 3 + 1` 分配输出，调用 `xrtPercentEncodeTo` 且空格转 `+`；decode 分配 `size + 1`，调用 `xrtFormUrlEncodedDecodeTo`；valid 调用 `xrtFormUrlEncodedValidateN`。失败返回空字符串或 false。 |
| 生成器文件名和行号 | `xl6_cgen_query.c:13`、`xl6_cgen_query.c:22`、`xl6_cgen_query.c:35`、`xl6_cgen_query.c:52`、`xl6_cgen_query.c:67` |
| 对应生成的 C 代码 | `xrtPercentEncodeTo(text, size, outText, cap, &outLen, true);` |
| 中间形态伪代码 | `allocate; call XRT encoder/decoder; nul terminate` |
| 实现同等功能手写的 C 代码 | 手写 C 需要做容量计算和失败释放。 |
| 额外依赖的封装代码 | `xl6_query_empty_string`、`xl6_query_mul3_add`。 |

## CGEN-STD-007 query pair/append

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `query.pair(name, value)` / `query.append(query, name, value)` |
| 生成规则描述 | pair 创建单个 form-url-encoded 字段；append 先复制原 query，再追加字段。两者都通过最坏情况容量计算防止 size_t 溢出，并调用 `xrtFormUrlEncodedAppendFieldTo`。 |
| 生成器文件名和行号 | `xl6_cgen_query.c:73`、`xl6_cgen_query.c:92` |
| 对应生成的 C 代码 | `xrtFormUrlEncodedAppendFieldTo(outText, cap, &off, name, nameSize, value, valueSize, true);` |
| 中间形态伪代码 | `cap = querySize + encodedPairWorstCase; append field` |
| 实现同等功能手写的 C 代码 | 手写 C 需要自己计算容量、复制旧 query、追加字段。 |
| 额外依赖的封装代码 | query 容量 helper。 |

## CGEN-STD-008 regex wrapper

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `regex.isMatch(text, pattern)` / `regex.find(text, pattern)` |
| 生成规则描述 | 每次调用创建 XRT regex 对象，执行匹配或查找，然后销毁 regex。`find` 成功返回起始位置，失败或创建失败返回 `-1`。 |
| 生成器文件名和行号 | `xl6_cgen_regex.c:7`、`xl6_cgen_regex.c:16`、`xl6_cgen_regex.c:29` |
| 对应生成的 C 代码 | `regex = xrtRegexCreate(pattern); found = xrtRegexFind(regex, text, size, &span); xrtRegexDestroy(regex);` |
| 中间形态伪代码 | `create regex; call; destroy; return bool/index` |
| 实现同等功能手写的 C 代码 | 手写 C 同样需要保证 regex 对象销毁。 |
| 额外依赖的封装代码 | `xregex` 和 `xregexspan` 本地 typedef。 |

## CGEN-STD-009 process exec wrapper

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `process.exec(command, timeoutMs)` |
| 生成规则描述 | wrapper 初始化 shell 进程配置，隐藏窗口，调用 `xrtExecCapture` 捕获 stdout。失败或 stdout 为空返回空字符串；成功复制 stdout 并 `xrtProcessResultUnit` 清理结果。 |
| 生成器文件名和行号 | `xl6_cgen_process.c:8`、`xl6_cgen_process.c:82`、`xl6_cgen_process.c:91` |
| 对应生成的 C 代码 | `xrtExecCapture(&config, &result, timeout); memcpy(outText, result.pStdout, size); xrtProcessResultUnit(&result);` |
| 中间形态伪代码 | `run shell command; capture stdout; copy string; cleanup result` |
| 实现同等功能手写的 C 代码 | 手写 C 需要配置 `xprocessconfig` 和释放 `xprocessresult`。 |
| 额外依赖的封装代码 | 进程结构 typedef、`xl6_process_config_shell`、`xl6_process_empty_string`。 |

## CGEN-STD-010 process success/exitCode wrapper

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `process.success(command, timeoutMs)` / `process.exitCode(command, timeoutMs)` |
| 生成规则描述 | success 运行命令并检查未超时、正常退出、exit code 为 0。exitCode 默认返回 `-1`，执行成功且未超时时返回实际退出码。两者都会清理 result。 |
| 生成器文件名和行号 | `xl6_cgen_process.c:111`、`xl6_cgen_process.c:127` |
| 对应生成的 C 代码 | `ok = xrtExecCapture(...) && !timedOut && exitKind == normal && result.iExitCode == 0;` |
| 中间形态伪代码 | `run; inspect exit info; cleanup; return bool/code` |
| 实现同等功能手写的 C 代码 | 手写 C 需要读取 `xprocessexitinfo` 并统一释放。 |
| 额外依赖的封装代码 | 进程结构 typedef。 |

## CGEN-STD-011 dir list/files/dirs wrapper

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `dir.list(path, recursive)` / `dir.files(path)` / `dir.dirs(path)` |
| 生成规则描述 | wrapper 创建 xvalue array，然后调用 `xrtDirScanEx`。回调按 mode 过滤文件/目录，把 path 创建成 xvalue text 并 append 到数组。扫描失败不会抛错，返回当前数组。 |
| 生成器文件名和行号 | `xl6_cgen_dir.c:15`、`xl6_cgen_dir.c:26`、`xl6_cgen_dir.c:43` |
| 对应生成的 C 代码 | `result = xvoCreateArray(); xrtDirScanEx(path, recursive ? 1 : 0, xl6_dir_list_each, &ctx);` |
| 中间形态伪代码 | `array = []; scan(path, callback append path)` |
| 实现同等功能手写的 C 代码 | 手写 C 需要编写 scan 回调和 xvalue array append。 |
| 额外依赖的封装代码 | `XL6_Dir_List_Ctx`、`xl6_dir_list_each`；强制需要 xvalue runtime。 |

## CGEN-STD-012 string startsWith/endsWith wrapper

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `string.startsWith(text, prefix, ignoreCase)` / `string.endsWith(text, suffix, ignoreCase)` |
| 生成规则描述 | starts/ends wrapper 对 null 字符串转空串，空前缀/后缀返回 true，长度不足返回 false，最终用 XRT `xrtStrComp` 按指定大小比较，可选忽略大小写。 |
| 生成器文件名和行号 | `xl6_cgen_string_edge.c:7`、`xl6_cgen_string_edge.c:11`、`xl6_cgen_string_edge.c:20` |
| 对应生成的 C 代码 | `return xrtStrComp(text + textSize - suffixSize, suffix, suffixSize, ignoreCase) == 0;` |
| 中间形态伪代码 | `guard null/empty/length; compare fixed range` |
| 实现同等功能手写的 C 代码 | 手写 C 需要处理空串和长度边界。 |
| 额外依赖的封装代码 | 无额外结构体。 |

## CGEN-STD-013 string split wrapper

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `string.split(text, sep)` |
| 生成规则描述 | wrapper 调用 XRT `xrtSplit` 得到 char**，为每一段创建 xvalue text 并 append 到 xvalue array。parts 数组最后用 `xrtFree` 释放。失败返回空数组。 |
| 生成器文件名和行号 | `xl6_cgen_std_string.c:12`、`xl6_cgen_std_string.c:20` |
| 对应生成的 C 代码 | `parts = xrtSplit(text, 0, sep, 0, false, &count); item = xvoCreateText(parts[i], 0, false);` |
| 中间形态伪代码 | `parts = split; result = array(parts as text)` |
| 实现同等功能手写的 C 代码 | 手写 C 需要把 split 结果逐个转成 xvalue。 |
| 额外依赖的封装代码 | xvalue array/text helper；强制需要 xvalue runtime。 |

## CGEN-STD-014 path join wrapper

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `path.join(a, b)` / `path.join(a, b, c, d)` |
| 生成规则描述 | wrapper 固定接收四个字符串参数，null 转空串。第三、第四参数为空时动态选择调用 `xrtPathJoin` 的 2/3/4 段版本，避免把空尾段拼进去。 |
| 生成器文件名和行号 | `xl6_cgen_path.c:7`、`xl6_cgen_path.c:11` |
| 对应生成的 C 代码 | `return xrtPathJoin(3u, first, second, third);` |
| 中间形态伪代码 | `normalize nulls; choose count by non-empty optional args` |
| 实现同等功能手写的 C 代码 | 手写 C 直接调用 `xrtPathJoin(count, ...)`。 |
| 额外依赖的封装代码 | 无。 |

## CGEN-STD-015 http.server runtime 类型

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `http.server.create(port, handler)` |
| 生成规则描述 | http server wrapper 生成大量 XRT net/httpd typedef，并定义 `XL6_Http_Server`，保存 engine、server、handler function value 和 running 状态。它不是 xlang class，而是 C helper 使用的运行时句柄对象。 |
| 生成器文件名和行号 | `xl6_cgen_http_server.c:7`、`xl6_cgen_http_server.c:112` |
| 对应生成的 C 代码 | `typedef struct XL6_Http_Server { xnetengine* engine; xhttpdserver* server; XL6_Function handler; int running; } XL6_Http_Server;` |
| 中间形态伪代码 | `HttpServerHandle = {engine, server, handler, running}` |
| 实现同等功能手写的 C 代码 | 手写 C 需要维护 engine/server/handler 的聚合结构。 |
| 额外依赖的封装代码 | HTTP server runtime typedef 和 XRT net/httpd 原型。 |

## CGEN-STD-016 http handler function 生命周期

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `http.server.create(8080, func(method, path, query, body) ... end)` |
| 生成规则描述 | 创建 server 时把 handler 保存进 `XL6_Http_Server`，若 function value 有 retain/env 则 retain。销毁和失败路径调用 release，并把 function 清零，防止闭包环境泄漏或悬空。 |
| 生成器文件名和行号 | `xl6_cgen_http_server.c:127`、`xl6_cgen_http_server.c:133`、`xl6_cgen_http_server.c:181`、`xl6_cgen_http_server.c:198`、`xl6_cgen_http_server.c:234` |
| 对应生成的 C 代码 | `self->handler = handler; xl6_http_server_retain_function(&self->handler); ... xl6_http_server_release_function(&self->handler);` |
| 中间形态伪代码 | `retain handler on create; release handler on destroy/fail` |
| 实现同等功能手写的 C 代码 | 手写 C 需要遵守 `XL6_Function` retain/release ABI。 |
| 额外依赖的封装代码 | function runtime / function-var runtime。 |

## CGEN-STD-017 http request 到 function value 调用桥

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | handler 接收 `method, path, query, body` 并返回响应文本 |
| 生成规则描述 | XRT HTTP 回调中，生成器把 method/path/query/body 创建为 xvalue text，然后调用 `xl6_function_call_var(self->handler, 4, ...)`。返回值按 xvalue text 读取，作为 `text/plain; charset=utf-8` 响应体。handler 不存在时返回 500 空响应。 |
| 生成器文件名和行号 | `xl6_cgen_http_server.c:139`、`xl6_cgen_http_server.c:142`、`xl6_cgen_http_server.c:159` |
| 对应生成的 C 代码 | `result = xl6_function_call_var(self->handler, 4, method, path, query, body); xrtHttpdResponseSetBodyCopy(resp, text, strlen(text), "text/plain; charset=utf-8");` |
| 中间形态伪代码 | `request -> xvalue args -> handler(args) -> text response` |
| 实现同等功能手写的 C 代码 | 手写 C 需要自己构造 xvalue 参数并调用 function value。 |
| 额外依赖的封装代码 | `xl6_http_server_text_value`、function-var runtime、xvalue runtime。 |

## CGEN-STD-018 http.server create 失败收束

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `http.server.create(port, handler)` |
| 生成规则描述 | create 检查端口范围和 handler 可调用性，分配 `XL6_Http_Server`，启动 net engine，绑定 `127.0.0.1:port`，创建 httpd server。任一步失败跳转 `fail`，销毁已创建 server/engine，释放 handler，释放 self，返回 0。 |
| 生成器文件名和行号 | `xl6_cgen_http_server.c:172`、`xl6_cgen_http_server.c:187`、`xl6_cgen_http_server.c:194` |
| 对应生成的 C 代码 | `if (self->engine == 0 || xrtNetEngineStart(self->engine) != 0) goto fail; ... fail: cleanup; return 0;` |
| 中间形态伪代码 | `allocate; init engine; init httpd; on failure cleanup in reverse order` |
| 实现同等功能手写的 C 代码 | 手写 C 需要严格维护失败路径释放顺序。 |
| 额外依赖的封装代码 | XRT net/httpd config 和 events。 |

## CGEN-STD-019 http.server start/port/stop/destroy

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `http.server.start(h)` / `http.server.port(h)` / `http.server.stop(h)` / `http.server.destroy(h)` |
| 生成规则描述 | start 检查 handle/server 并调用 `xrtHttpdStart`，成功设置 running。port 返回 bound port，空句柄返回 0。stop 停止 server 和 engine，清 running。destroy 销毁 server、停止并销毁 engine、release handler、释放 self。 |
| 生成器文件名和行号 | `xl6_cgen_http_server.c:203`、`xl6_cgen_http_server.c:212`、`xl6_cgen_http_server.c:219`、`xl6_cgen_http_server.c:228` |
| 对应生成的 C 代码 | `xrtHttpdStop(self->server); xrtNetEngineStop(self->engine); xrtNetEngineDestroy(self->engine); xrtFree(self);` |
| 中间形态伪代码 | `server lifecycle methods operate on opaque handle` |
| 实现同等功能手写的 C 代码 | 手写 C 需要按 server/engine/handler/self 顺序释放。 |
| 额外依赖的封装代码 | HTTP server runtime 类型。 |

## CGEN-STD-020 标准库 wrapper 的级联 runtime 依赖

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `string.split` / `dir.list` / `http.server.create` |
| 生成规则描述 | 某些 wrapper 会强制打开其他 runtime：`string.split` 和 `dir.list` 需要 xvalue runtime；http server 需要 function-var runtime 和 xvalue runtime。模块生成在 needs 阶段设置这些级联 flag。 |
| 生成器文件名和行号 | `xl6_cgen_module.c:41`、`xl6_cgen_module.c:57`、`xl6_cgen_module.c:60` |
| 对应生成的 C 代码 | `if (hasStdStringRuntime || hasDirRuntime) hasXValueRuntime = 1; if (hasHttpServerRuntime) { hasFunctionVarRuntime = 1; hasXValueRuntime = 1; }` |
| 中间形态伪代码 | `stdlib runtime may imply xvalue/function runtime` |
| 实现同等功能手写的 C 代码 | 手写 C 需要自己保证依赖 helper 在使用前定义。 |
| 额外依赖的封装代码 | xvalue runtime、function-var runtime。 |
