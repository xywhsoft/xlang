# P2 package 解析与 resolver 细则

本文细化 `demo6/src/package/xl6_package.c`。该文件不直接输出 C 代码，但它决定 `import` 目标如何解析为 XAPI 接口文件、package 身份、native/link/cSource 依赖以及私有命名空间输入，因此属于代码生成链路的前置规则。

## CGEN-PKG-001 字符串复制与空值归一

| 项目 | 内容 |
|---|---|
| xlang 写法 | `import pkg` 或读取 manifest 字段 |
| 生成规则描述 | package 层内部复制字符串时，`NULL` 统一视为空字符串，并通过 XRT 内存函数复制。固定缓冲区复制则使用空字符串作为默认输入，保证 resolver 结果中的文本字段始终可安全读取。 |
| 生成器文件名和行号 | `demo6/src/package/xl6_package.c:7`、`demo6/src/package/xl6_package.c:13` |
| 对应生成的 C 代码 | 不直接生成 C；后续 import 生成的 extern/type 声明会使用这些字段。 |
| 中间形态伪代码 | `copy(text ?? "")` |
| 实现同等功能手写的 C 代码 | 手写 resolver 也应把缺省字符串收束为空串，避免调用侧反复判空。 |
| 额外依赖的封装代码 | XRT 内存分配与释放函数。 |

## CGEN-PKG-002 目录名提取规则

| 项目 | 内容 |
|---|---|
| xlang 写法 | 从文件路径 import，如 `import "./pkg/foo"` |
| 生成规则描述 | 从路径中寻找最后一个 `\`、`/` 或 `:`。找不到分隔符时目录为 `.`；遇到 Windows 盘符冒号时保留冒号本身。空路径也回退为 `.`。 |
| 生成器文件名和行号 | `demo6/src/package/xl6_package.c:21` |
| 对应生成的 C 代码 | 不直接生成 C；影响相对 import 的搜索基准目录。 |
| 中间形态伪代码 | `dirname(path) => "." | path[0..lastSep]` |
| 实现同等功能手写的 C 代码 | 手写 C 项目中常见为 `PathRemoveFileSpec` 或自写 dirname。 |
| 额外依赖的封装代码 | 无。 |

## CGEN-PKG-003 路径拼接规则

| 项目 | 内容 |
|---|---|
| xlang 写法 | package 内 `xapi`、`release/extlibs/name`、`test/fixtures/name` |
| 生成规则描述 | `a` 末尾已有 `\` 或 `/` 时直接拼接，否则插入 `\`。输出使用固定缓冲区，只有 `snprintf` 完整写入时返回成功。 |
| 生成器文件名和行号 | `demo6/src/package/xl6_package.c:58` |
| 对应生成的 C 代码 | 不直接生成 C；影响 XAPI/interfacePath 和 package 搜索路径。 |
| 中间形态伪代码 | `join(a,b) = a + (needsSep ? "\\" : "") + b` |
| 实现同等功能手写的 C 代码 | 手写 C 可以使用 `PathCchCombine`，当前实现保持轻量无平台 API。 |
| 额外依赖的封装代码 | 无。 |

## CGEN-PKG-004 文件名回退为 package 名

| 项目 | 内容 |
|---|---|
| xlang 写法 | manifest 未声明 `name` |
| 生成规则描述 | 从路径中取最后一个分隔符后的 base name，并去掉最后一个扩展名。若结果为空，则回退为 `package`。当前 manifest 加载时传入的是 manifest 所在目录，因此默认名来自包目录名。 |
| 生成器文件名和行号 | `demo6/src/package/xl6_package.c:73` |
| 对应生成的 C 代码 | 不直接生成 C；影响 import namespace 和 packageId。 |
| 中间形态伪代码 | `name = basenameNoExt(path) || "package"` |
| 实现同等功能手写的 C 代码 | 手写包系统通常把目录名作为缺省包名。 |
| 额外依赖的封装代码 | 无。 |

## CGEN-PKG-005 packageId 统一格式

| 项目 | 内容 |
|---|---|
| xlang 写法 | 依赖和导入包身份 |
| 生成规则描述 | packageId 固定生成为 `<name>@<version>#abi<abiVersion>`。name 缺省 `package`，version 缺省 `0.0.0`，abiVersion 缺省 `1`。 |
| 生成器文件名和行号 | `demo6/src/package/xl6_package.c:122` |
| 对应生成的 C 代码 | 不直接生成 C；import 侧用 packageId 区分同名包的不同版本实例。 |
| 中间形态伪代码 | `id = f"{name}@{version}#abi{abiVersion}"` |
| 实现同等功能手写的 C 代码 | 手写 resolver 应生成稳定唯一的包实例键。 |
| 额外依赖的封装代码 | 无。 |

## CGEN-PKG-006 字符串列表所有权规则

| 项目 | 内容 |
|---|---|
| xlang 写法 | manifest 中 `links`、`loadlibs`、`cSources` |
| 生成规则描述 | 列表初始化清零；追加时复制文本并扩容指针数组；释放时逐项 `xrtFree` 后释放数组并清零。追加失败会释放刚复制的字符串。 |
| 生成器文件名和行号 | `demo6/src/package/xl6_package.c:133`、`demo6/src/package/xl6_package.c:154` |
| 对应生成的 C 代码 | 不直接生成 C；这些列表会进入 import 依赖和链接参数收集。 |
| 中间形态伪代码 | `list.items += strdup(text)` |
| 实现同等功能手写的 C 代码 | 手写 C 需要显式维护字符串数组生命周期。 |
| 额外依赖的封装代码 | XRT 内存分配与释放函数。 |

## CGEN-PKG-007 manifest 生命周期规则

| 项目 | 内容 |
|---|---|
| xlang 写法 | 读取 `xlang.package.json` |
| 生成规则描述 | manifest 初始化为全零；释放时释放 format/name/version/abiVersion/packageId/xapi/dir、三组字符串列表、每个 dependency 字段、native 字段，最后清零结构体。 |
| 生成器文件名和行号 | `demo6/src/package/xl6_package.c:177`、`demo6/src/package/xl6_package.c:184` |
| 对应生成的 C 代码 | 不直接生成 C；保证 import 多次解析不会泄漏旧 manifest。 |
| 中间形态伪代码 | `free all owned fields; memset(0)` |
| 实现同等功能手写的 C 代码 | 手写 resolver 必须明确每个字段的所有权。 |
| 额外依赖的封装代码 | XRT 内存释放函数。 |

## CGEN-PKG-008 package JSON 基础游标

| 项目 | 内容 |
|---|---|
| xlang 写法 | manifest JSON 任意字段 |
| 生成规则描述 | JSON 解析器只维护 text/len/pos。读取 token 前跳过空格、tab、CR、LF；`peek` 返回当前字符，`consume` 只有匹配指定字符时推进游标。 |
| 生成器文件名和行号 | `demo6/src/package/xl6_package.c:213`、`demo6/src/package/xl6_package.c:219`、`demo6/src/package/xl6_package.c:230`、`demo6/src/package/xl6_package.c:239` |
| 对应生成的 C 代码 | 不直接生成 C；决定 manifest 是否被接受。 |
| 中间形态伪代码 | `skipWs(); if text[pos] == ch: pos++` |
| 实现同等功能手写的 C 代码 | 手写 C 可改用通用 JSON 库，当前实现是小型专用解析器。 |
| 额外依赖的封装代码 | 无。 |

## CGEN-PKG-009 JSON 字符串读取规则

| 项目 | 内容 |
|---|---|
| xlang 写法 | manifest 字符串字段 |
| 生成规则描述 | 字符串必须以 `"` 开始。支持常见转义 `\"`、`\\`、`\/`、`\b`、`\f`、`\n`、`\r`、`\t`。`\uXXXX` 会跳过 4 位并写入 `?`。空字符串返回 XRT 复制的空串。未闭合或扩容失败返回失败。 |
| 生成器文件名和行号 | `demo6/src/package/xl6_package.c:249`、`demo6/src/package/xl6_package.c:275` |
| 对应生成的 C 代码 | 不直接生成 C；影响 packageName、xapi、link 等文本。 |
| 中间形态伪代码 | `read quoted string; decode simple escapes; unicode => "?"` |
| 实现同等功能手写的 C 代码 | 手写 JSON 读取应完整保留 UTF-8；当前实现对 `\uXXXX` 是保守降级。 |
| 额外依赖的封装代码 | XRT realloc/free。 |

## CGEN-PKG-010 JSON 布尔与未知值跳过

| 项目 | 内容 |
|---|---|
| xlang 写法 | manifest 中 `native.singleton` 或未知字段 |
| 生成规则描述 | 布尔只接受 `true`/`false`。未知值可递归跳过 string/object/array/bool/null，其他数字或裸 token 跳到逗号、右括号、右花括号或空白。 |
| 生成器文件名和行号 | `demo6/src/package/xl6_package.c:322`、`demo6/src/package/xl6_package.c:344`、`demo6/src/package/xl6_package.c:370`、`demo6/src/package/xl6_package.c:391` |
| 对应生成的 C 代码 | 不直接生成 C；使 manifest 可向后扩展未知字段。 |
| 中间形态伪代码 | `skip(value) handles {}, [], string, bool, null, primitive` |
| 实现同等功能手写的 C 代码 | 手写解析器常用同样的 unknown field skip 策略。 |
| 额外依赖的封装代码 | package JSON 游标。 |

## CGEN-PKG-011 字符串数组读取规则

| 项目 | 内容 |
|---|---|
| xlang 写法 | `"links": ["m"]`、`"loadlibs": ["x"]`、`"cSources": ["a.c"]` |
| 生成规则描述 | 字段值必须是数组；空数组合法；每一项必须是字符串并追加到目标列表。项之间必须使用逗号，数组以 `]` 结束。 |
| 生成器文件名和行号 | `demo6/src/package/xl6_package.c:425` |
| 对应生成的 C 代码 | 不直接生成 C；后续链接和 C 源文件注入会使用列表内容。 |
| 中间形态伪代码 | `for item in jsonArray: appendString(item)` |
| 实现同等功能手写的 C 代码 | 手写 manifest 读取需要把数组转为可遍历列表。 |
| 额外依赖的封装代码 | package string list。 |

## CGEN-PKG-012 dependency 对象读取与默认身份

| 项目 | 内容 |
|---|---|
| xlang 写法 | `"dependencies": [{"name":"C","version":"1.0","abiVersion":"1"}]` |
| 生成规则描述 | dependency 对象只读取 name/version/abiVersion，未知字段跳过。name 必须存在且非空；version 默认 `0.0.0`，abiVersion 默认 `1`；追加时同步生成 dependency packageId。 |
| 生成器文件名和行号 | `demo6/src/package/xl6_package.c:449`、`demo6/src/package/xl6_package.c:483` |
| 对应生成的 C 代码 | 不直接生成 C；import 依赖图用该 packageId 检查版本实例。 |
| 中间形态伪代码 | `dep = {name, version||0.0.0, abi||1, id}` |
| 实现同等功能手写的 C 代码 | 手写包管理器通常也会在解析依赖时补齐默认版本键。 |
| 额外依赖的封装代码 | packageId 生成规则。 |

## CGEN-PKG-013 dependencies 数组读取规则

| 项目 | 内容 |
|---|---|
| xlang 写法 | manifest 的 `dependencies` 字段 |
| 生成规则描述 | dependencies 必须是数组；空数组合法；每一项按 dependency 对象规则读取。任何一项失败会使整个 manifest 读取失败。 |
| 生成器文件名和行号 | `demo6/src/package/xl6_package.c:541` |
| 对应生成的 C 代码 | 不直接生成 C；影响 import resolver 的依赖实例集合。 |
| 中间形态伪代码 | `dependencies = [readDependencyObject()]` |
| 实现同等功能手写的 C 代码 | 手写包解析需要在加载包时同时加载依赖声明。 |
| 额外依赖的封装代码 | dependency append helper。 |

## CGEN-PKG-014 artifact 对象读取规则

| 项目 | 内容 |
|---|---|
| xlang 写法 | `"artifact": {"cSources": ["a.c"]}` |
| 生成规则描述 | artifact 字段必须是对象。当前只识别 `cSources`，并按字符串数组追加到 manifest 的 cSources；未知 artifact 字段跳过。 |
| 生成器文件名和行号 | `demo6/src/package/xl6_package.c:562` |
| 对应生成的 C 代码 | 不直接生成 C；后续构建会把 cSources 作为包附带 C 源文件。 |
| 中间形态伪代码 | `if key == cSources: readStringArray(manifest.cSources)` |
| 实现同等功能手写的 C 代码 | 手写扩展包需要显式把附属 C 源传给构建系统。 |
| 额外依赖的封装代码 | package string list。 |

## CGEN-PKG-015 native 对象读取规则

| 项目 | 内容 |
|---|---|
| xlang 写法 | `"native": {"singleton": true, "loadName": "x", "version": "1"}` |
| 生成规则描述 | native 字段必须是对象。识别 singleton 布尔值、loadName 字符串、version 字符串，未知字段跳过。重复字段以后出现的值覆盖前值。 |
| 生成器文件名和行号 | `demo6/src/package/xl6_package.c:595` |
| 对应生成的 C 代码 | 不直接生成 C；import 依赖阶段会用 native 信息处理动态库加载约束。 |
| 中间形态伪代码 | `native.singleton/loadName/version = json fields` |
| 实现同等功能手写的 C 代码 | 手写扩展系统通常把 native 动态库信息作为独立元数据。 |
| 额外依赖的封装代码 | JSON bool/string reader。 |

## CGEN-PKG-016 manifest 根对象字段读取

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xlang.package.json` 根对象 |
| 生成规则描述 | 根必须是 JSON 对象。识别 format/name/version/abiVersion/xapi/artifact/links/loadlibs/cSources/dependencies/native。未知字段跳过。links、loadlibs、根级 cSources 都是字符串数组。 |
| 生成器文件名和行号 | `demo6/src/package/xl6_package.c:634` |
| 对应生成的 C 代码 | 不直接生成 C；决定包导出的构建依赖和接口文件。 |
| 中间形态伪代码 | `switch rootKey: assign field or parse child object/array` |
| 实现同等功能手写的 C 代码 | 手写 manifest 解析应允许未知字段以便格式演进。 |
| 额外依赖的封装代码 | package JSON skip 和子对象解析器。 |

## CGEN-PKG-017 manifest 文件读取规则

| 项目 | 内容 |
|---|---|
| xlang 写法 | 解析 `xlang.package.json` |
| 生成规则描述 | 读取 manifest 前释放并重新初始化目标 manifest。文件内容通过 `xrtFileReadAll(path, XRT_CP_UTF8, &len)` 读取，读取失败或 XRT error 非空时直接失败。 |
| 生成器文件名和行号 | `demo6/src/package/xl6_package.c:707` |
| 对应生成的 C 代码 | 不直接生成 C；使用 XRT UTF-8 文件能力保证中文路径可读。 |
| 中间形态伪代码 | `text = xrtFileReadAll(path, UTF8)` |
| 实现同等功能手写的 C 代码 | 手写 C 用 CRT `fopen` 在 Windows 中文路径下可靠性较差。 |
| 额外依赖的封装代码 | XRT 文件 API 和错误缓冲。 |

## CGEN-PKG-018 manifest 格式校验规则

| 项目 | 内容 |
|---|---|
| xlang 写法 | `{"format":"xlang-package-1"}` |
| 生成规则描述 | JSON 解析成功后，manifest 的 format 必须存在并且等于 `xlang-package-1`。不匹配时整个 manifest 加载失败并释放已读内容。 |
| 生成器文件名和行号 | `demo6/src/package/xl6_package.c:737` |
| 对应生成的 C 代码 | 不直接生成 C；避免把非 xlang 包文件误当 package。 |
| 中间形态伪代码 | `ok = ok && manifest.format == "xlang-package-1"` |
| 实现同等功能手写的 C 代码 | 手写包系统也需要显式格式版本。 |
| 额外依赖的封装代码 | manifest lifecycle。 |

## CGEN-PKG-019 manifest 默认字段补齐

| 项目 | 内容 |
|---|---|
| xlang 写法 | manifest 缺省 `name`、`version`、`abiVersion`、`xapi` |
| 生成规则描述 | name 缺省为包目录名；version 缺省 `0.0.0`；abiVersion 缺省 `1`；xapi 缺省 `package.xapi.json`。这些默认值只在格式校验成功后补齐。 |
| 生成器文件名和行号 | `demo6/src/package/xl6_package.c:740` |
| 对应生成的 C 代码 | 不直接生成 C；影响 import namespace、packageId 和 interfacePath。 |
| 中间形态伪代码 | `name ||= fallbackDirName; version ||= 0.0.0; abi ||= 1; xapi ||= package.xapi.json` |
| 实现同等功能手写的 C 代码 | 手写包可让简单包只写 format 和 xapi。 |
| 额外依赖的封装代码 | basename/packageId helper。 |

## CGEN-PKG-020 manifest 最终身份构建与失败清理

| 项目 | 内容 |
|---|---|
| xlang 写法 | 成功读取 package manifest |
| 生成规则描述 | 成功时记录 manifest 所在目录，按 name/version/abiVersion 生成 packageId，并检查 name/version/abiVersion/xapi/dir/packageId 都存在。任何缺失都标记失败并释放整个 manifest。 |
| 生成器文件名和行号 | `demo6/src/package/xl6_package.c:752` |
| 对应生成的 C 代码 | 不直接生成 C；后续 import 以该身份作为包实例权威。 |
| 中间形态伪代码 | `manifest.dir = dirname; manifest.packageId = makePackageId(...)` |
| 实现同等功能手写的 C 代码 | 手写 resolver 应在返回前完成对象不变量校验。 |
| 额外依赖的封装代码 | manifest lifecycle 和 packageId helper。 |

## CGEN-PKG-021 直接 XAPI 文件结果归一

| 项目 | 内容 |
|---|---|
| xlang 写法 | `import "./foo.xapi.json"` |
| 生成规则描述 | 当解析目标本身以 `.xapi.json` 结尾时，不读取 manifest。resolver 把 manifestPath 同时作为 interfacePath；packageDir 为 XAPI 所在目录；packageName 使用 XAPI 文件名去扩展名，并额外剥离 `.xapi` 双后缀；version 固定 `0.0.0`，abiVersion 固定 `1`，并生成 packageId。 |
| 生成器文件名和行号 | `demo6/src/package/xl6_package.c:767` |
| 对应生成的 C 代码 | 不直接生成 C；import 后续直接读取该 XAPI。 |
| 中间形态伪代码 | `if path.endsWith(".xapi.json"): interfacePath = path; packageName = strip(".xapi")` |
| 实现同等功能手写的 C 代码 | 手写导入裸接口文件也需要补齐包身份。 |
| 额外依赖的封装代码 | packageId helper。 |

## CGEN-PKG-022 manifest 结果归一为 interfacePath

| 项目 | 内容 |
|---|---|
| xlang 写法 | `import "./pkg"` 命中 `xlang.package.json` |
| 生成规则描述 | 读取 manifest 后，把 manifest.dir 和 manifest.xapi 拼成 interfacePath，并复制 packageDir、packageName、packageVersion、abiVersion、packageId 到 resolver 结果。读取或拼接失败则解析失败。 |
| 生成器文件名和行号 | `demo6/src/package/xl6_package.c:781` |
| 对应生成的 C 代码 | 不直接生成 C；后续 XAPI loader 使用 interfacePath。 |
| 中间形态伪代码 | `result.interfacePath = join(manifest.dir, manifest.xapi)` |
| 实现同等功能手写的 C 代码 | 手写包解析需要从 manifest 映射到实际接口文件。 |
| 额外依赖的封装代码 | manifest loader。 |

## CGEN-PKG-023 manifest 尝试命中规则

| 项目 | 内容 |
|---|---|
| xlang 写法 | resolver 搜索某个候选路径 |
| 生成规则描述 | 候选路径必须存在。存在后先标记 resolved/kind/manifestPath；若是 `.xapi.json` 则走直接 XAPI 归一，否则读取 manifest 归一。manifest 归一失败时会重置 result 为 unresolved。 |
| 生成器文件名和行号 | `demo6/src/package/xl6_package.c:805` |
| 对应生成的 C 代码 | 不直接生成 C；决定 import 是否继续尝试下一个候选路径。 |
| 中间形态伪代码 | `if exists(path): finalizeBySuffixOrManifest()` |
| 实现同等功能手写的 C 代码 | 手写 resolver 应把“文件存在”和“manifest 可用”分开判断。 |
| 额外依赖的封装代码 | XRT 文件存在检查。 |

## CGEN-PKG-024 path import 判定

| 项目 | 内容 |
|---|---|
| xlang 写法 | `import "./a"`、`import "a.b"`、`import "pkg"` |
| 生成规则描述 | import target 只要包含 `\`、`/`、`:` 或 `.`，就按路径目标解析；否则按裸包名解析。空 target 不算路径。 |
| 生成器文件名和行号 | `demo6/src/package/xl6_package.c:827` |
| 对应生成的 C 代码 | 不直接生成 C；决定 resolver 搜索路径分支。 |
| 中间形态伪代码 | `looksLikePath = any(ch in "\\/:.")` |
| 实现同等功能手写的 C 代码 | 手写包系统常把带分隔符的 import 视为相对或绝对路径。 |
| 额外依赖的封装代码 | 无。 |

## CGEN-PKG-025 resolver 结果初始化与 kind 文本

| 项目 | 内容 |
|---|---|
| xlang 写法 | 每次 `import` 开始解析 |
| 生成规则描述 | resolver 结果初始化为全零，并把 kind 设为 `XL6_PACKAGE_RESOLVE_UNRESOLVED`。kind 文本只输出 `file`、`package`、`unresolved` 三种，用于诊断或调试输出。 |
| 生成器文件名和行号 | `demo6/src/package/xl6_package.c:842`、`demo6/src/package/xl6_package.c:851` |
| 对应生成的 C 代码 | 不直接生成 C；import 诊断使用该状态。 |
| 中间形态伪代码 | `result = {kind: unresolved}` |
| 实现同等功能手写的 C 代码 | 手写 resolver 应保证失败结果也处于可读状态。 |
| 额外依赖的封装代码 | 无。 |

## CGEN-PKG-026 import 入口与 target 保存

| 项目 | 内容 |
|---|---|
| xlang 写法 | `import target` |
| 生成规则描述 | `xl6PackageResolveImport` 先初始化 result，并把原始 target 复制到 result.target。result 为 NULL 或 target 为空时失败。importerPath 用于提取 importerDir，作为相对路径搜索基准。 |
| 生成器文件名和行号 | `demo6/src/package/xl6_package.c:907` |
| 对应生成的 C 代码 | 不直接生成 C；后续 import 展开使用 result.target 和 result.packageName。 |
| 中间形态伪代码 | `result.target = target; importerDir = dirname(importerPath)` |
| 实现同等功能手写的 C 代码 | 手写 resolver 需要保留原始 target 以便诊断。 |
| 额外依赖的封装代码 | dirname helper。 |

## CGEN-PKG-027 path target 候选集合与绝对路径原样命中

| 项目 | 内容 |
|---|---|
| xlang 写法 | `import "D:/pkg/xlang.package.json"` |
| 生成规则描述 | target 若被判定为路径，会先区分绝对路径和相对路径。绝对路径直接把 target 作为候选基准交给 `xl6PackageTryPathCandidates`；相对路径只在 importerDir 优先候选失败后，才把原始 target 作为 cwd 兜底候选。候选基准命中 `.xapi.json` 得到 file 结果；命中 manifest 文件时保持 file kind。 |
| 生成器文件名和行号 | `demo6/src/package/xl6_package.c:847`、`demo6/src/package/xl6_package.c:862`、`demo6/src/package/xl6_package.c:925`、`demo6/src/package/xl6_package.c:934` |
| 对应生成的 C 代码 | 不直接生成 C；命中后 import 使用该目标接口。 |
| 中间形态伪代码 | `if absolute(target) tryCandidates(target) else tryCandidates(join(importerDir,target)); tryCandidates(target)` |
| 实现同等功能手写的 C 代码 | 手写导入绝对路径时也应优先使用用户指定路径。 |
| 额外依赖的封装代码 | manifest try helper。 |

## CGEN-PKG-028 path target 自动追加 `.xapi.json`

| 项目 | 内容 |
|---|---|
| xlang 写法 | `import "./foo"`，旁边存在 `./foo.xapi.json` |
| 生成规则描述 | 每个候选基准路径都会先尝试原样路径，再追加 `.xapi.json` 作为 file kind 尝试。这样可以省略接口文件后缀；相对路径场景下，追加后缀也跟随 importerDir 优先级。 |
| 生成器文件名和行号 | `demo6/src/package/xl6_package.c:869`、`demo6/src/package/xl6_package.c:872` |
| 对应生成的 C 代码 | 不直接生成 C；命中后 import 读取追加后缀的 XAPI。 |
| 中间形态伪代码 | `tryManifest(base, FILE); tryManifest(base + ".xapi.json", FILE)` |
| 实现同等功能手写的 C 代码 | 手写包导入也常支持扩展名省略。 |
| 额外依赖的封装代码 | manifest try helper。 |

## CGEN-PKG-029 path target 相对 importer 目录命中

| 项目 | 内容 |
|---|---|
| xlang 写法 | 源文件位于 `src/main.xl`，写 `import "./pkg"` |
| 生成规则描述 | 相对路径 target 先把 importerDir 与 target 拼接后尝试完整候选集合；只有 importerDir 候选全部失败时，才回退尝试原始 target。这样同一进程 cwd 下的同名 XAPI 不会遮蔽源文件旁边的相对 import。 |
| 生成器文件名和行号 | `demo6/src/package/xl6_package.c:922`、`demo6/src/package/xl6_package.c:930`、`demo6/src/package/xl6_package.c:934` |
| 对应生成的 C 代码 | 不直接生成 C；使相对 import 以当前源文件所在目录为第一基准。 |
| 中间形态伪代码 | `base = join(importerDir, target); if tryCandidates(base) return; tryCandidates(target)` |
| 实现同等功能手写的 C 代码 | 手写模块系统通常使用 importer 文件目录解析相对路径。 |
| 额外依赖的封装代码 | path join helper。 |

## CGEN-PKG-030 path target 目录包命中顺序

| 项目 | 内容 |
|---|---|
| xlang 写法 | `import "./pkg"`，目录内有 `xlang.package.json` 或 `package.xapi.json` |
| 生成规则描述 | 每个候选基准路径都可以作为目录包：先尝试 `<base>/xlang.package.json` 并以 package kind 归一；失败后尝试 `<base>/package.xapi.json`，同样标为 package kind。相对路径目录包仍遵循 importerDir 优先。 |
| 生成器文件名和行号 | `demo6/src/package/xl6_package.c:876`、`demo6/src/package/xl6_package.c:880` |
| 对应生成的 C 代码 | 不直接生成 C；目录包优先使用 manifest，其次允许只有默认 XAPI 的轻量包。 |
| 中间形态伪代码 | `try base/xlang.package.json; try base/package.xapi.json` |
| 实现同等功能手写的 C 代码 | 手写包系统常约定目录下 manifest 优先于默认接口文件。 |
| 额外依赖的封装代码 | manifest try helper。 |

## CGEN-PKG-031 裸包名 packages 搜索规则

| 项目 | 内容 |
|---|---|
| xlang 写法 | `import json` |
| 生成规则描述 | target 不像路径时，先搜索 `release/extlibs/<target>/xlang.package.json`，再搜索 `release/extlibs/<target>/package.xapi.json`。两者均以 package kind 解析。 |
| 生成器文件名和行号 | `demo6/src/package/xl6_package.c:907` |
| 对应生成的 C 代码 | 不直接生成 C；命中后 import 声明生成会使用该包 XAPI。 |
| 中间形态伪代码 | `base = join("packages", target); try base/xlang.package.json; try base/package.xapi.json` |
| 实现同等功能手写的 C 代码 | 手写包管理器通常先查项目本地 packages 目录。 |
| 额外依赖的封装代码 | path join 和 manifest try helper。 |

## CGEN-PKG-032 裸包名 package path fixture 搜索与接口路径查询

| 项目 | 内容 |
|---|---|
| xlang 写法 | `import json`，包位于 `lib/json`；或 import 侧请求最终 XAPI 路径 |
| 生成规则描述 | `packages` 未命中后搜索 `lib/<target>/xlang.package.json`，再搜索 `lib/<target>/package.xapi.json`。解析完成后，`xl6PackageResolveInterfacePath` 优先返回 result.interfacePath；若为空且 manifestPath 是 `.xapi.json` 则返回 manifestPath；否则尝试 manifest 所在目录下的 `package.xapi.json`。 |
| 生成器文件名和行号 | `demo6/src/package/xl6_package.c:917`、`demo6/src/package/xl6_package.c:930` |
| 对应生成的 C 代码 | 不直接生成 C；最终返回路径供 `xl6XapiLoadFile` 读取接口元数据。 |
| 中间形态伪代码 | `search lib target; interface = result.interfacePath || manifestPathIfXapi || dirname(manifest)/package.xapi.json` |
| 实现同等功能手写的 C 代码 | 手写包系统需要一个单独 API 把解析结果转为接口文件路径。 |
| 额外依赖的封装代码 | XRT file exists、path helper。 |
