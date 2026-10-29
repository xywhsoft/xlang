# sudoku - 9x9 数独求解器

使用 `class Sudoku`、`array<int>` 和递归回溯求解标准 9x9 数独。输入使用 81 位数字串，`0` 表示空格。

## 用法

```bat
cd /d D:\GIT\x-lang\release
xl run projects\dev\sudoku\sudoku.xl -- "530070000600195000098000060800060003400803001700020006060000280000419005000080079"
xl run projects\dev\sudoku\sudoku.xl -- solve projects\dev\sudoku\puzzle1.txt
xl run projects\dev\sudoku\sudoku.xl -- selftest
```

## 实现

- 棋盘使用长度固定为 81 的 `array<int>`，索引为 `row * 9 + col`。
- `isValid` 检查行、列和 3x3 宫；`solve` 递归尝试 1 到 9 并在失败时回溯。
- `verify` 对完整结果再次执行冲突检查，避免只依赖求解路径判断正确性。
- 输入必须恰好包含 81 个数字，文件模式会先移除空白字符。
- `selftest` 求解一份已知题目并断言完整的 81 位结果，失败时返回非 0。

## 构建

```bat
xl build projects\dev\sudoku\sudoku.xl -o projects\dev\sudoku\sudoku.exe
projects\dev\sudoku\sudoku.exe selftest
```
