# 命令目录更新与验收

命令目录来自当前共享 `release/` 的编译器及其对应源码，不再使用旧的便携编译器快照。编译器源码固定到 `docs/reports/prerelease-0.6.0.json` 记录的 Windows 构建版本；三份产品的 SHA-256 必须与报告一致。编译器目录有未纳入该构建的源码修改时，先构建并完成相应验收，再更新目录。IDE 的并行源码修改不进入编译器输入范围。

在 `ide/` 下运行：

```powershell
.\tools\generate_catalog.ps1 -PrepareOnly -GccPath E:\software\w64devkit\bin\gcc.exe
python -X utf8 tools/check_catalog_update.py --pair build/cg-实际编号 --output build/catalog-check-新的编号 --cc E:/software/w64devkit/bin/gcc.exe
python -X utf8 tools/catalog_publish.py --pair build/cg-实际编号 --data data
python -X utf8 tools/check_catalog_coverage.py --catalog data/commands.json
```

`-ReleaseReportPath` 可以选择新的构建验收报告，`-Root` 指定其仓库。输出目录必须尚不存在。省略 `-PrepareOnly` 会在覆盖校验通过后直接发布到 `ide/data`。资料更新本身不重编译 GUI 或语言服务；语义服务的编译和验收独立进行。2026-10-31 扩展目录加入 `field`/`variable` 类型及禁止直接补全的工厂返回类型，需要本轮 IDE 读取器；旧二进制不能只替换数据就视作更新完成。

## 扩展日常刷新

仅更新扩展时使用同一入口的 `-ExtensionsOnly`，保留现有 0.6.0 的 6,156 条语言/标准库记录、API 清单、编译器绑定及来源字节。完整生成则先运行当前发布标准库展开器，再复用同一个扩展提取阶段，不维护两套扩展规则。

```powershell
.\tools\generate_catalog.ps1 -ExtensionsOnly -PrepareOnly -GccPath E:/software/w64devkit/bin/gcc.exe
python -B -X utf8 tools/refresh_extensions.py --check
```

扩展提取器直接编译未修改的共享 lexer/parser/AST，以编译器的 UTF-8 半开范围取得签名和默认参数，不猜测括号或复制大类函数体。支持函数/重载、构造、get/let、字段、常量、变量、枚举载荷、union/别名、`#doc` 以及公开工厂返回的未导出实例类型。私有/protected、native 句柄、析构钩子及六个内部包不进入公共列表；未导出类型明确提示不能直接构造，不作为全局静态补全候选。

本轮合并后为 **17,598** 条记录：语言 168、标准库 5,988、扩展 11,442；207 个公共包，145 份文档，593 份实际引用来源。`extensionCatalog` 保存当前输入、前端和工具指纹；源码/API/README/包增删会使旧目录检查失败。该检查同时进入 IDE 覆盖校验和根扩展发布门。详见[同步规则](../../docs/扩展库IDE命令目录同步规则.md)及[主工作区同步记录](../../docs/IDE扩展库命令同步记录_2026-10-31.md)。

## 数据来源

生成工具导出并核对实际函数／常量登记表。原生辅助程序以相同 Git 版本的源码编译，运行真实的标准库展开器，观察其生成声明；不会执行测试引用里的构造函数、网络操作或标准库业务函数。引用集合覆盖所有导出的类片段、成员名、函数名和高级模块，由真实展开器选择完整的条件声明，不再用文本拼接模拟条件分支。

捕获的 `src/stdlib/catalog.declarations.xl` 由当前发布的 `xl.exe parse` 重新解析，生成函数、重载、泛型、构造函数、getter、公开字段和枚举值的签名。内部名称、private／protected 声明和 native 存储字段不作为命令。公开字段使用目录格式 1 已有的成员类别 `method`，另存 `declarationKind=field`，签名不含调用括号；它们可供查阅，静态后备补全不会将它们插入全局。此做法保持现有 GUI 的目录格式兼容。枚举值使用 `constant` 类别。目录 JSON／索引固定 LF，内容寻址的 `.source` 文件禁止 Git 文本转换，保证新检出的字节仍与文件名摘要一致。

标准库类的“查看声明来源”显示实际展开的 xlang 声明字节与正确行号。常量定位其 C 登记片段。模块帮助引用真实项目文档；HTTP、WebSocket、网络、TLS、密码学、压缩和模板等分类单独提供资料。高级模块保留显式导入提示。扩展包仅从当前 `xlang.package.json` 所列 `artifact.xlSources` 提取公开声明，已删除包不保留旧命令。

## 校验与发布

- `check_catalog_coverage.py` 检查当前编译器绑定、源码版本、公开函数／常量／类型／成员覆盖、内部字段排除、签名摘要及全部来源字节。
- `check_catalog_update.py` 检查缺项／错误默认参数／存储字段泄漏负例、原生目录读取和检索、确定性再生成，以及原有发布工具的异常回退、并发锁和真实进程中断恢复。
- IDE 正常构建在复制资料前执行覆盖校验，并使用 `catalog_publish.py` 成对更新目录与来源；旧资料过期时停止交付并提示重新生成。

`commands.json` 与 `catalog-sources` 必须成对发布，禁止只复制一个 JSON。事务目录必须与目标数据目录位于同一文件系统，以保证目录 rename 的原子性；默认的 `ide/build/catalog-publications` 与本仓库共享发布目录同盘。发布工具保留事务记录和旧资料，异常时回退；中断后使用：

```powershell
python -X utf8 tools/catalog_publish.py --data data --recover
```

共享 `release/data` 同样使用上述发布工具；其安装回执和 `provenance/ide-catalog-0.6.0.json` 记录实际更新范围、产品哈希与验收结果。构建来源和当前编译器绑定验证不等于完整 GUI、语言服务或标准库逐接口运行验收。

当前生成与验收的机器可读记录见 [目录构建回执](../data/catalog-build-receipt.json)。
