# diff - 文本行级 Diff 工具

对两个文本文件做行级别对比，支持简单格式、unified diff 和纯统计三种输出。

## 用法

```bat
xl run projects\dev\diff\diff.xl -- <file1> <file2>
xl run projects\dev\diff\diff.xl -- <file1> <file2> -u
xl run projects\dev\diff\diff.xl -- <file1> <file2> -s
```

构建独立 exe：

```bat
xl build projects\dev\diff\diff.xl -o projects\dev\diff\diff.exe
diff.exe <file1> <file2> [-u|-s]
```

## 选项

| 选项 | 说明 |
|---|---|
| （默认） | 简单格式：输出全部行（` ` 相同 / `-` 删除 / `+` 新增）+ Summary |
| `-u` | unified diff 格式（3 行上下文，标准 `@@ -a,b +c,d @@` hunk） |
| `-s` | 仅输出 Summary 统计行 |
| `-h` | 显示帮助 |

## 退出码

| 码 | 含义 |
|---|---|
| 0 | 两文件相同 |
| 1 | 两文件不同 |
| 2 | 出错（文件不存在 / 参数不足 / 读取失败） |

## 输出示例（简单格式）

```
--- a.txt
+++ b.txt

 line 1
- line 2
+ line 2 modified
 line 3
 line 4
+ new line
 line 5

Summary: +2 added, -1 removed, 4 unchanged
```

## 输出示例（unified）

```
--- a.txt
+++ b.txt
@@ -1,5 +1,6 @@
 line 1
-line 2
+line 2 modified
 line 3
 line 4
+new line
 line 5
```

## 设计要点

- **读行**：`file.readText` + `string.splitLines`，存入 `array<string>`。
- **核心算法**：LCS（最长公共子序列）动态规划，一维展开 DP 表 `(m+1)*(n+1)`，
  回溯生成 `same/del/add` 操作序列（回溯逆序收集 → `array.reverse()` 翻正）。
- **回溯 tie-break**：dp 相等时优先走 left（add），使翻转后 del 在 add 之前，
  与 GNU diff `-` before `+` 惯例一致。
- **unified hunk**：变化行 ±3 行上下文标记 → 连续标记段分组 → 每段输出
  `@@ -oldStart,oldCount +newStart,newCount @@`。纯增/纯删 hunk 的行号采用
  GNU 约定（`-0,0` / `+0,0`）。
- **退出码**：added==0 and removed==0 → 0（相同），否则 → 1（不同）。

## 绕过已知 BUG

- **BUG-S1**（`string.splitLines` 尾部空串）：`toLines()` 在拆行后丢掉恰好一个
  尾部空串，使 `"a\nb\n"` 等价于 2 行（与 Unix 行工具一致）。

## 重要用法提醒：run 模式必须用 `--` 分隔脚本参数

`xl.exe` 自身可能占用部分 flag，脚本传参时建议用 `--` 隔开：

```bat
xl run projects\dev\diff\diff.xl -- a.txt b.txt -u
```

built exe (`diff.exe`) 不存在此问题，可直接 `diff.exe a.txt b.txt -u`。

## 验证

run 与 build 两条路径均通过：

- `diff.exe a.txt b.txt` → EXIT=1（文件不同）
- `diff.exe a.txt b.txt -u` → unified 格式，EXIT=1
- `diff.exe a.txt b.txt -s` → `Summary: +2 added, -1 removed, 4 unchanged`，EXIT=1
- `diff.exe a.txt a.txt` → EXIT=0（文件相同）
- `diff.exe empty.txt a.txt -u` → `@@ -0,0 +1,5 @@`（纯插入），EXIT=1
- `diff.exe a.txt empty.txt -s` → `+0 added, -5 removed, 0 unchanged`，EXIT=1
- `diff.exe nonexist.txt b.txt` → EXIT=2（文件不存在）
- `diff.exe -h` → 帮助，EXIT=0
- 多 hunk unified（big_a/big_b，两处远距离改动）→ 正确分离为两个 `@@` hunk

## BUG 清单

未发现新的编译器 BUG。仅使用已知 BUG-S1 的标准绕过（`splitLines` 尾部空串）。
