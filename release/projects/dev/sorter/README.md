# sorter - 数据排序去重工具

对文本文件按行做排序 / 去重 / 计数 / 反转，结果输出到 stdout (Unix `sort` 风格，便于管道消费)。

## 用法

```bat
cd /d D:\GIT\x-lang\release

xl run projects\dev\sorter\sorter.xl -- sort   projects\dev\sorter\data.txt
xl run projects\dev\sorter\sorter.xl -- sort   projects\dev\sorter\data.txt -r
xl run projects\dev\sorter\sorter.xl -- sort   projects\dev\sorter\data.txt -n
xl run projects\dev\sorter\sorter.xl -- sort   projects\dev\sorter\data.txt -n -r
xl run projects\dev\sorter\sorter.xl -- unique projects\dev\sorter\data.txt
xl run projects\dev\sorter\sorter.xl -- unique projects\dev\sorter\data.txt -s
xl run projects\dev\sorter\sorter.xl -- count  projects\dev\sorter\data.txt
xl run projects\dev\sorter\sorter.xl -- reverse projects\dev\sorter\data.txt

:: 编译为 exe (UTF-8 argv 已支持)
xl build projects\dev\sorter\sorter.xl -o projects\dev\sorter\sorter.exe
projects\dev\sorter\sorter.exe sort projects\dev\sorter\data.txt -n
```

## 命令

| 命令 | 说明 |
|---|---|
| `sort <file> [-r] [-n]` | 排序。默认字符串升序；`-r` 降序；`-n` 按数值 (每行 `float` 解析，失败按 `0.0`)；可组合 `-n -r`。 |
| `unique <file> [-s]` | 去重，默认保留首次出现顺序；`-s` 去重后再升序排序。 |
| `count <file>` | 统计每行出现次数，按次数降序；次数相同按首次出现顺序 (稳定)。输出 `<count> <line>`，count 右对齐到至少 4 位。 |
| `reverse <file>` | 反转行序。 |
| `help` | 显示帮助。 |

## 示例 (data.txt)

输入：
```
banana
apple
cherry
apple
banana
apple
date
10
2
1
10
2
100
```

`count`：
```
   3 apple
   2 banana
   2 10
   2 2
   1 cherry
   1 date
   1 1
   1 100
```

`sort -n`：非数字行 (banana/apple/cherry/date) `float` 解析失败按 `0.0`，排在所有正数前；`10/2/1/100` 按数值升序。

## 实现要点

- **读取**：`file.readText` + `string.splitLines`，收集到 `array<string>` (typed container)。
- **排序**：选择排序 (原地)，字符串模式用 `string.compare`，数值模式预解析 `float` 键到并行 `array<float>`，交换行时同步交换键。
- **去重**：顺序扫描 + `array.contains` 线性查重，保留首次出现顺序 (符合 spec，未用无序的 `set<T>`)。
- **计数**：去重得到首次出现序的 `unique` 列表 + 并行 `count`/`ord`，对三者同步做选择排序，键为 `(count desc, ord asc)`；`ord` 随元素一起交换，保证同次数时稳定保持首次出现顺序。
- **反转**：直接用 `array.reverse` 容器 API。
- **空类型数组**：`array<string> r = array[""]; r.remove(0)` 得到空 typed 数组 (无空 typed 字面量语法，见 `examples/container_methods.xl` 的 idiom)。
- **`var` → string**：`splitLines` 返回动态 `array`，元素经 `string(v)` 转换 (已确认 `func f(var)` 可绑定 typed array 实参，`dev/README.md` 已修复 BUG 列表)。

## 已知限制 / 触发 BUG

- **BUG-S1** (中)：`string.splitLines` 把末尾换行当分隔符而非行终结符，`"a\nb\n"` → `["a","b",""]`，比 Python `str.splitlines()` / Unix 行工具多一个尾部空串。本工具在 `loadLines` 中按惯例丢掉恰好一个尾部空串来兼容。详见 `../BUGS.md`。
- 其余遵循 `../README.md` 仍需注意的限制 (无 stdin、built exe argv 等)。
