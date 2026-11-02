# 前台服务与 PID 锁交接

从仓库根目录运行：

```powershell
./release/xl.exe run release/extlibs/daemon/examples/restart.xl
./release/xl.exe run release/extlibs/daemon/examples/restart_c.c
./release/xl.exe build release/extlibs/daemon/examples/restart.xl -o build/daemon-restart.exe
./build/daemon-restart.exe
```

Linux 使用 `./release/xl`，输出文件可省略 `.exe`。C 也可由 `xl build`
生成独立程序，或按包 manifest 用 GCC 分离编译并链接同一份 XRT。

两种语言均应得到 `restart.expected.txt` 的五行输出：

```text
held: true
contender: busy
failed switch: held
handoff: true
stop: true
```

同名锁被占用返回 false，不是异常。持有锁时改路径属于状态错误，失败后
原来的锁和 PID 仍有效；释放后由下一个对象显式获取。`requestStop` 只设置
业务循环可读取的状态，不会退出或杀掉进程。范例仅创建独占临时目录中的
PID 文件，并在结束时清理，不启动后台服务，也不修改其他进程的 PID 文件。

中央 `release/examples/extlibs/daemon.xl` / `daemon_c.c` 是基础用法，输出中的
PID 每次不同；包门禁只规范化该数字，再与固定预期比较。故障注入及真实
子进程/文件竞争测试在 `test/`，不把这些底层压力代码塞进教学范例。
