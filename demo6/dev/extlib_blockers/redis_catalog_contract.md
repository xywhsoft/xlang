# redis 目录 smoke 与当前命令合同不一致

2026-10-16，`release/extlibs/redis/xlang.package.json` 登记
`test/boundaries.xl` 后运行：

```powershell
cd D:\GIT\x-lang\demo6
./tools/smoke_extlib_catalog.ps1 -Package redis
./release/xl.exe run test/cases/extlib_redis.xl
```

包内边界用例通过，中央 `test/cases/extlib_redis.xl` 在
`assert redis.encode([]) == "*0\r\n"` 失败，实际抛出
`RangeError: RESP command argument count is out of range`。

`redis.encode` 与 `redis.encodeBytes` 的公开合同要求至少一个命令参数；
空 RESP 数组虽可编码，却不是可执行的 Redis 命令。包内实现、README 与
C/XLang 负例保持此合同，不为旧断言增加空命令兼容路径。

2026-10-18 已解决测试合同漂移：只改扩展库中央用例，令空命令检查
`RangeError`，保留非空 `PING`/`SET` 编码断言；没有放宽包实现或修改
编译器主线。当前 `smoke_extlib_catalog.ps1 -Package redis` 为 2/2，
Linux 包级脚本也已纳入这份中央用例。完整 AOT 的共享缺符号阻断另见
`text_snapshot_link.md`，目录 smoke 转绿不等于发布验收全部完成。
