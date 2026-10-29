# 普通 class 析构体写字段触发运行时状态错误

2026-10-26 使用当前 `release/xl.exe`（SHA-256
`CCF98FF05BAAA71C1827EAA0C8B7A011A9A17EAB1AECD593CD89848D227C5A2B`）
从 `demo6` 目录复现；该发布物的 `xlc.dll` SHA-256 为
`5522CC72A8DA92DB569D559FBD7B0A976942459BC9C06E0EB72493DD532D8261`。

`release/extlibs/serial/test/class_teardown_control.xl` 不导入任何扩展库：

```xlang
class CleanupProbe
    int value = 1

    destroy()
        self.value = 0
    end
end

func main(...) as int
    CleanupProbe probe = CleanupProbe()
    return 0
end
```

执行 `release/xl.exe run release/extlibs/serial/test/class_teardown_control.xl`
退出 1，报 `RuntimeError: operation is not valid in the current state`。
同文件以 `release/xl.exe build ... -o build/test/serial_guard/class_teardown_control.exe`
构建成功，但独立 EXE 也在清理阶段报同错并退出 1。

对照：`local_class_destroy_control.xl`（无显式析构）和
`local_class_empty_destroy_control.xl`（析构体为空）在 JIT 与独立 EXE
均退出 0。因而目前可证实的是**普通 class 析构体中的字段写入**边界，
不能直接等同于另一个“顶层 native-backed class 仅 AOT 退出失败”问题。

`serial` 的原生 C、分配故障和 ABI 检查通过，但当前默认 Windows 门禁
在 `xlang-jit` 首先报上述状态错误。公开串口范例已打印正确输出与
`serial example: PASS`，随后仍因清理错误退出 1。`Serial.destroy`
释放原生句柄后会写 `self.handle = null`，与独立最小用例同形；尚未证明
它们在运行时共享同一底层调用点。本包不删掉赋值来掩盖主线缺陷。

建议主线检查普通 class 析构调用期间 `self` 的状态、字段写入权限和
对象退役/错误作用域顺序：在析构体完成前对象应仍可访问自身字段，
析构结束后再转入不可用状态。修复后先复跑上述三个无扩展库用例的
JIT/独立 EXE，再运行 `tools/check_extlib_serial.ps1` 默认门禁和 Linux
`release/extlibs/serial/test/check.sh`。此任务不修改编译器或运行时。
