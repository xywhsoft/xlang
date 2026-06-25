# P2 XAPI 模块构建与 JSON 输出细则

本文继续细化 `demo6/src/abi/xl6_xapi_dump.c` 的后半部分，覆盖 class/union 形状收集、`XL6_XapiModule` 内存模型、export/type/import 模型构建，以及最终 `xlang-xapi-1` JSON 输出。

## CGEN-XMOD-001 class/union 结构节点访问

| 项目 | 内容 |
|---|---|
| xlang 写法 | `class A extends B ... end`、`union U ... end` |
| 生成规则描述 | XAPI 读取 class body、base、type params 和 union body 时按 AST 固定子节点位置访问：class child 0 是 base，child 1 是 body，child 2 是 type params；union child 0 是 body。节点 kind 或 childCount 不满足时返回 NULL。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_dump.c:985` |
| 对应生成的 C 代码 | 不生成 C；影响 XAPI type 的 base/typeParams/fields。 |
| 中间形态伪代码 | `classBase=child0; classBody=child1; classTypeParams=child2; unionBody=child0` |
| 实现同等功能手写的 C 代码 | 手写元数据需要按语言 AST 语义手动填写。 |
| 额外依赖的封装代码 | AST 结构约定。 |

## CGEN-XMOD-002 union 字段计数

| 项目 | 内容 |
|---|---|
| xlang 写法 | `union U int a string b end` |
| 生成规则描述 | union 字段计数只统计 union body 中 kind 为 `XL6_AST_FIELD` 的子节点；其他节点忽略。body 缺失时字段数为 0。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_dump.c:1017` |
| 对应生成的 C 代码 | 不生成 C；影响 XAPI type.fields 数组长度。 |
| 中间形态伪代码 | `count = number of FIELD children in union body` |
| 实现同等功能手写的 C 代码 | 手写接口元数据需要列出 union 字段。 |
| 额外依赖的封装代码 | union AST 遍历 helper。 |

## CGEN-XMOD-003 class 字段计数并展开内联 union

| 项目 | 内容 |
|---|---|
| xlang 写法 | `class A int a union int x string y end end` |
| 生成规则描述 | class 字段计数统计直接 `XL6_AST_FIELD`；若成员是 `XL6_AST_UNION`，则把该 union 内字段数加到 class 字段数中。这样匿名/内联 union 字段会进入 class 的 XAPI 字段列表。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_dump.c:1037` |
| 对应生成的 C 代码 | 不生成 C；生成 class type.fields 中包含 inline union 字段。 |
| 中间形态伪代码 | `fieldCount += direct fields; union member -> countUnionFields` |
| 实现同等功能手写的 C 代码 | 手写接口元数据需要决定是否展开匿名 union。 |
| 额外依赖的封装代码 | class/union AST 遍历 helper。 |

## CGEN-XMOD-004 单字段模型构建

| 项目 | 内容 |
|---|---|
| xlang 写法 | class 或 union 字段声明 |
| 生成规则描述 | 字段模型保存 name、canonical type string、可选 doc、storage。storage 缺省为 `field`，class 内联 union 字段传入 `inline-union`，独立 union 字段传入 `union`。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_dump.c:1062` |
| 对应生成的 C 代码 | 不生成 C；生成 `{"name":"x","type":"int","storage":"field"}`。 |
| 中间形态伪代码 | `field = { name, typeToString(type), doc?, storage }` |
| 实现同等功能手写的 C 代码 | 手写接口元数据需要记录字段来源和类型。 |
| 额外依赖的封装代码 | canonical type model。 |

## CGEN-XMOD-005 union 字段填充

| 项目 | 内容 |
|---|---|
| xlang 写法 | union 字段进入 XAPI |
| 生成规则描述 | 遍历 union body 中的 field，逐个调用字段模型构建并写入 fields 数组；index 超出预先统计的 fieldCount 或字段构建失败时返回失败。body 为空视为成功。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_dump.c:1078` |
| 对应生成的 C 代码 | 不生成 C；填充 XAPI type.fields。 |
| 中间形态伪代码 | `for union field: fields[index++] = buildField(storage)` |
| 实现同等功能手写的 C 代码 | 手写接口元数据逐个列出 union 字段。 |
| 额外依赖的封装代码 | 字段模型构建 helper。 |

## CGEN-XMOD-006 class 字段填充

| 项目 | 内容 |
|---|---|
| xlang 写法 | class 直接字段和内联 union 字段进入 XAPI |
| 生成规则描述 | class body 中直接 field 使用 storage=`field` 写入；union 成员调用 union 字段填充并使用 storage=`inline-union`。最终 index 必须等于预统计 fieldCount，否则失败。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_dump.c:1109` |
| 对应生成的 C 代码 | 不生成 C；生成 class fields 数组。 |
| 中间形态伪代码 | `direct field -> storage field; union member -> fill with inline-union` |
| 实现同等功能手写的 C 代码 | 手写接口元数据需要展开 class 中的匿名 union 字段。 |
| 额外依赖的封装代码 | 字段模型构建 helper。 |

## CGEN-XMOD-007 类型字段数组构建

| 项目 | 内容 |
|---|---|
| xlang 写法 | `class` 或 `union` 类型导出到 XAPI |
| 生成规则描述 | class 使用 class 字段计数，union 使用 union 字段计数；custom 不构建字段数组。字段数为 0 时直接成功。字段数组用 `xrtCalloc` 分配，class/union 分别调用对应填充函数。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_dump.c:1143` |
| 对应生成的 C 代码 | 不生成 C；影响 XAPI type 对象是否带 `fields`。 |
| 中间形态伪代码 | `if class/union: allocate fieldCount; fill fields` |
| 实现同等功能手写的 C 代码 | 手写接口元数据要为结构类型列字段。 |
| 额外依赖的封装代码 | 字段填充 helper。 |

## CGEN-XMOD-008 class 形状构建

| 项目 | 内容 |
|---|---|
| xlang 写法 | `class Box<T> extends Base` |
| 生成规则描述 | 只有 class 构建 class shape。若 base 名称非空，复制到 type.base；若有类型参数，分配 typeParams 数组并逐个复制参数名。custom/union 不走该规则。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_dump.c:1174` |
| 对应生成的 C 代码 | 不生成 C；生成 `"base":"Base"`、`"typeParams":["T"]`。 |
| 中间形态伪代码 | `if class: base?; typeParams[]?` |
| 实现同等功能手写的 C 代码 | 手写接口元数据需要表达继承和泛型形状。 |
| 额外依赖的封装代码 | class AST 访问 helper。 |

## CGEN-XMOD-009 XAPI module 初始化

| 项目 | 内容 |
|---|---|
| xlang 写法 | 生成一个模块的 XAPI |
| 生成规则描述 | module 初始化会把整个 `XL6_XapiModule` 清零；NULL 输入直接返回。它为后续 build/free 提供可重复调用的稳定初始状态。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_dump.c:1210` |
| 对应生成的 C 代码 | 不生成 C；初始化 XAPI module 内存模型。 |
| 中间形态伪代码 | `memset(module,0,sizeof(*module))` |
| 实现同等功能手写的 C 代码 | 手写工具也需要初始化元数据结构。 |
| 额外依赖的封装代码 | XAPI module model。 |

## CGEN-XMOD-010 XAPI module 完整释放

| 项目 | 内容 |
|---|---|
| xlang 写法 | XAPI module build 失败或输出完成 |
| 生成规则描述 | module free 释放 package/module/source 字符串、exports 的 kind/name/cSymbol/type/signature/exportId/callingConvention/varargMode/doc/params/returns、types 的 kind/name/doc/base/typeParams/fields、imports 的 target/mode/resolvedKind/paths/packageId，以及 links/loadlibs/cSources/native 字段，最后清零 module。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_dump.c:1218` |
| 对应生成的 C 代码 | 不生成 C；释放 XAPI module 内存。 |
| 中间形态伪代码 | `free every owned nested string/array; memset module zero` |
| 实现同等功能手写的 C 代码 | 手写工具需要明确嵌套元数据所有权。 |
| 额外依赖的封装代码 | XAPI module model。 |

## CGEN-XMOD-011 module 数组预分配

| 项目 | 内容 |
|---|---|
| xlang 写法 | 模块中存在 export、type、import |
| 生成规则描述 | 根据预统计数量分别为 exports、types、imports 分配数组；数量为 0 时不分配。任一需要的数组分配失败，返回失败。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_dump.c:1300` |
| 对应生成的 C 代码 | 不生成 C；为 XAPI module build 准备数组。 |
| 中间形态伪代码 | `if count>0 calloc(count,itemSize)` |
| 实现同等功能手写的 C 代码 | 手写工具常用动态数组或两遍扫描。 |
| 额外依赖的封装代码 | XAPI module model。 |

## CGEN-XMOD-012 export function 模型构建

| 项目 | 内容 |
|---|---|
| xlang 写法 | `export func f(int a = 1, ... ) int` |
| 生成规则描述 | function export 模型记录 kind/name/cSymbol/doc、callingConvention 固定 cdecl、varargMode、requiredParamCount、fixedParamCount。参数数组记录 name/type/optional/vararg；返回数组记录 canonical type string。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_dump.c:1323` |
| 对应生成的 C 代码 | 不生成 C；生成 XAPI exports function model。 |
| 中间形态伪代码 | `export = {function, cdecl, varargMode, params[], returns[]}` |
| 实现同等功能手写的 C 代码 | 手写接口元数据需要描述调用约定、可选参数和可变参数。 |
| 额外依赖的封装代码 | callable descriptor helper。 |

## CGEN-XMOD-013 export var/const 模型构建

| 项目 | 内容 |
|---|---|
| xlang 写法 | `export var int count`、`export const string name` |
| 生成规则描述 | var/const export 模型记录 kind、name、cSymbol、doc 和 canonical type string。kind 由 AST 节点区分 const 或 var。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_dump.c:1370` |
| 对应生成的 C 代码 | 不生成 C；生成 XAPI exports var/const model。 |
| 中间形态伪代码 | `export = { kind: const|var, name, cSymbol, type, doc? }` |
| 实现同等功能手写的 C 代码 | 手写接口元数据需要声明导出变量/常量类型和符号。 |
| 额外依赖的封装代码 | canonical type model。 |

## CGEN-XMOD-014 module build 初始化默认身份

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xl xapi source.xl` 或构建扩展库接口 |
| 生成规则描述 | module build 先释放旧 module 再重新初始化。默认 moduleName 为 `app`，packageName 跟随 module name，version 固定 `0.0.0`，abiVersion 固定 `1`，并构造 packageId/moduleId；source 使用 sourcePath 或空字符串，sourceHash 当前为空字符串。任一字段构造失败会释放并失败。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_dump.c:1385` |
| 对应生成的 C 代码 | 不生成 C；生成 XAPI package/module 基础字段。 |
| 中间形态伪代码 | `reset module; fill defaults; build packageId/moduleId/source/sourceHash` |
| 实现同等功能手写的 C 代码 | 手写接口元数据需要手动填写包和模块身份。 |
| 额外依赖的封装代码 | XAPI identity builder。 |

## CGEN-XMOD-015 module entry kind 构建

| 项目 | 内容 |
|---|---|
| xlang 写法 | 有 main、只有顶层语句、或库模块 |
| 生成规则描述 | module build 阶段复用 XAPI main/top-level 检测：有 main 则 entryKind 为 `XL6_XAPI_ENTRY_XLANG_MAIN`；无 main 但有顶层运行语句则为 `SCRIPT_MAIN`；否则为 `NONE`。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_dump.c:1416` |
| 对应生成的 C 代码 | 不生成 C；影响 JSON `entry.kind`。 |
| 中间形态伪代码 | `entryKind = main ? xlang-main : topLevel ? script-main : none` |
| 实现同等功能手写的 C 代码 | 手写接口元数据需要声明是否可执行。 |
| 额外依赖的封装代码 | XAPI entry detector。 |

## CGEN-XMOD-016 module build 预统计 export/type/import

| 项目 | 内容 |
|---|---|
| xlang 写法 | 顶层 export、class/custom/union、import |
| 生成规则描述 | 遍历 AST 根节点：export func/var/const 计入 exportCount；class/custom/union 计入 typeCount；import 计入 importCount。其他顶层节点不进入 XAPI module 的这三类数组。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_dump.c:1424` |
| 对应生成的 C 代码 | 不生成 C；决定 module arrays 分配大小。 |
| 中间形态伪代码 | `count exports/types/imports by child.kind` |
| 实现同等功能手写的 C 代码 | 手写工具通常也先统计或动态追加。 |
| 额外依赖的封装代码 | module array allocator。 |

## CGEN-XMOD-017 module build 填充 export 数组

| 项目 | 内容 |
|---|---|
| xlang 写法 | 顶层 export function/var/const |
| 生成规则描述 | 第二遍遍历中，export func 调用 function export 构建，export var/const 调用变量构建；每个 export 构建后立即补齐 signature/exportId。任一步失败会释放 module 并失败。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_dump.c:1456` |
| 对应生成的 C 代码 | 不生成 C；填充 XAPI exports。 |
| 中间形态伪代码 | `build export; finalize identity; exportIndex++` |
| 实现同等功能手写的 C 代码 | 手写接口元数据需要保证每个导出都有稳定身份。 |
| 额外依赖的封装代码 | export builder 和 identity builder。 |

## CGEN-XMOD-018 module build 填充 type 数组

| 项目 | 内容 |
|---|---|
| xlang 写法 | 顶层 class/custom/union |
| 生成规则描述 | class/custom/union 生成 type model：kind 为 custom/union/class，name 来自节点文本，doc 可选复制；class 额外构建 base/typeParams，class/union 构建 fields。任一步失败会释放 module 并失败。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_dump.c:1476` |
| 对应生成的 C 代码 | 不生成 C；填充 XAPI types。 |
| 中间形态伪代码 | `type = {kind,name,doc?, shape?, fields?}` |
| 实现同等功能手写的 C 代码 | 手写接口元数据需要描述类型形状。 |
| 额外依赖的封装代码 | type shape/field builder。 |

## CGEN-XMOD-019 module build 填充 import 数组并解析 package

| 项目 | 内容 |
|---|---|
| xlang 写法 | `import target` |
| 生成规则描述 | import target 先去源代码引号，mode 固定 `package-or-path`；随后调用 package resolver，把 resolved 布尔值、resolvedKind、manifestPath、interfacePath、packageId 写入 import model。任一字符串分配失败会释放 module 并失败。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_dump.c:1488` |
| 对应生成的 C 代码 | 不生成 C；生成 XAPI imports 中 resolver 结果。 |
| 中间形态伪代码 | `target=dupImportTarget; resolved=resolveImport(sourcePath,target); store paths/packageId` |
| 实现同等功能手写的 C 代码 | 手写接口元数据通常需要手动记录依赖解析结果。 |
| 额外依赖的封装代码 | package resolver。 |

## CGEN-XMOD-020 module build 最终计数提交

| 项目 | 内容 |
|---|---|
| xlang 写法 | 模块 AST 构建 XAPI 完成 |
| 生成规则描述 | 第二遍填充完成后，用实际 index 提交 exportCount、typeCount、importCount。这样即便预统计和填充路径都按同一规则，最终仍以实际成功填充数量为准。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_dump.c:1513` |
| 对应生成的 C 代码 | 不生成 C；确定最终 JSON 数组长度。 |
| 中间形态伪代码 | `module.counts = indexes; return success` |
| 实现同等功能手写的 C 代码 | 手写工具会在追加完成后记录数组长度。 |
| 额外依赖的封装代码 | XAPI module model。 |

## CGEN-XMOD-021 module 是否有可执行入口

| 项目 | 内容 |
|---|---|
| xlang 写法 | 运行或构建可执行目标时读取 XAPI |
| 生成规则描述 | 只有 entryKind 为 xlang main 或 script main 时，module 被视为有可执行入口；entry none 的模块需要显式初始化，通常作为库使用。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_dump.c:1519` |
| 对应生成的 C 代码 | 不生成 C；供工具链判断是否可执行。 |
| 中间形态伪代码 | `entry executable = kind in {XLANG_MAIN,SCRIPT_MAIN}` |
| 实现同等功能手写的 C 代码 | 手写元数据需要区分 exe 模块和库模块。 |
| 额外依赖的封装代码 | XAPI entry model。 |

## CGEN-XMOD-022 module entry JSON 输出

| 项目 | 内容 |
|---|---|
| xlang 写法 | XAPI module entry 输出 |
| 生成规则描述 | 根据 module->entryKind 输出与 AST 直接 dump 相同的 entry JSON：xlang-main、script-main 或 none；none 分支包含 `requiresExplicitInit:true`。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_dump.c:1526` |
| 对应生成的 C 代码 | 不生成 C；输出 `"entry":{...}`。 |
| 中间形态伪代码 | `switch entryKind -> entry object` |
| 实现同等功能手写的 C 代码 | 手写接口元数据需要稳定 entry schema。 |
| 额外依赖的封装代码 | XAPI entry schema。 |

## CGEN-XMOD-023 字符串数组 JSON 输出

| 项目 | 内容 |
|---|---|
| xlang 写法 | package links、loadlibs、cSources |
| 生成规则描述 | 字符串数组输出为 JSON array，按顺序输出每个字符串，元素间用逗号分隔；count 为 0 时输出空数组。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_dump.c:1540` |
| 对应生成的 C 代码 | 不生成 C；输出 `"links":[]` 等数组字段。 |
| 中间形态伪代码 | `emit "[" + jsonString(items[i]).join(",") + "]"` |
| 实现同等功能手写的 C 代码 | 手写 JSON 需要统一字符串数组输出。 |
| 额外依赖的封装代码 | JSON string helper。 |

## CGEN-XMOD-024 XAPI JSON 固定头部、package 与 module 输出

| 项目 | 内容 |
|---|---|
| xlang 写法 | 任何模块 XAPI 输出 |
| 生成规则描述 | JSON 顶层固定输出 format=`xlang-xapi-1`、compiler name/version/xlangAbi/xrtAbi、target os/arch/callingConvention；随后输出 package 的 name/version/abiVersion/packageId，以及 module 的 name/moduleId/source/sourceHash。NULL module 使用默认 app 值。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_dump.c:1554` |
| 对应生成的 C 代码 | 不生成 C；输出 XAPI JSON 头部。 |
| 中间形态伪代码 | `emit fixed metadata; emit package; emit module` |
| 实现同等功能手写的 C 代码 | 手写接口元数据需要版本化格式和目标平台字段。 |
| 额外依赖的封装代码 | XAPI JSON schema。 |

## CGEN-XMOD-025 exports JSON 输出

| 项目 | 内容 |
|---|---|
| xlang 写法 | export 函数、变量、常量 |
| 生成规则描述 | exports 数组按 module 模型顺序输出。每个 item 输出 kind/name/cSymbol，可选 signature/exportId/type/doc。若 item 是 function 或 paramCount>0，则输出 callingConvention、varargMode、requiredParamCount、fixedParamCount、params 数组和 returns 数组；参数包含 name/type/optional/vararg。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_dump.c:1587` |
| 对应生成的 C 代码 | 不生成 C；输出完整 exports JSON。 |
| 中间形态伪代码 | `for export: emit identity, type/doc, callable detail if function` |
| 实现同等功能手写的 C 代码 | 手写接口元数据需要完整描述导出调用面。 |
| 额外依赖的封装代码 | doc JSON helper 和 callable descriptor。 |

## CGEN-XMOD-026 types JSON 输出

| 项目 | 内容 |
|---|---|
| xlang 写法 | class/custom/union 类型 |
| 生成规则描述 | types 数组按 module 模型顺序输出 kind/name；class 若有 base 输出 base，若有 typeParams 输出类型参数数组；类型 doc 会输出 doc JSON；若有 fields，则每个字段输出 name/type/storage 和可选 doc。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_dump.c:1658` |
| 对应生成的 C 代码 | 不生成 C；输出完整 types JSON。 |
| 中间形态伪代码 | `for type: emit kind/name/base/typeParams/doc/fields` |
| 实现同等功能手写的 C 代码 | 手写接口元数据需要描述类型形状和字段文档。 |
| 额外依赖的封装代码 | doc JSON helper。 |

## CGEN-XMOD-027 imports JSON 输出

| 项目 | 内容 |
|---|---|
| xlang 写法 | import 声明 |
| 生成规则描述 | imports 数组按 module 模型顺序输出 target、mode、resolved、resolvedKind、manifest、interface、packageId。它记录 resolver 结果，而不只是源代码 import 文本。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_dump.c:1715` |
| 对应生成的 C 代码 | 不生成 C；输出完整 imports JSON。 |
| 中间形态伪代码 | `for import: emit source target and resolved package/interface paths` |
| 实现同等功能手写的 C 代码 | 手写接口元数据需要记录导入解析结果。 |
| 额外依赖的封装代码 | package resolver model。 |

## CGEN-XMOD-028 links/loadlibs/cSources/native/docs 输出

| 项目 | 内容 |
|---|---|
| xlang 写法 | package manifest 中的链接项、加载库、C 源码、native singleton |
| 生成规则描述 | 顶层输出 links、loadlibs、cSources 三个字符串数组；native 输出 singleton、loadName、version；docs 当前固定为空数组。NULL module 时这些字段输出空数组或默认空字符串/false。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_dump.c:1742` |
| 对应生成的 C 代码 | 不生成 C；输出 package 工具链相关 JSON 字段。 |
| 中间形态伪代码 | `emit arrays; emit native object; docs=[]` |
| 实现同等功能手写的 C 代码 | 手写接口元数据需要记录 native/package 链接信息。 |
| 额外依赖的封装代码 | XAPI package schema。 |

## CGEN-XMOD-029 公开 XAPI JSON dump 包装

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xl xapi file.xl` 或内部生成 package.xapi.json |
| 生成规则描述 | 公开入口先初始化 module；若 module build 成功，则输出完整 JSON module；最后无论成功与否都释放 module。out 为 NULL 时直接返回。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_dump.c:1762` |
| 对应生成的 C 代码 | 不生成 C；生成完整 `.xapi.json` 文件内容。 |
| 中间形态伪代码 | `init module; if build ok dump json; free module` |
| 实现同等功能手写的 C 代码 | 手写工具通常也是 build model -> serialize -> free。 |
| 额外依赖的封装代码 | XAPI module build/dump/free。 |
