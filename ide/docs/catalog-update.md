# 命令目录与来源资料更新

2026-10-31：静态目录含 **14,185** 条命令，其中语言/标准库原有 **2,744** 条保持不变，扩展 **11,441** 条，覆盖 **207** 个活动公共包。共享文档 **134** 份（6 份设计资料、128 份包 README），实际引用来源 **596** 份。命令位于左侧、帮助位于编辑器下方的原生布局不变。

## 扩展日常更新

在仓库根目录执行：

```powershell
./ide/tools/generate_catalog.ps1 -ExtensionsOnly -PrepareOnly -GccPath E:/software/w64devkit/bin/gcc.exe
```

目标为新建的 `ide/build/cg-编号`；可指定尚不存在的 `-OutputDirectory`。GCC 已进入 PATH 时可省略其参数。此路径保留语言/标准库快照，按当前 `release/extlibs/*/xlang.package.json` 的 `artifact.xlSources` 重新提取扩展，不需要旧的完整标准库构建缓存。

扩展提取器编译未修改的工作区 lexer/parser/AST 前端，以发布版 XRT 提供者链接声明导出工具。编译器提供声明的 UTF-8 字节范围，工具不另写语言解析器，不执行扩展，也不把整个大类/函数体重复写入每条帮助。输入、工具、前端、运行库指纹和原始资料均留在生成目录，生成期间发生变化即拒绝交付。

收录函数/重载、类与泛型、构造、属性 get/let、方法、字段、常量、变量、枚举、union、别名及声明内 `#doc`。工厂返回的未导出类可查其实例方法，但明确标注不能直接构造，且不进入全局静态补全。私有/protected、native 句柄、析构钩子及六个内部包不作为公共命令。

检查资料是否跟得上当前源码：

```powershell
python -B -X utf8 ide/tools/refresh_extensions.py --check
```

扩展 API、README、清单、提取工具或前端变化会使旧资料检查失败；新增/删项必须同阶段同步。此检查已进入根目录 `tools/check_release_extlibs.ps1`。长期规则见[扩展库 IDE 同步规则](../../docs/扩展库IDE命令目录同步规则.md)。覆盖目录不等于成熟度认证，不按函数存在就宣称包生产可用。

## 完整生成

不带 `-ExtensionsOnly` 时，仍先验证 `data/portable-build-inputs.json` 指定的源码、编译器、三个运行时与独立构建证明，生成固定语言/标准库资料，然后复用同一扩展阶段。六份设计资料默认取仓库根 `docs`，可指定 `-DocumentationRoot`。本轮只刷新扩展，没有重生成或伪造固定语言/标准库的版本证据。

旧语言/标准库来源中的 `demo6/...` 是既有快照的逻辑身份，不是去旧目录找文件的兼容路径；原始字节和摘要保留。新扩展来源一律为根目录 `release/extlibs/...`，读取时走配对快照，不映射旧目录。

## 发布与恢复

核对生成报告和聚焦测试后，从仓库根目录发布资料对：

```powershell
python -B -X utf8 ide/tools/catalog_publish.py --pair ide/build/cg-实际编号 --data ide/data
```

省略 `-PrepareOnly` 可在生成成功后自动发布。发布沿用写入锁、新旧完整资料和日志，成功后才 committed；普通异常回退，进程中断后下一次发布先恢复，或执行 `./ide/tools/generate_catalog.ps1 -Recover`。不允许仅复制 `commands.json` 而保留旧来源目录。

两项磁盘资源无法一次原子替换；短暂不匹配时 IDE 明确报错，不展示另一版正文。发布后使用“重新加载命令目录”。新增 `field`/`variable` 类型需本轮 IDE 读取器，旧二进制须重建；旧便携包不会因源资料更新自动重发。

## 验收入口

`check_extension_catalog.py` 使用真实声明工具验证 UTF-8/BOM/CRLF、中文参数、嵌套默认值、可变参数、重载、属性、枚举载荷、工厂实例表面、删项、包移除、私有排除、坏源拒绝和 1,800 个方法的大类；传入 `--pair` 还核对全目录来源行、公共包集合、非扩展原样保留以及变动拒绝。

`catalog_test` 验证原生加载、检索、签名、类型、C 候选隔离、实例字段/成员不混入全局补全、错误后保留旧目录。`check_catalog_publication.py` 对独立副本执行 14 项发布/回滚/真实进程中断/写入锁/篡改拒绝测试。原 IDE 全套界面和便携验收记录是历史证据，不能据此声称本轮重复执行；本轮证据见[覆盖审计](../../docs/IDE扩展库命令覆盖审计_2026-10-31.md)。
