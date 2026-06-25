# P2 import 声明生成与 AST 重写细则

本文继续细化 `demo6/src/compile/xl6_imports.c` 的后半部分，覆盖 XAPI export/type 到 xlang extern 声明的生成、生成声明重新解析并合并 AST、命名空间冲突检查，以及 `pkg.symbol` 成员访问重写为内部名称。

## CGEN-IMPG-001 function export 是否可进入 extern

| 项目 | 内容 |
|---|---|
| xlang 写法 | `import pkg` 后使用 `pkg.foo()` |
| 生成规则描述 | 只有 kind 为 `function`，callingConvention 为空或 `cdecl`，varargMode 为空或 `none`，返回值不超过一个，且所有参数都不是 optional/vararg 的函数，才能生成当前阶段的 extern 声明。其他导出登记为 unsupported，访问时给出诊断。 |
| 生成器文件名和行号 | `demo6/src/compile/xl6_imports.c:842` |
| 对应生成的 C 代码 | 支持者后续生成 `extern "c_symbol" func __xl6_import_pkg_foo(...)` 并由 cgen 生成 C extern 调用。 |
| 中间形态伪代码 | `function && cdecl && no vararg && <=1 return && no optional params` |
| 实现同等功能手写的 C 代码 | 手写桥接当前也只能直接声明简单 C ABI 函数。 |
| 额外依赖的封装代码 | XAPI callable metadata。 |

## CGEN-IMPG-002 泛型参数文本输出

| 项目 | 内容 |
|---|---|
| xlang 写法 | 导入 `class Box<T>` |
| 生成规则描述 | 类型有 typeParams 时输出 `<T, U>`，参数之间使用 `, ` 分隔。无 typeParams 时不输出任何内容。 |
| 生成器文件名和行号 | `demo6/src/compile/xl6_imports.c:866` |
| 对应生成的 C 代码 | 不直接生成 C；生成 import 声明中的 `class __xl6_import_pkg_Box<T>`。 |
| 中间形态伪代码 | `if typeParamCount: emit "<" + params.join(", ") + ">"` |
| 实现同等功能手写的 C 代码 | 手写桥接声明需要保留泛型形状。 |
| 额外依赖的封装代码 | import text builder。 |

## CGEN-IMPG-003 字段声明输出并重写类型

| 项目 | 内容 |
|---|---|
| xlang 写法 | 导入 class/union 字段 |
| 生成规则描述 | 字段必须有 name 和 type。输出缩进、重写后的类型、空格、字段名和换行。类型重写会把同 XAPI 模块内的类型名替换为内部名称。 |
| 生成器文件名和行号 | `demo6/src/compile/xl6_imports.c:883` |
| 对应生成的 C 代码 | 不直接生成 C；生成 xlang 字段声明文本。 |
| 中间形态伪代码 | `emit indent + rewrite(type) + " " + field.name` |
| 实现同等功能手写的 C 代码 | 手写桥接类型声明也需要字段类型加前缀。 |
| 额外依赖的封装代码 | import type rewrite helper。 |

## CGEN-IMPG-004 union 类型声明生成

| 项目 | 内容 |
|---|---|
| xlang 写法 | XAPI type kind 为 `union` |
| 生成规则描述 | 输出 `union <internalName>`，随后按字段顺序输出字段声明，最后输出 `end`。字段类型会按 import 类型重写规则处理。 |
| 生成器文件名和行号 | `demo6/src/compile/xl6_imports.c:901` |
| 对应生成的 C 代码 | 经解析和 cgen 后会生成对应 union C 声明。 |
| 中间形态伪代码 | `union internalName; fields; end` |
| 实现同等功能手写的 C 代码 | 手写桥接声明需要给导入 union 改名并保留字段。 |
| 额外依赖的封装代码 | import generated declaration parser。 |

## CGEN-IMPG-005 class 类型声明生成与 inline union 恢复

| 项目 | 内容 |
|---|---|
| xlang 写法 | XAPI type kind 为 `class`，字段 storage 可能是 `inline-union` |
| 生成规则描述 | 输出 `class <internalName>` 加泛型参数；有 base 时输出 `extends <rewritten base>`。字段顺序输出，连续 `inline-union` 字段会包在 `union ... end` 块中，非 inline 字段会关闭已打开的 union。最后输出 class `end`。 |
| 生成器文件名和行号 | `demo6/src/compile/xl6_imports.c:919` |
| 对应生成的 C 代码 | 经解析和 cgen 后生成改名后的 class C 布局。 |
| 中间形态伪代码 | `class internal<T> extends base; fields with inline union grouping; end` |
| 实现同等功能手写的 C 代码 | 手写桥接声明需要恢复 class 继承、泛型和匿名 union 形状。 |
| 额外依赖的封装代码 | import type rewrite helper。 |

## CGEN-IMPG-006 custom 类型声明生成

| 项目 | 内容 |
|---|---|
| xlang 写法 | XAPI type kind 为 `custom` |
| 生成规则描述 | custom 导入只生成空壳轻量类型：`custom <internalName>\nend`。当前导入侧不恢复 custom 方法映射。 |
| 生成器文件名和行号 | `demo6/src/compile/xl6_imports.c:957` |
| 对应生成的 C 代码 | 经 cgen 后生成 custom handle 类型基础声明。 |
| 中间形态伪代码 | `custom internalName; end` |
| 实现同等功能手写的 C 代码 | 手写桥接可以先声明 opaque custom 类型，再显式声明函数。 |
| 额外依赖的封装代码 | import generated declaration parser。 |

## CGEN-IMPG-007 XAPI 类型声明批量生成

| 项目 | 内容 |
|---|---|
| xlang 写法 | import package 中的 class/custom/union |
| 生成规则描述 | 遍历 module types，跳过无 name 或 kind 的项。每个类型生成内部名称，再按 kind 分派为 union/class/custom 声明。未知 kind 不输出声明。 |
| 生成器文件名和行号 | `demo6/src/compile/xl6_imports.c:964` |
| 对应生成的 C 代码 | 后续 cgen 看到一组改名后的类型声明。 |
| 中间形态伪代码 | `for type: internalName = make(ns,type.name); emit by kind` |
| 实现同等功能手写的 C 代码 | 手写桥接头需要先声明导入类型。 |
| 额外依赖的封装代码 | import type declaration generator。 |

## CGEN-IMPG-008 function extern 声明生成

| 项目 | 内容 |
|---|---|
| xlang 写法 | XAPI function export 可进入 extern |
| 生成规则描述 | 输出 `extern "<cSymbol>" func <internalName>(...)`。参数按顺序输出重写后的类型和参数名；当前只支持 0 或 1 个返回值，1 个返回值输出 ` -> <type>`。 |
| 生成器文件名和行号 | `demo6/src/compile/xl6_imports.c:994` |
| 对应生成的 C 代码 | 经 cgen 后生成 C extern 原型并可直接调用 C symbol。 |
| 中间形态伪代码 | `extern "cSymbol" func internal(params) -> ret?` |
| 实现同等功能手写的 C 代码 | 手写 xlang 桥接声明需要同样写 extern 函数。 |
| 额外依赖的封装代码 | import type rewrite helper。 |

## CGEN-IMPG-009 var extern 声明生成

| 项目 | 内容 |
|---|---|
| xlang 写法 | XAPI var export |
| 生成规则描述 | 只有 kind 为 `var` 且 type 存在时输出 `extern "<cSymbol>" var <type> <internalName>`。const 导出当前不会生成 extern var 声明。 |
| 生成器文件名和行号 | `demo6/src/compile/xl6_imports.c:1024` |
| 对应生成的 C 代码 | 经 cgen 后生成 C extern 变量声明。 |
| 中间形态伪代码 | `if kind var: extern "cSymbol" var rewrittenType internalName` |
| 实现同等功能手写的 C 代码 | 手写桥接声明需要 extern 变量。 |
| 额外依赖的封装代码 | import type rewrite helper。 |

## CGEN-IMPG-010 XAPI 声明生成与符号登记

| 项目 | 内容 |
|---|---|
| xlang 写法 | `import pkg` 后可使用 `pkg.foo` |
| 生成规则描述 | 先生成所有导入类型声明，再遍历 exports。每个 export 都会登记符号；supported 为 simple C ABI function 或 var。supported 的导出生成内部 extern 声明；unsupported 的导出只登记，用于访问时报明确错误。 |
| 生成器文件名和行号 | `demo6/src/compile/xl6_imports.c:1043` |
| 对应生成的 C 代码 | supported 声明经解析后进入 AST，再由 cgen 生成 C。 |
| 中间形态伪代码 | `emit types; for export: add symbol; if supported emit extern decl` |
| 实现同等功能手写的 C 代码 | 手写桥接需要同时维护声明和符号映射。 |
| 额外依赖的封装代码 | import symbol table 和 generated declaration parser。 |

## CGEN-IMPG-011 生成声明重新解析并合并 AST

| 项目 | 内容 |
|---|---|
| xlang 写法 | import 生成的隐式声明 |
| 生成规则描述 | 若 generated 为空，直接成功。否则调用 `xl6ParseSource(generated, "<xapi-import>")` 重新解析生成文本；解析失败会先输出稳定总括诊断 `XAPI import generated invalid extern declarations`，再把解析诊断转移到当前诊断列表。解析成功后把 result.root 的 children 移动到当前 root。 |
| 生成器文件名和行号 | `demo6/src/compile/xl6_imports.c:1087` |
| 对应生成的 C 代码 | 不直接生成 C；合并后的 AST 会被后续 sem/cgen 正常处理。 |
| 中间形态伪代码 | `parse generated declarations; move children into root` |
| 实现同等功能手写的 C 代码 | 手写编译器也可直接构造 AST，当前实现选择复用 parser。 |
| 额外依赖的封装代码 | parser API。 |

## CGEN-IMPG-012 import namespace 选择

| 项目 | 内容 |
|---|---|
| xlang 写法 | `import target` 后访问 `namespace.symbol` |
| 生成规则描述 | namespace 优先取 resolver 的 packageName；其次取 XAPI module packageName；再取 module name；最后回退到 import target 或 `package`。 |
| 生成器文件名和行号 | `demo6/src/compile/xl6_imports.c:1116` |
| 对应生成的 C 代码 | 不生成 C；影响用户访问前缀和内部名称前缀。 |
| 中间形态伪代码 | `ns = resolved.packageName || module.packageName || module.name || target || package` |
| 实现同等功能手写的 C 代码 | 手写包系统需要稳定命名空间策略。 |
| 额外依赖的封装代码 | package resolver 和 XAPI module model。 |

## CGEN-IMPG-013 resolver 结果覆盖 XAPI package 身份

| 项目 | 内容 |
|---|---|
| xlang 写法 | import 解析到带版本的 package 实例 |
| 生成规则描述 | 若 resolver 提供非空 packageId，则释放 XAPI module 中的 packageName/version/abiVersion/packageId，并用 resolver 的 packageName/packageVersion/abiVersion/packageId 覆盖。这样 XAPI 身份以实际解析到的 package 实例为准。 |
| 生成器文件名和行号 | `demo6/src/compile/xl6_imports.c:1130` |
| 对应生成的 C 代码 | 不生成 C；影响 namespace、packageId 和 exportId 语义。 |
| 中间形态伪代码 | `module.package = resolved.package` |
| 实现同等功能手写的 C 代码 | 手写包管理器应以 resolver 实例身份为权威。 |
| 额外依赖的封装代码 | XAPI module model。 |

## CGEN-IMPG-014 单个 import 目标解析

| 项目 | 内容 |
|---|---|
| xlang 写法 | `import counter` |
| 生成规则描述 | 先去掉 import target 引号，再调用 package resolver。若未解析成功，当前实现视为非 package/path import，释放 target 并成功返回，不产生诊断和声明。 |
| 生成器文件名和行号 | `demo6/src/compile/xl6_imports.c:1145` |
| 对应生成的 C 代码 | 未解析 import 不生成任何 C 或隐式声明。 |
| 中间形态伪代码 | `target=unquote; resolved=resolve; if !resolved return success` |
| 实现同等功能手写的 C 代码 | 手写实现可选择把未解析 import 留给其他模块系统。 |
| 额外依赖的封装代码 | package resolver。 |

## CGEN-IMPG-015 import 缺少 XAPI 接口诊断

| 项目 | 内容 |
|---|---|
| xlang 写法 | import 解析到 package，但找不到 interface 文件 |
| 生成规则描述 | 如果 resolver 不能得到 interface path，或对应文件不存在，则诊断 `import package missing package.xapi.json interface file` 并失败。 |
| 生成器文件名和行号 | `demo6/src/compile/xl6_imports.c:1174` |
| 对应生成的 C 代码 | 不生成 C；阻断 import 展开。 |
| 中间形态伪代码 | `if no xapi path or !fileExists: diag; fail` |
| 实现同等功能手写的 C 代码 | 手写包导入也必须要求接口文件存在。 |
| 额外依赖的封装代码 | package resolver 和 XRT file exists。 |

## CGEN-IMPG-016 public namespace 冲突检查

| 项目 | 内容 |
|---|---|
| xlang 写法 | 同一源文件直接 import 两个同名 namespace |
| 生成规则描述 | 加载 XAPI 后选择 namespace。若符号表中已存在同名 namespace 且该 namespace 不是 private，则诊断 `duplicate import namespace` 并失败；否则登记 public namespace。 |
| 生成器文件名和行号 | `demo6/src/compile/xl6_imports.c:1190` |
| 对应生成的 C 代码 | 不生成 C；避免 `pkg.symbol` 解析二义性。 |
| 中间形态伪代码 | `if existing namespace public: duplicate error; else add public namespace` |
| 实现同等功能手写的 C 代码 | 手写包系统需要检测公开命名空间冲突。 |
| 额外依赖的封装代码 | import namespace table。 |

## CGEN-IMPG-017 单个 import 的完整展开顺序

| 项目 | 内容 |
|---|---|
| xlang 写法 | `import pkg` |
| 生成规则描述 | 单个 import 的成功路径为：解析 target -> 检查 XAPI -> 可选读取 manifest -> 读取 XAPI -> 覆盖 package 身份 -> 检查/登记 namespace -> 收集 manifest deps -> 收集 XAPI deps -> 收集 manifest dependency graph -> 生成隐式声明 -> 解析并合并 AST -> 释放临时资源。 |
| 生成器文件名和行号 | `demo6/src/compile/xl6_imports.c:1209` |
| 对应生成的 C 代码 | 隐式声明进入 AST 后由 cgen 生成 C。 |
| 中间形态伪代码 | `resolve/load/collect/generate/parse/merge/free` |
| 实现同等功能手写的 C 代码 | 手写工具链也需要固定 import 展开阶段顺序。 |
| 额外依赖的封装代码 | XAPI loader、package manifest、parser API。 |

## CGEN-IMPG-018 成员访问节点重写为名称

| 项目 | 内容 |
|---|---|
| xlang 写法 | `pkg.foo` |
| 生成规则描述 | 当 import 符号匹配成功时，释放 member 节点原有所有子节点和 text，把 AST kind 改为 `XL6_AST_NAME`，text 改为内部名称。 |
| 生成器文件名和行号 | `demo6/src/compile/xl6_imports.c:1235` |
| 对应生成的 C 代码 | cgen 最终看到 `__xl6_import_pkg_foo` 名称。 |
| 中间形态伪代码 | `member(pkg,foo) -> name("__xl6_import_pkg_foo")` |
| 实现同等功能手写的 C 代码 | 手写预处理器也会把命名空间访问改写为内部符号。 |
| 额外依赖的封装代码 | AST rewrite helper。 |

## CGEN-IMPG-019 namespace 符号访问重写

| 项目 | 内容 |
|---|---|
| xlang 写法 | `pkg.foo()`、`pkg.value` |
| 生成规则描述 | 递归遍历 AST。若节点是 member，receiver 是 name，则尝试按 receiver.text 和成员名查 import symbol。找到 supported 符号时重写为内部名称；找到 unsupported 符号时报 `imported XAPI symbol is not supported by current C ABI bridge`。 |
| 生成器文件名和行号 | `demo6/src/compile/xl6_imports.c:1254` |
| 对应生成的 C 代码 | supported 符号经 cgen 生成对应内部 extern 调用/访问。 |
| 中间形态伪代码 | `if member ns.name and symbol supported -> rewrite; unsupported -> diag` |
| 实现同等功能手写的 C 代码 | 手写桥接需要把包命名空间符号映射到真实 C 声明。 |
| 额外依赖的封装代码 | import symbol table。 |

## CGEN-IMPG-020 namespace 存在但符号缺失的诊断

| 项目 | 内容 |
|---|---|
| xlang 写法 | `pkg.missing`、访问传递私有依赖 `dep.foo` |
| 生成规则描述 | 如果 namespace 存在但没有对应 symbol，private namespace 诊断 `private package dependency is not exported`，public namespace 诊断 `import package has no such exported symbol`。这能区分“依赖私有不可见”和“公开包没有该导出”。 |
| 生成器文件名和行号 | `demo6/src/compile/xl6_imports.c:1276` |
| 对应生成的 C 代码 | 不生成 C；语义阶段前阻止无效 import 访问。 |
| 中间形态伪代码 | `namespace exists && symbol missing -> private/public diagnostic` |
| 实现同等功能手写的 C 代码 | 手写包系统应给出清晰的缺符号错误。 |
| 额外依赖的封装代码 | import namespace table。 |

## CGEN-IMPG-021 模块 import 展开总入口

| 项目 | 内容 |
|---|---|
| xlang 写法 | 编译含多个 import 的模块 |
| 生成规则描述 | 总入口初始化符号表，记录初始 root childCount，只处理初始顶层 import，避免新合并声明被再次当成本轮 import 扫描。所有 import 都成功后，才递归重写 namespace 引用。最后释放符号表并返回整体 ok。 |
| 生成器文件名和行号 | `demo6/src/compile/xl6_imports.c:1299` |
| 对应生成的 C 代码 | import 合并和重写完成后，后续 cgen 正常生成 C。 |
| 中间形态伪代码 | `for initial imports loadOne; if ok rewriteNamespaceRefs(root)` |
| 实现同等功能手写的 C 代码 | 手写编译器也会先完成 import 展开，再进入语义检查和代码生成。 |
| 额外依赖的封装代码 | import loader、parser API、AST rewrite helper。 |
