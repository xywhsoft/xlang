# DEMO6 package/import 测试资产

本目录保存 import、package resolver、XAPI 接口元数据、native source/link/loadlib 和依赖版本隔离相关测试资产。这里的包是测试 fixture，不作为用户级标准扩展库发布。

## 使用方式

- `xlang.package.json` 是 package manifest，描述包名、版本、ABI 版本、XAPI 文件、C source、links/loadlibs、dependencies 和 native 元数据。
- `package.xapi.json` 是接口元数据，描述导出函数、变量、类型和 import 恢复所需信息。
- 包内 `.c` 文件是 native fixture，由 mixed build 或 import/package 回归按需编译链接。
- 负例包会故意缺失接口、使用无效格式或声明不可满足依赖，用于验证诊断。

## 包目录索引

<!-- package-index:start -->
- `broken_xapi`：无效/破损 XAPI 格式负例，验证格式诊断。
- `class_api`：导入包内 class 类型、字段、函数和 native C source。
- `class_inherit_api`：导入带继承关系的 class 类型和 native C source。
- `counter`：基础 package import、extern 函数注入和 native C source。
- `dep_value`：依赖版本 1.0.0 的基础包。
- `dep_value@2.0.0`：依赖版本 2.0.0 的并存包。
- `invalid_decl_xapi`：XAPI 生成非法声明负例。
- `metadata_combo`：cSources、links、loadlibs、native singleton metadata 组合包。
- `missing_interface`：缺失 `package.xapi.json` 负例。
- `native_one`：native singleton 冲突负例的一侧。
- `native_two`：native singleton 冲突负例的另一侧。
- `settings`：导入 extern 变量的基础包。
- `union_api`：导入包内 union 类型、变量和 native C source。
- `unsupported_xapi`：当前 C ABI bridge 不支持声明负例。
- `uses_dep`：依赖 `dep_value@1.0.0#abi1` 的正向包。
- `uses_dep_bad`：依赖不存在的 `dep_value@3.0.0#abi1` 负例。
- `uses_dep_v2`：依赖 `dep_value@2.0.0#abi1` 的正向包。
<!-- package-index:end -->

## 主要验证入口

在 `demo6` 目录下运行：

```bat
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_package_fixtures.ps1
```

完整 package/import 行为回归由 `test.bat` 覆盖，包括：

- XAPI 注入 extern 函数/变量。
- 包内 class/union 类型导入、字段访问和 C ABI 生成。
- 包内继承关系恢复。
- 私有依赖不污染用户命名空间。
- 依赖版本并存与版本不匹配诊断。
- 缺失接口、破损 XAPI、非法声明、不支持导入和 native singleton 冲突诊断。
- manifest 与 XAPI 的 `packageId`、`moduleId`、`exportId`、`cSources`、links/loadlibs 和 native metadata 自洽性。
