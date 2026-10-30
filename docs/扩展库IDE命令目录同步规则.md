# 扩展库 IDE 命令目录同步规则

定版：2026-10-31。此规则加入扩展库长期任务的阶段验收，不改变普通队列的 100 包范围。

## 必须同步

- 新增/移除包或公开 API、重命名、签名与默认参数变化，必须同步 `ide/data/commands.json` 和配对的 `ide/data/catalog-sources/`，在同一阶段提交中交付。
- 覆盖函数及重载、类与泛型参数、构造函数、方法、get/let 属性、公开字段、常量、变量、枚举、union 和类型别名。工厂公开返回的未导出类也收录其实例表面，但不作为可直接构造/导入的静态补全候选。
- 以各包 `xlang.package.json` 的 `artifact.xlSources` 为声明入口；不扫描测试/范例拼凑 API，不执行扩展代码，不解析 C 来猜测 XLang 声明。六类内部依赖不作为公共包列出，私有、protected、native 句柄和析构钩子不进入用户命令列表。
- README 和声明内 `#doc` 一并捕获。没有的说明/范例不虚构。目录覆盖表示声明已列出，绝不表示包已通过生产验收；占位包状态仍由发布验收矩阵负责。

## 日常操作

在仓库根目录执行（GCC 在 PATH 中时可省略 `-GccPath`）：

```powershell
./ide/tools/generate_catalog.ps1 -ExtensionsOnly -GccPath E:/software/w64devkit/bin/gcc.exe
python -B -X utf8 ide/tools/refresh_extensions.py --check
```

先验后发布时加 `-PrepareOnly -OutputDirectory build/test/本次目录`，再执行：

```powershell
python -B -X utf8 ide/tools/check_extension_catalog.py --exporter build/test/本次目录/frontend/dump-extension-catalog.exe --pair build/test/本次目录 --output build/test/本次测试
python -B -X utf8 ide/tools/catalog_publish.py --pair build/test/本次目录 --data ide/data
```

生成阶段编译一个声明专用工具：复用未修改的现有 lexer/parser/AST 前端和发布版 XRT 提供者，不新增语言解析器，也不重新编译一份 XRT。每个声明独立输出范围，大类无需把整个函数体复制进帮助。生成失败不触碰已安装数据；发布沿用已有锁、回滚与中断恢复，不能只替换 JSON。

扩展快速更新保留现有语言/标准库条目及其来源原始字节，不要求重建固定的标准库工具链缓存。完整重生成仍须准备 `ide/data/portable-build-inputs.json` 的匹配源码、编译器及构建证明，不能借快速刷新伪造那部分版本证据。

## 提交验收

1. 聚焦包测试与范例通过，公开声明与 README 定稿。
2. 生成资料对；新增声明形态时扩充 `catalog_extension_declarations.xl` 和 `check_extension_catalog.py`，验证真实前端、UTF-8 来源范围、可见性、重载与删项。
3. 运行新鲜度检查。输入、工具或前端变动均拒绝旧快照；`check_release_extlibs.ps1` 也执行此检查，不会静默自动修复旧数据。
4. 同阶段提交库代码、测试、范例、必要文档及 IDE 资料。只提交本次路径，不纳入并行工作，不自动推送。

IDE 的目录类型读取器有变化时需重新构建 IDE；只替换数据不能让旧二进制识别新类型。构建/便携交付使用生成后的资料对。仓库历史便携包和截图不因源数据刷新而被宣称已经重发。
