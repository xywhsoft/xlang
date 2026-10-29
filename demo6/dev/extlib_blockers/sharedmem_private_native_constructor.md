# sharedmem 导出类的私有原生构造器仍可被包外调用

状态：blocked-upstream。普通扩展库任务不修改 XLang 编译器。

## 最小复现

当前 `release/xl.exe` 的 SHA-256：`616888E9DA0097E5449CB4ABD25FC42F87606D836611B3D739860CE57E2D1E3F`。
在 `demo6/release` 执行：

```text
xl check extlibs/sharedmem/test/private_constructor_negative.xl
```

`SharedMemory` 是导出类型，其 `create(point raw)` 已声明为 `private extern`。
包外的 `SharedMemory(null)` 仍返回 `ok`、退出码 0；预期为访问权限错误。
**只运行编译检查，不运行该负例。** 非空任意指针也可能进入析构路径。
对照负例 `private_native_negative.xl` 访问私有 `writeNative` 会正确收到
`no matching callable class member ... access`，公开工厂的正常边界用例通过。

## 影响与修复建议

导入方可绕过 `sharedmem.create/open` 对名称与资源来源的校验，构造未经
本包创建的句柄。`xshm_adopt` 只是接管指针，C 端无法可靠辨认任意无效地址；
不应通过探测指针或新增兼容包装掩盖类访问控制问题。

修复主线对导出类 `private extern create` 的可见性传播及构造器重载解析：
包内工厂可调用，包外调用必须被拒绝。修复后要求本负例的 `xl check`
非零退出且明确报告访问控制，再复跑 `sharedmem/test/check.sh`、Windows
C/XLang 用例与完整 AOT。带输出 AOT 目前还受共享快照符号阻断。
