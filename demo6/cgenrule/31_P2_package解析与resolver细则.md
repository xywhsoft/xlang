# P2 package 解析与 resolver 细则

本文细化 `demo6/src/package/xl6_package.c`。该模块不直接输出 C 代码，但会把 `import` 目标解析为 package manifest，并提供包身份、源码、链接与动态库信息，因此是 import/cgen 链路的前置规则。JSON 词法、转义和 Unicode 统一由 XRT 处理；package 层只校验当前格式的字段约束。

## CGEN-PKG-001 字符串所有权与空值归一

| 项目 | 内容 |
|---|---|
| xlang 写法 | `import pkg`，或读取 manifest 文本字段 |
| 生成规则描述 | package 层拥有的文本均为独立 XRT 分配。内部复制时把 `NULL` 归一为空字符串，调用侧不需要为已构造字段增加特殊释放路径。 |
| 生成器文件名和行号 | `demo6/src/package/xl6_package.c:9` |
| 对应生成的 C 代码 | 不直接生成 C；后续 import 展开会读取这些稳定文本。 |
| 中间形态伪代码 | `owned = copy(text ?? "")` |
| 实现同等功能手写的 C 代码 | `char* owned = xrtCopyStr(src ? src : "", strlen(src ? src : ""));` |
| 额外依赖的封装代码 | XRT 字符串复制与内存释放。 |

## CGEN-PKG-002 XRT 路径目录提取

| 项目 | 内容 |
|---|---|
| xlang 写法 | 源文件中的相对导入，如 `import "./pkg"` |
| 生成规则描述 | 使用 `xrtPathGetDir` 提取导入者目录；空路径或空结果统一回退为 `.`，Windows UTF-8 路径由 XRT 处理。 |
| 生成器文件名和行号 | `demo6/src/package/xl6_package.c:15` |
| 对应生成的 C 代码 | 不直接生成 C；决定相对 import 的第一搜索基准。 |
| 中间形态伪代码 | `dir = dirname(path); if empty(dir): dir = "."` |
| 实现同等功能手写的 C 代码 | 调用平台路径 API，并额外处理 UTF-8 与空路径。 |
| 额外依赖的封装代码 | `xrtPathGetDir`。 |

## CGEN-PKG-003 动态路径拼接与固定输出边界

| 项目 | 内容 |
|---|---|
| xlang 写法 | 搜索 `<base>/<package>/xlang.package.json` |
| 生成规则描述 | 内部路径始终通过 `xrtPathJoin` 动态分配，避免固定缓冲区截断。公开固定缓冲接口只在完整结果可容纳时复制，否则失败，不产生截断路径。 |
| 生成器文件名和行号 | `demo6/src/package/xl6_package.c:30`、`demo6/src/package/xl6_package.c:38` |
| 对应生成的 C 代码 | 不直接生成 C；影响 manifest 搜索位置。 |
| 中间形态伪代码 | `joined = pathJoin(a, b); require len(joined) < outSize` |
| 实现同等功能手写的 C 代码 | 动态计算长度后分配并拼接路径。 |
| 额外依赖的封装代码 | `xrtPathJoin`。 |

## CGEN-PKG-004 缺省包名

| 项目 | 内容 |
|---|---|
| xlang 写法 | manifest 省略 `name` |
| 生成规则描述 | 使用 manifest 所在目录的最后一段作为缺省包名；无法取得有效名称时回退为 `package`。 |
| 生成器文件名和行号 | `demo6/src/package/xl6_package.c:60` |
| 对应生成的 C 代码 | 不直接生成 C；该名称进入 packageId 和私有命名空间。 |
| 中间形态伪代码 | `name = pathName(packageDir) || "package"` |
| 实现同等功能手写的 C 代码 | 取目录 basename，并处理根目录和空值。 |
| 额外依赖的封装代码 | `xrtPathGetName`。 |

## CGEN-PKG-005 后缀识别

| 项目 | 内容 |
|---|---|
| xlang 写法 | 解析显式 manifest 路径 |
| 生成规则描述 | 仅在文本长度足够且尾部完全相等时认定后缀匹配，不做大小写或模糊兼容。 |
| 生成器文件名和行号 | `demo6/src/package/xl6_package.c:72` |
| 对应生成的 C 代码 | 不直接生成 C。 |
| 中间形态伪代码 | `endsWith(text, suffix) = exact suffix match` |
| 实现同等功能手写的 C 代码 | `strcmp(text + textLen - suffixLen, suffix) == 0` |
| 额外依赖的封装代码 | 无。 |

## CGEN-PKG-006 packageId 稳定身份

| 项目 | 内容 |
|---|---|
| xlang 写法 | 多版本依赖中的包实例 |
| 生成规则描述 | packageId 固定为 `<name>@<version>#abi<abiVersion>`；空 name/version/abi 分别收束为 `package`、`0.0.0`、`1`。 |
| 生成器文件名和行号 | `demo6/src/package/xl6_package.c:85` |
| 对应生成的 C 代码 | 不直接生成 C；import 图用它区分同名不同版本或 ABI 的包。 |
| 中间形态伪代码 | `id = format("%s@%s#abi%s", name, version, abi)` |
| 实现同等功能手写的 C 代码 | 使用动态格式化构造稳定键。 |
| 额外依赖的封装代码 | `xrtFormat`。 |

## CGEN-PKG-007 字符串列表生命周期

| 项目 | 内容 |
|---|---|
| xlang 写法 | manifest 的 `cSources`、`xlSources`、`links`、`loadlibs` |
| 生成规则描述 | 列表初始化清零；追加时先复制文本，再扩容指针数组；扩容失败释放新文本且不增加 count；释放时逐项释放并清零。 |
| 生成器文件名和行号 | `demo6/src/package/xl6_package.c:94`、`demo6/src/package/xl6_package.c:101`、`demo6/src/package/xl6_package.c:115` |
| 对应生成的 C 代码 | 不直接生成 C；列表随后进入源码和链接依赖收集。 |
| 中间形态伪代码 | `copy = dup(text); items = realloc(items); append(copy)` |
| 实现同等功能手写的 C 代码 | 显式维护 `char**`、数量和逐项所有权。 |
| 额外依赖的封装代码 | XRT 分配、重分配与释放。 |

## CGEN-PKG-008 manifest 完整生命周期

| 项目 | 内容 |
|---|---|
| xlang 写法 | 读取任意 `xlang.package.json` |
| 生成规则描述 | manifest 初始化为全零；释放覆盖身份字段、四组字符串列表、每个依赖字段、native 字段，最后清零。失败路径可统一调用 free。 |
| 生成器文件名和行号 | `demo6/src/package/xl6_package.c:138`、`demo6/src/package/xl6_package.c:145` |
| 对应生成的 C 代码 | 不直接生成 C。 |
| 中间形态伪代码 | `free(all owned fields); zero(manifest)` |
| 实现同等功能手写的 C 代码 | 为每个拥有字段编写成对释放逻辑。 |
| 额外依赖的封装代码 | XRT 内存释放。 |

## CGEN-PKG-009 XRT JSON 字段查询

| 项目 | 内容 |
|---|---|
| xlang 写法 | manifest 根对象及子对象的字段 |
| 生成规则描述 | JSON 已由 XRT 解析为 `xvalue`。字段存在性和取值统一走 table API；package 层不维护第二套 JSON 游标或转义规则。 |
| 生成器文件名和行号 | `demo6/src/package/xl6_package.c:178`、`demo6/src/package/xl6_package.c:184` |
| 对应生成的 C 代码 | 不直接生成 C。 |
| 中间形态伪代码 | `if table.has(name): value = table[name]` |
| 实现同等功能手写的 C 代码 | 使用通用 JSON DOM/table API 查询字段。 |
| 额外依赖的封装代码 | XRT `xvalue` table API。 |

## CGEN-PKG-010 文本字段严格类型规则

| 项目 | 内容 |
|---|---|
| xlang 写法 | `"format":"xlang-package-1"`、`"name":"pkg"` |
| 生成规则描述 | required 字段缺失即失败；可选字段缺失成功。字段一旦存在就必须是 text；成功后复制为 manifest 自有字符串，并事务式替换旧值。 |
| 生成器文件名和行号 | `demo6/src/package/xl6_package.c:189` |
| 对应生成的 C 代码 | 不直接生成 C。 |
| 中间形态伪代码 | `require !required || exists; require isText(value); out = copy(value)` |
| 实现同等功能手写的 C 代码 | 校验 JSON 类型后复制字符串。 |
| 额外依赖的封装代码 | XRT text `xvalue` API。 |

## CGEN-PKG-011 字符串数组严格类型规则

| 项目 | 内容 |
|---|---|
| xlang 写法 | `"links":["m"]`、`"xlSources":["main.xl"]` |
| 生成规则描述 | 字段缺失合法；存在时必须是 array，且每项都必须是 text。任何一项类型错误或分配失败都会使 manifest 加载失败。 |
| 生成器文件名和行号 | `demo6/src/package/xl6_package.c:213` |
| 对应生成的 C 代码 | 不直接生成 C；成功列表进入后续构建参数。 |
| 中间形态伪代码 | `for item in array: require text; append(copy(item))` |
| 实现同等功能手写的 C 代码 | 遍历 JSON 数组并复制每个字符串。 |
| 额外依赖的封装代码 | XRT array `xvalue` API 与 package 字符串列表。 |

## CGEN-PKG-012 依赖追加事务

| 项目 | 内容 |
|---|---|
| xlang 写法 | manifest 声明一个依赖 |
| 生成规则描述 | 先完成 name/version/abi/packageId 四个字段构造，再扩容依赖数组；任一步失败都释放临时字段，原 manifest 的 count 和已有依赖保持不变。 |
| 生成器文件名和行号 | `demo6/src/package/xl6_package.c:239` |
| 对应生成的 C 代码 | 不直接生成 C；保障低内存失败不会留下半初始化依赖。 |
| 中间形态伪代码 | `tmp = buildDependency(); new = realloc(); commit(tmp)` |
| 实现同等功能手写的 C 代码 | 使用临时对象和最终提交点。 |
| 额外依赖的封装代码 | packageId 与 XRT 内存 API。 |

## CGEN-PKG-013 dependencies 双形态规则

| 项目 | 内容 |
|---|---|
| xlang 写法 | `"dependencies":["json", {"name":"core","version":"2","abiVersion":"1"}]` |
| 生成规则描述 | dependencies 必须是 array。文本简写要求非空，version/abi 使用通配 `*`；对象形态要求非空 text name，可选 version/abi 也必须为 text，缺省为 `*`。其他项类型全部拒绝。 |
| 生成器文件名和行号 | `demo6/src/package/xl6_package.c:293` |
| 对应生成的 C 代码 | 不直接生成 C；形成可多版本解析的依赖身份列表。 |
| 中间形态伪代码 | `text -> dep(name,"*","*"); object -> dep(name,version||"*",abi||"*")` |
| 实现同等功能手写的 C 代码 | 分支解析简写和完整依赖对象。 |
| 额外依赖的封装代码 | 事务式依赖追加。 |

## CGEN-PKG-014 artifact 源文件列表

| 项目 | 内容 |
|---|---|
| xlang 写法 | `"artifact":{"cSources":["a.c"],"xlSources":["a.xl"]}` |
| 生成规则描述 | artifact 缺失合法；存在时必须是 table。当前识别 `cSources` 与 `xlSources`，两者都服从严格字符串数组规则；未知字段由 XRT 值树保留但 package 层忽略。 |
| 生成器文件名和行号 | `demo6/src/package/xl6_package.c:353` |
| 对应生成的 C 代码 | 不直接生成 C；import 阶段按列表注入 C/XLang 源。 |
| 中间形态伪代码 | `artifact = table; readStringArray(cSources); readStringArray(xlSources)` |
| 实现同等功能手写的 C 代码 | 从 JSON 子对象提取两个源文件数组。 |
| 额外依赖的封装代码 | XRT table/array API。 |

## CGEN-PKG-015 native 元数据

| 项目 | 内容 |
|---|---|
| xlang 写法 | `"native":{"singleton":true,"loadName":"foo","version":"1"}` |
| 生成规则描述 | native 缺失合法；存在时必须是 table。singleton 一旦存在必须是 bool，loadName/version 一旦存在必须是 text；不存在的字段保持缺省值。 |
| 生成器文件名和行号 | `demo6/src/package/xl6_package.c:366` |
| 对应生成的 C 代码 | 不直接生成 C；影响动态库单例和加载信息。 |
| 中间形态伪代码 | `native.singleton = bool; native.loadName/version = text` |
| 实现同等功能手写的 C 代码 | 严格读取 native 配置对象。 |
| 额外依赖的封装代码 | XRT bool/text `xvalue` API。 |

## CGEN-PKG-016 manifest 根对象 schema

| 项目 | 内容 |
|---|---|
| xlang 写法 | 完整 `xlang.package.json` |
| 生成规则描述 | 根值必须是 table。format 必填；name/version/abiVersion 可选；artifact、根级 cSources/xlSources、links、loadlibs、dependencies、native 按各自规则读取。未知字段不会触发兼容分支。 |
| 生成器文件名和行号 | `demo6/src/package/xl6_package.c:389` |
| 对应生成的 C 代码 | 不直接生成 C。 |
| 中间形态伪代码 | `require table(root); read known schema fields` |
| 实现同等功能手写的 C 代码 | 用 schema 映射函数组织字段读取。 |
| 额外依赖的封装代码 | 前述字段读取器。 |

## CGEN-PKG-017 UTF-8/BOM 与 XRT JSON 解析

| 项目 | 内容 |
|---|---|
| xlang 写法 | 从 UTF-8 路径读取 package manifest |
| 生成规则描述 | 使用 `xrtFileReadAll(..., XRT_CP_UTF8)` 读取；只允许可选 UTF-8 BOM。正文先经 `xrtJsonValid` 完整校验，再由 `xrtParseJSON` 生成值树；package 层不重复实现 JSON。root 和文件缓冲在所有路径释放。 |
| 生成器文件名和行号 | `demo6/src/package/xl6_package.c:405` |
| 对应生成的 C 代码 | 不直接生成 C。 |
| 中间形态伪代码 | `text = readUtf8(path); skipUtf8Bom(); require jsonValid(); root = parseJson()` |
| 实现同等功能手写的 C 代码 | 使用支持 UTF-8 路径的文件 API和成熟 JSON 解析器。 |
| 额外依赖的封装代码 | XRT 文件、JSON 与 `xvalue` API。 |

## CGEN-PKG-018 格式版本与缺省身份

| 项目 | 内容 |
|---|---|
| xlang 写法 | format 为 `xlang-package-1`，其余身份字段可省略 |
| 生成规则描述 | format 必须精确等于 `xlang-package-1`。name 为空时取包目录名；version 缺省 `0.0.0`；abiVersion 缺省 `1`；随后生成 dir 与 packageId，并检查完整对象不变量。失败时释放整个 manifest。 |
| 生成器文件名和行号 | `demo6/src/package/xl6_package.c:449` |
| 对应生成的 C 代码 | 不直接生成 C；得到后续 import 的权威包身份。 |
| 中间形态伪代码 | `require format==v1; fill defaults; packageId=makeId(); require all fields` |
| 实现同等功能手写的 C 代码 | 校验格式版本后一次性补齐默认值。 |
| 额外依赖的封装代码 | XRT 路径与格式化 API。 |

## CGEN-PKG-019 manifest 结果事务归一

| 项目 | 内容 |
|---|---|
| xlang 写法 | resolver 命中一个 manifest |
| 生成规则描述 | 先独立加载临时 manifest，再复制 packageDir/name/version/abi/packageId 五个字段。只有五项全部成功才写入 result；失败时释放所有临时副本，result 不承接半初始化身份。 |
| 生成器文件名和行号 | `demo6/src/package/xl6_package.c:479` |
| 对应生成的 C 代码 | 不直接生成 C；为 import 展开提供稳定结果。 |
| 中间形态伪代码 | `tmp = loadManifest(); copies = copyAll(tmp); if complete: commit(result)` |
| 实现同等功能手写的 C 代码 | 使用临时 manifest 与提交点构造解析结果。 |
| 额外依赖的封装代码 | manifest 生命周期。 |

## CGEN-PKG-020 失败候选回滚

| 项目 | 内容 |
|---|---|
| xlang 写法 | 一个候选文件存在但 manifest 无效 |
| 生成规则描述 | 清理 manifestPath 与全部包身份字段，并把 resolved/kind 恢复为 unresolved；原始 target 不被清除，因此 resolver 仍可继续尝试其他候选并用于诊断。 |
| 生成器文件名和行号 | `demo6/src/package/xl6_package.c:516` |
| 对应生成的 C 代码 | 不直接生成 C。 |
| 中间形态伪代码 | `clearResolution(result); keep result.target` |
| 实现同等功能手写的 C 代码 | 只回滚候选解析产生的字段。 |
| 额外依赖的封装代码 | XRT 内存释放。 |

## CGEN-PKG-021 manifest 候选命中

| 项目 | 内容 |
|---|---|
| xlang 写法 | 路径或包名候选指向 `xlang.package.json` |
| 生成规则描述 | 候选必须由 `xrtFileExists` 确认存在；随后保存动态 manifestPath、设置 kind，并加载归一身份。内容无效时调用统一回滚，允许继续搜索。 |
| 生成器文件名和行号 | `demo6/src/package/xl6_package.c:537` |
| 对应生成的 C 代码 | 不直接生成 C。 |
| 中间形态伪代码 | `if exists(path): mark candidate; if !finalize: rollback` |
| 实现同等功能手写的 C 代码 | 把文件存在和 manifest 合法性作为两个检查阶段。 |
| 额外依赖的封装代码 | XRT UTF-8 文件存在检查。 |

## CGEN-PKG-022 path target 分类

| 项目 | 内容 |
|---|---|
| xlang 写法 | `import "./a"`、`import "a.xlang.package.json"`、`import pkg` |
| 生成规则描述 | target 含 `\\`、`/`、`:` 或 `.` 时视为路径；绝对路径包括根分隔符开头和 ASCII 盘符形式。分类只选择 resolver 分支，不做旧语法映射。 |
| 生成器文件名和行号 | `demo6/src/package/xl6_package.c:558`、`demo6/src/package/xl6_package.c:573` |
| 对应生成的 C 代码 | 不直接生成 C。 |
| 中间形态伪代码 | `looksLikePath = contains("\\/:."); absolute = rooted || driveLetter` |
| 实现同等功能手写的 C 代码 | 对 import token 做路径与裸包名分类。 |
| 额外依赖的封装代码 | 无。 |

## CGEN-PKG-023 路径候选集合

| 项目 | 内容 |
|---|---|
| xlang 写法 | `import "./pkg"` 或显式 manifest 路径 |
| 生成规则描述 | 对每个 path 先按 manifest 文件原样尝试，失败后尝试 `<path>/xlang.package.json`。第二个候选使用 package kind；路径均动态分配并及时释放。 |
| 生成器文件名和行号 | `demo6/src/package/xl6_package.c:588` |
| 对应生成的 C 代码 | 不直接生成 C。 |
| 中间形态伪代码 | `try(path, FILE) || try(join(path, manifestName), PACKAGE)` |
| 实现同等功能手写的 C 代码 | 依次探测显式文件和目录包。 |
| 额外依赖的封装代码 | XRT 路径 API 与 manifest 尝试器。 |

## CGEN-PKG-024 裸包名约束与 base 搜索

| 项目 | 内容 |
|---|---|
| xlang 写法 | `import json` |
| 生成规则描述 | 裸包名不能为空、不能以 `.` 开始，也不能含路径分隔符或盘符冒号；点号可以存在于名称内部。合法名称在 baseDir 下只搜索 `<base>/<target>/xlang.package.json`。 |
| 生成器文件名和行号 | `demo6/src/package/xl6_package.c:605`、`demo6/src/package/xl6_package.c:620` |
| 对应生成的 C 代码 | 不直接生成 C。 |
| 中间形态伪代码 | `require packageName(target); try base/target/manifest` |
| 实现同等功能手写的 C 代码 | 先验证包名，再构造固定 manifest 位置。 |
| 额外依赖的封装代码 | XRT 路径 API。 |

## CGEN-PKG-025 XLANG_PACKAGE_PATH 搜索

| 项目 | 内容 |
|---|---|
| xlang 写法 | 通过环境变量提供额外 package 根目录 |
| 生成规则描述 | 使用 `xrtEnvGet` 读取 `XLANG_PACKAGE_PATH`；Windows 以分号、其他平台以冒号分段。空段跳过，每个非空段动态复制后按 base 搜索，首次命中立即释放环境缓冲并返回。 |
| 生成器文件名和行号 | `demo6/src/package/xl6_package.c:637` |
| 对应生成的 C 代码 | 不直接生成 C。 |
| 中间形态伪代码 | `for base in split(env, platformSep): tryNamedPackageInBase(base,target)` |
| 实现同等功能手写的 C 代码 | 读取并逐段遍历平台相关的搜索路径变量。 |
| 额外依赖的封装代码 | XRT 环境变量 API。 |

## CGEN-PKG-026 可执行文件旁 extlibs 兜底

| 项目 | 内容 |
|---|---|
| xlang 写法 | 发布目录中的 `import json` |
| 生成规则描述 | 使用 `xrtPathAppDir` 获取当前 xl 可执行文件目录，再搜索其 `extlibs/<target>/xlang.package.json`。它是裸包名搜索的最后兜底，不依赖源码仓库上级目录。 |
| 生成器文件名和行号 | `demo6/src/package/xl6_package.c:676`、`demo6/src/package/xl6_package.c:686` |
| 对应生成的 C 代码 | 不直接生成 C。 |
| 中间形态伪代码 | `tryNamedPackageInBase(appDir()/"extlibs", target)` |
| 实现同等功能手写的 C 代码 | 取程序目录后搜索随程序发布的扩展库目录。 |
| 额外依赖的封装代码 | XRT 应用目录与路径 API。 |

## CGEN-PKG-027 裸包名搜索顺序

| 项目 | 内容 |
|---|---|
| xlang 写法 | `import pkg` |
| 生成规则描述 | 合法裸包名固定按 importerDir、`XLANG_PACKAGE_PATH`、可执行文件旁 `extlibs` 的顺序搜索；首次成功即停止。没有额外兼容目录或源码仓库固定路径。 |
| 生成器文件名和行号 | `demo6/src/package/xl6_package.c:703` |
| 对应生成的 C 代码 | 不直接生成 C；命中的 manifest 决定后续源码与链接信息。 |
| 中间形态伪代码 | `local || envRoots || appExtlibs` |
| 实现同等功能手写的 C 代码 | 明确并固定本地、用户环境、发布目录三层优先级。 |
| 额外依赖的封装代码 | 前述三类搜索器。 |

## CGEN-PKG-028 resolver 结果生命周期与 kind

| 项目 | 内容 |
|---|---|
| xlang 写法 | 每次 import 解析及诊断 |
| 生成规则描述 | init 清零并设置 unresolved；free 释放 target、manifestPath 和所有身份字段后重新 init。kind 对外只映射为 `file`、`package`、`unresolved` 三种稳定文本。 |
| 生成器文件名和行号 | `demo6/src/package/xl6_package.c:720`、`demo6/src/package/xl6_package.c:729`、`demo6/src/package/xl6_package.c:744` |
| 对应生成的 C 代码 | 不直接生成 C。 |
| 中间形态伪代码 | `init -> unresolved; free owned fields; kindName(kind)` |
| 实现同等功能手写的 C 代码 | 为解析结果提供对称 init/free 和诊断枚举文本。 |
| 额外依赖的封装代码 | XRT 内存释放。 |

## CGEN-PKG-029 import resolver 前置条件

| 项目 | 内容 |
|---|---|
| xlang 写法 | `import target` |
| 生成规则描述 | result 必须非空，target 必须非空；入口先初始化 result、复制原始 target 并取得 importerDir。复制或目录构造失败时释放已拥有字段，不进入候选搜索。 |
| 生成器文件名和行号 | `demo6/src/package/xl6_package.c:755` |
| 对应生成的 C 代码 | 不直接生成 C；为后续候选搜索建立可诊断、可释放的结果对象。 |
| 中间形态伪代码 | `init(result); require target; result.target=copy(target); importerDir=dirname(importer)` |
| 实现同等功能手写的 C 代码 | 在解析开始时建立结果对象不变量，并集中处理前置失败。 |
| 额外依赖的封装代码 | resolver 结果生命周期、字符串复制和 dirname helper。 |

## CGEN-PKG-030 路径 import 搜索顺序

| 项目 | 内容 |
|---|---|
| xlang 写法 | `import "D:/pkg"` 或 `import "./pkg"` |
| 生成规则描述 | 仅当 target 像路径时进入。绝对路径只原样探测；相对路径先探测 `<importerDir>/<target>`，再以当前工作目录中的 target 兜底。每次探测都包含显式 manifest 和目录 manifest 两个候选。 |
| 生成器文件名和行号 | `demo6/src/package/xl6_package.c:784` |
| 对应生成的 C 代码 | 不直接生成 C；命中结果交给 import 展开。 |
| 中间形态伪代码 | `absolute ? try(target) : try(importerDir/target) || try(target)` |
| 实现同等功能手写的 C 代码 | 以导入者目录优先解析相对路径，并保留 cwd 兜底。 |
| 额外依赖的封装代码 | path 分类、动态 join 与路径候选探测器。 |

## CGEN-PKG-031 裸包名入口优先于路径分支

| 项目 | 内容 |
|---|---|
| xlang 写法 | `import pkg` |
| 生成规则描述 | 总入口先调用裸包名搜索；只有 target 不满足裸包名或三层包根全部未命中，才考虑 path 分类。合法裸包名即使未命中，也不会被当作任意文件名兼容解析。 |
| 生成器文件名和行号 | `demo6/src/package/xl6_package.c:779` |
| 对应生成的 C 代码 | 不直接生成 C；成功后使用 manifest 的源码和链接元数据。 |
| 中间形态伪代码 | `if tryNamed(importerDir,target): return resolved` |
| 实现同等功能手写的 C 代码 | 在路径解析前执行固定的 package root 搜索。 |
| 额外依赖的封装代码 | 裸包名三层搜索器。 |

## CGEN-PKG-032 unresolved 返回边界

| 项目 | 内容 |
|---|---|
| xlang 写法 | target 在所有允许位置均不存在或 manifest 无效 |
| 生成规则描述 | 所有候选失败后释放 importerDir 并返回 0；result 保留原始 target，kind 保持 unresolved，候选产生的 manifest 和身份字段已经由回滚路径清空。没有旧目录、旧扩展名或固定仓库路径兼容回退。 |
| 生成器文件名和行号 | `demo6/src/package/xl6_package.c:803` |
| 对应生成的 C 代码 | 不直接生成 C；上层生成明确的 import 失败诊断。 |
| 中间形态伪代码 | `free(importerDir); assert result.kind==unresolved; return false` |
| 实现同等功能手写的 C 代码 | 统一释放搜索临时值，并返回仍可用于诊断的失败结果。 |
| 额外依赖的封装代码 | 候选回滚与 resolver 生命周期。 |
