# stb_truetype 导出类的私有原生句柄构造器仍可被包外调用

状态：blocked-upstream。普通扩展库任务不修改 XLang 编译器。

## 最小复现

当前 `release/xl.exe` 的 SHA-256：`616888E9DA0097E5449CB4ABD25FC42F87606D836611B3D739860CE57E2D1E3F`。
在 `release` 执行，仅使用 `check`，不可运行负例：

```text
xl check extlibs/stb_truetype/test/private_font_constructor_negative.xl
xl check extlibs/stb_truetype/test/private_bitmap_constructor_negative.xl
```

`Font` / `Bitmap` 均为导出类；它们的 `create(point)` 已声明为
`private extern`，但包外 `Font(null)` / `Bitmap(null)` 都返回 `ok`、
退出码 0。预期是访问权限错误。这里的 `null` 只为安全地证明可达性；
任意非空伪造指针会由 `xstbtt_*_adopt` 原样接管，随后在析构时解引用。

对照：`xl check extlibs/stb_truetype/test/private_bitmap_negative.xl`
访问私有 `Font.bitmapNative`，会正确报告类成员访问错误；包内
`load/loadBytes` 和 `font.bitmap` 的正常用例可运行。

## 修复建议

在主线语义与构造器重载解析中落实导出类 `private extern create`：
包内工厂及成员可调用，包外调用必须拒绝。修复后先要求两条构造器
负例的 `xl check` 非零退出且诊断指向访问控制，再复跑包级 Windows/
Linux C、XLang、双语门禁与完整 AOT。不能通过探测任意指针是否合法，
也不应为规避此缺陷取消公开 Font/Bitmap 类型或制造重复包装类型。
