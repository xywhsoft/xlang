# asciinema - 终端录制播放器（VT100/ANSI + 帧动画）

解析 [asciicast v2](https://docs.asciinema.org/manual/asciicast/v2/) 格式的终端录制文件，
通过一个完整的 VT100/ANSI 转义序列状态机维护虚拟终端缓冲，按时间戳逐帧重放。

## 命令

```
asciinema play <file.cast> [--speed Nx] [--pause] [--no-clear]
asciinema info <file.cast>
asciinema test
```

- `play` — 按时间戳重放。维护 `width × height` 虚拟终端，每个输出事件投喂给 VT100
  解析器后渲染一帧。
  - `--speed Nx` — 倍速（`2x`=2 倍速，`0.5x`=慢放，默认 `1x`）。
  - `--pause` — 每帧额外停顿 300ms（因 xlang 无 stdin，不能用按键步进）。
  - `--no-clear` — 帧分隔模式（打印帧号+时间戳），不使用 ESC 实时清屏。
- `info` — 显示元信息：版本、尺寸、标题、时间戳、时长、输出/输入事件数、字节数。
- `test` — 运行 28 项 VT100 解析器自测 + 内存演示回放（不依赖文件）。

## 支持的转义序列

| 类别 | 序列 | 说明 |
|---|---|---|
| 光标定位 | `\033[r;cH` `\033[r;cf` | 1-based 行;列 |
| 光标移动 | `\033[A/B/C/D/E/F` | 上/下/右/左/下行首/上行首 |
| 绝对定位 | `\033[G` `\033[` `\033[d` | 水平/垂直绝对 |
| 清屏 | `\033[0J` `\033[1J` `\033[2J` `\033[3J` | 光标后/前/全/全+回滚 |
| 清行 | `\033[0K` `\033[1K` `\033[2K` | 光标后/前/全行 |
| 颜色 SGR | `\033[...m` | **解析消费但不渲染颜色**，保留可见文本 |
| 字符编辑 | `\033[@P` `\033[L` `\033[M` | 插入/删除字符、插入/删除行 |
| 保存/恢复 | `\033[s` `\033[u` `\0337` `\0338` | 光标位置 |
| OSC | `\033]...BEL` | 窗口标题等，跳过 |
| 控制字符 | `\r \n \b \t \007` | CR / LF / 退格 / 制表 / 铃 |
| 模式 | `\033[?25h/l` 等 | 私有序列，忽略 |

## 示例

```bat
cd /d D:\GIT\x-lang\demo6\release
xl run projects\dev\asciinema\asciinema.xl -- test
xl run projects\dev\asciinema\asciinema.xl -- info projects\dev\asciinema\demo.cast
xl run projects\dev\asciinema\asciinema.xl -- play projects\dev\asciinema\hello.cast --speed 5x --no-clear
xl run projects\dev\asciinema\asciinema.xl -- play projects\dev\asciinema\demo.cast --speed 2x
```

## 测试文件

- `demo.cast` — 特性演示（颜色剥离、光标定位拼写 JUMP、清行、滚动）。
- `hello.cast` — 逐字符打字效果。

## 设计

- 虚拟终端：`array<string>` 一维模拟二维（`row*width+col`），每格单字符。
- 解析器：字符级状态机（NORMAL→ESC→CSI/OSC/CHARSET），逐字符投喂。
- 时间同步：`time.timer()` 单调秒 + `time.sleep(ms)` 等待到目标时间戳。
- 颜色：SGR 序列被识别并消费（不写入缓冲），仅可见文本进缓冲。

## 压测点

string 转义解析 / 终端控制模拟 / 时间同步 / array 二维操作 / class 状态机。
