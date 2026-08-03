# P2 import 声明合并与 AST 重写细则

本文细化 `demo6/src/compile/xl6_imports.c` 的模块源码展开路径。当前实现直接解析被导入的 `.xl` 源码，将公开声明登记到模块符号表，对模块内全部声明做唯一改名后合并 AST；不再生成一份简化 extern 文本作为 XLang 接口真相。

## CGEN-IMPG-001 限定名称拆分

| 项目 | 内容 |
|---|---|
| xlang 写法 | `pkg.sub.Type`、`pkg.value` |
| 生成规则描述 | member AST 被拆成由名称组成的点分段；只有完整的名称链才参与 import namespace 匹配，普通对象成员表达式不会被当作模块名。 |
| 生成器文件名和行号 | `demo6/src/compile/xl6_imports.c:891` |
| 对应生成的 C 代码 | 不直接生成 C；成功匹配后整条限定名称会被替换为唯一内部名称。 |
| 中间形态伪代码 | `member(member(name("pkg"),"sub"),"Type") -> ["pkg","sub","Type"]` |
| 实现同等功能手写的 C 代码 | C 通常直接使用头文件中带前缀的平坦符号名。 |
| 额外依赖的封装代码 | AST member 模型。 |

## CGEN-IMPG-002 点分名称连接

| 项目 | 内容 |
|---|---|
| xlang 写法 | `pkg.sub.Type` |
| 生成规则描述 | 把限定名称的连续分段按 `.` 连接，用于尝试最长 namespace 前缀；分配失败立即停止 import 展开。 |
| 生成器文件名和行号 | `demo6/src/compile/xl6_imports.c:919` |
| 对应生成的 C 代码 | 不直接生成 C。 |
| 中间形态伪代码 | `join(["pkg","sub"], ".") -> "pkg.sub"` |
| 实现同等功能手写的 C 代码 | 手写 C 不需要此语法层步骤。 |
| 额外依赖的封装代码 | XRT 内存管理。 |

## CGEN-IMPG-003 模块公开声明内部名称

| 项目 | 内容 |
|---|---|
| xlang 写法 | `export class Point ... end`、`export func make()` |
| 生成规则描述 | 模块顶层声明统一改名为 `__xl6_import_<namespace>_<name>`；namespace 和名称中的非 C 标识符字符会稳定替换为 `_`。同一声明无论通过原 namespace、namespace 别名还是选择性导入访问，都指向这一内部名称。 |
| 生成器文件名和行号 | `demo6/src/compile/xl6_imports.c:948` |
| 对应生成的 C 代码 | 例如 `module.Point` 最终由常规 cgen 生成为内部类型 `__xl6_import_module_Point`。 |
| 中间形态伪代码 | `internal(ns,name) = "__xl6_import_" + sanitize(ns) + "_" + sanitize(name)` |
| 实现同等功能手写的 C 代码 | 手写 C 可用 `module_Point` 一类前缀约定。 |
| 额外依赖的封装代码 | import 名称编码规则。 |

## CGEN-IMPG-004 模块私有声明内部名称

| 项目 | 内容 |
|---|---|
| xlang 写法 | 模块源码中的未 export class、func、var、const |
| 生成规则描述 | 私有顶层声明同样必须改名，避免多个包的实现细节发生链接冲突；若公开内部名已被合并依赖占用，则使用带 `_local_` 的内部名称。私有声明不会进入用户可见符号表。 |
| 生成器文件名和行号 | `demo6/src/compile/xl6_imports.c:970` |
| 对应生成的 C 代码 | 例如私有 `helper` 生成包私有前缀名称，不暴露 `pkg.helper`。 |
| 中间形态伪代码 | `privateInternal = "__xl6_import_" + ns + "_local_" + name` |
| 实现同等功能手写的 C 代码 | C 模块常用 `static` 或私有前缀；当前合并单元使用稳定私有前缀。 |
| 额外依赖的封装代码 | import 名称编码规则。 |

## CGEN-IMPG-005 已导入内部名称反解

| 项目 | 内容 |
|---|---|
| xlang 写法 | 被导入模块自身又 `import dep`，并在实现中使用 `dep.Type` |
| 生成规则描述 | 对已带 `__xl6_import_` 前缀的声明识别其原公开短名，保留依赖模块已有内部身份，不按当前模块再次改名；`_local_` 私有声明不能反解为公开名称。 |
| 生成器文件名和行号 | `demo6/src/compile/xl6_imports.c:992` |
| 对应生成的 C 代码 | 传递依赖只保留一份内部声明和一份 C 符号。 |
| 中间形态伪代码 | `publicNameFromInternal(ns, internal); reject "_local_"` |
| 实现同等功能手写的 C 代码 | C 构建系统通过 include guard 和链接去重避免重复定义。 |
| 额外依赖的封装代码 | package namespace 身份。 |

## CGEN-IMPG-006 可改名声明与名称写回

| 项目 | 内容 |
|---|---|
| xlang 写法 | 顶层 func、class、union、typedef、extern type、var、const |
| 生成规则描述 | 根据 AST kind 从节点文本或函数名称子节点读取声明名，并在改名后写回同一位置。方法、局部声明和匿名表达式不作为模块顶层符号改名入口。 |
| 生成器文件名和行号 | `demo6/src/compile/xl6_imports.c:1041` |
| 对应生成的 C 代码 | 后续 sem/cgen 只看到改名后的声明。 |
| 中间形态伪代码 | `declName(node); if renameable setDeclName(node, internal)` |
| 实现同等功能手写的 C 代码 | 相当于给所有文件级 C 声明加稳定 package 前缀。 |
| 额外依赖的封装代码 | AST 声明布局。 |

## CGEN-IMPG-007 模块公开符号种类

| 项目 | 内容 |
|---|---|
| xlang 写法 | `export func`、`export extern "c_name" func`、`export var`、`export extern "c_name" Type var`、`export const`、`export class`、`export union`、`export typedef`、`export extern type` |
| 生成规则描述 | xlang 函数、变量和常量使用各自 export AST kind；直接映射的 extern 函数/变量以及 class、具名 union、typedef、extern type 使用 PUBLIC 标志。仅这些声明进入模块公开符号表。DEMO6 当前没有 enum 语句，因此不虚构 enum 导出；未来增加 enum 时应在该统一公开声明判定中扩展。 |
| 生成器文件名和行号 | `demo6/src/compile/xl6_imports.c:1135` |
| 对应生成的 C 代码 | export 控制 XLang 模块可见性；不会自动生成额外 C ABI wrapper 或导出成员方法。 |
| 中间形态伪代码 | `public = export-func/var/const || public-flag(extern-func/extern-var/class/union/typedef/extern-type)` |
| 实现同等功能手写的 C 代码 | C 的公开面通常由头文件声明和链接器导出表共同决定。 |
| 额外依赖的封装代码 | AST PUBLIC 标志；C ABI 导出仍由独立 ABI/XAPI 规则控制。 |

## CGEN-IMPG-008 重复传递声明去重

| 项目 | 内容 |
|---|---|
| xlang 写法 | A、B 都依赖同一 package C |
| 生成规则描述 | 合并源码前，若待合并声明已经是 import 内部名称且目标 AST 已有同名声明，则释放重复节点；用户自然声明不参与这条内部去重规则。 |
| 生成器文件名和行号 | `demo6/src/compile/xl6_imports.c:1169` |
| 对应生成的 C 代码 | 同一依赖声明只生成一次 C 定义。 |
| 中间形态伪代码 | `if internalName && destination.has(name): drop duplicate` |
| 实现同等功能手写的 C 代码 | 相当于 include guard 加依赖图去重。 |
| 额外依赖的封装代码 | AST 所有权转移。 |

## CGEN-IMPG-009 直接导入名称的词法作用域

| 项目 | 内容 |
|---|---|
| xlang 写法 | `from pkg import value` 后在函数中声明同名参数或局部变量 |
| 生成规则描述 | 直接名称映射维护块级 scope；参数、局部 var/const、多变量声明和 for 目标会进入当前作用域。局部名称优先，不会被选择性导入符号错误替换。 |
| 生成器文件名和行号 | `demo6/src/compile/xl6_imports.c:1254` |
| 对应生成的 C 代码 | 局部 C 标识符保持局部身份，未遮蔽引用才生成导入内部符号。 |
| 中间形态伪代码 | `if name in lexicalScope: keep; else apply directImportMap` |
| 实现同等功能手写的 C 代码 | 与 C 的局部变量遮蔽文件级声明一致。 |
| 额外依赖的封装代码 | package string list 作为临时 scope 集合。 |

## CGEN-IMPG-010 直接名称与类型名称重写

| 项目 | 内容 |
|---|---|
| xlang 写法 | `from pkg import Point as P`、`P p`、`func(P) as P` |
| 生成规则描述 | 递归重写 NAME 与 TYPE 节点，支持映射名称后的泛型/限定后缀。函数、属性、构造、匿名函数和嵌套块分别建立正确作用域；import 节点自身不重写。 |
| 生成器文件名和行号 | `demo6/src/compile/xl6_imports.c:1330` |
| 对应生成的 C 代码 | 所有类型位置和表达式位置最终引用同一内部声明。 |
| 中间形态伪代码 | `P -> __xl6_import_pkg_Point; P.Inner -> __xl6_import_pkg_Point.Inner` |
| 实现同等功能手写的 C 代码 | 相当于预处理阶段将局部别名解析到真实 C typedef/符号。 |
| 额外依赖的封装代码 | scope-aware AST walker。 |

## CGEN-IMPG-011 源模块统一准备

| 项目 | 内容 |
|---|---|
| xlang 写法 | 导入任意 package `.xl` 或相对 `.xl` 文件 |
| 生成规则描述 | 第一遍为模块中所有可改名顶层声明建立 oldName 到 internalName 映射并写回声明；公开声明同时登记符号 kind。第二遍按映射重写字段类型、继承、泛型约束、函数签名、函数体及初始化器中的引用。 |
| 生成器文件名和行号 | `demo6/src/compile/xl6_imports.c:1570` |
| 对应生成的 C 代码 | 合并后的 AST 走与主文件相同的 sem/cgen 路径，不需要一套简化接口代码生成器。 |
| 中间形态伪代码 | `collect+rename declarations; register exports; rewrite all module references` |
| 实现同等功能手写的 C 代码 | 相当于给独立 C 模块做符号前缀化并生成一致头文件。 |
| 额外依赖的封装代码 | 名称映射、作用域重写、公开符号表。 |

## CGEN-IMPG-012 导入符号登记与类型身份

| 项目 | 内容 |
|---|---|
| xlang 写法 | `pkg.Point` 与 `from pkg import Point as P` 同时出现 |
| 生成规则描述 | symbol 保存 namespace、公开名、唯一 internalName、AST kind 和 supported。重复 namespace/name 复用首次登记；别名不会创建新类型声明，所以 `pkg.Point` 与 `P` 的类型身份相同。 |
| 生成器文件名和行号 | `demo6/src/compile/xl6_imports.c:1669` |
| 对应生成的 C 代码 | 两种写法都落到同一 C 类型或符号名。 |
| 中间形态伪代码 | `identity(pkg.Point) == identity(P) == internalName` |
| 实现同等功能手写的 C 代码 | C 的多个 typedef 别名最终也可指向同一底层声明。 |
| 额外依赖的封装代码 | import symbol table。 |

## CGEN-IMPG-013 命名空间别名发布

| 项目 | 内容 |
|---|---|
| xlang 写法 | `import long_package as lp` |
| 生成规则描述 | 别名建立新的公开 namespace 视图，并复制源 namespace 的公开 symbol 记录；复制项继续引用原 internalName、kind 和 supported。指定别名后 canonical namespace 不向用户公开，除非另有普通 import。 |
| 生成器文件名和行号 | `demo6/src/compile/xl6_imports.c:1709` |
| 对应生成的 C 代码 | `lp.Point` 与源模块内部声明共用同一 C 名称，不生成副本。 |
| 中间形态伪代码 | `for symbol in sourceNs: publish(alias, symbol.name, same identity)` |
| 实现同等功能手写的 C 代码 | 相当于仅在编译器符号表中增加命名空间别名。 |
| 额外依赖的封装代码 | namespace table。 |

## CGEN-IMPG-014 选择性导入绑定

| 项目 | 内容 |
|---|---|
| xlang 写法 | `from pkg import Point, make as create` |
| 生成规则描述 | 每个选择项必须是模块公开符号；本地名不得与已有直接导入、公开 namespace、内置类型或当前文件自然顶层声明冲突。成功后只加入本地名称映射，不公开整个 package namespace，不支持通配符。内置 feature module 不接受选择性导入。 |
| 生成器文件名和行号 | `demo6/src/compile/xl6_imports.c:1747` |
| 对应生成的 C 代码 | `Point`、`create()` 分别重写为对应唯一内部类型和函数名。 |
| 中间形态伪代码 | `for selected: require exported; require no collision; direct[alias||name]=internal` |
| 实现同等功能手写的 C 代码 | 相当于在当前 C 文件建立有限的 typedef/宏别名，但由编译器完成作用域处理。 |
| 额外依赖的封装代码 | 公开符号表、内置类型注册表。 |

## CGEN-IMPG-015 导入声明插入顺序

| 项目 | 内容 |
|---|---|
| xlang 写法 | 文件先写 import，随后使用导入类型声明全局变量或 class |
| 生成规则描述 | 被导入模块的 AST 子节点先追加，再整体移动到原文件正文之前、初始 import 节点之后，确保 sem 看到使用点之前已有完整类型与函数声明。移动只转移节点指针，不复制声明对象。 |
| 生成器文件名和行号 | `demo6/src/compile/xl6_imports.c:1811` |
| 对应生成的 C 代码 | 导入类型和函数定义在使用它们的 C 代码之前生成。 |
| 中间形态伪代码 | `[imports, body, appendedModules] -> [imports, appendedModules, body]` |
| 实现同等功能手写的 C 代码 | 对应把依赖头文件/实现声明放在主文件使用点之前。 |
| 额外依赖的封装代码 | AST child 所有权移动。 |

## CGEN-IMPG-016 package 源码逐文件合并

| 项目 | 内容 |
|---|---|
| xlang 写法 | package manifest 的 `xlSources` 包含多个文件 |
| 生成规则描述 | 每个 manifest 相对路径经 package 目录解析并检查存在，随后 parse、递归展开其 import、复制诊断并移动 AST。缺文件、解析失败或递归 import 失败都会阻断当前导入。 |
| 生成器文件名和行号 | `demo6/src/compile/xl6_imports.c:1867` |
| 对应生成的 C 代码 | 多个 package 源文件最终进入同一编译单元的常规 C 输出。 |
| 中间形态伪代码 | `for xlSource: resolve -> parse -> expand imports -> move AST` |
| 实现同等功能手写的 C 代码 | 相当于构建系统逐个编译/合并包源文件并传播诊断。 |
| 额外依赖的封装代码 | package manifest、parser API、递归 import。 |

## CGEN-IMPG-017 相对源码 import 解析

| 项目 | 内容 |
|---|---|
| xlang 写法 | `import helper`、`import "./helper.xl"` |
| 生成规则描述 | package resolver 未命中后，按当前 sourcePath 目录尝试目标原名和 `.xl` 后缀。namespace 从 target 归一化：路径分隔符变成 `.`，末尾 `.xl` 去掉。最终无法解析会诊断 `cannot resolve import`，不再静默忽略。 |
| 生成器文件名和行号 | `demo6/src/compile/xl6_imports.c:1938` |
| 对应生成的 C 代码 | 成功解析后与 package 源码使用相同 AST 合并路径。 |
| 中间形态伪代码 | `resolve package; else resolve source-relative target(.xl); else error` |
| 实现同等功能手写的 C 代码 | 类似 C include 搜索路径，但 XLang 导入的是模块语义而非文本替换。 |
| 额外依赖的封装代码 | XRT UTF-8 文件访问、package resolver。 |

## CGEN-IMPG-018 单个 import 加载与缓存

| 项目 | 内容 |
|---|---|
| xlang 写法 | `import pkg`、`import pkg as p`、`from pkg import Name` |
| 生成规则描述 | 先处理内置 feature import，再解析 package，最后尝试源码路径。模块首次加载时收集依赖、递归展开并登记公开符号；后续导入复用 namespace 的 loaded 状态。普通 import 公开 canonical namespace，别名和选择性导入只私下加载 canonical namespace。 |
| 生成器文件名和行号 | `demo6/src/compile/xl6_imports.c:2112` |
| 对应生成的 C 代码 | 每个模块只生成一份 C 声明/定义。 |
| 中间形态伪代码 | `load(target, exposeNamespace); if !loaded merge; mark loaded; return canonicalNs` |
| 实现同等功能手写的 C 代码 | 对应构建图中的模块缓存和 include guard。 |
| 额外依赖的封装代码 | package resolver、manifest、source loader、feature registry。 |

## CGEN-IMPG-019 限定表达式符号重写

| 项目 | 内容 |
|---|---|
| xlang 写法 | `pkg.make()`、`alias.value`、多段 namespace 访问 |
| 生成规则描述 | 先对完整 member 链寻找最长公开 namespace，再按剩余公开符号名查表。命中后将整个 member AST 原地改为 NAME internalName；私有 namespace、缺失符号和暂不支持的桥接分别给出明确诊断。普通对象成员访问保持不变。 |
| 生成器文件名和行号 | `demo6/src/compile/xl6_imports.c:2249` |
| 对应生成的 C 代码 | cgen 看到平坦唯一名称，例如 `__xl6_import_pkg_make(...)`。 |
| 中间形态伪代码 | `qualifiedMember(ns,symbol) -> name(symbol.internalName)` |
| 实现同等功能手写的 C 代码 | 手写 C 直接调用带 package 前缀的符号。 |
| 额外依赖的封装代码 | import namespace/symbol table。 |

## CGEN-IMPG-020 限定类型引用重写

| 项目 | 内容 |
|---|---|
| xlang 写法 | `pkg.Point p`、`array<pkg.Box<int>>`、`func(pkg.Point) as pkg.Point`、`T is pkg.Base` |
| 生成规则描述 | TYPE 或语法允许的类型 NAME 含点号时，按 namespace 和最后一段查询公开符号；仅 class、union、typedef、extern type 可进入类型位置。私有依赖、缺少类型、符号不是类型分别报错。重写只替换类型根名称，泛型子节点继续递归处理。 |
| 生成器文件名和行号 | `demo6/src/compile/xl6_imports.c:2334` |
| 对应生成的 C 代码 | 所有限定类型位置都落到导入声明的唯一 C 类型名。 |
| 中间形态伪代码 | `type("pkg.Point") -> type("__xl6_import_pkg_Point")` |
| 实现同等功能手写的 C 代码 | 相当于使用包前缀 C typedef/struct 名。 |
| 额外依赖的封装代码 | 带 kind 的 import symbol table。 |

## CGEN-IMPG-021 模块 import 展开总入口

| 项目 | 内容 |
|---|---|
| xlang 写法 | 顶层混合普通 import、namespace alias 和 `from ... import ...` |
| 生成规则描述 | 只扫描进入函数时已有的顶层 import，逐个加载模块并完成 alias/selective 绑定。所有加载成功后，先按直接名称映射重写原文件 AST，再把导入声明移到正文前，最后重写 namespace 表达式和限定类型。别名、选择性名称和普通 namespace 均检查内置类型及自然顶层声明冲突。 |
| 生成器文件名和行号 | `demo6/src/compile/xl6_imports.c:2479` |
| 对应生成的 C 代码 | import 完全消解后由统一 sem/cgen 生成 C；import 语句自身不生成运行时代码。 |
| 中间形态伪代码 | `load initial imports -> bind names -> rewrite direct refs -> reorder declarations -> rewrite qualified refs` |
| 实现同等功能手写的 C 代码 | 相当于构建系统、头文件命名和链接前缀化三部分的组合。 |
| 额外依赖的封装代码 | package/import loader、AST 重写、类型注册表、诊断系统。 |
