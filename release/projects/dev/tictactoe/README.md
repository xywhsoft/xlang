# tictactoe - Tic-Tac-Toe (computer vs computer)

用 `class Board`（`array<int>` 长度 9 棋盘）+ **minimax 最优 AI** 实现的井字棋，
全部计算机 vs 计算机（无 stdin），用于压测 **class 封装 + 递归 AI + 集合去重**。

## 用法

```bat
cd /d D:\GIT\x-lang\demo6\release
xl run projects\dev\tictactoe\tictactoe.xl -- game
xl run projects\dev\tictactoe\tictactoe.xl -- smart
xl run projects\dev\tictactoe\tictactoe.xl -- tournament 1000
xl run projects\dev\tictactoe\tictactoe.xl -- analyze
```

> 注：`xl run` 需用 `--` 分隔解释器参数与程序 argv（同 `config` 工具）。

| 命令                  | 作用                                                         |
| --------------------- | ------------------------------------------------------------ |
| `game`                | 随机 AI (X) vs 随机 AI (O)，逐步打印棋盘与结果               |
| `smart`               | 随机 AI (X) vs 最优 minimax AI (O)，逐步打印                 |
| `tournament <N>`      | N 局锦标赛（随机 X vs 最优 O），统计胜/负/平与胜率           |
| `analyze`             | 用 minimax 遍历博弈树，统计可达互异局面数 + 空棋盘 minimax 值|

随机种子固定（`random.seed(uint64(20260708), uint64(1))`），故 `xl run` 与 built exe
输出可复现、互相一致。

## 输出示例（`game`）

```
=== Game: Random (X) vs Random (O) ===

Move 1 (X):
 . | . | .
 . | . | X
 . | . | .

Move 2 (O):
 . | O | .
 . | . | X
 . | . | .

...

Move 5 (X):
 O | O | X
 . | . | X
 . | . | X

Result: X wins! (5 moves)
```

## 设计

- **棋盘**：`array<int>` 长度 9，`0=空 1=X 2=O`；位置编号 `idx = row*3 + col`。
- **`class Board`** 封装：`cell/place/unset/checkWin/isFull/signature/symbol/
  render/showBoard/randomMove/minimax/bestMove`。胜负判定硬编码 8 条线（3 行 + 3 列 + 2 对角）。
- **独立棋盘**：`Board` 声明 `derive clone`，分析器通过标准库 `clone(board)` 深克隆棋盘和内部 `array<int>`。
- **最优 AI**：`minimax(player)` 用 **negamax** 写法（`score = -self.minimax(opponent)`），
  直接扫描固定的 9 格并在 `self.cells` 上 **make/undo**，递归热路径不创建临时空位数组，
  评分 `+10 胜 / -10 负 / 0 平`。`bestMove(player)` 遍历 9 格取最大分。
- **锦标赛**：随机 X vs 最优 O。理论：最优 O 永不输（实测 X wins 恒为 0，O≈76~81%，平≈18~24%）。
- **analyze**：`explore(Board b)` 从空棋盘递归遍历博弈树，用 `set<string>` 按 9 位签名去重，
  在已决胜负/满盘处剪枝（不展开子节点但计入该局面）。经典结果 **5478 个可达互异局面**；
  空棋盘对 X 的 minimax 值为 **0（最优下和棋）**——与博弈论结论一致。

## 运行与构建（均 EXIT=0，无需 companion `.c`）

```bat
cd /d D:\GIT\x-lang\demo6\release
xl run  projects\dev\tictactoe\tictactoe.xl -- game
xl build projects\dev\tictactoe\tictactoe.xl -o projects\dev\tictactoe\tictactoe.exe
```

`xl build` 在当前 release 已不再需要 argv shim（BUG-19 已修复，与 `life` 等工具一致）。
built exe 经全命令验证与 `xl run` 输出逐字节一致，退出码透传正确（有效命令 EXIT=0，
无参/未知命令 EXIT=1）。

## 语言能力验证

- `emptyCells() as array<int>` 验证 class 方法返回静态泛型容器。
- `derive clone` 与标准库 `clone(board)` 验证 class 和容器字段的深克隆。
- `get` 可在成员名上下文中作为普通方法名；本例使用 `cell/place/unset` 是领域命名，不是语法绕过。
