# singleinstance 双语范例

从仓库根目录运行；Windows 命令为 `release/xl.exe`，Linux 为 `./release/xl`。
不依赖外部服务。示例按真实应用使用固定锁名：同时启动第二份时会返回
“已有实例”分支，而不是保证两个程序都通过成功输出。

| 场景 | XLang | C | 正常单实例预期 |
| --- | --- | --- | --- |
| 基础获取/查询/释放 | `release/examples/extlibs/singleinstance.xl` | `singleinstance_c.c` | `../test/singleinstance.expected.txt` |
| 错误后恢复与必定释放 | `recovery.xl` | `recovery_c.c` | `recovery.expected.txt` |

```powershell
release/xl.exe run release/extlibs/singleinstance/examples/recovery.xl
release/xl.exe run release/extlibs/singleinstance/examples/recovery_c.c
release/xl.exe build release/extlibs/singleinstance/examples/recovery.xl -o build/singleinstance-recovery.exe
./build/singleinstance-recovery.exe
```

```text
acquired: true
rejected reacquire keeps owner: true
released: true
retry and repeated release: true
```

XLang 在获取成功后立即进入 `try/finally`，业务异常也调用 `release()`。
同一包实例重复获取是使用错误，不会释放原锁；这与另一个进程/独立包
实例占用时的普通 `false` 不同。C 用 1/0/-1 区分这些结果，先读取本次
错误再查询状态；公开查询会清除旧诊断。恢复范例捕获的是明确的重复
获取错误，不是吞掉任意异常继续运行。

独立 GCC 使用同一 C API，无额外桥接（Windows）：

```powershell
gcc -std=c11 -O2 -Wall -Wextra -Werror -Ilib/xrt -Irelease/extlibs/singleinstance release/extlibs/singleinstance/examples/recovery_c.c release/extlibs/singleinstance/singleinstance_native.c release/extlibs/singleinstance/test/runtime.c -lws2_32 -ladvapi32 -liphlpapi -lm -o build/singleinstance-recovery-c.exe
./build/singleinstance-recovery-c.exe
```

Linux 编译相同三个源文件，移除 Windows 链接库，使用 `-D_DEFAULT_SOURCE
-pthread -ldl -lm`。测试 runtime 只是 C 对照的单份 XRT 实现，不进入生产
桥接。C/GCC、C 内存运行、C 独立产物、XLang 内存运行和独立产物在两
平台逐字对照同一 golden。

锁名会以 Base64URL 编码留在当前用户主目录的 `.xlang/singleinstance/`。
释放保留文件，避免删名重建造成两个锁域；不要把文件存在当作持锁。
范例运行会留下这个普通空锁文件，不删除潜在的其他应用锁文件。
完整门禁使用独占 `USERPROFILE`/`HOME`，不使用个人主目录。

当前两个平台默认门禁都会在最终真实双 `XL_Context` 检查失败（M8），
并保存本次现场。这项要求不同于两个独立静态包实例或两个进程，不可
用那些已通过的结果替代。普通双语 AOT 和恢复示例通过不表示整包已
签收发布级；深 Windows 主目录与完整线程/设备/权限矩阵也未签收。
