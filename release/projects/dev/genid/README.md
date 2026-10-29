# genid - ID 生成器

一次性产出多种风格的唯一 ID，全部输出到 stdout，一行一个。

## 用法

```
genid xid                       一个 XID（XRT 原生 id.xid()，32 字符）
genid xid <count>               批量 XID
genid uuid                      UUID v4（4×random.u32 拼接，钉死 version=4 / variant=8）
genid snowflake <machineId>     简化版 Snowflake（可选追加 <count> 批量）
genid nanoid [len=21]           NanoID 风格 URL-safe 随机串（A-Za-z0-9_-）
genid sequential <start> <count>
genid help | -h | --help
```

## Snowflake 位布局（63 bit）

```
| 41 bit ms 时间戳 (自定义 epoch 起) | 10 bit machineId | 12 bit seq |
  高位                                            中段              低位
```

- 自定义 epoch = `1700000000000`（2023-11-14T22:13:20Z）。
- ms 时间戳 = `time.toUnix(time.nowUTC())*1000` + `time.timer()` 的亚秒偏移。
- 同 ms 内 seq 自增，溢出（>4095）回绕到下一 ms；时钟回拨时强制 +1ms 推进。
- `machineId` 取值范围 `[0, 1023]`，越界报错退出 1。

解码自校验（PowerShell）：

```powershell
$id = 350297551435636738
$seq = $id -band 4095          # 2
$machine = ($id -shr 12) -band 1023   # 42
$ts = ($id -shr 22) + 1700000000000   # 解码到当前日期
```

## 验证

`xl run` 与 `xl build` 两条路径均 EXIT=0。错误参数（缺参、count<1、machineId 越界、未知子命令）统一返回 EXIT=1 并打印提示。

## 备注

- `xl run` 把脚本之后的裸参数当链接库，传给程序的 argv 必须用 `--` 分隔：`xl run genid.xl -- xid`。这是 `xl run` 的标准约定（见 `xl help`），不是本工具问题。
- 开发中未触发任何编译器 BUG。
