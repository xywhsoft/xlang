# XGE3 游戏范例

九个范例保留完整玩法，使用当前 XGE3 声明。`_game.xl` 只共用窗口循环、
坐标记录构造和可选资产查找；输入、绘制、字体和声音调用对应的原生 C 函数。

| 文件 | 操作与内容 |
| --- | --- |
| snake.xl | 方向键 / WASD，R 重开 |
| 2048.xl | 方向键合并方块，R 重开 |
| tetris.xl | 方向键移动、旋转，空格落下，R 重开 |
| flappy.xl | 空格跳跃，R 重开 |
| pong.xl | 鼠标控制左侧挡板，R 重开 |
| breakout.xl | 鼠标控制挡板，R 重开 |
| minesweeper.xl | 左键打开、右键插旗，R 重开 |
| asteroids.xl | 方向键移动，空格射击，R 重开 |
| shooter.xl | 鼠标 / 方向键移动，鼠标左键 / 空格射击 |

从 Windows 运行目录构建并启动：

```bat
cd release\win_x64
xl.exe build examples\games\snake.xl -o snake.exe
snake.exe
```

`snake.exe 60` 会创建真实窗口并在 60 帧后退出。它是限帧图形验证。
当前 JIT 的 XGE 动态后端仍需要配套的 `xge3_host.dll`；上述 AOT 默认静态链接。
`run_games.bat` 会依次检查、构建并限帧运行全部九个游戏。

`runGame` 在图形帧开始后加载资源，在退出时先解除回调，再清理资源，最后关闭引擎。
新游戏只需实现 `initializeGame`、`on_frame`、`cleanupGame` 三个普通函数。

字体优先使用运行包内的字体，随后查找系统字体。`assets/` 中的精灵和声音用于
shooter；缺少可选资产时使用形状并静音。素材查找同时支持从运行目录或本目录启动。
