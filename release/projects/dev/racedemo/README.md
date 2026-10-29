# racedemo - task 高级 API 验证工具

验证 DEMO6 第 13 节声明的 task 高级 API：`task.race` / `task.spawn` / `task.cancel`，以及 async 异常传播、重复 await、race loser 取消等边界语义。

## 用法

```
xl run projects\dev\racedemo\racedemo.xl
xl build projects\dev\racedemo\racedemo.xl -o projects\dev\racedemo\racedemo.exe
projects\dev\racedemo\racedemo.exe
```

无参数。直接运行全部用例，输出逐条 `[PASS]/[FAIL]`，末尾汇总 `passed/failed` 并给出 `RESULT: OK`（exit 0）或 `RESULT: FAIL`（exit 1）。

## 覆盖的用例（共 15 条断言）

| # | 能力 | 验证点 |
|---|---|---|
| 1 | `task.race` | 第一个完成者获胜（50ms vs 200ms → winner 2）；winner future 仍可重复 await |
| 2 | `task.spawn` | 从普通 `func(int,int) as int` 与闭包 `makeAdder(40)` 创建异步任务 |
| 3 | `task.cancel` | 取消 pending future 返回 `true`；之后 await 抛 `future cancel requested.` |
| 3b | `task.cancel` | 对已完成 future 取消返回 `false`，原值不变 |
| 4 | async 异常传播 | async 函数内 throw，await 侧 `catch e` 拿到 `e.message`；重复 await 仍保留原错误 |
| 5 | 重复 await | 已完成 future 连续 await 三次均返回相同值 |
| 6 | race loser 取消 | winner 完成后，仍在 pending 的 loser future 进入取消态（await 抛 cancel）；winner 仍可 await |
| 7 | `task.spawn` 失败传播 | spawn 的函数 throw，await 侧捕获原 message |
| 8 | 边界 | `task.race` 收到空数组抛 `task.race requires a non-empty future array` |

## 设计要点

1. **`await` 只能出现在顶层 / async func / async generator**（`[sem] await can only be used at top level, inside async func, or inside async generator`）。因此全部用例包在 `async func runAll()` 里，在顶层 `await runAll()` 驱动；进程退出码由普通 `func main(...) as int` 读取全局 `g_pass/g_fail` 计数返回。顶层初始化先于 `main` 执行，故计数在 `main` 运行时已就绪。
2. 失败用例不中断：每条 `try/catch` 只匹配期望的 `e.message`，未匹配时状态保持 0 → `[FAIL]` 但不崩溃；预期异常以外的情况也能被 `check` 捕获为失败。
3. 计时用例全部留出足够间隔（winner 40~50ms vs loser 200~300ms），消除 worker 调度抖动。

## 发现的 BUG

无。`task.race` / `task.spawn` / `task.cancel` / async 异常传播 / 重复 await / race loser 取消均与 `DEMO6功能清单.md` 第 13 节描述完全一致。

## 验证

- `xl run` 与 `xl build` + exe 双路径均 `EXIT=0`。
- exe 连续运行 8 次，15/15 全部 `RESULT: OK`，无间歇性竞态。
