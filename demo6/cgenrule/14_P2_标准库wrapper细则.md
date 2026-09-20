# P2 标准库 wrapper 细则

本文细化当前 cgen 中标准库 wrapper 的 C 生成规则。这里的 xlang 写法按用户可见标准库写法描述；实际触发点是标准库声明层生成或引用的 `__xl_std_runtime_*` 内部调用名，needs 扫描命中后由 `xl6CgenEmit*Runtime` 注入对应 helper。

## CGEN-STD-001 标准库 wrapper 注入总规则

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `crypto.sha256(s)`、`process.exec(cmd)` 等 |
| 生成规则描述 | 标准库 wrapper 不总是输出，只有 needs 扫描发现对应 `__xl_std_runtime_*` 调用时才注入。每个 wrapper 文件用 `#ifndef XL6_*_RUNTIME` 包裹，避免同一 C 输出中重复定义。 |
| 生成器文件名和行号 | `xl6_cgen_module.c:44`、`xl6_cgen_module.c:104`、`xl6_cgen_crypto.c:7` |
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
| 生成规则描述 | 完整 URL wrapper 已从核心移入 xhttp 产品；历史实现只作为扩展迁移资产保存在 `dev/deferred/xhttp/src/xl6_cgen_url.c`，核心不得注入 `xrtUrl*`。 |
| 生成器文件名和行号 | 扩展迁移资产 `dev/deferred/xhttp/src/xl6_cgen_url.c` |
| 对应生成的 C 代码 | `view = url.tHost; return xl6_url_copy_text(view.sPtr, view.iLen);` |
| 中间形态伪代码 | `parse(url); switch(part); copy(view)` |
| 实现同等功能手写的 C 代码 | 手写 C 需要调用 `xrtUrlParseViewN` 并复制 view。 |
| 额外依赖的封装代码 | `xrturlview` 本地 typedef、`xl6_url_empty_string`、`xl6_url_copy_text`。 |

## CGEN-STD-004 url flag 与 port wrapper

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `url.valid(text)` / `url.isAbsolute(text)` / `url.isSecure(text)` / `url.port(text)` |
| 生成规则描述 | URL flag 与 port wrapper 属于 xhttp 扩展，不参与核心 needs 扫描或 C 输出。XRT 核心的 HTTP target/Host 能力应由独立轻量类型绑定。 |
| 生成器文件名和行号 | 扩展迁移资产 `dev/deferred/xhttp/src/xl6_cgen_url.c` |
| 对应生成的 C 代码 | `return (url.iFlags & XL6_URL_F_SECURE) != 0u;` / `return (int)url.iPort;` |
| 中间形态伪代码 | `parse(url); flag switch -> bool/int` |
| 实现同等功能手写的 C 代码 | 手写 C 直接读取 `xrturlview.iFlags/iPort`。 |
| 额外依赖的封装代码 | `XL6_URL_F_ABSOLUTE`、`XL6_URL_F_SECURE` 常量。 |

## CGEN-STD-005 url resolve 与 normalizePath wrapper

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `url.resolve(base, ref)` / `url.normalizePath(path)` |
| 生成规则描述 | URL resolve 与 path normalize 属于 xhttp 完整 URL 产品，不进入核心标准库或核心生成代码。 |
| 生成器文件名和行号 | 扩展迁移资产 `dev/deferred/xhttp/src/xl6_cgen_url.c` |
| 对应生成的 C 代码 | `xrtUrlResolveTo(&baseUrl, ref, refSize, buffer, sizeof(buffer), &outLen)` |
| 中间形态伪代码 | `fixedBuffer[8192]; if xrtOk -> copy(buffer,outLen) else ""` |
| 实现同等功能手写的 C 代码 | 手写 C 需要准备缓冲区和失败路径。 |
| 额外依赖的封装代码 | URL copy/empty helper。 |

## CGEN-STD-006 query encode/decode/valid

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `query.encode(text)` / `query.decode(text)` / `query.valid(text)` |
| 生成规则描述 | Query 与 form-urlencoded wrapper 属于 xhttp 扩展，不参与核心 needs 扫描或 C 输出；核心只保留严格 RFC 3986 percent codec。 |
| 生成器文件名和行号 | 扩展迁移资产 `dev/deferred/xhttp/src/xl6_cgen_query.c` |
| 对应生成的 C 代码 | `xrtPercentEncodeTo(text, size, outText, cap, &outLen, true);` |
| 中间形态伪代码 | `allocate; call XRT encoder/decoder; nul terminate` |
| 实现同等功能手写的 C 代码 | 手写 C 需要做容量计算和失败释放。 |
| 额外依赖的封装代码 | `xl6_query_empty_string`、`xl6_query_mul3_add`。 |

## CGEN-STD-007 query pair/append

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `query.pair(name, value)` / `query.append(query, name, value)` |
| 生成规则描述 | Query pair/append 属于 xhttp 扩展；历史容量与溢出边界测试随迁移资产保留，接入扩展时必须继续使用 xhttp 公共 API。 |
| 生成器文件名和行号 | 扩展迁移资产 `dev/deferred/xhttp/src/xl6_cgen_query.c` |
| 对应生成的 C 代码 | `xrtFormUrlEncodedAppendFieldTo(outText, cap, &off, name, nameSize, value, valueSize, true);` |
| 中间形态伪代码 | `cap = querySize + encodedPairWorstCase; append field` |
| 实现同等功能手写的 C 代码 | 手写 C 需要自己计算容量、复制旧 query、追加字段。 |
| 额外依赖的封装代码 | query 容量 helper。 |

## CGEN-STD-008 regex wrapper

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `regex.isMatch(text, pattern)` / `regex.find(text, pattern)` |
| 生成规则描述 | Regex 已从 XRT 核心拆为 xregex 产品，核心编译器不得输出 `XRT_MODULE_REGEX` 或隐式链接正则实现。语言对象适配器随 xregex 包显式导入。 |
| 生成器文件名和行号 | 扩展迁移资产 `dev/deferred/xregex/src/xl6_cgen_regex.c` |
| 对应生成的 C 代码 | xregex 包调用公开的 `xrtRegexCompileConfig`、matcher 与 set API。 |
| 中间形态伪代码 | `import regex -> package source/link closure -> Regex object adapter` |
| 实现同等功能手写的 C 代码 | C 用户显式包含并链接 xregex。 |
| 额外依赖的封装代码 | xregex 公共声明和包级 native adapter。 |

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

## CGEN-STD-015 xhttp 服务端类型已迁出

高级 HTTP 服务端类型不再属于 xlang 核心标准库。实现与完整规则已保存到
`dev/deferred/xhttp`，等待以 xhttp 扩展包重新接入。

## CGEN-STD-016 xhttp handler 生命周期已迁出

HTTP route handler 的 retain/release 桥接已从核心移出，迁移资产位于
`dev/deferred/xhttp`。

## CGEN-STD-017 xhttp request 调用桥已迁出

请求对象到 function value 的高级桥接已从核心移出，迁移资产位于
`dev/deferred/xhttp`。

## CGEN-STD-018 xhttp 服务端生命周期已迁出

高级 create/route/static/start 流程已从核心移出，迁移资产位于
`dev/deferred/xhttp`。

## CGEN-STD-019 xhttp 服务端控制 API 已迁出

高级 start/port/stop/destroy 包装已从核心移出，迁移资产位于
`dev/deferred/xhttp`。

## CGEN-STD-020 标准库 wrapper 的级联 runtime 依赖

| 字段 | 内容 |
| --- | --- |
| xlang 写法 | `string.split` / `dir.list` |
| 生成规则描述 | 某些 wrapper 会强制打开其他 runtime：`string.split` 和 `dir.list` 需要 xvalue runtime。模块生成在 needs 阶段设置这些级联 flag。 |
| 生成器文件名和行号 | `xl6_cgen_module.c:41`、`xl6_cgen_module.c:57` |
| 对应生成的 C 代码 | `if (hasStdStringRuntime || hasDirRuntime) hasXValueRuntime = 1;` |
| 中间形态伪代码 | `stdlib runtime may imply xvalue/function runtime` |
| 实现同等功能手写的 C 代码 | 手写 C 需要自己保证依赖 helper 在使用前定义。 |
| 额外依赖的封装代码 | xvalue runtime。 |
