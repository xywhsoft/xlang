# volc.phone 切片函数参数 ABI 不一致

`src/phone.xl` 将 `x_volc_phone_substr` 声明为接受两个 XLang `int`
（64 位），`x_volc_phone_native.c` 的定义却接受两个 C `int`（32 位）。
不导入已被 `xhttp.multipart` 阻断的包，也能从 `demo6` 目录直接复现：

```powershell
.\release\xl.exe check dev\extlib_blockers\volc_phone_substr_abi.xl
.\release\xl.exe run dev\extlib_blockers\volc_phone_substr_abi.xl release\extlibs\volc.phone\x_volc_phone_native.c
```

当前第一条命令退出 0；第二条在 `x_volc_phone_native.c:44` 报
`incompatible types for redefinition of 'x_volc_phone_substr'`，退出 1。
如果拆开编译而绕过原型检查，`4294967296` 可能窄化成零下标；复现中的
断言在修复后还应验证大下标不会返回首字符。

调用方还将 `string.len/indexOf` 的字符位置传给按 `strlen` 字节位置切片
的 C 实现；仅把 XLang 参数写成 `c_int` 不能修正索引单位，反而保留截断。
该 helper 同时使用未上锁的进程共享返回槽，见既有导入阻断记录。
待 `xhttp.multipart` 导入链恢复后，优先以结构化 JSON 解析替代这套手写
位置切片并删除 helper；若确需保留，C 接口须接受 64 位整数并完整校验
范围、索引单位和返回值生命周期。当前不修改被阻断包的实现，也不把
最小复现的编译失败当作已修复。
