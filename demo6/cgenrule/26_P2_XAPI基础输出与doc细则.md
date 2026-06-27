# P2 XAPI 基础输出与 doc 细则

本文细化 `demo6/src/abi/xl6_xapi_dump.c` 的前半部分。XAPI 不是 C 代码，但它是 xl 生成的接口元数据产物，会影响 import/package/C 互通，因此本目录把它作为“生成规则”一并记录。

## CGEN-XAPI-001 XAPI 文本比较大小写敏感

| 项目 | 内容 |
|---|---|
| xlang 写法 | `main`、`Main`、`function` 等关键元数据名称 |
| 生成规则描述 | XAPI 内部名称比较只在两侧非 NULL 且 `strcmp` 完全相等时为真，不做大小写折叠。入口识别、func 类型识别和 kind 判断都保持大小写敏感。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_dump.c:9` |
| 对应生成的 C 代码 | 不生成 C；影响 XAPI 中是否输出 main entry、function signature 等字段。 |
| 中间形态伪代码 | `equals = a != null && b != null && strcmp(a,b)==0` |
| 实现同等功能手写的 C 代码 | C 符号和入口名同样大小写敏感。 |
| 额外依赖的封装代码 | 无。 |

## CGEN-XAPI-002 JSON 字符串转义范围输出

| 项目 | 内容 |
|---|---|
| xlang 写法 | export 名称、doc 文本、import target 中含引号、反斜杠或换行 |
| 生成规则描述 | 对指定字节范围逐字节输出 JSON 转义：`"`、`\`、退格、换页、换行、回车、tab 使用 JSON 短转义；小于 0x20 的控制字符使用 `\u00XX`；其他字节原样输出。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_dump.c:14` |
| 对应生成的 C 代码 | 不生成 C；生成 JSON 片段如 `"a\\nb"`。 |
| 中间形态伪代码 | `for ch in bytes: emit json escaped ch` |
| 实现同等功能手写的 C 代码 | 手写 JSON 也必须转义控制字符和引号。 |
| 额外依赖的封装代码 | XL6_Output 输出接口。 |

## CGEN-XAPI-003 JSON 字符串包装

| 项目 | 内容 |
|---|---|
| xlang 写法 | 任意字符串字段进入 XAPI |
| 生成规则描述 | `xl6XapiJsonString` 输出双引号，调用 JSON 转义，再输出结束双引号；NULL 文本会输出空字符串内容。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_dump.c:45` |
| 对应生成的 C 代码 | 不生成 C；生成 JSON 字符串字段。 |
| 中间形态伪代码 | `emit '"' + escaped(text) + '"'` |
| 实现同等功能手写的 C 代码 | 手写 JSON 需要统一字符串输出函数。 |
| 额外依赖的封装代码 | JSON 转义 helper。 |

## CGEN-XAPI-004 C 符号字符串去引号输出

| 项目 | 内容 |
|---|---|
| xlang 写法 | `export func foo` 的 `node->text` 可能是裸 C 符号或带引号符号 |
| 生成规则描述 | C symbol 输出 JSON 时，如果文本以单引号或双引号开头且长度足够，则去掉首尾字符后转义输出；否则直接转义整个文本。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_dump.c:59` |
| 对应生成的 C 代码 | 不生成 C；生成 `"cSymbol":"foo_impl"`。 |
| 中间形态伪代码 | `if quoted symbol: json(inner); else json(text)` |
| 实现同等功能手写的 C 代码 | 手写接口描述应记录真实 C symbol，而不是源代码引号。 |
| 额外依赖的封装代码 | JSON 转义 helper。 |

## CGEN-XAPI-005 类型别名查找与解析

| 项目 | 内容 |
|---|---|
| xlang 写法 | `typedef MyInt int` 后导出 `MyInt` |
| 生成规则描述 | XAPI 把普通 type alias 和 extern type alias 都视为可解析别名。解析仅处理简单无泛型实参的 `XL6_AST_TYPE`，最多追踪 32 层，找到别名后使用别名右侧类型节点。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_dump.c:75` |
| 对应生成的 C 代码 | 不生成 C；XAPI 类型字符串会输出别名解析后的规范类型。 |
| 中间形态伪代码 | `while guard < 32 and simple type has alias: type = alias.target` |
| 实现同等功能手写的 C 代码 | 手写接口文件通常直接写规范类型或保持别名，两者需明确约定。 |
| 额外依赖的封装代码 | AST 根节点类型别名扫描。 |

## CGEN-XAPI-006 func 类型 JSON 字符串

| 项目 | 内容 |
|---|---|
| xlang 写法 | `func(int,string) bool`、`func() (int,string)` |
| 生成规则描述 | 类型名为 `function` 且有参数列表和返回列表两个子节点时，输出 `func(arg1,arg2)`；有返回值时追加 `as ret`，多返回值用 `as (a,b)` 包住。参数和返回类型递归走同一类型输出规则。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_dump.c:114` |
| 对应生成的 C 代码 | 不生成 C；生成 XAPI 类型字符串如 `"func(int,string) as bool"`。 |
| 中间形态伪代码 | `func(args) as returns; multi returns wrapped with parentheses` |
| 实现同等功能手写的 C 代码 | 手写接口元数据需要稳定的函数签名字符串。 |
| 额外依赖的封装代码 | 类型 AST 输出 helper。 |

## CGEN-XAPI-007 普通与泛型类型 JSON 字符串

| 项目 | 内容 |
|---|---|
| xlang 写法 | `int`、`array<int>`、`dict<string>` |
| 生成规则描述 | 非 func 类型先输出解析别名后的类型名；若有第一个子节点且该子节点是 list，则输出 `<...>` 泛型实参，实参之间用逗号分隔并递归输出。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_dump.c:157` |
| 对应生成的 C 代码 | 不生成 C；生成 `"array<int>"` 一类 XAPI 类型字符串。 |
| 中间形态伪代码 | `name + optional "<" + args.join(",") + ">"` |
| 实现同等功能手写的 C 代码 | 手写接口元数据需要与编译器一致的类型字符串规范。 |
| 额外依赖的封装代码 | 类型 AST 输出 helper。 |

## CGEN-XAPI-008 AST 入口识别

| 项目 | 内容 |
|---|---|
| xlang 写法 | `func main(...)` 或顶层运行语句 |
| 生成规则描述 | XAPI 使用与 cgen 入口类似的规则：顶层节点排除 import、函数/extern/export、类型别名、custom/union/class 后视为运行语句；普通函数名精确为 `main` 时视为 xlang main。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_dump.c:184` |
| 对应生成的 C 代码 | 不生成 C；影响 XAPI entry kind。 |
| 中间形态伪代码 | `main = first FUNC named main; hasTopLevel = any runtime stmt` |
| 实现同等功能手写的 C 代码 | C 入口由 `main` 函数决定，不存在顶层运行语句。 |
| 额外依赖的封装代码 | AST 入口扫描 helper。 |

## CGEN-XAPI-009 AST 入口 JSON 输出

| 项目 | 内容 |
|---|---|
| xlang 写法 | 有 `main`、只有顶层语句、或纯库模块 |
| 生成规则描述 | 如果有 xlang main，输出 `kind:xlang-main`、`function:main`、`argvMode:array<var>`、`returnType:int`、`xlInit/xlUnit`；否则若有顶层运行语句输出 `kind:script-main`；两者都没有则输出 `kind:none` 且 `requiresExplicitInit:true`。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_dump.c:237` |
| 对应生成的 C 代码 | 不生成 C；生成 `"entry":{...}`。 |
| 中间形态伪代码 | `entry = main ? xlang-main : topLevel ? script-main : none` |
| 实现同等功能手写的 C 代码 | 手写元数据需声明库是否有可执行入口。 |
| 额外依赖的封装代码 | XAPI entry schema。 |

> 备注：entry schema 的 `xlang-main` 与 `script-main` 分支都必须输出 `cSymbol:"xlang_main"`，该字段是 DLL 运行版和 C 侧工具直接定位入口函数的稳定 ABI 锚点。

## CGEN-XAPI-010 参数 JSON 数组输出

| 项目 | 内容 |
|---|---|
| xlang 写法 | `func f(int a, string b)` |
| 生成规则描述 | 参数列表输出为 JSON 数组，每个参数包含 `name` 和 `type`；缺失类型时输出 `<missing-type>`。此早期直接 dump 路径不输出 optional/vararg 字段，完整 module 路径会补充。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_dump.c:257` |
| 对应生成的 C 代码 | 不生成 C；生成 `"params":[{"name":"a","type":"int"}]`。 |
| 中间形态伪代码 | `for param: emit {name,type or missing}` |
| 实现同等功能手写的 C 代码 | 手写接口文件需要列出参数名和类型。 |
| 额外依赖的封装代码 | 类型字符串输出 helper。 |

## CGEN-XAPI-011 返回值 JSON 数组输出

| 项目 | 内容 |
|---|---|
| xlang 写法 | `func pair() (int,string)` |
| 生成规则描述 | 返回列表输出为 JSON 数组，每个返回值只包含 `type` 字段；多返回值按源顺序输出多个元素。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_dump.c:282` |
| 对应生成的 C 代码 | 不生成 C；生成 `"returns":[{"type":"int"},{"type":"string"}]`。 |
| 中间形态伪代码 | `for returnType in returns: emit {type}` |
| 实现同等功能手写的 C 代码 | 手写接口文件需要稳定表示多返回值顺序。 |
| 额外依赖的封装代码 | 类型字符串输出 helper。 |

## CGEN-XAPI-012 export function 直接 JSON 输出

| 项目 | 内容 |
|---|---|
| xlang 写法 | `export func add(int a, int b) int` |
| 生成规则描述 | 直接 dump 路径输出 `{kind:function,name,cSymbol,params,returns}`。name 来自函数名节点，cSymbol 来自导出节点文本并去引号，params/returns 使用基础数组输出。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_dump.c:300` |
| 对应生成的 C 代码 | 不生成 C；生成 exports 数组中的 function 对象。 |
| 中间形态伪代码 | `emit export function json object` |
| 实现同等功能手写的 C 代码 | 手写接口文件需要同步维护函数名、C symbol 和签名。 |
| 额外依赖的封装代码 | XAPI exports schema。 |

## CGEN-XAPI-013 export var/const 直接 JSON 输出

| 项目 | 内容 |
|---|---|
| xlang 写法 | `export var int count`、`export const string name` |
| 生成规则描述 | export var 和 export const 输出 `{kind,name,cSymbol,type}`，kind 为 `const` 或 `var`，cSymbol 去引号，type 使用 XAPI 类型字符串。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_dump.c:324` |
| 对应生成的 C 代码 | 不生成 C；生成 exports 数组中的 var/const 对象。 |
| 中间形态伪代码 | `kind = export const ? const : var; emit type` |
| 实现同等功能手写的 C 代码 | 手写接口文件需要声明变量符号和类型。 |
| 额外依赖的封装代码 | XAPI exports schema。 |

## CGEN-XAPI-014 类型声明直接 JSON 输出

| 项目 | 内容 |
|---|---|
| xlang 写法 | `class A`、`custom File`、`union U` |
| 生成规则描述 | 直接 dump 路径仅输出类型 kind 和 name：custom 为 `custom`，union 为 `union`，其他为 `class`。完整字段、基类和泛型参数由 module 路径补充。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_dump.c:346` |
| 对应生成的 C 代码 | 不生成 C；生成 types 数组中的简略类型对象。 |
| 中间形态伪代码 | `kind = custom/union/class; emit name` |
| 实现同等功能手写的 C 代码 | 手写接口文件至少需要类型名和类型类别。 |
| 额外依赖的封装代码 | XAPI types schema。 |

## CGEN-XAPI-015 import 直接 JSON 输出

| 项目 | 内容 |
|---|---|
| xlang 写法 | `import "pkg"`、`import counter` |
| 生成规则描述 | import target 如果是完整引号包裹的字符串，则去掉首尾引号后输出；否则按原文本输出。mode 固定为 `package-or-path`。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_dump.c:358` |
| 对应生成的 C 代码 | 不生成 C；生成 `"imports":[{"target":"counter","mode":"package-or-path"}]`。 |
| 中间形态伪代码 | `target = unquote if quoted else text; mode = package-or-path` |
| 实现同等功能手写的 C 代码 | 手写接口文件需要记录导入目标和解析模式。 |
| 额外依赖的封装代码 | XAPI imports schema。 |

## CGEN-XAPI-016 XAPI 字符串构造器初始化与扩容

| 项目 | 内容 |
|---|---|
| xlang 写法 | 需要构建 packageId、signature、doc 文本等字符串 |
| 生成规则描述 | string builder 初始化为全零并设置 ok=1；扩容按 64 起步并倍增直到容纳 `len + addLen + 1`；扩容失败会把 ok 置 0，后续追加失败。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_dump.c:380` |
| 对应生成的 C 代码 | 不生成 C；用于生成 XAPI 字符串字段。 |
| 中间形态伪代码 | `capacity = max(64, double until required); ok=false on realloc fail` |
| 实现同等功能手写的 C 代码 | 手写工具一般使用动态字符串或缓冲区。 |
| 额外依赖的封装代码 | XAPI string builder。 |

## CGEN-XAPI-017 字符串构造器追加、接管和复制

| 项目 | 内容 |
|---|---|
| xlang 写法 | 构造签名、doc 字段正文、moduleId |
| 生成规则描述 | append range 在扩容成功后 memcpy 并补 `\0`；append string 忽略 NULL；append char 追加单字符；take 会把 builder 持有的 data 转移给调用方，空 builder 返回空字符串副本；dup text 把 NULL 视为空字符串复制。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_dump.c:429` |
| 对应生成的 C 代码 | 不生成 C；生成内部元数据字符串。 |
| 中间形态伪代码 | `append; take transfers ownership; null text -> ""` |
| 实现同等功能手写的 C 代码 | 手写工具需要明确字符串所有权。 |
| 额外依赖的封装代码 | XAPI string builder。 |

## CGEN-XAPI-018 doc 起止标记与基础缩进

| 项目 | 内容 |
|---|---|
| xlang 写法 | `#doc ... #end` |
| 生成规则描述 | 检测 `#doc`/`#end` 时先去掉行尾 CR/LF，再跳过行首空格和 tab；基础缩进取原始 doc 中 `#doc` 所在行的前导缩进。后续解析每行先去掉这层基础缩进。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_dump.c:477` |
| 对应生成的 C 代码 | 不生成 C；影响 XAPI doc.fields 的文本内容。 |
| 中间形态伪代码 | `baseIndent = indent(line where trimmed starts #doc)` |
| 实现同等功能手写的 C 代码 | 手写文档生成器通常也会剥离公共缩进。 |
| 额外依赖的封装代码 | doc parser helper。 |

## CGEN-XAPI-019 doc 字段头解析

| 项目 | 内容 |
|---|---|
| xlang 写法 | `desc:`、`param name:`、`return value:`、`note:`、`example:` |
| 生成规则描述 | 字段头必须从行首开始，不能以空格或 tab 开头，并包含冒号。支持 `desc`、`note`、`example`、`param <name>`、`return <name>`；冒号后若有一个空格或 tab 会被跳过作为正文起点。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_dump.c:554` |
| 对应生成的 C 代码 | 不生成 C；生成 doc fields 的 kind/name/text。 |
| 中间形态伪代码 | `head before ':' -> kind/name; body after optional single whitespace` |
| 实现同等功能手写的 C 代码 | 手写文档工具需要相同字段语法。 |
| 额外依赖的封装代码 | doc parser helper。 |

## CGEN-XAPI-020 doc 正文去缩进字符串输出

| 项目 | 内容 |
|---|---|
| xlang 写法 | 多行 `desc:`、`note:`、`example:` 正文 |
| 生成规则描述 | 输出字段正文前，先扫描非空内容行的最小缩进；输出时每行剥离该最小缩进，去掉行尾 `\r`，行间用 JSON `\n` 连接，最后作为 JSON 字符串输出。全空正文最小缩进按 0 处理。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_dump.c:611` |
| 对应生成的 C 代码 | 不生成 C；生成 `"text":"line1\nline2"`。 |
| 中间形态伪代码 | `minIndent = min(indent(nonblank lines)); emit each line without minIndent` |
| 实现同等功能手写的 C 代码 | 手写文档生成器需要避免把代码块外层缩进带入元数据。 |
| 额外依赖的封装代码 | JSON 转义 helper。 |

## CGEN-XAPI-021 doc 单字段 JSON 输出

| 项目 | 内容 |
|---|---|
| xlang 写法 | `param path:`、`example:` |
| 生成规则描述 | 当存在当前字段 kind 时，输出 `{kind,text}`；param/return 这类有 name 的字段额外输出 `name`。多个字段之间由调用方维护逗号。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_dump.c:682` |
| 对应生成的 C 代码 | 不生成 C；生成 `{"kind":"param","name":"path","text":"..."}`。 |
| 中间形态伪代码 | `if kind: emit {kind, optional name, dedented text}` |
| 实现同等功能手写的 C 代码 | 手写 XAPI 需要保持字段数组结构。 |
| 额外依赖的封装代码 | doc field 输出 helper。 |

## CGEN-XAPI-022 完整 doc JSON 解析输出

| 项目 | 内容 |
|---|---|
| xlang 写法 | `#doc ... #end` 绑定到函数、变量、类型或字段 |
| 生成规则描述 | doc JSON 总是输出 `raw` 原文和 `fields` 数组。若 raw 为 NULL，fields 为空。解析时跳过 `#doc/#end` 行；遇到新字段头时先输出上一字段；非字段头行追加到当前字段正文。没有当前字段的裸文本会被忽略，语义检查阶段负责报错。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_dump.c:709` |
| 对应生成的 C 代码 | 不生成 C；生成 `"doc":{"raw":"...","fields":[...]}`。 |
| 中间形态伪代码 | `for lines: marker skip; header -> flush/start; body -> append to current` |
| 实现同等功能手写的 C 代码 | 手写文档工具也会先解析成字段数组再序列化。 |
| 额外依赖的封装代码 | doc parser 与 string builder。 |

## CGEN-XAPI-023 C symbol 与 import target 复制规范

| 项目 | 内容 |
|---|---|
| xlang 写法 | export C symbol、`import "pkg"` |
| 生成规则描述 | 复制 C symbol 时，若文本以单引号或双引号开头且长度足够，则复制中间内容；复制 import target 时要求首尾引号匹配才去引号。NULL 一律复制为空字符串。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_dump.c:783` |
| 对应生成的 C 代码 | 不生成 C；影响 XAPI module 模型中的 cSymbol/import.target。 |
| 中间形态伪代码 | `dupCStringSymbol = strip first/last if leading quote; dupImport = strip only matched quotes` |
| 实现同等功能手写的 C 代码 | 手写元数据应记录无源代码引号的真实 symbol/target。 |
| 额外依赖的封装代码 | XAPI 字符串复制 helper。 |

## CGEN-XAPI-024 packageId 与 moduleId 构造

| 项目 | 内容 |
|---|---|
| xlang 写法 | 默认应用模块或 package 模块 |
| 生成规则描述 | packageId 格式为 `name@version#abiVersion`，默认 `app@0.0.0#abi1`；moduleId 格式为 `packageId::moduleName`，默认模块名 `app`。packageId 使用 512 字节栈缓冲区，溢出返回 NULL。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_dump.c:812` |
| 对应生成的 C 代码 | 不生成 C；生成 `"packageId":"app@0.0.0#abi1"`、`"moduleId":"app@0.0.0#abi1::app"`。 |
| 中间形态伪代码 | `packageId = name@version#abiAbi; moduleId = packageId + "::" + moduleName` |
| 实现同等功能手写的 C 代码 | 手写包元数据必须稳定描述包实例身份。 |
| 额外依赖的封装代码 | XAPI string builder。 |

## CGEN-XAPI-025 导出签名字符串构造

| 项目 | 内容 |
|---|---|
| xlang 写法 | `export func add(int,int) int`、`export var int count` |
| 生成规则描述 | function 导出签名为 `func(paramTypes) as returns`，多返回值用括号包住；非 function 导出签名为 `kind:type`。缺失类型使用 `<missing-type>`。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_dump.c:841` |
| 对应生成的 C 代码 | 不生成 C；生成 `"signature":"func(int,int) as int"`。 |
| 中间形态伪代码 | `if function: func(args) as rets else kind:type` |
| 实现同等功能手写的 C 代码 | 手写接口元数据需要与编译器一致的签名字符串。 |
| 额外依赖的封装代码 | XAPI string builder。 |

## CGEN-XAPI-026 exportId 构造与身份补齐

| 项目 | 内容 |
|---|---|
| xlang 写法 | 任意 export 函数、变量或常量 |
| 生成规则描述 | exportId 格式为 `moduleId::name(signature)`。最终化导出身份时，若 signature 或 exportId 为空，则分别构造；两者都非 NULL 才视为成功。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_dump.c:886` |
| 对应生成的 C 代码 | 不生成 C；生成 `"exportId":"pkg@ver#abi1::mod::add(func(int) as int)"`。 |
| 中间形态伪代码 | `signature ??= buildSignature(item); exportId ??= moduleId::name(signature)` |
| 实现同等功能手写的 C 代码 | 手写接口元数据必须提供稳定唯一的导出身份。 |
| 额外依赖的封装代码 | XAPI module identity。 |

## CGEN-XAPI-027 类型字符串构造器路径

| 项目 | 内容 |
|---|---|
| xlang 写法 | 类型字段进入 module 模型，例如 `array<int>`、`func(int) string` |
| 生成规则描述 | 字符串构造器版本的类型输出与 JSON 直接输出保持同一语义：先解析别名，function 输出 `func(...)` 和可选返回值，普通类型输出名称和泛型实参。公开的 `xl6XapiTypeToString` 当前委托给 canonical type 模型，保证 XAPI module 模型使用规范类型字符串。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_dump.c:917` |
| 对应生成的 C 代码 | 不生成 C；生成 module 模型中的 `type`、`params[].type`、`returns[]` 字符串。 |
| 中间形态伪代码 | `canonicalTypeString(root,typeNode)` |
| 实现同等功能手写的 C 代码 | 手写接口元数据需要复用统一类型字符串规范，避免同义类型多种写法。 |
| 额外依赖的封装代码 | canonical type model。 |
