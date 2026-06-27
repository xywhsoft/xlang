# P2 XAPI 元数据验证矩阵

本文继续细化 `34_规则测试范例交叉索引.md`，覆盖以下 P2 规则文档：

- `26_P2_XAPI基础输出与doc细则.md`：27 条
- `27_P2_XAPI模块构建与JSON输出细则.md`：29 条
- `28_P2_XAPI读取与模型恢复细则.md`：31 条

合计 87 条规则。本文件不新增 `CGEN-*` 规则，只记录 XAPI 生成、JSON 输出、doc 结构化字段、模块模型构建、读取恢复与 import 消费当前由哪些测试证明。

## XAPI 基础输出与 doc

| 规则范围 | 当前验证用例 | 证据等级 | 核验结论 |
|---|---|---|---|
| `CGEN-XAPI-001..004` 大小写敏感、JSON 字符串转义、字符串包装、C symbol 去引号 | `export_add.xl`、`export_alias.xl`、`import_metadata.xl`、`xapi_doc.xl`、`xapi_json_escape.xl`、大量 `xl xapi --emit` 输出检查 | 直接 JSON 检查 + JSON 回读快照 | XAPI 字符串字段、导出 C symbol、import target 和 doc 文本输出有覆盖；引号、反斜杠和 `\n` 文本经过 JSON 输出后可正确回读。 |
| `CGEN-XAPI-005..007` 类型别名解析、func 类型字符串、普通/泛型类型字符串 | `export_alias.xl`；`test.bat` 检查 `"type":"int"`、`function_types.xl`、`xapi_type_fields.xl`、feature suite function/generic 示例 | 直接 JSON/IR 检查 + 运行行为检查 | alias 规范化、func 签名、多返回签名、泛型类型字符串有覆盖。 |
| `CGEN-XAPI-008..009` AST 入口识别与 entry JSON 输出 | `dump_hello/hello.xapi.json`、`cgen_main_entry.xl`、`cgen_main_case_sensitive.xl`、`cgen_main_case_sensitive_script.xl`、`import_metadata.xl`、库式 export 用例 | 直接 JSON 检查 + 运行行为检查 | `xlang-main`、`script-main`、`none` 三类入口模型有覆盖；显式 main 与脚本入口均检查 `entry.cSymbol == xlang_main`；`Main` 非入口已有 XAPI `entry.kind == none` 快照，`Main` 与顶层语句共存时已有 `entry.kind == script-main` 快照。 |
| `CGEN-XAPI-010..015` 参数、返回、export function/var/const、类型声明、import 直接 JSON 输出 | `export_alias.xl`、`export_vars.xl`、`export_union.xl` dump-dir、`import_metadata.xl`、`xapi_type_fields.xl` | 直接 JSON 检查 | params/returns、function/var/const export、class/custom/union type、imports 数组有覆盖。 |
| `CGEN-XAPI-016..017` XAPI string builder 初始化、扩容、追加、接管和复制 | `xapi_doc.xl`、`xapi_type_fields.xl`、`import_metadata.xl`、较长 doc/example 文本 | 间接覆盖 | builder 由 doc、signature、packageId/moduleId 和 JSON 字段构造大量使用；扩容失败路径属于内部错误间接覆盖。 |
| `CGEN-XAPI-018..022` `#doc ... #end` 起止、字段头、多行去缩进、单字段和完整 doc JSON 输出 | `xapi_doc.xl`；`test.bat` 检查 `"doc"`、`Add two integers.`、`auto value = add(1, 2)`；`xapi_type_fields.xl` 检查字段 doc；`sem_doc_*.xl` | 直接 JSON 检查 + 语义负例 | desc/param/return/note/example、raw、fields、多行 example 和字段 doc 有覆盖。 |
| `CGEN-XAPI-023..027` C symbol/import target 复制、packageId/moduleId、signature、exportId、类型字符串构造器 | `export_alias.xl` 检查 callingConvention/varargMode、`import_metadata.xl` 检查 resolved package、package fixture 的 `package.xapi.json`、`import_xapi_use.xl` | 直接 JSON 检查 + import 消费 | package/module 身份、导出唯一身份、签名和 import target 去引号有覆盖。 |

## XAPI Module 构建与 JSON 输出

| 规则范围 | 当前验证用例 | 证据等级 | 核验结论 |
|---|---|---|---|
| `CGEN-XMOD-001..008` class/union AST 访问、字段计数、inline union 展开、字段模型、class shape | `xapi_type_fields.xl`；`test.bat` 检查 `"kind":"class"`、`"base":"Packet"`、`"typeParams":["T"]`、`"storage":"inline-union"`、`"kind":"custom"`；`export_union.xl` dump-dir 检查 union fields | 直接 JSON 检查 + loader 回读 | class base、泛型参数、class/union 字段、inline union storage、custom type 有强覆盖。 |
| `CGEN-XMOD-009..011` module 初始化、完整释放、数组预分配 | `xl xapi` 成功/失败路径、`xapi_load_types_host.c`、import/package 主回归 | 间接覆盖 + C 宿主运行 | module build/free 在所有 XAPI 输出和读取测试中反复执行；内存不足路径未直接模拟。 |
| `CGEN-XMOD-012..013` export function/var/const 模型构建 | `export_alias.xl`、`export_c_abi_types.xl`、`export_vars.xl`、`import_xapi_use.xl`、`import_xapi_var_use.xl` | 直接 JSON/IR/C 检查 + import 消费 | callingConvention、varargMode、required/fixed 参数数、params/returns、var/const type/cSymbol 有覆盖。 |
| `CGEN-XMOD-014..016` 默认身份、entry kind、export/type/import 预统计 | `dump_hello/hello.xapi.json`、`import_metadata.xapi.json`、`xapi_type_fields.json`、export/import/class/custom/union 混合用例 | 直接 JSON 检查 | 默认 app 身份、entry、exports/types/imports 数组存在性和数量路径有覆盖。 |
| `CGEN-XMOD-017..020` module 填充 export/type/import 数组、resolver 结果、最终计数提交 | `import_metadata.xl`；`test.bat` 检查 `"resolved":true`、`xlang.package.json`、`counter`、`local/math`；`xapi_type_fields.xl`；`export_alias.xl` | 直接 JSON 检查 + import 消费 | export/type/import 三类数组填充和 resolver 回填 manifest/interface/packageId 有覆盖。 |
| `CGEN-XMOD-021..024` executable entry、entry JSON、字符串数组、固定头部/package/module 输出 | `dump_hello/hello.xapi.json` 检查 `xlang-xapi-1`、package fixture、`metadata_combo/package.xapi.json`、`import_metadata.xapi.json` | 直接 JSON 检查 | format、compiler/target、package/module、entry、links/loadlibs/cSources 基础输出与组合字段输出有覆盖。 |
| `CGEN-XMOD-025..028` exports/types/imports/links/loadlibs/cSources/native/docs JSON 输出 | `export_alias.xl`、`xapi_type_fields.xl`、`import_metadata.xl`、`metadata_combo/package.xapi.json`、`import_metadata_combo.xl`、stdlib/package 构建用例 | 直接 JSON 检查 + import 消费 + build/run | 完整 exports/types/imports、native/package 相关字段输出有覆盖；links/loadlibs/cSources/native 同包组合由 `metadata_combo` 固化。 |
| `CGEN-XMOD-029` 公开 XAPI JSON dump 包装 | 所有 `release\xl.exe xapi ... --emit ...`、`xl cgen --dump-dir` 生成 `.xapi.json` | 直接工具链检查 | 公开 dump 入口 build model -> dump JSON -> free module 的主路径有覆盖。 |

## XAPI 读取与模型恢复

| 规则范围 | 当前验证用例 | 证据等级 | 核验结论 |
|---|---|---|---|
| `CGEN-XLOAD-001..006` JSON parser 状态、空白/consume、字符串/bool/int、未知 value 跳过 | `import_broken_xapi.xl`；`test.bat` 检查格式错误诊断含 `xlang-xapi-1`；`xapi_load_types_host.c` 读取真实 JSON；`xapi_loader_defaults_host.c` 读取 `xapi_unknown_fields.xapi.json` | C 宿主运行 + 语义负例 + 手写 fixture | parser 基础读取、format 错误、真实 JSON 成功解析和未知 root/package/module/export/param/return/type/field/native 字段跳过均有覆盖。 |
| `CGEN-XLOAD-007..012` param/return/field 动态追加、字符串数组、doc、参数/返回数组读取 | `xapi_load_types_host.c release\tmp\xapi_type_fields.json`、`xapi_doc.xl`、`export_alias.xl`、import XAPI function/var/type 系列 | C 宿主运行 + import 消费 | params、returns、fields、typeParams、doc raw 恢复有覆盖。 |
| `CGEN-XLOAD-013..018` exports/types 对象和数组读取 | `import_xapi_use.xl`、`import_xapi_var_use.xl`、`import_xapi_type_union_use.xl`、`import_xapi_type_class_use.xl`、`xapi_load_types_host.c` | IR/C 检查 + C 宿主运行 + import 消费 | function/var export、class/custom/union type、字段恢复和后续 AST 注入有覆盖。 |
| `CGEN-XLOAD-019..022` package/module/native/root format 读取 | package fixture 的 `package.xapi.json`、`import_metadata.xl`、`import_broken_xapi.xl`、`import_missing_xapi.xl` | import 消费 + 语义负例 | package/module/native/root format 读取、缺失接口和错误格式诊断有覆盖。 |
| `CGEN-XLOAD-023..028` 读取侧 packageId/moduleId/signature/exportId 默认补齐、function ABI 默认补齐 | `export_alias.xl` 检查 `callingConvention`/`varargMode`、import XAPI fixtures、package resolver 用例；`xapi_loader_defaults_host.c` 读取 `xapi_legacy_defaults.xapi.json` | 直接 JSON 检查 + import 消费 + 手写 fixture | 缺省 package/module 身份、source/sourceHash、native load/version、function callingConvention/varargMode、signature/exportId、field storage 均有直接 host 断言。 |
| `CGEN-XLOAD-029..031` XAPI 文件读取、尾部校验、finalize 与失败清理 | `import_missing_xapi.xl`、`import_broken_xapi.xl`、所有 import XAPI 成功用例 | import 消费 + 语义负例 | UTF-8 文件读取、无法读取、尾部/format 错误和 finalize 成功主路径有覆盖。 |

## 剩余风险与后续增强

| 缺口 | 建议补法 |
|---|---|
| JSON 字符串转义的引号、反斜杠和 `\n` 文本已由 `xapi_json_escape.xl` 覆盖。 | 后续可补 Unicode 控制字符和非 ASCII doc 文本的专门快照。 |
| XAPI loader 的未知字段跳过、缺省字段补齐已由 `xapi_loader_defaults_host.c`、`xapi_legacy_defaults.xapi.json`、`xapi_unknown_fields.xapi.json` 覆盖。 | 后续可补畸形未知值负例，但当前合法旧格式与合法未知字段跳过路径已有直接 host 断言。 |
| links/loadlibs/cSources/native 的组合输出已由 `metadata_combo/package.xapi.json`、`import_metadata_combo.xl` 与 `import_deps_snapshot_host.c` 覆盖。 | 当前 JSON 字段、实际 build/run 链路和最终 importDeps 机器快照均有直接证据；后续可扩展更多依赖图组合。 |

## 阶段结论

XAPI 生成、JSON 输出、doc、module 构建与读取恢复已经完成矩阵级整理；import/package 矩阵已落在 `41_P2_import_package验证矩阵.md`。
