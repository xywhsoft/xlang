# zip.ZipArchive 的私有内存构造器仍可被包外调用

状态：blocked-upstream。普通扩展库任务不修改 XLang 编译器。

## 最小复现

当前 `release/xl.exe` SHA-256：`616888E9DA0097E5449CB4ABD25FC42F87606D836611B3D739860CE57E2D1E3F`。
在 `demo6` 目录仅执行编译检查，绝不运行负例：

```text
release\xl.exe check release/extlibs/zip/test/private_memory_constructor_negative.xl
```

导出类 `ZipArchive` 的 `create(point data, c_size size, ...)` 已声明
为 `private extern`，但包外以 `null`、非零长度调用时仍返回 `ok`、
退出码 0；预期是访问权限错误。公开 `zip.open(Buffer)` 会从 Buffer
取得真实地址和长度并检查选项，而薄绑定的内存构造器把调用方的
裸地址及长度直接传入 XRT ZIP。此文件只作编译检查，不能运行。

对照：`release\xl.exe run release/extlibs/zip/test/boundaries.xl`
正常通过，公开内存及文件归档路径未因私有标记失效。

## 修复建议与复验

在主线语义和构造器重载解析中落实导出类的
`private extern create`：包内工厂可调用，包外必须收到访问权限错误。
修复后先要求本负例非零退出，再复跑 ZIP Windows/Linux C、XLang、
双语范例与完整 AOT。包内还存在公开原生成员方法，须另做可见性
审计；这项主线修复本身不解决已记录的解压目录并发替换风险。
