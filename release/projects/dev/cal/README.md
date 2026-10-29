# cal - 日历工具

命令行日历，验证 xlang `time` 标准库（`today`/`fromDate`/`year`/`month`/`day`/`weekday`/`daysInMonth`/`isLeapYear`）。

## 用法

```bat
cd /d D:\GIT\x-lang\release
xl run projects\dev\cal\cal.xl --                 :: 当月
xl run projects\dev\cal\cal.xl -- 2026 7           :: 指定月份
xl run projects\dev\cal\cal.xl -- year 2026        :: 全年（3x4 网格）
xl run projects\dev\cal\cal.xl -- weekday 2026-07-08  :: 星期几
xl run projects\dev\cal\cal.xl -- help
```

注意 `xl run` 传程序参数要用 `--` 分隔（与 todo 工具一致）。

`<date>` 支持 `2026-07-08`、`2026/7/8`、`20260708`。

构建独立 exe：
```bat
xl build projects\dev\cal\cal.xl -o projects\dev\cal\cal.exe
```

## 输出约定

- **单月**：中文表头 `日 一 二 三 四 五 六`，每格 2 字符右对齐（` 1`、`12`、`31`）。2 月追加 `(闰年 29 天)`/`(平年 28 天)`（用 `time.isLeapYear`）。
- **全年**：3 个月并排 × 4 行，英文表头 `Su Mo Tu We Th Fr Sa`（用 ASCII 以保证 CJK 环境下并排对齐；单月仍用中文表头）。

## weekday() 编码

`time.weekday()` 返回周日开头编码：`0=周日 1=周一 2=周二 3=周三 4=周四 5=周五 6=周六`。

日历网格直接使用该编码作为列号。

校验锚点：2026-07-08 是星期三，`weekday()=3`；2026-07-05 是星期日，`weekday()=0`；2026-01-01 是星期四，`weekday()=4`。

## 验证

- `xl run` 与 `xl build`+exe **双路径 EXIT=0**，且输出逐行一致（`Compare-Object` 为空）。
- 多月核对：2026-01-01(周四)、2026-02-01(周日)、2026-02-28(周六)、2026-07-08(周三)、2026-12-01(周二)、2024-02(闰年 29 天) 全部正确。

## 发现的问题（详见 dev/BUGS.md）

- 两条 codegen 限制（NOTE-24）：typed 泛型局部不能从返回无类型 `array` 的函数初始化（用 `auto`）；多值返回不能 `return f(...)` 转发（先解构）。
- 正向验证：本环境的 `array`/`list` 容器**作函数参数与返回值均正常**（与 README "已修复 BUG" 一致；BUGS.md BUG-11/12 在本 build 已不复现）。
