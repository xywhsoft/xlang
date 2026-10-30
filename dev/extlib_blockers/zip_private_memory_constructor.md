# ZIP 包外私有内存构造器（M3）

## 当前复现

从仓库根目录运行，只做语义检查，不执行带伪造地址的程序：

```powershell
release/xl.exe check release/extlibs/zip/test/private_memory_constructor_negative.xl
```

`src/zip.xl` 的 `ZipArchive` 已把接收 `point data, c_size size` 的
`extern create` 声明为 `private`。导入方仍能直接选中这一重载；
2026-10-30 固定 Windows 发布物返回 `ok`、退出 0，而正确结果必须是
访问控制诊断、退出 1。同包私有 `addNative` 成员负例已被正确拒绝，
因此不能用一般私有方法通过来代替构造器边界。

完整门禁 `release/extlibs/zip/test/check.ps1` 最后一项强制检查此负例，
不会再把只完成运行功能的门禁标成完整通过。该次 Windows 记录：
`build/test/zip_guard_547df19697cb4aba8552eff69eec9b60.report.json`，
176 阶段中前 175 项符合各自预期，末项实际退出 0 而要求 1。
Linux 的默认 `test/check.sh` 也保留同一必拒绝条件。

同日最终双平台默认门禁在完成新普通功能测试后也保留此项：Windows
`zip_guard_77b92a54f8194de68a03e070756de133.report.json`，Linux
`/var/tmp/xlang-zip-commit-audit-vTSMUrPs/tmp/xlang-zip-LWdcv7`。
原始 `xl check` 均仍为 0；负例验证器返回 1 表示未满足拒绝要求，
不是编译器已经拒绝。本轮还分别发现独立的 M2 与 M12，详包审查。

## 主线修复建议

在构造器重载解析和选中后的访问控制中使用实际声明的可见性及定义类/
模块作用域；对 native-backed 和普通类一致执行。主线应同时补测本类
合法调用、包内工厂、包外限定名、导入别名、公开/私有同名重载，避免
只按参数类型选择构造器后漏掉访问检查。具体修复位置仍须由主线审查
当前实现确定，本扩展库任务不修改语义分析器或降低可见性要求。

公共调用继续使用 `zip.open(Buffer)`，其薄绑定从真正的 Buffer 读取
地址和长度。这不是规避测试，也不是允许任意 `point` 构造。
不得运行本负例来确认危险地址是否崩溃。

此项与 RFS 并发目录替换独立：后者需要冻结 XRT 之外的根锚定文件提交
能力，不因私有构造器将来修好而自动关闭。
