# floatEnv.Snapshot 的私有原生句柄构造器仍可被包外调用

状态：blocked-upstream。普通扩展库任务不修改 XLang 编译器。

## 最小复现

当前 `release/xl.exe` SHA-256：`616888E9DA0097E5449CB4ABD25FC42F87606D836611B3D739860CE57E2D1E3F`。
在 `demo6` 目录仅执行编译检查，绝不运行负例：

```text
release\xl.exe check release/extlibs/floatEnv/test/private_adopt_constructor_negative.xl
```

导出类 `Snapshot` 的 `create(point handle)` 已标为 `private extern`，
但包外 `Snapshot(null)` 仍返回 `ok`、退出码 0；预期是访问权限错误。
`null` 只用于安全证明可达性。C 的 `xfenv_snapshot_adopt` 原样返回指针，
`xfenv_snapshot_destroy` 调用 `xrtFree`；伪造非空句柄进入语言对象后
可能在恢复、hold 或析构时访问非法内存。不得执行非空指针探针。

对照：`release\xl.exe run release/extlibs/floatEnv/test/boundaries.xl`
正常通过，工厂捕获、hold/restore 边界未因私有标记失效。

## 修复建议与复验

主线语义与构造器重载解析应拒绝包外调用导出类的私有构造器，
同时允许包内成员接管句柄。修复后本负例须非零退出并明确报告
访问控制，再复跑 Windows/Linux C、XLang、双语范例及完整 AOT。
不要通过校验任意裸指针是否有效来代替编译期访问控制。
