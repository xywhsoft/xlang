# SHA3 导出类的私有句柄构造器仍可被包外调用

状态：blocked-upstream。普通扩展库任务不修改 XLang 编译器。

## 最小复现

在 `demo6` 根目录只做编译检查，绝不运行负例：

```text
release\xl.exe check release/extlibs/sha3/test/private_adopt_constructor_negative.xl
```

`Sha3Hasher` 和 `ShakeHasher` 的 `create(point handle)` 已标记为
`private extern`，包外的 `Class(null)` 仍输出 `ok`、退出码 0；
期望为访问权限错误。`null` 只证明调用可达，不触发析构危险路径。
如果包外传入伪造非空指针，接管和析构可能访问或释放不属于自身的内存。

对照：`private_update_negative.xl` 的包外 `updateNative(point, size)`
已被拒绝，两平台门禁验证该边界；SHA3 的公开工厂、clone 和
完整文件范例仍正常运行。方法边界修复不代表构造器边界已修复。

## 主线修复与复验

构造器重载解析和导出类访问控制需落实 `private extern create`：
包内 clone 可以接管真实句柄，包外不能直接构造。修复后本负例必须
非零退出，并明确报告访问权限错误；随后复跑 Windows/Linux 包门禁、
完整双语范例和 AOT。不得运行非空伪造指针负例，也不以包侧裸指针
有效性检查替代编译器访问控制。

2026-10-21：当前发布 `xl.exe` 的 `xl check` 仍返回 `ok`。
