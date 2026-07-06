# P2 import 与 package 验证矩阵

本文继续细化 `34_规则测试范例交叉索引.md`，覆盖以下 P2 规则文档：

- `29_P2_import依赖与命名空间细则.md`：21 条
- `30_P2_import声明生成与AST重写细则.md`：21 条
- `31_P2_package解析与resolver细则.md`：32 条

合计 74 条规则。本文件不新增 `CGEN-*` 规则，只记录当前 source import、package manifest、companion C、命名空间和 resolver 由哪些测试、范例和 fixture 证明。旧接口 JSON 机制已从主线删除，不再作为验证来源。

## Import 依赖与命名空间

| 规则范围 | 当前验证用例 | 证据等级 | 核验结论 |
|---|---|---|---|
| `CGEN-IMP-001..003` import 内部模型、文本构造器、字符串引号与 target 去引号 | `import_metadata.xl`；`test.bat` 检查 IR 中 `import counter`、`import "local/math"`；`import_source_use.xl` 检查源码模块声明消费 | IR/JSON 检查 + import 消费 | import target、生成声明文本、extern C symbol 引号输出有覆盖。 |
| `CGEN-IMP-004..007` 依赖集合、字符串/packageId 去重、路径判断、package 相对路径拼接 | `import_metadata.xl`、packages `counter/class_api/union_api/uses_dep*`、`metadata_combo`、`import_dependency_versions.xl`、`import_deps_snapshot_host.c` | JSON 检查 + build/run + C 宿主快照 | cSources/links/loadlibs/packageId 收集、相对 packageDir 路径和重复依赖跳过有覆盖。 |
| `CGEN-IMP-008..010` native singleton、manifest 依赖信息、package 依赖信息收集 | `import_native_singleton_conflict.xl`、packages `native_one/native_two`、packages `uses_dep/uses_dep_v2`、`metadata_combo`、`import_dependency_private.xl`、`import_deps_snapshot_host.c` | 语义负例 + build/run + C 宿主快照 | native singleton 冲突、重复 native singleton 跳过、manifest 中 cSources/link/loadlib/native 收集有覆盖。 |
| `CGEN-IMP-011..014` dependency 版本/ABI 匹配、解析策略、递归依赖图、resolved package 闭包 | `import_dependency_versions.xl`、`import_dependency_version_mismatch.xl`、packages `dep_value` 与 `dep_value@2.0.0`、`uses_dep_bad` | build/run + 语义负例 | 同一项目多版本依赖、版本不匹配诊断、传递依赖闭包和 private namespace 登记有覆盖。 |
| `CGEN-IMP-015..018` 符号表生命周期、namespace public/private 合并、导入符号查找、内部名称生成 | `import_source_use.xl`、`import_package_use.xl`、`import_package_var_use.xl`、`import_dependency_no_pollution.xl`、`import_no_global_pollution.xl` | IR/C 检查 + 语义负例 | `__xl6_import_<ns>_<name>` 内部名、公有 namespace、私有依赖不可访问和全局不污染有覆盖。 |
| `CGEN-IMP-019..020` package 类型查找、owner 泛型参数保护、导入类型文本重写 | `import_package_type_class_use.xl`、`import_package_type_union_use.xl`、fixtures `class_api/class_inherit_api/union_api` | IR/C 检查 + build/run | class/union 类型内部改名、继承 base 重写、字段类型重写和 owner 泛型参数保留有覆盖。 |
| `CGEN-IMP-021` 导入符号登记 | `import_source_use.xl`、`import_source_native_use.xl`、`import_package_use.xl`、`import_package_var_use.xl` | IR/C 检查 + build/run | function/var/type/native symbol 进入导入命名空间，访问时改写为内部名称。 |

## Import 声明生成与 AST 重写

| 规则范围 | 当前验证用例 | 证据等级 | 核验结论 |
|---|---|---|---|
| `CGEN-IMPG-001` function export 是否可进入 extern | `import_source_native_use.xl`、`import_package_use.xl`、`extern_bool.xl` | IR 检查 + build/run | cdecl、单返回、extern symbol 生成和调用路径有覆盖。 |
| `CGEN-IMPG-002..007` 泛型参数、字段声明、union/class 类型声明生成、inline union 恢复、批量生成 | `import_package_type_union_use.xl`、`import_package_type_class_use.xl`、fixtures `class_api/class_inherit_api/union_api` | IR/C 检查 + build/run | 导入 class/union 声明、base、字段、inline union 和改名类型进入 AST 后正常 cgen。 |
| `CGEN-IMPG-008..010` function extern、var extern、package 声明生成与符号登记 | `import_package_use.xl` 检查 `extern "xl6_import_counter_add" func __xl6_import_counter_counterAdd`；`import_package_var_use.xl` 检查 extern var；`import_package_type_*_use.xl` 检查类型声明 | IR/C 检查 + build/run | function/var extern 注入、类型声明先于 export 声明、符号登记均有覆盖。 |
| `CGEN-IMPG-011` 生成声明重新解析并合并 AST | `import_source_use.xl`、`import_source_native_use.xl`、`import_package_type_class_use.xl`、`import_package_type_union_use.xl` | IR 检查 + build/run | 生成文本经 parser 重新解析后进入 root，后续语义和 cgen 可消费。 |
| `CGEN-IMPG-012..017` namespace 选择、resolver 身份覆盖、单 import 解析、缺 package manifest 诊断、public namespace 冲突、完整展开顺序 | `import_metadata.xl`、`import_no_global_pollution.xl`、`import_dependency_private.xl`、`import_dependency_versions.xl`、fixture `missing_interface` | JSON/IR 检查 + build/run + 语义负例 | namespace 来源、resolver package 身份权威、缺 manifest、重复/私有命名空间、依赖收集和声明合并顺序有覆盖。 |
| `CGEN-IMPG-018..021` 成员访问重写、namespace 符号访问、缺符号/private 诊断、模块 import 总入口 | `import_source_use.xl` 检查调用内部函数；`import_package_var_use.xl` 检查变量重写；`import_dependency_no_pollution.xl`、`import_source_no_global_pollution.xl` | IR/C 检查 + 语义负例 | `pkg.symbol` 改写为内部名称、缺符号和 private dependency 诊断、只处理初始顶层 import 有覆盖。 |

## Package 解析与 Resolver

| 规则范围 | 当前验证用例 | 证据等级 | 核验结论 |
|---|---|---|---|
| `CGEN-PKG-001..007` 字符串复制、dirname、path join、默认包名、packageId、字符串列表、manifest 生命周期 | release/extlibs 与 test/fixtures 下所有 `xlang.package.json`、`import_metadata.xl`、`import_dependency_versions.xl` | build/run + JSON 检查 | manifest 默认身份、packageId、路径拼接、字符串列表和释放路径在多 package fixture 中反复覆盖。 |
| `CGEN-PKG-008..016` manifest JSON parser、字符串/bool/未知值、数组、dependencies、artifact、native、根对象 | packages `counter`、`class_api`、`union_api`、`uses_dep*`、`native_one/native_two`、`metadata_combo`、`missing_interface`、`test/fixtures/libprobe` | build/run + 语义负例 + fixture | links/loadlibs/cSources/dependencies/native 字段读取、组合字段同包共存、未知字段跳过主策略和未知对象/数组跳过均有覆盖。 |
| `CGEN-PKG-017..020` manifest 文件读取、format 校验、默认字段补齐、最终身份和失败清理 | package fixtures、`import_dependency_version_mismatch.xl`、fixture `missing_interface` | build/run + 语义负例 | XRT UTF-8 文件读取、格式/默认值/packageId/dir 补齐和失败清理有覆盖。 |
| `CGEN-PKG-021..023` manifest 归一、候选命中规则、目录 package 入口 | `counter/xlang.package.json`、`import_path_no_ext.xl`、fixture `missing_interface` | import 消费 + IR/C 检查 + 语义负例 | manifest 到 source package 的归一、目录 package 入口、存在但不可用时重置并诊断有覆盖。 |
| `CGEN-PKG-024..030` path import 判定、resolver 结果、绝对/相对候选、相对 importer 优先、目录包命中顺序 | `import_metadata.xl` 的 `"local/math"`、`import_path_no_ext.xl` 的 `"path_no_ext/probe"`、`test/cases/import_relative_scope/root/source/import_relative_scope.xl`、package fixture | JSON 检查 + IR/C 检查 + import 消费 | path target、相对 importer 目录、目录下 `xlang.package.json` 命中顺序有覆盖。 |
| `CGEN-PKG-031..032` 裸包名 release/extlibs 与 package path fixture 搜索、manifestPath 查询 | `import_package_use.xl`、`import_lib_search.xl`、`import_dependency_private.xl`、`import_dependency_versions.xl`、packages `counter/uses_dep*`、`test/fixtures/libprobe` | build/run + IR/C 检查 | `release/extlibs/<name>` 搜索、`test/fixtures/<name>` 搜索、`name@version` 多版本目录和 manifestPath 查询均有覆盖。 |

## 剩余风险与后续增强

| 缺口 | 建议补法 |
|---|---|
| package JSON parser 的未知字段跳过已由 `test/fixtures/libprobe/xlang.package.json` 的未知对象/数组字段覆盖。 | 后续可增加畸形未知值负例，但不是当前 RC 阻断项。 |
| path import 相对 importerDir 优先和 `test/fixtures/<name>` 搜索已分别由 `import_relative_scope.xl` 和 `import_lib_search.xl` 覆盖。 | 后续可补更多同名目录和 package path 顺序测试。 |
| import generated declaration 解析失败路径当前依赖通用 parser/semantic 负例覆盖。 | 后续可补专门畸形 package 源声明负例。 |
| links/loadlibs/cSources/native 同包组合、相对路径拼接和重复依赖已由 `metadata_combo` 的 build/run 与 `import_deps_snapshot_host.c` 覆盖。 | 后续可扩展更多依赖图快照；当前最终 deps 列表数量、packageId、相对路径文本和 native 去重已有机器断言。 |

## 阶段结论

import、package、source 声明注入、AST 重写与 resolver 已完成矩阵级整理；CLI/DLL 矩阵已落在 `42_P2_CLI_DLL验证矩阵.md`。
