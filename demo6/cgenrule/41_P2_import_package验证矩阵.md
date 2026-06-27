# P2 import 与 package 验证矩阵

本文继续细化 `34_规则测试范例交叉索引.md`，覆盖以下 P2 规则文档：

- `29_P2_import依赖与命名空间细则.md`：21 条
- `30_P2_import声明生成与AST重写细则.md`：21 条
- `31_P2_package解析与resolver细则.md`：32 条

合计 74 条规则。本文件不新增 `CGEN-*` 规则，只记录 import 依赖收集、命名空间、XAPI 声明注入、AST 重写、package manifest 和 resolver 当前由哪些测试、范例和 fixture 证明。

## Import 依赖与命名空间

| 规则范围 | 当前验证用例 | 证据等级 | 核验结论 |
|---|---|---|---|
| `CGEN-IMP-001..003` import 内部模型、文本构造器、字符串引号与 target 去引号 | `import_metadata.xl`；`test.bat` 检查 IR 中 `import counter`、`import "local/math"`；`import_xapi_use.xl` 检查生成 extern 声明 | IR/JSON 检查 + import 消费 | import target、生成声明文本、extern C symbol 引号输出有覆盖。 |
| `CGEN-IMP-004..007` 依赖集合、字符串/packageId 去重、路径判断、package 相对路径拼接 | `import_metadata.xl`、packages `counter/class_api/union_api/uses_dep*`、`metadata_combo`、`import_dependency_versions.xl`、`import_deps_snapshot_host.c` | JSON 检查 + build/run + C 宿主快照 | cSources/links/loadlibs/packageId 收集、相对 packageDir 路径和重复依赖跳过有覆盖；`metadata_combo` 同时从 manifest 与 XAPI 给出相同依赖，快照断言最终 package/cSource/link/loadlib 均为 1 条。 |
| `CGEN-IMP-008..010` native singleton、manifest 依赖信息、XAPI 依赖信息收集 | `import_native_singleton_conflict.xl`、packages `native_one/native_two`、packages `uses_dep/uses_dep_v2`、`metadata_combo`、`import_dependency_private.xl`、`import_deps_snapshot_host.c` | 语义负例 + build/run + C 宿主快照 | native singleton 冲突、重复 native singleton 跳过、manifest/XAPI 中 cSources/link/loadlib/native 收集有覆盖；组合包同时覆盖 cSource、link、loadlib 和 native 元数据。 |
| `CGEN-IMP-011..014` dependency 版本/ABI 匹配、解析策略、递归依赖图、resolved package 闭包 | `import_dependency_versions.xl`、`import_dependency_version_mismatch.xl`、packages `dep_value` 与 `dep_value@2.0.0`、`uses_dep_bad` | build/run + 语义负例 | 同一项目多版本依赖、版本不匹配诊断、传递依赖闭包和 private namespace 登记有覆盖。 |
| `CGEN-IMP-015..018` 符号表生命周期、namespace public/private 合并、导入符号查找、内部名称生成 | `import_xapi_use.xl`、`import_xapi_var_use.xl`、`import_dependency_no_pollution.xl`、`import_no_global_pollution.xl` | IR/C 检查 + 语义负例 | `__xl6_import_<ns>_<name>` 内部名、公有 namespace、私有依赖不可访问和全局不污染有覆盖。 |
| `CGEN-IMP-019..020` XAPI 类型查找、owner 泛型参数保护、导入类型文本重写 | `import_xapi_type_class_use.xl`、`import_xapi_type_class_inherit_use.xl`、`import_xapi_type_union_use.xl`、`xapi_type_fields.xl` | IR/C 检查 + build/run | class/union 类型内部改名、继承 base 重写、字段类型重写和 owner 泛型参数保留有覆盖。 |
| `CGEN-IMP-021` 导入符号登记 | `import_xapi_use.xl`、`import_xapi_var_use.xl`、`import_unsupported_xapi_use.xl` | IR/C 检查 + 语义负例 | supported symbol 生成 extern，unsupported symbol 登记后访问时报 `C ABI bridge: unsupported_xapi.unsupportedCall`，带具体 namespace 和符号名。 |

## Import 声明生成与 AST 重写

| 规则范围 | 当前验证用例 | 证据等级 | 核验结论 |
|---|---|---|---|
| `CGEN-IMPG-001` function export 是否可进入 extern | `import_xapi_use.xl`、`import_unsupported_xapi_use.xl`、packages `unsupported_xapi` | IR 检查 + 语义负例 | cdecl、无 optional/vararg、单返回约束有覆盖；不支持函数访问时诊断明确。 |
| `CGEN-IMPG-002..007` 泛型参数、字段声明、union/class/custom 类型声明生成、inline union 恢复、批量生成 | `import_xapi_type_union_use.xl`、`import_xapi_type_class_use.xl`、`import_xapi_type_class_inherit_use.xl`、`xapi_load_types_host.c` | IR/C 检查 + C 宿主运行 | 导入 class/custom/union 声明、base、字段、inline union 和改名类型进入 AST 后正常 cgen。 |
| `CGEN-IMPG-008..010` function extern、var extern、XAPI 声明生成与符号登记 | `import_xapi_use.xl` 检查 `extern "xl6_import_counter_add" func __xl6_import_counter_counterAdd`；`import_xapi_var_use.xl` 检查 extern var；`import_xapi_type_*_var_use.xl` | IR/C 检查 + build/run | function/var extern 注入、类型声明先于 export 声明、supported/unsupported 登记均有覆盖。 |
| `CGEN-IMPG-011` 生成声明重新解析并合并 AST | 所有 import XAPI 成功用例，特别是 `import_xapi_type_class_inherit_use.xl` 的 IR 检查；`import_invalid_decl_xapi.xl` 覆盖生成声明解析失败诊断 | IR 检查 + build/run + 语义负例 | 生成文本经 parser 重新解析后进入 root，后续语义和 cgen 可消费；畸形 XAPI 生成非法 extern 时会转为稳定编译诊断。 |
| `CGEN-IMPG-012..017` namespace 选择、resolver 身份覆盖、单 import 解析、缺 XAPI 诊断、public namespace 冲突、完整展开顺序 | `import_metadata.xl`、`import_missing_xapi.xl`、`import_no_global_pollution.xl`、`import_dependency_private.xl`、`import_dependency_versions.xl` | JSON/IR 检查 + build/run + 语义负例 | namespace 来源、resolver package 身份权威、缺接口文件、重复/私有命名空间、依赖收集和声明合并顺序有覆盖。 |
| `CGEN-IMPG-018..021` 成员访问重写、namespace 符号访问、缺符号/private 诊断、模块 import 总入口 | `import_xapi_use.xl` 检查 `call __xl6_import_counter_counterAdd`；`import_xapi_var_use.xl` 检查变量重写；`import_dependency_no_pollution.xl`；`import_xapi_type_no_global_pollution.xl` | IR/C 检查 + 语义负例 | `pkg.symbol` 改写为内部名称、缺符号和 private dependency 诊断、只处理初始顶层 import 有覆盖。 |

## Package 解析与 Resolver

| 规则范围 | 当前验证用例 | 证据等级 | 核验结论 |
|---|---|---|---|
| `CGEN-PKG-001..007` 字符串复制、dirname、path join、默认包名、packageId、字符串列表、manifest 生命周期 | packages 目录下所有 `xlang.package.json`、`import_metadata.xl`、`import_dependency_versions.xl` | build/run + JSON 检查 | manifest 默认身份、packageId、路径拼接、字符串列表和释放路径在多 package fixture 中反复覆盖。 |
| `CGEN-PKG-008..016` manifest JSON parser、字符串/bool/未知值、数组、dependencies、artifact、native、根对象 | packages `counter`、`class_api`、`union_api`、`uses_dep*`、`native_one/native_two`、`metadata_combo`、`missing_interface`、`lib/libprobe` | build/run + 语义负例 + fixture | links/loadlibs/cSources/dependencies/native 字段读取、组合字段同包共存、未知字段跳过主策略和未知对象/数组跳过均有覆盖。 |
| `CGEN-PKG-017..020` manifest 文件读取、format 校验、默认字段补齐、最终身份和失败清理 | packages fixture、`import_missing_xapi.xl`、`import_dependency_version_mismatch.xl` | build/run + 语义负例 | XRT UTF-8 文件读取、格式/默认值/packageId/dir/xapi 补齐和失败清理有覆盖。 |
| `CGEN-PKG-021..023` 直接 XAPI、manifest 归一、候选命中规则 | packages `broken_xapi/package.xapi.json`、`counter/xlang.package.json`、`import_path_no_ext.xl`、`import_broken_xapi.xl`、`import_missing_xapi.xl` | import 消费 + IR/C 检查 + 语义负例 | 直接 `.xapi.json` 与 manifest 到 interfacePath 的归一、`.xapi.json` 双后缀剥离、存在但不可用时重置并诊断有覆盖。 |
| `CGEN-PKG-024..030` path import 判定、resolver 结果、绝对/相对候选、自动追加 `.xapi.json`、相对 importer 优先、目录包命中顺序 | `import_metadata.xl` 的 `"local/math"`、`import_path_no_ext.xl` 的 `"path_no_ext/probe"`、`test/cases/import_relative_scope/root/source/import_relative_scope.xl`、package fixture、`import_missing_xapi.xl` | JSON 检查 + IR/C 检查 + import 消费 | path target、相对 importer 目录、自动追加 `.xapi.json`、目录下 `xlang.package.json`/`package.xapi.json` 命中顺序有覆盖；`import_relative_scope` 额外证明进程 cwd 下的同名 `pkg.xapi.json` 不会遮蔽 importer 源文件旁边的 `./pkg.xapi.json`。 |
| `CGEN-PKG-031..032` 裸包名 packages/lib 搜索与 interfacePath 查询 | `import_xapi_use.xl`、`import_lib_search.xl`、`import_dependency_private.xl`、`import_dependency_versions.xl`、packages `counter/uses_dep*`、`lib/libprobe` | build/run + IR/C 检查 | `packages/<name>` 搜索、`lib/<name>` 搜索、`name@version` 多版本目录和 interfacePath 查询均有覆盖。 |

## 剩余风险与后续增强

| 缺口 | 建议补法 |
|---|---|
| package JSON parser 的未知字段跳过已由 `lib/libprobe/xlang.package.json` 的未知对象/数组字段覆盖。 | 后续可增加畸形未知值负例，但不是当前 RC 阻断项。 |
| path import 自动追加 `.xapi.json`、相对 importerDir 优先和 `lib/<name>` 搜索已分别由 `import_path_no_ext.xl`、`import_relative_scope.xl` 和 `import_lib_search.xl` 覆盖。 | 后续可补直接 XAPI 包名冲突负例。 |
| import generated declaration 解析失败路径已由 `packages/invalid_decl_xapi` 与 `import_invalid_decl_xapi.xl` 覆盖。 | 后续可补更多畸形 type 文本，但当前已有稳定总括诊断 `XAPI import generated invalid extern declarations`。 |
| links/loadlibs/cSources/native 同包组合、相对路径拼接和 manifest/XAPI 重复依赖已由 `metadata_combo` 的 build/run 与 `import_deps_snapshot_host.c` 覆盖。 | 后续可扩展更多依赖图快照；当前最终 deps 列表数量、packageId、相对路径文本和 native 去重已有机器断言。 |

## 阶段结论

import、package、XAPI 声明注入、AST 重写与 resolver 已完成矩阵级整理；CLI/DLL 矩阵已落在 `42_P2_CLI_DLL验证矩阵.md`。
