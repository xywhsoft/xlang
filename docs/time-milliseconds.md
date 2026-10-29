# 时间合同

xlang 与当前 XRT 统一使用整数毫秒，不提供微秒入口或兼容转换。

- `time` 是从公元 0001-01-01 00:00:00 UTC 起算的有符号 64 位毫秒；`time(0)` 是该原点。
- Unix 时间通过 `time.fromUnix`／`time.toUnix`（秒）或 `time.fromUnixMs`／`time.toUnixMs`（毫秒）转换，并检查溢出。
- `DateTime.millisecond`、构造参数 `millisecond` 范围为 0–999；时区偏移 `offset` 按国际约定使用秒。
- 相对等待、缓存 TTL、进程耗时和配置超时均使用毫秒。接受无限等待的相对参数使用 `-1`；具体配置中原生零值表示禁用的语义由配置接口说明。
- `time.monotonic()` 和 `*Until(deadlineMs)` 使用同一个单调毫秒时钟，绝不能与日历时间混用。Until 在调用原生 For 前减去当前单调时间；重试时继续使用同一个绝对期限。
- `time.timer()` 返回高精度 double 秒，保留用于性能计时；协议字段明确以秒定义的值（例如 TLS 票据 lifetime）继续遵循协议。
- 旧阶段验收文档仅记录当时快照；当前 API 与单位以本合同和生成的 XRT 声明为准。
