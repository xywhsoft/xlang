# P2 XAPI 读取与模型恢复细则

本文细化 `demo6/src/abi/xl6_xapi_load.c`。该文件读取 `xlang-xapi-1` JSON，并恢复为 `XL6_XapiModule` 模型，供 import/package 链路消费。

## CGEN-XLOAD-001 JSON 读取状态与诊断

| 项目 | 内容 |
|---|---|
| xlang 写法 | `import` 读取外部 `.xapi.json` |
| 生成规则描述 | XAPI JSON parser 保存 path、text、len、pos 和诊断列表。解析错误统一追加到诊断列表，位置当前固定为 1:1，默认消息为 “XAPI JSON 解析失败”。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_load.c:5` |
| 对应生成的 C 代码 | 不生成 C；影响 import/XAPI 错误诊断。 |
| 中间形态伪代码 | `json = {path,text,len,pos,diags}; error(diags,message)` |
| 实现同等功能手写的 C 代码 | 手写 loader 也需要保存输入位置和错误通道。 |
| 额外依赖的封装代码 | 诊断列表。 |

## CGEN-XLOAD-002 空白跳过、peek 与 consume

| 项目 | 内容 |
|---|---|
| xlang 写法 | 读取 JSON 中的对象、数组、冒号和逗号 |
| 生成规则描述 | parser 会跳过空格、tab、CR、LF；peek 返回跳过空白后的当前字符，越界返回 0；consume 只有当前字符等于目标字符时前进一位并返回成功。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_load.c:20` |
| 对应生成的 C 代码 | 不生成 C；决定 XAPI JSON 语法读取。 |
| 中间形态伪代码 | `skipWs(); peek text[pos]; consume(ch) if text[pos]==ch` |
| 实现同等功能手写的 C 代码 | 手写 JSON parser 需要相同基础游标操作。 |
| 额外依赖的封装代码 | XAPI JSON parser。 |

## CGEN-XLOAD-003 JSON 字符串缓冲追加

| 项目 | 内容 |
|---|---|
| xlang 写法 | 读取任意 JSON 字符串字段 |
| 生成规则描述 | 字符串缓冲区容量从 32 起按 2 倍扩容，始终保留结尾 `\0`。追加失败会返回 0，由调用方释放已分配字符串并报错。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_load.c:50` |
| 对应生成的 C 代码 | 不生成 C；恢复 XAPI 字符串字段。 |
| 中间形态伪代码 | `grow if len+2>cap; out[len++]=ch; out[len]=0` |
| 实现同等功能手写的 C 代码 | 手写 parser 需要动态字符串缓冲。 |
| 额外依赖的封装代码 | JSON string parser。 |

## CGEN-XLOAD-004 JSON 字符串读取

| 项目 | 内容 |
|---|---|
| xlang 写法 | `"name"`、`"doc"`、`"type"` 等字段 |
| 生成规则描述 | 字符串必须以双引号开始。支持常见转义 `\"`、`\\`、`\/`、`\b`、`\f`、`\n`、`\r`、`\t`；`\uXXXX` 当前跳过四位并写入 `?`。空字符串返回空字符串副本。未闭合或内存不足会报错。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_load.c:76` |
| 对应生成的 C 代码 | 不生成 C；恢复 XAPI 字符串值。 |
| 中间形态伪代码 | `read until closing quote; decode escapes; unicode escape -> '?'` |
| 实现同等功能手写的 C 代码 | 手写 JSON parser 需要完整字符串解码策略。 |
| 额外依赖的封装代码 | JSON string parser。 |

## CGEN-XLOAD-005 JSON bool 与 int 读取

| 项目 | 内容 |
|---|---|
| xlang 写法 | `optional:true`、`requiredParamCount:2` |
| 生成规则描述 | bool 只接受字面量 `true` 或 `false`；int 支持可选负号和十进制数字，至少要有一位数字。读取失败分别报 “需要布尔值” 或 “需要整数”。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_load.c:133` |
| 对应生成的 C 代码 | 不生成 C；恢复布尔和整数元数据字段。 |
| 中间形态伪代码 | `read true/false; read optional '-' and digits` |
| 实现同等功能手写的 C 代码 | 手写 loader 需要按 schema 读取标量。 |
| 额外依赖的封装代码 | XAPI JSON parser。 |

## CGEN-XLOAD-006 未知 JSON value 跳过

| 项目 | 内容 |
|---|---|
| xlang 写法 | XAPI 中存在当前 loader 不认识的扩展字段 |
| 生成规则描述 | loader 可跳过未知字符串、对象、数组、bool、null 和整数。对象/数组递归跳过元素，并要求逗号分隔正确。该规则让 XAPI 具备向前兼容能力。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_load.c:183` |
| 对应生成的 C 代码 | 不生成 C；未知字段不会阻断读取。 |
| 中间形态伪代码 | `skip value by first char; recursively skip object/array` |
| 实现同等功能手写的 C 代码 | 手写元数据读取器常会忽略未知字段以便格式演进。 |
| 额外依赖的封装代码 | JSON skip parser。 |

## CGEN-XLOAD-007 param/return/field 动态追加

| 项目 | 内容 |
|---|---|
| xlang 写法 | XAPI exports.params、exports.returns、types.fields |
| 生成规则描述 | param、return、field 都用 `xrtRealloc` 按 count+1 扩展数组。param/field 追加成功后把临时对象所有权转移进数组并清零临时对象；return 直接接管 type 字符串。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_load.c:260` |
| 对应生成的 C 代码 | 不生成 C；恢复 module 模型中的数组字段。 |
| 中间形态伪代码 | `realloc array(count+1); array[count]=item; count++` |
| 实现同等功能手写的 C 代码 | 手写 loader 需要管理数组扩容和所有权转移。 |
| 额外依赖的封装代码 | XAPI module model。 |

## CGEN-XLOAD-008 字符串数组读取

| 项目 | 内容 |
|---|---|
| xlang 写法 | XAPI `links`、`loadlibs`、`cSources`、`typeParams` |
| 生成规则描述 | 字符串数组必须以 `[` 开始，允许空数组。每个元素必须是 JSON 字符串，读取后追加到字符串数组，元素之间必须用逗号。追加失败会释放当前字符串并失败。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_load.c:319` |
| 对应生成的 C 代码 | 不生成 C；恢复字符串数组字段。 |
| 中间形态伪代码 | `[` string (',' string)* `]` |
| 实现同等功能手写的 C 代码 | 手写 loader 需要数组读取循环。 |
| 额外依赖的封装代码 | XAPI JSON parser。 |

## CGEN-XLOAD-009 doc 值读取

| 项目 | 内容 |
|---|---|
| xlang 写法 | XAPI export/type/field 的 `doc` 字段 |
| 生成规则描述 | doc 字段必须是对象，并且必须包含 `raw` 字符串字段；其他字段按前向演进策略跳过。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_load.c:359` |
| 对应生成的 C 代码 | 不生成 C；恢复 doc 原文。 |
| 中间形态伪代码 | `doc = object.raw; skip unknown fields; missing raw -> error` |
| 实现同等功能手写的 C 代码 | 手写 loader 只消费当前机器生成的 raw 文档，忽略派生 fields。 |
| 额外依赖的封装代码 | XAPI doc schema。 |

## CGEN-XLOAD-010 参数对象读取

| 项目 | 内容 |
|---|---|
| xlang 写法 | XAPI param `{name,type,optional,vararg}` |
| 生成规则描述 | param 必须是对象。已知字段包括 name、type、optional、vararg；未知字段跳过。optional/vararg 必须是 bool。对象内字段之间必须用逗号。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_load.c:402` |
| 对应生成的 C 代码 | 不生成 C；恢复导出函数参数元数据。 |
| 中间形态伪代码 | `parse {name?,type?,optional?,vararg?, ...}` |
| 实现同等功能手写的 C 代码 | 手写 loader 按 schema 读取参数对象。 |
| 额外依赖的封装代码 | XAPI export model。 |

## CGEN-XLOAD-011 参数数组读取

| 项目 | 内容 |
|---|---|
| xlang 写法 | XAPI export `params` |
| 生成规则描述 | params 必须是数组。每个元素读取为临时 param 对象，再追加到 export；失败时释放临时 name/type。元素之间必须用逗号。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_load.c:448` |
| 对应生成的 C 代码 | 不生成 C；恢复 `item->params`。 |
| 中间形态伪代码 | `for param object in array: appendParam` |
| 实现同等功能手写的 C 代码 | 手写 loader 需要按顺序恢复参数。 |
| 额外依赖的封装代码 | XAPI export model。 |

## CGEN-XLOAD-012 返回值对象与数组读取

| 项目 | 内容 |
|---|---|
| xlang 写法 | XAPI export `returns` |
| 生成规则描述 | return object 只消费 `type` 字段，未知字段跳过。对象结束时若 type 非 NULL 则追加到 export；空对象不会追加。returns 数组按顺序读取多个 return object。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_load.c:474` |
| 对应生成的 C 代码 | 不生成 C；恢复多返回类型列表。 |
| 中间形态伪代码 | `return object -> optional type; append if present` |
| 实现同等功能手写的 C 代码 | 手写 loader 需要保留返回值顺序。 |
| 额外依赖的封装代码 | XAPI export model。 |

## CGEN-XLOAD-013 export 对象读取

| 项目 | 内容 |
|---|---|
| xlang 写法 | XAPI exports 数组元素 |
| 生成规则描述 | export 对象消费 kind、name、cSymbol、type、signature、exportId、doc、callingConvention、varargMode、requiredParamCount、fixedParamCount、params、returns。未知字段跳过。数值和布尔字段类型不匹配时失败。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_load.c:536` |
| 对应生成的 C 代码 | 不生成 C；恢复导出函数/变量/常量模型。 |
| 中间形态伪代码 | `parse known export fields; skip unknown` |
| 实现同等功能手写的 C 代码 | 手写 loader 需要消费导出模型 schema。 |
| 额外依赖的封装代码 | XAPI export model。 |

## CGEN-XLOAD-014 exports 数组读取

| 项目 | 内容 |
|---|---|
| xlang 写法 | XAPI 顶层 `exports` |
| 生成规则描述 | exports 数组每读一个元素，先扩展 module exports 数组并清零新槽，再解析 export object 到该槽。元素之间必须用逗号。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_load.c:605` |
| 对应生成的 C 代码 | 不生成 C；恢复 module->exports。 |
| 中间形态伪代码 | `append empty export slot; parse object into slot` |
| 实现同等功能手写的 C 代码 | 手写 loader 需要动态追加导出项。 |
| 额外依赖的封装代码 | XAPI module model。 |

## CGEN-XLOAD-015 类型字段对象读取

| 项目 | 内容 |
|---|---|
| xlang 写法 | XAPI type field `{name,type,storage,doc}` |
| 生成规则描述 | type field 对象消费 name、type、storage、doc；未知字段跳过。storage 是当前 XAPI 格式的必需字段，缺失会在模型收口阶段判定为格式错误。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_load.c:645` |
| 对应生成的 C 代码 | 不生成 C；恢复类型字段模型。 |
| 中间形态伪代码 | `require field.storage` |
| 实现同等功能手写的 C 代码 | 手写 loader 需要按当前 schema 校验字段完整性。 |
| 额外依赖的封装代码 | XAPI type model。 |

## CGEN-XLOAD-016 类型字段数组读取

| 项目 | 内容 |
|---|---|
| xlang 写法 | XAPI type `fields` |
| 生成规则描述 | fields 数组逐个读取 type field object，追加成功后转移字段所有权；失败时释放临时 name/type/doc/storage。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_load.c:693` |
| 对应生成的 C 代码 | 不生成 C；恢复 type->fields。 |
| 中间形态伪代码 | `for field object: parse; appendField; cleanup temp on fail` |
| 实现同等功能手写的 C 代码 | 手写 loader 需要恢复字段列表。 |
| 额外依赖的封装代码 | XAPI type model。 |

## CGEN-XLOAD-017 type 对象读取

| 项目 | 内容 |
|---|---|
| xlang 写法 | XAPI types 数组元素 |
| 生成规则描述 | type 对象消费 kind、name、doc、base、typeParams、fields；未知字段跳过。typeParams 使用字符串数组读取，fields 使用字段数组读取。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_load.c:720` |
| 对应生成的 C 代码 | 不生成 C；恢复 class/custom/union 元数据。 |
| 中间形态伪代码 | `parse known type fields; skip unknown` |
| 实现同等功能手写的 C 代码 | 手写 loader 需要恢复类型声明形状。 |
| 额外依赖的封装代码 | XAPI type model。 |

## CGEN-XLOAD-018 types 数组读取

| 项目 | 内容 |
|---|---|
| xlang 写法 | XAPI 顶层 `types` |
| 生成规则描述 | types 数组每读一个元素，先扩展 module types 数组并清零新槽，再解析 type object 到该槽。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_load.c:772` |
| 对应生成的 C 代码 | 不生成 C；恢复 module->types。 |
| 中间形态伪代码 | `append empty type slot; parse object into slot` |
| 实现同等功能手写的 C 代码 | 手写 loader 需要动态追加类型项。 |
| 额外依赖的封装代码 | XAPI module model。 |

## CGEN-XLOAD-019 package 对象读取

| 项目 | 内容 |
|---|---|
| xlang 写法 | XAPI 顶层 `package` |
| 生成规则描述 | package 对象消费 name、version、abiVersion、packageId；未知字段跳过。这些字段属于当前 XAPI 格式的必需身份信息，缺失会在 finalize 阶段判定为格式错误。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_load.c:812` |
| 对应生成的 C 代码 | 不生成 C；恢复 package identity。 |
| 中间形态伪代码 | `parse package known strings; skip unknown` |
| 实现同等功能手写的 C 代码 | 手写 loader 需要读取包身份字段。 |
| 额外依赖的封装代码 | XAPI package model。 |

## CGEN-XLOAD-020 module 对象读取

| 项目 | 内容 |
|---|---|
| xlang 写法 | XAPI 顶层 `module` |
| 生成规则描述 | module 对象消费 name、moduleId、source、sourceHash；未知字段跳过。这些字段属于当前 XAPI 格式的必需身份信息，缺失会在 finalize 阶段判定为格式错误。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_load.c:848` |
| 对应生成的 C 代码 | 不生成 C；恢复 module identity。 |
| 中间形态伪代码 | `parse module known strings; skip unknown` |
| 实现同等功能手写的 C 代码 | 手写 loader 需要读取模块身份字段。 |
| 额外依赖的封装代码 | XAPI module model。 |

## CGEN-XLOAD-021 native 对象读取

| 项目 | 内容 |
|---|---|
| xlang 写法 | XAPI 顶层 `native` |
| 生成规则描述 | native 对象消费 singleton、loadName、version；singleton 必须是 bool，未知字段跳过。loadName/version 是当前 XAPI 格式的必需字段，缺失会在 finalize 阶段判定为格式错误。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_load.c:884` |
| 对应生成的 C 代码 | 不生成 C；恢复 native package 元数据。 |
| 中间形态伪代码 | `native = { singleton?, loadName?, version? }` |
| 实现同等功能手写的 C 代码 | 手写 loader 需要读取 native 装载信息。 |
| 额外依赖的封装代码 | XAPI native model。 |

## CGEN-XLOAD-022 XAPI 根对象读取与 format 校验

| 项目 | 内容 |
|---|---|
| xlang 写法 | 读取完整 `.xapi.json` |
| 生成规则描述 | 根必须是对象。只要遇到 format 字段，就要求值为 `xlang-xapi-1` 并设置 sawFormat。根对象支持 package/module/exports/types/links/loadlibs/cSources/native；其他字段跳过。对象结束时若没有正确 format，报 “XAPI format 不是 xlang-xapi-1”。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_load.c:921` |
| 对应生成的 C 代码 | 不生成 C；决定 XAPI 文件是否可被 import 消费。 |
| 中间形态伪代码 | `parse root; require format == xlang-xapi-1; skip unknown fields` |
| 实现同等功能手写的 C 代码 | 手写 loader 应校验格式版本，避免误读。 |
| 额外依赖的封装代码 | XAPI JSON parser。 |

## CGEN-XLOAD-023 读取侧 packageId 与 moduleId 必需字段校验

| 项目 | 内容 |
|---|---|
| xlang 写法 | XAPI 必须显式提供 packageId 和 moduleId |
| 生成规则描述 | 读取侧不再根据 name/version/abiVersion/moduleName 构造身份字段。packageId 或 moduleId 缺失会在 finalize 阶段判定为格式错误。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_load.c:998` |
| 对应生成的 C 代码 | 不生成 C；校验 module/package identity。 |
| 中间形态伪代码 | `require packageId; require moduleId` |
| 实现同等功能手写的 C 代码 | 手写 loader 应消费机器生成的身份字段，而不是恢复残缺接口文件。 |
| 额外依赖的封装代码 | XAPI module model。 |

## CGEN-XLOAD-024 读取侧不构造身份字符串

| 项目 | 内容 |
|---|---|
| xlang 写法 | XAPI signature/exportId 由 dump 侧机器生成 |
| 生成规则描述 | 读取侧已经删除动态字符串拼接和身份构造 helper。signature、exportId、packageId、moduleId 都必须来自当前 XAPI 文件本身。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_load.c:998` |
| 对应生成的 C 代码 | 不生成 C；读取侧不产生派生身份字符串。 |
| 中间形态伪代码 | `read fields; finalize requires fields` |
| 实现同等功能手写的 C 代码 | 手写 loader 不需要为当前格式拼接身份字符串。 |
| 额外依赖的封装代码 | XAPI module model。 |

## CGEN-XLOAD-025 读取侧导出签名必需字段校验

| 项目 | 内容 |
|---|---|
| xlang 写法 | XAPI export 必须显式提供 signature |
| 生成规则描述 | loader 只读取当前 XAPI 格式中已经存在的 signature。signature 缺失会在 finalize 阶段判定为格式错误，不再根据参数和返回列表补构造。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_load.c:1004` |
| 对应生成的 C 代码 | 不生成 C；校验 export.signature。 |
| 中间形态伪代码 | `require export.signature` |
| 实现同等功能手写的 C 代码 | 手写 loader 应按 schema 校验机器生成的签名字段。 |
| 额外依赖的封装代码 | XAPI module model。 |

## CGEN-XLOAD-026 读取侧 exportId 必需字段校验

| 项目 | 内容 |
|---|---|
| xlang 写法 | XAPI export 必须显式提供 exportId |
| 生成规则描述 | loader 只读取当前 XAPI 格式中已经存在的 exportId。exportId 缺失会在 finalize 阶段判定为格式错误，不再根据 moduleId、name 和 signature 补构造。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_load.c:1004` |
| 对应生成的 C 代码 | 不生成 C；校验 export.exportId。 |
| 中间形态伪代码 | `require export.exportId` |
| 实现同等功能手写的 C 代码 | 手写 loader 应消费机器生成的稳定导出身份。 |
| 额外依赖的封装代码 | XAPI module model。 |

## CGEN-XLOAD-027 module finalize 必需字段校验

| 项目 | 内容 |
|---|---|
| xlang 写法 | 读取当前 XAPI 格式的 package/module/native 字段 |
| 生成规则描述 | finalize 只校验 module name/source/sourceHash、packageName/version/abiVersion/packageId、moduleId、nativeLoadName/nativeVersion 是否完整。任一必需字段缺失都会失败，不做默认值补齐。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_load.c:998` |
| 对应生成的 C 代码 | 不生成 C；保证读取后的 module 模型字段来自当前格式。 |
| 中间形态伪代码 | `require module/package/native identity fields` |
| 实现同等功能手写的 C 代码 | 手写 loader 应在 parse 后做 schema 必需字段校验。 |
| 额外依赖的封装代码 | XAPI module model。 |

## CGEN-XLOAD-028 function export 调用约定必需字段校验

| 项目 | 内容 |
|---|---|
| xlang 写法 | 读取当前 XAPI 中的 function export ABI 字段 |
| 生成规则描述 | finalize 遍历 exports。function kind 必须已经包含 callingConvention、varargMode、signature 和 exportId，任一缺失都会失败；loader 不再补 `cdecl`、`none` 或导出身份字段。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_load.c:1004` |
| 对应生成的 C 代码 | 不生成 C；保证 import 消费 function export 时有显式 ABI 信息。 |
| 中间形态伪代码 | `if function: require convention, varargMode, signature, exportId` |
| 实现同等功能手写的 C 代码 | 手写 loader 应拒绝残缺 function export 元数据。 |
| 额外依赖的封装代码 | callable ABI model。 |

## CGEN-XLOAD-029 XAPI 文件读取

| 项目 | 内容 |
|---|---|
| xlang 写法 | `import` 解析到某个 `.xapi.json` 文件 |
| 生成规则描述 | 公开入口要求 module 和 path 非 NULL。读取前清除 XRT error，使用 `xrtFileReadAll(path, XRT_CP_UTF8, &len)` 读取 UTF-8 文本；若 XRT error 非空或 text 为 NULL，释放 text 并向诊断追加 “无法读取 XAPI 文件”。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_load.c:1030` |
| 对应生成的 C 代码 | 不生成 C；读取 XAPI 文件内容。 |
| 中间形态伪代码 | `text = xrtFileReadAll(path, UTF8); fail -> diag` |
| 实现同等功能手写的 C 代码 | 手写 loader 应使用 UTF-8 安全文件读取。 |
| 额外依赖的封装代码 | XRT 文件读取和诊断列表。 |

## CGEN-XLOAD-030 XAPI root 解析与尾部校验

| 项目 | 内容 |
|---|---|
| xlang 写法 | 读取完整 XAPI 文件 |
| 生成规则描述 | 读取成功后先释放并初始化 module，再设置 JSON parser 状态。root parse 成功后跳过尾部空白，若 pos 不等于 len，报 “XAPI JSON 末尾存在多余字符” 并失败。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_load.c:1058` |
| 对应生成的 C 代码 | 不生成 C；保证 XAPI 文件没有尾随垃圾。 |
| 中间形态伪代码 | `parseRoot; skipWs; require pos == len` |
| 实现同等功能手写的 C 代码 | 手写 parser 通常也要检查是否完整消费输入。 |
| 额外依赖的封装代码 | XAPI JSON parser。 |

## CGEN-XLOAD-031 XAPI finalize 与失败清理

| 项目 | 内容 |
|---|---|
| xlang 写法 | XAPI 文件解析完成 |
| 生成规则描述 | root parse 和尾部校验成功后调用 finalize 校验当前 XAPI 必需字段。finalize 失败时报 “XAPI metadata finalize failed”。无论成功失败都释放文件文本；失败时释放 module，成功时保留 module 给调用方。 |
| 生成器文件名和行号 | `demo6/src/abi/xl6_xapi_load.c:1066` |
| 对应生成的 C 代码 | 不生成 C；控制 import 是否能继续消费 module。 |
| 中间形态伪代码 | `if ok && finalize: keep module else free module` |
| 实现同等功能手写的 C 代码 | 手写 loader 需要在错误路径释放半成品模型。 |
| 额外依赖的封装代码 | XAPI module free/finalize。 |
