# P2 import 依赖与命名空间细则

本文细化 `demo6/src/compile/xl6_imports.c` 的前半部分，覆盖 import 生成文本缓冲、依赖收集、package 多版本图、native singleton 冲突、命名空间表和导入类型名重写。

## CGEN-IMP-001 import 内部模型

| 项目 | 内容 |
|---|---|
| xlang 写法 | `import counter` 后使用 `counter.inc()` |
| 生成规则描述 | import 分片维护导入符号表、命名空间表和直接名称映射表。符号项记录 namespace、公开名称、唯一内部名称、AST 声明种类及当前桥接能力；命名空间项记录公开/私有状态和模块是否已经加载；名称映射项服务于 `from ... import ...` 与内置模块别名。 |
| 生成器文件名和行号 | `demo6/src/compile/xl6_imports.c:7` |
| 对应生成的 C 代码 | 不直接生成 C；导入源码声明经唯一改名后合并到当前 AST，再由常规 sem/cgen 路径生成 C。 |
| 中间形态伪代码 | `symbol={ns,name,internalName,kind,supported}; namespace={ns,isPrivate,loaded}; direct={localName,internalName}` |
| 实现同等功能手写的 C 代码 | 手写 C 通常通过头文件前缀或命名空间约定避免符号冲突。 |
| 额外依赖的封装代码 | import symbol table。 |

## CGEN-IMP-002 import 文本比较、复制与构造器

| 项目 | 内容 |
|---|---|
| xlang 写法 | 生成 import extern 声明文本 |
| 生成规则描述 | 文本比较大小写敏感；NULL 文本复制为空字符串。文本构造器容量从 256 起按 2 倍扩容，ok=false 后停止追加；take 会把 data 所有权交给调用方，空构造器返回空字符串。 |
| 生成器文件名和行号 | `demo6/src/compile/xl6_imports.c:33` |
| 对应生成的 C 代码 | 不直接生成 C；生成临时 xlang 声明文本。 |
| 中间形态伪代码 | `builder append; grow 256*2; take ownership` |
| 实现同等功能手写的 C 代码 | 手写导入桥接需要拼接声明文本或构造 AST。 |
| 额外依赖的封装代码 | import text builder。 |

## CGEN-IMP-003 import 生成字符串引号与 target 去引号

| 项目 | 内容 |
|---|---|
| xlang 写法 | `import "pkg"`、生成 `extern "c_symbol" func ...` |
| 生成规则描述 | 生成带引号字符串时只转义 `"` 和 `\`。import target 复制时，若首尾为匹配的单引号或双引号，则去掉首尾；否则原样复制。 |
| 生成器文件名和行号 | `demo6/src/compile/xl6_imports.c:105` |
| 对应生成的 C 代码 | 不直接生成 C；生成 `extern "foo" func ...` 形式的 xlang 声明。 |
| 中间形态伪代码 | `quote text for extern; unquote import target if matched quote pair` |
| 实现同等功能手写的 C 代码 | 手写桥接声明需要转义 C symbol 字符串。 |
| 额外依赖的封装代码 | import text builder。 |

## CGEN-IMP-004 import 依赖集合初始化与释放

| 项目 | 内容 |
|---|---|
| xlang 写法 | 编译单元收集 import 依赖 |
| 生成规则描述 | deps 初始化会清零结构并初始化 cSources、links、loadlibs 三个字符串列表；释放时释放这些列表、packageIds、native singleton 条目，最后清零 deps。 |
| 生成器文件名和行号 | `demo6/src/compile/xl6_imports.c:151` |
| 对应生成的 C 代码 | 不生成 C；影响 driver 后续链接/编译依赖。 |
| 中间形态伪代码 | `deps = {cSources,links,loadlibs,packageIds,natives}` |
| 实现同等功能手写的 C 代码 | 手写构建系统需要独立维护依赖集合。 |
| 额外依赖的封装代码 | import deps model。 |

## CGEN-IMP-005 依赖字符串去重

| 项目 | 内容 |
|---|---|
| xlang 写法 | 多个包引入同一个链接库或 C 源 |
| 生成规则描述 | 字符串列表按精确文本去重；空字符串不加入。append unique 只有在列表中不存在时才调用 package string list append。 |
| 生成器文件名和行号 | `demo6/src/compile/xl6_imports.c:185` |
| 对应生成的 C 代码 | 不生成 C；避免重复传递 link/loadlib/cSources。 |
| 中间形态伪代码 | `if text empty or contains: ok; else append` |
| 实现同等功能手写的 C 代码 | 手写构建脚本通常也会去重链接参数。 |
| 额外依赖的封装代码 | package string list。 |

## CGEN-IMP-006 packageId 去重

| 项目 | 内容 |
|---|---|
| xlang 写法 | A 和 B 都依赖同一个 C@1.0.0#abi1 |
| 生成规则描述 | packageId 为空不加入；已存在的 packageId 不重复加入。新增时扩展 packageIds 数组并复制字符串。该规则是避免依赖图重复遍历的基础。 |
| 生成器文件名和行号 | `demo6/src/compile/xl6_imports.c:208` |
| 对应生成的 C 代码 | 不生成 C；影响 import/package 依赖图遍历。 |
| 中间形态伪代码 | `if packageId not in deps: push copy(packageId)` |
| 实现同等功能手写的 C 代码 | 手写包管理器会用 visited set。 |
| 额外依赖的封装代码 | import deps model。 |

## CGEN-IMP-007 路径判断与 package 相对路径拼接

| 项目 | 内容 |
|---|---|
| xlang 写法 | manifest 中 `cSources`、`links`、`loadlibs` |
| 生成规则描述 | 绝对路径判定支持以 `/`、`\` 开头或第二个字符为 `:`。看起来像路径的文本包含 `/`、`\`、`:` 或 `.`。追加 package 路径时，若不是 alwaysPath 且不像路径，则作为原始链接名加入；若是绝对路径或没有 packageDir，则原样加入；否则用 packageDir 拼接相对路径。 |
| 生成器文件名和行号 | `demo6/src/compile/xl6_imports.c:223` |
| 对应生成的 C 代码 | 不生成 C；影响 driver 编译/链接参数。 |
| 中间形态伪代码 | `path = absolute ? value : join(packageDir,value)` |
| 实现同等功能手写的 C 代码 | 手写构建系统需要区分库名和文件路径。 |
| 额外依赖的封装代码 | package path join。 |

## CGEN-IMP-008 native singleton 冲突检测

| 项目 | 内容 |
|---|---|
| xlang 写法 | 两个 package 声明同一个 native singleton loadName 但版本或 packageId 不同 |
| 生成规则描述 | 只有 singleton 且 loadName 非空时登记 native。若已有 singleton loadName 相同且 version/packageId 也相同，则视为重复依赖并跳过；若 loadName 相同但 version 或 packageId 不同，则诊断 `native singleton package conflict` 并失败。否则追加 native 项。 |
| 生成器文件名和行号 | `demo6/src/compile/xl6_imports.c:292` |
| 对应生成的 C 代码 | 不生成 C；阻止不兼容 native 单例同时进入项目。 |
| 中间形态伪代码 | `same loadName && same identity -> skip; same loadName && identity differs -> conflict` |
| 实现同等功能手写的 C 代码 | 手写包管理器需要解决 native 单例冲突。 |
| 额外依赖的封装代码 | import deps native table。 |

## CGEN-IMP-009 manifest 依赖信息收集

| 项目 | 内容 |
|---|---|
| xlang 写法 | `xlang.package.json` 中的 cSources、links、loadlibs、native |
| 生成规则描述 | 从 manifest 收集 packageId、cSources、links、loadlibs 和 native 信息。cSources 始终按路径处理；links/loadlibs 可保留纯库名。所有路径会相对 manifest->dir 解析。 |
| 生成器文件名和行号 | `demo6/src/compile/xl6_imports.c:355` |
| 对应生成的 C 代码 | 不生成 C；影响后续 build/link 参数。 |
| 中间形态伪代码 | `deps += manifest package/link/loadlib/cSource/native` |
| 实现同等功能手写的 C 代码 | 手写构建系统读取 package manifest 后合并依赖。 |
| 额外依赖的封装代码 | package manifest model。 |

## CGEN-IMP-010 package manifest 依赖信息收集

| 项目 | 内容 |
|---|---|
| xlang 写法 | package manifest 中的 cSources、links、loadlibs、native |
| 生成规则描述 | 从 source module 收集 packageId、cSources、links、loadlibs 和 native 信息。相对路径基准为 resolved packageDir。 |
| 生成器文件名和行号 | `demo6/src/compile/xl6_imports.c:394` |
| 对应生成的 C 代码 | 不生成 C；影响后续 build/link 参数。 |
| 中间形态伪代码 | `deps += manifest package/link/loadlib/cSource/native` |
| 实现同等功能手写的 C 代码 | 手写构建系统读取 package manifest 后合并依赖。 |
| 额外依赖的封装代码 | source module model。 |

## CGEN-IMP-011 dependency 版本与 ABI 匹配

| 项目 | 内容 |
|---|---|
| xlang 写法 | package A 依赖 C 的指定 version/abiVersion |
| 生成规则描述 | dependency 匹配要求 resolved packageVersion 与 dep->version 相等，resolved abiVersion 与 dep->abiVersion 相等。不匹配时报 `package dependency version mismatch: <packageId>`。 |
| 生成器文件名和行号 | `demo6/src/compile/xl6_imports.c:434` |
| 对应生成的 C 代码 | 不生成 C；防止错误版本 package 进入依赖图。 |
| 中间形态伪代码 | `version == dep.version && abi == dep.abiVersion` |
| 实现同等功能手写的 C 代码 | 手写包管理器需要版本约束检查。 |
| 额外依赖的封装代码 | package dependency model。 |

## CGEN-IMP-012 dependency 解析策略

| 项目 | 内容 |
|---|---|
| xlang 写法 | manifest 依赖 `C` 的 `1.0.0` |
| 生成规则描述 | 先按 dependency name 在 manifest->dir 下解析；若解析成功且版本匹配则使用。若解析到包但版本不匹配，记录 sawMismatchedPackage。随后尝试 `name@version` 形式解析。若仍失败且曾有不匹配包，则输出版本不匹配诊断；否则输出 `package dependency is not resolved`。 |
| 生成器文件名和行号 | `demo6/src/compile/xl6_imports.c:462` |
| 对应生成的 C 代码 | 不生成 C；影响多版本 package 选择。 |
| 中间形态伪代码 | `resolve name; else resolve name@version; diagnose mismatch/unresolved` |
| 实现同等功能手写的 C 代码 | 手写包管理器常用版本限定路径解决依赖冲突。 |
| 额外依赖的封装代码 | package resolver。 |

## CGEN-IMP-013 manifest dependency graph 递归收集

| 项目 | 内容 |
|---|---|
| xlang 写法 | A 依赖 C@1，B 依赖 C@2 |
| 生成规则描述 | 遍历 manifest dependencies，若 packageId 已在 deps 中则跳过；否则解析 dependency 并递归收集 resolved package deps。该规则支持同一项目中按 packageId 区分多版本依赖。 |
| 生成器文件名和行号 | `demo6/src/compile/xl6_imports.c:508` |
| 对应生成的 C 代码 | 不生成 C；影响导入依赖闭包。 |
| 中间形态伪代码 | `for dep: if not visited(dep.packageId): resolve; collect resolved package` |
| 实现同等功能手写的 C 代码 | 手写包管理器需要 visited set 和递归依赖解析。 |
| 额外依赖的封装代码 | package resolver 和 import deps。 |

## CGEN-IMP-014 resolved package 依赖闭包收集

| 项目 | 内容 |
|---|---|
| xlang 写法 | import 成功解析到 package |
| 生成规则描述 | 如果 packageId 已访问则跳过。必须解析 manifestPath 且能读取 `xlang.package.json`。读取 manifest 后用 resolver 结果覆盖 package 身份，并把依赖 package namespace 作为 private 加入符号表。随后收集 manifest deps 和 manifest dependency graph。 |
| 生成器文件名和行号 | `demo6/src/compile/xl6_imports.c:537` |
| 对应生成的 C 代码 | 不生成 C；为 import 建立依赖闭包和私有命名空间。 |
| 中间形态伪代码 | `load manifest; namespace private; collect manifest/dependency graph` |
| 实现同等功能手写的 C 代码 | 手写包管理器需要解析 package manifest 并收集传递依赖。 |
| 额外依赖的封装代码 | source import loader、package manifest、package resolver。 |

## CGEN-IMP-015 import 符号表初始化与释放

| 项目 | 内容 |
|---|---|
| xlang 写法 | 单个模块 import 展开期间 |
| 生成规则描述 | 符号表和直接名称映射表分别初始化。释放时逐项释放 symbol 的 ns/name/internalName、namespace 的 ns，以及直接映射的 name/internalName，最后释放数组并清零；部分分配失败也会释放本次尚未登记的字段。 |
| 生成器文件名和行号 | `demo6/src/compile/xl6_imports.c:669` |
| 对应生成的 C 代码 | 不生成 C；管理 import 展开状态。 |
| 中间形态伪代码 | `free symbols[]; free namespaces[]; free directImports[]` |
| 实现同等功能手写的 C 代码 | 手写工具需要管理临时符号表生命周期。 |
| 额外依赖的封装代码 | import symbol table。 |

## CGEN-IMP-016 命名空间登记与私有性合并

| 项目 | 内容 |
|---|---|
| xlang 写法 | 直接 `import A` 与 A 的私有依赖 B |
| 生成规则描述 | namespace 为空不能登记。若 namespace 已存在，公开登记会把 `isPrivate` 清零，私有登记不会覆盖已有公开状态。每个 namespace 另有 `loaded` 位：同一模块无论普通导入、别名导入还是选择性导入，只展开一次；先作为传递私有依赖加载、后被用户直接导入时会升级为公开命名空间而不重复合并 AST。 |
| 生成器文件名和行号 | `demo6/src/compile/xl6_imports.c:781` |
| 对应生成的 C 代码 | 不生成 C；决定 `B.xxx` 是否可被用户访问。 |
| 中间形态伪代码 | `existing ? public upgrades visibility : add {ns,isPrivate,loaded=false}; load only once` |
| 实现同等功能手写的 C 代码 | 手写包系统需要区分直接依赖和传递私有依赖。 |
| 额外依赖的封装代码 | import namespace table。 |

## CGEN-IMP-017 导入符号查找

| 项目 | 内容 |
|---|---|
| xlang 写法 | `counter.inc`、`counter.Point`、`from counter import Point as P` |
| 生成规则描述 | 导入符号按 namespace 和公开名称精确匹配，并保留声明 kind。表达式成员访问可匹配函数、变量和常量；限定类型或选择性类型导入还要求 kind 为 class、union、typedef 或 extern type。namespace 已存在但符号缺失、符号不是类型或 namespace 为私有依赖时分别给出确定诊断。 |
| 生成器文件名和行号 | `demo6/src/compile/xl6_imports.c:864` |
| 对应生成的 C 代码 | 不直接生成 C；决定表达式或类型引用是否重写为同一个唯一内部名称。 |
| 中间形态伪代码 | `symbol=find(ns,name); if type-position require kind in {class,union,typedef,extern-type}` |
| 实现同等功能手写的 C 代码 | 手写桥接代码通过命名表或头文件查找符号。 |
| 额外依赖的封装代码 | import symbol table。 |

## CGEN-IMP-018 导入内部名称生成

| 项目 | 内容 |
|---|---|
| xlang 写法 | `counter.inc` 被重写为内部符号 |
| 生成规则描述 | 内部名称格式为 `__xl6_import_<ns>_<name>`。namespace 和 name 中非 ASCII 标识符字符会替换为 `_`；合法字符为 ASCII 字母、数字和下划线。 |
| 生成器文件名和行号 | `demo6/src/compile/xl6_imports.c:699` |
| 对应生成的 C 代码 | 后续 cgen 会看到 `__xl6_import_counter_inc(...)`。 |
| 中间形态伪代码 | `internal = "__xl6_import_" + sanitize(ns) + "_" + sanitize(name)` |
| 实现同等功能手写的 C 代码 | 手写 C 桥接通常使用前缀避免命名冲突。 |
| 额外依赖的封装代码 | import text builder。 |

## CGEN-IMP-019 source module 类型查找与 owner 泛型参数保护

| 项目 | 内容 |
|---|---|
| xlang 写法 | import class `Box<T>` 字段类型中出现 `T` 和同包类型名 |
| 生成规则描述 | 类型文本 token 支持 ASCII 标识符字符和 UTF-8 字节。查找 source type 时按 token 与 module->types[i].name 精确匹配；若 token 是当前 owner 的 typeParam，则不当作导入类型改名。 |
| 生成器文件名和行号 | `demo6/src/compile/xl6_imports.c:734` |
| 对应生成的 C 代码 | 不直接生成 C；影响生成的 import 类型声明文本。 |
| 中间形态伪代码 | `if token in owner.typeParams: keep; else if token is imported type: rewrite` |
| 实现同等功能手写的 C 代码 | 手写泛型类型声明需要区分类型参数和外部类型名。 |
| 额外依赖的封装代码 | import type table。 |

## CGEN-IMP-020 导入类型文本重写

| 项目 | 内容 |
|---|---|
| xlang 写法 | imported class 字段 `OtherType value` |
| 生成规则描述 | 扫描类型字符串中的标识符 token。若 token 是导入模块中的类型且不是 owner type param，则替换为该类型的内部名称；否则原样保留。非标识符字符逐字节复制。 |
| 生成器文件名和行号 | `demo6/src/compile/xl6_imports.c:770` |
| 对应生成的 C 代码 | 后续 cgen 会看到改名后的类型，例如 `__xl6_import_pkg_OtherType`。 |
| 中间形态伪代码 | `typeText tokens -> imported type names rewritten to internal names` |
| 实现同等功能手写的 C 代码 | 手写桥接头文件需要对导入类型加前缀。 |
| 额外依赖的封装代码 | import type rewrite helper。 |

## CGEN-IMP-021 导入符号登记

| 项目 | 内容 |
|---|---|
| xlang 写法 | source export 进入 import 符号表 |
| 生成规则描述 | 若同 namespace/name 已登记则复用原身份；否则复制 ns/name/internalName，并保存 AST kind 与 supported。公开 class、具名 union、typedef、extern type、xlang func、直接映射的 extern func/var、var、const 都可登记；非公开声明不会进入表。命名空间别名只复制符号视图，内部名称不变，因此别名、限定名与选择性导入保持同一类型身份。 |
| 生成器文件名和行号 | `demo6/src/compile/xl6_imports.c:1669` |
| 对应生成的 C 代码 | 不直接生成 C；源码声明已合并到 AST，后续引用统一指向其唯一内部名称。 |
| 中间形态伪代码 | `add-if-missing {ns,name,internalName,kind,supported}; alias reuses internalName` |
| 实现同等功能手写的 C 代码 | 手写桥接表也需要记录哪些符号可用。 |
| 额外依赖的封装代码 | import symbol table。 |
