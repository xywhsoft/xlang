# stb_image.StbImage 的私有句柄构造器仍可被包外调用

状态：blocked-upstream。普通扩展库任务不修改 XLang 编译器。

## 最小复现

当前 `release/xl.exe` SHA-256：`616888E9DA0097E5449CB4ABD25FC42F87606D836611B3D739860CE57E2D1E3F`。
在 `demo6` 目录仅执行编译检查，绝不运行负例：

```text
release\xl.exe check release/extlibs/stb_image/test/private_adopt_constructor_negative.xl
```

导出类 `StbImage` 的 `create(point image)` 已标为 `private extern`，
但包外 `StbImage(null)` 仍返回 `ok`、退出码 0；预期是访问权限错误。
`null` 只用于证明可达性。薄绑定 `xstbi_image_adopt` 原样返回指针，
析构会释放图像数据和句柄；伪造非空指针可能被解引用或释放。
不得运行此负例，也不得以非空伪造指针测试。

对照：`release\xl.exe run release/examples/extlibs/stb_image.xl`
正常通过，文件/内存工厂与像素读取路径未因私有标记失效。

## 修复建议与复验

主线语义及构造器重载解析应拒绝包外调用导出类的
`private extern create`，同时允许包内工厂接管原生句柄。
修复后本负例必须非零退出并报告访问权限错误，再复跑
Windows/Linux C、XLang、双语范例及完整 AOT 门禁。
不要通过检查任意裸指针是否有效来代替编译期访问控制。
